#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "vm.h"

void initChunk(Chunk* chunk) {
    mem_clear(chunk, sizeof(Chunk));
}

void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(LineStart, chunk->lines, chunk->lineCapacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

void freezeChunk(Chunk* chunk) {
    // Freeze all array capacities to current size after filling.
    // No more items should be added afterwards.

    // Only shrink when < 80% full
    if ((chunk->count << 2) + chunk->count < (chunk->capacity << 2)) {
        chunk->code     = RESIZE_ARRAY(uint8_t, chunk->code, chunk->capacity, chunk->count);
        chunk->capacity = chunk->count;
    }

    // Only shrink when < 80% full
    if ((chunk->lineCount << 2) + chunk->lineCount < (chunk->lineCapacity << 2)) {
        chunk->lines        = RESIZE_ARRAY(LineStart, chunk->lines, chunk->lineCapacity, chunk->lineCount);
        chunk->lineCapacity = chunk->lineCount;
    }

    freezeValueArray(&chunk->constants);
}

void appendChunk(Chunk* chunk, int byte, int line) {
    int16_t    oldCapacity;
    LineStart* lineStart;

    if (chunk->capacity < chunk->count + 1) {
        oldCapacity     = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code     = RESIZE_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }
    chunk->code[chunk->count++] = byte;

    // See if we're still on same line
    if (chunk->lineCount > 0 && chunk->lines[chunk->lineCount - 1].line == line)
        return;

    // Append new LineStart
    if (chunk->lineCapacity < chunk->lineCount + 1) {
        oldCapacity         = chunk->lineCapacity;
        chunk->lineCapacity = GROW_CAPACITY(oldCapacity);
        chunk->lines        = RESIZE_ARRAY(LineStart, chunk->lines, oldCapacity, chunk->lineCapacity);
    }

    lineStart = &chunk->lines[chunk->lineCount++];
    lineStart->offset = chunk->count - 1;
    lineStart->line   = line;
}

int addConstant(Chunk* chunk, Value value) {
    int i;
    for (i = 0; i < chunk->constants.count; i++)
        if (valuesEqual(chunk->constants.values[i], value))
            return i;

    pushUnchecked(value);
    appendValueArray(&chunk->constants, value);
    drop();
    return chunk->constants.count - 1;
}

int getLine(Chunk* chunk, int offset) {
    int beg = 0;
    int end = chunk->lineCount - 1;
    int mid;
    LineStart* line;

    for (;;) {
        mid  = (beg + end) >> 1;
        line = &chunk->lines[mid];
        if (offset < line->offset)
            end = mid - 1;
        else if (mid == chunk->lineCount - 1 ||
                 offset < chunk->lines[mid + 1].offset)
            return line->line;
        else
            beg = mid + 1;
    }
    return -1; // not reached
}
