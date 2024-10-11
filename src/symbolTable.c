#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>

#include "memmng.h"
#include "object.h"
#include "symbolTable.h"

#define TABLE_LOAD_FACTOR 0.75

void initTable(Table* table) {
    table->capacity = 0;
    table->count = 0;
    table->entries = NULL;
}

void freeTable(Table* table) {
    reallocate(table->entries, sizeof(Entry) * table->capacity, 0);
    initTable(table);
}

static Entry* findEntry(Entry* entries, int capacity, String* key) {
    uint32_t index = key->hash % capacity;
    Entry* tombstone = NULL;
    for (;;) {
        Entry* entry = &entries[index];
        if (entry->key  == key || entry->key == NULL) {
            return entry;
        }
        index = (index + 1) % capacity;
    }
} 

bool tableGet(Table* table, String* key, Value* value) {
    if (table->count == 0) {
        return false;
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) {
        return false;
    }

    *value = entry->value;
    return true;
}

static void adjustCapacity(Table* table, int capacity) {
    Entry* entries = (Entry*)reallocate(NULL, 0, sizeof(Entry) * capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value.as.number = 0;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) {
            continue;
        }
        Entry* destination = findEntry(entries, capacity, entry->key);
        destination->key = entry->key;
        destination->value = entry->value;
        table->count++;
    }

    reallocate(table->entries, sizeof(Entry) * table->capacity, 0);

    table->entries = entries;
    table->capacity = capacity;
}

bool tableSet(Table* table, String* key, Value value) {
    if (table->capacity * TABLE_LOAD_FACTOR < table->count + 1 ) {
        int capacity = table->capacity < 16 ? 16 : table->capacity * 2;
        adjustCapacity(table, capacity);
    }
    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey && entry->value.as.number == 0) {
        table->count++;
    }
    entry->key = key;
    entry->value = value;
    return isNewKey;
}

String* findString(Table* table, const char* characters, int length, \
                    uint32_t hash) {
    if (table->count == 0) {
        return NULL;
    }

    uint32_t index = hash % table->capacity;
    for (;;) {
        Entry* entry = &table->entries[index];
        if (entry->key == NULL) {
            return NULL;
        } else if (entry->key->length == length && 
                    entry->key->hash == hash && 
                    memcmp(entry->key->characaters, characters, length) == 0) {
            return entry->key;
        }
        index = (index + 1) % table->capacity;
    }
}