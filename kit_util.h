#ifndef clox_kit_util_h
#define clox_kit_util_h

void fix_memcpy(char* dest, const char* src, size_t size);
int  fix_memcmp(const char* a, const char* b, size_t size);
int  putstr(const char* str); // like puts, but without newline
int  loadROM(void);
int  loadSource(const char* name);

#endif