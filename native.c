#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include "common.h"
#include "debug.h"
#include "object.h"
#include "native.h"
#include "memory.h"
#include "vm.h"


static const char* matchesType(Value value, char type) {
    switch (type) {
        case 'A': return                                      NULL;  // any value
        case 'N': return IS_INT(value)                      ? NULL : "an int";
        case 'R': return IS_INT(value) || IS_REAL(value)    ? NULL : "a number";
        case 'S': return IS_STRING(value)                   ? NULL : "a string";
        case 'L': return IS_LIST(value)                     ? NULL : "a list";
        case 'Q': return IS_STRING(value) || IS_LIST(value) ? NULL : "a sequence";
        case 'B': return IS_BOOL(value)                     ? NULL : "a bool";
        case 'I': return IS_INSTANCE(value)                 ? NULL : "an instance";
        case 'T': return IS_ITERATOR(value)                 ? NULL : "an iterator";
        default:  return "an unknown type";
    }
}

bool checkNativeSignature(const char* signature, int argCount, Value* args) {
    int maxParmCount;
    int minParmCount;
    int i;
    const char* expected;

    // Trailing lower-case letters in signature indicate optional arguments.
    maxParmCount = 0;
    for (i = 0; i < sizeof(Signature); i++)
        if (signature[i])
            maxParmCount++;
        else
            break; 
    minParmCount = maxParmCount;

    while (minParmCount > 0 && islower(signature[minParmCount-1]))
        minParmCount--;

    if (minParmCount > argCount || argCount > maxParmCount) {
        if (minParmCount == maxParmCount)
            runtimeError("Expected %d arguments but got %d.", maxParmCount, argCount);
        else
            runtimeError("Expected %d to %d arguments but got %d.",
                          minParmCount, maxParmCount, argCount);
        return false;
    }

    for (i = 0; i < argCount; i++) {
        expected = matchesType(args[i], toupper(signature[i]));
        if (expected != NULL) {
            runtimeError("Type mismatch at argument %d, expected %s.", i + 1, expected);
            return false;
        }
    }
    return true;
}


static bool realNative(int argCount, Value* args) {
    if (IS_INT(args[0]))
        args[-1] = newReal(intToReal(AS_INT(args[0])));
    else
        args[-1] = args[0];
    return true;
}

static bool absNative(int argCount, Value* args) {
    if (IS_INT(args[0]))
        args[-1] = INT_VAL(abs(AS_INT(args[0])));
    else
        args[-1] = newReal(fabs(AS_REAL(args[0])));
    return true;
}

static bool truncNative(int argCount, Value* args) {
    if (IS_INT(args[0]))
        args[-1] = args[0];
    else
        args[-1] = INT_VAL(realToInt(AS_REAL(args[0])));
    if (errno != 0) {
        runtimeError("Arithmetic error.");
        return false;
    }
    return true;
}

typedef Real (*RealFun)(Real);

static bool transcendentalNative(int argCount, Value* args, RealFun fn) {
    if (IS_INT(args[0]))
        args[-1] = newReal((*fn)(intToReal(AS_INT(args[0]))));
    else
        args[-1] = newReal((*fn)(AS_REAL(args[0])));
    if (errno != 0) {
        runtimeError("Arithmetic error.");
        return false;
    }
    return true;
}

static bool sqrtNative(int argCount, Value* args) {
    return transcendentalNative(argCount, args, sqrt);
}

static bool sinNative(int argCount, Value* args) {
    return transcendentalNative(argCount, args, sin);
}

static bool cosNative(int argCount, Value* args) {
    return transcendentalNative(argCount, args, cos);
}

static bool tanNative(int argCount, Value* args) {
    return transcendentalNative(argCount, args, tan);
}

static bool sinhNative(int argCount, Value* args) {
    return transcendentalNative(argCount, args, sinh);
}

static bool coshNative(int argCount, Value* args) {
    return transcendentalNative(argCount, args, cosh);
}

static bool tanhNative(int argCount, Value* args) {
    return transcendentalNative(argCount, args, tanh);
}

static bool expNative(int argCount, Value* args) {
    return transcendentalNative(argCount, args, exp);
}

static bool logNative(int argCount, Value* args) {
    return transcendentalNative(argCount, args, log);
}

static bool atanNative(int argCount, Value* args) {
    return transcendentalNative(argCount, args, atan);
}

