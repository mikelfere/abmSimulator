#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "memoryBus.h"
#include "socket.h"

Bus bus;
Memory memory;

void initBus() {
    bus.memory = &memory;
    bus.cores = reallocate(NULL, 0, 2 * sizeof(CoreData));
    initMemory(bus.memory);
    pthread_mutex_init(&bus.lock, NULL);
    bus.cores[0].inUse = false;
    bus.cores[1].inUse = false;
}

void freeBus() {
    reallocate(bus.cores, 2 * sizeof(CoreData), 0);
    freeMemory(bus.memory);
    pthread_mutex_destroy(&bus.lock);
    bus.cores[0].inUse = false;
    bus.cores[1].inUse = false;
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
    memory->state = reallocate(memory->state, 0, 2 * sizeof(State*));
}

void freeMemory(Memory* memory) {
    freeTable(&memory->globals);
    freeTable(&memory->strings);
    freeObjects(memory->objects);
    reallocate(memory->values, sizeof(Value) * memory->capacity, 0);
    reallocate(memory->altAddresses, sizeof(int) * memory->capacity, 0);
    reallocate(memory->upperBounds, sizeof(int) * memory->capacity, 0);
    reallocate(memory->lowerBounds, sizeof(int) * memory->capacity, 0);
    reallocate(memory->state[0], sizeof(State) * memory->capacity, 0);
    reallocate(memory->state[1], sizeof(State) * memory->capacity, 0);
    reallocate(memory->state, sizeof(State) * 2, 0);
}

int setGlobal(Memory* memory, String* key, int firstAddress) {
    int address = memory->count;
    tableSetAddress(&memory->globals, key, &address);
    // Reallocate arrays to match size of globals
    if (memory->globals.capacity != memory->capacity) {
        memory->altAddresses = (int*)reallocate(memory->altAddresses, sizeof(int) * \
            memory->capacity, sizeof(int) * memory->globals.capacity);
        memory->upperBounds = (int*)reallocate(memory->upperBounds, sizeof(int) * \
            memory->capacity, sizeof(int) * memory->globals.capacity);
        memory->lowerBounds = (int*)reallocate(memory->lowerBounds, sizeof(int) * \
            memory->capacity, sizeof(int) * memory->globals.capacity);
        memory->values = (Value*)reallocate(memory->values, sizeof(Value) * \
            memory->capacity, sizeof(Value) * memory->globals.capacity);
        for (int i = 0; i < 2; i++) {
            memory->state[i] = (State*)reallocate(memory->state[i], sizeof(State) * \
                memory->capacity, sizeof(State) * memory->globals.capacity);
        }
        memory->capacity = memory->globals.capacity;
    }
    // If is not new entry set address to altAddresses array.
    if (firstAddress != -1) {
        memory->altAddresses[address] = memory->count;
        memory->altAddresses[memory->count] = address;
        memory->values[memory->count] = memory->values[address];
    } else {
        memory->altAddresses[address] = -1;
        memory->values[memory->count] = (Value){NUM_VALUE, {.number = 0}};
    }
    // printf("memory->count = %d --- address = %d ---- alt \
    // address = %d\n", memory->count, address, bus.memory->altAddresses[address]);
    memory->count++;
    return memory->count - 1;
}



void invalidate(int socket_fd, Value id) {
    char buffer[1024] = "0 ";
    int i = 0;
    if (id.type == OBJ_VALUE) {     // Invalidate by name
        String* name = (String*)id.as.obj;
        for (i = 0; i < name->length && i < 1021; i++) {
            buffer[i + 2] = name->characters[i];
        }
        buffer[i + 2] = '\0';
    } else {                        // Invalidate by address
        buffer[1] = 'a';
        buffer[2] = ' ';
        char temp[64];
        memset(temp, '\0', sizeof(temp));
        sprintf(temp, "%d", id.as.number);
        for (i = 0; temp[i] != '\0'; i++) {
            buffer[i + 3] = temp[i];
        }
        buffer[i + 3] = '\0';
    }
   
    if (write(socket_fd, buffer, 1024) <= 0) {
        perror("Could not write to bus");
        exit(-1);
    }
    // puts(buffer);
}


/**
 * @brief Handles all requests coming in from a core
 * Requests could be: 0-Invalidate, 1-Bus_Read, 2-Bus_Read_X, 3-Write_Back,
 * add-Add to memory
 * 
 * @param arg 
 * @return void* 
 */
