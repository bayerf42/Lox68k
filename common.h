#ifndef clox_common_h
#define clox_common_h

#include <stddef.h>

#ifdef KIT68K

/////////////////////////////////////////////////////////////////
// Wichichote 68008 KIT / IDE68K C compiler specific definitions
/////////////////////////////////////////////////////////////////

typedef signed char    int8_t;
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef short          int16_t;
typedef int            int32_t;
typedef short          bool;


#define true       1
#define false      0

#define UINT8_MAX  0xff
#define UINT16_MAX 0xffff
#define INT32_MAX  0x7fffffff

extern void memmove(void *, void *, int);


typedef int Real;

#include "ffp_glue.h"


// Code at the very beginning of a function to check for stack overflow:
//
// cmp.l STACKPOINTER.W,A7
// bge   *+6
// jsr   __stackoverflow
//
// please adapt the definition of STACKPOINTER in the project file when the address
// of variable __stack in the assembler startup file changes, I haven't found a way
// to automate this... 

extern void _stackoverflow(void);
#define CHECK_STACKOVERFLOW \
  _word(0xbff8); _word(STACKPOINTER); \
  _word(0x6c06); \
  _stackoverflow();

#else

/////////////////////////////////////////////////////////////////
// Now for the more modern compilers Tiny C and GNU C
/////////////////////////////////////////////////////////////////

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

typedef double Real;

#include <math.h>

#define intToReal(x) (x)
#define realToInt(x) ((Int)(x))
#define neg(x)       (-(x))
#define add(x,y)     ((x)+(y))
#define sub(x,y)     ((x)-(y))
#define mul(x,y)     ((x)*(y))
#define div(x,y)     ((x)/(y))
#define less(x,y)    ((x)<(y))

#define CHECK_STACKOVERFLOW

#endif

/////////////////////////////////////////////////////////////////
// Common definitions
/////////////////////////////////////////////////////////////////

#define UINT8_COUNT (UINT8_MAX + 1)

#define HEAP_SIZE   65536
#define STACK_MAX    4096
#define INPUT_SIZE  16384
#define FRAMES_MAX    128
#define GRAY_MAX     1024

#endif