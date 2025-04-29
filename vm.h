#ifndef clox_vm_h
#define clox_vm_h

#include "table.h"
#include "value.h"

typedef struct {
    uint8_t*    ip;      // instruction pointer, keep first for fast addressing without offset  
    Value*      fp;      // frame pointer into value stack
    ObjClosure* closure; // active closure 
    Value       handler; // exception handler, if callable; dynamic variable, if string
} CallFrame;

typedef struct {
    Value*      sp;                  // stack pointer, keep first for fast addressing without offset
    Value       stack[STACK_MAX];    // value stack
    CallFrame   frames[FRAMES_MAX];  // call stack
    int         frameCount;          // call depth
    Table       globals;             // table of global variables
    Table       strings;             // weak set of all string used
    ObjString*  initString;          // ref to 'init' string
    ObjUpvalue* openUpvalues;        // list of open upvalues
    int         lambdaCount;         // 
    uint32_t    randomState;         // state of pseudo-random number generator

    bool        handleException;     // internal state
    bool        hadStackoverflow;    // internal state
    size_t      bytesAllocated;      // to trigger GC
    Obj*        objects;             // list of all objects used
    int         grayCount;           // for GC 
    Obj*        grayStack[GRAY_MAX]; // for GC

#ifndef KIT68K
  volatile bool interrupted;         // set from signal handler
#endif

#ifdef LOX_DBG
    bool        log_native_result;   // logging status
    size_t      totallyAllocated;    // execution statistics...
    int         numGCs;
    steps_t     stepsExecuted;
    clock_t     started;

    bool        debug_print_code;    // debugging flags...
    bool        debug_trace_steps;
    bool        debug_trace_calls;
    bool        debug_trace_natives;
    int16_t     debug_log_gc;
    bool        debug_statistics;
#endif
} VM;

typedef enum {
    EVAL_OK,
    EVAL_COMPILE_ERROR,
    EVAL_RUNTIME_ERROR,
    EVAL_INTERRUPTED,
} EvalResult;

extern VM vm;

void       initVM(void);
void       freeVM(void);
EvalResult interpret(const char* source);
void       push(Value value);
void       runtimeError(const char* format, ...);
void       userError(Value exception);

#define drop()               --vm.sp
#define pop()                (*(--vm.sp))
#define peek(distance)       (vm.sp[-1 - (distance)])
#define pushUnchecked(value) *vm.sp++ = (value)

#endif
