#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "disasm.h"
#include "memory.h"
#include "scanner.h"

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
    PREC_FACTOR,     // * \ / 
    PREC_UNARY,      // ! -
    PREC_POSTFIX,    // . () [] @ ^
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
    ParseFn    prefix;
    ParseFn    infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token   name;
    int8_t  depth;
    uint8_t isCaptured;
    int16_t _dummy; // force size 16 to avoid multiplication in offset calculation 
} Local;

typedef enum {
    TYPE_SCRIPT,
    TYPE_FUN,
    TYPE_LAMBDA,
    TYPE_METHOD,     // allowed in classes only 
    TYPE_INIT,
} FunctionType;

typedef struct LoopInfo {
    struct LoopInfo* enclosing;
    int8_t           scopeDepth;
    int8_t           breakCount;
    int16_t          breaks[MAX_BREAKS];
} LoopInfo;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction*     target;
    FunctionType     type;
    Local            locals[MAX_LOCALS];
    Upvalue          upvalues[MAX_UPVALUES];
    int8_t           scopeDepth;
    uint8_t          localCount;
    LoopInfo*        currentLoop;
} Compiler;

typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
    bool                  hasSuperclass;
} ClassCompiler;

static Parser         parser;
static Compiler*      currentComp;
static ClassCompiler* currentClass;

static Chunk* currentChunk(void) {
    return &currentComp->target->chunk;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Error reporting
////////////////////////////////////////////////////////////////////////////////////////////////////

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode)
        return;
    parser.panicMode = true;
    printf("[line %d] Error", token->line);

    if (token->type == TOKEN_EOF)
        putstr(" at end");
    else if (token->type != TOKEN_ERROR) {
        // IDE68K's printf("%.*s") is broken
        putstr(" at '");
        putstrn(token->length, token->start);
        putstr("'");
    }     

    printf(": %s\n", message);
    parser.hadError = true;
}

static void error(const char* message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Token matching
////////////////////////////////////////////////////////////////////////////////////////////////////

static void advance(void) {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR)
            break;
        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType expected, const char* message) {
    if (parser.current.type == expected) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

static void consumeExp(TokenType expected, const char* context) {
    if (parser.current.type == expected) {
        advance();
        return;
    }
    sprintf(buffer, "Expect '%c' %s %s.",
            TOKEN_CHARS[expected],
            expected == TOKEN_LEFT_BRACE || expected == TOKEN_LEFT_PAREN ? "before" : "after",
            context);
    errorAtCurrent(buffer);
}

static bool check(TokenType expected) {
    return parser.current.type == expected;
}

static bool match(TokenType expected) {
    if (parser.current.type != expected)
        return false;
    advance();
    return true;
}

static void synchronize(void) {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON ||
            parser.current.type  >= TOKEN_BREAK)    // Tokens to synchronize on
            return;
        advance();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Code emitting
////////////////////////////////////////////////////////////////////////////////////////////////////

static void emitByte(int byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emit2Bytes(int byte1, int byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emit3Bytes(int byte1, int byte2, int byte3) {
    emitByte(byte1);
    emitByte(byte2);
    emitByte(byte3);
}

static void emitLoop(int loopStart) {
    int offset;
    emitByte(OP_LOOP);

    offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX)
        error("Jump too large.");
    emit2Bytes(offset >> 8, offset);
}

static int emitJump(int instruction) {
    emit3Bytes(instruction, 0xff, 0xff);
    return currentChunk()->count - 2;
}

static void emitReturn(void) {
    if (currentComp->type == TYPE_INIT)
        emit3Bytes(OP_GET_LOCAL, 0, OP_RETURN);
    else
        emitByte(OP_RETURN_NIL);
}

static int makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }
    return constant;
}

static void emitConstant(Value value) {
    if      (value == INT_VAL(0)) emitByte(OP_ZERO);
    else if (value == INT_VAL(1)) emitByte(OP_ONE);
    else                          emit2Bytes(OP_CONSTANT, makeConstant(value));
}

static void patchJump(int offset) {
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX)
        error("Jump too large.");
    currentChunk()->code[offset]     = jump >> 8;
    currentChunk()->code[offset + 1] = jump;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Compiler scoping
////////////////////////////////////////////////////////////////////////////////////////////////////

