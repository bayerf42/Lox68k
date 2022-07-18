#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

#include "common.h"
#include "debug.h"
#include "object.h"
#include "native.h"
#include "memory.h"
#include "vm.h"

static const char* matchesType(Value value, char type) {
    switch (type) {
        case 'A': return NULL; // any value
        case 'N': return IS_NUMBER(value) ? NULL : "a number";
        case 'S': return IS_STRING(value) ? NULL : "a string";
        case 'L': return IS_LIST(value) ? NULL : "a list";
        case 'Q': return IS_STRING(value) || IS_LIST(value) ? NULL : "a sequence";
        case 'B': return IS_BOOL(value) ? NULL : "a bool";
        case 'I': return IS_OBJ(value) ? NULL : "an instance";
        default:  return "an unknown type";
    }
}

bool checkNativeSignature(const char* signature, int argCount, Value* args) {
    int maxParmCount = strlen(signature);
    int minParmCount = maxParmCount;
    int i;
    const char* expected;

    // Trailing lower-case letters in signature indicate optional arguments.

    while (minParmCount > 0 && islower(signature[minParmCount-1])) minParmCount--;

    if (minParmCount > argCount || argCount > maxParmCount) {
        if (minParmCount == maxParmCount)
            runtimeError("Expected %d arguments but got %d.", maxParmCount, argCount);
        else
            runtimeError("Expected %d to %d arguments but got %d.", minParmCount, maxParmCount, argCount);
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


static bool absNative(int argCount, Value* args) {
    args[-1] = NUMBER_VAL(abs(AS_NUMBER(args[0])));
    return true;
}


static bool lengthNative(int argCount, Value* args) {
    if (IS_STRING(args[0])) {
        args[-1] = NUMBER_VAL(AS_STRING(args[0])->length);
    } else {
        args[-1] = NUMBER_VAL(AS_LIST(args[0])->count);
    }
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
    insertIntoList(list, args[2], AS_NUMBER(args[1]));
    args[-1] = NIL_VAL;
    return true;
}

static bool deleteNative(int argCount, Value* args) {
    ObjList* list = AS_LIST(args[0]);
    int index = AS_NUMBER(args[1]);
    if (!isValidListIndex(list, index)) {
        runtimeError("List index out of bound.");
        return false;
    }
    deleteFromList(list, index);
    args[-1] = NIL_VAL;
    return true;
}

static bool indexNative(int argCount, Value* args) {
    ObjList* list = AS_LIST(args[1]);
    Value item = args[0];
    int start = (argCount == 2) ? 0 : AS_NUMBER(args[2]);
    int i;

    args[-1] = NIL_VAL;
    if (list->count == 0) return true;

    if (!isValidListIndex(list, start)) {
        runtimeError("Start index out of bound.");
        return false;
    }

    if (start < 0) start += list->count;

    for (i = start; i < list->count; i++) {
        if (valuesEqual(item, list->items[i])) {
            args[-1] = NUMBER_VAL(i);
            break;
        }
    }
    return true;
}

static bool slotsNative(int argCount, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[0]);
    args[-1] = OBJ_VAL(allKeys(&instance->fields));
    return true;
}

static bool removeNative(int argCount, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[0]);
    bool removed = tableDelete(&instance->fields, AS_STRING(args[1]));
    args[-1] = BOOL_VAL(removed);
    return true;
}

static bool globalsNative(int argCount, Value* args) {
    args[-1] = OBJ_VAL(allKeys(&vm.globals));
    return true;
}

static bool typeNative(int argCount, Value* args) {
    args[-1] = OBJ_VAL(valueType(args[0]));
    return true;
}

// Some datatype conversions


static bool ascNative(int argCount, Value* args) {
    ObjString* string = AS_STRING(args[0]);
    int index = (argCount == 1) ? 0 : AS_NUMBER(args[1]);

    if (!isValidStringIndex(string, index)) {
        runtimeError("String index out of range.");
        return false;
    }
    if (index < 0) index += string->length;
    args[-1] = NUMBER_VAL((uint8_t)string->chars[index]);
    return true;
}

static bool chrNative(int argCount, Value* args) {
    Number code = AS_NUMBER(args[0]);
    char codepoint;

    if (code < 0 || code > 255) {
        runtimeError("Char code out of range.");
        return false;
    }
    codepoint = (char)code;
    args[-1] = OBJ_VAL(copyString(&codepoint, 1));
    return true;
}

