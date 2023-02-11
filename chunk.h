#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,      // push constant value at index int0
    OP_NIL,           // push value nil
    OP_TRUE,          // push value true
    OP_FALSE,         // push value false
    OP_POP,           // pop TOS
    OP_GET_LOCAL,     // push local variable at index int0 
    OP_SET_LOCAL,     // update local variable at index int0 with TOS  
    OP_GET_GLOBAL,    // push global variable named str0
    OP_DEF_GLOBAL,    // create or update global variable named arg0 with TOS
    OP_SET_GLOBAL,    // update global variable named str0 with TOS
    OP_GET_UPVALUE,   // push upvalue at index int0
    OP_SET_UPVALUE,   // update upvalue at index int0 with TOS
    OP_GET_PROPERTY,  // push property named str0 of TOS
    OP_SET_PROPERTY,  // update property named str0 of TOS-1 with TOS
    OP_GET_SUPER,     // push method named str0 in superclass of TOS class
    OP_EQUAL,         // compare 2 TOS values for equality
    OP_LESS,          // compare 2 TOS values for less than
    OP_ADD,           // add 2 TOS values
    OP_SUB,           // subtract 2 TOS values 
    OP_MUL,           // multiply 2 TOS values
    OP_DIV,           // divide 2 TOS values
    OP_MOD,           // remainder of 2 TOS values 
    OP_NOT,           // invert logical value of TOS
    OP_NEG,           // invert sign of TOS 
    OP_PRINT,         // print TOS
    OP_PRINTLN,       // print TOS, print newline
    OP_JUMP,          // jump forwards  by arg0 bytes
    OP_JUMP_TRUE,     // jump forwards  by arg0 bytes if TOS is true,  pop TOS otherwise 
    OP_JUMP_FALSE,    // jump forwards  by arg0 bytes if TOS is false, pop TOS otherwise
    OP_LOOP,          // jump backwards by arg0 bytes
    OP_CALL,          // call a value with int0 arguments on stack
    OP_INVOKE,        // invoke method str0 with int1 arguments on stack
    OP_SUPER_INVOKE,  // invoke super method str0 with int1 arguments on stack 
    OP_CLOSURE,       // build a closure from template fun0 with variable number of upvalues
    OP_CLOSE_UPVALUE, // close one upvalue
    OP_RETURN,        // return from current closure, close all upvalues
    OP_CLASS,         // define a new class named arg0 and push it
    OP_INHERIT,       // copy all methods from superclass TOS-1 to class TOS
    OP_METHOD,        // use closure TOS as method named str0 in class TOS-1
    OP_LIST,          // push new list of int0 values from stack
    OP_GET_INDEX,     // get value at index TOS from indexable TOS-1
    OP_SET_INDEX,     // set value of index TOS-1 from indexable TOS-2 to TOS
    OP_GET_SLICE,     // get slice of slicable TOS-2 from TOS-1 upto TOS
    OP_SWAP,          // swap 2 TOS values
    OP_UNPACK,        // unpack TOS list onto stack, push its length plus TOS-1
    OP_VCALL,         // call a value with (int0 + TOS) arguments on stack
    OP_VINVOKE,       // invoke method (int0 + TOS) with arg1 arguments on stack
    OP_VSUPER_INVOKE, // invoke super method str0 with (int1 + TOS) arguments on stack
    OP_VLIST,         // push new list of (int0 + TOS) values from stack  
    OP_GET_ITVAL,     // push value of TOS iterator
    OP_SET_ITVAL,     // set value of TOS-1 iterator to TOS 
    OP_GET_ITKEY,     // push key of TOS iterator
} OpCode;


typedef struct {
    int16_t offset;
    int16_t line;
} LineStart;


typedef struct {
    int16_t    count;
    int16_t    capacity;
    uint8_t*   code;
    int16_t    lineCount;
    int16_t    lineCapacity;
    LineStart* lines;
    ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int  addConstant(Chunk* chunk, Value value);
int  getLine(Chunk* chunk, int offset);

#endif