static void initCompiler(Compiler* compiler, FunctionType type) {
    Local* local;
    compiler->enclosing   = currentComp;
    compiler->target      = NULL;
    compiler->type        = type;
    compiler->localCount  = 0;
    compiler->scopeDepth  = 0;
    compiler->target      = makeFunction();
    compiler->currentLoop = NULL;
    currentComp           = compiler;

    if (type != TYPE_SCRIPT) {
        if (type == TYPE_LAMBDA)
            currentComp->target->name = INT_VAL(vm.lambdaCount++);
        else
            currentComp->target->name =
                OBJ_VAL(makeString(parser.previous.start, parser.previous.length));
    }

    local = &currentComp->locals[currentComp->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    if (type >= TYPE_METHOD) {
        local->name.start  = "this";
        local->name.length = 4;
    } else {
        local->name.start  = "";
        local->name.length = 0;
    }
}

static ObjFunction* endCompiler(bool addReturn) {
    ObjFunction* function = currentComp->target;
    if (addReturn)
        emitReturn();
    freezeChunk(currentChunk());

#ifdef LOX_DBG
    if (vm.debug_print_code && !parser.hadError)
        disassembleChunk(currentChunk(), functionName(function));
#endif

    currentComp = currentComp->enclosing;
    return function;
}

static void beginScope(void) {
    currentComp->scopeDepth++;
}

static void endScope(void) {
    currentComp->scopeDepth--;
    while (currentComp->localCount > 0 &&
           currentComp->locals[currentComp->localCount - 1].depth > currentComp->scopeDepth) {
        if (currentComp->locals[currentComp->localCount - 1].isCaptured)
            emitByte(OP_CLOSE_UPVALUE);
        else
            emitByte(OP_POP);
        currentComp->localCount--;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Forward declarations
////////////////////////////////////////////////////////////////////////////////////////////////////

static void  expression(void);
static void  statement(bool topLevel);
static void  declaration(bool topLevel);
static void  function(FunctionType type);
static void  parsePrecedence(Precedence precedence);
static int   argumentList(bool* isVarArg, TokenType terminator);
static int   identifierConstant(Token* name);
static const ParseRule* getRule(TokenType type);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Expressions
////////////////////////////////////////////////////////////////////////////////////////////////////

static void binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    const ParseRule* rule  = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));
    switch (operatorType) {
        case TOKEN_BANG_EQUAL:    emit2Bytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL); break;
        case TOKEN_GREATER:       emit2Bytes(OP_SWAP, OP_LESS); break;
        case TOKEN_LESS_EQUAL:    emit3Bytes(OP_SWAP, OP_LESS, OP_NOT); break;
        case TOKEN_LESS:          emitByte(OP_LESS); break;
        case TOKEN_GREATER_EQUAL: emit2Bytes(OP_LESS, OP_NOT); break;
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
    int  argCount = argumentList(&isVarArg, TOKEN_RIGHT_PAREN);
    if (isVarArg)
        emit2Bytes(OP_VCALL, argCount);
    else if (argCount <= 2)
        emitByte(OP_CALL0 + argCount); // special case 0, 1, or 2 args
    else
        emit2Bytes(OP_CALL, argCount);
}

static void dot(bool canAssign) {
    int  pname;
    int  argCount;
    bool isVarArg = false;

    consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    pname = identifierConstant(&parser.previous);

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emit2Bytes(OP_SET_PROPERTY, pname);
    } else if (match(TOKEN_LEFT_PAREN)) {
        argCount = argumentList(&isVarArg, TOKEN_RIGHT_PAREN);
        emit3Bytes(isVarArg ? OP_VINVOKE : OP_INVOKE, pname, argCount);
    } else
        emit2Bytes(OP_GET_PROPERTY, pname);
}

static void slice(bool canAssign) {
    if (match(TOKEN_RIGHT_BRACKET))
        emitConstant(INT_VAL(INT32_MAX>>1));
    else {
        expression();
        consumeExp(TOKEN_RIGHT_BRACKET, "slice");
    }
    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
        return;
    } else
        emitByte(OP_GET_SLICE);
}

