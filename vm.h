#ifndef clox_vm_h
#define clox_vm_h

#ifndef KIT68K
#include <time.h>
#endif

#include "object.h"
#include "table.h"
#include "value.h"

typedef struct {
    uint8_t*    ip;                  // keep first to allow fast addressing without offset  
    ObjClosure* closure;
    Value*      slots;
} CallFrame;

typedef struct {
    Value*      stackTop;            // keep first to allow fast addressing without offset
    Value       stack[STACK_MAX];
    CallFrame   frames[FRAMES_MAX];
    int         frameCount;
    Table       globals;
    Table       strings;
    ObjString*  initString;
    ObjUpvalue* openUpvalues;
    int         lambdaCount;

  volatile bool interrupted;
    bool        hadStackoverflow;
    size_t      bytesAllocated;
    Obj*        objects;
    int         grayCount;
    Obj*        grayStack[GRAY_MAX];

    size_t      totallyAllocated;
#ifdef KIT68K
    uint32_t    stepsExecuted;
#else
    uint64_t    stepsExecuted;
    clock_t     started;
#endif
    int         numGCs;

    bool        debug_print_code;
    bool        debug_trace_execution;
    int16_t     debug_log_gc;
    bool        debug_statistics;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
    INTERPRET_INTERRUPTED,
} InterpretResult;

extern VM vm;

void            initVM(void);
void            freeVM(void);
InterpretResult interpret(const char* source);
void            push(Value value);
void            runtimeError(const char* format, ...);

#define drop()         --vm.stackTop
#define pop()          (*(--vm.stackTop))
#define peek(distance) (vm.stackTop[-1 - (distance)])

#endif
