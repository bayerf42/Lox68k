#include <stdio.h>

#include "disasm.h"
#include "object.h"


static int simpleInst(const char* name, int offset) {
    printf("%s", name);
    return offset + 1;
}

static int byteInst(const char* name, Chunk* chunk, int offset) {
    int arg = chunk->code[offset + 1];
    printf("%-9s %4d", name, arg);
    return offset + 2;
}

static int jumpInst(const char* name, int sign, Chunk* chunk, int offset) {
    int jump = chunk->code[offset + 1] << 8;
    jump |= chunk->code[offset + 2];
    printf("%-9s %4d ; -> %d", name, jump, offset + 3 + ((sign>0) ? jump : -jump));
    return offset + 3;     
}

static int constInst(const char* name, Chunk* chunk, int offset) {
    int constant = chunk->code[offset + 1];
    printf("%-9s %4d ; ", name, constant);
    printValue(chunk->constants.values[constant], true, true);
    return offset + 2;
}

static int invokeInst(const char* name, Chunk* chunk, int offset) {
    int constant = chunk->code[offset + 1];
    int argCount = chunk->code[offset + 2];
    printf("%-9s %4d ; ", name, constant);
    printValue(chunk->constants.values[constant], true, true);
    printf(" (%d args)", argCount);
    return offset + 3;
}

void disassembleChunk(Chunk* chunk, const char* name) {
    int offset;
    printf("== %s ==\n", name);
    for (offset = 0; offset < chunk->count;) {
        offset = disassembleInst(chunk, offset);
        printf("\n");
    }
}

int disassembleInst(Chunk* chunk, int offset) {
    int          instruction, constant;
    ObjFunction* function;
    int          j, upvalue, line;

    printf("%04d ", offset);
    line = getLine(chunk, offset);
    if (offset > 0 && line == getLine(chunk, offset - 1))
        printf("   | ");
    else
        printf("%4d ", line);

    instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:      return constInst("CONST", chunk, offset);
        case OP_NIL:           return simpleInst("NIL", offset);
        case OP_TRUE:          return simpleInst("TRUE", offset);
        case OP_FALSE:         return simpleInst("FALSE", offset);
        case OP_ZERO:          return simpleInst("ZERO", offset);
        case OP_ONE:           return simpleInst("ONE", offset);
        case OP_POP:           return simpleInst("POP", offset);
        case OP_SWAP:          return simpleInst("SWAP", offset);
        case OP_DUP:           return simpleInst("DUP", offset);
        case OP_GET_LOCAL:     return byteInst("GET_LOC", chunk, offset);
        case OP_SET_LOCAL:     return byteInst("SET_LOC", chunk, offset);
        case OP_GET_GLOBAL:    return constInst("GET_GLOB", chunk, offset);
        case OP_DEF_GLOBAL:    return constInst("DEF_GLOB", chunk, offset);
        case OP_SET_GLOBAL:    return constInst("SET_GLOB", chunk, offset);
        case OP_GET_UPVALUE:   return byteInst("GET_UPVAL", chunk, offset);
        case OP_SET_UPVALUE:   return byteInst("SET_UPVAL", chunk, offset);
        case OP_GET_PROPERTY:  return constInst("GET_PROP", chunk, offset);
        case OP_SET_PROPERTY:  return constInst("SET_PROP", chunk, offset);
        case OP_GET_SUPER:     return constInst("GET_SUPER", chunk, offset);
        case OP_EQUAL:         return simpleInst("EQUAL", offset);
        case OP_LESS:          return simpleInst("LESS", offset);
        case OP_ADD:           return simpleInst("ADD", offset);
        case OP_SUB:           return simpleInst("SUB", offset);
        case OP_MUL:           return simpleInst("MUL", offset);
        case OP_DIV:           return simpleInst("DIV", offset);
        case OP_MOD:           return simpleInst("MOD", offset);
        case OP_NOT:           return simpleInst("NOT", offset);
        case OP_NEG:           return simpleInst("NEG", offset);
        case OP_PRINT:         return simpleInst("PRINT", offset);
        case OP_PRINTLN:       return simpleInst("PRINTLN", offset);
        case OP_JUMP:          return jumpInst("JUMP", 1, chunk, offset);
        case OP_JUMP_OR:       return jumpInst("JUMP_OR", 1, chunk, offset);
        case OP_JUMP_AND:      return jumpInst("JUMP_AND", 1, chunk, offset);
        case OP_JUMP_TRUE:     return jumpInst("JUMP_T", 1, chunk, offset);
        case OP_JUMP_FALSE:    return jumpInst("JUMP_F", 1, chunk, offset);
        case OP_LOOP:          return jumpInst("LOOP", -1, chunk, offset);
        case OP_CALL:          return byteInst("CALL", chunk, offset);
        case OP_CALL0:         return simpleInst("CALL0", offset);
        case OP_CALL1:         return simpleInst("CALL1", offset);
        case OP_CALL2:         return simpleInst("CALL2", offset);
        case OP_INVOKE:        return invokeInst("INVOKE", chunk, offset);
        case OP_SUPER_INVOKE:  return invokeInst("SUP_INV", chunk, offset);
        case OP_CLOSURE: {
            offset++;
            constant = chunk->code[offset++];
            printf("%-9s %4d ; ", "CLOSURE", constant);
            printValue(chunk->constants.values[constant], true, true);

            function = AS_FUNCTION(chunk->constants.values[constant]);
            for (j = 0; j < function->upvalueCount; j++) {
                upvalue = chunk->code[offset];
                printf("\n%04d    |   %5s   %4d", offset++,
                       UV_ISLOC(upvalue) ? "LOCAL" : "UPVAL", UV_INDEX(upvalue));
            }

            return offset;
        }
        case OP_CLOSE_UPVALUE: return simpleInst("CLOSE_UPV", offset);
        case OP_RETURN:        return simpleInst("RET", offset);
        case OP_RETURN_NIL:    return simpleInst("RET_NIL", offset);
        case OP_CLASS:         return constInst("CLASS", chunk, offset);
        case OP_INHERIT:       return simpleInst("INHERIT", offset);
        case OP_METHOD:        return constInst("METHOD", chunk, offset);
        case OP_LIST:          return byteInst("LIST", chunk, offset);
        case OP_GET_INDEX:     return simpleInst("GET_INDEX", offset);
        case OP_SET_INDEX:     return simpleInst("SET_INDEX", offset);
        case OP_GET_SLICE:     return simpleInst("GET_SLICE", offset);
        case OP_UNPACK:        return simpleInst("UNPACK", offset);
        case OP_VCALL:         return byteInst("VCALL", chunk, offset);
        case OP_VINVOKE:       return invokeInst("VINVOKE", chunk, offset);
        case OP_VSUPER_INVOKE: return invokeInst("VSUP_INV", chunk, offset);
        case OP_VLIST:         return byteInst("VLIST", chunk, offset);
        case OP_GET_ITVAL:     return simpleInst("GET_ITVAL", offset);
        case OP_SET_ITVAL:     return simpleInst("SET_ITVAL", offset);
        case OP_GET_ITKEY:     return simpleInst("GET_ITKEY", offset);
        default:
            printf("Unknown opcode %d", instruction);
            return offset + 1;
    }
}