static void index_(bool canAssign) {
    if (match(TOKEN_COLON)) {
        emitConstant(INT_VAL(0));
        slice(canAssign);
    } else {
        expression();
        if (match(TOKEN_COLON))
            slice(canAssign);
        else {
            consumeExp(TOKEN_RIGHT_BRACKET, "index");
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

static void nil_(bool canAssign) {
    emitByte(OP_NIL);
}

static void false_(bool canAssign) {
    emitByte(OP_FALSE);
}

static void true_(bool canAssign) {
    emitByte(OP_TRUE);
}

static void grouping(bool canAssign) {
    expression();
    consumeExp(TOKEN_RIGHT_PAREN, "expression");
}

static void intNum(bool canAssign) {
    Value value = parseInt(parser.previous.start, false);
    emitConstant(value);
}

static void realNum(bool canAssign) {
    Value value;
    errno = 0;
    value = makeReal(strToReal(parser.previous.start, NULL));
    if (errno != 0)
        error("Real constant overflow.");
    emitConstant(value);
}

static void string(bool canAssign) {
    // Redundant code in IDE68K without str local variable?
    ObjString* str = makeString(parser.previous.start + 1, parser.previous.length - 2);
    emitConstant(OBJ_VAL(str));
}

static int identifierConstant(Token* name) {
    // Redundant code in IDE68K without str local variable?
    ObjString* str = makeString(name->start, name->length);
    return makeConstant(OBJ_VAL(str));
}

static void list(bool canAssign) {
    bool isVarArg = false;
    int  argCount = argumentList(&isVarArg, TOKEN_RIGHT_BRACKET);
    emit2Bytes(isVarArg ? OP_VLIST : OP_LIST, argCount);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Variable handling
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length)
        return false;
    return fix_memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
    int    i;
    Local* local;

    for (i = compiler->localCount - 1; i >= 0; i--) {
        local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1)
                error("Can't read local variable in its own initializer.");
            return i;
        }
    }
    return -1;
}

static int addUpvalue(Compiler* compiler, int index, bool isLocal) {
    int      upvalueCount = compiler->target->upvalueCount;
    Upvalue  newUpvalue   = isLocal ? (index | LOCAL_MASK) : index;
    int      i;

    for (i = 0; i < upvalueCount; i++)
        if (compiler->upvalues[i] == newUpvalue)
            return i;

    if (upvalueCount == MAX_UPVALUES) {
        error("Too many upvalues in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount] = newUpvalue; 
    return compiler->target->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {
    int local, upvalue;

    if (compiler->enclosing == NULL)
        return -1;

    local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, local, true);
    }

    upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1)
        return addUpvalue(compiler, upvalue, false);

    return -1;
}

static void addLocal(Token name) {
    Local* local;
    if (currentComp->localCount == MAX_LOCALS) {
        error("Too many local variables in function.");
        return;
    }
    local = &currentComp->locals[currentComp->localCount++];
    local->name       = name;
    local->depth      = -1;
    local->isCaptured = false;
}

static void declareVariable(void) {
    Token* name;
    int    i;
    Local* local;

    if (currentComp->scopeDepth == 0)
        return;

    name = &parser.previous;
    for (i = currentComp->localCount - 1; i >= 0; i--) {
        local = &currentComp->locals[i];
        if (local->depth != -1 && local->depth < currentComp->scopeDepth)
            break;

        if (identifiersEqual(name, &local->name))
            error("Already a variable with this name in this scope.");
    }
    addLocal(*name);
}

static void namedVariable(Token name, bool canAssign) {
    int getOp, setOp;
    int arg = resolveLocal(currentComp, &name);

    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(currentComp, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }
    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emit2Bytes(setOp, arg);
    } else
        emit2Bytes(getOp, arg);
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Expressions continued
////////////////////////////////////////////////////////////////////////////////////////////////////

static Token syntheticToken(const char* text) {
    Token token;
    token.start  = text;
    token.length = (int)strlen(text);
    return token;
}

