#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "scanner.h"
#include "debug.h"
#include "vm.h"

typedef struct {
    Token current;
    Token previous;
    bool  hadError;
    bool  panicMode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * / %
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_SUBSCRIPT,  // [] @ ^
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);
typedef struct {
    ParseFn    prefix;
    ParseFn    infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token   name;
    int16_t depth;
    bool    isCaptured;
} Local;

typedef struct {
    uint8_t index;
    bool    isLocal;
} Upvalue;

typedef enum {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_METHOD,
    TYPE_SCRIPT
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction*     function;
    FunctionType     type;
    Local            locals[MAX_LOCALS];
    int16_t          localCount;
    Upvalue          upvalues[MAX_UPVALUES];
    int16_t          scopeDepth;
} Compiler;

typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
    bool                  hasSuperclass;
} ClassCompiler;

Parser         parser;
Compiler*      currentUnit;
ClassCompiler* currentClass;

static Chunk* currentChunk(void) {
    return &currentUnit->function->chunk;
}

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode)
        return;
    parser.panicMode = true;
    printf("[line %d] Error", token->line);

    if (token->type == TOKEN_EOF)
        printf(" at end");
    else if (token->type != TOKEN_ERROR)
        printf(" at '%.*s'", token->length, token->start);

    printf(": %s\n", message);
    parser.hadError = true;
}

static void error(const char* message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

static void advance(void) {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR)
            break;
        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

static void consumeExp(TokenType type, const char* first, const char* second) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    sprintf(buffer, "Expect %s after %s.", first, second);
    errorAtCurrent(buffer);
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (parser.current.type != type)
        return false;
    advance();
    return true;
}

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitLoop(int loopStart) {
    int offset;
    emitByte(OP_LOOP);

    offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX)
        error("Jump too large.");
    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

static int emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

static void emitReturn(void) {
    if (currentUnit->type == TYPE_INITIALIZER)
        emitBytes(OP_GET_LOCAL, 0);
    else
        emitByte(OP_NIL);
    emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t)constant;
}

static void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

static void patchJump(int offset) {
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX)
        error("Jump too large.");
    currentChunk()->code[offset]     = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler* compiler, FunctionType type) {
    Local* local;
    compiler->enclosing  = currentUnit;
    compiler->function   = NULL;
    compiler->type       = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function   = newFunction();
    currentUnit          = compiler;

    if (type != TYPE_SCRIPT) {
        if (parser.previous.type == TOKEN_FUN) {
            sprintf(buffer, "#%d", vm.lambdaCount++);
            currentUnit->function->name = copyString(buffer, strlen(buffer));
        } else
            currentUnit->function->name = copyString(parser.previous.start, parser.previous.length);
    }

    local = &currentUnit->locals[currentUnit->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    if (type != TYPE_FUNCTION) {
        local->name.start  = "this";
        local->name.length = 4;
    } else {
        local->name.start  = "";
        local->name.length = 0;
    }
}

static ObjFunction* endCompiler(bool addReturn) {
    ObjFunction* function;
    if (addReturn)
        emitReturn();
    function = currentUnit->function;

    if (vm.debug_print_code && !parser.hadError)
        disassembleChunk(currentChunk(), function->name != NULL
                                       ? function->name->chars : "<script>");

    currentUnit = currentUnit->enclosing;
    return function;
}

static void beginScope(void) {
    currentUnit->scopeDepth++;
}

static void endScope(void) {
    currentUnit->scopeDepth--;

    while (currentUnit->localCount > 0 &&
            currentUnit->locals[currentUnit->localCount - 1].depth > currentUnit->scopeDepth) {
        if (currentUnit->locals[currentUnit->localCount - 1].isCaptured)
            emitByte(OP_CLOSE_UPVALUE);
        else
            emitByte(OP_POP);
        currentUnit->localCount--;
    }
}

static void expression(void);
static void statement(void);
static void declaration(void);
static const ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static uint8_t argumentList(bool* isVarArg, TokenType terminator);
static uint8_t identifierConstant(Token* name);

static void binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    const ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));
    switch (operatorType) {
        case TOKEN_BANG_EQUAL:    emitBytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL); break;
        case TOKEN_GREATER:       emitBytes(OP_SWAP, OP_LESS); break;
        case TOKEN_LESS_EQUAL:    emitBytes(OP_SWAP, OP_LESS); emitByte(OP_NOT); break;
        case TOKEN_LESS:          emitByte(OP_LESS); break;
        case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
        case TOKEN_PLUS:          emitByte(OP_ADD); break;
        case TOKEN_MINUS:         emitByte(OP_SUB); break;
        case TOKEN_STAR:          emitByte(OP_MUL); break;
        case TOKEN_SLASH:         emitByte(OP_DIV); break;
        case TOKEN_BACKSLASH:     emitByte(OP_MOD); break;
        default: return; // Unreachable
    }
}