static bool powNative(int argCount, Value* args) {
    Real x = (IS_INT(args[0])) ? intToReal(AS_INT(args[0])) : AS_REAL(args[0]);
    Real y = (IS_INT(args[1])) ? intToReal(AS_INT(args[1])) : AS_REAL(args[1]);
    args[-1] = newReal(pow(x,y));
    if (errno != 0) {
        runtimeError("Arithmetic error.");
        return false;
    }
    return true;
}

static bool lengthNative(int argCount, Value* args) {
    if (IS_STRING(args[0]))
        args[-1] = INT_VAL(AS_STRING(args[0])->length);
    else
        args[-1] = INT_VAL(AS_LIST(args[0])->count);
    return true;
}

static bool appendNative(int argCount, Value* args) {
    ObjList* list = AS_LIST(args[0]);
    appendToList(list, args[1]);
    args[-1] = NIL_VAL;
    return true;
}

static bool insertNative(int argCount, Value* args) {
    ObjList* list = AS_LIST(args[0]);
    insertIntoList(list, args[2], AS_INT(args[1]));
    args[-1] = NIL_VAL;
    return true;
}

static bool deleteNative(int argCount, Value* args) {
    ObjList* list  = AS_LIST(args[0]);
    int      index = AS_INT(args[1]);
    if (!isValidListIndex(list, index)) {
        runtimeError("%s out of range.", "List index");
        return false;
    }
    deleteFromList(list, index);
    args[-1] = NIL_VAL;
    return true;
}

static bool indexNative(int argCount, Value* args) {
    ObjList* list  = AS_LIST(args[1]);
    Value    item  = args[0];
    int      start = (argCount == 2) ? 0 : AS_INT(args[2]);
    int      i;

    args[-1] = NIL_VAL;
    if (list->count == 0)
        return true;

    if (!isValidListIndex(list, start)) {
        runtimeError("%s out of range.", "Start index");
        return false;
    }

    if (start < 0) start += list->count;

    for (i = start; i < list->count; i++) {
        if (valuesEqual(item, list->items[i])) {
            args[-1] = INT_VAL(i);
            break;
        }
    }
    return true;
}

static bool removeNative(int argCount, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[0]);
    bool         removed  = tableDelete(&instance->fields, args[1]);
    args[-1] = BOOL_VAL(removed);
    return true;
}

static bool globalsNative(int argCount, Value* args) {
    args[-1] = OBJ_VAL(newIterator(&vm.globals, NULL));
    return true;
}

static bool typeNative(int argCount, Value* args) {
    args[-1] = OBJ_VAL(valueType(args[0]));
    return true;
}

// Iterators

static bool slotsNative(int argCount, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[0]);
    args[-1] = OBJ_VAL(newIterator(&instance->fields, instance));
    return true;
}

static bool validNative(int argCount, Value* args) {
    ObjIterator* iter = AS_ITERATOR(args[0]);
    args[-1] = BOOL_VAL(isValidIterator(iter));
    return true;
}

static bool nextNative(int argCount, Value* args) {
    ObjIterator* iter = AS_ITERATOR(args[0]);
    nextIterator(iter);
    args[-1] = BOOL_VAL(isValidIterator(iter));
    return true;
}


// Some datatype conversions

static bool ascNative(int argCount, Value* args) {
    ObjString* string = AS_STRING(args[0]);
    Int        code;
    int        index = (argCount == 1) ? 0 : AS_INT(args[1]);

    if (!isValidStringIndex(string, index)) {
        runtimeError("%s out of range.", "String index");
        return false;
    }
    if (index < 0)
        index += string->length;
    code = string->chars[index] & 0xff;
    args[-1] = INT_VAL(code);
    return true;
}

static bool chrNative(int argCount, Value* args) {
    Int  code = AS_INT(args[0]);
    char codepoint;

    if (code < 0 || code > 255) {
        runtimeError("%s out of range.", "Char code");
        return false;
    }
    codepoint = (char)code;
    args[-1] = OBJ_VAL(copyString(&codepoint, 1));
    return true;
}

static bool decNative(int argCount, Value* args) {
    const char* res;
    if (IS_INT(args[0]))
        res = formatInt(AS_INT(args[0]));
    else
        res = formatReal(AS_REAL(args[0]));
    args[-1] = OBJ_VAL(copyString(res, strlen(res)));
    return true;
}

