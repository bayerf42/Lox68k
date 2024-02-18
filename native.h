#ifndef clox_native_h
#define clox_native_h

#include "object.h"

void  defineAllNatives(void);
bool  checkNativeSignature(const Native* native, int argCount, Value* args);
void  handleInterrupts(bool enable);
void  startTicker(void);
char* readLine(void);

extern uint32_t rand32;
extern bool     onKit;

#endif