static void call(bool canAssign) {
    bool isVarArg = false;
    uint8_t argCount = argumentList(&isVarArg, TOKEN_RIGHT_PAREN);
    emitBytes(isVarArg ? OP_VCALL : OP_CALL, argCount);
}

static void dot(bool canAssign) {
    uint8_t name;
    uint8_t argCount;
    bool isVarArg = false;

    consumeExp(TOKEN_IDENTIFIER, "property name", "'.'");
    name = identifierConstant(&parser.previous);

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(OP_SET_PROPERTY, name);
    } else if (match(TOKEN_LEFT_PAREN)) {
        argCount = argumentList(&isVarArg, TOKEN_RIGHT_PAREN);
        emitBytes(isVarArg ? OP_VINVOKE : OP_INVOKE, name);
        emitByte(argCount);
    } else
        emitBytes(OP_GET_PROPERTY, name);
}

static void slice(bool canAssign) {
    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
        return;
    } else
        emitByte(OP_GET_SLICE);
}

static void slice_right(bool canAssign) {
    if (match(TOKEN_RIGHT_BRACKET))
        emitConstant(INT_VAL(INT32_MAX>>1));
    else {
        expression();
        consumeExp(TOKEN_RIGHT_BRACKET, "']'", "slice");
    }
    slice(canAssign);
}

static void index_(bool canAssign) {
    if (match(TOKEN_COLON)) {
        emitConstant(INT_VAL(0));
        slice_right(canAssign);
    } else {
        expression();
        if (match(TOKEN_COLON))
            slice_right(canAssign);
        else {
            consumeExp(TOKEN_RIGHT_BRACKET, "']'", "index");

            if (canAssign && match(TOKEN_EQUAL)) {
                expression();
                emitByte(OP_SET_INDEX);
            } else
                emitByte(OP_GET_INDEX);
        }
    }
}

static void iter(bool canAssign) {
    TokenType accessor = parser.previous.type;
    if (canAssign && match(TOKEN_EQUAL)) {
        if (accessor == TOKEN_HAT) {
            expression();
            emitByte(OP_SET_ITVAL);
        } else
            error("Invalid assignment target.");
    } else
        emitByte(accessor == TOKEN_HAT ? OP_GET_ITVAL : OP_GET_ITKEY);
}

static void literal(bool canAssign) {
    switch (parser.previous.type) {
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NIL:   emitByte(OP_NIL);   break;
        case TOKEN_TRUE:  emitByte(OP_TRUE);  break;
        default: return; // unreachable
    }
}

static void grouping(bool canAssign) {
    expression();
    consumeExp(TOKEN_RIGHT_PAREN, "')'", "expression");
}

static void intNum(bool canAssign) {
    Value value = parseInt(parser.previous.start, false);
    emitConstant(value);
}

static void realNum(bool canAssign) {
#ifdef KIT68K
    Value value = newReal(scanReal(parser.previous.start));
    if (errno != 0)
        error("Real constant overflow.");
#else
    Value value = newReal(strtod(parser.previous.start, NULL));
#endif
    emitConstant(value);
}

static void string(bool canAssign) {
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static uint8_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length)
        return false;
    return fix_memcmp(a->start, b->start, a->length) == 0;
}

static void list(bool canAssign) {
    bool isVarArg = false;
    uint8_t argCount = argumentList(&isVarArg, TOKEN_RIGHT_BRACKET);
    emitBytes(isVarArg ? OP_VLIST : OP_LIST, argCount);
}

static int resolveLocal(Compiler* compiler, Token* name) {
    int i;
    Local* local;

    for (i = compiler->localCount-1; i >= 0; i--) {
        local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1)
                error("Can't read local variable in its own initializer.");
            return i;
        }
    }
    return -1;
}

