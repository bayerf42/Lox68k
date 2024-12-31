#ifndef clox_object_h
#define clox_object_h

#include "chunk.h"
#include "table.h"
#include "value.h"
#include "native.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Object accessors 
////////////////////////////////////////////////////////////////////////////////////////////////////
#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_BOUND(value)        isObjType(value, OBJ_BOUND)
#define IS_CLASS(value)        isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_DYNVAR(value)       isObjType(value, OBJ_DYNVAR)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)
#define IS_ITERATOR(value)     isObjType(value, OBJ_ITERATOR)
#define IS_LIST(value)         isObjType(value, OBJ_LIST)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_REAL(value)         isObjType(value, OBJ_REAL)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)

#define AS_BOUND(value)        ((ObjBound*)AS_OBJ(value))
#define AS_CLASS(value)        ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_DYNVAR(value)       ((ObjDynvar*)AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))
#define AS_ITERATOR(value)     ((ObjIterator*)AS_OBJ(value))
#define AS_LIST(value)         ((ObjList*)AS_OBJ(value))
#define AS_NATIVE(value)       (((ObjNative*)AS_OBJ(value))->native)
#define AS_REAL(value)         (((ObjReal*)AS_OBJ(value))->content)
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

////////////////////////////////////////////////////////////////////////////////////////////////////
// Object types 
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef enum {     //  callable  leaf object (keep enum order for type tests)
    OBJ_DYNVAR,    //
    OBJ_FUNCTION,  //
    OBJ_INSTANCE,  //
    OBJ_ITERATOR,  // 
    OBJ_LIST,      //
    OBJ_UPVALUE,   //
    OBJ_BOUND,     //  X       
    OBJ_CLASS,     //  X
    OBJ_CLOSURE,   //  X
    OBJ_NATIVE,    //  X         X
    OBJ_REAL,      //            X
    OBJ_STRING,    //            X
} ObjType;

// The IDE68K C compiler doesn't seem to like including struct Obj in the following structures
// and generates wrong code when casting, so we expand struct Obj manually.

#define OBJ_HEADER         \
    Obj*         nextObj;  \
    uint8_t      type;     \
    uint8_t      isMarked;

struct Obj {
    OBJ_HEADER
};

struct ObjBound {
    OBJ_HEADER
    Value        receiver;
    ObjClosure*  method;
};

struct ObjClass {
    OBJ_HEADER
    ObjString*   name;
    ObjClass*    superClass;   // nil when no superclass
    Table        methods;
};

struct ObjClosure {
    OBJ_HEADER
    int16_t      upvalueCount; // too big, but keep alignment
    ObjFunction* function;
    ObjUpvalue*  upvalues[];   // array embedded in structure
};

struct ObjDynvar {
    OBJ_HEADER
    Value        current;
    Value        previous;     // can also be an ObjDynvar building a stack
};

struct ObjFunction {
    OBJ_HEADER
    uint8_t      arity;        // lower 7 bits arity, highest bit rest parameter flag        
    uint8_t      upvalueCount;
    Chunk        chunk;
    Value        name;         // string for named functions, int for anonymous, nil for script
    ObjClass*    klass;        // defining class for method, nil for normal function
};

struct ObjInstance {
    OBJ_HEADER
    ObjClass*    klass;
    Table        fields;
};

struct ObjIterator {
    OBJ_HEADER
    int16_t      position;     // -1: before first; -2: after last; else valid position
    ObjInstance* instance;
};

struct ObjList {
    OBJ_HEADER
    ValueArray   arr;
};

struct ObjNative {
    OBJ_HEADER
    const Native* native;
};

struct ObjReal {
    OBJ_HEADER
    Real         content;
};

struct ObjString {
    OBJ_HEADER
    int16_t      length;
    uint32_t     hash;
    char         chars[];      // single alloc for strings, as embedded char array
};

struct ObjUpvalue {
    OBJ_HEADER
    Value*       location;
    Value        closed;
    ObjUpvalue*  nextUpvalue;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Object functions 
////////////////////////////////////////////////////////////////////////////////////////////////////

ObjBound*    makeBound(Value receiver, ObjClosure* method);
ObjClass*    makeClass(ObjString* name);
ObjClosure*  makeClosure(ObjFunction* function);
ObjDynvar*   makeDynvar(Value current, Value previous);
ObjFunction* makeFunction(void);
ObjInstance* makeInstance(ObjClass* klass);
ObjIterator* makeIterator(ObjInstance* instance);
ObjList*     makeList(int len, Value* items, int numCopy, int stride);
ObjNative*   makeNative(const Native* native);
Value        makeReal(Real val);
ObjString*   makeString0(const char* chars);
ObjString*   makeString(const char* chars, int length);
ObjUpvalue*  makeUpvalue(Value* slot);

void         printObject(Value value, int flags);
const char*  typeName(ObjType type);
bool         isObjType(Value value, ObjType type);
bool         isCallable(Value value);

void         insertIntoList(ObjList* list, Value value, int index);
void         storeToList(ObjList* list, int index, Value value);
Value        indexFromList(ObjList* list, int index);
ObjList*     sliceFromList(ObjList* list, int begin, int end);
void         deleteFromList(ObjList* list, int index);
bool         isValidListIndex(ObjList* list, int index);
ObjList*     concatLists(ObjList* a, ObjList* b);

ObjString*   indexFromString(ObjString* string, int index);
ObjString*   sliceFromString(ObjString* string, int begin, int end);
bool         isValidStringIndex(ObjString* string, int index);
ObjString*   concatStrings(ObjString* a, ObjString* b);
ObjString*   caseString(ObjString* a, bool toUpper);

const char*  formatReal(Real val);
const char*  formatInt(Int val);
const char*  formatHex(Int val);
const char*  formatBin(Int val);
const char*  functionName(ObjFunction* function);
Value        parseInt(const char* start, bool checkLen);
void         putstrn(int len, const char* str);

extern char  buffer[];

#endif