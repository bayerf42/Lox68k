#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "compiler.h"
#include "disasm.h"
#include "memory.h"
#include "native.h"
#include "vm.h"

VM vm;

static void resetStack(void) {
    vm.sp           = vm.stack;
    vm.frameCount   = 0;
    vm.openUpvalues = NULL;
}

static void closeUpvalues(Value* last);

#ifdef LOX_DBG

static void indentCallTrace(void) {
    int depth = vm.frameCount;
    while (depth--)
        putstr("  ");
}

static void printArgList(int argCount) {
    const char* separator = "";
    Value*      args      = vm.sp - argCount; 
    while (argCount--) {
        putstr(separator);
        separator = ", ";
        printValue(*args++, PRTF_MACHINE | PRTF_EXPAND);
    } 
}

static void printStack(void) {
    Value*  slot;
    for (slot = vm.stack; slot < vm.sp; slot++) {
        printValue(*slot, PRTF_MACHINE | PRTF_COMPACT);
        putstr(" | ");
    }
    putstr("\n");
}

#endif

static void printBacktrace(void) {
    int          i;
    size_t       instruction;
    CallFrame*   frame;
    ObjFunction* function;

    for (i = vm.frameCount - 1; i >= 0; i--) {
        frame       = &vm.frames[i];
        function    = frame->closure->function;
        instruction = frame->ip - function->chunk.code - 1;
        printf("[line %d] in %s\n", getLine(&function->chunk, instruction), functionName(function));
    }
    resetStack();
}

void runtimeError(const char* format, ...) {
    va_list    args;
    int        i;
    CallFrame* frame;

    va_start(args, format);
    vsprintf(big_buffer, format, args);
    va_end(args);

#ifdef LOX_DBG
    if (vm.log_native_result) {
        printf("/!\\ \"%s\"\n", big_buffer);
        vm.log_native_result = false;
    }

    if (vm.debug_trace_calls) {
        indentCallTrace();
        printf("<== \"%s\"\n", big_buffer);
    }
#endif

    // search for handler in frames
    for (i = vm.frameCount - 1; i >= 0; i--) {
        frame = &vm.frames[i];
        if (!IS_NIL(frame->handler)) {
            if (IS_STRING(frame->handler)) {
                popGlobal(frame->handler);
                continue;
            }  
            closeUpvalues(frame->fp);
            vm.sp         = frame->fp;
            vm.frameCount = i;
            pushUnchecked(frame->handler);
            pushUnchecked(OBJ_VAL(makeString0(big_buffer)));
            vm.handleException = true; 
            return;
        }
    }
    putstr("Runtime error: ");
    putstr(big_buffer);
    putstr("\n");
    printBacktrace();
}

void userError(Value exception) {
    int        i;
    CallFrame* frame;

#ifdef LOX_DBG
    if (vm.log_native_result) {
        putstr("/!\\ ");
        printValue(exception, PRTF_MACHINE | PRTF_EXPAND);
        putstr("\n");
        vm.log_native_result = false;
    }

    if (vm.debug_trace_calls) {
        indentCallTrace();
        putstr("<== ");
        printValue(exception, PRTF_MACHINE | PRTF_EXPAND);
        putstr("\n");
    }
#endif

    // search for handler in frames
    for (i = vm.frameCount - 1; i >= 0; i--) {
        frame = &vm.frames[i];
        if (!IS_NIL(frame->handler)) {
            if (IS_STRING(frame->handler)) {
                popGlobal(frame->handler);
                continue;
            }  
            closeUpvalues(frame->fp);
            vm.sp         = frame->fp;
            vm.frameCount = i;
            pushUnchecked(frame->handler);
            pushUnchecked(exception);
            vm.handleException = true; 
            return;
        }
    }
    putstr("Runtime error: ");
    printValue(exception, PRTF_HUMAN | PRTF_EXPAND);
    putstr("\n");
    printBacktrace();
}

void initVM(void) {
    resetStack();
    vm.objects        = NULL;
    vm.bytesAllocated = 0;
    vm.grayCount      = 0;
    vm.lambdaCount    = 0;
    vm.randomState    = 47110815;

    initTable(&vm.globals);
    initTable(&vm.strings);

    vm.initString = NULL;
    vm.initString = makeString0("init");

    defineAllNatives();
}

#ifndef KIT68K
void freeVM(void) {
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    vm.initString = NULL;
    freeObjects();
}
#endif

#ifndef KIT68K
// Optimized ASM code for 68K, see kit_util.asm
void push(Value value) {
    if (vm.sp >= (vm.stack + (STACK_MAX-1))) {
        vm.hadStackoverflow = true;
        return;
    }
    *vm.sp++ = value;
}
#endif

#define dropNpush(n,value) { \
    vm.sp -= (n) - 1;        \
    vm.sp[-1] = (value);     \
}