static bool hexNative(int argCount, Value* args) {
    const char* res = formatHex(AS_INT(args[0]));
    args[-1] = OBJ_VAL(copyString(res, strlen(res)));
    return true;
}

static bool binNative(int argCount, Value* args) {
    const char* res = formatBin(AS_INT(args[0]));
    args[-1] = OBJ_VAL(copyString(res, strlen(res)));
    return true;
}

static bool parseIntNative(int argCount, Value* args) {
    args[-1] = parseInt(AS_CSTRING(args[0]), true);
    return true;
}

char* terminator; // filled by scanReal()

static bool parseRealNative(int argCount, Value* args) {
    char* start = AS_CSTRING(args[0]);
    char* end   = start;
    Real  real;

#ifdef KIT68K
    real = scanReal(start);
    if (errno == 0 && start + strlen(start) == terminator)
        args[-1] = newReal(real);
    else
        args[-1] = NIL_VAL;
#else
    real = strtod(start, &end);
    if (start + strlen(start) == end)
        args[-1] = newReal(real);
    else
        args[-1] = NIL_VAL;
#endif
    return true;
}

static bool inputNative(int argCount, Value* args) {
    if (argCount > 0)
        printf("%s ", AS_CSTRING(args[0]));
    GETS(input_line);
    args[-1] = OBJ_VAL(copyString(input_line, strlen(input_line)));
    return true;
}

static bool bitAndNative(int argCount, Value* args) {
    // No shifting and masking needed :-)
    args[-1] = args[0] & args[1];
    return true;
}

static bool bitOrNative(int argCount, Value* args) {
    args[-1] = args[0] | args[1];
    return true;
}

static bool bitXorNative(int argCount, Value* args) {
    args[-1] = (args[0] ^ args[1]) | 1;
    return true;
}

static bool bitNotNative(int argCount, Value* args) {
    args[-1] = (~args[0]) | 1;
    return true;
}

static bool bitShiftNative(int argCount, Value* args) {
    Int value = AS_INT(args[0]);
    Int amount = AS_INT(args[1]);
    if (amount > 0)
        args[-1] = INT_VAL(value << amount);
    else
        args[-1] = INT_VAL(value >> (-amount));
    return true;
}

uint32_t rand32;

static bool randomNative(int argCount, Value* args) {
    rand32 ^= rand32 << 13;
    rand32 ^= rand32 >> 17;
    rand32 ^= rand32 << 5;
    args[-1] = INT_VAL(rand32 & 0x3fffffff);
    return true;
}

static bool seedRandNative(int argCount, Value* args) {
    args[-1] = INT_VAL(rand32);
    rand32 = AS_INT(args[0]);
    return true;
}

static bool lowerNative(int argCount, Value* args) {
    args[-1] = OBJ_VAL(caseString(AS_STRING(args[0]), false));
    return true;
}


static bool upperNative(int argCount, Value* args) {
    args[-1] = OBJ_VAL(caseString(AS_STRING(args[0]), true));
    return true;
}

// Modifying debugging options

static bool dbgCodeNative(int argCount, Value* args) {
    vm.debug_print_code = AS_BOOL(args[0]);
    args[-1] = args[0];
    return true;
}

static bool dbgTraceNative(int argCount, Value* args) {
    vm.debug_trace_execution = AS_BOOL(args[0]);
    args[-1] = args[0];
    return true;
}

static bool dbgGcNative(int argCount, Value* args) {
    vm.debug_log_gc = AS_INT(args[0]);
    args[-1] = args[0];
    return true;
}

static bool dbgStatNative(int argCount, Value* args) {
    vm.debug_statistics = AS_BOOL(args[0]);
    args[-1] = args[0];
    return true;
}


// Remember your old 80s home computers?

static bool peekNative(int argCount, Value* args) {
    int32_t address = AS_INT(args[0]);
    Int     byte    = *((uint8_t*)address);
    args[-1] = INT_VAL(byte);
    return true;
}

static bool pokeNative(int argCount, Value* args) {
    int32_t address = AS_INT(args[0]);
    Int     byte    = AS_INT(args[1]);
    if (byte < 0 || byte > 255) {
        runtimeError("%s out of range.", "Byte value");
        return false;
    }
    *((uint8_t*)address) = byte & 0xff;
    args[-1] = NIL_VAL;
    return true;
}

