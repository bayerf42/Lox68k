#include <stdlib.h>
#include <stdio.h>

#include "compiler.h"
#include "memory.h"
#include "vm.h"
#include "nano_malloc.h"

char big_buffer[INPUT_SIZE];

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    void* result;

#ifdef LOX_DBG
    if (vm.debug_log_gc & DBG_GC_STRESS) {
        if (newSize > oldSize)
            collectGarbage(false);
    }
#endif
    vm.bytesAllocated += newSize - oldSize;

    if (newSize == 0) {
        if (pointer != 0)
            nano_free(pointer);
        return NULL;
    }

    result = nano_malloc(newSize);

    if (result == NULL) {
#ifdef LOX_DBG
        if (vm.debug_log_gc & DBG_GC_GENERAL)
            putstr("GC -- malloc failed, now trying gc.\n");
#endif

        collectGarbage(true);

        result = nano_malloc(newSize);
        if (result == NULL) {
            putstr("Out of heap space, exiting.\n");
            exit(1);
        }
    }

#ifdef LOX_DBG
    vm.totallyAllocated += newSize;
#endif

    if (oldSize != 0) {
        fix_memcpy(result, pointer, (oldSize < newSize) ? oldSize : newSize);
        nano_free(pointer);
    }
    return result;
}

void markObject(Obj* object) {
    if (object == NULL || object->isMarked)
        return;

#ifdef LOX_DBG
    if (vm.debug_log_gc & DBG_GC_MARK) {
        printf("GC %05x mark ", (int32_t)object);
        printValue(OBJ_VAL(object), PRTF_MACHINE | PRTF_COMPACT | PRTF_NO_REALS);
        putstr("\n");
    }
#endif

    object->isMarked = true;

    if (object->type < (uint8_t)OBJ_NATIVE) { // Not a leaf object
        if (vm.grayCount >= GRAY_MAX) {
            putstr("Gray stack size exceeded, exiting.\n");
            exit(1);
        }
        vm.grayStack[vm.grayCount++] = object;
    }
}

void markValue(Value value) {
    if (IS_OBJ(value))
        markObject(AS_OBJ(value));
}

static void markArray(ValueArray* array) {
    int i;
    for (i = 0; i < array->count; i++)
        markValue(array->values[i]);
}

static void blackenObject(Obj* object) {
    int16_t i;

#ifdef LOX_DBG
    if (vm.debug_log_gc & DBG_GC_BLACK) {
        printf("GC %05x blak ", (int32_t)object);
        printValue(OBJ_VAL(object), PRTF_MACHINE | PRTF_COMPACT | PRTF_NO_REALS);
        putstr("\n");
    }
#endif

    switch (object->type) {
        case OBJ_BOUND:
            markValue(((ObjBound*)object)->receiver);
            markObject((Obj*)((ObjBound*)object)->method);
            break;

        case OBJ_CLASS:
            markObject((Obj*)((ObjClass*)object)->name);
            markObject((Obj*)((ObjClass*)object)->superClass);
            markTable(&((ObjClass*)object)->methods);
            break;

        case OBJ_CLOSURE:
            markObject((Obj*)((ObjClosure*)object)->function);
            for (i = 0; i < ((ObjClosure*)object)->upvalueCount; i++)
                markObject((Obj*)((ObjClosure*)object)->upvalues[i]);
            break;

        case OBJ_DYNVAR:
            markValue(((ObjDynvar*)object)->varName);
            markValue(((ObjDynvar*)object)->previous);
            break;

        case OBJ_FUNCTION:
            markValue(((ObjFunction*)object)->name);
            markObject((Obj*)((ObjFunction*)object)->klass);
            markArray(&((ObjFunction*)object)->chunk.constants);
            break;

        case OBJ_INSTANCE:
            markObject((Obj*)((ObjInstance*)object)->klass);
            markTable(&((ObjInstance*)object)->fields);
            break;

        case OBJ_ITERATOR:
            markObject((Obj*)((ObjIterator*)object)->instance);
            break;

        case OBJ_LIST:
            markArray(&((ObjList*)object)->arr);
            break;

        case OBJ_UPVALUE:
            markValue(*((ObjUpvalue*)object)->location);
            break;
    }
}

