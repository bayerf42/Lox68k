#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);

    object->type = type;
    object->isMarked = false;

    object->nextObj = vm.objects;
    vm.objects = object;

    if (vm.debug_log_gc) {
        printf("%lx allocate %d for %s\n", (void*)object, size, typeName(type));
    }

    return object;
}

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method) {
    ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);

    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

ObjClass* newClass(ObjString* name) {
    ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);

    klass->name = name;
    initTable(&klass->methods);
    return klass;
}

ObjClosure* newClosure(ObjFunction* function) {
    ObjClosure* closure = (ObjClosure*)
        allocateObject(sizeof(ObjClosure) + sizeof(ObjUpvalue*) * function->upvalueCount,
                       OBJ_CLOSURE);
    int i;

    for (i = 0; i < function->upvalueCount; i++) {
        closure->upvalues[i] = NULL;
    }
    closure->function = function;
    closure->upvalueCount = function->upvalueCount;

    return closure;
}

ObjFunction* newFunction() {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);

    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjInstance* newInstance(ObjClass* klass) {
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);

    instance->klass = klass;
    initTable(&instance->fields);
    return instance;
}


ObjNative* newNative(const char* signature, NativeFn function) {
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);

    strncpy(native->signature, signature, 8);
    native->function = function;
    return native;
}


static uint32_t hashString(const char* chars, int length) {
    // FNV-1 hash with 32 bits
    uint32_t hash = 2166136261u;
    int i;

    for (i = 0; i < length; i++) {
        hash ^= (uint8_t)chars[i];
        hash *= 16777619;
    }
    return hash;
}


ObjString* copyString(const char* chars, int length) {
    // Check if we already have an equal string
    uint32_t hash = hashString(chars, length);
    ObjString* string;
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) return interned;

    // Create new string
    string = (ObjString*)allocateObject(sizeof(ObjString) + length + 1, OBJ_STRING);
    string->length = length;
    string->hash = hash;
    fix_memcpy(string->chars, chars, length);
    string->chars[length] = '\0';

    // and save it in table
    push(OBJ_VAL(string));
    tableSet(&vm.strings, OBJ_VAL(string), NIL_VAL);
    drop();
    return string;
}

ObjUpvalue* newUpvalue(Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);

    upvalue->closed = NIL_VAL;
    upvalue->location = slot;
    upvalue->nextUpvalue = NULL;
    return upvalue;
}

static void printFunction(const char* subType, ObjFunction* function) {
    if (function->name == NULL) {
        printf("<script>");
        return;
    }
    printf("<%s %s>", subType, function->name->chars);
}

static void printList(ObjList* list) {
    int i;

    CHECK_STACKOVERFLOW

    printf("[");
    for (i = 0; i < list->count - 1; i++) {
        printValue(list->items[i], false);
        printf(", ");
    }
    if (list->count != 0) {
        printValue(list->items[list->count - 1], false);
    }
    printf("]");
}

const char* typeName(ObjType type) {
    switch(type) {
        case OBJ_BOUND_METHOD: return "bound_method";
        case OBJ_CLASS:        return "class";
        case OBJ_CLOSURE:      return "closure";
        case OBJ_FUNCTION:     return "function";;
        case OBJ_INSTANCE:     return "instance";
        case OBJ_LIST:         return "list";
        case OBJ_NATIVE:       return "native";
        case OBJ_STRING:       return "string";
        case OBJ_UPVALUE:      return "upvalue";
        default:               return "unknown";
    }
}


static void fix_printf(const char* str) {
    // printf("%s", str) has a bug in IDE68k library, inserting arbitrary spaces for strings
    // longer than 127 characters, so do it manually (again)
    while (*str)
        putchar(*str++);
}


void printObject(Value value, bool compact) {
    switch (OBJ_TYPE(value)) {
        case OBJ_BOUND_METHOD: printFunction("bound", AS_BOUND_METHOD(value)->method->function); break;
        case OBJ_CLASS: printf("<class %s>", AS_CLASS(value)->name->chars); break;
        case OBJ_CLOSURE: printFunction("clos", AS_CLOSURE(value)->function); break;
        case OBJ_FUNCTION: printFunction("fun", AS_FUNCTION(value)); break;
        case OBJ_INSTANCE: printf("<%s instance>", AS_INSTANCE(value)->klass->name->chars); break;
        case OBJ_LIST: if (compact) printf("<list %d>",AS_LIST(value)->count); else printList(AS_LIST(value)); break;
        case OBJ_NATIVE: printf("<native>"); break;
        case OBJ_STRING: fix_printf(AS_CSTRING(value)); break;
        case OBJ_UPVALUE: printf("<upvalue>"); break;
    }
}