void* handleCore(void* arg) {
    CoreData* core = (CoreData*)arg;
    core->inUse = true;
    char buffer[1024], tempBuffer[1024];
    int valread, i, j, dataCount, lastAddress, address, prevAddress = -1;
    char name[1024] = {0};
    String *key, *prevKey;
    Value value;

    // printf("Handling core %d\n", core->coreID);
    while ((valread = read(core->socket, buffer, 1024)) > 0) {
        // printf("Core %d sent: %s\n", core->coreID, buffer);
        pthread_mutex_lock(&bus.lock);
        // puts("Bus acquired lock");
        // printf("Core %d sent: %s\n", core->coreID, buffer);

        // pthread_mutex_lock(&bus.lock[core->coreID -1]);
        // while (!bus.running[core->coreID - 1]) {
        //     pthread_cond_wait(&bus.cond[core->coreID - 1], &bus.lock[core->coreID - 1]);
        // }
        // bus.running[core->coreID - 1] = 0;
        // pthread_mutex_unlock(&bus.lock[core->coreID - 1]);

        // for (int i = 0; i < memory.count; i++) {
        //     printf("memory->count = %d --- address = %d ---- alt address = %d ---- state1: %d ---- state2: %d\n", \
        //             i, 0 , bus.memory->altAddresses[i], bus.memory->state[0][i], bus.memory->state[1][i]);
        // }
        i = 0, j = 0, dataCount = 0;
        // If adding from data segment
        if (memcmp(buffer, "add", 3) == 0) {
            // printf("Core %d adds\n", core->coreID);
            i = 4;
            // To read variable names
            while (buffer[i] != '\0') {
                // Do for each variable name read
                if (buffer[i] == ' ') {
                    key = copyString(&bus.memory->strings, bus.memory->objects, name, j);
                    address = tableGetValue(&bus.memory->globals, key, &value);
                    // If new entry
                    if (address == -1) {
                        // If previous entry was defined in another program, we need to modify the data count and bounds if two sequential programs use interlapping data segments
                        if (prevAddress != -1 && prevAddress != memory.count - 1 && \
                                memory.upperBounds[prevAddress] == prevAddress) {
                            // puts("IIIIIIIIIIIIIIIIIIIIIIIII");
                            setGlobal(bus.memory, prevKey, prevAddress);
                            if(bus.cores[2 - core->coreID].inUse) {
                                invalidate(bus.cores[2 - core->coreID].snoopSocket, (Value){OBJ_VALUE, {.number = prevAddress}});
                            }
                            dataCount++;
                            // So that we don't keep adding the previous key on each iteration
                            prevAddress = -1;   
                        } else if (prevAddress != -1 && prevAddress == memory.count - 1 && \
                                    memory.upperBounds[prevAddress] == prevAddress) {
                            // puts("HHHHHHHHHHHHHHHHHHHHHHHHH");
                            if(bus.cores[2 - core->coreID].inUse) {
                                invalidate(bus.cores[2 - core->coreID].snoopSocket, (Value){OBJ_VALUE, {.number = prevAddress}});
                            }
                            dataCount += prevAddress + 1;
                            // So that we don't keep adding the previous key on each iteration
                            prevAddress = -1;
                        }
                        lastAddress = setGlobal(bus.memory, key, -1);
                        dataCount++;
                    } else {                // Entry already defined
                        if (address < memory.upperBounds[address] && prevAddress == -1 && dataCount > 0) {
                            // puts("EEEEEEEEEEEEEEEEEEEEEEEEEE");
                            lastAddress = setGlobal(bus.memory, key, address);
                            dataCount++;
                        }
                        prevKey = key;
                        prevAddress = address;
                    }
                    j = 0;
                } else {    // Read the actual name
                    name[j++] = buffer[i];
                }
                i++;
            }

            for(int i = lastAddress - (dataCount - 1); i <= lastAddress; i++) {
                bus.memory->upperBounds[i] = lastAddress;
                bus.memory->lowerBounds[i] = lastAddress - (dataCount - 1);
            }
            
            // for (int i = 0; i < memory.count; i++) {
            //     printf("memory->count = %d --- address = %d ---- alt address = %d\n", 
            //             i, 0 , bus.memory->altAddresses[i]);
            // }
            // printf("Core %d added\n", core->coreID);
    
        } else if (memcmp(buffer, "0", 1) == 0) {
            // printf("Core %d invalidates\n", core->coreID);
            if (memcmp(buffer + 1, "a", 1) == 0) {     // if by address
                i = 3; j = 0;
                char temp[64];
                memset(temp, '\0', sizeof(temp));
                while (buffer[i] != '\0') {
                    temp[j] = buffer[i];
                    i++;
                    j++;
                }
                // sprintf(temp, "%d", address);
                address = atoi(temp);
                key = tableGetKey(&bus.memory->globals, address);
                if (key == NULL) {
                    address = bus.memory->altAddresses[address];
                    key = tableGetKey(&bus.memory->globals, address);
                }
            } else {                                    // if by name
                i = 2, j = 0;
                while (buffer[i] != '\0') {
                    name[j] = buffer[i];
                    i++;
                    j++;
                }
                name[j] = '\0';
                key = copyString(&bus.memory->strings, bus.memory->objects, name, j);
                address = tableGetValue(&bus.memory->globals, key, &value);
            }
            bus.memory->state[core->coreID - 1][address] = MODIFIED;
            memset(buffer, '\0', sizeof(buffer));
            if (bus.cores[2 - core->coreID].inUse) {
                invalidate(bus.cores[2 - core->coreID].snoopSocket, (Value){OBJ_VALUE, {.obj = (Object*)key}});
                bus.memory->state[2 - core->coreID][address] = INVALID;
            }
            // printf("Core %d invalidate ends\n", core->coreID);
        } else if (memcmp(buffer, "1", 1) == 0) {
            // printf("Core %d reads\n", core->coreID);
            // puts("Getting key/address");
            if (memcmp(buffer + 1, "a", 1) == 0) {     // if by address
                i = 3; j = 0;
                char temp[64];
                memset(temp, '\0', sizeof(temp));
                while (buffer[i] != '\0') {
                    temp[j] = buffer[i];
                    i++;
                    j++;
                }
                // sprintf(address, "%d", temp);
                address = atoi(temp);
                key = tableGetKey(&bus.memory->globals, address);
                if (key == NULL) {
                    address = bus.memory->altAddresses[address];
                    key = tableGetKey(&bus.memory->globals, address);
                }
                // puts("Got key");
                memset(buffer, '\0', sizeof(buffer));
                for (int i = 0; i < key->length; i++) {
                    buffer[i] = key->characters[i];
                }
                // puts("Sending key back");
                if (write(core->socket, buffer, 1024) <= 0) {
                    perror("Could not write to core");
                    exit(-1);
                }
                // puts(buffer);
            } else {            
                i = 2, j = 0;
                while (buffer[i] != '\0') {
                    name[j] = buffer[i];
                    i++;
                    j++; 
                }
                name[j] = '\0';
                key = copyString(&bus.memory->strings, bus.memory->objects, name, j);
                address = tableGetValue(&bus.memory->globals, key, &value);
            }
            // puts("Got key/address");
            // Clean buffer
            memset(buffer, '\0', sizeof(buffer));
            // If not defined in memory
            if (address == -1) {
                // puts("Not found");
                if (write(core->socket, "nfd", 4) <= 0) {
                    perror("Could not write to core");
                    exit(-1);
                }
            } else {       // If variable was found
                // puts("Found");
                // Set state to shared on processor
                // If INVALID set to shared because PrRd was received, else no change
                if (bus.memory->state[core->coreID - 1][address] == INVALID) {
                    bus.memory->state[core->coreID - 1][address] = SHARED;
                }
                // If other processor is in modified state and running, write back and change to shared
                if (bus.cores[2 - core->coreID].inUse && bus.memory->state[2 - core->coreID][address] == MODIFIED) {
                    memset(tempBuffer, '\0', sizeof(tempBuffer));
                    tempBuffer[0] = '3', tempBuffer[1] = ' ';
                    for (i = 0; i < key->length && i < 1021; i++) {
                        tempBuffer[i + 2] = key->characters[i];
                    }
                    tempBuffer[i + 2] = '\0';
                    // puts("Writing to other snoop");
                    if (write(bus.cores[2 - core->coreID].snoopSocket, tempBuffer, 1024) <= 0) {
                        perror("Could not write to core");
                        exit(-1);
                    }
                    // puts("Reading from snoop");
                    memset(tempBuffer, '\0', sizeof(tempBuffer));
                    if (read(bus.cores[2 - core->coreID].snoopSocket, tempBuffer, sizeof(tempBuffer)) <= 0) {
                        perror("Could not read from core");
                        exit(-1);
                    }
                    // printf("Read %s from core\n", tempBuffer);
                    bus.memory->values[address] = (Value){NUM_VALUE, {.number = atoi(tempBuffer)}};
                    bus.memory->state[2 - core->coreID][address] = SHARED;
                }
                char temp[64]; 
                // Add address to temp
                memset(temp, '\0', sizeof(temp));
                sprintf(temp, "%d", address);
                i = 0, j = 0;
                while (temp[i] != '\0') {
                    buffer[j++] = temp[i++];
                }
                buffer[j++] = ' ';
                // Add value to temp
                memset(temp, '\0', sizeof(temp));
                value = bus.memory->values[address];
                sprintf(temp, "%d", value.as.number);
                i = 0;
                while (temp[i] != '\0') {
                    buffer[j++] = temp[i++];
                }
                buffer[j++] = ' ';
                // Add upper bound to buffer
                memset(temp, '\0', sizeof(temp));
                sprintf(temp, "%d", bus.memory->upperBounds[address]);
                i = 0;
                while (temp[i] != '\0') {
                    buffer[j++] = temp[i++];
                }
                buffer[j++] = ' ';
                // Add lower bound to buffer
                memset(temp, '\0', sizeof(temp));
                sprintf(temp, "%d", bus.memory->lowerBounds[address]);
                i = 0;
                while (temp[i] != '\0') {
                    buffer[j++] = temp[i++];
                }
                buffer[j++] = ' ';
                // Add alternative address to buffer
                memset(temp, '\0', sizeof(temp));
                sprintf(temp, "%d", bus.memory->altAddresses[address]);
                i = 0;
                while (temp[i] != '\0') {
                    buffer[j++] = temp[i++];
                }
                // If another address exists for this location
                if (bus.memory->altAddresses[address] != -1) {
                    buffer[j++] = ' ';
                    // Add upper bound of the other address
                    memset(temp, '\0', sizeof(temp));
                    sprintf(temp, "%d", bus.memory->upperBounds[bus.memory->altAddresses[address]]);
                    i = 0;
                    while (temp[i] != '\0') {
                        buffer[j++] = temp[i++];
                    }
                    buffer[j++] = ' ';
                    // Add lower bound of the other address
                    memset(temp, '\0', sizeof(temp));
                    sprintf(temp, "%d", bus.memory->lowerBounds[bus.memory->altAddresses[address]]);
                    i = 0;
                    while (temp[i] != '\0') {
                        buffer[j++] = temp[i++];
                    }
                }
                // puts("Sent back");
                buffer[j] = '\0';
                if (write(core->socket, buffer, 1024) <= 0) {
                    perror("Could not write to core");
                    exit(-1);
                }
                // puts(buffer);
            }
            // printf("Core %d read end\n", core->coreID);
        } else if (memcmp(buffer, "2", 1) == 0) {
            // printf("Core %d read X\n", core->coreID);
            if (memcmp(buffer + 1, "a", 1) == 0) {     // if by address
                i = 3; j = 0;
                char temp[64];
                memset(temp, '\0', sizeof(temp));
                while (buffer[i] != '\0') {
                    temp[j] = buffer[i];
                    i++;
                    j++;
                }
                // sprintf(address, "%d", temp);
                address = atoi(temp);
                key = tableGetKey(&bus.memory->globals, address);
                if (key == NULL) {
                    address = bus.memory->altAddresses[address];
                    key = tableGetKey(&bus.memory->globals, address);
                }
                memset(buffer, '\0', sizeof(buffer));
                for (int i = 0; i < key->length; i++) {
                    buffer[i] = key->characters[i];
                }
                if (write(core->socket, buffer, 1024) <= 0) {
                    perror("Could not write to core");
                    exit(-1);
                }
                // puts(buffer);
            } else {
                i = 2, j = 0;
                while (buffer[i] != '\0') {
                    name[j] = buffer[i];
                    i++;
                    j++; 
                }
                name[j] = '\0';
                key = copyString(&bus.memory->strings, bus.memory->objects, name, j);
                address = tableGetValue(&bus.memory->globals, key, &value);
            }
            // Clean buffer
            memset(buffer, '\0', sizeof(buffer));
            // If not defined in memory
            if (address == -1) {
                if (write(core->socket, "nfd", 4) <= 0) {
                    perror("Could not write to core");
                    exit(-1);
                }
            } else {       // If variable was found
                // Set state to modified on processor if not already
                if (bus.memory->state[core->coreID - 1][address] != MODIFIED) {
                    bus.memory->state[core->coreID - 1][address] = MODIFIED;
                }
                // If other processor line is not invalid
                if (bus.cores[2 - core->coreID].inUse && bus.memory->state[2 - core->coreID][address] != INVALID) {
                    memset(tempBuffer, '\0', sizeof(tempBuffer));
                    if (bus.memory->state[2 - core->coreID][address] == MODIFIED) {  // Write-back for modified 
                        tempBuffer[0] = '3', tempBuffer[1] = ' ';
                        for (i = 0; i < key->length && i < 1021; i++) {
                            tempBuffer[i + 2] = key->characters[i];
                        }
                        tempBuffer[i + 2] = '\0';
                        
                        // Use a second mutex before reading--------------------------------------------------------------------------
                        if (write(bus.cores[2 - core->coreID].snoopSocket, tempBuffer, 1024) <= 0) {
                            perror("Could not write to core");
                            exit(-1);
                        }
                        // puts(tempBuffer);
                        memset(tempBuffer, '\0', sizeof(tempBuffer));
                        if (read(bus.cores[2 - core->coreID].snoopSocket, tempBuffer, sizeof(tempBuffer)) <= 0) {
                            perror("Could not read from core");
                            exit(-1);
                        }
                        // printf("Read %s from core\n", tempBuffer);
                        bus.memory->values[address] = (Value){NUM_VALUE, {.number = atoi(tempBuffer)}};
                    } 
                    // Invalidate for shared -- always send for BusReadX
                    memset(tempBuffer, '\0', sizeof(tempBuffer));
                    tempBuffer[0] = '0', tempBuffer[1] = ' ';
                    for (i = 0; i < key->length && i < 1021; i++) {
                        tempBuffer[i + 2] = key->characters[i];
                    }
                    tempBuffer[i + 2] = '\0';
                    if (write(bus.cores[2 - core->coreID].snoopSocket, tempBuffer, 1024) <= 0) {
                        perror("Could not write to core");
                        exit(-1);
                    }
                    // puts(tempBuffer);
                    bus.memory->state[2 - core->coreID][address] = INVALID;
                }
                char temp[64]; 
                // Add address to temp
                memset(temp, '\0', sizeof(temp));
                sprintf(temp, "%d", address);
                i = 0, j = 0;
                while (temp[i] != '\0') {
                    buffer[j++] = temp[i++];
                }
                buffer[j++] = ' ';
                // Add value to temp
                memset(temp, '\0', sizeof(temp));
                value = bus.memory->values[address];
                sprintf(temp, "%d", value.as.number);
                i = 0;
                while (temp[i] != '\0') {
                    buffer[j++] = temp[i++];
                }
                buffer[j++] = ' ';
                // Add upper bound to buffer
                memset(temp, '\0', sizeof(temp));
                sprintf(temp, "%d", bus.memory->upperBounds[address]);
                i = 0;
                while (temp[i] != '\0') {
                    buffer[j++] = temp[i++];
                }
                buffer[j++] = ' ';
                // Add lower bound to buffer
                memset(temp, '\0', sizeof(temp));
                sprintf(temp, "%d", bus.memory->lowerBounds[address]);
                i = 0;
                while (temp[i] != '\0') {
                    buffer[j++] = temp[i++];
                }
                buffer[j++] = ' ';
                // Add alternative address to buffer
                memset(temp, '\0', sizeof(temp));
                sprintf(temp, "%d", bus.memory->altAddresses[address]);
                i = 0;
                while (temp[i] != '\0') {
                    buffer[j++] = temp[i++];
                }
                // If another address exists for this location
                if (bus.memory->altAddresses[address] != -1) {
                    buffer[j++] = ' ';
                    // Add upper bound of the other address
                    memset(temp, '\0', sizeof(temp));
                    sprintf(temp, "%d", bus.memory->upperBounds[bus.memory->altAddresses[address]]);
                    i = 0;
                    while (temp[i] != '\0') {
                        buffer[j++] = temp[i++];
                    }
                    buffer[j++] = ' ';
                    // Add lower bound of the other address
                    memset(temp, '\0', sizeof(temp));
                    sprintf(temp, "%d", bus.memory->lowerBounds[bus.memory->altAddresses[address]]);
                    i = 0;
                    while (temp[i] != '\0') {
                        buffer[j++] = temp[i++];
                    }
                }
                buffer[j] = '\0';
                if (write(core->socket, buffer, 1024) <= 0) {
                    perror("Could not write to core");
                    exit(-1);
                }
                // puts(buffer);
                // printf("Core %d read X end\n", core->coreID);
            }
        } else if (memcmp(buffer, "3", 1) == 0) {
            // printf("Core %d writes back\n", core->coreID);
            i = 2, j = 0;
            char temp[64];
            while (buffer[i] != ' ') {
                temp[j++] = buffer[i++];
            }
            temp[j] = '\0';
            address = atoi(temp);
            i++, j = 0;
            while (buffer[i] != '\0') {
                temp[j++] = buffer[i++];
            }
            temp[j] = '\0';
            int val = atoi(temp);
            bus.memory->values[address] = (Value){NUM_VALUE, {.number = val}};
            // Invalidate for both processors
            if (bus.cores[2 - core->coreID].inUse && bus.memory->state[2-core->coreID] != INVALID) {
                invalidate(bus.cores[2 - core->coreID].snoopSocket, (Value){NUM_VALUE, {.number = address}});
            }
            bus.memory->state[core->coreID - 1][address] = INVALID;
            bus.memory->state[2 - core->coreID][address] = INVALID;
            if (bus.memory->altAddresses[address] != -1) {
                bus.memory->values[bus.memory->altAddresses[address]] = (Value){NUM_VALUE, {.number = val}};
                bus.memory->state[core->coreID - 1][bus.memory->altAddresses[address]] = INVALID;
                bus.memory->state[2 - core->coreID][bus.memory->altAddresses[address]] = INVALID;
            }
            // printf("Core %d write back ends\n", core->coreID);
        } else if(memcmp(buffer, "ret", 3) == 0) {
            // printf("Bus released lock %d\n", core->coreID);
            pthread_mutex_unlock(&bus.lock);
            break;
        } else {
            perror("Didn't receive a valid command");
            exit(-1);
        }
        // printf("Bus released lock %d\n", core->coreID);
        pthread_mutex_unlock(&bus.lock);
        
        // pthread_mutex_lock(&bus.lock[2 - core->coreID]);
        // bus.running[2 - core->coreID] = 1;
        // pthread_cond_signal(&bus.cond[2 - core->coreID]);
        // pthread_mutex_unlock(&bus.lock[2 - core->coreID]);
    }
    // pthread_mutex_unlock(&bus.lock[0]);
    core->inUse = false;
    close(core->socket);
    // pthread_exit(NULL);
    // make all states invalid --------------------------------------------
    // free(core);
    return NULL;
}

