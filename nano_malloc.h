#ifndef clox_nano_malloc_h
#define clox_nano_mamlloc_h

void  init_freelist(void);
void* nano_malloc(size_t s);
void  nano_free(void* free_p);

#endif