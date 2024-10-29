#ifndef clox_disasm_h
#define clox_disasm_h

#include "chunk.h"

#ifdef LOX_DBG
void disassembleChunk(Chunk* pChunk, const char* name);
int  disassembleInst( Chunk* pChunk, int pOffset);
#endif

#endif