#define CHECK_ARITH_ERROR(op)                   \
if (errno != 0) {                               \
    runtimeError("'%s' arithmetic error.", op); \
    goto handleError;                           \
}

#define CHECK_LOX_STACK_OVERFLOW()              \
if (vm.frameCount == FRAMES_MAX) {              \
    runtimeError("Lox call stack overflow.");   \
    return false;                               \
}


static bool callClosure(ObjClosure* closure, int argCount) {
    CallFrame*   frame;
    ObjFunction* function = closure->function;
    ObjList*     args;
    int          itemCount;
    int          arity;

    CHECK_LOX_STACK_OVERFLOW()

#ifdef LOX_DBG
    if (vm.debug_trace_calls) {
        indentCallTrace();
        printf("--> %s (", functionName(function));
        printArgList(argCount);
        putstr(")\n");
    }
#endif

    arity = function->arity & ARITY_MASK;
    if (function->arity & REST_PARM_MASK) {
        if (argCount < arity - 1) {
            runtimeError("'%s' expected %s%d arguments but got %d.",
                         functionName(function), "at least ", arity - 1, argCount);
            return false;
        }
        itemCount = argCount - arity + 1;
        args = makeList(itemCount, vm.sp - itemCount, itemCount, 1);
        dropNpush(itemCount, OBJ_VAL(args));
    }
    else if (argCount != arity) {
        runtimeError("'%s' expected %s%d arguments but got %d.",
                     functionName(function), "", arity, argCount);
        return false;
    }

    frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->handler = NIL_VAL;
    frame->ip      = function->chunk.code;
    frame->fp      = vm.sp - arity - 1;
    return true;
}

static bool callWithHandler(void) {
    CallFrame*   frame;
    ObjClosure*  closure  = AS_CLOSURE(peek(1));
    ObjFunction* function = closure->function;

    CHECK_LOX_STACK_OVERFLOW()

    if (!isCallable(peek(0))) {
        runtimeError("Handler must be callable.");
        return false;
    }

#ifdef LOX_DBG
    if (vm.debug_trace_calls) {
        indentCallTrace();
        printf("==> %s () handler ", functionName(function));
        printValue(peek(0), PRTF_MACHINE | PRTF_EXPAND);
        putstr("\n");
    }
#endif

    frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->handler = pop();
    frame->ip      = function->chunk.code;
    frame->fp      = vm.sp - 1;
    return true;
}

static bool callBinding(Value varName) {
    CallFrame*   frame;
    ObjClosure*  closure  = AS_CLOSURE(peek(0));
    ObjFunction* function = closure->function;

    CHECK_LOX_STACK_OVERFLOW()

#ifdef LOX_DBG
    if (vm.debug_trace_calls) {
        indentCallTrace();
        printf("~~> %s () %s = ", functionName(function), AS_CSTRING(varName));
        printValue(peek(1), PRTF_MACHINE | PRTF_EXPAND);
        putstr("\n");
    }
#endif

    pushGlobal(varName, peek(1));
    dropNpush(2, OBJ_VAL(closure));

    frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->handler = varName;
    frame->ip      = function->chunk.code;
    frame->fp      = vm.sp - 1;
    return true;
}

static bool callValue(Value callee, int argCount) {
    const Native* native;
    ObjClass*     klass;
    ObjBound*     bound;
    Value         initializer = NIL_VAL;

    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_BOUND:
                bound = AS_BOUND(callee);
                vm.sp[-argCount - 1] = bound->receiver;
                return callClosure(bound->method, argCount);

            case OBJ_CLASS:
                klass = AS_CLASS(callee);
                vm.sp[-argCount - 1] = OBJ_VAL(makeInstance(klass));
                if (tableGet(&klass->methods, OBJ_VAL(vm.initString), &initializer))
                    return callClosure(AS_CLOSURE(initializer), argCount);
                else if (argCount != 0) {
                    runtimeError("'%s' expected %s%d arguments but got %d.",
                                 AS_CSTRING(klass->name), "", 0, argCount);
                    return false;
                }
                return true;

            case OBJ_CLOSURE:
                return callClosure(AS_CLOSURE(callee), argCount);

            case OBJ_NATIVE:
                native = AS_NATIVE(callee);

#ifdef LOX_DBG
                if (vm.debug_trace_natives) {
                    if (vm.debug_trace_calls)
                        indentCallTrace();
                    printf("--- %s (", native->name);
                    printArgList(argCount);
                    putstr(") -> ");
                    vm.log_native_result = true;
                }
#endif

                if (!callNative(native, argCount, vm.sp - argCount)) // don't update vm.sp yet!
                    return false;
                vm.sp -= argCount;

#ifdef LOX_DBG
                if (vm.log_native_result) {
                    printValue(vm.sp[-1], PRTF_MACHINE | PRTF_EXPAND);
                    putstr("\n");
                    vm.log_native_result = false;
                } 
#endif
                return true;
        }
    }
    runtimeError("Can't %s type %s.", "call", valueType(callee));
    return false;
}

