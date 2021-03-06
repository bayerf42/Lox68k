#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

// 68008 Value tagging scheme
// Numbers          sxxx xxxx ... xxxx xxx1 (shift 1 bit for arithmetic)
// Specials         1000 0000 ... 0000 0xx0 (nil, false, true; see below)
// Object pointers  0xxx xxxx ... xxxx xxx0 (even address, < 2GB)

typedef uint32_t Value;
typedef int32_t Number;

#define IS_BOOL(value)   (((value)|2) == TRUE_VAL)
#define IS_NIL(value)    ((value) == NIL_VAL)
#define IS_NUMBER(value) (((value)&1) == 1)
#define IS_OBJ(value)    (((value)&0x80000001) == 0)

#define AS_BOOL(value)   ((value) == TRUE_VAL)
#define AS_NUMBER(value) (((Number)(value))>>1)
#define AS_OBJ(value)    ((Obj*)(value))

#define BOOL_VAL(b)      ((b) ? TRUE_VAL : FALSE_VAL)
#define NIL_VAL          0x80000002
#define FALSE_VAL        0x80000004
#define TRUE_VAL         0x80000006
#define NUMBER_VAL(num)  ((Value)(((num)<<1)|1))
#define OBJ_VAL(obj)     ((Value)(obj))

typedef struct {
    int16_t capacity;
    int16_t count;
    Value*  values;
} ValueArray;

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value, bool compact);
ObjString* valueType(Value value);

#endif