ObjList* newList() {
    ObjList* list = ALLOCATE_OBJ(ObjList, OBJ_LIST);

    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

void appendToList(ObjList* list, Value value) {
    int oldCapacity;

    if (list->capacity < list->count + 1) {
        oldCapacity = list->capacity;
        list->capacity = GROW_CAPACITY(oldCapacity);
        list->items = GROW_ARRAY(Value, list->items, oldCapacity, list->capacity);
    }
    list->items[list->count++] = value;
}

void insertIntoList(ObjList* list, Value value, int index) {
    int oldCapacity, i;
    int count = list->count;

    if (list->capacity < list->count + 1) {
        oldCapacity = list->capacity;
        list->capacity = GROW_CAPACITY(oldCapacity);
        list->items = GROW_ARRAY(Value, list->items, oldCapacity, list->capacity);
    }
    if (index < 0) index = (index < -count) ? 0 : index + count;
    if (index > count) index = count;
    for (i = count; i >= index; i--) {
        list->items[i + 1] = list->items[i];
    } 
    list->items[index] = value;
    ++list->count;
}

void storeToList(ObjList* list, int index, Value value) {
    if (index < 0) index += list->count;
    list->items[index] = value;
}

Value indexFromList(ObjList* list, int index) {
    if (index < 0) index += list->count;
    return list->items[index];
}

ObjList* sliceFromList(ObjList* list, int begin, int end) {
    ObjList* result = newList();
    int n = list->count;
    int i;

    if (begin < 0)  begin += n;
    if (begin < 0)  begin = 0; 
    if (begin >= n) begin = n;
    if (end < 0)    end += n;
    if (end <= 0)   end = 0; 
    if (end > n)    end = n;

    push(OBJ_VAL(result));
    for (i = begin; i < end; i++) {
        appendToList(result, list->items[i]);
    }
    drop();
    return result;
}

void deleteFromList(ObjList* list, int index) {
    int i;

    if (index < 0) index += list->count;
    for (i = index; i < list->count - 1; i++) {
        list->items[i] = list->items[i + 1];
    }
    list->items[--list->count] = NIL_VAL;
}

bool isValidListIndex(ObjList* list, int index) {
    return (index >= 0) && (index < list->count) ||
           (index < 0) && (index >= -list->count);
}

ObjString* indexFromString(ObjString* string, int index) {
    if (index < 0) index += string->length;
    return copyString(string->chars + index, 1);
}

bool isValidStringIndex(ObjString* string, int index) {
    return (index >= 0) && (index < string->length) ||
           (index < 0) && (index >= -string->length);
}

ObjString* sliceFromString(ObjString* string, int begin, int end) {
    int n = string->length;

    if (begin < 0)  begin += n;
    if (begin < 0)  begin = 0; 
    if (begin >= n) begin = n;
    if (end < 0)    end += n;
    if (end <= 0)   end = 0; 
    if (end > n)    end = n;

    return copyString(string->chars + begin, (end > begin) ? end - begin : 0);
}

ObjString* concatStrings(ObjString* a, ObjString* b) {
    int length = a->length + b->length;

    fix_memcpy(input_line, a->chars, a->length);
    fix_memcpy(input_line + a->length, b->chars, b->length);
    input_line[length] = '\0';

    return copyString(input_line, length);
}

ObjString* caseString(ObjString* a, bool toUpper) {
    int length = a->length;
    char* cp;
    fix_memcpy(input_line, a->chars, length);
    input_line[length] = '\0';

#ifdef __linux__
    if (toUpper)
        for (cp=input_line; *cp; ++cp)
            *cp = toupper(*cp);  
    else  
        for (cp=input_line; *cp; ++cp)
            *cp = tolower(*cp);  
#else
    if (toUpper)
        strupr(input_line);
    else  
        strlwr(input_line); 
#endif

    return copyString(input_line, length);
}


ObjList* concatLists(ObjList* a, ObjList* b) {
    ObjList* result = newList();
    int i;

    push(OBJ_VAL(result));
    for (i = 0; i < a->count; i++)
        appendToList(result, a->items[i]);
    for (i = 0; i < b->count; i++)
        appendToList(result, b->items[i]);
    drop();
    return result;
}

ObjList* allKeys(Table* table) {
    ObjList* result = newList();
    int i;
    Entry* entry;

    push(OBJ_VAL(result));
    for (i = 0; i < table->capacity; i++) {
        entry = &table->entries[i];
        if (!IS_EMPTY(entry->key)) {
            appendToList(result, OBJ_VAL(entry->key));
        }
    }
    drop();
    return result;
}

