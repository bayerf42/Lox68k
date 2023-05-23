#ifndef clox_chunk_h
#define clox_chunk_h

#include "opcodes.h"
#include "value.h"

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
void writeChunk(Chunk* chunk, int byte, int line);
int  addConstant(Chunk* chunk, Value value);
int  getLine(Chunk* chunk, int offset);

#endif