static bool execNative(int argCount, Value* args) {
    typedef Value sub0(void);
    typedef Value sub1(Value);
    typedef Value sub2(Value,Value);
    typedef Value sub3(Value,Value,Value);

    int32_t address = AS_INT(args[0]);
    Value   result;

    switch (argCount) {
        case 1: result = ((sub0*) address)(); break;
        case 2: result = ((sub1*) address)(args[1]); break;
        case 3: result = ((sub2*) address)(args[1],args[2]); break;
        case 4: result = ((sub3*) address)(args[1],args[2],args[3]); break;
    }

    args[-1] = result;
    return true;
}

static bool addrNative(int argCount, Value* args) {
    if (IS_OBJ(args[0]))
        args[-1] = INT_VAL((uint32_t)AS_OBJ(args[0]));
    else
        args[-1] = NIL_VAL;
    return true;
}

#ifdef KIT68K

#include "monitor4x.h"

static bool lcdClearNative(int argCount, Value* args) {
    lcd_clear();
    args[-1] = NIL_VAL;
    return true;
}

static bool lcdGotoNative(int argCount, Value* args) {
    lcd_goto(AS_INT(args[0]), AS_INT(args[1]));
    args[-1] = NIL_VAL;
    return true;
}

static bool lcdPutsNative(int argCount, Value* args) {
    lcd_puts(AS_CSTRING(args[0]));
    args[-1] = NIL_VAL;
    return true;
}

static bool lcdDefcharNative(int argCount, Value* args) {
    int      udc        = AS_INT(args[0]);
    ObjList* pattern    = AS_LIST(args[1]);
    char     bitmap[8];
    Int      byte;
    int      i;

    if (udc < 0 || udc > 7) {
        runtimeError("%s out of range.", "UDC number");
        return false;
    }
    if (pattern->count != 8) {
        runtimeError("UDC bitmap must be 8 bytes.");
        return false;
    }
    for (i = 0; i < 8; i++) {
        if (IS_INT(pattern->items[i])) {
            byte = AS_INT(pattern->items[i]);
            if (byte < 0 || byte > 255) {
                runtimeError("%s out of range.", "Byte value");
                return false;
            }
            bitmap[i] = (char)byte;
        } else {
            runtimeError("Integer expected in UDC bitmap at %d.", i);
            return false;
        }
    }

    lcd_defchar(udc, bitmap);
    args[-1] = NIL_VAL;
    return true;
}

static bool keycodeNative(int argCount, Value* args) {
    int code = monitor_scan();
    if (code == 255)
        args[-1] = NIL_VAL;
    else
        args[-1] = INT_VAL(code);
    return true;
}

//#define LOOPS_NOTE 34483  // for non-register variables
#define LOOPS_NOTE 91900 // for register variables

static bool soundNative(int argCount, Value* args) {
    int delay = AS_INT(args[0]);
    int len   = AS_INT(args[1]);
    int loops = LOOPS_NOTE;
    int i,j;

    loops *= len;
    loops /= 1000;
    loops /= delay; // direct init generates wrong code, PEA 34483.W, which is negative

    *port2 = 0;
    for (i = 0; i < loops; i++) {
        *port1 &= ~0x40;
        for (j = 0; j < delay; j++);
        *port1 |= 0x40;
        for (j = 0; j < delay; j++);
    }

    args[-1] = NIL_VAL;
    return true;
}

//#define LOOPS_PER_MILLI 72  // for non-register variables
#define LOOPS_PER_MILLI 142 // for register variables

static bool sleepNative(int argCount, Value* args) {
    int32_t millis = AS_INT(args[0]);
    int32_t i, j;

    for (i = 0; i < millis; i++)
        for (j = 0; j < LOOPS_PER_MILLI; j++)
            ;

    args[-1] = NIL_VAL;
    return true;
}

static bool trapNative(int argCount, Value* args) {
    _trap(0);
    args[-1] = NIL_VAL;
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

    args[-1] = NIL_VAL;
    return true;
}

#endif

static bool clockNative(int argCount, Value* args) {
#ifdef KIT68K
    args[-1] = INT_VAL(0); // not available
#else
    args[-1] = INT_VAL(clock() * 1000 / CLOCKS_PER_SEC); // in milliseconds
#endif
    return true;
}

