#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"


void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash & (capacity - 1);
    Entry* tombstone = NULL;
    Entry* entry;

    for (;;) {
        entry = &entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                // Empty entry
                return tombstone != NULL ? tombstone : entry;
            } else {
                // We found a tombstone
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (entry->key == key) {
            // We found the key
            return entry;
        }

        index = (index + 1) & (capacity - 1);
    }
    return NULL; // not reached
}

bool tableGet(Table* table, ObjString* key, Value* value) {
    Entry* entry;

    if (table->count == 0) return false;

    entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *value = entry->value;
    return true;
}

static void adjustCapacity(Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    Entry* entry;
    Entry* dest;
    int i;

    for (i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (i = 0; i < table->capacity; i++) {
        entry = &table->entries[i];
        if (entry->key == NULL) continue;
        dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool tableSet(Table* table, ObjString* key, Value value) {
    Entry* entry;
    bool isNewKey;
    int capacity;

    //if (table->count + 1 > ((table->capacity * 3) >> 2)) {
    if (table->count + 1 > (table->capacity >> 1)) {
        capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    entry = findEntry(table->entries, table->capacity, key);
    isNewKey = entry->key == NULL;
    if (isNewKey && IS_NIL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableDelete(Table* table, ObjString* key) {
    Entry* entry;

    if (table->count == 0) return false;

    entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    // Place a tombstone in the entry.
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

void tableAddAll(Table* from, Table* to) {
    int i;
    Entry* entry;

    for (i = 0; i < from->capacity; i++) {
        entry = &from->entries[i];
        if (entry->key != NULL) {
            tableSet(to, entry->key, entry->value);
        }
    }
}

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
    uint32_t index;
    Entry* entry;

    if (table->count == 0) return NULL;

    index = hash & (table->capacity - 1);

    for (;;) {
        entry = &table->entries[index];
        if (entry->key == NULL) {
            // Stop if we find an empty non-tombstone entry.
            if (IS_NIL(entry->value)) return NULL;
        } else if (entry->key->length == length &&
                entry->key->hash == hash &&
                strncmp(entry->key->chars, chars, length) == 0) {
            // we found duplicate string
            return entry->key;
        }
        index = (index + 1) & (table->capacity - 1);
    }
    return NULL; // not reached
}

void tableRemoveWhite(Table* table) {
    int i;
    Entry* entry;

    for (i = 0; i < table->capacity; i++) {
        entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->isMarked) {
            tableDelete(table, entry->key);
        }
    }
}

void markTable(Table* table) {
    int i;
    Entry* entry;

    for (i = 0; i < table->capacity; i++) {
        entry = &table->entries[i];
        markObject((Obj*)entry->key);
        markValue(entry->value);
    }
}