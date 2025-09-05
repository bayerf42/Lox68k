#ifndef clox_kit_util_h
#define clox_kit_util_h

void mem_copy(char* dest, const char* src, size_t size);
void mem_clear(char* dest, size_t size);
int  mem_not_eq(const char* a, const char* b, size_t size);
int  putstr(const char* str); // like puts, but without newline
int  loadROM(void);
int  loadSource(const char* name);

#endif