#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "object.h"
#include "disasm.h"
#include "native.h"
#include "memory.h"
#include "vm.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Typechecking natives
////////////////////////////////////////////////////////////////////////////////////////////////////

static const char* matchesType(Value value, int type) {
    const char* NULLPTR = 0; // Using NULL constant in next statements creates inefficient code???

    switch (type) {
        case 'A': return                      NULLPTR;  // any value
        case 'C': return IS_CLASS(value)    ? NULLPTR : "a class";
#ifdef LOX_DBG
        case 'F': return IS_CLOSURE(value)
                      || IS_BOUND(value)
                      || IS_FUNCTION(value) ? NULLPTR : "a function";
#endif
        case 'I': return IS_INSTANCE(value) ? NULLPTR : "an instance";
        case 'L': return IS_LIST(value)     ? NULLPTR : "a list";
        case 'N': return IS_INT(value)      ? NULLPTR : "an int";
        case 'Q': return IS_STRING(value)
                      || IS_LIST(value)     ? NULLPTR : "a sequence";
        case 'R': return IS_INT(value)
                      || IS_REAL(value)     ? NULLPTR : "a number";
        case 'S': return IS_STRING(value)   ? NULLPTR : "a string";
        case 'T': return IS_ITERATOR(value) ? NULLPTR : "an iterator";
        default : return                                "an unknown type";
    }
}

bool callNative(const Native* native, int argCount, Value* args) {
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
        if (expected) {
            runtimeError("'%s' type mismatch at argument %d, expected %s but got %s.",
                         native->name, i + 1, expected, valueType(args[i]));
            return false;
        }
    }
    return (*native->function)(argCount, args);
}

#define RESULT args[-1]
#define NATIVE(fun) static bool fun(int argCount, Value* args)

// Concatening fun name with ## crashes IDE68K compiler

// Calling convention for natives:
// ===============================
//
// 1st parameter is number of arguments (only useful for variadic arity)
// 2nd parameter is pointer to call stack slice, arguments can be accessed by
// args[0], args[1], etc.
//
// Basic arity and type checks already have been done via the signature string,
// only in special cases you have to do further checks,
// e.g. join, lcd_defchar
//
// Each char in the signature strings stands for the allowed argument type at the
// corresponding position. See matchesType() above for possible types.
// Uppercase means required parameter, lower case optional parameter (must be trailing).
// Test argCount for actual number in this case. 
//
// If you allocate heap objects, you have to ensure that prior objects are not
// garbage collected, so you have to add them to the root set of reachable objects.
// The easiest way to do this is to push() every allocated object and drop() before returning. 
//
// Make sure that the stack level is the same after calling your native or terrible things
// may happen.
//
// The result is stored into args[-1] (or use macro RESULT)
//
// To indicate success, return true, on error return false.
// you have to call runtimeError(...) before returning false to provide a useful error message
// and setup possible exception handling.

////////////////////////////////////////////////////////////////////////////////////////////////////
// Arithmetics
////////////////////////////////////////////////////////////////////////////////////////////////////

#define CHECK_ARITH_ERROR(op)                       \
    if (errno != 0) {                               \
        runtimeError("'%s' arithmetic error.", op); \
        return false;                               \
    }

NATIVE(absNative) {
    if (IS_INT(args[0]))
        RESULT = INT_VAL(abs(AS_INT(args[0])));
    else
        RESULT = makeReal(fabs(AS_REAL(args[0])));
    return true;
}

NATIVE(truncNative) {
    errno = 0;
    if (IS_INT(args[0]))
        RESULT = args[0];
    else
        RESULT = INT_VAL(realToInt(AS_REAL(args[0])));
    CHECK_ARITH_ERROR("trunc")
    return true;
}

typedef Real (*RealFun)(Real);

#define NUMERIC_ARG(name,n) \
    Real name = (IS_INT(args[n])) ? intToReal(AS_INT(args[n])) : AS_REAL(args[n])

static bool transcendentalNative(Value* args, RealFun fun, const char* funName) {
    NUMERIC_ARG(x, 0);
    errno  = 0;
    RESULT = makeReal((*fun)(x));
    CHECK_ARITH_ERROR(funName)
    return true;
}

