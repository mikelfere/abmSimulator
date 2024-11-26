#ifndef memoryBus_h
#define memoryBus_h

#include <pthread.h>

#include "memmng.h"
#include "symbolTable.h"
#include "vm.h"

/**
 * struct Memory - Stores data belonging to the memory
 * @a: capacity -> int : stores the current memory capacity
 * @b: count -> int : stores the number of variables in memory
 * @c: altAddresses -> int* : stores an alternative address of an entry
 * @d: upperBounds -> int* : stores the end address of the block of entries
 * @e: lowerBounds -> int* : stores the beginning address of the block of entries
 * @f: values -> Value* : stores the Value stored in the address (index)
 * @g: globals -> Table : stores memory's variable
 * @h: strings -> Table : used for String interning
 * @i: objects -> Object* : stores linked list of all objects used in memory
 */
typedef struct {
    int capacity;
    int count;
    int* altAddresses;
    int* upperBounds;
    int* lowerBounds;
    State** state;
    Value* values;
    Table globals;
    Table strings;
    Object* objects;
} Memory;

/**
 * struct CoreData - Stores the socket and core ID of each core connected to the bus
 * @a: socket -> int : stores the socket used by the core
 * @b: coreID -> int : stores the ID of the core, can be 1 or 2 only
 */
typedef struct {
    int socket;
    int snoopSocket;
    int coreID;
    bool inUse;
} CoreData;

/**
 * struct Bus - Stores the data used by the bus
 * @a: memory -> Memory* : stores a pointer to the memory
 * @b: cores -> CoreData* : stores the array of cores
 */
typedef struct {
    Memory* memory;
    CoreData* cores;
    pthread_mutex_t lock;
} Bus;

/**
 * @brief Initializes Bus members
 * 
 */
void initBus();

/**
 * @brief Frees all data used by the bus members
 * 
 */
void freeBus();

/**
 * @brief Sets up all members of the given memory struct
 * 
 * @param memory 
 */
void initMemory(Memory* memory);

/**
 * @brief Frees all members of the given memory struct
 * 
 * @param memory 
 */
void freeMemory(Memory* memory);

/**
 * @brief Adds a new entry to memory, modifying its alternative address
 * if the entry was stored somewhere else also. Returns the new address
 * of the stored entry.
 * 
 * @param memory 
 * @param key 
 * @param firstAddress 
 * @return int 
 */
int setGlobal(Memory* memory, String* key, int firstAddress);

#endif