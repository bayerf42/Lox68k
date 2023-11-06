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

static void indentCallTrace(void) {
    int depth = vm.frameCount;
    while (depth--)
        printf("  ");
}

static void printArgList(int argCount, Value* args) {
    const char* separator = "";
    while (argCount--) {
        printf(separator);
        separator = ", ";
        printValue(*args++, false, true);
    } 
}

void runtimeError(const char* format, ...) {
    va_list      args;
    size_t       instruction;
    int          i;
    CallFrame*   frame;
    ObjFunction* function;

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");

    for (i = vm.frameCount - 1; i >= 0; i--) {
        frame       = &vm.frames[i];
        function    = frame->closure->function;
        instruction = frame->ip - function->chunk.code - 1;
        printf("[line %d] in %s\n", getLine(&function->chunk, instruction), functionName(function));
    }
    resetStack();
}

void initVM(void) {
    resetStack();
    vm.objects        = NULL;
    vm.bytesAllocated = 0;
    vm.grayCount      = 0;
    vm.lambdaCount    = 0;

    initTable(&vm.globals);
    initTable(&vm.strings);

    vm.initString = NULL;
    vm.initString = makeString("init", 4);

    defineAllNatives();
}

void freeVM(void) {
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    vm.initString = NULL;
    freeObjects();
}

void push(Value value) {
    if (vm.sp >= (vm.stack + (STACK_MAX-1))) {
        vm.hadStackoverflow = true;
        return;
    }
    *vm.sp++ = value;
}

#define dropNpush(n,value) {   \
    vm.sp -= (n) - 1;    \
    vm.sp[-1] = (value); \
}

#define CHECK_ARITH_ERROR              \
if (errno != 0) {                      \
    runtimeError("Arithmetic error."); \
    return INTERPRET_RUNTIME_ERROR;    \
}

static bool call(ObjClosure* closure, int argCount) {
    CallFrame*  frame;
    ObjList*    args;
    int         itemCount;
    int         arity;

    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Lox call stack overflow.");
        return false;
    }

    if (vm.debug_trace_calls) {
        indentCallTrace();
        printf("--> %s (", functionName(closure->function));
        printArgList(argCount, vm.sp - argCount);
        printf(")\n");
    }

    arity = closure->function->arity & ARITY_MASK;
    if (closure->function->arity & REST_PARM_MASK) {
        if (argCount < arity - 1) {
            runtimeError("Expected %s%d arguments but got %d.", "at least ", arity - 1, argCount);
            return false;
        }
        itemCount = argCount - arity + 1;
        args = makeList(itemCount, vm.sp - itemCount, itemCount, 1);
        dropNpush(itemCount, OBJ_VAL(args));
    }
    else if (argCount != arity) {
        runtimeError("Expected %s%d arguments but got %d.", "", arity, argCount);
        return false;
    }

    frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip      = closure->function->chunk.code;
    frame->fp      = vm.sp - arity - 1;
    return true;
}

