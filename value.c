#include <stdio.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "value.h"

void initValueArray(ValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void writeValueArray(ValueArray* array, Value value) {
    int oldCapacity;
    if (array->capacity < array->count + 1) {
        oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }
    array->values[array->count++] = value;
}

void freeValueArray(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

void printValue(Value value, bool compact) {
    if (IS_BOOL(value))        printf(AS_BOOL(value) ? "true" : "false");
    else if (IS_NIL(value))    printf("nil");
    else if (IS_EMPTY(value))  printf("<empty>");
    else if (IS_NUMBER(value)) printf("%ld", AS_NUMBER(value));
    else if (IS_OBJ(value))    printObject(value, compact);
}

ObjString* valueType(Value value) {
    const char* type;

    if (IS_BOOL(value))        type = "bool";
    else if (IS_NIL(value))    type = "nil";
    else if (IS_EMPTY(value))  type = "empty";
    else if (IS_NUMBER(value)) type = "number";
    else if (IS_OBJ(value))    type = typeName(OBJ_TYPE(value));

    return copyString(type, strlen(type));
}

bool valuesEqual(Value a, Value b) {
    if (IS_REAL(a) && IS_REAL(b))
        return AS_REAL(a) == AS_REAL(b);
    return a == b;
}

uint32_t hashValue(Value value) {
    if (IS_STRING(value)) return AS_STRING(value)->hash;
    else if (IS_REAL(value)) return *((uint32_t*)&AS_REAL(value));
    else return value;
}
