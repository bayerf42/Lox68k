#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "table.h"
#include "vm.h"

void initTable(Table* table) {
    table->count    = 0;
    table->capacity = 0;
    table->entries  = NULL;
}

void freeTable(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

static Entry* findEntry(Entry* entries, int capacity, Value key) {
    uint32_t index     = hashValue(key) & (capacity - 1);
    Entry*   tombstone = NULL;
    Entry*   entry;

    for (;;) {
        entry = &entries[index];
        if (IS_EMPTY(entry->key)) {
            if (IS_NIL(entry->value))
                return tombstone != NULL ? tombstone : entry; // Empty entry
            else {
                // We found a tombstone
                if (tombstone == NULL)
                    tombstone = entry;
            }
        } else if (valuesEqual(key, entry->key))
            return entry; //We found the key

        index = (index + 1) & (capacity - 1);
    }
    return NULL; // not reached
}

bool tableGet(Table* table, Value key, Value* value) {
    Entry* entry;

    if (table->count == 0)
        return false;

    entry = findEntry(table->entries, table->capacity, key);
    if (IS_EMPTY(entry->key))
        return false;

    *value = entry->value;
    return true;
}

static void adjustCapacity(Table* table, int capacity) {
    Entry*  entries = ALLOCATE(Entry, capacity);
    Entry*  entry;
    Entry*  dest;
    int16_t i;

    for (i = 0; i < capacity; i++) {
        entries[i].key   = EMPTY_VAL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (i = 0; i < table->capacity; i++) {
        entry = &table->entries[i];
        if (IS_EMPTY(entry->key))
            continue;
        dest        = findEntry(entries, capacity, entry->key);
        dest->key   = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries  = entries;
    table->capacity = capacity;
}

bool tableSet(Table* table, Value key, Value value) {
    Entry*  entry;
    bool    isNewKey;
    int16_t capacity = table->capacity;

    // Grow when load factor exceeds 0.75
    if (table->count + 1 > ((capacity + capacity + capacity) >> 2)) {
        capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    entry    = findEntry(table->entries, table->capacity, key);
    isNewKey = IS_EMPTY(entry->key);
    if (isNewKey && IS_NIL(entry->value))
        table->count++;

    entry->key   = key;
    entry->value = value;
    return isNewKey;
}

bool tableDelete(Table* table, Value key) {
    Entry* entry;

    if (table->count == 0)
        return false;

    entry = findEntry(table->entries, table->capacity, key);
    if (IS_EMPTY(entry->key))
        return false;

    // Place a tombstone in the entry.
    entry->key   = EMPTY_VAL;
    entry->value = BOOL_VAL(true);
    return true;
}

void tableAddAll(Table* from, Table* to) {
    int16_t i;
    Entry*  entry;

    for (i = 0; i < from->capacity; i++) {
        entry = &from->entries[i];
        if (!IS_EMPTY(entry->key))
            tableSet(to, entry->key, entry->value);
    }
}

void tableShrink(Table* table) {
  int16_t num_entries = 0;
  int16_t i, capacity;
  
  for (i = 0; i < table->capacity; i++)
      if (!IS_EMPTY(table->entries[i].key))
          num_entries++;

  // Find optimal capacity for load factor 0.75
  for (capacity = 8; num_entries > ((capacity + capacity + capacity) >> 2); capacity <<= 1)
      ;
  if (capacity < table->capacity) {
      if (vm.debug_log_gc & DBG_GC_STRINGS) 
          printf("GC shrink strings from %d to %d\n", table->capacity, capacity);
      adjustCapacity(table, capacity);
  }
}

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
    uint32_t   index;
    Entry*     entry;
    ObjString* string;

    if (table->count == 0)
        return NULL;

    index = hash & (table->capacity - 1);

    for (;;) {
        entry = &table->entries[index];
        if (IS_EMPTY(entry->key)) {
            // Stop if we find an empty non-tombstone entry.
            if (IS_NIL(entry->value))
                return NULL;
        } else {
            string = AS_STRING(entry->key); // keys will always be strings here!
            if (string->length == length &&
                string->hash   == hash   &&
                strncmp(string->chars, chars, length) == 0) {
                // we found duplicate string
                return string;
            }
        }
        index = (index + 1) & (table->capacity - 1);
    }
    return NULL; // not reached
}

void tableRemoveWhite(Table* table) {
    int16_t i;
    Entry*  entry;

    for (i = 0; i < table->capacity; i++) {
        entry = &table->entries[i];
        // keys either empty or ObjString*
        if (!IS_EMPTY(entry->key) && !(AS_OBJ(entry->key))->isMarked)
            tableDelete(table, entry->key);
    }
}

void markTable(Table* table) {
    int16_t i;
    Entry*  entry;

    for (i = 0; i < table->capacity; i++) {
        entry = &table->entries[i];
        markValue(entry->key);
        markValue(entry->value);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Hashtable iterators 
////////////////////////////////////////////////////////////////////////////////////////////////////

int firstIterator(Table* table) {
    int i;
    if (table->count > 0)
        for (i = 0; i < table->capacity; i++)
            if (!IS_EMPTY(table->entries[i].key))
                return i;
    return -1;
}

void nextIterator(ObjIterator* iter) {
    int16_t i;
    Table*  table = iter->table;
    if (iter->position >= 0) {
        for (i = iter->position + 1; i < table->capacity; i++)
            if (!IS_EMPTY(table->entries[i].key)) {
                iter->position = i;
                return;
            }
    }
    iter->position = -1;
}

bool isValidIterator(ObjIterator* iter) {
    bool valid = iter->position >= 0            &&
         iter->position < iter->table->capacity &&
         !IS_EMPTY(iter->table->entries[iter->position].key);
    return valid;
}

Value getIterator(ObjIterator* iter, bool wantKey) {
    // Valid iterator has already been checked
    Entry* entry = &iter->table->entries[iter->position];
    return wantKey ? entry->key : entry->value;
}

void setIterator(ObjIterator* iter, Value value) {
    // Valid iterator has already been checked
    Entry* entry = &iter->table->entries[iter->position];
    entry->value = value;
}
