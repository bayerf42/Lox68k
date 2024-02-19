#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "native.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

char        buffer[130];     // general purpose: debugging, name building, error messages
static char cvBuffer[32];    // for number conversion

bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) {
    Obj* object      = (Obj*)reallocate(NULL, 0, size);
    object->type     = type;
    object->isMarked = false;
    object->nextObj  = vm.objects;
    vm.objects       = object;

    if (vm.debug_log_gc & DBG_GC_ALLOC)
        printf("GC %05x aloc %d %s\n", (int32_t)object, size, typeName(type));

    return object;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Object creation 
////////////////////////////////////////////////////////////////////////////////////////////////////
ObjBound* makeBound(Value receiver, ObjClosure* method) {
    ObjBound* bound = ALLOCATE_OBJ(ObjBound, OBJ_BOUND);
    bound->receiver = receiver;
    bound->method   = method;
    return bound;
}

ObjClass* makeClass(ObjString* name) {
    ObjClass* klass   = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
    klass->name       = name;
    klass->superClass = NIL_VAL;
    initTable(&klass->methods);
    return klass;
}

ObjClosure* makeClosure(ObjFunction* function) {
    ObjClosure* closure = (ObjClosure*)
        allocateObject(sizeof(ObjClosure) + sizeof(ObjUpvalue*) * function->upvalueCount,
                       OBJ_CLOSURE);
    int i;
    for (i = 0; i < function->upvalueCount; i++)
        closure->upvalues[i] = NULL;
    closure->function     = function;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjFunction* makeFunction() {
    ObjFunction* function  = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity        = 0;
    function->upvalueCount = 0;
    function->name         = NIL_VAL;
    function->klass        = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjInstance* makeInstance(ObjClass* klass) {
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    instance->klass       = klass;
    initTable(&instance->fields);
    return instance;
}

ObjIterator* makeIterator(Table* table, ObjInstance* instance) {
    ObjIterator* iter = ALLOCATE_OBJ(ObjIterator, OBJ_ITERATOR);
    iter->table    = table;
    iter->instance = instance;
    iter->position = firstIterator(table);
    return iter;
}

ObjList* makeList(int len, Value* items, int numCopy, int stride) {
    // Create new list of length len. Init it with numCopy values taken from items (rest NIL)
    // stepping items by stride (1 = array copy, 0 = init from unique value, -1 = list reverse)
    ObjList* list = ALLOCATE_OBJ(ObjList, OBJ_LIST);
    int16_t  i, newCap;

    initValueArray(&list->arr);
    push(OBJ_VAL(list));
    if (len > 0) {
        newCap             = MIN_CAPACITY(len); // avoid fragmentation with many small lists
        list->arr.values   = GROW_ARRAY(Value, list->arr.values, 0, newCap);
        list->arr.count    = len;
        list->arr.capacity = newCap;
        for (i = 0; i < len; i++) {
            list->arr.values[i] = (--numCopy >= 0) ? *items : NIL_VAL;
            items += stride;
        }
    }
    drop();
    return list;
}

ObjNative* makeNative(const Native* native) {
    ObjNative* res = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    res->native = native;
    return res;
}

Value makeReal(Real val) {
    ObjReal* real = ALLOCATE_OBJ(ObjReal, OBJ_REAL);
    real->content = val;
    return OBJ_VAL(real);
}

ObjString* makeString(const char* chars, int length) {
    // Check if we already have an equal string
    uint32_t hash = hashBytes((const uint8_t*)chars, length);
    ObjString* string;
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL)
        return interned;

    // Create new string
    string = (ObjString*)allocateObject(sizeof(ObjString) + length + 1, OBJ_STRING);
    string->length = length;
    string->hash   = hash;
    fix_memcpy(string->chars, chars, length);
    string->chars[length] = '\0';

    // and save it in table
    push(OBJ_VAL(string));
    tableSet(&vm.strings, OBJ_VAL(string), NIL_VAL);
    drop();
    return string;
}

ObjUpvalue* makeUpvalue(Value* slot) {
    ObjUpvalue* upvalue  = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->closed      = NIL_VAL;
    upvalue->location    = slot;
    upvalue->nextUpvalue = NULL;
    return upvalue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Object printing 
////////////////////////////////////////////////////////////////////////////////////////////////////

const char* functionName(ObjFunction* function) {
    if (IS_NIL(function->name))
        return "#script";
    else if (IS_INT(function->name))
        sprintf(buffer, "#%d", AS_INT(function->name));
    else if (function->klass == NULL)
        return AS_CSTRING(function->name);
    else
        // Limit output length to avoid buffer overflow 
        sprintf(buffer, "%.64s.%.64s", function->klass->name->chars, AS_CSTRING(function->name));
    return buffer;
}

static void printList(ObjList* list, bool machine) {
    int i;
    const char* sep = "";

    CHECK_STACKOVERFLOW

    putstr("[");
    for (i = 0; i < list->arr.count; i++) {
        putstr(sep);
        printValue(list->arr.values[i], false, machine);
        sep = ", ";
    }
    putstr("]");
}

const char* typeName(ObjType type) {
    switch (type) {
        case OBJ_BOUND:    return "bound";
        case OBJ_CLASS:    return "class";
        case OBJ_CLOSURE:  return "closure";
        case OBJ_FUNCTION: return "fun";      // internal
        case OBJ_INSTANCE: return "instance";
        case OBJ_ITERATOR: return "iterator";
        case OBJ_LIST:     return "list";
        case OBJ_NATIVE:   return "native";
        case OBJ_REAL:     return "real";
        case OBJ_STRING:   return "string";
        case OBJ_UPVALUE:  return "upvalue";  // internal
        default:           return "unknown";  // shouldn't happen
    }
}

void printObject(Value value, bool compact, bool machine) {
    switch (OBJ_TYPE(value)) {
        case OBJ_BOUND:
            printf("<bound %s>", functionName(AS_BOUND(value)->method->function));
            break;

        case OBJ_CLASS:
            printf("<class %s>", AS_CLASS(value)->name->chars);
            break;

        case OBJ_CLOSURE:
            printf("<closure %s>", functionName(AS_CLOSURE(value)->function));
            break;

        case OBJ_FUNCTION:
            printf("<fun %s>", functionName(AS_FUNCTION(value)));
            break;

        case OBJ_INSTANCE:
            printf("<%s instance>", AS_INSTANCE(value)->klass->name->chars);
            break;

        case OBJ_ITERATOR:
            printf("<iterator %d>", AS_ITERATOR(value)->position);
            break;

        case OBJ_LIST:
            if (compact)
                printf("<list %d>", AS_LIST(value)->arr.count);
            else
                printList(AS_LIST(value), machine);
            break;

        case OBJ_NATIVE:
            printf("<native %s>", AS_NATIVE(value)->native->name);
            break;

        case OBJ_REAL:
            putstr(formatReal(AS_REAL(value)));
            break;

        case OBJ_STRING:
            if (machine) putstr("\"");
            putstr(AS_CSTRING(value));
            if (machine) putstr("\"");
            break;

        case OBJ_UPVALUE:
            putstr("<upvalue>");
            break;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Lists 
////////////////////////////////////////////////////////////////////////////////////////////////////

static void limitIndex(int* place, int n) {
    if (*place < 0) *place += n;
    if (*place < 0) *place  = 0;
    if (*place > n) *place  = n;
}

void insertIntoList(ObjList* list, Value value, int index) {
    int oldCapacity, i;
    int n = list->arr.count;

    if (list->arr.capacity < list->arr.count + 1) {
        oldCapacity        = list->arr.capacity;
        list->arr.capacity = GROW_CAPACITY(oldCapacity);
        list->arr.values   = GROW_ARRAY(Value, list->arr.values, oldCapacity, list->arr.capacity);
    }
    limitIndex(&index, n);
    for (i = n; i >= index; i--)
        list->arr.values[i + 1] = list->arr.values[i];
    list->arr.values[index] = value;
    ++list->arr.count;
}

void storeToList(ObjList* list, int index, Value value) {
    if (index < 0)
        index += list->arr.count;
    list->arr.values[index] = value;
}

Value indexFromList(ObjList* list, int index) {
    if (index < 0)
        index += list->arr.count;
    return list->arr.values[index];
}


ObjList* sliceFromList(ObjList* list, int begin, int end) {
    int n = list->arr.count;
    limitIndex(&begin, n);
    limitIndex(&end,   n);
    return makeList(end - begin, &list->arr.values[begin], end - begin, 1);
}

void deleteFromList(ObjList* list, int index) {
    int i;

    if (index < 0)
        index += list->arr.count;
    for (i = index; i < list->arr.count - 1; i++)
        list->arr.values[i] = list->arr.values[i + 1];
    list->arr.values[--list->arr.count] = NIL_VAL;
}

bool isValidListIndex(ObjList* list, int index) {
    return (index >= 0 && index <   list->arr.count) ||
           (index <  0 && index >= -list->arr.count);
}

ObjList* concatLists(ObjList* a, ObjList* b) {
    ObjList* result = makeList(a->arr.count + b->arr.count, a->arr.values, a->arr.count, 1);
    int16_t  i, dest;

    for (i = 0; i < b->arr.count; i++) {
        // expanding dest into next lines generates wrong code in IDE68k
        dest = a->arr.count + i;
        result->arr.values[dest] = b->arr.values[i];
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Strings 
////////////////////////////////////////////////////////////////////////////////////////////////////

ObjString* indexFromString(ObjString* string, int index) {
    if (index < 0)
        index += string->length;
    return makeString(string->chars + index, 1);
}

bool isValidStringIndex(ObjString* string, int index) {
    return (index >= 0 && index <   string->length) ||
           (index <  0 && index >= -string->length);
}

ObjString* sliceFromString(ObjString* string, int begin, int end) {
    int n = string->length;
    limitIndex(&begin, n);
    limitIndex(&end,   n);
    return makeString(string->chars + begin, (end > begin) ? end - begin : 0);
}

ObjString* concatStrings(ObjString* a, ObjString* b) {
    if (a->length + b->length >= INPUT_SIZE)
        return NULL;
    fix_memcpy(big_buffer, a->chars, a->length);
    fix_memcpy(big_buffer + a->length, b->chars, b->length);
    return makeString(big_buffer, a->length + b->length);
}

ObjString* caseString(ObjString* a, bool toUpper) {
    int   length = a->length;
    char* cp;
    fix_memcpy(big_buffer, a->chars, length);
    big_buffer[length] = '\0';

    if (toUpper)
        for (cp=big_buffer; *cp; ++cp)
            *cp = toupper(*cp);
    else
        for (cp=big_buffer; *cp; ++cp)
            *cp = tolower(*cp);

    return makeString(big_buffer, length);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Reals 
////////////////////////////////////////////////////////////////////////////////////////////////////
const char* formatReal(Real val) {
#ifdef KIT68K
    int   expo, dp, off;
    char* estr;
    char* dest;

    if (val==0)
        return "0.0";

    realToStr(cvBuffer, val);
    expo = atoi(cvBuffer + 11);
    estr = cvBuffer + 10;

    // Fixed format returned from realToStr:
    // s.mmmmmmmmEsdd
    //           1111
    // 01234567890123

    if (expo >= 1 && expo <= 7) {
        // shift DP to the right
        for (dp = 1; expo > 0; dp++, expo--) {
            cvBuffer[dp] = cvBuffer[dp + 1];
            cvBuffer[dp + 1] = '.';
        }
        cvBuffer[10] = '\0'; // cut off exponent
        estr = NULL;
    } else if (expo <= 0 && expo >= -3) {
        // shift mantissa to the right
        for (off = 9; off > 1; off--)
            cvBuffer[off - expo + 1] = cvBuffer[off];
        cvBuffer[11 - expo] = '\0'; // cut off exponent
        estr = NULL;
        for (off = 2 - expo; off > 0; off--)
            cvBuffer[off] = '0';
        cvBuffer[2] = '.';
    } else {
        // exponential display, but 1 digit left to DP
        cvBuffer[1] = cvBuffer[2];
        cvBuffer[2] = '.';
        sprintf(cvBuffer + 11, "%d", expo - 1);
    }

    // Squeeze out trailing zeros in mantissa
    for (dest = estr ? estr : cvBuffer + strlen(cvBuffer);
         dest[-1] == '0' && dest[-2] != '.';)
        *--dest = '\0';
    // Move exponent over squeezed zeros
    if (estr)
        while ((*dest++ = *estr++) != 0)
            ;

    if (cvBuffer[0] == '+')
        return cvBuffer + 1;

#else
    sprintf(cvBuffer, "%.15g", val);
    // Make sure it doesn't look like an int
    if (!strpbrk(cvBuffer, ".eE"))
        strcat(cvBuffer, ".0");
#endif

    return cvBuffer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Other conversions 
////////////////////////////////////////////////////////////////////////////////////////////////////

const char* formatInt(Int val) {
#ifndef linux
    itoa(val, cvBuffer, 10);
#else
    sprintf(cvBuffer, "%d", val);
#endif
    return cvBuffer;
}

const char* formatHex(Int val) {
#ifndef linux
    itoa(val, cvBuffer, 16);
#else
    sprintf(cvBuffer, "%x", val);
#endif
    return cvBuffer;
}

const char* formatBin(Int val) {
    uint32_t mask = 0x80000000;
    char*    outp = cvBuffer;

    for (; mask; mask >>= 1)
        *outp++ = val & mask ? '1' : '0';
    *outp = 0;

    // find first non-zero
    for (outp = cvBuffer; *outp == '0'; outp++)
        ;
    return val == 0 ? outp - 1 : outp;
}

Value parseInt(const char* start, bool checkLen) {
    char* end = NULL;
    Int   number;

    if (*start == '%')
        number = strtol(++start, &end, 2);
    else if (*start == '$')
        number = strtol(++start, &end, 16);
    else
        number = strtol(start, &end, 10);

    if (start == end || (checkLen && start + strlen(start) != end))
        return NIL_VAL;
    else
        return INT_VAL(number);
}

