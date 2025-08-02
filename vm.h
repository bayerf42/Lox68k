#ifndef clox_vm_h
#define clox_vm_h

#include "table.h"
#include "value.h"

typedef struct {
    uint8_t*    ip;      // instruction pointer, keep first for fast addressing without offset  
    Value*      fp;      // frame pointer into value stack
    ObjClosure* closure; // active closure 
    Value       handler; // exception handler, if callable; unbind dynamic variable, if dynvar
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
    uint32_t    randomState;         // state of pseudo-random number generator

    bool        handleException;     // internal state
    bool        hadStackoverflow;    // internal state
    size_t      bytesAllocated;      // current heap usage
    Obj*        objects;             // list of all objects used
    int         grayCount;           // 
    Obj*        grayStack[GRAY_MAX]; // objects to be checked for reachability

#ifndef KIT68K
  volatile bool interrupted;         // set from signal handler
#endif

#ifdef LOX_DBG
    bool        log_native_result;   // log result of native call?
    size_t      totallyAllocated;    // accumulates total memory allocated
    int         numGCs;              // accumulates number of garbage collections
    steps_t     stepsExecuted;       // accumulates number of VM instructions executed
    clock_t     started;             // clock at start of evaluation

    bool        debug_print_code;    // print disassembly of code entered
    bool        debug_trace_steps;   // trace every VM step with opcode and stack dump
    bool        debug_trace_calls;   // trace call and return of closures, exceptions, and dynvars
    bool        debug_trace_natives; // trace call of natives 
    int16_t     debug_log_gc;        // trace garbage collection to several degrees of detail 
    bool        debug_statistics;    // print accumulated values above after evaluation
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
