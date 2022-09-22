#ifndef clox_common_h
#define clox_common_h

#include <stddef.h>

#ifdef KIT68K

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

#include <stdbool.h>
#include <stdint.h>

#define CHECK_STACKOVERFLOW

#endif

#define DEDUPLICATE_CONSTANTS
#define UINT8_COUNT (UINT8_MAX + 1)

#ifdef KIT68K

// HEAP_SIZE, INPUT_SIZE, and STACK_MAX are set in project file, different for RAM and ROM version.

#else

#define HEAP_SIZE   65536
#define STACK_MAX    4096
#define INPUT_SIZE  65536

#endif

#define FRAMES_MAX  128
#define GRAY_MAX   1024

#endif