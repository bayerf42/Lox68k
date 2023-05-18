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

void printValue(Value value, bool compact, bool machine) {
    if      (IS_BOOL(value))   printf(AS_BOOL(value) ? "true" : "false");
    else if (IS_NIL(value))    printf("nil");
    else if (IS_EMPTY(value))  printf("<empty>");
    else if (IS_INT(value))    printf("%d", AS_INT(value));
    else                       printObject(value, compact, machine);
}

const char* valueType(Value value) {
    if      (IS_BOOL(value))   return "bool";
    else if (IS_NIL(value))    return "nil";
    else if (IS_EMPTY(value))  return "empty"; // internal
    else if (IS_INT(value))    return "int";
    else                       return typeName(OBJ_TYPE(value));
}

bool valuesEqual(Value a, Value b) {
    if (IS_REAL(a) && IS_REAL(b))
        return AS_REAL(a) == AS_REAL(b);
    else 
        return a == b;
}

uint32_t hashValue(Value value) {
    if      (IS_STRING(value)) return AS_STRING(value)->hash;
    else if (IS_REAL(value))   return hashBytes((uint8_t*)&AS_REAL(value), sizeof(Real));
    else                       return value;
}

uint32_t hashBytes(const uint8_t* bytes, int length) {
    // Bernstein hash (djb2)
    uint32_t hash = 5381;

    while (length--)
        hash = ((hash << 5) + hash) ^ *bytes++;
    return hash;
}