static void super_(bool canAssign) {
    int  mname;
    int  argCount;
    bool isVarArg = false;

    if (currentClass == NULL)
        error("Can't use 'super' outside of a class.");
    else if (!currentClass->hasSuperclass)
        error("Can't use 'super' in a class with no superclass.");

    consumeExp(TOKEN_DOT, "'super'");
    consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
    mname = identifierConstant(&parser.previous);

    namedVariable(syntheticToken("this"), false);
    if (match(TOKEN_LEFT_PAREN)) {
        argCount = argumentList(&isVarArg, TOKEN_RIGHT_PAREN);
        namedVariable(syntheticToken("super"), false);
        emit3Bytes(isVarArg ? OP_VSUPER_INVOKE : OP_SUPER_INVOKE, mname, argCount);
    } else {
        namedVariable(syntheticToken("super"), false);
        emit2Bytes(OP_GET_SUPER, mname);
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
    int endJump = emitJump(OP_JUMP_AND);
    parsePrecedence(PREC_AND);
    patchJump(endJump);
}

static void or_(bool canAssign) {
    int endJump = emitJump(OP_JUMP_OR);
    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

static void not(bool canAssign) {
    parsePrecedence(PREC_UNARY);
    emitByte(OP_NOT);
}

static void negative(bool canAssign) {
    emitConstant(INT_VAL(0)); 
    parsePrecedence(PREC_UNARY);
    emitByte(OP_SUB);
}

static void lambda(bool canAssign) {
    function(TYPE_LAMBDA);
}

static void buildThunk(void) {
    Compiler     compiler;
    ObjFunction* function;
    int          i;

    initCompiler(&compiler, TYPE_LAMBDA);
    beginScope();
    expression();
    emitByte(OP_RETURN);
    function = endCompiler(false);

    emit2Bytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));
    for (i = 0; i < function->upvalueCount; i++) 
        emitByte(compiler.upvalues[i]);
}

static void handler(bool canAssign) {
    consumeExp(TOKEN_LEFT_PAREN, "expression");
    buildThunk();
    consumeExp(TOKEN_COLON, "expression");

    expression();
    consumeExp(TOKEN_RIGHT_PAREN, "handler");
    emitByte(OP_CALL_HAND);
}

static void ifExpr(bool canAssign) {
    int thenJump, elseJump;

    consumeExp(TOKEN_LEFT_PAREN, "condition");
    expression();
    consumeExp(TOKEN_COLON, "condition");
    thenJump = emitJump(OP_JUMP_FALSE);

    expression();
    consumeExp(TOKEN_COLON, "consequent");
    elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);

    expression();
    patchJump(elseJump);
    consumeExp(TOKEN_RIGHT_PAREN, "alternative");
}

static void dynvar(bool canAssign) {
    int vname;

    consumeExp(TOKEN_LEFT_PAREN, "variable");
    consume(TOKEN_IDENTIFIER, "Expect variable.");

    vname = identifierConstant(&parser.previous);
    consumeExp(TOKEN_EQUAL, "variable");

    expression();
    consumeExp(TOKEN_COLON, "binding");

    buildThunk();
    consumeExp(TOKEN_RIGHT_PAREN, "expression");
    emit2Bytes(OP_CALL_BIND, vname);
}


