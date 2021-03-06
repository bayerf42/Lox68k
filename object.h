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

typedef enum {
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_LIST,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE
} ObjType;

struct Obj {
    struct Obj* nextObj;
    uint8_t     type;
    uint8_t     isMarked;
};

// The IDE68K C compiler doesn't seem to like including struct Obj in the following structures
// and generates wrong code when casting, so we expand struct Obj manually.

typedef struct {
    struct Obj* nextObj;
    uint8_t     type;
    uint8_t     isMarked;

    uint8_t     arity;
    uint8_t     upvalueCount;
    Chunk       chunk;
    ObjString*  name;
} ObjFunction;

typedef bool (*NativeFn)(int argCount, Value* args);
typedef char Signature[8];

typedef struct {
    struct Obj* nextObj;
    uint8_t     type;
    uint8_t     isMarked;

    Signature   signature;
    NativeFn    function;
} ObjNative;

struct ObjString {
    struct Obj* nextObj;
    uint8_t     type;
    uint8_t     isMarked;

    int16_t     length;
    char*       chars;
    uint32_t    hash;
};

typedef struct ObjUpvalue {
    struct Obj*        nextObj;
    uint8_t            type;
    uint8_t            isMarked;

    Value*             location;
    Value              closed;
    struct ObjUpvalue* nextUpvalue;
} ObjUpvalue;

typedef struct {
    struct Obj*  nextObj;
    uint8_t      type;
    uint8_t      isMarked;

    ObjFunction* function;
    ObjUpvalue** upvalues;
    uint8_t      upvalueCount;
    int8_t       padding;
} ObjClosure;

typedef struct {
    struct Obj* nextObj;
    uint8_t     type;
    uint8_t     isMarked;

    ObjString*  name;
    Table       methods;
} ObjClass;

typedef struct {
    struct Obj* nextObj;
    uint8_t     type;
    uint8_t     isMarked;

    ObjClass*   klass;
    Table       fields;
} ObjInstance;

typedef struct {
    struct Obj* nextObj;
    uint8_t     type;
    uint8_t     isMarked;

    Value       receiver;
    ObjClosure* method;
} ObjBoundMethod;

typedef struct {
    struct Obj* nextObj;
    uint8_t     type;
    uint8_t     isMarked;

    int16_t     count;
    int16_t     capacity;
    Value*      items;
} ObjList;

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method);
ObjClass* newClass(ObjString* name);
ObjClosure* newClosure(ObjFunction* function);
ObjFunction* newFunction(void);
ObjInstance* newInstance(ObjClass* klass);
ObjList* newList(void);
ObjNative* newNative(const char* signature, NativeFn function);
ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
ObjUpvalue* newUpvalue(Value* slot);
void printObject(Value value, bool compact);
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

ObjList* allKeys(Table* table);

bool isObjType(Value value, ObjType type);


#endif