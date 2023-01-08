#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEF_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_GET_SUPER,
    OP_EQUAL,
    OP_LESS,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_NOT,
    OP_NEG,
    OP_PRINT,
    OP_PRINTLN,
    OP_JUMP,
    OP_JUMP_TRUE,
    OP_JUMP_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_INVOKE,
    OP_SUPER_INVOKE,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_RETURN,
    OP_CLASS,
    OP_INHERIT,
    OP_METHOD,
    OP_LIST,
    OP_GET_INDEX,
    OP_SET_INDEX,
    OP_GET_SLICE,
    OP_SWAP,
    OP_UNPACK,
    OP_VCALL,
    OP_VINVOKE,
    OP_VSUPER_INVOKE,
    OP_VLIST,
    OP_GET_ITVAL,
    OP_SET_ITVAL,
    OP_GET_ITKEY,
} OpCode;


typedef struct {
    int16_t offset;
    int16_t line;
} LineStart;


typedef struct {
    int16_t    count;
    int16_t    capacity;
    uint8_t*   code;
    int16_t    lineCount;
    int16_t    lineCapacity;
    LineStart* lines;
    ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int  addConstant(Chunk* chunk, Value value);
int  getLine(Chunk* chunk, int offset);

#endif