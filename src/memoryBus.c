#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "memoryBus.h"
#include "socket.h"

typedef struct {
    int socket;
    int coreID;
} CoreData;

Bus bus;
Memory memory;

pthread_mutex_t lock;

void initBus() {
    bus.memory = &memory;
    initMemory(bus.memory);
}

void freeBus() {
    freeMemory(bus.memory);
}

void initMemory(Memory* memory) {
    initTable(&memory->globals);
    initTable(&memory->strings);
    memory->count = 0;
    memory->capacity = 0;
    memory->altAddresses = NULL;
    memory->upperBounds = NULL;
    memory->lowerBounds = NULL;
    memory->values = NULL;
    memory->objects = NULL;
}

void freeMemory(Memory* memory) {
    freeTable(&memory->globals);
    freeTable(&memory->strings);
    freeObjects(memory->objects);
    reallocate(memory->values, sizeof(Value) * memory->capacity, 0);
    reallocate(memory->altAddresses, sizeof(int) * memory->capacity, 0);
    reallocate(memory->upperBounds, sizeof(int) * memory->capacity, 0);
    reallocate(memory->lowerBounds, sizeof(int) * memory->capacity, 0);
}

int setGlobal(Memory* memory, String* key) {
    // printf("Setting globals\n");
    int address = memory->count;
    bool isNew = tableSetAddress(&memory->globals, key, &address);
    // printf("After set address\n");
    // Reallocate arrays to match size of globals
    if (memory->globals.capacity != memory->capacity) {
        memory->altAddresses = (int*)reallocate(memory->altAddresses, sizeof(int) * memory->capacity, sizeof(int) * memory->globals.capacity);
        memory->upperBounds = (int*)reallocate(memory->upperBounds, sizeof(int) * memory->capacity, sizeof(int) * memory->globals.capacity);
        memory->lowerBounds = (int*)reallocate(memory->lowerBounds, sizeof(int) * memory->capacity, sizeof(int) * memory->globals.capacity);
        memory->values = (Value*)reallocate(memory->values, sizeof(Value) * memory->capacity, sizeof(Value) * memory->globals.capacity);
        memory->capacity = memory->globals.capacity;
    }
    printf("memory->count = %d --- address = %d ---- memory->capacity = %d\n", memory->count, address, memory->capacity);
    // If is not new entry set address to altAddresses array.
    if (!isNew) {
        // puts("altAddress for existing");
        memory->altAddresses[address] = memory->count;
        memory->altAddresses[memory->count] = address;
    } else {
        // puts("altAddress for new");
        memory->altAddresses[address] = -1;
        
    }
    // printf("Set altAddress\n");
    memory->values[memory->count] = (Value){NUM_VALUE, {.number = 0}};
    memory->count++;
    // printf("Set globals\n");
    return memory->count - 1;
}

