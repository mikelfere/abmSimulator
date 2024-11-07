#ifndef memoryBus_h
#define memoryBus_h

#include "memmng.h"
#include "symbolTable.h"
#include "vm.h"

typedef struct {
    int capacity;
    int count;
    int* altAddresses;
    int* upperBounds;
    int* lowerBounds;
    Value* values;
    Table globals;
    Table strings;
    Object* objects;
} Memory;

typedef struct {
    int socket;
    int coreID;
} CoreData;

typedef struct {
    Memory* memory;
    CoreData* cores;
} Bus;

// extern Memory memory;

void initBus();
void freeBus();
void initMemory(Memory* memory);
void freeMemory(Memory* memory);
int setGlobal(Memory* memory, String* key, int firstAddress);

#endif