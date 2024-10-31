#ifndef memoryBus_h
#define memoryBus_h

#include "memmng.h"
#include "symbolTable.h"
#include "vm.h"

typedef struct {
    // int data;
    int capacity;
    int count;
    int* altAddresses;
    int* bounds;
    Table globals;
} Memory;

typedef struct {
    VM* core1;
    VM* core2;
    Memory* memory;
} Bus;

extern Memory memory;

// void initBus(Bus* bus);
// void freeBus(Bus* bus);
void initMemory();
void freeMemory();
int setGlobal(String* key, Value value);
bool getGlobal(String* key, Value* value);

#endif