static void freeObject(Obj* object) {

#ifdef LOX_DBG
    if (vm.debug_log_gc & DBG_GC_FREE)
        printf("GC %05x free %s\n", (int32_t)object, typeName(object->type));
#endif

    switch (object->type) {
        case OBJ_BOUND:
            FREE(ObjBound, object);
            break;

        case OBJ_CLASS:
            freeTable(&((ObjClass*)object)->methods);
            FREE(ObjClass, object);
            break;

        case OBJ_CLOSURE:
            reallocate(object, sizeof(ObjClosure)  +
                               sizeof(ObjUpvalue*) * ((ObjClosure*)object)->upvalueCount, 0);  
            break;

        case OBJ_DYNVAR:
            FREE(ObjDynvar, object);
            break;

        case OBJ_FUNCTION:
            freeChunk(&((ObjFunction*)object)->chunk);
            FREE(ObjFunction, object);
            break;

        case OBJ_INSTANCE: 
            freeTable(&((ObjInstance*)object)->fields);
            FREE(ObjInstance, object);
            break;

        case OBJ_ITERATOR:
            FREE(ObjIterator, object);
            break;

        case OBJ_LIST:
            freeValueArray(&((ObjList*)object)->arr);
            FREE(ObjList, object);
            break;

        case OBJ_NATIVE:
            FREE(ObjNative, object);
            break;

        case OBJ_REAL:
            FREE(ObjReal, object);
            break;

        case OBJ_STRING:
            reallocate(object, sizeof(ObjString) + ((ObjString*)object)->length + 1, 0);  
            break;

        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object);
            break;
    }
}

static void markRoots(void) {
    Value* slot;
    int i;
    ObjUpvalue* upvalue;
    CallFrame*  frame = vm.frames;

    for (slot = vm.stack; slot < vm.sp; slot++)
        markValue(*slot);

    for (i = 0; i < vm.frameCount; i++, frame++) {
        markObject((Obj*)frame->closure);
        markValue(frame->handler);
    }

    for (upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->uv.next)
        markObject((Obj*)upvalue);

    markTable(&vm.globals);
    markCompilerRoots();
    markObject((Obj*)vm.initString);
}

static void traceReferences(void) {
    while (vm.grayCount) {
        Obj* object = vm.grayStack[--vm.grayCount];
        blackenObject(object);
    }
}

static void sweep(void) {
    Obj* previous = NULL;
    Obj* object = vm.objects;
    Obj* unreached;

    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
            previous = object;
            object = object->nextObj;
        } else {
            unreached = object;
            object = object->nextObj;
            if (previous != NULL) 
                previous->nextObj = object;
            else
                vm.objects = object;
            freeObject(unreached);
        }
    }
}

void collectGarbage(bool checkReclaim) {
    size_t before = vm.bytesAllocated;

#ifdef LOX_DBG
    if (vm.debug_log_gc & DBG_GC_GENERAL)
        putstr("GC >>> begin\n");
#endif

    markRoots();
    traceReferences();
    tableRemoveWhite(&vm.strings); // making vm.strings a weak hash-table
    sweep();

    if (checkReclaim && before == vm.bytesAllocated) {
        putstr("GC failed to reclaim enough space, exiting.\n");
        exit(1);
    }

#ifdef LOX_DBG
    if (vm.debug_log_gc & DBG_GC_GENERAL) {
        putstr("GC <<< ended\n");
        printf("GC collected %d bytes (from %d to %d)\n",
               before - vm.bytesAllocated, before, vm.bytesAllocated);
    }
#endif

#ifdef LOX_DBG
    if (!(vm.debug_log_gc & DBG_GC_STRESS)) // Avoid endless recursion
        tableShrink(&vm.strings);
    vm.numGCs++;
#else
    tableShrink(&vm.strings);
#endif

}

#ifndef KIT68K
void freeObjects(void) {
    Obj* object = vm.objects;
    Obj* next;

    while (object != NULL) {
        next = object->nextObj;
        freeObject(object);
        object = next;
    }
}
#endif