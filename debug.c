#include <stdio.h>

#include "debug.h"
#include "object.h"
#include "value.h"


static int simpleInstruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int byteInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t arg = chunk->code[offset + 1];
    printf("%-12s %4d\n", name, arg);
    return offset + 2;
}

static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-12s %4d -> %d\n", name, jump, offset + 3 + ((sign>0) ? jump : -(int)jump));
    return offset + 3;
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-12s %4d '", name, constant);
    printValue(chunk->constants.values[constant], true);
    printf("'\n");
    return offset + 2;
}

static int invokeInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    uint8_t argCount = chunk->code[offset + 2];
    printf("%-12s %4d '", name, constant);
    printValue(chunk->constants.values[constant], true);
    printf("' (%d args)\n", argCount);
    return offset + 3;
}

void disassembleChunk(Chunk* chunk, const char* name) {
    int offset;
    printf("== %s ==\n", name);
    for (offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

int disassembleInstruction(Chunk* chunk, int offset) {
    uint8_t instruction, constant;
    ObjFunction* function;
    int j;
    int isLocal, upIndex;
    int line;

    printf("%04d ", offset);
    line = getLine(chunk, offset);
    if (offset > 0 && line == getLine(chunk, offset - 1))
        printf("   | ");
    else
        printf("%4d ", line);

    instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:      return constantInstruction("CONST", chunk, offset);
        case OP_NIL:           return simpleInstruction("NIL", offset);
        case OP_TRUE:          return simpleInstruction("TRUE", offset);
        case OP_FALSE:         return simpleInstruction("FALSE", offset);
        case OP_POP:           return simpleInstruction("POP", offset);
        case OP_GET_LOCAL:     return byteInstruction("GET_LOC", chunk, offset);
        case OP_SET_LOCAL:     return byteInstruction("SET_LOC", chunk, offset);
        case OP_GET_GLOBAL:    return constantInstruction("GET_GLOB", chunk, offset);
        case OP_DEF_GLOBAL:    return constantInstruction("DEF_GLOB", chunk, offset);
        case OP_SET_GLOBAL:    return constantInstruction("SET_GLOB", chunk, offset);
        case OP_GET_UPVALUE:   return byteInstruction("GET_UPVAL", chunk, offset);
        case OP_SET_UPVALUE:   return byteInstruction("SET_UPVAL", chunk, offset);
        case OP_GET_PROPERTY:  return constantInstruction("GET_PROP", chunk, offset);
        case OP_SET_PROPERTY:  return constantInstruction("SET_PROP", chunk, offset);
        case OP_GET_SUPER:     return constantInstruction("GET_SUPER", chunk, offset);
        case OP_EQUAL:         return simpleInstruction("EQ", offset);
        case OP_GREATER:       return simpleInstruction("GT", offset);
        case OP_LESS:          return simpleInstruction("LT", offset);
        case OP_ADD:           return simpleInstruction("ADD", offset);
        case OP_SUBTRACT:      return simpleInstruction("SUB", offset);
        case OP_MULTIPLY:      return simpleInstruction("MUL", offset);
        case OP_DIVIDE:        return simpleInstruction("DIV", offset);
        case OP_MODULO:        return simpleInstruction("MOD", offset);
        case OP_NOT:           return simpleInstruction("NOT", offset);
        case OP_NEGATE:        return simpleInstruction("NEG", offset);
        case OP_PRINT:         return simpleInstruction("PRINT", offset);
        case OP_PRINTLN:       return simpleInstruction("PRINTLN", offset);
        case OP_JUMP:          return jumpInstruction("JUMP", 1, chunk, offset);
        case OP_JUMP_TRUE:     return jumpInstruction("JUMP_TRUE", 1, chunk, offset);
        case OP_JUMP_FALSE:    return jumpInstruction("JUMP_FALSE", 1, chunk, offset);
        case OP_LOOP:          return jumpInstruction("LOOP", -1, chunk, offset);
        case OP_CALL:          return byteInstruction("CALL", chunk, offset);
        case OP_INVOKE:        return invokeInstruction("INVOKE", chunk, offset);
        case OP_SUPER_INVOKE:  return invokeInstruction("SUP_INVOKE", chunk, offset);
        case OP_CLOSURE: {
            offset++;
            constant = chunk->code[offset++];
            printf("%-12s %4d ", "CLOSURE", constant);
            printValue(chunk->constants.values[constant], true);
            printf("\n");

            function = AS_FUNCTION(chunk->constants.values[constant]);
            for (j = 0; j < function->upvalueCount; j++) {
                isLocal = chunk->code[offset++];
                upIndex = chunk->code[offset++];
                printf("%04d    |                   %s %d\n", offset - 2, isLocal ? "local" : "upvalue", upIndex);
            }

            return offset;
        }
        case OP_CLOSE_UPVALUE: return simpleInstruction("CLOSE_UPVAL", offset);
        case OP_RETURN:        return simpleInstruction("RET", offset);
        case OP_CLASS:         return constantInstruction("CLASS", chunk, offset);
        case OP_INHERIT:       return simpleInstruction("INHERIT", offset);
        case OP_METHOD:        return constantInstruction("METHOD", chunk, offset);
        case OP_LIST:          return byteInstruction("LIST", chunk, offset);
        case OP_GET_INDEX:     return simpleInstruction("GET_INDEX", offset);
        case OP_SET_INDEX:     return simpleInstruction("SET_INDEX", offset);
        case OP_GET_SLICE:     return simpleInstruction("GET_SLICE", offset);
        case OP_SWAP:          return simpleInstruction("SWAP", offset);
        case OP_UNPACK:        return simpleInstruction("UNPACK", offset);
        case OP_VCALL:         return byteInstruction("VCALL", chunk, offset);
        case OP_VINVOKE:       return invokeInstruction("VINVOKE", chunk, offset);
        case OP_VSUPER_INVOKE: return invokeInstruction("VSUP_INVOKE", chunk, offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}