static bool decNative(int argCount, Value* args) {
    Number number = AS_NUMBER(args[0]);
    char buffer[12];

    sprintf(buffer, "%ld", number);
    args[-1] = OBJ_VAL(copyString(buffer, strlen(buffer)));
    return true;
}

static bool hexNative(int argCount, Value* args) {
    Number number = AS_NUMBER(args[0]);
    char buffer[12];

    sprintf(buffer, "%lx", number);
    args[-1] = OBJ_VAL(copyString(buffer, strlen(buffer)));
    return true;
}

static bool intNative(int argCount, Value* args) {
    char* start = AS_CSTRING(args[0]);
    char* end = start;
    Number number;

    if (*start == '$')
        number = strtol(start + 1, &end, 16);
    else
        number = strtol(start, &end, 10);

    if (start + strlen(start) == end)
        args[-1] = NUMBER_VAL(number);
    else
        args[-1] = NIL_VAL;
    return true;
}

static bool inputNative(int argCount, Value* args) {
    if (argCount > 0) printf("%s ", AS_CSTRING(args[0]));

#ifdef KIT68K
    gets(input_line);
#else
    fgets(input_line, sizeof(input_line), stdin);
#endif

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
    Number value = AS_NUMBER(args[0]);
    Number amount = AS_NUMBER(args[1]);
    if (amount > 0)
        args[-1] = NUMBER_VAL(value << amount);
    else
        args[-1] = NUMBER_VAL(value >> (-amount));
    return true;
}

uint32_t rand32;

static bool randomNative(int argCount, Value* args) {
    rand32 ^= rand32 << 13;
    rand32 ^= rand32 >> 17;
    rand32 ^= rand32 << 5;
    args[-1] = NUMBER_VAL(rand32 & 0x3fffffff);
    return true;
}

static bool seedRandNative(int argCount, Value* args) {
    args[-1] = NUMBER_VAL(rand32);
    rand32 = AS_NUMBER(args[0]);
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
    vm.debug_log_gc = AS_BOOL(args[0]);
    args[-1] = args[0];
    return true;
}

static bool dbgStressNative(int argCount, Value* args) {
    vm.debug_stress_gc = AS_BOOL(args[0]);
    args[-1] = args[0];
    return true;
}

static bool dbgStatNative(int argCount, Value* args) {
    vm.debug_statistics = AS_BOOL(args[0]);
    args[-1] = args[0];
    return true;
}

#ifdef KIT68K

#include "monitor4x.h"
#define LOOPS_PER_MILLI 72

// Remember your old 80s home computers? 

static bool peekNative(int argCount, Value* args) {
    int32_t address = AS_NUMBER(args[0]);
    uint8_t byte = *((uint8_t*)address);
    args[-1] = NUMBER_VAL(byte);
    return true;
}

static bool pokeNative(int argCount, Value* args) {
    int32_t address = AS_NUMBER(args[0]);
    Number  byte = AS_NUMBER(args[1]);
    if (byte < 0 || byte > 255) {
        runtimeError("Value not in range 0-255.");
        return false;
    }
    *((uint8_t*)address) = byte & 0xff;
    args[-1] = NIL_VAL;
    return true;
}

static bool execNative(int argCount, Value* args) {
    typedef int sub0(void);
    typedef int sub1(int);
    typedef int sub2(int,int);
    typedef int sub3(int,int,int);

    int32_t address = AS_NUMBER(args[0]);
    int32_t result;

    switch (argCount) {
        case 1: result = ((sub0*) address)(); break;
        case 2: result = ((sub1*) address)(AS_NUMBER(args[1])); break;
        case 3: result = ((sub2*) address)(AS_NUMBER(args[1]),AS_NUMBER(args[2])); break;
        case 4: result = ((sub3*) address)(AS_NUMBER(args[1]),AS_NUMBER(args[2]),AS_NUMBER(args[3])); break;
    }

    args[-1] = NUMBER_VAL(result);
    return true;
}

static bool lcdClearNative(int argCount, Value* args) {
    lcd_clear();
    args[-1] = NIL_VAL;
    return true;
}

static bool lcdGotoNative(int argCount, Value* args) {
    lcd_goto(AS_NUMBER(args[0]), AS_NUMBER(args[1]));
    args[-1] = NIL_VAL;
    return true;
}

static bool lcdPutsNative(int argCount, Value* args) {
    lcd_puts(AS_CSTRING(args[0]));
    args[-1] = NIL_VAL;
    return true;
}