static bool gcNative(int argCount, Value* args) {
    collectGarbage(false);
    args[-1] = INT_VAL(vm.bytesAllocated);
    return true;
}

static void defineNative(const char* name, const char* signature, NativeFn function) {
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(signature, function)));
    tableSet(&vm.globals, vm.stack[0], vm.stack[1]);
    drop();
    drop();
}

void defineAllNatives(void) {
    defineNative("real",        "R",    realNative);
    defineNative("abs",         "R",    absNative);
    defineNative("trunc",       "R",    truncNative);
    defineNative("sqrt",        "R",    sqrtNative);
    defineNative("sin",         "R",    sinNative);
    defineNative("cos",         "R",    cosNative);
    defineNative("tan",         "R",    tanNative);
    defineNative("sinh",        "R",    sinhNative);
    defineNative("cosh",        "R",    coshNative);
    defineNative("tanh",        "R",    tanhNative);
    defineNative("exp",         "R",    expNative);
    defineNative("log",         "R",    logNative);
    defineNative("atan",        "R",    atanNative);
    defineNative("pow",         "RR",   powNative);

    defineNative("asc",         "Sn",   ascNative);
    defineNative("chr",         "N",    chrNative);
    defineNative("dec",         "R",    decNative);
    defineNative("hex",         "N",    hexNative);
    defineNative("bin",         "N",    binNative);
    defineNative("parse_int",   "S",    parseIntNative);
    defineNative("parse_real",  "S",    parseRealNative);
    defineNative("input",       "s",    inputNative);

    defineNative("bit_and",     "NN",   bitAndNative);
    defineNative("bit_or",      "NN",   bitOrNative);
    defineNative("bit_xor",     "NN",   bitXorNative);
    defineNative("bit_not",     "N",    bitNotNative);
    defineNative("bit_shift",   "NN",   bitShiftNative);

    defineNative("random",      "",     randomNative);
    defineNative("seed_rand",   "N",    seedRandNative);

    defineNative("lower",       "S",    lowerNative);
    defineNative("upper",       "S",    upperNative);

    defineNative("length",      "Q",    lengthNative);
    defineNative("append",      "LA",   appendNative);
    defineNative("insert",      "LNA",  insertNative);
    defineNative("delete",      "LN",   deleteNative);
    defineNative("index",       "ALn",  indexNative);

    defineNative("remove",      "IA",   removeNative);
    defineNative("globals",     "",     globalsNative);
    defineNative("type",        "A",    typeNative);
    defineNative("clock",       "",     clockNative);
    defineNative("sleep",       "N",    sleepNative);
    defineNative("gc",          "",     gcNative);

    defineNative("slots",       "I",    slotsNative);
    defineNative("valid",       "T",    validNative);
    defineNative("next",        "T",    nextNative);

    defineNative("peek",        "N",    peekNative);
    defineNative("poke",        "NN",   pokeNative);
    defineNative("exec",        "Naaa", execNative);
    defineNative("addr",        "A",    addrNative);

#ifdef KIT68K
    defineNative("trap",        "",     trapNative);
    defineNative("lcd_clear",   "",     lcdClearNative);
    defineNative("lcd_goto",    "NN",   lcdGotoNative);
    defineNative("lcd_puts",    "S",    lcdPutsNative);
    defineNative("lcd_defchar", "NL",   lcdDefcharNative);
    defineNative("keycode",     "",     keycodeNative);
    defineNative("sound",       "NN",   soundNative);
#endif

    defineNative("dbg_code",    "B",    dbgCodeNative);
    defineNative("dbg_trace",   "B",    dbgTraceNative);
    defineNative("dbg_gc",      "N",    dbgGcNative);
    defineNative("dbg_stat",    "B",    dbgStatNative);
}

#ifdef KIT68K

#define INTERRUPT2_VECTOR ((long*)0x0068)

interrupt void irqHandler(void) {
    vm.interrupted = true;
}

void handleInterrupts(bool enable) {
    *INTERRUPT2_VECTOR = (long)irqHandler;

    if (enable) {
        // ANDI  #$f0ff,SR  ; clear interrupt mask in status register
        _word(0x027c); _word(0xf0ff);
    } else {
        // ORI   #$0f00,SR  ; set interrupt mask in status register
        _word(0x007c); _word(0x0f00);
    }
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