static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;
    int i;
    Upvalue* upvalue;

    for (i = 0; i < upvalueCount; i++) {
        upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal)
            return i;
    }

    if (upvalueCount == MAX_UPVALUES) {
        error("Too many upvalues in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal; 
    compiler->upvalues[upvalueCount].index   = index; 
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {
    int local, upvalue;

    if (compiler->enclosing == NULL)
        return -1;

    local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1)
        return addUpvalue(compiler, (uint8_t)upvalue, false);

    return -1;
}

static void addLocal(Token name) {
    Local* local;
    if (currentUnit->localCount == MAX_LOCALS) {
        error("Too many local variables in function.");
        return;
    }
    local = &currentUnit->locals[currentUnit->localCount++];
    local->name       = name;
    local->depth      = -1;
    local->isCaptured = false;
}

static void declareVariable(void) {
    Token* name;
    int i;
    Local* local;

    if (currentUnit->scopeDepth == 0)
        return;

    name = &parser.previous;
    for (i = currentUnit->localCount - 1; i >= 0; i--) {
        local = &currentUnit->locals[i];
        if (local->depth != -1 && local->depth < currentUnit->scopeDepth)
            break;

        if (identifiersEqual(name, &local->name))
            error("Already a variable with this name in this scope.");
    }

    addLocal(*name);
}

static void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(currentUnit, &name);

    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(currentUnit, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }
    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(setOp, (uint8_t)arg);
    } else
        emitBytes(getOp, (uint8_t)arg);
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

static Token syntheticToken(const char* text) {
    Token token;
    token.start = text;
    token.length = (int)strlen(text);
    return token;
}

static void super_(bool canAssign) {
    uint8_t name;
    uint8_t argCount;
    bool    isVarArg = false;

    if (currentClass == NULL)
        error("Can't use 'super' outside of a class.");
    else if (!currentClass->hasSuperclass)
        error("Can't use 'super' in a class with no superclass.");

    consumeExp(TOKEN_DOT, "'.'", "'super'");
    consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
    name = identifierConstant(&parser.previous);

    namedVariable(syntheticToken("this"), false);
    if (match(TOKEN_LEFT_PAREN)) {
        argCount = argumentList(&isVarArg, TOKEN_RIGHT_PAREN);
        namedVariable(syntheticToken("super"), false);
        emitBytes(isVarArg ? OP_VSUPER_INVOKE : OP_SUPER_INVOKE, name);
        emitByte(argCount);
    } else {
        namedVariable(syntheticToken("super"), false);
        emitBytes(OP_GET_SUPER, name);
    }
}

static void this_(bool canAssign) {
    if (currentClass == NULL) {
        error("Can't use 'this' outside of a class.");
        return;
    }
    variable(false);
}

static void and_(bool canAssign) {
    int endJump = emitJump(OP_JUMP_FALSE);

    parsePrecedence(PREC_AND);
    patchJump(endJump);
}

static void or_(bool canAssign) {
    int endJump = emitJump(OP_JUMP_TRUE);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

static void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;

    parsePrecedence(PREC_UNARY);

    switch (operatorType) {
        case TOKEN_BANG:  emitByte(OP_NOT); break;
        case TOKEN_MINUS: emitByte(OP_NEG); break;
        default: return; // Unreachable
    }
}

static void function(FunctionType type);

static void lambda(bool canAssign) {
    function(TYPE_FUNCTION);
}

