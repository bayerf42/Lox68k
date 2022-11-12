#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "native.h"
#include "vm.h"
#include "value.h"

VM vm;

static void resetStack(void) {
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

void runtimeError(const char* format, ...) {
    va_list args;
    size_t instruction;
    int line, i;
    CallFrame* frame;
    ObjFunction* function;

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");

    for (i = vm.frameCount - 1; i >= 0; i--) {
        frame = &vm.frames[i];
        function = frame->closure->function;
        instruction = frame->ip - function->chunk.code - 1;
        printf("[line %d] in ", getLine(&function->chunk, instruction));
        if (function->name == NULL) {
            printf("<script>\n");
        } else {
            printf("%s\n", function->name->chars);
        }
    }
    resetStack();
}

void initVM(void) {
    resetStack();
    vm.objects = NULL;
    vm.bytesAllocated = 0;
    vm.grayCount = 0;

    initTable(&vm.globals);
    initTable(&vm.strings);

    vm.initString = NULL;
    vm.initString = copyString("init", 4);

    defineAllNatives();
}

void freeVM(void) {
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    vm.initString = NULL;
    freeObjects();
}

void push(Value value) {
    if (vm.stackTop >= (vm.stack + (STACK_MAX-1))) {
        vm.hadStackoverflow = true;
        return;
    }
  
    *vm.stackTop++ = value;
}

static void dropNpush(int n, Value value) {
    vm.stackTop -= n - 1;
    vm.stackTop[-1] = value;
}

static bool call(ObjClosure* closure, int argCount) {
    CallFrame* frame;
    ObjList* args;
    int itemCount,i;

    if (closure->function->isVarArg) {
        if (argCount < closure->function->arity - 1) {
            runtimeError("Expected at least %d arguments but got %d.",
                         closure->function->arity - 1, argCount);
            return false;
        }
        args = newList();
        itemCount = argCount - closure->function->arity + 1;
        push(OBJ_VAL(args)); // protect from GC
        for (i = itemCount; i>0; --i) {
            appendToList(args, peek(i));
        }
        dropNpush(itemCount + 1, OBJ_VAL(args));
        argCount = closure->function->arity; // actual parameter count
    }
    else if (argCount != closure->function->arity) {
        runtimeError("Expected %d arguments but got %d.", closure->function->arity, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Lox call stack overflow.");
        return false;
    }

    frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount) {
    NativeFn native;
    ObjClass* klass;
    Value initializer = NIL_VAL;
    ObjBoundMethod* bound;

    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_BOUND_METHOD: {
                bound = AS_BOUND_METHOD(callee);
                vm.stackTop[-argCount - 1] = bound->receiver;
                return call(bound->method, argCount);
            }
            case OBJ_CLASS: {
                klass = AS_CLASS(callee);
                vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));
                if (tableGet(&klass->methods, OBJ_VAL(vm.initString), &initializer)) {
                    return call(AS_CLOSURE(initializer), argCount);
                } else if (argCount != 0) {
                    runtimeError("Expected 0 arguments but got %d.", argCount);
                    return false;
                }
                return true;
            }
            case OBJ_CLOSURE:
                return call(AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                native = AS_NATIVE(callee);
                if (!checkNativeSignature(AS_NATIVE_SIG(callee), argCount, vm.stackTop - argCount)) return false;
                if (!(*native)(argCount, vm.stackTop - argCount)) return false;
                vm.stackTop -= argCount;
                return true;
            }
            default:
                break; // Non-callable object type
        }
    }
    runtimeError("Can only call functions and classes.");
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
    Value receiver = peek(argCount);
    ObjInstance* instance;
    Value value = NIL_VAL;

    if (!IS_INSTANCE(receiver)) {
        runtimeError("Only instances have methods.");
        return false;
    }
    instance = AS_INSTANCE(receiver);

    if (tableGet(&instance->fields, OBJ_VAL(name), &value)) {
        vm.stackTop[-argCount - 1] = value;
        return callValue(value, argCount);
    }
    return invokeFromClass(instance->klass, name, argCount);
}

static bool bindMethod(ObjClass* klass, ObjString* name) {
    Value method = NIL_VAL;
    ObjBoundMethod* bound;
    if (!tableGet(&klass->methods, OBJ_VAL(name), &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }

    bound = newBoundMethod(peek(0), AS_CLOSURE(method));
    dropNpush(1, OBJ_VAL(bound));
    return true;
}

static ObjUpvalue* captureUpvalue(Value* local) {
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm.openUpvalues;
    ObjUpvalue* createdUpvalue;

    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->nextUpvalue;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    createdUpvalue = newUpvalue(local);
    createdUpvalue->nextUpvalue = upvalue;

    if (prevUpvalue == NULL) {
        vm.openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->nextUpvalue = createdUpvalue;
    }

    return createdUpvalue;
}

static void closeUpvalues(Value* last) {
    ObjUpvalue* upvalue;
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
        upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->nextUpvalue;
    }
}