void* handleCore(void* arg) {
    CoreData* core = (CoreData*)arg;
    char buffer[1024];
    int valread, i, j, dataCount, lastAddress;
    char name[256] = {0};
    String* key;
    Value value;

    printf("Handling core %d\n", core->coreID);

    while ((valread = read(core->socket, buffer, 1024)) > 0) {
        printf("Core %d sent: %s\n", core->coreID, buffer);
        i = 0, j = 0, dataCount = 0;
        // If adding to memory:
        if (memcmp(buffer, "add", 3) == 0) {
            // printf("In add\n");
            // Reminder for potential mutex use----------------------------------------------------
            i = 4;
            while (buffer[i] != '\0') {
                if (buffer[i] == ' ') {
                    // printf("Before copy key\n");
                    // puts(name);
                    key = copyString(&bus.memory->strings, bus.memory->objects, name, j);
                    // printf("Before setGlobal\n");
                    lastAddress = setGlobal(bus.memory, key);
                    j = 0;
                    dataCount++;
                } else {
                    name[j++] = buffer[i];
                }
                i++;
            }

            for(int i = lastAddress - (dataCount - 1); i <= lastAddress; i++) {
                bus.memory->upperBounds[i] = lastAddress;
                bus.memory->lowerBounds[i] = lastAddress - (dataCount - 1);
            }
            // printf("After add\n");
            // Reminder for potential mutex use----------------------------------------------------
        } else if (memcmp(buffer, "rad", 3) == 0) {
            // printf("In read address\n");
            i = 4, j = 0;
            printf("%d\n", buffer[i] != '\0');
            while (buffer[i] != '\0') {
                name[j] = buffer[i];
                i++;
                j++;
            }
            name[j] = '\0';
            puts(name);
            printf("%d\n",j);
            // printf("After while\n");
            key = copyString(&bus.memory->strings, bus.memory->objects, name, j);
            // printf("Copied String");
            int address = tableGetValue(&bus.memory->globals, key, &value);
            printf("address = %d\n", address);
            if (address == -1) {
                write(core->socket, "nfd", 4);
            } else {
                // printf("Sending\n");
                char num[1024];
                memset(num, '\0', sizeof(num));
                sprintf(num, "%d", address);
                if (write(core->socket, num, 1024) <= 0) {
                    perror("Could not write to core");
                    exit(-1);
                } 
                // printf("Written\n");
            }
            // printf("After read address\n");
        } else if (memcmp(buffer, "rda", 3) == 0) {
            i = 4;
            char caddress[256];
            memset(caddress, '\0', sizeof(caddress));
            while(buffer[i] != '\0') {
                caddress[j++] = buffer[i++];
            }
            int address = atoi(caddress);
            if (address >= bus.memory->count) {
                if (write(core->socket, "nfd", 4) <= 0) {
                    perror("Could not write to core");
                    exit(-1);
                }
            } else {
                char num[3][256];
                for (int k = 0; k < 3; k++) {
                    memset(num[k], '\0', sizeof(num[k]));
                }
                sprintf(num[0], "%d", bus.memory->altAddresses[address]);
                sprintf(num[1], "%d", bus.memory->upperBounds[address]);
                sprintf(num[2], "%d", bus.memory->lowerBounds[address]);
                j = 0;
                for (int k = 0; k < 3; k++) {
                    int i = 0;
                    while (num[k][i] != '\0') {
                        buffer[j++] = num[k][i++];
                    }
                    buffer[j++] = ' ';
                }
                buffer[j] = '\0';
                if (write(core->socket, buffer, 1024) <= 0) {
                    perror("Could not write to core");
                    exit(-1);
                }
            }
        } else if (memcmp(buffer, "rvl", 3) == 0) {
            i = 4;
            char caddress[256];
            memset(caddress, '\0', sizeof(caddress));
            while (buffer[i] != '\0') {
                caddress[j++] = buffer[i++];
            }
            int address = atoi(caddress);
            value = bus.memory->values[address];
            memset(buffer, '\0', sizeof(buffer));
            sprintf(buffer, "%d", value.as.number);
            if (write(core->socket, buffer, 1024) <= 0) {
                perror("Could not write to core");
                exit(-1);
            }
        } else if (memcmp(buffer, "wrv", 3) == 0){
            i = 4;
            char caddress[256], cval[256];
            memset(caddress, '\0', sizeof(caddress));
            bool afterAddress = false;
            while (buffer[i] != '\0') {
                if (buffer[i] == ' ') {
                    afterAddress = true;
                    j = 0;
                    i++;
                    continue;
                }
                if (!afterAddress) {
                    caddress[j++] = buffer[i++];
                } else {
                    cval[j++] = buffer[i++];
                }
            }
            int address = atoi(caddress);
            int value = atoi(cval);
            bus.memory->values[address] = (Value){NUM_VALUE, {.number = value}};
            if (bus.memory->altAddresses[address] != -1) {
                bus.memory->values[bus.memory->altAddresses[address]] = (Value){NUM_VALUE, {.number = value}};
            }
        } else {
            perror("Didn't receive a valid command");
            exit(-1);
        }
        // write(core->socket, buffer, valread);
    }
    close(core->socket);
    free(core);
    return NULL;
}

int main() {
    initBus();
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("Socket error");
        exit(-1);
    }
    
    struct sockaddr_in saddr;
    // To clear contents of saddr
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(PORT);

    if (bind(socket_fd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        perror("Bind error");
        exit(-1);
    }

    if (listen(socket_fd, 2) < 0) {
        perror("Listen error");
        exit(-1);
    }

    printf("Listening on port %d for cores...\n", PORT);
    int coreId = 0;
    // pthread_mutex_init(&lock, NULL);

    while (true) {
        struct sockaddr_in caddr;
        int len = sizeof(caddr);

        int core_fd = accept(socket_fd, (struct sockaddr*) &caddr, &len);
        if (core_fd < 0) {
            perror("Accept error");
            exit(-1);
        }

        CoreData* core = malloc(sizeof(CoreData));
        core->socket = core_fd;
        core->coreID = ++coreId;

        pthread_t threadId;
        pthread_create(&threadId, NULL, handleCore, core);
        pthread_detach(threadId);
    }
    freeBus();
    // pthread_mutex_destroy(&lock);
    return 0;
}