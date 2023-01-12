#include <stdlib.h>
#include <stdio.h>

#include "compiler.h"
#include "memory.h"
#include "vm.h"
#include "debug.h"
#include "nano_malloc.h"


#define GC_HEAP_GROW_FACTOR 2

char input_line[INPUT_SIZE];


void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    void* result;

    if (vm.debug_stress_gc) {
        if (newSize > oldSize) {
            collectGarbage(false);
        }
    }

    vm.bytesAllocated += newSize - oldSize;

    if (newSize == 0) {
        if (pointer != 0)
            nano_free(pointer);
        return NULL;
    }

    result = nano_malloc(newSize);

    if (result == NULL) {
        if (vm.debug_log_gc) {
            printf("-- malloc failed, now trying gc.\n");
        }

        collectGarbage(true);

        result = nano_malloc(newSize);
        if (result == NULL) {
            printf("Out of heap space, exiting.\n");
            exit(1);
        }
    }
    vm.totallyAllocated += newSize;

    if (oldSize != 0) {
    	fix_memcpy(result, pointer, (oldSize < newSize) ? oldSize : newSize);
	nano_free(pointer);
    }
    return result;
}


void markObject(Obj* object) {
    if (object == NULL) return;
    if (object->isMarked) return;

    if (vm.debug_log_gc) {
        printf("%lx mark ", (void*) object);
        printValue(OBJ_VAL(object), true, true);
        printf("\n");
    }

    object->isMarked = true;

    switch (object->type) {
        case OBJ_STRING:
        case OBJ_REAL:
        case OBJ_NATIVE:
            // Leaf object, nothing to do
            break;

        default:
            if (vm.grayCount + 1 > GRAY_MAX) {
                printf("Gray stack sixe exceeded, exiting.\n");
                exit(1);
            }
            vm.grayStack[vm.grayCount++] = object;
    }
}


void markValue(Value value) {
    if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

static void markArray(ValueArray* array) {
    int i;
    for (i = 0; i < array->count; i++) {
        markValue(array->values[i]);
    }
}


static void blackenObject(Obj* object) {
    int16_t i;

    if (vm.debug_log_gc) {
        printf("%lx blacken ", (void*)object);
        printValue(OBJ_VAL(object), true, true);
        printf("\n");
    }

    switch (object->type) {
        case OBJ_BOUND_METHOD:
            markValue(((ObjBoundMethod*)object)->receiver);
            markObject((Obj*)((ObjBoundMethod*)object)->method);
            break;

        case OBJ_CLASS:
            markObject((Obj*)((ObjClass*)object)->name);
            markTable(&((ObjClass*)object)->methods);
            break;

        case OBJ_CLOSURE:
            markObject((Obj*)((ObjClosure*)object)->function);
            for (i = 0; i < ((ObjClosure*)object)->upvalueCount; i++)
                markObject((Obj*)((ObjClosure*)object)->upvalues[i]);
            break;

        case OBJ_FUNCTION:
            markObject((Obj*)((ObjFunction*)object)->name);
            markArray(&((ObjFunction*)object)->chunk.constants);
            break;

        case OBJ_INSTANCE:
            markObject((Obj*)((ObjInstance*)object)->klass);
            markTable(&((ObjInstance*)object)->fields);
            break;

        case OBJ_LIST:
            for (i = 0; i < ((ObjList*)object)->count; i++)
                markValue(((ObjList*)object)->items[i]);
            break;

        case OBJ_UPVALUE:
            markValue(((ObjUpvalue*)object)->closed);
            break;

        case OBJ_ITERATOR:
            markTable(((ObjIterator*)object)->table);
            break;
    }
}

static void freeObject(Obj* object) {

    if (vm.debug_log_gc) {
        printf("%lx free type %s\n", (void*)object, typeName(object->type));
    }

    switch (object->type) {
        case OBJ_BOUND_METHOD:
            FREE(ObjBoundMethod, object);
            break;

        case OBJ_CLASS:
            freeTable(&((ObjClass*)object)->methods);
            FREE(ObjClass, object);
            break;

        case OBJ_CLOSURE:
            reallocate(object, sizeof(ObjClosure) + sizeof(ObjUpvalue*) *
                                                    ((ObjClosure*)object)->upvalueCount, 0);  
            break;

        case OBJ_FUNCTION:
            freeChunk(&((ObjFunction*)object)->chunk);
            FREE(ObjFunction, object);
            break;

        case OBJ_INSTANCE: 
            freeTable(&((ObjInstance*)object)->fields);
            FREE(ObjInstance, object);
            break;

        case OBJ_LIST: 
            FREE_ARRAY(Value, ((ObjList*)object)->items, ((ObjList*)object)->capacity);
            FREE(ObjList, object);
            break;

        case OBJ_NATIVE:
            FREE(ObjNative, object);
            break;

        case OBJ_STRING:
            reallocate(object, sizeof(ObjString) + ((ObjString*)object)->length + 1, 0);  
            break;

        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object);
            break;

        case OBJ_REAL:
            FREE(ObjReal, object);
            break;

        case OBJ_ITERATOR:
            FREE(ObjIterator, object);
            break;
    }
}

static void markRoots(void) {
    Value* slot;
    int i;
    ObjUpvalue* upvalue;

    for (slot = vm.stack; slot < vm.stackTop; slot++)
        markValue(*slot);

    for (i = 0; i < vm.frameCount; i++)
        markObject((Obj*)vm.frames[i].closure);

    for (upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->nextUpvalue)
        markObject((Obj*)upvalue);

    markTable(&vm.globals);
    markCompilerRoots();
    markObject((Obj*)vm.initString);
}

static void traceReferences(void) {
    while (vm.grayCount > 0) {
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

    if (vm.debug_log_gc) {
        printf("-- gc begin\n");
    }

    markRoots();
    traceReferences();
    tableRemoveWhite(&vm.strings);
    sweep();

    if (checkReclaim && before == vm.bytesAllocated) {
        printf("GC failed to reclaim enough space, exiting.\n");
        exit(1);
    }

    if (vm.debug_log_gc) {
        printf("-- gc end\n");
        printf("   collected %d bytes (from %d to %d)\n",
               before - vm.bytesAllocated, before, vm.bytesAllocated);
    }

    if (!vm.debug_stress_gc) // Avoid endless recursion
        tableShrink(&vm.strings);
    vm.numGCs++;
}


void freeObjects(void) {
    Obj* object = vm.objects;
    Obj* next;

    while (object != NULL) {
        next = object->nextObj;
        freeObject(object);
        object = next;
    }
}

void fix_memcpy(char* dest, const char* src, size_t size) {
    // Library memcpy has an alignment bug,
    // leading to address error exception...

    while (size--)
        *dest++ = *src++;
}

int fix_memcmp(const char* a, const char* b, size_t size) {
    int delta;

    while (size--) {
        delta = *b++ - *a++;
        if (delta) return delta;
    }
    return 0;
}