// The power of C macros...
#define TRANS_NATIVE(fun) \
    { return transcendentalNative(args, fun, #fun); }

NATIVE(sqrtNative)  TRANS_NATIVE(sqrt)
NATIVE(sinNative)   TRANS_NATIVE(sin)
NATIVE(cosNative)   TRANS_NATIVE(cos)
NATIVE(tanNative)   TRANS_NATIVE(tan)
NATIVE(sinhNative)  TRANS_NATIVE(sinh)
NATIVE(coshNative)  TRANS_NATIVE(cosh)
NATIVE(tanhNative)  TRANS_NATIVE(tanh)
NATIVE(expNative)   TRANS_NATIVE(exp)
NATIVE(logNative)   TRANS_NATIVE(log)
NATIVE(atanNative)  TRANS_NATIVE(atan)

NATIVE(powNative) {
    NUMERIC_ARG(x, 0);
    NUMERIC_ARG(y, 1);
    errno  = 0;
    RESULT = makeReal(pow(x,y));
    CHECK_ARITH_ERROR("pow")
    return true;
}

#ifdef KIT68K

Real mod(Real a, Real b) {
    Real q = div(a, b);
    Real h = intToReal(realToInt(q));
    h      = sub(q, h);
    return mul(b, h);
}

#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// Lists and strings
////////////////////////////////////////////////////////////////////////////////////////////////////

NATIVE(lengthNative) {
    if (IS_STRING(args[0]))
        RESULT = INT_VAL(AS_STRING(args[0])->length);
    else
        RESULT = INT_VAL(AS_LIST(args[0])->arr.count);
    return true;
}

NATIVE(listNative) {
    Value    item = (argCount == 2) ? args[1] : NIL_VAL;
    Int      len  = AS_INT(args[0]);

    if (len >= 16000) {
        runtimeError("'%s' %s out of range.", "list", "length");
        return false;
    }

    RESULT = OBJ_VAL(makeList(len, &item, len, 0));
    return true;
}

NATIVE(reverseNative) {
    ObjList* src = AS_LIST(args[0]);
    ObjList* res = makeList(src->arr.count, src->arr.values + src->arr.count - 1,
                            src->arr.count, -1);
    RESULT = OBJ_VAL(res);
    return true;
}

NATIVE(appendNative) {
    ObjList* list = AS_LIST(args[0]);
    appendValueArray(&list->arr, args[1]);
    RESULT = NIL_VAL;
    return true;
}

NATIVE(insertNative) {
    ObjList* list = AS_LIST(args[0]);
    insertIntoList(list, args[2], AS_INT(args[1]));
    RESULT = NIL_VAL;
    return true;
}

NATIVE(deleteNative) {
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

NATIVE(indexNative) {
    ObjList* list  = AS_LIST(args[1]);;
    int      start = (argCount == 2) ? 0 : AS_INT(args[2]);
    int      i;

    RESULT = NIL_VAL;
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
    return true;
}

NATIVE(removeNative) {
    ObjInstance* instance = AS_INSTANCE(args[0]);
    bool         removed  = tableDelete(&instance->fields, args[1]);
    RESULT = BOOL_VAL(removed);
    return true;
}

NATIVE(equalNative) {
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
        res = ait->instance == bit->instance && ait->position == bit->position;
    } else
        res = a == b;
    RESULT = BOOL_VAL(res);
    return true;
}

NATIVE(lowerNative) {
    RESULT = OBJ_VAL(caseString(AS_STRING(args[0]), false));
    return true;
}

NATIVE(upperNative) {
    RESULT = OBJ_VAL(caseString(AS_STRING(args[0]), true));
    return true;
}

#define APPEND(str) { \
    len = strlen(str); if (dest - big_buffer + len >= (INPUT_SIZE-1)) goto Overflow; \
    strcpy(dest, (str)); \
    dest += len; }

// FIXME?? Doesn't handle embedded \0 characters correctly.

NATIVE(joinNative) {
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

Overflow:
    runtimeError("'%s' stringbuffer overflow.", "join");
    return false;
}

NATIVE(splitNative) {
    ObjList*    list  = makeList(0, NULL, 0, 0);
    const char* curr  = AS_CSTRING(args[0]);
    const char* sepas = AS_CSTRING(args[1]);
    size_t      len;

    curr += strspn(curr, sepas);
    push(OBJ_VAL(list)); // protect result list from GC
    push(NIL_VAL);       // protect each string from GC
    while (*curr) {
        len     = strcspn(curr, sepas);
        peek(0) = OBJ_VAL(makeString(curr, len));
        appendValueArray(&list->arr, peek(0));
        curr += len;
        curr += strspn(curr, sepas);
    }

    RESULT = OBJ_VAL(list);
    drop();
    drop();
    return true;
}

// A minimal regex matcher described at
// https://www.cs.princeton.edu/courses/archive/spr09/cos333/beautiful.html
// Supports meta characters . ^ $ * ?
// compiles to about 400 bytes code,
// thus fitting the minimalistic approach of Lox68k
// Changes: added '?' meta character, return match position instead of bool, const pointers

static const char* matchhere(const char* regexp, const char* text);
static const char* matchstar(int c, const char* regexp, const char* text, int limit);

/* match: search for regexp anywhere in text */
static const char* match(const char* regexp, const char* text)
{
    if (regexp[0] == '^')
        return matchhere(regexp + 1, text) ? text : NULL;
    do {    /* must look even if string is empty */
        if (matchhere(regexp, text))
            return text;
    } while (*text++);
    return NULL;
}

/* matchhere: search for regexp at beginning of text */
static const char* matchhere(const char* regexp, const char* text)
{
    CHECK_STACKOVERFLOW

    if (regexp[0] == '\0')
        return text;
    if (regexp[1] == '*')
        return matchstar(regexp[0], regexp + 2, text, -1);
    if (regexp[1] == '?')
        return matchstar(regexp[0], regexp + 2, text, 1);
    if (regexp[0] == '$' && regexp[1] == '\0')
        return *text == '\0' ? text : NULL;
    if (*text != '\0' && (regexp[0] == '.' || regexp[0] == *text))
        return matchhere(regexp + 1, text + 1);
    return NULL;
}

/* matchstar: search for c*regexp at beginning of text */
static const char* matchstar(int c, const char* regexp, const char* text, int limit)
{
    do {    /* a * matches zero or more instances */
        if (matchhere(regexp, text))
            return text;
    } while (limit-- && *text != '\0' && (*text++ == c || c == '.'));
    return NULL;
}

NATIVE(matchNative) {
    const char* text = AS_CSTRING(args[1]);
    const char* pos  = match(AS_CSTRING(args[0]), text);
    RESULT = pos ? INT_VAL(pos - text) : NIL_VAL;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// Iterators
////////////////////////////////////////////////////////////////////////////////////////////////////

NATIVE(slotsNative) {
    ObjInstance* instance = AS_INSTANCE(args[0]);
    RESULT = OBJ_VAL(makeIterator(instance));
    return true;
}

NATIVE(validNative) {
    ObjIterator* iter = AS_ITERATOR(args[0]);
    RESULT = BOOL_VAL(isValidIterator(iter));
    return true;
}

NATIVE(nextNative) {
    ObjIterator* iter = AS_ITERATOR(args[0]);
    advanceIterator(iter, iter->position + 1);
    RESULT = BOOL_VAL(isValidIterator(iter));
    return true;
}

NATIVE(itCloneNative) {
    ObjIterator* src  = AS_ITERATOR(args[0]);
    ObjIterator* dest = makeIterator(src->instance);
    dest->position = src->position;
    RESULT = OBJ_VAL(dest);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Some datatype conversions
////////////////////////////////////////////////////////////////////////////////////////////////////

NATIVE(ascNative) {
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

NATIVE(chrNative) {
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

NATIVE(decNative) {
    const char* res;
    if (IS_INT(args[0]))
        res = formatInt(AS_INT(args[0]));
    else
        res = formatReal(AS_REAL(args[0]));
    RESULT = OBJ_VAL(makeString0(res));
    return true;
}

NATIVE(hexNative) {
    const char* res = formatHex(AS_INT(args[0]));
    RESULT = OBJ_VAL(makeString0(res));
    return true;
}

NATIVE(binNative) {
    const char* res = formatBin(AS_INT(args[0]));
    RESULT = OBJ_VAL(makeString0(res));
    return true;
}

NATIVE(parseIntNative) {
    RESULT = parseInt(AS_CSTRING(args[0]), true);
    return true;
}

NATIVE(parseRealNative) {
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

NATIVE(inputNative) {
    if (argCount > 0)
        putstr(AS_CSTRING(args[0]));
    if (readLine())
        RESULT = OBJ_VAL(makeString0(big_buffer));
    else
        RESULT = NIL_VAL;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Integers
////////////////////////////////////////////////////////////////////////////////////////////////////

NATIVE(bitAndNative) {
    // No shifting and masking needed :-)
    RESULT = args[0] & args[1];
    return true;
}

NATIVE(bitOrNative) {
    RESULT = args[0] | args[1];
    return true;
}

NATIVE(bitXorNative) {
    RESULT = (args[0] ^ args[1]) | 1;
    return true;
}

NATIVE(bitNotNative) {
    RESULT = ~args[0] | 1;
    return true;
}

NATIVE(bitShiftNative) {
    Int amount = AS_INT(args[1]);
    if (amount > 0)
        RESULT = (args[0] & ~1) << amount | 1;
    else
        RESULT = args[0] >> -amount | 1;
    return true;
}

NATIVE(randomNative) {
    vm.randomState ^= vm.randomState << 13;
    vm.randomState ^= vm.randomState >> 17;
    vm.randomState ^= vm.randomState << 5;
    RESULT = INT_VAL(vm.randomState & 0x3fffffff);
    return true;
}

NATIVE(seedRandNative) {
    RESULT = INT_VAL(vm.randomState);
    vm.randomState = AS_INT(args[0]);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Modifying debugging options, disassembler
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef LOX_DBG

static bool setVMFlag(Value* args, bool* flag) {
    *flag  = !IS_FALSEY(args[0]);
    RESULT = NIL_VAL;
    return true;
}

NATIVE(dbgCodeNative) {
    return setVMFlag(args, &vm.debug_print_code);
}

NATIVE(dbgStepNative) {
    return setVMFlag(args, &vm.debug_trace_steps);
}

NATIVE(dbgCallNative) {
    return setVMFlag(args, &vm.debug_trace_calls);
}

NATIVE(dbgNatNative) {
    return setVMFlag(args, &vm.debug_trace_natives);
}

NATIVE(dbgGcNative) {
    vm.debug_log_gc = AS_INT(args[0]);
    RESULT = NIL_VAL;
    return true;
}

NATIVE(dbgStatNative) {
    return setVMFlag(args, &vm.debug_statistics);
}

NATIVE(disasmNative) {
    ObjFunction* fun = IS_FUNCTION(args[0]) ? AS_FUNCTION(args[0])
                     : IS_BOUND(args[0])    ? AS_BOUND(args[0])->method->function
                                            : AS_CLOSURE(args[0])->function;
    Chunk* chunk     = &fun->chunk;
    int offset       = AS_INT(args[1]);
    if (offset < 0 || offset >= chunk->count) {
        runtimeError("'%s' %s out of range.", "disasm", "offset");
        return false;
    }
    offset = disassembleInst(chunk, offset);
    RESULT = (offset < chunk->count) ? INT_VAL(offset) : NIL_VAL;
    return true;
}

#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
// Machine code support
////////////////////////////////////////////////////////////////////////////////////////////////////

NATIVE(peekNative) {
    int32_t address = AS_INT(args[0]);
    Int     byte    = *((uint8_t*)address);
    RESULT = INT_VAL(byte);
    return true;
}

NATIVE(pokeNative) {
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

NATIVE(execNative) {
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

NATIVE(addrNative) {
    if (IS_OBJ(args[0]))
        RESULT = INT_VAL((uint32_t)AS_OBJ(args[0]));
    else
        RESULT = NIL_VAL;
    return true;
}

NATIVE(heapNative) {
    int32_t address = AS_INT(args[0]);
    RESULT = *((Value*)address);
    return true;
}

#ifdef KIT68K

////////////////////////////////////////////////////////////////////////////////////////////////////
// 68008 Kit specific routines
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "monitor4x.h"

NATIVE(lcdClearNative) {
    lcd_clear();
    RESULT = NIL_VAL;
    return true;
}

NATIVE(lcdGotoNative) {
    lcd_goto(AS_INT(args[0]), AS_INT(args[1]));
    RESULT = NIL_VAL;
    return true;
}

NATIVE(lcdPutsNative) {
    lcd_puts(AS_CSTRING(args[0]));
    RESULT = NIL_VAL;
    return true;
}

NATIVE(lcdDefcharNative) {
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

NATIVE(keycodeNative) {
    int code = monitor_scan();
    if (code == 255)
        RESULT = NIL_VAL;
    else
        RESULT = INT_VAL(code);
    return true;
}

#define LOOPS_NOTE  97400
#define DELAY_EXTRA     2

NATIVE(soundNative) {
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

NATIVE(trapNative) {
    _trap(0);
    RESULT = NIL_VAL;
    return true;
}

#define LOOPS_PER_MILLI 147

NATIVE(sleepNative) {
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

NATIVE(sleepNative) {
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

NATIVE(gcNative) {
    collectGarbage(false);
    RESULT = INT_VAL(vm.bytesAllocated);
    return true;
}

NATIVE(typeNative) {
    const char* type = valueType(args[0]);
    RESULT = OBJ_VAL(makeString0(type));
    return true;
}

NATIVE(nameNative) {
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
            RESULT = OBJ_VAL(makeString0(name));
    }
    return true;
}

NATIVE(parentNative) {
    RESULT = OBJ_VAL(AS_CLASS(args[0])->superClass);
    return true;
}

NATIVE(classOfNative) {
    if (IS_INSTANCE(args[0]))
        RESULT = OBJ_VAL(AS_INSTANCE(args[0])->klass);
    else if (IS_BOUND(args[0]))
        RESULT = OBJ_VAL(AS_BOUND(args[0])->method->function->klass);
    else
        RESULT = NIL_VAL;
    return true;
}

NATIVE(errorNative) {
    userError(args[0]);
    return false;
}

NATIVE(clockNative) {
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
    {"index",       "ALn",  indexNative},
    {"remove",      "IA",   removeNative},
    {"equal",       "AA",   equalNative},

    {"lower",       "S",    lowerNative},
    {"upper",       "S",    upperNative},
    {"join",        "Lsss", joinNative},
    {"split",       "SS",   splitNative},
    {"match",       "SS",   matchNative},

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

    {"sleep",       "N",    sleepNative},
    {"gc",          "",     gcNative},
    {"type",        "A",    typeNative},
    {"name",        "A",    nameNative},
    {"parent",      "C",    parentNative},
    {"class_of",    "A",    classOfNative},
    {"error",       "A",    errorNative},
    {"clock",       "",     clockNative},

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

#ifdef LOX_DBG
    {"dbg_code",    "A",    dbgCodeNative},
    {"dbg_step",    "A",    dbgStepNative},
    {"dbg_call",    "A",    dbgCallNative},
    {"dbg_nat",     "A",    dbgNatNative},
    {"dbg_gc",      "N",    dbgGcNative},
    {"dbg_stat",    "A",    dbgStatNative},
    {"disasm",      "FN",   disasmNative},
#endif
};

void defineAllNatives() {
    int           natCount = sizeof(allNatives) / sizeof(Native);
    const Native* currNat  = allNatives;

    pushUnchecked(NIL_VAL);
    pushUnchecked(NIL_VAL);

    while (natCount--) {
        vm.stack[0] = OBJ_VAL(makeString0(currNat->name));
        vm.stack[1] = OBJ_VAL(makeNative(currNat));
        tableSet(&vm.globals, vm.stack[0], vm.stack[1]);
        currNat++;
    }

    drop();
    drop();
}


