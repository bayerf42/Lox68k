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

#else

#include <stdbool.h>
#include <stdint.h>

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