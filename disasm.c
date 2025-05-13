#ifdef LOX_DBG

#include <stdio.h>
#include "disasm.h"
#include "object.h"

static int    offset;
static Chunk* chunk;

static void simpInst(const char* name) {
    // simple instruction, no extra bytes
    putstr(name);
    ++offset;
}

static void byteInst(const char* name) {
    // byte instruction, 1 extra byte, 0-255 local/upvalue index, small int const, argument count
    int arg = chunk->code[++offset];
    printf("%-9s %4d", name, arg);
    ++offset;
}

static void jumpInst(const char* name) {
    // jump instruction, 2 extra bytes, 0-65535 jump distance (big endian)
    int delta = chunk->code[++offset] << 8;
    delta    |= chunk->code[++offset];
    printf("%-9s %4d ; -> %d", name, delta, ++offset + (*name != 'L' ? delta : -delta));
}

static void cnstInst(const char* name) {
    // constant instruction, 1 extra byte, 0-255 index into constants table
    int constant = chunk->code[++offset];
    printf("%-9s %4d ; ", name, constant);
    printValue(chunk->constants.values[constant], PRTF_MACHINE | PRTF_COMPACT);
    ++offset;
}

static void invoInst(const char* name) {
    // invoke instruction, 2 extra bytes, first  0-255 index into constant table,
    //                                    second 0-255 argument count
    int constant = chunk->code[++offset];
    int argCount = chunk->code[++offset];
    printf("%-9s %4d ; ", name, constant);
    printValue(chunk->constants.values[constant], PRTF_MACHINE | PRTF_COMPACT);
    printf(" (%d args)", argCount);
    ++offset;
}

static void closInst(const char* name) {
    // closure instruction, 1 extra byte, 0-255 index into constants table, function stored there
    // determines additional extra bytes each describing an upvalues 
    int          constant = chunk->code[++offset];
    ObjFunction* function = AS_FUNCTION(chunk->constants.values[constant]);
    int          j, upvalue;
    printf("%-9s %4d ; ", name, constant);
    printValue(chunk->constants.values[constant], PRTF_MACHINE | PRTF_COMPACT);
    for (j = 0, ++offset; j < function->upvalueCount; j++, ++offset) {
        upvalue = chunk->code[offset];
        printf("\n%04d    |   %5s   %4d", offset,
               UV_ISLOC(upvalue) ? "LOCAL" : "UPVAL", UV_INDEX(upvalue));
    }
}

static void disassembleIntern(void) {
    int opcd = chunk->code[offset];
    int line = getLine(chunk, offset);

    printf("%04d ", offset);
    if (offset > 0 && line == getLine(chunk, offset - 1))
        putstr("   | ");
    else
        printf("%4d ", line);

    switch (opcd) {
        case OP_CONSTANT:      cnstInst("CONST");     break;
        case OP_INT:           byteInst("INT");       break;
        case OP_ZERO:          simpInst("ZERO");      break;
        case OP_NIL:           simpInst("NIL");       break;
        case OP_TRUE:          simpInst("TRUE");      break;
        case OP_FALSE:         simpInst("FALSE");     break;
        case OP_POP:           simpInst("POP");       break;
        case OP_SWAP:          simpInst("SWAP");      break;
        case OP_DUP:           simpInst("DUP");       break;
        case OP_GET_LOCAL:     byteInst("GET_LOC");   break;
        case OP_SET_LOCAL:     byteInst("SET_LOC");   break;
        case OP_GET_GLOBAL:    cnstInst("GET_GLOB");  break;
        case OP_DEF_GLOBAL:    cnstInst("DEF_GLOB");  break;
        case OP_SET_GLOBAL:    cnstInst("SET_GLOB");  break;
        case OP_GET_UPVALUE:   byteInst("GET_UPVAL"); break;
        case OP_SET_UPVALUE:   byteInst("SET_UPVAL"); break;
        case OP_GET_PROPERTY:  cnstInst("GET_PROP");  break;
        case OP_SET_PROPERTY:  cnstInst("SET_PROP");  break;
        case OP_GET_SUPER:     cnstInst("GET_SUPER"); break;
        case OP_EQUAL:         simpInst("EQUAL");     break;
        case OP_LESS:          simpInst("LESS");      break;
        case OP_ADD:           simpInst("ADD");       break;
        case OP_SUB:           simpInst("SUB");       break;
        case OP_MUL:           simpInst("MUL");       break;
        case OP_DIV:           simpInst("DIV");       break;
        case OP_MOD:           simpInst("MOD");       break;
        case OP_NOT:           simpInst("NOT");       break;
        case OP_PRINT:         simpInst("PRINT");     break;
        case OP_PRINTLN:       simpInst("PRINTLN");   break;
        case OP_PRINTQ:        simpInst("PRINTQ");    break;
        case OP_JUMP:          jumpInst("JUMP");      break;
        case OP_JUMP_OR:       jumpInst("JUMP_OR");   break;
        case OP_JUMP_AND:      jumpInst("JUMP_AND");  break;
        case OP_JUMP_TRUE:     jumpInst("JUMP_T");    break;
        case OP_JUMP_FALSE:    jumpInst("JUMP_F");    break;
        case OP_LOOP:          jumpInst("LOOP");      break;
        case OP_CALL:          byteInst("CALL");      break;
        case OP_CALL0:         simpInst("CALL0");     break;
        case OP_CALL1:         simpInst("CALL1");     break;
        case OP_CALL2:         simpInst("CALL2");     break;
        case OP_CALL_HAND:     simpInst("CALL_HAND"); break;
        case OP_CALL_BIND:     cnstInst("CALL_BIND"); break;
        case OP_INVOKE:        invoInst("INVOKE");    break;
        case OP_SUPER_INVOKE:  invoInst("SUP_INV");   break;
        case OP_CLOSURE:       closInst("CLOSURE");   break; 
        case OP_CLOSE_UPVALUE: simpInst("CLOSE_UPV"); break;
        case OP_RETURN:        simpInst("RET");       break;
        case OP_RETURN_NIL:    simpInst("RET_NIL");   break;
        case OP_CLASS:         cnstInst("CLASS");     break;
        case OP_INHERIT:       simpInst("INHERIT");   break;
        case OP_METHOD:        cnstInst("METHOD");    break;
        case OP_LIST:          byteInst("LIST");      break;
        case OP_GET_INDEX:     simpInst("GET_INDEX"); break;
        case OP_SET_INDEX:     simpInst("SET_INDEX"); break;
        case OP_GET_SLICE:     simpInst("GET_SLICE"); break;
        case OP_UNPACK:        simpInst("UNPACK");    break;
        case OP_VCALL:         byteInst("VCALL");     break;
        case OP_VINVOKE:       invoInst("VINVOKE");   break;
        case OP_VSUPER_INVOKE: invoInst("VSUP_INV");  break;
        case OP_VLIST:         byteInst("VLIST");     break;
        case OP_GET_ITVAL:     simpInst("GET_ITVAL"); break;
        case OP_SET_ITVAL:     simpInst("SET_ITVAL"); break;
        case OP_GET_ITKEY:     simpInst("GET_ITKEY"); break;
        default:
            printf("Unknown opcode %d", opcd);
            ++offset;
    }
    putstr("\n");
}

void disassembleChunk(Chunk* pChunk, const char* name) {
    chunk = pChunk;
    printf("== %s ==\n", name);
    for (offset = 0; offset < chunk->count;)
        disassembleIntern();
}

int disassembleInst(Chunk* pChunk, int pOffset) {
    chunk  = pChunk;
    offset = pOffset;
    disassembleIntern();
    return offset;
}

#endif