static bool callValue(Value callee, int argCount) {
    ObjNative* native;
    ObjClass*  klass;
    ObjBound*  bound;
    Value      initializer = NIL_VAL;
    bool       logNatRes   = false;

    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_BOUND:
                bound = AS_BOUND(callee);
                vm.sp[-argCount - 1] = bound->receiver;
                return call(bound->method, argCount);

            case OBJ_CLASS:
                klass = AS_CLASS(callee);
                vm.sp[-argCount - 1] = OBJ_VAL(makeInstance(klass));
                if (tableGet(&klass->methods, OBJ_VAL(vm.initString), &initializer))
                    return call(AS_CLOSURE(initializer), argCount);
                else if (argCount != 0) {
                    runtimeError("Expected %s%d arguments but got %d.", "", 0, argCount);
                    return false;
                }
                return true;

            case OBJ_CLOSURE:
                return call(AS_CLOSURE(callee), argCount);

            case OBJ_NATIVE:
                native = AS_NATIVE(callee);

                if (vm.debug_trace_natives) {
                    if (vm.debug_trace_calls)
                        indentCallTrace();
                    logNatRes = true;
                    printf("--- %s (", nativeName(native->function));
                    printArgList(argCount, vm.sp - argCount);
                    printf(") -> ");
                }

                if (!checkNativeSignature(native->signature, argCount, vm.sp - argCount) ||
                    !(*native->function)(argCount, vm.sp - argCount))
                    return false;
                vm.sp -= argCount;

                if (logNatRes) {
                    printValue(vm.sp[-1], false, true);
                    printf("\n");
                } 
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
    return call(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjString* name, int argCount) {
    Value        receiver = peek(argCount);
    ObjInstance* instance;
    Value        value = NIL_VAL;

    if (!IS_INSTANCE(receiver)) {
        runtimeError("Only instances have methods.");
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
        upvalue = upvalue->nextUpvalue;
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
    Value     method = peek(0);
    ObjClass* klass  = AS_CLASS(peek(1));
    tableSet(&klass->methods, OBJ_VAL(name), method);
    drop();
}

#define READ_BYTE()   (*frame->ip++)
#define READ_USHORT() (frame->ip += 2, (frame->ip[-2] << 8) | frame->ip[-1])

static InterpretResult run(void) {
    Value*  slot;
    int     instruction;
    int     index, begin, end, i;
    Value   constant;

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
    CallFrame    *frame  = &vm.frames[vm.frameCount - 1];
    Value        *consts = frame->closure->function->chunk.constants.values;

    vm.hadStackoverflow = false;
    vm.stepsExecuted    = 0;

    _trap(1);

    for (;;) {
        if (vm.hadStackoverflow) {
            runtimeError("Lox value stack overflow.");
            return INTERPRET_RUNTIME_ERROR;
        }

        if (INTERRUPTED()) {
            (void)READ_BYTE();
            runtimeError("Interrupted.");
            return INTERPRET_INTERRUPTED;
        }

        if (vm.debug_trace_steps) {
            for (slot = vm.stack; slot < vm.sp; slot++) {
                printValue(*slot, true, true);
                printf(" | ");
            }
            printf("\n");
            disassembleInst(&frame->closure->function->chunk,
                            (int)(frame->ip - frame->closure->function->chunk.code));
            printf("\n");
        }

        ++vm.stepsExecuted;
        instruction = READ_BYTE();

        switch (instruction) {
            case OP_CONSTANT:
                index = READ_BYTE();
                constant = consts[index];
                push(constant);
                break;

            case OP_NIL:   push(NIL_VAL);    break;
            case OP_TRUE:  push(TRUE_VAL);   break;
            case OP_FALSE: push(FALSE_VAL);  break;
            case OP_ZERO:  push(INT_VAL(0)); break;
            case OP_ONE:   push(INT_VAL(1)); break;
            case OP_POP:   drop();           break;
            case OP_DUP:   push(peek(0));    break;

            case OP_SWAP:
                aVal    = peek(0);
                peek(0) = peek(1);
                peek(1) = aVal;
                break;
  
            case OP_GET_LOCAL:
                slotNr = READ_BYTE();
                push(frame->fp[slotNr]);
                break;

            case OP_SET_LOCAL:
                slotNr = READ_BYTE();
                frame->fp[slotNr] = peek(0);
                break;

            case OP_GET_GLOBAL:
                index    = READ_BYTE();
                constant = consts[index];
                if (!tableGet(&vm.globals, constant, &aVal)) {
                    runtimeError("Undefined variable '%s'.", AS_CSTRING(constant));
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(aVal);
                break;

            case OP_DEF_GLOBAL:
                index    = READ_BYTE();
                constant = consts[index];
                tableSet(&vm.globals, constant, peek(0));
                drop();
                break;

            case OP_SET_GLOBAL:
                index    = READ_BYTE();
                constant = consts[index];
                if (tableSet(&vm.globals, constant, peek(0))) {
                    tableDelete(&vm.globals, constant);
                    runtimeError("Undefined variable '%s'.", AS_CSTRING(constant));
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;

            case OP_GET_UPVALUE:
                slotNr = READ_BYTE();
                push(*frame->closure->upvalues[slotNr]->location);
                break;

            case OP_SET_UPVALUE:
                slotNr = READ_BYTE();
                *frame->closure->upvalues[slotNr]->location = peek(0);
                break;

            case OP_GET_PROPERTY:
                if (!IS_INSTANCE(peek(0))) {
                    runtimeError("Only instances have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                instance = AS_INSTANCE(peek(0));
                index    = READ_BYTE();
                constant = consts[index];
                if (tableGet(&instance->fields, constant, &aVal)) {
                    dropNpush(1, aVal);
                    break;
                }
                aStr = AS_STRING(constant);
                if (!bindMethod(instance->klass, aStr))
                    return INTERPRET_RUNTIME_ERROR;
                break;

            case OP_SET_PROPERTY:
                if (!IS_INSTANCE(peek(1))) {
                    runtimeError("Only instances have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                instance = AS_INSTANCE(peek(1));
                index    = READ_BYTE();
                constant = consts[index];
                tableSet(&instance->fields, constant, peek(0));
                aVal = pop();
                dropNpush(1, aVal);
                break;

            case OP_GET_SUPER:
                index      = READ_BYTE();
                constant   = consts[index];
                aStr       = AS_STRING(constant);
                superclass = AS_CLASS(pop());
                if (!bindMethod(superclass, aStr))
                    return INTERPRET_RUNTIME_ERROR;
                break;

            case OP_EQUAL:
                bVal = pop();
                aVal = pop();
                pushUnchecked(BOOL_VAL(valuesEqual(aVal, bVal)));
                break;

            case OP_LESS:
                if (IS_INT(peek(0))) {
                    if (IS_INT(peek(1))) {
                        bInt = AS_INT(pop());
                        aInt = AS_INT(pop());
                        pushUnchecked(BOOL_VAL(aInt < bInt));
                        break;
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
                    runtimeError("Can't %s types %s and %s.", "compare",
                                 valueType(peek(1)), valueType(peek(0)));
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;

            case OP_ADD:
                if (IS_INT(peek(0))) {
                    if (IS_INT(peek(1))) {
                        bInt = AS_INT(pop());
                        aInt = AS_INT(pop());
                        pushUnchecked(INT_VAL(aInt + bInt));
                        break;
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
                    dropNpush(2, makeReal(add(aReal,bReal)));
                    CHECK_ARITH_ERROR
                } else if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    bStr = AS_STRING(peek(0));
                    aStr = AS_STRING(peek(1));
                    resStr = concatStrings(aStr, bStr);
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
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;

            case OP_SUB:
                if (IS_INT(peek(0))) {
                    if (IS_INT(peek(1))) {
                        bInt = AS_INT(pop());
                        aInt = AS_INT(pop());
                        pushUnchecked(INT_VAL(aInt - bInt));
                        break;
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
                    return INTERPRET_RUNTIME_ERROR;
                }
                dropNpush(2, makeReal(sub(aReal,bReal)));
                CHECK_ARITH_ERROR
                break;

            case OP_MUL:
                if (IS_INT(peek(0))) {
                    if (IS_INT(peek(1))) {
                        bInt = AS_INT(pop());
                        aInt = AS_INT(pop());
                        pushUnchecked(INT_VAL(aInt * bInt));
                        break;
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
                    return INTERPRET_RUNTIME_ERROR;
                }
                dropNpush(2, makeReal(mul(aReal,bReal)));
                CHECK_ARITH_ERROR
                break;

            case OP_DIV:
                if (IS_INT(peek(0))) {
                    if (IS_INT(peek(1))) {
                        bInt = AS_INT(pop());
                        aInt = AS_INT(pop());
                        pushUnchecked(INT_VAL(aInt / bInt));
                        break;
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
                    return INTERPRET_RUNTIME_ERROR;
                }
                dropNpush(2, makeReal(div(aReal,bReal)));
                CHECK_ARITH_ERROR
                break;

            case OP_MOD:
                if (IS_INT(peek(0)) && IS_INT(peek(1))) {
                    bInt = AS_INT(pop());
                    aInt = AS_INT(pop());
                    pushUnchecked(INT_VAL(aInt % bInt));
                } else
                    goto typeErrorDiv;
                break;

            case OP_NOT:
                peek(0) = BOOL_VAL(IS_FALSEY(peek(0)));
                break;

            case OP_PRINT:
                printValue(pop(), false, false);
                break;

            case OP_PRINTLN:
                printValue(pop(), false, false);
                printf("\n");
                break;

            case OP_JUMP:
                offset = READ_USHORT();
                frame->ip += offset;
                break;

            case OP_JUMP_OR:
                offset = READ_USHORT();
                if (IS_FALSEY(peek(0)))
                    drop();
                else
                    frame->ip += offset;
                break;

            case OP_JUMP_AND:
                offset = READ_USHORT();
                if (IS_FALSEY(peek(0)))
                    frame->ip += offset;
                else
                    drop();
                break;

            case OP_JUMP_TRUE:
                offset = READ_USHORT();
                if (!IS_FALSEY(pop()))
                    frame->ip += offset;
                break;

            case OP_JUMP_FALSE:
                offset = READ_USHORT();
                if (IS_FALSEY(pop()))
                    frame->ip += offset;
                break;

            case OP_LOOP:
                offset = READ_USHORT();
                frame->ip -= offset;
                break;

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
                    return INTERPRET_RUNTIME_ERROR;
            updateFrame:
                frame  = &vm.frames[vm.frameCount - 1];
                consts = frame->closure->function->chunk.constants.values;
                break;

            case OP_VCALL:
                argCount = READ_BYTE() + AS_INT(pop());
                goto cont_call;

            case OP_INVOKE:
                index    = READ_BYTE();
                argCount = READ_BYTE();
            cont_invoke:
                constant = consts[index];
                aStr = AS_STRING(constant);
                if (!invoke(aStr, argCount))
                    return INTERPRET_RUNTIME_ERROR;
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
                    return INTERPRET_RUNTIME_ERROR;
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
                break;

            case OP_CLOSE_UPVALUE:
                closeUpvalues(vm.sp - 1);
                drop();
                break;

            case OP_RETURN_NIL:
                resVal = NIL_VAL;
                goto cont_ret;

            case OP_RETURN:
                resVal = pop();
            cont_ret:
                closeUpvalues(frame->fp);
                vm.frameCount--;

                if (vm.debug_trace_calls) {
                    indentCallTrace();
                    printf("<-- %s ", functionName(frame->closure->function));
                    printValue(resVal, false, true);
                    printf("\n");
                }

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
                break;

            case OP_INHERIT:
                aVal = peek(1);
                if (!IS_CLASS(aVal)) {
                    runtimeError("Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                subclass = AS_CLASS(peek(0));
                tableAddAll(&AS_CLASS(aVal)->methods, &subclass->methods);
                drop();
                break;

            case OP_METHOD:
                index    = READ_BYTE();
                constant = consts[index];
                aStr     = AS_STRING(constant);
                defineMethod(aStr);
                break;

            case OP_LIST:
                argCount = READ_BYTE();
            cont_list:
                aLst     = makeList(argCount, vm.sp - argCount, argCount, 1);
                dropNpush(argCount, OBJ_VAL(aLst));
                break;

            case OP_VLIST:
                argCount = READ_BYTE() + AS_INT(pop());
                goto cont_list;

            case OP_UNPACK:
                aVal     = pop();
                argCount = AS_INT(pop());
                if (!IS_LIST(aVal)) {
                    runtimeError("Can't %s type %s.", "unpack", valueType(aVal));
                    return INTERPRET_RUNTIME_ERROR;
                }
                aLst      = AS_LIST(aVal);
                itemCount = aLst->arr.count;
                if (vm.sp + itemCount >= vm.stack + STACK_MAX) {
                    runtimeError("Lox value stack overflow.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                for (i = 0; i < itemCount; i++)
                    vm.sp[i] = aLst->arr.values[i];
                vm.sp += itemCount;
                pushUnchecked(INT_VAL(itemCount + argCount));
                break;

            case OP_GET_INDEX:
                aVal = peek(0); // index
                bVal = peek(1); // object

                if (IS_LIST(bVal)) {
                    bLst = AS_LIST(bVal);
                    if (!IS_INT(aVal)) {
                        runtimeError("%s is not an integer.", "List index");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    index = AS_INT(aVal);
                    if (!isValidListIndex(bLst, index)) {
                        runtimeError("%s out of range.", "List index");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    resVal = indexFromList(bLst, index);
                    dropNpush(2, resVal);
                    break;
                } else if (IS_STRING(bVal)) {
                    bStr = AS_STRING(bVal);
                    if (!IS_INT(aVal)) {
                        runtimeError("%s is not an integer.", "String index");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    index = AS_INT(aVal);
                    if (!isValidStringIndex(bStr, index)) {
                        runtimeError("%s out of range.", "String index");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    resVal = OBJ_VAL(indexFromString(bStr, index));
                    dropNpush(2, resVal);
                    break;
                } else if (IS_INSTANCE(bVal)) {
                    instance = AS_INSTANCE(bVal);
                    resVal   = NIL_VAL;
                    tableGet(&instance->fields, aVal, &resVal); // not found -> nil
                    dropNpush(2, resVal);
                    break;
                } else {
                    runtimeError("Can't %s type %s.", "index into", valueType(bVal));
                    return INTERPRET_RUNTIME_ERROR;
                }

            case OP_SET_INDEX:
                cVal = peek(0); // item
                aVal = peek(1); // index
                bVal = peek(2); // object   

                if (IS_LIST(bVal)) {
                    bLst = AS_LIST(bVal);
                    if (!IS_INT(aVal)) {
                        runtimeError("%s is not an integer.", "List index");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    index = AS_INT(aVal);
                    if (!isValidListIndex(bLst, index)) {
                        runtimeError("%s out of range.", "List index");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    storeToList(bLst, index, cVal);
                    dropNpush(3, cVal);
                    break;
                } else if (IS_INSTANCE(bVal)) {
                    instance = AS_INSTANCE(bVal);
                    tableSet(&instance->fields, aVal, cVal);
                    dropNpush(3, cVal);
                    break;
                } else {
                    runtimeError("Can't %s type %s.", "store into", valueType(bVal));
                    return INTERPRET_RUNTIME_ERROR;
                }

            case OP_GET_SLICE:
                aVal = pop();   // end
                bVal = pop();   // begin
                cVal = peek(0); // object

                if (!IS_INT(bVal)) {
                    runtimeError("%s is not an integer.", "Slice begin");
                    return INTERPRET_RUNTIME_ERROR;
                }
                begin = AS_INT(bVal);
                if (!IS_INT(aVal)) {
                    runtimeError("%s is not an integer.", "Slice end");
                    return INTERPRET_RUNTIME_ERROR;
                }
                end = AS_INT(aVal);
                if (IS_LIST(cVal)) {
                    aLst   = AS_LIST(cVal);
                    resVal = OBJ_VAL(sliceFromList(aLst, begin, end));
                    dropNpush(1, resVal);
                    break;
                } else if (IS_STRING(cVal)) {
                    aStr   = AS_STRING(cVal);
                    resVal = OBJ_VAL(sliceFromString(aStr, begin, end));
                    dropNpush(1, resVal);
                    break;
                } else {
                    runtimeError("Can't %s type %s.", "slice into", valueType(cVal));
                    return INTERPRET_RUNTIME_ERROR;
                }

            case OP_GET_ITVAL:
            case OP_GET_ITKEY:
                aVal = peek(0); // iterator
                if (!IS_ITERATOR(aVal)) {
                    runtimeError("Can't %s type %s.", "deref", valueType(aVal));
                    return INTERPRET_RUNTIME_ERROR;
                }
                aIt = AS_ITERATOR(aVal);
                if (!isValidIterator(aIt)) {
                    runtimeError("Invalid iterator.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                resVal = getIterator(aIt, instruction==OP_GET_ITKEY);
                dropNpush(1, resVal);
                break;

            case OP_SET_ITVAL:
                bVal = peek(0); // item
                aVal = peek(1); // iterator
                if (!IS_ITERATOR(aVal)) {
                    runtimeError("Can't %s type %s.", "deref", valueType(aVal));
                    return INTERPRET_RUNTIME_ERROR;
                }
                aIt = AS_ITERATOR(aVal);
                if (!isValidIterator(aIt)) {
                    runtimeError("Invalid iterator.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                setIterator(aIt, bVal);
                dropNpush(2, bVal);
                break;

            default:
                runtimeError("Invalid byte code $%02x.", instruction);
                return INTERPRET_RUNTIME_ERROR;
        }
    }
    return INTERPRET_RUNTIME_ERROR; // not reached
}

InterpretResult interpret(const char* source) {
    ObjFunction*    function = compile(source);
    ObjClosure*     closure;
    InterpretResult result;

    if (function == NULL)
        return INTERPRET_COMPILE_ERROR;

    pushUnchecked(OBJ_VAL(function));
    closure = makeClosure(function);
    dropNpush(1, OBJ_VAL(closure));
    
    call(closure, 0);

    RESET_INTERRUPTED();
    vm.started = clock();
    handleInterrupts(true);
    result = run();
    handleInterrupts(false);

    _trap(1);

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
    return result;
}
