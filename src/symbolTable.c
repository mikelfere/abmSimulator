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
    uint32_t index = key->hash & (capacity - 1);
    for (;;) {
        Entry* entry = &entries[index];
        if (entry->key  == key || entry->key == NULL) {
            return entry;
        }
        index = (index + 1) % capacity;
    }
} 

int tableGetValue(Table* table, String* key, Value* value) {
    if (table->count == 0) {
        return -1;
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    int address = entry->address;
    if (entry->key == NULL) {
        // address == -1 if entry not set
        return address;
    }

    *value = entry->value;
    if (address >= 0) {
        return address;
    }
    // -2 will mean that entry refers only to local variables
    return -2;
}

String* tableGetKey(Table* table, int address) {
    // puts("In tableGetKey");
    if (address == -1) {
        return NULL;
    }   
    for (int i = 0; i < table->capacity; i++) {
        // printf("%d", i);
        if (table->entries[i].address == address) {
            // puts("Key found");
            return table->entries[i].key;
        }
    }
    // puts("Key not found");
    return NULL;
}

static void adjustCapacity(Table* table, int capacity) {
    Entry* entries = (Entry*)reallocate(NULL, 0, sizeof(Entry) * capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value.as.number = 0;
        entries[i].address = -1;
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
        destination->address = entry->address;
        table->count++;
    }

    reallocate(table->entries, sizeof(Entry) * table->capacity, 0);

    table->entries = entries;
    table->capacity = capacity;
}

bool tableSetValue(Table* table, String* key, Value value) {
    if (table->capacity * TABLE_LOAD_FACTOR < table->count + 1 ) {
        int capacity = table->capacity < 16 ? 16 : table->capacity * 2;
        adjustCapacity(table, capacity);
    }
    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey) {
        table->count++;
    }
    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableSetValueAddress(Table* table, String* key, Value value, int* address) {
    if (table->capacity * TABLE_LOAD_FACTOR < table->count + 1 ) {
        int capacity = table->capacity < 16 ? 16 : table->capacity * 2;
        adjustCapacity(table, capacity);
    }
    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey) {
        table->count++;
        entry->address = *address;   // Only change address if new Entry
    } else {
        *address = entry->address;
    }
    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableSetAddress(Table* table, String* key, int* address) {
    if (table->capacity * TABLE_LOAD_FACTOR < table->count + 1 ) {
        int capacity = table->capacity < 16 ? 16 : table->capacity * 2;
        adjustCapacity(table, capacity);
    }
    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey) {
        table->count++;
        entry->address = *address;   // Only change address if new Entry
    } else {
        *address = entry->address;
    }
    entry->key = key;
    return isNewKey;
}

bool tableChangeAddress(Table* table, String* key, int address) {
    if (table->capacity * TABLE_LOAD_FACTOR < table->count + 1 ) {
        int capacity = table->capacity < 16 ? 16 : table->capacity * 2;
        adjustCapacity(table, capacity);
    }
    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (!isNewKey) {
        entry->address = address;
    }
    return !isNewKey;
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
                    memcmp(entry->key->characters, characters, length) == 0) {
            return entry->key;
        }
        index = (index + 1) % table->capacity;
    }
}