const ParseRule rules[] = {
    /* [TOKEN_LEFT_PAREN]    = */ {grouping, call,   PREC_CALL},
    /* [TOKEN_RIGHT_PAREN]   = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_LEFT_BRACE]    = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_RIGHT_BRACE]   = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_LEFT_BRACKET]  = */ {list,     index_, PREC_SUBSCRIPT},
    /* [TOKEN_RIGHT_BRACKET] = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_COMMA]         = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_DOT]           = */ {NULL,     dot,    PREC_CALL},
    /* [TOKEN_MINUS]         = */ {unary,    binary, PREC_TERM},
    /* [TOKEN_PLUS]          = */ {NULL,     binary, PREC_TERM},
    /* [TOKEN_SEMICOLON]     = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_COLON]         = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_SLASH]         = */ {NULL,     binary, PREC_FACTOR},
    /* [TOKEN_STAR]          = */ {NULL,     binary, PREC_FACTOR},
    /* [TOKEN_PERCENT]       = */ {NULL,     binary, PREC_FACTOR},
    /* [TOKEN_AT]            = */ {NULL,     iter,   PREC_SUBSCRIPT},
    /* [TOKEN_HAT]           = */ {NULL,     iter,   PREC_SUBSCRIPT},
    /* [TOKEN_BANG]          = */ {unary,    NULL,   PREC_NONE},
    /* [TOKEN_BANG_EQUAL]    = */ {NULL,     binary, PREC_EQUALITY},
    /* [TOKEN_EQUAL]         = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_EQUAL_EQUAL]   = */ {NULL,     binary, PREC_EQUALITY},
    /* [TOKEN_GREATER]       = */ {NULL,     binary, PREC_COMPARISON},
    /* [TOKEN_GREATER_EQUAL] = */ {NULL,     binary, PREC_COMPARISON},
    /* [TOKEN_LESS]          = */ {NULL,     binary, PREC_COMPARISON},
    /* [TOKEN_LESS_EQUAL]    = */ {NULL,     binary, PREC_COMPARISON},
    /* [TOKEN_DOT_DOT]       = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_ARROW]         = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_IDENTIFIER]    = */ {variable, NULL,   PREC_NONE},
    /* [TOKEN_STRING]        = */ {string,   NULL,   PREC_NONE},
    /* [TOKEN_INT]           = */ {intNum,   NULL,   PREC_NONE},
    /* [TOKEN_REAL]          = */ {realNum,  NULL,   PREC_NONE},
    /* [TOKEN_AND]           = */ {NULL,     and_,   PREC_AND},
    /* [TOKEN_CLASS]         = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_ELSE]          = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_FALSE]         = */ {literal,  NULL,   PREC_NONE},
    /* [TOKEN_FOR]           = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_FUN]           = */ {lambda,   NULL,   PREC_NONE},
    /* [TOKEN_IF]            = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_NIL]           = */ {literal,  NULL,   PREC_NONE},
    /* [TOKEN_OR]            = */ {NULL,     or_,    PREC_OR},
    /* [TOKEN_PRINT]         = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_RETURN]        = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_SUPER]         = */ {super_,   NULL,   PREC_NONE},
    /* [TOKEN_THIS]          = */ {this_,    NULL,   PREC_NONE},
    /* [TOKEN_TRUE]          = */ {literal,  NULL,   PREC_NONE},
    /* [TOKEN_VAR]           = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_WHILE]         = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_ERROR]         = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_EOF]           = */ {NULL,     NULL,   PREC_NONE},
};


static void parsePrecedence(Precedence precedence) {
    ParseFn prefixRule;
    ParseFn infixRule;
    bool canAssign;

    advance();
    prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    canAssign = precedence <= PREC_ASSIGNMENT;
    (*prefixRule)(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        infixRule = getRule(parser.previous.type)->infix;
        (*infixRule)(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL))
        error("Invalid assignment target.");
}

static uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (currentUnit->scopeDepth > 0)
        return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialized(void) {
    if (currentUnit->scopeDepth == 0)
        return;
    currentUnit->locals[currentUnit->localCount - 1].depth = currentUnit->scopeDepth;
}

static void defineVariable(uint8_t global) {
    if (currentUnit->scopeDepth > 0) {
        markInitialized();
        return;
    }
    emitBytes(OP_DEF_GLOBAL, global);
}

static uint8_t argumentList(bool* isVarArg, TokenType terminator) {
    uint8_t argCount = 0;

    if (!check(terminator)) {
        do {
            if (match(TOKEN_DOT_DOT)) {
                // First UNPACK, introduce count of arguments from lists
                if (!*isVarArg)
                    emitConstant(INT_VAL(0)); 
                *isVarArg = true;
                expression();
                emitByte(OP_UNPACK); // this also adapts list arguments count
            } else {
                expression();
                if (*isVarArg)
                    emitByte(OP_SWAP); // bubble list arguments count to TOS
                if (argCount == UINT8_MAX)
                    error("Can't have more than 255 items in an argument list.");
                argCount++;
            }
        } while (match(TOKEN_COMMA));
    }
    if (terminator == TOKEN_RIGHT_PAREN)
        consumeExp(terminator, "')'", "arguments");
    else
        consumeExp(terminator, "']'", "list");
    return argCount;
}

static const ParseRule* getRule(TokenType type) {
    return &rules[type];
}

