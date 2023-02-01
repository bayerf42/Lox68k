#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value) isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value) isObjType(value, OBJ_INSTANCE)
#define IS_LIST(value) isObjType(value, OBJ_LIST)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_REAL(value) isObjType(value, OBJ_REAL)
#define IS_ITERATOR(value) isObjType(value, OBJ_ITERATOR)

#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLASS(value) ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value) ((ObjInstance*)AS_OBJ(value))
#define AS_LIST(value) ((ObjList*)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value))->function)
#define AS_NATIVE_SIG(value) (((ObjNative*)AS_OBJ(value))->signature)
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)
#define AS_REAL(value) (((ObjReal*)AS_OBJ(value))->content)
#define AS_ITERATOR(value) ((ObjIterator*)AS_OBJ(value))

typedef enum {
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_ITERATOR,
    OBJ_LIST,
    OBJ_NATIVE,
    OBJ_REAL,
    OBJ_STRING,
    OBJ_UPVALUE
} ObjType;


#define OBJ_HEADER        \
    struct Obj* nextObj;  \
    uint8_t     type;     \
    uint8_t     isMarked;

struct Obj {
    OBJ_HEADER
};

// The IDE68K C compiler doesn't seem to like including struct Obj in the following structures
// and generates wrong code when casting, so we expand struct Obj manually.

typedef struct {
    OBJ_HEADER

    uint8_t     arity;
    uint8_t     upvalueCount;
    uint8_t     isVarArg;
    uint8_t     _padding;
    Chunk       chunk;
    ObjString*  name;
} ObjFunction;

typedef bool (*NativeFn)(int argCount, Value* args);
typedef char Signature[8];

typedef struct {
    OBJ_HEADER

    Signature    signature;
    NativeFn     function;
} ObjNative;

struct ObjString {
    OBJ_HEADER

    int16_t      length;
    uint32_t     hash;
    char         chars[]; // chap 19, single alloc for strings, as embedded char array
};

struct ObjUpvalue {
    OBJ_HEADER

    Value*       location;
    Value        closed;
    ObjUpvalue*  nextUpvalue;
};

typedef struct {
    OBJ_HEADER

    ObjFunction* function;
    int16_t      upvalueCount; // too big, but keep alignment
    ObjUpvalue*  upvalues[];   // like ObjString, array embedded in structure
} ObjClosure;

typedef struct {
    OBJ_HEADER

    ObjString*   name;
    Table        methods;
} ObjClass;

typedef struct {
    OBJ_HEADER

    ObjClass*    klass;
    Table        fields;
} ObjInstance;

typedef struct {
    OBJ_HEADER

    Value        receiver;
    ObjClosure*  method;
} ObjBoundMethod;

typedef struct {
    OBJ_HEADER

    int16_t      count;
    int16_t      capacity;
    Value*       items;
} ObjList;

typedef struct {
    OBJ_HEADER

    Real         content;
} ObjReal;

struct ObjIterator {
    OBJ_HEADER

    ObjInstance* instance;  // for GC
    Table*       table;
    int16_t      position;  // -1 means invalid
};


ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method);
ObjClass* newClass(ObjString* name);
ObjClosure* newClosure(ObjFunction* function);
ObjFunction* newFunction(void);
ObjInstance* newInstance(ObjClass* klass);
ObjList* newList(void);
Value newReal(Real val);
ObjNative* newNative(const char* signature, NativeFn function);
ObjIterator* newIterator(Table* table, ObjInstance* instance);
ObjUpvalue* newUpvalue(Value* slot);
ObjString* copyString(const char* chars, int length);

void printObject(Value value, bool compact, bool machine);
const char* typeName(ObjType type);

void appendToList(ObjList* list, Value value);
void insertIntoList(ObjList* list, Value value, int index);
void storeToList(ObjList* list, int index, Value value);
Value indexFromList(ObjList* list, int index);
ObjList* sliceFromList(ObjList* list, int begin, int end);
void deleteFromList(ObjList* list, int index);
bool isValidListIndex(ObjList* list, int index);

ObjString* indexFromString(ObjString* string, int index);
ObjString* sliceFromString(ObjString* string, int begin, int end);
bool isValidStringIndex(ObjString* string, int index);

ObjString* concatStrings(ObjString* a, ObjString* b);
ObjList* concatLists(ObjList* a, ObjList* b);

ObjString* caseString(ObjString* a, bool toUpper);

bool isObjType(Value value, ObjType type);

const char* formatReal(Real val);
const char* formatInt(Int val);
const char* formatHex(Int val);
const char* formatBin(Int val);

extern char numBuffer[];

#endif