#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"
#include "vm.h"

#define MAX_UPVALUES  32
#define MAX_LOCALS    64
#define MAX_BRANCHES 128

ObjFunction* compile(const char* source);
void         markCompilerRoots(void);

#endif