static const ParseRule rules[] = {
    // Keep same order as TokenType enum values in scanner.h
    // since C89 doesn't support array init by index values.

    // single character punctuation ***********************************
    /* [TOKEN_LEFT_PAREN]    = */ {grouping, call,   PREC_POSTFIX},
    /* [TOKEN_RIGHT_PAREN]   = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_LEFT_BRACE]    = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_RIGHT_BRACE]   = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_LEFT_BRACKET]  = */ {list,     index_, PREC_POSTFIX},
    /* [TOKEN_RIGHT_BRACKET] = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_COMMA]         = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_DOT]           = */ {NULL,     dot,    PREC_POSTFIX},
    /* [TOKEN_SEMICOLON]     = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_COLON]         = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_EQUAL]         = */ {NULL,     NULL,   PREC_NONE},

    // single character operators *************************************
    /* [TOKEN_PLUS]          = */ {NULL,     binary, PREC_TERM},
    /* [TOKEN_MINUS]         = */ {negative, binary, PREC_TERM},
    /* [TOKEN_STAR]          = */ {NULL,     binary, PREC_FACTOR},
    /* [TOKEN_SLASH]         = */ {NULL,     binary, PREC_FACTOR},
    /* [TOKEN_BACKSLASH]     = */ {NULL,     binary, PREC_FACTOR},
    /* [TOKEN_AT]            = */ {NULL,     iter,   PREC_POSTFIX},
    /* [TOKEN_HAT]           = */ {NULL,     iter,   PREC_POSTFIX},
    /* [TOKEN_BANG]          = */ {not,      NULL,   PREC_NONE},
    /* [TOKEN_GREATER]       = */ {NULL,     binary, PREC_COMPARISON},
    /* [TOKEN_LESS]          = */ {NULL,     binary, PREC_COMPARISON},

    // double character operators *************************************
    /* [TOKEN_BANG_EQUAL]    = */ {NULL,     binary, PREC_EQUALITY},
    /* [TOKEN_EQUAL_EQUAL]   = */ {NULL,     binary, PREC_EQUALITY},
    /* [TOKEN_GREATER_EQUAL] = */ {NULL,     binary, PREC_COMPARISON},
    /* [TOKEN_LESS_EQUAL]    = */ {NULL,     binary, PREC_COMPARISON},
    /* [TOKEN_DOT_DOT]       = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_ARROW]         = */ {NULL,     NULL,   PREC_NONE},

    // literals *******************************************************
    /* [TOKEN_IDENTIFIER]    = */ {variable, NULL,   PREC_NONE},
    /* [TOKEN_STRING]        = */ {string,   NULL,   PREC_NONE},
    /* [TOKEN_INT]           = */ {intNum,   NULL,   PREC_NONE},
    /* [TOKEN_REAL]          = */ {realNum,  NULL,   PREC_NONE},

    // specials *******************************************************
    /* [TOKEN_ERROR]         = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_EOF]           = */ {NULL,     NULL,   PREC_NONE},

    // non-syncing keywords *******************************************
    /* [TOKEN_AND]           = */ {NULL,     and_,   PREC_AND},
    /* [TOKEN_DYNVAR]        = */ {dynvar,   NULL,   PREC_NONE},
    /* [TOKEN_ELSE]          = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_FALSE]         = */ {false_,   NULL,   PREC_NONE},
    /* [TOKEN_HANDLE]        = */ {handler,  NULL,   PREC_NONE},
    /* [TOKEN_NIL]           = */ {nil_,     NULL,   PREC_NONE},
    /* [TOKEN_OR]            = */ {NULL,     or_,    PREC_OR},
    /* [TOKEN_SUPER]         = */ {super_,   NULL,   PREC_NONE},
    /* [TOKEN_THIS]          = */ {this_,    NULL,   PREC_NONE},
    /* [TOKEN_TRUE]          = */ {true_,    NULL,   PREC_NONE},
    /* [TOKEN_WHEN]          = */ {NULL,     NULL,   PREC_NONE},

    // syncing keywords ***********************************************
    /* [TOKEN_BREAK]         = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_CASE]          = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_CLASS]         = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_FOR]           = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_FUN]           = */ {lambda,   NULL,   PREC_NONE},
    /* [TOKEN_IF]            = */ {ifExpr,   NULL,   PREC_NONE},
    /* [TOKEN_PRINT]         = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_RETURN]        = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_VAR]           = */ {NULL,     NULL,   PREC_NONE},
    /* [TOKEN_WHILE]         = */ {NULL,     NULL,   PREC_NONE},
};

