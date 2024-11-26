#ifndef cache_h
#define cache_h

#include <pthread.h>

#include "object.h"
#include "symbolTable.h"

typedef enum {
    PROCESSOR_READ,
    PROCESSOR_WRITE,
} Action;

typedef enum {
    INVALID,
    SHARED,
    MODIFIED
} State;

typedef struct Cache{
    Entry entries[3];
    int addressData[3][5];
    State states[3];
    int lastUsed[3];
    // pthread_mutex_t lineLock[3];
    int count;
} Cache;

void initCache(Cache* cache);
void freeCache(Cache* cache);

#endif