static void expression(void) {
    CHECK_STACKOVERFLOW

    parsePrecedence(PREC_ASSIGNMENT);
}

static void block(void) {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
        declaration();
    consumeExp(TOKEN_RIGHT_BRACE, "'}'", "block");
}

static void function(FunctionType type) {
    Compiler     compiler;
    ObjFunction* function;
    int          i;
    uint8_t      parameter;

    CHECK_STACKOVERFLOW

    initCompiler(&compiler, type);
    beginScope();

    consumeExp(TOKEN_LEFT_PAREN, "'('", "function name");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            if (currentUnit->function->isVarArg)
                errorAtCurrent("No more parameters allowed after vararg.");
            if (++currentUnit->function->arity >= MAX_LOCALS)
                errorAtCurrent("Too many parameters.");
            if (match(TOKEN_DOT_DOT))
                currentUnit->function->isVarArg = true;
            parameter = parseVariable("Expect parameter name.");
            defineVariable(parameter);
        } while (match(TOKEN_COMMA));
    }
    consumeExp(TOKEN_RIGHT_PAREN, "')'", "parameter");

    if (match(TOKEN_ARROW)) {
        if (type == TYPE_INITIALIZER)
            error("Can't return a value from an initializer.");
        expression();
        emitByte(OP_RETURN);
        function = endCompiler(false);
    } else {
        consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
        block();
        function = endCompiler(true);
    }

    emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    for (i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal);
        emitByte(compiler.upvalues[i].index);
    }
}

static void method(void) {
    uint8_t      constant;
    FunctionType type;

    consume(TOKEN_IDENTIFIER, "Expect method name.");
    constant = identifierConstant(&parser.previous);
    type = TYPE_METHOD;
    if (parser.previous.length == 4 && fix_memcmp(parser.previous.start, "init", 4) == 0)
        type = TYPE_INITIALIZER;
    function(type);
    emitBytes(OP_METHOD, constant);
}

static void classDeclaration(void) {
    uint8_t       nameConstant;
    Token         className;
    ClassCompiler classCompiler;

    consume(TOKEN_IDENTIFIER, "Expect class name.");
    className = parser.previous;
    nameConstant = identifierConstant(&parser.previous);
    declareVariable();

    emitBytes(OP_CLASS, nameConstant);
    defineVariable(nameConstant);

    classCompiler.enclosing = currentClass;
    classCompiler.hasSuperclass = false;
    currentClass = &classCompiler;

    if (match(TOKEN_LESS)) {
        // should we allow an expression here?
        consume(TOKEN_IDENTIFIER, "Expect superclass name.");
        variable(false);

        if (identifiersEqual(&className, &parser.previous))
            error("A class can't inherit from itself.");

        beginScope();
        addLocal(syntheticToken("super"));
        defineVariable(0);

        namedVariable(className, false);
        emitByte(OP_INHERIT);
        classCompiler.hasSuperclass = true;
    }

    namedVariable(className, false);
    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
        method();
    consumeExp(TOKEN_RIGHT_BRACE, "'}'", "class body");
    emitByte(OP_POP);

    if (classCompiler.hasSuperclass)
        endScope();

    currentClass = currentClass->enclosing;
}

static void funDeclaration(void) {
    uint8_t fname = parseVariable("Expect function name.");

    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(fname);
}

static void varDeclaration(void) {
    uint8_t vname = parseVariable("Expect variable name.");

    if (match(TOKEN_EQUAL))
        expression();
    else
        emitByte(OP_NIL);
    consumeExp(TOKEN_SEMICOLON, "';'", "variable declaration");
    defineVariable(vname);
}

static void expressionStatement(void) {
    expression();
    consumeExp(TOKEN_SEMICOLON, "';'", "expression");
    emitByte(OP_POP);
}