static void defineMethod(ObjString* name) {
    Value method = peek(0);
    ObjClass* klass = AS_CLASS(peek(1));
    tableSet(&klass->methods, OBJ_VAL(name), method);
    drop();
}

#define READ_BYTE() (*frame->ip++)
#define READ_USHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define BINARY_OP(valueType, op) \
    do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtimeError("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        bNum = AS_NUMBER(pop()); \
        aNum = AS_NUMBER(pop()); \
        push(valueType(aNum op bNum)); \
    } while (false)
#define COMPARE_OP(op) \
    do { \
        if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) { \
            bNum = AS_NUMBER(pop()); \
            aNum = AS_NUMBER(pop()); \
            push(BOOL_VAL(aNum op bNum)); \
        } else if (IS_STRING(peek(0)) && IS_STRING(peek(1))) { \
            bStr = AS_STRING(peek(0)); \
            aStr = AS_STRING(peek(1)); \
            dropNpush(2, BOOL_VAL(strcmp(aStr->chars, bStr->chars) op 0)); \
        } else { \
            runtimeError("Operands must be numbers or strings."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
    } while (false)


static InterpretResult run(void) {
    Value* slot;
    uint8_t instruction;
    int index, begin, end, i;
    Value constant;

    // The IDE68K ancient C compiler generates wrong code for local vars in case branches.
    // Thus, we declare all needed variables at function start..
    Number aNum, bNum;
    Value  aVal=NIL_VAL, bVal, cVal, resVal;
    ObjString *aStr, *bStr, *resStr;
    ObjList   *aLst, *bLst, *resLst;
    ObjClass  *superclass, *subclass;
    ObjInstance* instance;
    uint8_t slotNr;
    ObjFunction* function;
    ObjClosure* closure;
    int argCount, itemCount;
    uint16_t offset;
    uint8_t isLocal, upIndex;

    CallFrame* frame = &vm.frames[vm.frameCount - 1];
    vm.hadStackoverflow = false;
    vm.stepsExecuted = 0;

    for (;;) {
        if (vm.hadStackoverflow) {
            runtimeError("Lox value stack overflow.");
            return INTERPRET_RUNTIME_ERROR;
        }

        if (vm.interrupted) {
            READ_BYTE();
            runtimeError("Interrupted.");
            return INTERPRET_INTERRUPTED;
        }

        if (vm.debug_trace_execution) {
            printf("          ");
            for (slot = vm.stack; slot < vm.stackTop; slot++) {
                printf("[");
                printValue(*slot, true);
                printf("] ");
            }
            printf("\n");
            disassembleInstruction(&frame->closure->function->chunk,
                                   (int)(frame->ip - frame->closure->function->chunk.code));
        }

        ++vm.stepsExecuted;
        instruction = READ_BYTE();
        switch (instruction) {
            case OP_CONSTANT: {
                index = READ_BYTE();
                constant = frame->closure->function->chunk.constants.values[index];
                push(constant);
                break;
            }
            case OP_NIL:   push(NIL_VAL); break;
            case OP_TRUE:  push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_POP:   drop(); break;
            case OP_SWAP:  aVal = peek(0); peek(0) = peek(1); peek(1) = aVal; break;
            case OP_GET_LOCAL: {
                slotNr = READ_BYTE();
                push(frame->slots[slotNr]);
                break;
            }
            case OP_SET_LOCAL: {
                slotNr = READ_BYTE();
                frame->slots[slotNr] = peek(0);
                break;
            }
            case OP_GET_GLOBAL: {
                index = READ_BYTE();
                constant = frame->closure->function->chunk.constants.values[index];
                aStr = AS_STRING(constant);
                if (!tableGet(&vm.globals, constant, &aVal)) {
                    runtimeError("Undefined variable '%s'.", aStr->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(aVal);
                break;
            }
            case OP_DEF_GLOBAL: {
                index = READ_BYTE();
                constant = frame->closure->function->chunk.constants.values[index];
                tableSet(&vm.globals, constant, peek(0));
                drop();
                break;
            }
            case OP_SET_GLOBAL: {
                index = READ_BYTE();
                constant = frame->closure->function->chunk.constants.values[index];
                aStr = AS_STRING(constant);
                if (tableSet(&vm.globals, constant, peek(0))) {
                    tableDelete(&vm.globals, constant);
                    runtimeError("Undefined variable '%s'.", aStr->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_UPVALUE: {
                slotNr = READ_BYTE();
                push(*frame->closure->upvalues[slotNr]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                slotNr = READ_BYTE();
                *frame->closure->upvalues[slotNr]->location = peek(0);
                break;
            }
            case OP_GET_PROPERTY: {
                if (!IS_INSTANCE(peek(0))) {
                    runtimeError("Only instances have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                instance = AS_INSTANCE(peek(0));
                index = READ_BYTE();
                constant = frame->closure->function->chunk.constants.values[index];
                aStr = AS_STRING(constant);

                if (tableGet(&instance->fields, constant, &aVal)) {
                    dropNpush(1, aVal);
                    break;
                }

                if (!bindMethod(instance->klass, aStr)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_PROPERTY: {
                if (!IS_INSTANCE(peek(1))) {
                    runtimeError("Only instances have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                instance = AS_INSTANCE(peek(1));
                index = READ_BYTE();
                constant = frame->closure->function->chunk.constants.values[index];
                tableSet(&instance->fields, constant, peek(0));
                aVal = pop();
                dropNpush(1, aVal);
                break;
            }
            case OP_GET_SUPER: {
                index = READ_BYTE();
                constant = frame->closure->function->chunk.constants.values[index];
                aStr = AS_STRING(constant);
                superclass = AS_CLASS(pop());

                if (!bindMethod(superclass, aStr)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_EQUAL:
                bVal = pop();
                aVal = pop();
                push(BOOL_VAL(valuesEqual(aVal, bVal)));
                break;
            case OP_GREATER:  COMPARE_OP(>); break;
            case OP_LESS:     COMPARE_OP(<); break;
            case OP_ADD: {
                if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    bNum = AS_NUMBER(pop());
                    aNum = AS_NUMBER(pop());
                    resVal = NUMBER_VAL(aNum + bNum);
                    push(resVal);
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
                    runtimeError("Operands must be numbers, strings, or lists.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
            case OP_MODULO:   BINARY_OP(NUMBER_VAL, %); break;
            case OP_NOT:      push(BOOL_VAL(IS_FALSEY(pop()))); break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            case OP_PRINT:
                printValue(pop(), false);
                break;
            case OP_PRINTLN:
                printValue(pop(), false);
                printf("\n");
                break;
            case OP_JUMP: {
                offset = READ_USHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                offset = READ_USHORT();
                if (IS_FALSEY(peek(0))) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                offset = READ_USHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                argCount = READ_BYTE();
            cont_call:
                if (!callValue(peek(argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_VCALL: {
                argCount = READ_BYTE() + AS_NUMBER(pop());
                goto cont_call;
            }
            case OP_INVOKE: {
                index = READ_BYTE();
                argCount = READ_BYTE();
            cont_invoke:
                constant = frame->closure->function->chunk.constants.values[index];
                aStr = AS_STRING(constant);
                if (!invoke(aStr, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_VINVOKE: {
                index = READ_BYTE();
                argCount = READ_BYTE() + AS_NUMBER(pop());
                goto cont_invoke;
            }
            case OP_SUPER_INVOKE: {
                index = READ_BYTE();
                superclass = AS_CLASS(pop());
                argCount = READ_BYTE();
            cont_super_invoke:
                constant = frame->closure->function->chunk.constants.values[index];
                aStr = AS_STRING(constant);
                if (!invokeFromClass(superclass, aStr, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_VSUPER_INVOKE: {
                index = READ_BYTE();
                superclass = AS_CLASS(pop());
                argCount = READ_BYTE() + AS_NUMBER(pop());
                goto cont_super_invoke;
            }
            case OP_CLOSURE: {
                index = READ_BYTE();
                constant = frame->closure->function->chunk.constants.values[index];
                function = AS_FUNCTION(constant);
                closure = newClosure(function);
                push(OBJ_VAL(closure));
                for (i = 0; i < closure->upvalueCount; i++) {
                    isLocal = READ_BYTE();
                    upIndex = READ_BYTE();
                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(frame->slots + upIndex);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[upIndex];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE: {
                closeUpvalues(vm.stackTop - 1);
                drop();
                break;
            }
            case OP_RETURN: {
                resVal = pop();
                closeUpvalues(frame->slots);
                vm.frameCount--;
                if (vm.frameCount == 0) {
                    drop();
                    return INTERPRET_OK;
                }

                vm.stackTop = frame->slots;
                push(resVal);
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_CLASS:
                index = READ_BYTE();
                constant = frame->closure->function->chunk.constants.values[index];
                aStr = AS_STRING(constant);
                push(OBJ_VAL(newClass(aStr)));
                break;
            case OP_INHERIT: {
                aVal = peek(1);
                if (!IS_CLASS(aVal)) {
                    runtimeError("Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                subclass = AS_CLASS(peek(0));
                tableAddAll(&AS_CLASS(aVal)->methods, &subclass->methods);
                drop();
                break;
            }
            case OP_METHOD:
                index = READ_BYTE();
                constant = frame->closure->function->chunk.constants.values[index];
                aStr = AS_STRING(constant);
                defineMethod(aStr);
                break;
            case OP_LIST: {
                aLst = newList();
                itemCount = READ_BYTE();

                push(OBJ_VAL(aLst)); // protect from GC
                for (i = itemCount; i > 0; i--) {
                    appendToList(aLst, peek(i));
                }
                dropNpush(itemCount + 1, OBJ_VAL(aLst));
                break;
            }
            case OP_UNPACK: {
                aVal = pop();
                argCount = AS_NUMBER(pop());
                if (!IS_LIST(aVal)) {
                    runtimeError("Item to unpack is not a list.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                aLst = AS_LIST(aVal);
                itemCount = aLst->count;
                for (i = 0; i < itemCount; i++) {
                    push(aLst->items[i]);
                }
                push(NUMBER_VAL(itemCount + argCount));
                break;
            }
            case OP_GET_INDEX : {
                aVal = peek(0); // index
                bVal = peek(1); // object

                if (IS_LIST(bVal)) {
                    bLst = AS_LIST(bVal);
                    if (!IS_NUMBER(aVal)) {
                        runtimeError("List index is not a number.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    index = AS_NUMBER(aVal);
                    if (!isValidListIndex(bLst, index)) {
                        runtimeError("List index out of range.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    resVal = indexFromList(bLst, index);
                    dropNpush(2, resVal);
                    break;

                } else if (IS_STRING(bVal)) {
                    bStr = AS_STRING(bVal);
                    if (!IS_NUMBER(aVal)) {
                        runtimeError("String index is not a number.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    index = AS_NUMBER(aVal);
                    if (!isValidStringIndex(bStr, index)) {
                        runtimeError("String index out of range.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    resVal = OBJ_VAL(indexFromString(bStr, index));
                    dropNpush(2, resVal);
                    break;

                } else if (IS_INSTANCE(bVal)) {
                    instance = AS_INSTANCE(bVal);
                    resVal = NIL_VAL;
                    tableGet(&instance->fields, aVal, &resVal); // not found -> nil
                    dropNpush(2, resVal);
                    break;

                } else {
                    runtimeError("Invalid type to index into.");
                    return INTERPRET_RUNTIME_ERROR;
                }
            }
            case OP_SET_INDEX : {
                cVal = peek(0); // item
                aVal = peek(1); // index
                bVal = peek(2); // object   

                if (IS_LIST(bVal)) {
                    bLst = AS_LIST(bVal);
                    if (!IS_NUMBER(aVal)) {
                        runtimeError("List index is not a number.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    index = AS_NUMBER(aVal);
                    if (!isValidListIndex(bLst, index)) {
                        runtimeError("List index out of range.");
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
                    runtimeError("Invalid type to store into.");
                    return INTERPRET_RUNTIME_ERROR;
                }
            }
            case OP_GET_SLICE: {
                aVal = pop();   // end
                bVal = pop();   // begin
                cVal = peek(0); // object

                if (!IS_NUMBER(bVal)) {
                    runtimeError("Slice begin is not a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                begin = AS_NUMBER(bVal);
                if (!IS_NUMBER(aVal)) {
                    runtimeError("Slice end is not a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                end = AS_NUMBER(aVal);

                if (IS_LIST(cVal)) {
                    aLst = AS_LIST(cVal);
                    resVal = OBJ_VAL(sliceFromList(aLst, begin, end));
                    dropNpush(1, resVal);
                    break;

                } else if (IS_STRING(cVal)) {
                    aStr = AS_STRING(cVal);
                    resVal = OBJ_VAL(sliceFromString(aStr, begin, end));
                    dropNpush(1, resVal);
                    break;

                } else {
                    runtimeError("Invalid type to slice into.");
                    return INTERPRET_RUNTIME_ERROR;
                }
            }
        }
    }
    return INTERPRET_RUNTIME_ERROR; // not reached
}

#undef READ_BYTE
#undef READ_USHORT
#undef BINARY_OP

InterpretResult interpret(const char* source) {
    ObjFunction* function = compile(source);
    ObjClosure* closure;
    InterpretResult result;

    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    closure = newClosure(function);
    dropNpush(1, OBJ_VAL(closure));
    
    call(closure, 0);

    vm.interrupted = false;
#ifndef KIT68K
    vm.started = clock();
#endif
    handleInterrupts(true);
    result = run();
    handleInterrupts(false);
    if (vm.debug_statistics) {
#ifdef KIT68K
        printf("[%u steps; %d bytes; %d GCs]\n",
               vm.stepsExecuted, vm.totallyAllocated, vm.numGCs);
#else
        printf("[%.3f sec; %llu steps; %d bytes; %d GCs]\n",
               (double)(clock() - vm.started) / CLOCKS_PER_SEC, vm.stepsExecuted, vm.totallyAllocated, vm.numGCs);
#endif
    }
    return result;
}