#ifndef clox_value_h
#define clox_value_h

#include "machine.h"

typedef int32_t Value;
typedef int32_t Int;

// Stupid C with separate namespaces for structs and types
typedef struct Obj         Obj;
typedef struct ObjBound    ObjBound;
typedef struct ObjClass    ObjClass;
typedef struct ObjClosure  ObjClosure;
typedef struct ObjDynvar   ObjDynvar;
typedef struct ObjFunction ObjFunction;
typedef struct ObjInstance ObjInstance;
typedef struct ObjIterator ObjIterator;
typedef struct ObjList     ObjList;
typedef struct ObjNative   ObjNative;
typedef struct ObjReal     ObjReal;
typedef struct ObjString   ObjString;
typedef struct ObjUpvalue  ObjUpvalue;

// 68008 Value tagging scheme
// Integers         sxxx xxxx ... xxxx xxx1 (shift 1 bit for arithmetic)
// Object pointers  xxxx xxxx ... xxxx xxx0 (even address > 6)
// Specials         0000 0000 ... 0000 0xx0 (special Obj*, empty, nil, false, true; see below)

#define NIL_VAL          0x00000000
#define FALSE_VAL        0x00000002
#define EMPTY_VAL        0x00000004
#define TRUE_VAL         0x00000006

#define IS_BOOL(value)   (((value) | 4) == TRUE_VAL)
#define IS_NIL(value)    ((value) == NIL_VAL)
#define IS_EMPTY(value)  ((value) == EMPTY_VAL)
#define IS_INT(value)    ((value) & 1)
#define IS_OBJ(value)    (((value) & 1) == 0 && ((unsigned)(value) > TRUE_VAL))
#define IS_FALSEY(value) (((value) | 2) == FALSE_VAL)

#define AS_BOOL(value)   ((value) == TRUE_VAL)
#define AS_INT(value)    (((Int)(value))>>1)
#define AS_OBJ(value)    ((Obj*)(value))

#define BOOL_VAL(b)      ((b) ? TRUE_VAL : FALSE_VAL)
#define INT_VAL(num)     ((Value)(((num)<<1) | 1))
#define OBJ_VAL(obj)     ((Value)(obj))

// Fast version, use native equal to compare reals and iterators
#define valuesEqual(a,b) ((a) == (b))

// Printing flags
#define PRTF_HUMAN   0x00
#define PRTF_MACHINE 0x01
#define PRTF_EXPAND  0x00
#define PRTF_COMPACT 0x02

typedef struct {
    int16_t count;
    int16_t capacity;
    Value*  values;
} ValueArray;

void        initValueArray(ValueArray* array);
void        appendValueArray(ValueArray* array, Value value);
void        freeValueArray(ValueArray* array);
void        freezeValueArray(ValueArray* array);
void        printValue(Value value, int flags);
const char* valueType(Value value);
uint32_t    hashValue(Value value);

#endif