static void parsePrecedence(Precedence precedence) {
    ParseFn prefixRule;
    ParseFn infixRule;
    bool    canAssign;

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

static int parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (currentComp->scopeDepth > 0)
        return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialized(void) {
    if (currentComp->scopeDepth == 0)
        return;
    currentComp->locals[currentComp->localCount - 1].depth = currentComp->scopeDepth;
}

static void defineVariable(int global) {
    if (currentComp->scopeDepth > 0) {
        markInitialized();
        return;
    }
    emit2Bytes(OP_DEF_GLOBAL, global);
}

static int argumentList(bool* isVarArg, TokenType terminator) {
    int argCount = 0;

    if (!check(terminator)) {
        do {
            if (match(TOKEN_DOT_DOT)) {
                // First UNPACK, introduce count of arguments from lists
                if (!*isVarArg)
                    emitConstant(INT_VAL(0)); 
                *isVarArg = true;
                expression();
                emitByte(OP_UNPACK);   // this also adapts list arguments count
            } else {
                expression();
                if (*isVarArg)
                    emitByte(OP_SWAP); // bubble list arguments count to TOS
                if (argCount == UINT8_MAX)
                    error("Too many arguments.");
                argCount++;
            }
        } while (match(TOKEN_COMMA));
    }
    consumeExp(terminator, "arguments");
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
        declaration(false);
    consumeExp(TOKEN_RIGHT_BRACE, "block");
}

static void function(FunctionType type) {
    Compiler     compiler;
    ObjFunction* function;
    int          i;
    int          parameter;
    uint8_t      restParm = 0;

    CHECK_STACKOVERFLOW
    initCompiler(&compiler, type);

    beginScope();
    consumeExp(TOKEN_LEFT_PAREN, "parameters");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            if (restParm)
                errorAtCurrent("No more parameters allowed after rest parameter.");
            if (++currentComp->target->arity >= MAX_LOCALS)
                errorAtCurrent("Too many parameters.");
            if (match(TOKEN_DOT_DOT))
                restParm = REST_PARM_MASK;
            parameter = parseVariable("Expect parameter name.");
            defineVariable(parameter);
        } while (match(TOKEN_COMMA));
    }
    consumeExp(TOKEN_RIGHT_PAREN, "parameters");
    currentComp->target->arity |= restParm;        

    if (match(TOKEN_ARROW)) {
        if (type == TYPE_INIT)
            error("Can't return a value from an initializer.");
        expression();
        emitByte(OP_RETURN);
        function = endCompiler(false);
    } else {
        consumeExp(TOKEN_LEFT_BRACE, "function body");
        block();
        function = endCompiler(true);
    }

    emit2Bytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));
    for (i = 0; i < function->upvalueCount; i++) 
        emitByte(compiler.upvalues[i]);
}

