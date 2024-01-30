#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"
#include "vm.h"

#define MAX_UPVALUES  32
#define MAX_LOCALS    64
#define MAX_BRANCHES 127
#define MAX_LABELS    31
#define MAX_BREAKS    16

ObjFunction* compile(const char* source);
void         markCompilerRoots(void);

#endif