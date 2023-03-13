#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct {
    Value key;
    Value value;
} Entry;

typedef struct {
    int16_t count;
    int16_t capacity;
    Entry*  entries;
} Table;

void       initTable(Table* table);
void       freeTable(Table* table);
bool       tableGet(Table* table, Value key, Value* value);
bool       tableSet(Table* table, Value key, Value value);
bool       tableDelete(Table* table, Value key);
void       tableAddAll(Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);
void       tableRemoveWhite(Table* table);
void       tableShrink(Table* table);
void       markTable(Table* table);

void       nextIterator(ObjIterator* iter);
bool       isValidIterator(ObjIterator* iter);
Value      getIterator(ObjIterator* iter, bool wantKey);
void       setIterator(ObjIterator* iter, Value value);

#endif