static void method(void) {
    int          mname;
    FunctionType type = TYPE_METHOD;

    consume(TOKEN_IDENTIFIER, "Expect method name.");
    mname = identifierConstant(&parser.previous);
    if (parser.previous.length == 4 && fix_memcmp(parser.previous.start, "init", 4) == 0)
        type = TYPE_INIT;
    function(type);
    emit2Bytes(OP_METHOD, mname);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////////////////////////

static void classDeclaration(void) {
    int           nameConstant;
    Token         className;
    ClassCompiler classCompiler;

    consume(TOKEN_IDENTIFIER, "Expect class name.");
    className = parser.previous;
    nameConstant = identifierConstant(&className);
    declareVariable();

    emit2Bytes(OP_CLASS, nameConstant);
    defineVariable(nameConstant);

    classCompiler.enclosing     = currentClass;
    classCompiler.hasSuperclass = false;
    currentClass = &classCompiler;

    if (match(TOKEN_LESS)) {
        // superclass can be any expression!
        expression(); 

        beginScope();
        addLocal(syntheticToken("super"));
        defineVariable(0);

        namedVariable(className, false);
        emitByte(OP_INHERIT);
        classCompiler.hasSuperclass = true;
    }

    namedVariable(className, false);
    consumeExp(TOKEN_LEFT_BRACE, "class body");
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
        method();
    consumeExp(TOKEN_RIGHT_BRACE, "class body");
    emitByte(OP_POP);

    if (classCompiler.hasSuperclass)
        endScope();

    currentClass = currentClass->enclosing;
}

static void funDeclaration(void) {
    int fname = parseVariable("Expect function name.");

    markInitialized();
    function(TYPE_FUN);
    defineVariable(fname);
}

static void varDeclaration(void) {
    int vname;

    do {
        vname = parseVariable("Expect variable name.");
        if (match(TOKEN_EQUAL))
            expression();
        else
            emitByte(OP_NIL);
        defineVariable(vname);
    } while (match(TOKEN_COMMA));
    consumeExp(TOKEN_SEMICOLON, "variable declarations");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Statements
////////////////////////////////////////////////////////////////////////////////////////////////////

static void expressionStatement(bool topLevel) {
    expression();
    if (topLevel)
        emitByte(match(TOKEN_SEMICOLON) ? OP_POP : OP_PRINTQ);
    else {
        consumeExp(TOKEN_SEMICOLON, "expression");
        emitByte(OP_POP);
    }
}

static void initBreaks(LoopInfo* loop) {
    loop->enclosing          = currentComp->currentLoop;
    loop->scopeDepth         = currentComp->scopeDepth;
    loop->breakCount         = 0;
    currentComp->currentLoop = loop;
}

static void patchBreaks(LoopInfo* loop) {
    while (loop->breakCount)
        patchJump(loop->breaks[--loop->breakCount]);
    currentComp->currentLoop = loop->enclosing;
}

static void forStatement(void) {
    int      loopStart;
    int      exitJump;
    int      bodyJump;
    int      incrementStart;
    LoopInfo loopInfo;

    CHECK_STACKOVERFLOW

    beginScope();
    initBreaks(&loopInfo);

    consumeExp(TOKEN_LEFT_PAREN, "'for' clauses");
    if (match(TOKEN_SEMICOLON))
        {}// no initializer
    else if (match(TOKEN_VAR))
        varDeclaration();
    else
        expressionStatement(false);

    loopStart = currentChunk()->count;
    exitJump  = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consumeExp(TOKEN_SEMICOLON, "loop condition");
        // jump out of the loop if condition is false.
        exitJump = emitJump(OP_JUMP_FALSE);
    }

    if (!match(TOKEN_RIGHT_PAREN)) {
        bodyJump       = emitJump(OP_JUMP);
        incrementStart = currentChunk()->count;
        expression();
        emitByte(OP_POP);
        consumeExp(TOKEN_RIGHT_PAREN, "'for' clauses");

        emitLoop(loopStart);
        loopStart = incrementStart;
        patchJump(bodyJump);
    }

    statement(false);
    emitLoop(loopStart);

    if (exitJump != -1)
        patchJump(exitJump);

    patchBreaks(&loopInfo);
    endScope();
}

static void ifStatement(void) {
    int thenJump, elseJump;

    consumeExp(TOKEN_LEFT_PAREN, "condition");
    expression();
    consumeExp(TOKEN_RIGHT_PAREN, "condition");

    thenJump = emitJump(OP_JUMP_FALSE);
    statement(false);

    if (match(TOKEN_ELSE)) {
        elseJump = emitJump(OP_JUMP);
        patchJump(thenJump);
        statement(false);
        patchJump(elseJump);
    } else
        patchJump(thenJump);
}

static void caseStatement(void) {
    int       state        = 0; // 0: before 'when', 1: before 'else', 2: after 'else'
    int       caseCount    = 0;
    int       labelCount   = 0;
    int       prevCaseSkip = -1;
    bool      emptyBranch  = false;
    int16_t   caseEnds[MAX_BRANCHES];
    int16_t   whenLabels[MAX_LABELS];
    TokenType caseType;

    CHECK_STACKOVERFLOW

    consumeExp(TOKEN_LEFT_PAREN, "'case' expression");
    expression();
    consumeExp(TOKEN_RIGHT_PAREN, "'case' expression");
    consumeExp(TOKEN_LEFT_BRACE, "branches");

    beginScope();
    // reserve one stack slot for case test value
    addLocal(syntheticToken("case"));
    defineVariable(0);

    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        if (match(TOKEN_WHEN) || match(TOKEN_ELSE)) {
            if (emptyBranch)
                error("Can't have empty branch.");
            emptyBranch = true;
            caseType    = parser.previous.type;
            if (state == 2)
                error("Can't have another branch after 'else'.");
            if (state == 1) {
                // at end of previous case, jump over the others.
                if (caseCount < MAX_BRANCHES)
                    caseEnds[caseCount++] = emitJump(OP_JUMP);
                else
                    error("Too many case branches.");
                // patch its condition to jump to the next (= this case).
                patchJump(prevCaseSkip);
            }
            if (caseType == TOKEN_WHEN) {
                state = 1;
                do {
                    emitByte(OP_DUP);
                    expression();
                    emitByte(OP_EQUAL);
                    if (check(TOKEN_COMMA)) {
                        // jump over other label tests to statement
                        if (labelCount < MAX_LABELS)
                            whenLabels[labelCount++] = emitJump(OP_JUMP_TRUE);
                        else
                            errorAtCurrent("Too many 'when' labels.");
                    } 
                } while (match(TOKEN_COMMA)); 
                consumeExp(TOKEN_COLON, "expression");
                prevCaseSkip = emitJump(OP_JUMP_FALSE);
            } else {
                state        = 2;
                prevCaseSkip = -1;
            }
        } else {
            // Statement inside current branch
            if (state == 0)
                errorAtCurrent("Can't have statement before any branch.");
            // Fix jumps from previous labels, will execute on first statement in branch only (!)
            while (labelCount)
                patchJump(whenLabels[--labelCount]);
            statement(false);
            emptyBranch = false;
        }
    }      
    consumeExp(TOKEN_RIGHT_BRACE, "branches");
    if (emptyBranch)
        error("Can't have empty branch.");

    // if we ended without 'else' branch, patch its condition jump.
    if (state == 1)
        patchJump(prevCaseSkip);

    while (caseCount)
        patchJump(caseEnds[--caseCount]);

    endScope();
}

