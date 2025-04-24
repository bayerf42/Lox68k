#ifndef clox_disasm_h
#define clox_disasm_h

#ifdef LOX_DBG
#include "chunk.h"

void disassembleChunk(Chunk* pChunk, const char* name);
int  disassembleInst( Chunk* pChunk, int pOffset);

#endif
#endif