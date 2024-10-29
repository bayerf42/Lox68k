#include <stdio.h>

#ifdef LOX_DBG

#include "disasm.h"
#include "object.h"

static int    offset;
static Chunk* chunk;
static void disassembleIntern(void);

void disassembleChunk(Chunk* pChunk, const char* name) {
    chunk = pChunk;
    printf("== %s ==\n", name);
    for (offset = 0; offset < chunk->count;)
        disassembleIntern();
}

int disassembleInst(Chunk* pChunk, int pOffset) {
    chunk  = pChunk;
    offset = pOffset;
    disassembleIntern();
    return offset;
}

static void simpleInst(const char* name) {
    putstr(name);
    ++offset;
}

static void byteInst(const char* name) {
    int arg = chunk->code[++offset];
    printf("%-9s %4d", name, arg);
    ++offset;
}

static void jumpInst(const char* name, int sign) {
    int jump = chunk->code[++offset] << 8;
    jump    |= chunk->code[++offset];
    printf("%-9s %4d ; -> %d", name, jump, ++offset + ((sign>0) ? jump : -jump));
}

static void constInst(const char* name) {
    int constant = chunk->code[++offset];
    printf("%-9s %4d ; ", name, constant);
    printValue(chunk->constants.values[constant], PRTF_MACHINE | PRTF_COMPACT);
    ++offset;
}

static void invokeInst(const char* name) {
    int constant = chunk->code[++offset];
    int argCount = chunk->code[++offset];
    printf("%-9s %4d ; ", name, constant);
    printValue(chunk->constants.values[constant], PRTF_MACHINE | PRTF_COMPACT);
    printf(" (%d args)", argCount);
    ++offset;
}

static void closureInst(const char* name) {
    int          constant = chunk->code[++offset];
    ObjFunction* function = AS_FUNCTION(chunk->constants.values[constant]);
    int          j, upvalue;
    printf("%-9s %4d ; ", name, constant);
    printValue(chunk->constants.values[constant], PRTF_MACHINE | PRTF_COMPACT);
    for (j = 0, ++offset; j < function->upvalueCount; j++, ++offset) {
        upvalue = chunk->code[offset];
        printf("\n%04d    |   %5s   %4d", offset,
               UV_ISLOC(upvalue) ? "LOCAL" : "UPVAL", UV_INDEX(upvalue));
    }
}

static void disassembleIntern(void) {
    int opcd = chunk->code[offset];
    int line = getLine(chunk, offset);

    printf("%04d ", offset);
    if (offset > 0 && line == getLine(chunk, offset - 1))
        putstr("   | ");
    else
        printf("%4d ", line);

    switch (opcd) {
        case OP_CONSTANT:      constInst("CONST");      break;
        case OP_NIL:           simpleInst("NIL");       break;
        case OP_TRUE:          simpleInst("TRUE");      break;
        case OP_FALSE:         simpleInst("FALSE");     break;
        case OP_ZERO:          simpleInst("ZERO");      break;
        case OP_ONE:           simpleInst("ONE");       break;
        case OP_POP:           simpleInst("POP");       break;
        case OP_SWAP:          simpleInst("SWAP");      break;
        case OP_DUP:           simpleInst("DUP");       break;
        case OP_GET_LOCAL:     byteInst("GET_LOC");     break;
        case OP_SET_LOCAL:     byteInst("SET_LOC");     break;
        case OP_GET_GLOBAL:    constInst("GET_GLOB");   break;
        case OP_DEF_GLOBAL:    constInst("DEF_GLOB");   break;
        case OP_SET_GLOBAL:    constInst("SET_GLOB");   break;
        case OP_GET_UPVALUE:   byteInst("GET_UPVAL");   break;
        case OP_SET_UPVALUE:   byteInst("SET_UPVAL");   break;
        case OP_GET_PROPERTY:  constInst("GET_PROP");   break;
        case OP_SET_PROPERTY:  constInst("SET_PROP");   break;
        case OP_GET_SUPER:     constInst("GET_SUPER");  break;
        case OP_EQUAL:         simpleInst("EQUAL");     break;
        case OP_LESS:          simpleInst("LESS");      break;
        case OP_ADD:           simpleInst("ADD");       break;
        case OP_SUB:           simpleInst("SUB");       break;
        case OP_MUL:           simpleInst("MUL");       break;
        case OP_DIV:           simpleInst("DIV");       break;
        case OP_MOD:           simpleInst("MOD");       break;
        case OP_NOT:           simpleInst("NOT");       break;
        case OP_PRINT:         simpleInst("PRINT");     break;
        case OP_PRINTLN:       simpleInst("PRINTLN");   break;
        case OP_PRINTQ:        simpleInst("PRINTQ");    break;
        case OP_JUMP:          jumpInst("JUMP",     1); break;
        case OP_JUMP_OR:       jumpInst("JUMP_OR",  1); break;
        case OP_JUMP_AND:      jumpInst("JUMP_AND", 1); break;
        case OP_JUMP_TRUE:     jumpInst("JUMP_T",   1); break;
        case OP_JUMP_FALSE:    jumpInst("JUMP_F",   1); break;
        case OP_LOOP:          jumpInst("LOOP",    -1); break;
        case OP_CALL:          byteInst("CALL");        break;
        case OP_CALL0:         simpleInst("CALL0");     break;
        case OP_CALL1:         simpleInst("CALL1");     break;
        case OP_CALL2:         simpleInst("CALL2");     break;
        case OP_CALL_HAND:     simpleInst("CALL_HAND"); break;
        case OP_INVOKE:        invokeInst("INVOKE");    break;
        case OP_SUPER_INVOKE:  invokeInst("SUP_INV");   break;
        case OP_CLOSURE:       closureInst("CLOSURE");  break; 
        case OP_CLOSE_UPVALUE: simpleInst("CLOSE_UPV"); break;
        case OP_RETURN:        simpleInst("RET");       break;
        case OP_RETURN_NIL:    simpleInst("RET_NIL");   break;
        case OP_CLASS:         constInst("CLASS");      break;
        case OP_INHERIT:       simpleInst("INHERIT");   break;
        case OP_METHOD:        constInst("METHOD");     break;
        case OP_LIST:          byteInst("LIST");        break;
        case OP_GET_INDEX:     simpleInst("GET_INDEX"); break;
        case OP_SET_INDEX:     simpleInst("SET_INDEX"); break;
        case OP_GET_SLICE:     simpleInst("GET_SLICE"); break;
        case OP_UNPACK:        simpleInst("UNPACK");    break;
        case OP_VCALL:         byteInst("VCALL");       break;
        case OP_VINVOKE:       invokeInst("VINVOKE");   break;
        case OP_VSUPER_INVOKE: invokeInst("VSUP_INV");  break;
        case OP_VLIST:         byteInst("VLIST");       break;
        case OP_GET_ITVAL:     simpleInst("GET_ITVAL"); break;
        case OP_SET_ITVAL:     simpleInst("SET_ITVAL"); break;
        case OP_GET_ITKEY:     simpleInst("GET_ITKEY"); break;
        default:
            printf("Unknown opcode %d", opcd);
            ++offset;
    }
    putstr("\n");
}

#endif