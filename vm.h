#ifndef clox_vm_h
#define clox_vm_h

#include "table.h"
#include "value.h"

typedef struct {
    uint8_t*    ip;      // instruction pointer, keep first to allow fast addressing without offset  
    Value*      fp;      // frame pointer into value stack
    ObjClosure* closure; // active closure 
    Value       handler; // exception handler, if callable; dynamic variable, if string
} CallFrame;

typedef struct {
    Value*      sp;      // keep first to allow fast addressing without offset
    Value       stack[STACK_MAX];
    CallFrame   frames[FRAMES_MAX];
    int         frameCount;
    Table       globals;
    Table       strings;
    ObjString*  initString;
    ObjUpvalue* openUpvalues;
    int         lambdaCount;
    uint32_t    randomState;

    bool        handleException;
    bool        hadStackoverflow;
    size_t      bytesAllocated;
    Obj*        objects;
    int         grayCount;
    Obj*        grayStack[GRAY_MAX];

#ifndef KIT68K
  volatile bool interrupted;
#endif

#ifdef LOX_DBG
    bool        log_native_result;
    size_t      totallyAllocated;
    int         numGCs;
    steps_t     stepsExecuted;
    clock_t     started;

    bool        debug_print_code;
    bool        debug_trace_steps;
    bool        debug_trace_calls;
    bool        debug_trace_natives;
    int16_t     debug_log_gc;
    bool        debug_statistics;
#endif
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
void            userError(Value exception);

#define drop()               --vm.sp
#define pop()                (*(--vm.sp))
#define peek(distance)       (vm.sp[-1 - (distance)])
#define pushUnchecked(value) *vm.sp++ = (value)

#endif
