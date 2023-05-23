#ifndef clox_memory_h
#define clox_memory_h

#include "object.h"

#define DBG_GC_GENERAL  1  // log begin/end of GC and trigger from alloc
#define DBG_GC_ALLOC    2  // log allocation of an object
#define DBG_GC_FREE     4  // log de-allocation of an object
#define DBG_GC_MARK     8  // log each object marked during GC
#define DBG_GC_BLACK   16  // log each object blackened during GC
#define DBG_GC_STRINGS 32  // log shrinking of strings table
#define DBG_GC_STRESS  64  // force GC before each allocation

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) \
    reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define MIN_CAPACITY(len) \
    ((len) < 8 ? 8 : (len))

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), 0)

void* reallocate(void* pointer, size_t oldSize, size_t newSize);
void  markObject(Obj* object);
void  markValue(Value value);
void  collectGarbage(bool checkReclaim);
void  freeObjects(void);

void  fix_memcpy(char* dest, const char* src, size_t size);
int   fix_memcmp(const char* a, const char* b, size_t size);

extern char input_line[INPUT_SIZE];

#endif