static bool lcdDefcharNative(int argCount, Value* args) {
    int udc = AS_NUMBER(args[0]);
    ObjList* pattern = AS_LIST(args[1]);
    char bitmap[8];
    Number byte;
    int i;

    if (udc < 0 || udc > 7) {
        runtimeError("UDC number out of range.");
        return false;
    }
    if (pattern->count != 8) {
        runtimeError("UDC bitmap must be 8 bytes.");
        return false;
    }
    for (i = 0; i < 8; i++) {
        if (IS_NUMBER(pattern->items[i])) {
            byte = AS_NUMBER(pattern->items[i]);
            if (byte < 0 || byte > 255) {
                runtimeError("Byte %d out of range.", i);
                return false;
            }
            bitmap[i] = (char)byte;
        } else {
            runtimeError("Number expected in UDC bitmap at %d.", i);
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
        args[-1] = NUMBER_VAL(code);
    return true;
}

static bool soundNative(int argCount, Value* args) {
    int delay = AS_NUMBER(args[0]);
    int len  = AS_NUMBER(args[1]);
    int loops = 34483;
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

static bool sleepNative(int argCount, Value* args) {
    int32_t millis = AS_NUMBER(args[0]);
    int32_t i, j;

    for (i = 0; i < millis; i++) {
        for (j = 0; j < LOOPS_PER_MILLI; j++)
            ;
    }
    args[-1] = NIL_VAL;
    return true;
}

static bool trapNative(int argCount, Value* args) {
    _trap(0);
    args[-1] = NIL_VAL;
    return true;
}

#endif

static bool clockNative(int argCount, Value* args) {
#ifdef KIT68K
    args[-1] = NUMBER_VAL(0); // not available
#else
    args[-1] = NUMBER_VAL(clock() * 1000 / CLOCKS_PER_SEC); // in milliseconds
#endif
    return true;
}

static bool gcNative(int argCount, Value* args) {
    collectGarbage(false);
    args[-1] = NUMBER_VAL(vm.bytesAllocated);
    return true;
}

static void defineNative(const char* name, const char* signature, NativeFn function) {
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(signature, function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void defineAllNatives(void) {
    defineNative("abs",         "N",    absNative);

    defineNative("asc",         "Sn",   ascNative);
    defineNative("chr",         "N",    chrNative);
    defineNative("dec",         "N",    decNative);
    defineNative("hex",         "N",    hexNative);
    defineNative("int",         "S",    intNative);
    defineNative("input",       "s",    inputNative);

    defineNative("bit_and",     "NN",   bitAndNative);
    defineNative("bit_or",      "NN",   bitOrNative);
    defineNative("bit_xor",     "NN",   bitXorNative);
    defineNative("bit_not",     "N",    bitNotNative);
    defineNative("bit_shift",   "NN",   bitShiftNative);

    defineNative("random",      "",     randomNative);
    defineNative("seed_rand",   "N",    seedRandNative);

    defineNative("length",      "Q",    lengthNative);
    defineNative("append",      "LA",   appendNative);
    defineNative("insert",      "LNA",  insertNative);
    defineNative("delete",      "LN",   deleteNative);
    defineNative("index",       "ALn",  indexNative);

    defineNative("slots",       "I",    slotsNative);
    defineNative("remove",      "IS",   removeNative);
    defineNative("globals",     "",     globalsNative);
    defineNative("type",        "A",    typeNative);
    defineNative("clock",       "",     clockNative);
    defineNative("gc",          "",     gcNative);

#ifdef KIT68K
    defineNative("peek",        "N",    peekNative);
    defineNative("poke",        "NN",   pokeNative);
    defineNative("exec",        "Nnnn", execNative);
    defineNative("trap",        "",     trapNative);

    defineNative("lcd_clear",   "",     lcdClearNative);
    defineNative("lcd_goto",    "NN",   lcdGotoNative);
    defineNative("lcd_puts",    "S",    lcdPutsNative);
    defineNative("lcd_defchar", "NL",   lcdDefcharNative);
    defineNative("keycode",     "",     keycodeNative);
    defineNative("sound",       "NN",   soundNative);
    defineNative("sleep",       "N",    sleepNative);
#endif

    defineNative("dbg_code",    "B",    dbgCodeNative);
    defineNative("dbg_trace",   "B",    dbgTraceNative);
    defineNative("dbg_gc",      "B",    dbgGcNative);
    defineNative("dbg_stress",  "B",    dbgStressNative);
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