static void forStatement(void) {
    int loopStart;
    int exitJump;
    int bodyJump;
    int incrementStart;

    beginScope();
    consumeExp(TOKEN_LEFT_PAREN, "'('", "'for'");
    if (match(TOKEN_SEMICOLON))
        {}// no initializer
    else if (match(TOKEN_VAR))
        varDeclaration();
    else
        expressionStatement();

    loopStart = currentChunk()->count;
    exitJump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consumeExp(TOKEN_SEMICOLON, "';'", "loop condition");
        // jump out of the loop if condition is false.
        exitJump = emitJump(OP_JUMP_FALSE);
    }

    if (!match(TOKEN_RIGHT_PAREN)) {
        bodyJump = emitJump(OP_JUMP);
        incrementStart = currentChunk()->count;
        expression();
        emitByte(OP_POP);
        consumeExp(TOKEN_RIGHT_PAREN, "')'", "for clauses");

        emitLoop(loopStart);
        loopStart = incrementStart;
        patchJump(bodyJump);
    }

    statement();
    emitLoop(loopStart);

    if (exitJump != -1) {
        patchJump(exitJump);
        emitByte(OP_POP); // condition
    }
    endScope();
}

static void ifStatement(void) {
    int thenJump, elseJump;

    consumeExp(TOKEN_LEFT_PAREN, "'('", "'if'");
    expression();
    consumeExp(TOKEN_RIGHT_PAREN, "')'", "condition");

    thenJump = emitJump(OP_JUMP_FALSE);
    statement();
    elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);
    emitByte(OP_POP);

    if (match(TOKEN_ELSE))
        statement();
    patchJump(elseJump);
}

static void printStatement(void) {
    if (match(TOKEN_SEMICOLON)) {
        emitConstant(OBJ_VAL(copyString("", 0)));
        emitByte(OP_PRINTLN);
    } else {
        expression();
        while (match(TOKEN_COMMA)) {
            emitByte(OP_PRINT);
            if (match(TOKEN_COMMA)) {
                emitConstant(OBJ_VAL(copyString(PRINT_SEPARATOR, sizeof(PRINT_SEPARATOR) - 1)));
                emitByte(OP_PRINT);
            }
            if (match(TOKEN_SEMICOLON))
                return;
            expression();
        }
        consumeExp(TOKEN_SEMICOLON, "';'", "expression");
        emitByte(OP_PRINTLN);
    }
}

static void returnStatement(void) {
    if (currentUnit->type == TYPE_SCRIPT)
        error("Can't return from top-level code.");

    if (match(TOKEN_SEMICOLON))
        emitReturn();
    else {
        if (currentUnit->type == TYPE_INITIALIZER)
            error("Can't return a value from an initializer.");
        expression();
        consumeExp(TOKEN_SEMICOLON, "';'", "return value");
        emitByte(OP_RETURN);
    }
}

static void whileStatement(void) {
    int loopStart = currentChunk()->count;
    int exitJump;

    consumeExp(TOKEN_LEFT_PAREN, "'('", "'while'");
    expression();
    consumeExp(TOKEN_RIGHT_PAREN, "')'", "condition");

    exitJump = emitJump(OP_JUMP_FALSE);
    statement();
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
}

static void synchronize(void) {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;

        // Original switch crashed the compiler, no idea, why....
        if (parser.current.type == TOKEN_CLASS)  return;
        if (parser.current.type == TOKEN_FUN)    return;
        if (parser.current.type == TOKEN_VAR)    return;
        if (parser.current.type == TOKEN_FOR)    return;
        if (parser.current.type == TOKEN_IF)     return;
        if (parser.current.type == TOKEN_WHILE)  return;
        if (parser.current.type == TOKEN_PRINT)  return;
        if (parser.current.type == TOKEN_RETURN) return;

        advance();
    }
}

static void declaration(void) {
    if      (match(TOKEN_CLASS))  classDeclaration();
    else if (match(TOKEN_FUN))    funDeclaration();
    else if (match(TOKEN_VAR))    varDeclaration();
    else                          statement();

    if (parser.panicMode)
        synchronize();
}

static void statement(void) {
    if      (match(TOKEN_PRINT))  printStatement();
    else if (match(TOKEN_FOR))    forStatement();
    else if (match(TOKEN_IF))     ifStatement();
    else if (match(TOKEN_RETURN)) returnStatement();
    else if (match(TOKEN_WHILE))  whileStatement();
    else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    }
    else
        expressionStatement();
}

ObjFunction* compile(const char* source) {
    Compiler     compiler;
    ObjFunction* function;

    vm.totallyAllocated = 0;
    vm.numGCs = 0;
    errno = 0;

    initScanner(source);
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError  = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF))
        declaration();

    function = endCompiler(true);
    return parser.hadError ? NULL : function;
}

void markCompilerRoots(void) {
    Compiler* compiler = currentUnit;
    while (compiler != NULL) {
        markObject((Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}
