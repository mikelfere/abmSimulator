#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cache.h"

void initCache(Cache* cache) {
    for (int i = 0; i < 3; i++) {
        cache->entries[i].address = -1;
        cache->entries[i].key = NULL;
        cache->entries[i].value = (Value){NUM_VALUE, {.number = 0}};
        cache->lastUsed[i] = -1;
        cache->states[i] = INVALID;
        // pthread_mutex_init(&cache->lineLock[i], NULL);
        for (int j = 0; j < 5; j++) {
            cache->addressData[i][j] = -1;
        }
    }
    cache->count = 0;
}

void freeCache(Cache* cache) {
    initCache(cache);
}