static bool invokeFromClass(ObjClass* klass, ObjString* name, int argCount) {
    Value method = NIL_VAL;
    if (!tableGet(&klass->methods, OBJ_VAL(name), &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }
    return callClosure(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjString* name, int argCount) {
    Value        receiver = peek(argCount);
    ObjInstance* instance;
    Value        value = NIL_VAL;

    if (!IS_INSTANCE(receiver)) {
        runtimeError("Only instances have %s.", "methods");
        return false;
    }
    instance = AS_INSTANCE(receiver);

    if (tableGet(&instance->fields, OBJ_VAL(name), &value)) {
        vm.sp[-argCount - 1] = value;
        return callValue(value, argCount);
    }
    return invokeFromClass(instance->klass, name, argCount);
}

static bool bindMethod(ObjClass* klass, ObjString* name) {
    Value     method = NIL_VAL;
    ObjBound* bound;
    if (!tableGet(&klass->methods, OBJ_VAL(name), &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }
    bound = makeBound(peek(0), AS_CLOSURE(method));
    dropNpush(1, OBJ_VAL(bound));
    return true;
}

static ObjUpvalue* captureUpvalue(Value* local) {
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue     = vm.openUpvalues;
    ObjUpvalue* createdUpvalue;

    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue     = upvalue->nextUpvalue;
    }

    if (upvalue != NULL && upvalue->location == local)
        return upvalue;

    createdUpvalue = makeUpvalue(local);
    createdUpvalue->nextUpvalue = upvalue;

    if (prevUpvalue == NULL)
        vm.openUpvalues = createdUpvalue;
    else
        prevUpvalue->nextUpvalue = createdUpvalue;

    return createdUpvalue;
}

static void closeUpvalues(Value* last) {
    ObjUpvalue* upvalue;
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
        upvalue           = vm.openUpvalues;
        upvalue->closed   = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues   = upvalue->nextUpvalue;
    }
}

static void defineMethod(ObjString* name) {
    Value       method = peek(0);
    ObjClass*   klass  = AS_CLASS(peek(1));
    ObjClosure* clos   = AS_CLOSURE(method);

    clos->function->klass = klass;
    tableSet(&klass->methods, OBJ_VAL(name), method);
    drop();
}

#define READ_BYTE()   (*frame->ip++)
#define READ_USHORT() (frame->ip += 2, (frame->ip[-2] << 8) | frame->ip[-1])
#define CURR_INSTR()  (frame->ip[-1])

static InterpretResult run(void) {
    int   index, begin, end, i;
    Value constant;

    // The IDE68K ancient C compiler generates wrong code for local vars in case branches.
    // Thus, we declare all needed variables at function start..
    Int          aInt, bInt;
    Real         aReal, bReal;
    Value        aVal=NIL_VAL, bVal, cVal, resVal;
    ObjString    *aStr, *bStr, *resStr;
    ObjList      *aLst, *bLst, *resLst;
    ObjIterator  *aIt;
    ObjClass     *superclass, *subclass;
    ObjInstance  *instance;
    int          slotNr;
    ObjFunction  *function;
    ObjClosure   *closure;
    int          argCount, itemCount;
    int          offset;
    int          upvalue;
    CallFrame    *frame;
    Value        *consts;

    vm.hadStackoverflow  = false;
    vm.handleException   = false;

#ifdef LOX_DBG
    vm.log_native_result = false;
    vm.stepsExecuted     = 0;
    STATIC_BREAKPOINT();
#endif

updateFrame:
    // Last op changed call frame, update cached values
    frame  = &vm.frames[vm.frameCount - 1];
    consts = frame->closure->function->chunk.constants.values;

nextInst:
    // Last op possibly caused stack overflow, check here
    if (vm.hadStackoverflow) {
        runtimeError("Lox value stack overflow.");
        goto handleError;
    }

nextInstNoSO:
    // Last op guaranteed no stack overflow, omit check
    if (INTERRUPTED()) {
        (void)READ_BYTE();
        putstr("Interrupted.\n");
        printBacktrace();
        return INTERPRET_INTERRUPTED;
    }

#ifdef LOX_DBG
    if (vm.debug_trace_steps) {
        printStack();
        disassembleInst(&frame->closure->function->chunk,
                        (int)(frame->ip - frame->closure->function->chunk.code));
    }
    ++vm.stepsExecuted;
#endif

    switch (READ_BYTE()) {
        case OP_CONSTANT:
            index = READ_BYTE();
            constant = consts[index];
            push(constant);
            goto nextInst;

        case OP_NIL:   push(NIL_VAL);    goto nextInst;
        case OP_TRUE:  push(TRUE_VAL);   goto nextInst;
        case OP_FALSE: push(FALSE_VAL);  goto nextInst;
        case OP_ZERO:  push(INT_VAL(0)); goto nextInst;
        case OP_ONE:   push(INT_VAL(1)); goto nextInst;
        case OP_POP:   drop();           goto nextInstNoSO;
        case OP_DUP:   push(peek(0));    goto nextInst;

        case OP_SWAP:
            aVal    = peek(0);
            peek(0) = peek(1);
            peek(1) = aVal;
            goto nextInstNoSO;
  
        case OP_GET_LOCAL:
            slotNr = READ_BYTE();
            push(frame->fp[slotNr]);
            goto nextInst;

        case OP_SET_LOCAL:
            slotNr = READ_BYTE();
            frame->fp[slotNr] = peek(0);
            goto nextInstNoSO;

        case OP_GET_GLOBAL:
            index    = READ_BYTE();
            constant = consts[index];
            if (!tableGet(&vm.globals, constant, &aVal)) {
                runtimeError("Undefined variable '%s'.", AS_CSTRING(constant));
                goto handleError;
            }
            if (IS_DYNVAR(aVal)) 
                aVal = AS_DYNVAR(aVal)->current;
            push(aVal);
            goto nextInst;

        case OP_DEF_GLOBAL:
            index    = READ_BYTE();
            constant = consts[index];
            tableSet(&vm.globals, constant, peek(0)); // not pushGlobal to avoid chains when redefining
            drop();
            goto nextInstNoSO;

        case OP_SET_GLOBAL:
            index    = READ_BYTE();
            constant = consts[index];
            if (!setGlobal(constant, peek(0))) {
                runtimeError("Undefined variable '%s'.", AS_CSTRING(constant));
                goto handleError;
            }
            goto nextInstNoSO;

        case OP_GET_UPVALUE:
            slotNr = READ_BYTE();
            push(*frame->closure->upvalues[slotNr]->location);
            goto nextInst;

        case OP_SET_UPVALUE:
            slotNr = READ_BYTE();
            *frame->closure->upvalues[slotNr]->location = peek(0);
            goto nextInstNoSO;

        case OP_GET_PROPERTY:
            if (!IS_INSTANCE(peek(0))) {
                runtimeError("Only instances have %s.", "properties");
                goto handleError;
            }
            instance = AS_INSTANCE(peek(0));
            index    = READ_BYTE();
            constant = consts[index];
            if (tableGet(&instance->fields, constant, &aVal)) {
                dropNpush(1, aVal);
                goto nextInstNoSO;
            }
            aStr = AS_STRING(constant);
            if (!bindMethod(instance->klass, aStr))
                goto handleError;
            goto nextInstNoSO;

        case OP_SET_PROPERTY:
            if (!IS_INSTANCE(peek(1))) {
                runtimeError("Only instances have %s.", "properties");
                goto handleError;
            }
            instance = AS_INSTANCE(peek(1));
            index    = READ_BYTE();
            constant = consts[index];
            tableSet(&instance->fields, constant, peek(0));
            aVal = pop();
            dropNpush(1, aVal);
            goto nextInstNoSO;

        case OP_GET_SUPER:
            index      = READ_BYTE();
            constant   = consts[index];
            aStr       = AS_STRING(constant);
            superclass = AS_CLASS(pop());
            if (!bindMethod(superclass, aStr))
                goto handleError;
            goto nextInstNoSO;

        case OP_EQUAL:
            bVal = pop(); 
            dropNpush(1, BOOL_VAL(valuesEqual(peek(0), bVal)));
            goto nextInstNoSO;

        case OP_LESS:
            if (IS_INT(peek(0))) {
                if (IS_INT(peek(1))) {
                    bVal = pop(); 
                    dropNpush(1, BOOL_VAL(peek(0) < bVal)); // relying on Value tagging for int
                    goto nextInstNoSO;
                } else if (IS_REAL(peek(1))) {
                    aReal = AS_REAL(peek(1));
                    bReal = intToReal(AS_INT(peek(0)));
                    goto lessReals;
                } else goto typeErrorLess;
            } else if (IS_REAL(peek(0))) {
                bReal = AS_REAL(peek(0));
                if (IS_INT(peek(1)))
                    aReal = intToReal(AS_INT(peek(1)));
                else if (IS_REAL(peek(1)))
                    aReal = AS_REAL(peek(1));
                else goto typeErrorLess;
            lessReals: 
                dropNpush(2, BOOL_VAL(less(aReal,bReal)));
            } else if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                bStr = AS_STRING(peek(0));
                aStr = AS_STRING(peek(1));
                dropNpush(2, BOOL_VAL(strcmp(aStr->chars, bStr->chars) < 0));
            } else {
            typeErrorLess:
                runtimeError("Can't %s types %s and %s.", "order",
                             valueType(peek(1)), valueType(peek(0)));
                goto handleError;
            }
            goto nextInstNoSO;

        case OP_ADD:
            if (IS_INT(peek(0))) {
                if (IS_INT(peek(1))) {
                    bVal = pop(); 
                    dropNpush(1, peek(0) + bVal - 1); // relying on Value tagging for int
                    goto nextInstNoSO;
                } else if (IS_REAL(peek(1))) {
                    aReal = AS_REAL(peek(1));
                    bReal = intToReal(AS_INT(peek(0)));
                    goto addReals;
                } else goto typeErrorAdd;
            } else if (IS_REAL(peek(0))) {
                bReal = AS_REAL(peek(0));
                if (IS_INT(peek(1)))
                    aReal = intToReal(AS_INT(peek(1)));
                else if (IS_REAL(peek(1)))
                    aReal = AS_REAL(peek(1));
                else goto typeErrorAdd;
            addReals: 
                errno = 0;
                dropNpush(2, makeReal(add(aReal,bReal)));
                CHECK_ARITH_ERROR("+")
            } else if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                bStr = AS_STRING(peek(0));
                aStr = AS_STRING(peek(1));
                resStr = concatStrings(aStr, bStr);
                if (!resStr) {
                    runtimeError("'%s' stringbuffer overflow.", "+");
                    goto handleError;
                }
                dropNpush(2, OBJ_VAL(resStr));
            } else if (IS_LIST(peek(0)) && IS_LIST(peek(1))) {
                bLst = AS_LIST(peek(0));
                aLst = AS_LIST(peek(1));
                resLst = concatLists(aLst, bLst);
                dropNpush(2, OBJ_VAL(resLst));
            } else {
            typeErrorAdd:
                runtimeError("Can't %s types %s and %s.", "add",
                             valueType(peek(1)), valueType(peek(0)));
                goto handleError;
            }
            goto nextInstNoSO;

        case OP_SUB:
            if (IS_INT(peek(0))) {
                if (IS_INT(peek(1))) {
                    bVal = pop();
                    dropNpush(1, peek(0) - bVal + 1); // relying on Value tagging for int
                    goto nextInstNoSO;
                } else if (IS_REAL(peek(1))) {
                    aReal = AS_REAL(peek(1));
                    bReal = intToReal(AS_INT(peek(0)));
                } else goto typeErrorSub;
            } else if (IS_REAL(peek(0))) {
                bReal = AS_REAL(peek(0));
                if (IS_INT(peek(1)))
                    aReal = intToReal(AS_INT(peek(1)));
                else if (IS_REAL(peek(1)))
                    aReal = AS_REAL(peek(1));
                else goto typeErrorSub;
            } else {
            typeErrorSub:
                runtimeError("Can't %s types %s and %s.", "subtract",
                             valueType(peek(1)), valueType(peek(0)));
                goto handleError;
            }
            errno = 0;
            dropNpush(2, makeReal(sub(aReal,bReal)));
            CHECK_ARITH_ERROR("-")
            goto nextInstNoSO;

        case OP_MUL:
            if (IS_INT(peek(0))) {
                if (IS_INT(peek(1))) {
                    bInt = AS_INT(pop());
                    dropNpush(1, INT_VAL(AS_INT(peek(0)) * bInt));
                    goto nextInstNoSO;
                } else if (IS_REAL(peek(1))) {
                    aReal = AS_REAL(peek(1));
                    bReal = intToReal(AS_INT(peek(0)));
                } else goto typeErrorMul;
            } else if (IS_REAL(peek(0))) {
                bReal = AS_REAL(peek(0));
                if (IS_INT(peek(1)))
                    aReal = intToReal(AS_INT(peek(1)));
                else if (IS_REAL(peek(1)))
                    aReal = AS_REAL(peek(1));
                else goto typeErrorMul;
            } else {
            typeErrorMul:
                runtimeError("Can't %s types %s and %s.", "multiply",
                             valueType(peek(1)), valueType(peek(0)));
                goto handleError;
            }
            errno = 0;
            dropNpush(2, makeReal(mul(aReal,bReal)));
            CHECK_ARITH_ERROR("*")
            goto nextInstNoSO;

        case OP_DIV:
        case OP_MOD:
            if (IS_INT(peek(0))) {
                if (IS_INT(peek(1))) {
                    bInt = AS_INT(pop());
                    aInt = AS_INT(pop());
                    if (bInt == 0) {
                        runtimeError("div by zero.");
                        goto handleError;
                    }
                    if (CURR_INSTR()==OP_DIV) 
                        aInt = aInt / bInt;
                    else
                        aInt = aInt % bInt;
                    pushUnchecked(INT_VAL(aInt));
                    goto nextInstNoSO;
                } else if (IS_REAL(peek(1))) {
                    aReal = AS_REAL(peek(1));
                    bReal = intToReal(AS_INT(peek(0)));
                } else goto typeErrorDiv;
            } else if (IS_REAL(peek(0))) {
                bReal = AS_REAL(peek(0));
                if (IS_INT(peek(1)))
                    aReal = intToReal(AS_INT(peek(1)));
                else if (IS_REAL(peek(1)))
                    aReal = AS_REAL(peek(1));
                else goto typeErrorDiv;
            } else {
            typeErrorDiv:
                runtimeError("Can't %s types %s and %s.", "divide",
                             valueType(peek(1)), valueType(peek(0)));
                goto handleError;
            }
            if (bReal == 0) {
                runtimeError("div by zero.");
                goto handleError;
            }
            errno = 0;
            if (CURR_INSTR()==OP_DIV)
                aReal = div(aReal,bReal);
            else
                aReal = mod(aReal,bReal);
            dropNpush(2, makeReal(aReal));
            CHECK_ARITH_ERROR("div")
            goto nextInstNoSO;

        case OP_NOT:
            peek(0) = BOOL_VAL(IS_FALSEY(peek(0)));
            goto nextInstNoSO;

        case OP_PRINT:
            printValue(pop(), PRTF_HUMAN | PRTF_EXPAND);
            goto nextInstNoSO;

        case OP_PRINTLN:
            printValue(pop(), PRTF_HUMAN | PRTF_EXPAND);
            putstr("\n");
            goto nextInstNoSO;

        case OP_PRINTQ:
            printValue(pop(), PRTF_MACHINE | PRTF_EXPAND);
            putstr("\n");
            goto nextInstNoSO;

        case OP_JUMP:
            offset = READ_USHORT();
            frame->ip += offset;
            goto nextInstNoSO;

        case OP_JUMP_OR:
            offset = READ_USHORT();
            if (IS_FALSEY(peek(0)))
                drop();
            else
                frame->ip += offset;
            goto nextInstNoSO;

        case OP_JUMP_AND:
            offset = READ_USHORT();
            if (IS_FALSEY(peek(0)))
                frame->ip += offset;
            else
                drop();
            goto nextInstNoSO;

        case OP_JUMP_TRUE:
            offset = READ_USHORT();
            if (!IS_FALSEY(pop()))
                frame->ip += offset;
            goto nextInstNoSO;

        case OP_JUMP_FALSE:
            offset = READ_USHORT();
            if (IS_FALSEY(pop()))
                frame->ip += offset;
            goto nextInstNoSO;

        case OP_LOOP:
            offset = READ_USHORT();
            frame->ip -= offset;
            goto nextInstNoSO;

        case OP_CALL0:
            argCount = 0;
            goto cont_call;

        case OP_CALL1:
            argCount = 1;
            goto cont_call;

        case OP_CALL2:
            argCount = 2;
            goto cont_call;

        case OP_CALL:
            argCount = READ_BYTE();
        cont_call:
            if (!callValue(peek(argCount), argCount))
                goto handleError;
            goto updateFrame;

        case OP_VCALL:
            argCount = READ_BYTE() + AS_INT(pop());
            goto cont_call;

        case OP_CALL_HAND:
            if (!callWithHandler())
                goto handleError;
            goto updateFrame;

        case OP_CALL_BIND:
            index    = READ_BYTE();
            constant = consts[index];
            if (!callBinding(constant))
                goto handleError;
            goto updateFrame;

        case OP_INVOKE:
            index    = READ_BYTE();
            argCount = READ_BYTE();
        cont_invoke:
            constant = consts[index];
            aStr = AS_STRING(constant);
            if (!invoke(aStr, argCount))
                goto handleError;
            goto updateFrame;

        case OP_VINVOKE:
            index    = READ_BYTE();
            argCount = READ_BYTE() + AS_INT(pop());
            goto cont_invoke;

        case OP_SUPER_INVOKE:
            index      = READ_BYTE();
            superclass = AS_CLASS(pop());
            argCount   = READ_BYTE();
        cont_super_invoke:
            constant = consts[index];
            aStr     = AS_STRING(constant);
            if (!invokeFromClass(superclass, aStr, argCount))
                goto handleError;
            goto updateFrame;

        case OP_VSUPER_INVOKE:
            index      = READ_BYTE();
            superclass = AS_CLASS(pop());
            argCount   = READ_BYTE() + AS_INT(pop());
            goto cont_super_invoke;

        case OP_CLOSURE:
            index    = READ_BYTE();
            constant = consts[index];
            function = AS_FUNCTION(constant);
            closure  = makeClosure(function);
            push(OBJ_VAL(closure));
            for (i = 0; i < closure->upvalueCount; i++) {
                upvalue = READ_BYTE();
                if (UV_ISLOC(upvalue))
                    closure->upvalues[i] = captureUpvalue(frame->fp + UV_INDEX(upvalue));
                else
                    closure->upvalues[i] = frame->closure->upvalues[UV_INDEX(upvalue)];
            }
            goto nextInst;

        case OP_CLOSE_UPVALUE:
            closeUpvalues(vm.sp - 1);
            drop();
            goto nextInstNoSO;

        case OP_RETURN_NIL:
            resVal = NIL_VAL;
            goto cont_ret;

        case OP_RETURN:
            resVal = pop();
        cont_ret:
            closeUpvalues(frame->fp);

            if (IS_STRING(frame->handler))
                popGlobal(frame->handler);

            vm.frameCount--;

#ifdef LOX_DBG
            if (vm.debug_trace_calls) {
                indentCallTrace();
                printf("<-- %s ", functionName(frame->closure->function));
                printValue(resVal, PRTF_MACHINE | PRTF_EXPAND);
                putstr("\n");
            }
#endif

            if (vm.frameCount == 0) {
                drop();
                return INTERPRET_OK;
            }
            vm.sp = frame->fp;
            pushUnchecked(resVal);
            goto updateFrame;

        case OP_CLASS:
            index    = READ_BYTE();
            constant = consts[index];
            aStr     = AS_STRING(constant);
            push(OBJ_VAL(makeClass(aStr)));
            goto nextInst;

        case OP_INHERIT:
            aVal = peek(1);
            if (!IS_CLASS(aVal)) {
                runtimeError("Can't %s type %s.", "inherit from", valueType(aVal));
                goto handleError;
            }
            superclass = AS_CLASS(aVal);
            subclass   = AS_CLASS(peek(0));
            if (superclass == subclass) {
                runtimeError("Can't %s itself.", "inherit from");
                goto handleError;
            }
            subclass->superClass = superclass;
            tableAddAll(&superclass->methods, &subclass->methods);
            drop();
            goto nextInstNoSO;

        case OP_METHOD:
            index    = READ_BYTE();
            constant = consts[index];
            aStr     = AS_STRING(constant);
            defineMethod(aStr);
            goto nextInstNoSO;

        case OP_LIST:
            argCount = READ_BYTE();
        cont_list:
            aLst     = makeList(argCount, vm.sp - argCount, argCount, 1);
            dropNpush(argCount, OBJ_VAL(aLst));
            goto nextInst;

        case OP_VLIST:
            argCount = READ_BYTE() + AS_INT(pop());
            goto cont_list;

        case OP_UNPACK:
            aVal     = pop();
            argCount = AS_INT(pop());
            if (!IS_LIST(aVal)) {
                runtimeError("Can't %s type %s.", "unpack", valueType(aVal));
                goto handleError;
            }
            aLst      = AS_LIST(aVal);
            itemCount = aLst->arr.count;
            if (vm.sp + itemCount >= vm.stack + (STACK_MAX-1)) {
                runtimeError("Lox value stack overflow.");
                goto handleError;
            }
            for (i = 0; i < itemCount; i++)
                vm.sp[i] = aLst->arr.values[i];
            vm.sp += itemCount;
            pushUnchecked(INT_VAL(itemCount + argCount));
            goto nextInstNoSO;

        case OP_GET_INDEX:
            aVal = peek(0); // index
            bVal = peek(1); // object

            if (IS_LIST(bVal)) {
                bLst = AS_LIST(bVal);
                if (!IS_INT(aVal)) {
                    runtimeError("%s is not an integer.", "List index");
                    goto handleError;
                }
                index = AS_INT(aVal);
                if (!isValidListIndex(bLst, &index)) {
                    runtimeError("%s out of range.", "List index");
                    goto handleError;
                }
                resVal = indexFromList(bLst, index);
                dropNpush(2, resVal);
                goto nextInstNoSO;
            } else if (IS_STRING(bVal)) {
                bStr = AS_STRING(bVal);
                if (!IS_INT(aVal)) {
                    runtimeError("%s is not an integer.", "String index");
                    goto handleError;
                }
                index = AS_INT(aVal);
                if (!isValidStringIndex(bStr, &index)) {
                    runtimeError("%s out of range.", "String index");
                    goto handleError;
                }
                resVal = OBJ_VAL(indexFromString(bStr, index));
                dropNpush(2, resVal);
                goto nextInstNoSO;
            } else if (IS_INSTANCE(bVal)) {
                instance = AS_INSTANCE(bVal);
                resVal   = NIL_VAL;
                tableGet(&instance->fields, aVal, &resVal); // not found -> nil
                dropNpush(2, resVal);
                goto nextInstNoSO;
            } else {
                runtimeError("Can't %s type %s.", "index into", valueType(bVal));
                goto handleError;
            }

        case OP_SET_INDEX:
            cVal = peek(0); // item
            aVal = peek(1); // index
            bVal = peek(2); // object   

            if (IS_LIST(bVal)) {
                bLst = AS_LIST(bVal);
                if (!IS_INT(aVal)) {
                    runtimeError("%s is not an integer.", "List index");
                    goto handleError;
                }
                index = AS_INT(aVal);
                if (!isValidListIndex(bLst, &index)) {
                    runtimeError("%s out of range.", "List index");
                    goto handleError;
                }
                storeToList(bLst, index, cVal);
                dropNpush(3, cVal);
                goto nextInstNoSO;
            } else if (IS_INSTANCE(bVal)) {
                instance = AS_INSTANCE(bVal);
                tableSet(&instance->fields, aVal, cVal);
                dropNpush(3, cVal);
                goto nextInstNoSO;
            } else {
                runtimeError("Can't %s type %s.", "store into", valueType(bVal));
                goto handleError;
            }

        case OP_GET_SLICE:
            aVal = pop();   // end
            bVal = pop();   // begin
            cVal = peek(0); // object

            if (!IS_INT(bVal)) {
                runtimeError("%s is not an integer.", "Slice begin");
                goto handleError;
            }
            begin = AS_INT(bVal);
            if (!IS_INT(aVal)) {
                runtimeError("%s is not an integer.", "Slice end");
                goto handleError;
            }
            end = AS_INT(aVal);
            if (IS_LIST(cVal)) {
                aLst   = AS_LIST(cVal);
                resVal = OBJ_VAL(sliceFromList(aLst, begin, end));
                dropNpush(1, resVal);
                goto nextInstNoSO;
            } else if (IS_STRING(cVal)) {
                aStr   = AS_STRING(cVal);
                resVal = OBJ_VAL(sliceFromString(aStr, begin, end));
                dropNpush(1, resVal);
                goto nextInstNoSO;
            } else {
                runtimeError("Can't %s type %s.", "slice into", valueType(cVal));
                goto handleError;
            }

        case OP_GET_ITVAL:
        case OP_GET_ITKEY:
            aVal = peek(0); // iterator
            if (!IS_ITERATOR(aVal)) {
                runtimeError("Can't %s type %s.", "deref", valueType(aVal));
                goto handleError;
            }
            aIt = AS_ITERATOR(aVal);
            if (!isValidIterator(aIt)) {
                runtimeError("Invalid iterator.");
                goto handleError;
            }
            resVal = getIterator(aIt, CURR_INSTR()==OP_GET_ITKEY);
            dropNpush(1, resVal);
            goto nextInstNoSO;

        case OP_SET_ITVAL:
            bVal = peek(0); // item
            aVal = peek(1); // iterator
            if (!IS_ITERATOR(aVal)) {
                runtimeError("Can't %s type %s.", "deref", valueType(aVal));
                goto handleError;
            }
            aIt = AS_ITERATOR(aVal);
            if (!isValidIterator(aIt)) {
                runtimeError("Invalid iterator.");
                goto handleError;
            }
            setIterator(aIt, bVal);
            dropNpush(2, bVal);
            goto nextInstNoSO;

        default:
            runtimeError("Invalid byte code $%02x.", CURR_INSTR());
    }

