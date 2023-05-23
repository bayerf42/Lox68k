#ifndef clox_disasm_h
#define clox_disasm_h

#include "chunk.h"

void disassembleChunk(Chunk* chunk, const char* name);
int  disassembleInst(Chunk* chunk, int offset);

#endif