static void printStatement(void) {
    if (match(TOKEN_SEMICOLON)) {
        emitConstant(OBJ_VAL(makeString0("")));
        emitByte(OP_PRINTLN);
    } else {
        expression();
        while (match(TOKEN_COMMA)) {
            emitByte(OP_PRINT);
            if (match(TOKEN_COMMA)) {
                emitConstant(OBJ_VAL(makeString0(PRINT_SEPARATOR)));
                emitByte(OP_PRINT);
            }
            if (match(TOKEN_SEMICOLON))
                return;
            expression();
        }
        consumeExp(TOKEN_SEMICOLON, "expression");
        emitByte(OP_PRINTLN);
    }
}

static void returnStatement(void) {
    if (currentComp->type == TYPE_SCRIPT)
        error("Can't return from top-level code.");

    if (match(TOKEN_SEMICOLON))
        emitReturn();
    else {
        if (currentComp->type == TYPE_INIT)
            error("Can't return a value from an initializer.");
        expression();
        consumeExp(TOKEN_SEMICOLON, "return value");
        emitByte(OP_RETURN);
    }
}

static void whileStatement(void) {
    int      loopStart = currentChunk()->count;
    int      exitJump;
    LoopInfo loopInfo;

    CHECK_STACKOVERFLOW

    consumeExp(TOKEN_LEFT_PAREN, "condition");
    expression();
    consumeExp(TOKEN_RIGHT_PAREN, "condition");

    initBreaks(&loopInfo);
    exitJump = emitJump(OP_JUMP_FALSE);
    statement(false);
    emitLoop(loopStart);

    patchJump(exitJump);
    patchBreaks(&loopInfo);
}

static void breakStatement(void) {
    LoopInfo* loop = currentComp->currentLoop;
    int i;

    if (loop) {
        consumeExp(TOKEN_SEMICOLON, "'break'");

        // discard all variables upto loop broken 
        i = currentComp->localCount - 1;
        for (; i >= 0 && currentComp->locals[i].depth > loop->scopeDepth; i--)
            emitByte(currentComp->locals[i].isCaptured ? OP_CLOSE_UPVALUE : OP_POP);

        if (loop->breakCount < MAX_BREAKS)
            loop->breaks[loop->breakCount++] = emitJump(OP_JUMP);
        else
            error("Too many 'break's in loop.");
    } else
        error("Not in a loop.");
}

static void declaration(bool topLevel) {
    if      (match(TOKEN_CLASS))  classDeclaration();
    else if (match(TOKEN_FUN))    funDeclaration();
    else if (match(TOKEN_VAR))    varDeclaration();
    else                          statement(topLevel);

    if (parser.panicMode)
        synchronize();
}

static void statement(bool topLevel) {
    if      (match(TOKEN_PRINT))  printStatement();
    else if (match(TOKEN_FOR))    forStatement();
    else if (match(TOKEN_IF))     ifStatement();
    else if (match(TOKEN_RETURN)) returnStatement();
    else if (match(TOKEN_WHILE))  whileStatement();
    else if (match(TOKEN_CASE))   caseStatement();
    else if (match(TOKEN_BREAK))  breakStatement();
    else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    }
    else
        expressionStatement(topLevel);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Compiler entry
////////////////////////////////////////////////////////////////////////////////////////////////////

ObjFunction* compile(const char* source) {
    Compiler     compiler;
    ObjFunction* function;

#ifdef LOX_DBG
    STATIC_BREAKPOINT();
    vm.totallyAllocated = 0;
    vm.numGCs           = 0;
#endif

    initScanner(source);
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError  = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF))
        declaration(true);

    function = endCompiler(true);
    return parser.hadError ? NULL : function;
}

void markCompilerRoots(void) {
    Compiler* compiler = currentComp;
    while (compiler != NULL) {
        markObject((Obj*)compiler->target);
        compiler = compiler->enclosing;
    }
}