handleError:
    if (vm.handleException) {
        // handler and exception have already been pushed in runtimeError()
        vm.handleException = false;
        argCount           = 1;
        goto cont_call;
    }
    return INTERPRET_RUNTIME_ERROR;
}

#ifndef KIT68K
#include <signal.h>

static void irqHandler(int ignored) {
    vm.interrupted = true;
}

static void handleInterrupts(bool enable) {
    if (enable)
        signal(SIGINT, &irqHandler);
    else
        signal(SIGINT, SIG_DFL);
}
#endif

InterpretResult interpret(const char* source) {
    ObjFunction*    function = compile(source);
    ObjClosure*     closure;
    InterpretResult result;

    if (function == NULL)
        return INTERPRET_COMPILE_ERROR;

    pushUnchecked(OBJ_VAL(function));
    closure = makeClosure(function);
    dropNpush(1, OBJ_VAL(closure));
    
    callClosure(closure, 0);

    RESET_INTERRUPTED();
#ifdef LOX_DBG
    vm.started = clock();
#endif
    handleInterrupts(true);
    result = run();
    handleInterrupts(false);

#ifdef LOX_DBG
    STATIC_BREAKPOINT();
    if (vm.debug_statistics) {
#ifdef KIT68K
        printf("[%d.%02d sec; %u steps; %d bytes; %d GCs]\n",
               (clock() - vm.started) / 100, (clock() - vm.started) % 100,
               vm.stepsExecuted, vm.totallyAllocated, vm.numGCs);
#else
        printf("[%.3f sec; %llu steps; %d bytes; %d GCs]\n",
               (double)(clock() - vm.started) / CLOCKS_PER_SEC,
               vm.stepsExecuted, vm.totallyAllocated, vm.numGCs);
#endif
    }
#endif
    return result;
}
