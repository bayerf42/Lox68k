#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

#include "object.h"
#include "native.h"
#include "memory.h"
#include "vm.h"

bool    onKit;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Typechecking natives
////////////////////////////////////////////////////////////////////////////////////////////////////

static const char* matchesType(Value value, int type) {
    switch (type) {
        case 'A': return                                      NULL;  // any value
        case 'B': return IS_BOOL(value)                     ? NULL : "a bool";
        case 'I': return IS_INSTANCE(value)                 ? NULL : "an instance";
        case 'K': return IS_CLASS(value)                    ? NULL : "a class";
        case 'L': return IS_LIST(value)                     ? NULL : "a list";
        case 'N': return IS_INT(value)                      ? NULL : "an int";
        case 'Q': return IS_STRING(value) || IS_LIST(value) ? NULL : "a sequence";
        case 'R': return IS_INT(value) || IS_REAL(value)    ? NULL : "a number";
        case 'S': return IS_STRING(value)                   ? NULL : "a string";
        case 'T': return IS_ITERATOR(value)                 ? NULL : "an iterator";
        default:  return                                             "an unknown type";
    }
}

bool checkNativeSignature(const Native* native, int argCount, Value* args) {
    int maxParmCount      = 0;
    int minParmCount      = 0;
    int i;
    const char* signature = native->signature;
    const char* currParm  = signature;
    const char* expected;

    // Trailing lower-case letters in signature indicate optional arguments.
    while (*currParm) {
        ++maxParmCount;
        if (!(*currParm & LOWER_CASE_MASK))
            ++minParmCount;
        ++currParm;
    }

    if (minParmCount > argCount || argCount > maxParmCount) {
        if (minParmCount == maxParmCount)
            runtimeError("'%s' expected %d arguments but got %d.",
                         native->name, maxParmCount, argCount);
        else
            runtimeError("'%s' expected %d to %d arguments but got %d.",
                         native->name, minParmCount, maxParmCount, argCount);
        return false;
    }

    for (i = 0; i < argCount; i++) {
        expected = matchesType(args[i], signature[i] & ~LOWER_CASE_MASK);
        if (expected != NULL) {
            runtimeError("'%s' type mismatch at argument %d, expected %s but got %s.",
                         native->name, i + 1, expected, valueType(args[i]));
            return false;
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Arithmetics
////////////////////////////////////////////////////////////////////////////////////////////////////

#define RESULT args[-1]

#define CHECK_ARITH_ERROR(op) \
    if (errno != 0) {                               \
        runtimeError("'%s' arithmetic error.", op); \
        return false;                               \
    }

#define TRANS_NATIVE(fun) return transcendentalNative(argCount, args, fun, #fun);

static bool absNative(int argCount, Value* args) {
    if (IS_INT(args[0]))
        RESULT = INT_VAL(abs(AS_INT(args[0])));
    else
        RESULT = makeReal(fabs(AS_REAL(args[0])));
    return true;
}

static bool truncNative(int argCount, Value* args) {
    if (IS_INT(args[0]))
        RESULT = args[0];
    else
        RESULT = INT_VAL(realToInt(AS_REAL(args[0])));
    CHECK_ARITH_ERROR("trunc")
    return true;
}

typedef Real (*RealFun)(Real);

static bool transcendentalNative(int argCount, Value* args, RealFun fn, const char* op) {
    if (IS_INT(args[0]))
        RESULT = makeReal((*fn)(intToReal(AS_INT(args[0]))));
    else
        RESULT = makeReal((*fn)(AS_REAL(args[0])));
    CHECK_ARITH_ERROR(op)
    return true;
}

static bool sqrtNative(int argCount, Value* args) {
    TRANS_NATIVE(sqrt)
}

static bool sinNative(int argCount, Value* args) {
    TRANS_NATIVE(sin)
}

static bool cosNative(int argCount, Value* args) {
    TRANS_NATIVE(cos)
}

static bool tanNative(int argCount, Value* args) {
    TRANS_NATIVE(tan)
}

static bool sinhNative(int argCount, Value* args) {
    TRANS_NATIVE(sinh)
}

static bool coshNative(int argCount, Value* args) {
    TRANS_NATIVE(cosh)
}

static bool tanhNative(int argCount, Value* args) {
    TRANS_NATIVE(tanh)
}

static bool expNative(int argCount, Value* args) {
    TRANS_NATIVE(exp)
}

static bool logNative(int argCount, Value* args) {
    TRANS_NATIVE(log)
}

static bool atanNative(int argCount, Value* args) {
    TRANS_NATIVE(atan)
}

static bool powNative(int argCount, Value* args) {
    Real x = (IS_INT(args[0])) ? intToReal(AS_INT(args[0])) : AS_REAL(args[0]);
    Real y = (IS_INT(args[1])) ? intToReal(AS_INT(args[1])) : AS_REAL(args[1]);
    RESULT = makeReal(pow(x,y));
    CHECK_ARITH_ERROR("pow")
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Lists and strings
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool lengthNative(int argCount, Value* args) {
    if (IS_STRING(args[0]))
        RESULT = INT_VAL(AS_STRING(args[0])->length);
    else
        RESULT = INT_VAL(AS_LIST(args[0])->arr.count);
    return true;
}

static bool listNative(int argCount, Value* args) {
    Value    item = (argCount == 2) ? args[1] : NIL_VAL;
    Int      len  = AS_INT(args[0]);
    ObjList* list = makeList(len, &item, len, 0);
    RESULT = OBJ_VAL(list);
    return true;
}

static bool reverseNative(int argCount, Value* args) {
    ObjList* src = AS_LIST(args[0]);
    ObjList* res = makeList(src->arr.count, src->arr.values + src->arr.count - 1,
                            src->arr.count, -1);
    RESULT = OBJ_VAL(res);
    return true;
}

static bool appendNative(int argCount, Value* args) {
    ObjList* list = AS_LIST(args[0]);
    appendValueArray(&list->arr, args[1]);
    RESULT = NIL_VAL;
    return true;
}

static bool insertNative(int argCount, Value* args) {
    ObjList* list = AS_LIST(args[0]);
    insertIntoList(list, args[2], AS_INT(args[1]));
    RESULT = NIL_VAL;
    return true;
}

static bool deleteNative(int argCount, Value* args) {
    ObjList* list  = AS_LIST(args[0]);
    int      index = AS_INT(args[1]);
    if (!isValidListIndex(list, index)) {
        runtimeError("'%s' %s out of range.", "delete", "index");
        return false;
    }
    deleteFromList(list, index);
    RESULT = NIL_VAL;
    return true;
}

static bool indexNative(int argCount, Value* args) {
    ObjList*   list;
    ObjString* haystack;
    ObjString* needle;
    int        start = (argCount == 2) ? 0 : AS_INT(args[2]);
    int        i, delta;

    RESULT = NIL_VAL;
    if (IS_LIST(args[1])) {
        list  = AS_LIST(args[1]);
        if (list->arr.count == 0 && start == 0)
            return true;
        if (!isValidListIndex(list, start)) {
            runtimeError("'%s' %s out of range.", "index", "start index");
            return false;
        }
        if (start < 0) start += list->arr.count;

        for (i = start; i < list->arr.count; i++) {
            if (valuesEqual(args[0], list->arr.values[i])) {
                RESULT = INT_VAL(i);
                break;
            }
        }
    } else {
        haystack = AS_STRING(args[1]);
        if (!IS_STRING(args[0])) {
            runtimeError("'%s' type mismatch at argument %d, expected %s but got %s.",
                         "index", 1, "a string", valueType(args[0]));
            return false;
        }
        needle = AS_STRING(args[0]);
        if (haystack->length == 0 && start == 0) {
            if (needle->length == 0)
                RESULT = INT_VAL(0);
            return true;
        }
        if (!isValidStringIndex(haystack, start)) {
            runtimeError("'%s' %s out of range.", "index", "start index");
            return false;
        }
        if (start < 0) start += haystack->length;

        // strstr is broken in IDE68K C library, wrote our own...
        delta = haystack->length - needle->length;
        for (i = start; i <= delta; i++) {
            if (!fix_memcmp(haystack->chars + i, needle->chars, needle->length)) {
                RESULT = INT_VAL(i);
                break;
            }
        } 
    }
    return true;
}

static bool removeNative(int argCount, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[0]);
    bool         removed  = tableDelete(&instance->fields, args[1]);
    RESULT = BOOL_VAL(removed);
    return true;
}

static bool equalNative(int argCount, Value* args) {
    Value       a   = args[0];
    Value       b   = args[1];
    bool        res = false;
    ObjIterator *ait, *bit;

    if (IS_REAL(a)) {
        if (IS_INT(b))
            res = AS_REAL(a) == intToReal(AS_INT(b));
        else if (IS_REAL(b))
            res = AS_REAL(a) == AS_REAL(b);
    } else if (IS_REAL(b) && IS_INT(a)) { 
        res = AS_REAL(b) == intToReal(AS_INT(a));  
    } else if (IS_ITERATOR(a) && IS_ITERATOR(b)) {
        ait = AS_ITERATOR(a);  
        bit = AS_ITERATOR(b);  
        res = ait->table == bit->table && ait->position == bit->position;
    } else
        res  = a == b;
    RESULT = BOOL_VAL(res);
    return true;
}

static bool lowerNative(int argCount, Value* args) {
    RESULT = OBJ_VAL(caseString(AS_STRING(args[0]), false));
    return true;
}

static bool upperNative(int argCount, Value* args) {
    RESULT = OBJ_VAL(caseString(AS_STRING(args[0]), true));
    return true;
}

#define APPEND(str) { \
    len = strlen(str); if (dest - big_buffer + len >= (INPUT_SIZE-1)) goto ErrExit; \
    strcpy(dest, (str)); \
    dest += len; }

static bool joinNative(int argCount, Value* args) {
    ObjList*    list   = AS_LIST(args[0]);
    const char* sepa   = (argCount > 1) ? AS_CSTRING(args[1]) : "";
    const char* first  = (argCount > 2) ? AS_CSTRING(args[2]) : "";
    const char* last   = (argCount > 3) ? AS_CSTRING(args[3]) : "";

    const char* joiner = "";
    const char* curr;
    char*       dest   = big_buffer;
    Value       item;
    int         i, len;

    APPEND(first);
    for (i = 0; i < list->arr.count; i++) {
        APPEND(joiner);
        joiner = sepa;
        item   = list->arr.values[i];
        if (!IS_STRING(item)) {
            runtimeError("'%s' string expected in list at %d.", "join", i);
            return false;
        }
        curr = AS_CSTRING(item);
        APPEND(curr);
    }
    APPEND(last);
    RESULT = OBJ_VAL(makeString(big_buffer, dest - big_buffer));
    return true;

ErrExit:
    runtimeError("'%s' stringbuffer overflow.", "join");
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// Iterators
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool globalsNative(int argCount, Value* args) {
    RESULT = OBJ_VAL(makeIterator(&vm.globals, NULL));
    return true;
}

static bool slotsNative(int argCount, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[0]);
    RESULT = OBJ_VAL(makeIterator(&instance->fields, instance));
    return true;
}

static bool validNative(int argCount, Value* args) {
    ObjIterator* iter = AS_ITERATOR(args[0]);
    RESULT = BOOL_VAL(isValidIterator(iter));
    return true;
}

static bool nextNative(int argCount, Value* args) {
    ObjIterator* iter = AS_ITERATOR(args[0]);
    nextIterator(iter);
    RESULT = BOOL_VAL(isValidIterator(iter));
    return true;
}

static bool itCloneNative(int argCount, Value* args) {
    ObjIterator* src  = AS_ITERATOR(args[0]);
    ObjIterator* dest = makeIterator(src->table, src->instance);
    dest->position = src->position;
    RESULT = OBJ_VAL(dest);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Some datatype conversions
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool ascNative(int argCount, Value* args) {
    ObjString* string = AS_STRING(args[0]);
    Int        code;
    int        index = (argCount == 1) ? 0 : AS_INT(args[1]);

    if (!isValidStringIndex(string, index)) {
        runtimeError("'%s' %s out of range.", "asc", "index");
        return false;
    }
    if (index < 0)
        index += string->length;
    code = string->chars[index] & 0xff;
    RESULT = INT_VAL(code);
    return true;
}

static bool chrNative(int argCount, Value* args) {
    Int  code = AS_INT(args[0]);
    char codepoint;

    if (code < 0 || code > 255) {
        runtimeError("'%s' %s out of range.", "chr", "code");
        return false;
    }
    codepoint = (char)code;
    RESULT = OBJ_VAL(makeString(&codepoint, 1));
    return true;
}

static bool decNative(int argCount, Value* args) {
    const char* res;
    if (IS_INT(args[0]))
        res = formatInt(AS_INT(args[0]));
    else
        res = formatReal(AS_REAL(args[0]));
    RESULT = OBJ_VAL(makeString(res, strlen(res)));
    return true;
}

static bool hexNative(int argCount, Value* args) {
    const char* res = formatHex(AS_INT(args[0]));
    RESULT = OBJ_VAL(makeString(res, strlen(res)));
    return true;
}

static bool binNative(int argCount, Value* args) {
    const char* res = formatBin(AS_INT(args[0]));
    RESULT = OBJ_VAL(makeString(res, strlen(res)));
    return true;
}

static bool parseIntNative(int argCount, Value* args) {
    RESULT = parseInt(AS_CSTRING(args[0]), true);
    return true;
}

static bool parseRealNative(int argCount, Value* args) {
    char* start = AS_CSTRING(args[0]);
    char* end   = start;
    Real  real;

    errno = 0;
    real  = strToReal(start, &end);
    if (errno == 0 && start + strlen(start) == end)
        RESULT = makeReal(real);
    else
        RESULT = NIL_VAL;
    return true;
}

static bool inputNative(int argCount, Value* args) {
    if (argCount > 0)
        putstr(AS_CSTRING(args[0]));
    if (readLine())
        RESULT = OBJ_VAL(makeString(big_buffer, strlen(big_buffer)));
    else
        RESULT = NIL_VAL;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Integers
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool bitAndNative(int argCount, Value* args) {
    // No shifting and masking needed :-)
    RESULT = args[0] & args[1];
    return true;
}

static bool bitOrNative(int argCount, Value* args) {
    RESULT = args[0] | args[1];
    return true;
}

static bool bitXorNative(int argCount, Value* args) {
    RESULT = (args[0] ^ args[1]) | 1;
    return true;
}

static bool bitNotNative(int argCount, Value* args) {
    RESULT = (~args[0]) | 1;
    return true;
}

static bool bitShiftNative(int argCount, Value* args) {
    Int value = AS_INT(args[0]);
    Int amount = AS_INT(args[1]);
    if (amount > 0)
        RESULT = INT_VAL(value << amount);
    else
        RESULT = INT_VAL(value >> (-amount));
    return true;
}

uint32_t rand32;

static bool randomNative(int argCount, Value* args) {
    rand32  ^= rand32 << 13;
    rand32  ^= rand32 >> 17;
    rand32  ^= rand32 << 5;
    RESULT = INT_VAL(rand32 & 0x3fffffff);
    return true;
}

static bool seedRandNative(int argCount, Value* args) {
    RESULT = INT_VAL(rand32);
    rand32   = AS_INT(args[0]);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Modifying debugging options
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool dbgCodeNative(int argCount, Value* args) {
    vm.debug_print_code = AS_BOOL(args[0]);
    RESULT = args[0];
    return true;
}

static bool dbgStepNative(int argCount, Value* args) {
    vm.debug_trace_steps = AS_BOOL(args[0]);
    RESULT = args[0];
    return true;
}

static bool dbgCallNative(int argCount, Value* args) {
    vm.debug_trace_calls = AS_BOOL(args[0]);
    RESULT = args[0];
    return true;
}

static bool dbgNatNative(int argCount, Value* args) {
    vm.debug_trace_natives = AS_BOOL(args[0]);
    RESULT = args[0];
    return true;
}

static bool dbgGcNative(int argCount, Value* args) {
    vm.debug_log_gc = AS_INT(args[0]);
    RESULT = args[0];
    return true;
}

static bool dbgStatNative(int argCount, Value* args) {
    vm.debug_statistics = AS_BOOL(args[0]);
    RESULT = args[0];
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Machine code support
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool peekNative(int argCount, Value* args) {
    int32_t address = AS_INT(args[0]);
    Int     byte    = *((uint8_t*)address);
    RESULT = INT_VAL(byte);
    return true;
}

static bool pokeNative(int argCount, Value* args) {
    int32_t address = AS_INT(args[0]);
    Int     byte    = AS_INT(args[1]);
    if (byte < 0 || byte > 255) {
        runtimeError("'%s' %s out of range.", "poke", "byte");
        return false;
    }
    *((uint8_t*)address) = byte;
    RESULT = NIL_VAL;
    return true;
}

static bool execNative(int argCount, Value* args) {
    typedef Value sub0(void);
    typedef Value sub1(Value);
    typedef Value sub2(Value,Value);
    typedef Value sub3(Value,Value,Value);

    int32_t address = AS_INT(args[0]);
    Value   res     = NIL_VAL;

    switch (argCount) {
        case 1: res = ((sub0*) address)(); break;
        case 2: res = ((sub1*) address)(args[1]); break;
        case 3: res = ((sub2*) address)(args[1],args[2]); break;
        case 4: res = ((sub3*) address)(args[1],args[2],args[3]); break;
    }

    RESULT = res;
    return true;
}

static bool addrNative(int argCount, Value* args) {
    if (IS_OBJ(args[0]))
        RESULT = INT_VAL((uint32_t)AS_OBJ(args[0]));
    else
        RESULT = NIL_VAL;
    return true;
}

static bool heapNative(int argCount, Value* args) {
    int32_t address = AS_INT(args[0]);
    RESULT = *((Value*)address);
    return true;
}

#ifdef KIT68K

////////////////////////////////////////////////////////////////////////////////////////////////////
// 68008 Kit specific routines
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "monitor4x.h"

static bool lcdClearNative(int argCount, Value* args) {
    lcd_clear();
    RESULT = NIL_VAL;
    return true;
}

static bool lcdGotoNative(int argCount, Value* args) {
    lcd_goto(AS_INT(args[0]), AS_INT(args[1]));
    RESULT = NIL_VAL;
    return true;
}

static bool lcdPutsNative(int argCount, Value* args) {
    lcd_puts(AS_CSTRING(args[0]));
    RESULT = NIL_VAL;
    return true;
}

static bool lcdDefcharNative(int argCount, Value* args) {
    int      udc        = AS_INT(args[0]);
    ObjList* pattern    = AS_LIST(args[1]);
    char     bitmap[8];
    Int      byte;
    int      i;

    if (udc < 0 || udc > 7) {
        runtimeError("'%s' %s out of range.", "lcd_defchar", "UDC number");
        return false;
    }
    if (pattern->arr.count != 8) {
        runtimeError("'%s' UDC bitmap must be 8 bytes.", "lcd_defchar");
        return false;
    }
    for (i = 0; i < 8; i++) {
        if (IS_INT(pattern->arr.values[i])) {
            byte = AS_INT(pattern->arr.values[i]);
            if (byte < 0 || byte > 255) {
                runtimeError("'%s' %s out of range.", "lcd_defchar", "byte");
                return false;
            }
            bitmap[i] = (char)byte;
        } else {
            runtimeError("'%s' integer expected in UDC bitmap at %d.", "lcd_defchar", i);
            return false;
        }
    }

    lcd_defchar(udc, bitmap);
    RESULT = NIL_VAL;
    return true;
}

static bool keycodeNative(int argCount, Value* args) {
    int code = monitor_scan();
    if (code == 255)
        RESULT = NIL_VAL;
    else
        RESULT = INT_VAL(code);
    return true;
}

#define LOOPS_NOTE  97400
#define DELAY_EXTRA     2

static bool soundNative(int argCount, Value* args) {
    int delay = AS_INT(args[0]);
    int len   = AS_INT(args[1]);
    int loops = LOOPS_NOTE;
    int i,j;

    loops *= len;
    loops /= 1000;
    loops /= (delay + DELAY_EXTRA);

    *port2 = 0;
    for (i = 0; i < loops; i++) {
        *port1 &= ~0x40;
        for (j = 0; j < delay; j++);
        *port1 |= 0x40;
        for (j = 0; j < delay; j++);
    }

    RESULT = NIL_VAL;
    return true;
}

static bool trapNative(int argCount, Value* args) {
    _trap(0);
    RESULT = NIL_VAL;
    return true;
}

#define LOOPS_PER_MILLI 147

static bool sleepNative(int argCount, Value* args) {
    int32_t millis = AS_INT(args[0]);
    int32_t i, j;

    for (i = 0; i < millis; i++)
        for (j = 0; j < LOOPS_PER_MILLI; j++)
            ;

    RESULT = NIL_VAL;
    return true;
}

#else

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <unistd.h>
#endif

static bool sleepNative(int argCount, Value* args) {
    int32_t millis = AS_INT(args[0]);

#ifdef _WIN32
    Sleep(millis);
#else
    usleep(millis*1000);
#endif

    RESULT = NIL_VAL;
    return true;
}

#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// System
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool gcNative(int argCount, Value* args) {
    collectGarbage(false);
    RESULT = INT_VAL(vm.bytesAllocated);
    return true;
}

static bool typeNative(int argCount, Value* args) {
    const char* type = valueType(args[0]);
    RESULT = OBJ_VAL(makeString(type, strlen(type)));
    return true;
}

static bool nameNative(int argCount, Value* args) {
    Obj*        object;
    const char* name = NULL;

    RESULT = NIL_VAL;
    if (IS_OBJ(args[0])) {
        object = AS_OBJ(args[0]);
        switch (object->type) {
            case OBJ_BOUND:
                name = functionName(((ObjBound*)object)->method->function);
                break;

            case OBJ_CLASS:
                name = ((ObjClass*)object)->name->chars;
                break;

            case OBJ_CLOSURE:
                name = functionName(((ObjClosure*)object)->function);
                break;

            case OBJ_FUNCTION: // should never happen in fact
                name = functionName((ObjFunction*)object);
                break;

            case OBJ_NATIVE:
                name = ((ObjNative*)object)->native->name;
                break;
        }
        if (name)
            RESULT = OBJ_VAL(makeString(name, strlen(name)));
    }
    return true;
}

static bool parentNative(int argCount, Value* args) {
    RESULT = AS_CLASS(args[0])->superClass;
    return true;
}

static bool classOfNative(int argCount, Value* args) {
    if (IS_INSTANCE(args[0]))
        RESULT = OBJ_VAL(AS_INSTANCE(args[0])->klass);
    else if (IS_BOUND(args[0]))
        RESULT = OBJ_VAL(AS_BOUND(args[0])->method->function->klass);
    else
        RESULT = NIL_VAL;
    return true;
}

static bool errorNative(int argCount, Value* args) {
    userError(args[0]);
    return false;
}

static bool clockNative(int argCount, Value* args) {
#ifdef KIT68K
    RESULT = INT_VAL(clock() * 10);   // CLOCKS_PER_SEC == 100
#else
#ifdef _WIN32
    RESULT = INT_VAL(clock());        // CLOCKS_PER_SEC == 1000
#else
    RESULT = INT_VAL(clock() / 1000); // CLOCKS_PER_SEC == 1000000
#endif
#endif
    return true;
}

char* readLine() {
#ifdef KIT68K
    return gets(big_buffer);
#else
    char* res = fgets(big_buffer, sizeof(big_buffer), stdin);
    if (res)
        big_buffer[strcspn(big_buffer, "\n")] = 0;
    return res;      
#endif
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup everyting
////////////////////////////////////////////////////////////////////////////////////////////////////

static const Native allNatives[] = {
    {"abs",         "R",    absNative},
    {"trunc",       "R",    truncNative},
    {"sqrt",        "R",    sqrtNative},
    {"sin",         "R",    sinNative},
    {"cos",         "R",    cosNative},
    {"tan",         "R",    tanNative},
    {"sinh",        "R",    sinhNative},
    {"cosh",        "R",    coshNative},
    {"tanh",        "R",    tanhNative},
    {"exp",         "R",    expNative},
    {"log",         "R",    logNative},
    {"atan",        "R",    atanNative},
    {"pow",         "RR",   powNative},

    {"length",      "Q",    lengthNative},
    {"list",        "Na",   listNative},
    {"reverse",     "L",    reverseNative},
    {"append",      "LA",   appendNative},
    {"insert",      "LNA",  insertNative},
    {"delete",      "LN",   deleteNative},
    {"index",       "AQn",  indexNative},
    {"remove",      "IA",   removeNative},
    {"equal",       "AA",   equalNative},

    {"lower",       "S",    lowerNative},
    {"upper",       "S",    upperNative},
    {"join",        "Lsss", joinNative},

    {"globals",     "",     globalsNative},
    {"slots",       "I",    slotsNative},
    {"valid",       "T",    validNative},
    {"next",        "T",    nextNative},
    {"it_clone",    "T",    itCloneNative},

    {"asc",         "Sn",   ascNative},
    {"chr",         "N",    chrNative},
    {"dec",         "R",    decNative},
    {"hex",         "N",    hexNative},
    {"bin",         "N",    binNative},
    {"parse_int",   "S",    parseIntNative},
    {"parse_real",  "S",    parseRealNative},
    {"input",       "s",    inputNative},

    {"bit_and",     "NN",   bitAndNative},
    {"bit_or",      "NN",   bitOrNative},
    {"bit_xor",     "NN",   bitXorNative},
    {"bit_not",     "N",    bitNotNative},
    {"bit_shift",   "NN",   bitShiftNative},
    {"random",      "",     randomNative},
    {"seed_rand",   "N",    seedRandNative},

    {"dbg_code",    "B",    dbgCodeNative},
    {"dbg_step",    "B",    dbgStepNative},
    {"dbg_call",    "B",    dbgCallNative},
    {"dbg_nat",     "B",    dbgNatNative},
    {"dbg_gc",      "N",    dbgGcNative},
    {"dbg_stat",    "B",    dbgStatNative},

    {"peek",        "N",    peekNative},
    {"poke",        "NN",   pokeNative},
    {"exec",        "Naaa", execNative},
    {"addr",        "A",    addrNative},
    {"heap",        "N",    heapNative},

#ifdef KIT68K
    {"lcd_clear",   "",     lcdClearNative},
    {"lcd_goto",    "NN",   lcdGotoNative},
    {"lcd_puts",    "S",    lcdPutsNative},
    {"lcd_defchar", "NL",   lcdDefcharNative},
    {"keycode",     "",     keycodeNative},
    {"sound",       "NN",   soundNative},
    {"trap",        "",     trapNative},
#endif

    {"sleep",       "N",    sleepNative},
    {"gc",          "",     gcNative},
    {"type",        "A",    typeNative},
    {"name",        "A",    nameNative},
    {"parent",      "K",    parentNative},
    {"class_of",    "A",    classOfNative},
    {"error",       "A",    errorNative},
    {"clock",       "",     clockNative},
};

void defineAllNatives() {
    int           natCount = sizeof(allNatives) / sizeof(Native);
    const Native* currNat  = allNatives;

    pushUnchecked(NIL_VAL);
    pushUnchecked(NIL_VAL);

    while (natCount--) {
        vm.stack[0] = OBJ_VAL(makeString(currNat->name, strlen(currNat->name)));
        vm.stack[1] = OBJ_VAL(makeNative(currNat));
        tableSet(&vm.globals, vm.stack[0], vm.stack[1]);
        currNat++;
    }

    drop();
    drop();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupting computations and ticker handling
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef KIT68K

void startTicker(void) {
    clock()     = 0;
    IRQ2_VECTOR = (void *)ticker;
    // ANDI  #$f0ff,SR  ; clear interrupt mask in status register
    _word(0x027c); _word(0xf0ff);
}

#else

#include <signal.h>

void irqHandler(int ignored) {
    vm.interrupted = true;
}

void handleInterrupts(bool enable) {
    if (enable)
        signal(SIGINT, &irqHandler);
    else
        signal(SIGINT, SIG_DFL);
}

#endif