int main() {
    initBus();
    
    // Setting up the socket to listen for cores
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

    // printf("Listening on port %d for cores...\n", PORT);
    int coreId = 0;
    while (true) {
        while (true) {
            if (bus.cores[coreId % 2].inUse && !bus.cores[1 - (coreId % 2)].inUse) {
                coreId++;
                continue;
            } else if (!bus.cores[coreId % 2].inUse) {
                break;
            }
        }
        struct sockaddr_in caddr;
        int len = sizeof(caddr);

        int core_fd = accept(socket_fd, (struct sockaddr*) &caddr, &len);
        if (core_fd < 0) {
            perror("Accept error");
            exit(-1);
        }

        struct sockaddr_in snoop_caddr;
        int snoop_len = sizeof(snoop_caddr);
        
        int snoop_core_fd = accept(socket_fd, (struct sockaddr*) &snoop_caddr, &snoop_len);
        if (snoop_core_fd < 0) {
            perror("Accept error");
            exit(-1);
        }

        // CoreData* core = malloc(sizeof(CoreData));
        // core->socket = core_fd;
        // core->coreID = ++coreId;
        bus.cores[coreId % 2].socket = core_fd;
        bus.cores[coreId % 2].snoopSocket = snoop_core_fd;
        bus.cores[coreId % 2].coreID = coreId % 2 + 1;

        pthread_t threadId;
        pthread_create(&threadId, NULL, handleCore, &bus.cores[(coreId++) % 2]);
        // pthread_create(&threadId, NULL, handleCore, core);
        pthread_detach(threadId);
    }
    freeBus();
    return 0;
}