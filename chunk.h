#ifndef clox_chunk_h
#define clox_chunk_h

#include "opcodes.h"
#include "value.h"

typedef uint8_t Upvalue; // lower 7 bits index, highest bit set if local
#define UV_INDEX(u)    ((u)&0x7f)
#define UV_ISLOC(u)    ((u)&0x80)
#define LOCAL_MASK     0x80

#define ARITY_MASK     0x7f
#define REST_PARM_MASK 0x80

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
void freezeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, int byte, int line);
int  addConstant(Chunk* chunk, Value value);
int  getLine(Chunk* chunk, int offset);

#endif