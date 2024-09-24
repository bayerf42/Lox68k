#ifndef clox_native_h
#define clox_native_h

#include "value.h"

typedef bool (*NativeFn)(int argCount, Value* args);

typedef struct {
    const char* name;
    const char* signature;
    NativeFn    function;
} Native;


void  defineAllNatives(void);
bool  callNative(const Native* native, int argCount, Value* args);
char* readLine(void);

#endif