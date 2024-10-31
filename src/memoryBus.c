#include "memoryBus.h"

Memory memory;

void initMemory() {
    initTable(&memory.globals);
    memory.count = 0;
    memory.capacity = 0;
    memory.altAddresses = NULL;
    memory.bounds = NULL;
}

void freeMemory() {
    freeTable(&memory.globals);
    reallocate(memory.altAddresses, sizeof(int) * memory.capacity, 0);
    reallocate(memory.bounds, sizeof(int) * memory.capacity, 0);
}

int setGlobal(String* key, Value value) {
    int newAddress = memory.count;
    bool isNew = tableSetValueAddress(&memory.globals, key, value, &newAddress);
    // Reallocate arrays to match size of globals
    if (memory.globals.capacity != memory.capacity) {
        reallocate(memory.altAddresses, sizeof(int) * memory.capacity, sizeof(int) * memory.globals.capacity);
        reallocate(memory.bounds, sizeof(int) * memory.capacity, sizeof(int) * memory.globals.capacity);
    }
    // If is not new entry set newAddress to altAddresses array.
    if (!isNew) {
        memory.altAddresses[newAddress] = memory.count;
    }
    memory.count++;
    return memory.count - 1;
}

bool getGlobal(String* key, Value* value) {
    return tableGetValue(&memory.globals, key, value);
}