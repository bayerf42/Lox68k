#ifndef clox_native_h
#define clox_native_h

#include "object.h"

void  defineAllNatives(void);
bool  callNative(const Native* native, int argCount, Value* args);
char* readLine(void);

#endif