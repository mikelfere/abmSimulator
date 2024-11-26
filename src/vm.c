#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "compiler.h"
#include "memmng.h"
#include "vm.h"

static void initStack(VM* vm) {
    vm->stackTop = vm->stack;
}

static void runtimeError(VM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf(format, args);
    va_end(args);
    puts("\n");
    
    for (int i = vm->frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm->frames[i];
        FunctionObject* function = frame->function;
        size_t instruction = frame->ip - function->sequence.code - 1;
        printf("--line %d-- in ", function->sequence.lines[instruction]);
        if (function->name == NULL) {
            printf("main program\n");
        } else {
            printf("%s()\n", function->name->characters);
        }
    }
}

void initVM(VM* vm) {
    initStack(vm);
    vm->objects = NULL;
    vm->frameCount = 0;
    initTable(&vm->strings);
    initCache(&vm->cache);
    vm->clock = 0;
}

void freeVM(VM* vm) {
    freeTable(&vm->strings);
    freeObjects(vm->objects);
    freeCache(&vm->cache);
}

void push(VM* vm, Value value) {
    *vm->stackTop = value;
    vm->stackTop++;
}

Value pop(VM* vm) {
    vm->stackTop--;
    return *vm->stackTop;
}

static Value peek(VM* vm, int distance) {
    return vm->stackTop[-1 - distance];
}

static bool call(VM* vm, FunctionObject* function) {
    if(vm->frameCount == FRAMES_CAPACITY) {
        runtimeError(vm, "Stack overflow.");
        return false;
    }
    CallFrame* frame = &vm->frames[vm->frameCount++];
    frame->function = function;
    frame->ip = function->sequence.code;
    return true;
}

static bool callValue(VM* vm, Value callee) {
    if (callee.as.obj->type == FUNCTION_OBJECT) {
        return call(vm, (FunctionObject*)callee.as.obj);
    }
    runtimeError(vm, "Can only call functions.");
    return false;
}

// Clock should belong to the main method of running program
static int replaceLRU(Cache* cache, int* clock) {
    // puts("In replace");
    int minIndex = 0;
    for (int i = 1; i < 3; i++) {
        if (cache->lastUsed[minIndex] > cache->lastUsed[i]) {
            minIndex = i;
        }
    }
    cache->lastUsed[minIndex] = *clock;
    (*clock)++;
    // puts("After replace");
    return minIndex;
}

static bool sendBusRead(int socket_fd, Value id, VM* vm, int lineIndex) {
    // puts("In read");
    char buffer[1024] = "1 ";
    int i, address, altAddress, upperB, lowerB, altUB, altLB;
    Value value;
    String *key, *recKey;
    if (id.type == OBJ_VALUE) {     // If String -> name
        key = (String*)id.as.obj;
        for (i = 0; i < key->length && i < 1021; i++) {
            buffer[i + 2] = key->characters[i];
        }
        buffer[i + 2] = '\0';
    } else {        // If number -> address
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
    // printf("Sending bus read: %s\n", buffer);
    if (write(socket_fd, buffer, 1024) <= 0) {
        perror("Could not write to bus");
        exit(-1);
    }
	memset(buffer, '\0', sizeof(buffer));
	if (read(socket_fd, buffer, sizeof(buffer)) <= 0) {
        perror("Could not read from bus");
        exit(-1);
    }
    // printf("Read: %s\n", buffer);

    // If number, read name first, then read data as normal
    if (id.type == NUM_VALUE) {  
        i = 0; 
        char name[1024];  
        while (buffer[i] != '\0') {
            name[i] = buffer[i];
            i++;
        }
        name[i] = '\0';
        recKey = copyString(&vm->strings, vm->objects, name, i);
        // Read data now
        memset(buffer, '\0', sizeof(buffer));
        if (read(socket_fd, buffer, sizeof(buffer)) <= 0) {
            perror("Could not read from bus");
            exit(-1);
        }
        // printf("Read: %s\n", buffer);
    }
    // puts("Getting data from bus");
    i = 0;
    // return false for entry not set in memory
    if (memcmp(buffer, "nfd", 3) == 0) {
        return false;
    } else {    // set cache values
        if (id.type == OBJ_VALUE) {
            recKey = copyString(&vm->strings, vm->objects, key->characters, key->length);
        }
        // puts("Reading address");
        int j = 0;
        char temp[64];
        // Read address
        memset(temp, '\0', sizeof(temp));
        while (buffer[i] != ' ') {
            temp[j++] = buffer[i++];
        }
        address = atoi(temp);
        // Read value
        i++, j = 0;
        memset(temp, '\0', sizeof(temp));
        while (buffer[i] != ' ') {
            temp[j++] = buffer[i++];
        }
        value = (Value){NUM_VALUE, {.number = atoi(temp)}};
        // Read upper bound
        i++, j = 0;
        memset(temp, '\0', sizeof(temp));
        while (buffer[i] != ' ') {
            temp[j++] = buffer[i++];
        }
        upperB = atoi(temp);
        // Read lower bound
        i++, j = 0;
        memset(temp, '\0', sizeof(temp));
        while (buffer[i] != ' ') {
            temp[j++] = buffer[i++];
        }
        lowerB = atoi(temp);
        // Read alternative address
        i++, j = 0;
        memset(temp, '\0', sizeof(temp));
        while (buffer[i] != ' ' && buffer[i] != '\0') {
            temp[j++] = buffer[i++];
        }
        altAddress = atoi(temp);
        // puts("Read first half of data");
        // If there is another location for variable, read its upper and lower bounds
        if (buffer[i] == ' ') {
            i++, j = 0;
            memset(temp, '\0', sizeof(temp));
            while (buffer[i] != ' ') {
                temp[j++] = buffer[i++];
            }
            altUB = atoi(temp);
            // puts("Set alt upper bound");
            i++, j = 0;
            memset(temp, '\0', sizeof(temp));
            // puts("Reading alt lower bound");
            while (buffer[i] != '\0') {
                // printf("%d\n", i);
                temp[j++] = buffer[i++];
            }
            // puts("Read alt lower bound");
            altLB = atoi(temp);
            // puts("Set alt lower bound");
        } else {
            altUB = -1;
            altLB = -1;
        }

        // pthread_mutex_lock(&vm->cache.lineLock[lineIndex]);
        vm->cache.entries[lineIndex].key = recKey;
        vm->cache.entries[lineIndex].address = address;
        vm->cache.entries[lineIndex].value = value;
        vm->cache.addressData[lineIndex][0] = upperB;
        vm->cache.addressData[lineIndex][1] = lowerB;
        vm->cache.addressData[lineIndex][2] = altAddress;
        vm->cache.addressData[lineIndex][3] = altUB;
        vm->cache.addressData[lineIndex][4] = altLB;
        // pthread_mutex_unlock(&vm->cache.lineLock[lineIndex]);
        // puts("Got data from bus");
        // printf("Cache line: index= %d, address=%d, aa=%d, ub=%d, lb=%d, aub=%d, alb=%d\n", \
        //     lineIndex, vm->cache.entries[lineIndex].address, vm->cache.addressData[lineIndex][2], \
        //     vm->cache.addressData[lineIndex][0], vm->cache.addressData[lineIndex][1], vm->cache.addressData[lineIndex][3],\
        //     vm->cache.addressData[lineIndex][4]);
        // puts("After read");
        return true;
    }
}

static bool sendBusReadX(int socket_fd, Value id, VM* vm, int lineIndex) {
    // puts("In read x");
    char buffer[1024] = "2 ";
    int i, address, altAddress, upperB, lowerB, altUB, altLB;
    Value value;
    String *key, *recKey;
    if (id.type == OBJ_VALUE) {     // If String -> name
        key = (String*)id.as.obj;
        for (i = 0; i < key->length && i < 1021; i++) {
            buffer[i + 2] = key->characters[i];
        }
        buffer[i + 2] = '\0';
    } else {        // If number -> address
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

    // printf("Sending bus read x: %s\n", buffer);
    if (write(socket_fd, buffer, 1024) <= 0) {
        perror("Could not write to bus");
        exit(-1);
    }
	memset(buffer, '\0', sizeof(buffer));
	if (read(socket_fd, buffer, sizeof(buffer)) <= 0) {
        perror("Could not read from bus");
        exit(-1);
    }
    // printf("Read: %s\n", buffer);
    // If number, read name first, then read data as normal
    if (id.type == NUM_VALUE) {  
        i = 0; 
        char name[1024];  
        while (buffer[i] != '\0') {
            name[i] = buffer[i];
            i++;
        }
        name[i] = '\0';
        recKey = copyString(&vm->strings, vm->objects, name, i);
        // Read data now
        memset(buffer, '\0', sizeof(buffer));
        if (read(socket_fd, buffer, sizeof(buffer)) <= 0) {
            perror("Could not read from bus");
            exit(-1);
        }
        // printf("Read: %s\n", buffer);
    }

    i = 0;
    // return false for entry not set in memory
    if (memcmp(buffer, "nfd", 3) == 0) {
        return false;
    } else {    // set cache values
        if (id.type == OBJ_VALUE) {
            recKey = copyString(&vm->strings, vm->objects, key->characters, key->length);
        }
        int j = 0;
        char temp[64];
        // Read address
        memset(temp, '\0', sizeof(temp));
        while (buffer[i] != ' ') {
            temp[j++] = buffer[i++];
        }
        address = atoi(temp);
        // Read value
        i++, j = 0;
        memset(temp, '\0', sizeof(temp));
        while (buffer[i] != ' ') {
            temp[j++] = buffer[i++];
        }
        value = (Value){NUM_VALUE, {.number = atoi(temp)}};
        // Read upper bound
        i++, j = 0;
        memset(temp, '\0', sizeof(temp));
        while (buffer[i] != ' ') {
            temp[j++] = buffer[i++];
        }
        upperB = atoi(temp);
        // Read lower bound
        i++, j = 0;
        memset(temp, '\0', sizeof(temp));
        while (buffer[i] != ' ') {
            temp[j++] = buffer[i++];
        }
        lowerB = atoi(temp);
        // Read alternative address
        i++, j = 0;
        memset(temp, '\0', sizeof(temp));
        while (buffer[i] != ' ' && buffer[i] != '\0') {
            temp[j++] = buffer[i++];
        }
        altAddress = atoi(temp);
        // If there is another location for variable, read its upper and lower bounds
        if (buffer[i] == ' ') {
            i++, j = 0;
            memset(temp, '\0', sizeof(temp));
            while (buffer[i] != ' ') {
                temp[j++] = buffer[i++];
            }
            altUB = atoi(temp);

            i++, j = 0;
            memset(temp, '\0', sizeof(temp));
            while (buffer[i] != '\0') {
                temp[j++] = buffer[i++];
            }
            altLB = atoi(temp);
        } else {
            altUB = -1;
            altLB = -1;
        }

        // pthread_mutex_lock(&vm->cache.lineLock[lineIndex]);
        vm->cache.entries[lineIndex].key = recKey;
        vm->cache.entries[lineIndex].address = address;
        vm->cache.entries[lineIndex].value = value;
        vm->cache.addressData[lineIndex][0] = upperB;
        vm->cache.addressData[lineIndex][1] = lowerB;
        vm->cache.addressData[lineIndex][2] = altAddress;
        vm->cache.addressData[lineIndex][3] = altUB;
        vm->cache.addressData[lineIndex][4] = altLB;
        // pthread_mutex_unlock(&vm->cache.lineLock[lineIndex]);

        // printf("Cache line: index= %d, address=%d, aa=%d, ub=%d, lb=%d, aub=%d, alb=%d\n", \
        //     lineIndex, vm->cache.entries[lineIndex].address, vm->cache.addressData[lineIndex][2], \
        //     vm->cache.addressData[lineIndex][0], vm->cache.addressData[lineIndex][1], vm->cache.addressData[lineIndex][3],\
        //     vm->cache.addressData[lineIndex][4]);
        // puts("After read x");
        return true;
    }
}

void invalidate(int socket_fd, Value id) {
    // puts("In invalidate()");
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
    // puts("After invalidate()");
}

void sendWriteBack(int socket_fd, int address, Value value) {
    char buffer[1024] = "3 ", temp[64];
    memset(temp, '\0', sizeof(temp));
    sprintf(temp, "%d", address);
    int i = 2, j = 0;
    while (temp[j] != '\0') {
        buffer[i++] = temp[j++];
    }
    buffer[i++] = ' ';
    memset(temp, '\0', sizeof(temp));
    sprintf(temp, "%d", value.as.number);
    j = 0;
    while (temp[j] != '\0') {
        buffer[i++] = temp[j++];
    }
    buffer[i] = '\0';
    if (write(socket_fd, buffer, 1024) <= 0) {
        perror("Could not write to bus");
        exit(-1);
    }
}

static int findLineByName(Action action, int socket_fd, VM* vm, Value name) {
    // puts("Find by name");
    // printf("Name: %s\n", ((String*)name.as.obj)->characters);
    for (int index = 0; index < 3; index++) {
        if (vm->cache.entries[index].key == (String*)name.as.obj) {  // normal hit
            // puts("After find line success - no read");
            return index;
        } else if (vm->cache.entries[index].key == NULL) {   // cache is not full
            switch (action) {
                case PROCESSOR_READ:
                    if(sendBusRead(socket_fd, name, vm, index)) {    // if found in memory
                        vm->cache.count++;
                        // pthread_mutex_lock(&vm->cache.lineLock[index]);
                        vm->cache.states[index] = SHARED;
                        // pthread_mutex_unlock(&vm->cache.lineLock[index]);
                        // puts("After find line success");
                        return index;
                    }
                    break;
                case PROCESSOR_WRITE:
                    if(sendBusReadX(socket_fd, name, vm, index)) {
                        vm->cache.count++;
                        // pthread_mutex_lock(&vm->cache.lineLock[index]);
                        vm->cache.states[index] = MODIFIED;
                        // pthread_mutex_unlock(&vm->cache.lineLock[index]);
                        // puts("After find line success");
                        return index;
                    }
                    break;
            }
            break;
        }
    }
    // puts("After find line fail");
    return -1;
}

static int findLineByAddress(Action action, int socket_fd, VM* vm, Value address) {
    // puts("Find by address");
    for (int index = 0; index < 3; index++) {
        if (vm->cache.entries[index].address == address.as.number || \
            vm->cache.addressData[index][2] == address.as.number) {  // normal hit
            // puts("After find line success - no read");
            // if (vm->cache.states[index] == INVALID) {
            //     sendBusRead(socket_fd, address, vm, index);
            //     vm->cache.states[index] = SHARED;
            // }
            return index;
        } else if (vm->cache.entries[index].address == -1) {   // cache is not full
            switch (action) {
                case PROCESSOR_READ:
                    if(sendBusRead(socket_fd, address, vm, index)) {    // if found in memory
                        vm->cache.count++;
                        // pthread_mutex_lock(&vm->cache.lineLock[index]);
                        vm->cache.states[index] = SHARED;
                        // pthread_mutex_unlock(&vm->cache.lineLock[index]);
                        // puts("After find line success");
                        return index;
                    }
                    break;
                case PROCESSOR_WRITE:
                    if(sendBusReadX(socket_fd, address, vm, index)) {
                        vm->cache.count++;
                        // // pthread_mutex_lock(&vm->cache.lineLock[index]);
                        vm->cache.states[index] = MODIFIED;
                        // pthread_mutex_unlock(&vm->cache.lineLock[index]);
                        // puts("After find line success");
                        return index;
                    }
                    break;
            }
            break;
        }
    }
    // puts("After find line fail");
    return -1;
}

int getLine(int socket_fd, VM* vm, Value id) {
    // puts("In get line");
    int index;
    if (id.type == OBJ_VALUE) {
        // puts("Find by name");
        index = findLineByName(PROCESSOR_READ, socket_fd, vm, id);
    } else {
        index = findLineByAddress(PROCESSOR_READ, socket_fd, vm, id);
    }
    // If found
    if (index >= 0) {
        // pthread_mutex_lock(&vm->cache.lineLock[index]);
        if (vm->cache.states[index] == INVALID) { // If invalid trigger a bus read
            // If in cache, it is already in memory
            sendBusRead(socket_fd, id, vm, index);
            vm->cache.states[index] = SHARED;
        }    
        // pthread_mutex_unlock(&vm->cache.lineLock[index]);   
    } else {    // Normal miss
        // Get LRU line
        index = replaceLRU(&vm->cache, &vm->clock);
        // To be written back if line is replaced
        Value val = vm->cache.entries[index].value;
        int address = vm->cache.entries[index].address;
        if (sendBusRead(socket_fd, id, vm, index)) {   // If in memory
            // Write back if dirty
            // pthread_mutex_lock(&vm->cache.lineLock[index]);
            if (vm->cache.states[index] == MODIFIED) {
                // pthread_mutex_unlock(&vm->cache.lineLock[index]);
                sendWriteBack(socket_fd, address, val);
                // pthread_mutex_lock(&vm->cache.lineLock[index]);
            }
            vm->cache.states[index] = SHARED;
            // pthread_mutex_unlock(&vm->cache.lineLock[index]);
        } else {    // If not found in memory
            // puts("After get line");
            return -1;
        }
    }
    // puts("After get line");
    // printf("Index: %d\n", index);
    return index;
}

// Change to handle address too------------------------------------
int setLine(int socket_fd, VM* vm, Value id, Value value) {
    // puts("In set Line");
    int index;
    if (id.type == OBJ_VALUE) {
        index = findLineByName(PROCESSOR_WRITE, socket_fd, vm, id);
    } else {
        index = findLineByAddress(PROCESSOR_WRITE, socket_fd, vm, id);
    }
    // If found 
    if (index >= 0) {
        // pthread_mutex_lock(&vm->cache.lineLock[index]);
        if (vm->cache.states[index] == MODIFIED) {     // Normal hit
            vm->cache.entries[index].value = value;
        } else if (vm->cache.states[index] == SHARED) {    // Coherence
            invalidate(socket_fd, id);
            // sendBusReadX(socket_fd, name, cache, index);
            vm->cache.entries[index].value = value;
            vm->cache.states[index] = MODIFIED;
        } else if (vm->cache.states[index] == INVALID) {   // Normal miss
            // Place write miss on bus 
            sendBusReadX(socket_fd, id, vm, index);
            vm->cache.entries[index].value = value;
            vm->cache.states[index] = MODIFIED;            
        }
        // pthread_mutex_unlock(&vm->cache.lineLock[index]);
    } else {    // Normal miss
        // Get LRU line
        index = replaceLRU(&vm->cache, &vm->clock);
        // To be written back if line is replaced
        Value val = vm->cache.entries[index].value;
        int address = vm->cache.entries[index].address;
        if (sendBusReadX(socket_fd, id, vm, index)) {
            // Write back if dirty
            // pthread_mutex_lock(&vm->cache.lineLock[index]);
            if (vm->cache.states[index] == MODIFIED) {
                sendWriteBack(socket_fd, address, val);
            }
            vm->cache.entries[index].value = value;
            vm->cache.states[index] = MODIFIED;
            // pthread_mutex_unlock(&vm->cache.lineLock[index]);
        } else {    // If not found in memory
            // puts("After set line");
            return -1;
        }
    }
    // puts("After set line");
    return index;
}


static InterpretResult run(int socket_fd, VM* vm) {
    bool inReturn = false, isNumAddress = false;
    int address;
    CallFrame* frame = &vm->frames[vm->frameCount - 1];
    CallFrame* tempFrame = frame;
    for (;;) {
        // printf("\nCache contents: \n");
        // for (int i = 0; i < 3; i++) {
        //     if (vm->cache.entries[i].key != NULL) {
        //         printf("key: %s, address: %d, ", vm->cache.entries[i].key->characters, vm->cache.entries[i].address);
        //         if (vm->cache.states[i] == INVALID) {
        //             printf("key: %s, state: INVALID\n", vm->cache.entries[i].key->characters);
        //         } else if (vm->cache.states[i] == SHARED) {
        //             printf("state: SHARED\n");
        //         } else {
        //             printf("state: MODIFIED\n");
        //         }
        //     }
        // }
        // sleep(1);
        uint8_t instruction;
        switch (instruction = (*frame->ip++)) {
            case OP_LVALUE: {
                // puts("In lvalue");
                Value name = frame->function->sequence.constants.values[*frame->ip++];
                Value val = (Value){NUM_VALUE, {.number = 0}};
                // pthread_mutex_lock(&vm->vmLock);
                // while (!vm->vmRun) {
                //     pthread_cond_wait(&vm->vmCond, &vm->vmLock);
                // }
                // vm->vmRun = 0;
                // pthread_mutex_unlock(&vm->vmLock);
                // pthread_mutex_lock(&vm->cacheLock);
                // puts("Run acquired mutex");
                if (getLine(socket_fd, vm, name) == -1) {
                    if (!inReturn) {
                        if (tableGetValue(&tempFrame->locals, (String*)name.as.obj, &val) == -1) {
                            tableSetValue(&tempFrame->locals, (String*)name.as.obj, val);
                        }
                    } else {
                        if (tableGetValue(&frame->locals, (String*)name.as.obj, &val) == -1) {
                            tableSetValue(&frame->locals, (String*)name.as.obj, val);
                        }
                    }
                }
                // puts("Run released mutex");
                // pthread_mutex_unlock(&vm->cacheLock);
                // pthread_mutex_lock(&vm->snoopLock);
                // vm->snoopRun = 1;
                // pthread_cond_signal(&vm->snoopCond);
                // pthread_mutex_unlock(&vm->snoopLock);
                push(vm, name);
                // puts("After lvalue");
                break;
            }
            case OP_RVALUE: {
                // puts("In rvalue");
                Value name = frame->function->sequence.constants.values[*frame->ip++];
                Value val = (Value){NUM_VALUE, {.number = 0}};
                // pthread_mutex_lock(&vm->vmLock);
                // while (!vm->vmRun) {
                //     pthread_cond_wait(&vm->vmCond, &vm->vmLock);
                // }
                // vm->vmRun = 0;
                // pthread_mutex_unlock(&vm->vmLock);
                // pthread_mutex_lock(&vm->cacheLock);
                // puts("Run acquired mutex");
                int index = getLine(socket_fd, vm, name);
                if (index == -1) {
                    if (!inReturn) {
                        if (tableGetValue(&frame->locals, (String*)name.as.obj, &val) == -1) {
                            tableSetValue(&frame->locals, (String*)name.as.obj, val);
                        }
                    } else {
                        if (tableGetValue(&tempFrame->locals, (String*)name.as.obj, &val) == -1) {
                            tableSetValue(&tempFrame->locals, (String*)name.as.obj, val);
                        }
                    }
                } else {    // If found in global memory
                    // pthread_mutex_lock(&vm->cache.lineLock[index]);
                    val = vm->cache.entries[index].value;
                    // pthread_mutex_unlock(&vm->cache.lineLock[index]);
                }
                // puts("Run released mutex");
                // pthread_mutex_unlock(&vm->cacheLock);
                // pthread_mutex_lock(&vm->snoopLock);
                // vm->snoopRun = 1;
                // pthread_cond_signal(&vm->snoopCond);
                // pthread_mutex_unlock(&vm->snoopLock);
                push(vm, val);
                // puts("After rvalue");
                break;
            }
            case OP_ADD: {
                // puts("In add");
                Value b = pop(vm);
                Value a = pop(vm);
                if (a.type == OBJ_VALUE) {
                    // pthread_mutex_lock(&vm->vmLock);
                    // while (!vm->vmRun) {
                    //     pthread_cond_wait(&vm->vmCond, &vm->vmLock);
                    // }
                    // vm->vmRun = 0;
                    // pthread_mutex_unlock(&vm->vmLock);
                    // pthread_mutex_lock(&vm->cacheLock);
                    // puts("Run acquired mutex");
                    int index = getLine(socket_fd, vm, a);
                    Value val;
                    if (index == -1) {      // If not defined in global memory
                        address = tableGetValue(&frame->locals, (String*)a.as.obj, &val);
                        if (address < 0) {          // If not found in local memory either
                            runtimeError(vm, "Variable doesn't have an address");
                        } else {
                            index = getLine(socket_fd, vm, (Value){NUM_VALUE, {.number = address}});
                        }
                    }
                    // If out of bounds
                    // pthread_mutex_lock(&vm->cache.lineLock[index]);
                    if (vm->cache.entries[index].address + b.as.number > vm->cache.addressData[index][0]) {
                        // index = vm->cache.addressData[index][2];
                        if (vm->cache.addressData[index][2] + b.as.number > vm->cache.addressData[index][3]) {
                            runtimeError(vm, "Cannot access memory location");
                        } else {
                            push(vm, (Value){NUM_VALUE, {.number = vm->cache.addressData[index][2] + b.as.number}});
                        }
                    } else {
                        push(vm, (Value){NUM_VALUE, {.number = vm->cache.entries[index].address + b.as.number}});
                    }
                    // pthread_mutex_unlock(&vm->cache.lineLock[index]);
                    // puts("Run released mutex");
                    // pthread_mutex_unlock(&vm->cacheLock);
                    // pthread_mutex_lock(&vm->snoopLock);
                    // vm->snoopRun = 1;
                    // pthread_cond_signal(&vm->snoopCond);
                    // pthread_mutex_unlock(&vm->snoopLock);
                    isNumAddress = true;
                } else {
                    push(vm, (Value){NUM_VALUE, {.number = (a.as.number + b.as.number)}});
                }
                // puts("After add");
                break;
            }
            case OP_SUBTRACT: {
                // puts("In sub");
                Value b = pop(vm);
                Value a = pop(vm);
                if (a.type == OBJ_VALUE) {
                    // pthread_mutex_lock(&vm->vmLock);
                    // while (!vm->vmRun) {
                    //     pthread_cond_wait(&vm->vmCond, &vm->vmLock);
                    // }
                    // vm->vmRun = 0;
                    // pthread_mutex_unlock(&vm->vmLock);
                    // pthread_mutex_lock(&vm->cacheLock);
                    // puts("Run acquired mutex");
                    int index = getLine(socket_fd, vm, a);
                    Value val;
                    if (index == -1) {    // If not defined in global memory 
                        address = tableGetValue(&frame->locals, (String*)a.as.obj, &val);
                        if (address < 0) {          // If not found in local memory either
                            runtimeError(vm, "Variable doesn't have an address");
                        } else {
                            index = getLine(socket_fd, vm, (Value){NUM_VALUE, {.number = address}});
                        }
                    }
                    // pthread_mutex_lock(&vm->cache.lineLock[index]);
                    if (vm->cache.entries[index].address - b.as.number < vm->cache.addressData[index][1]) {
                        if (vm->cache.addressData[index][2] - b.as.number < vm->cache.addressData[index][4]) {
                            runtimeError(vm, "Cannot access memory location");
                        } else {
                            push(vm, (Value){NUM_VALUE, {.number = vm->cache.addressData[index][2] - b.as.number}});
                        }
                        
                    } else {
                        push(vm, (Value){NUM_VALUE, {.number = vm->cache.entries[index].address - b.as.number}});
                    }
                    // pthread_mutex_unlock(&vm->cache.lineLock[index]);
                    // puts("Run released mutex");
                    // pthread_mutex_unlock(&vm->cacheLock);
                // pthread_mutex_lock(&vm->snoopLock);
                // vm->snoopRun = 1;
                // pthread_cond_signal(&vm->snoopCond);
                // pthread_mutex_unlock(&vm->snoopLock);
                    isNumAddress = true;
                } else {
                    push(vm, (Value){NUM_VALUE, {.number = (a.as.number - b.as.number)}});
                }
                // puts("After sub");
                break;
            }
            case OP_MULTIPLY: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                push(vm, (Value){NUM_VALUE, {.number = (a * b)}});
                break;
            }
            case OP_DIVIDE: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                push(vm, (Value){NUM_VALUE, {.number = (a / b)}});
                break;
            }
            case OP_REMAINDER: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                push(vm, (Value){NUM_VALUE, {.number = (a % b)}});
                break;
            }
            case OP_AND: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                push(vm, (Value){NUM_VALUE, {.number = (a && b)}});
                break;
            }
            case OP_OR: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                push(vm, (Value){NUM_VALUE, {.number = (a || b)}});
                break;
            }
            case OP_NOT: {
                push(vm, (Value){NUM_VALUE, {.number = !(pop(vm).as.number)}});
                break;
            }
            case OP_NOT_EQUAL: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                int result = 0;
                if (a != b) {
                    result = 1;
                }
                push(vm, (Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_EQUAL: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                int result = 0;
                if (a == b) {
                    result = 1;
                }
                push(vm, (Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_LESS: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                int result = 0;
                if (a < b) {
                    result = 1;
                }
                push(vm, (Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_LESS_EQUAL: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                int result = 0;
                if (a <= b) {
                    result = 1;
                }
                push(vm, (Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_GREATER: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                int result = 0;
                if (a > b) {
                    result = 1;
                }
                push(vm, (Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_GREATER_EQUAL: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                int result = 0;
                if (a >= b) {
                    result = 1;
                }
                push(vm, (Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_PRINT: 
                printf("%d\n", peek(vm, 0).as.number);
                break;
            case OP_SHOW: {
                Value string = frame->function->sequence.constants.values[*frame->ip++];
                printf("%.*s", ((String*)string.as.obj)->length, ((String*)string.as.obj)->characters);
                break;
            }
            case OP_POP: 
                pop(vm);
                break;
            case OP_PUSH: {
                Value val = frame->function->sequence.constants.values[*frame->ip++];
                push(vm, val);
                break;
            }
            case OP_COPY:
                push(vm, peek(vm, 0));
                break;
            case OP_ASSIGN: {
                // puts("In assign");
                Value val = pop(vm);
                Value name = pop(vm);
                Value temp;
                // pthread_mutex_lock(&vm->vmLock);
                // while (!vm->vmRun) {
                //     pthread_cond_wait(&vm->vmCond, &vm->vmLock);
                // }
                // vm->vmRun = 0;
                // pthread_mutex_unlock(&vm->vmLock);
                // pthread_mutex_lock(&vm->cacheLock);
                // puts("Run acquired mutex");
                int index = getLine(socket_fd, vm, name);
                if (index == -1) {      // If variable is not global    
                    if (!inReturn) {
                        tableSetValue(&tempFrame->locals, (String*)name.as.obj, val);
                        address = tableGetValue(&tempFrame->locals, (String*)name.as.obj, &temp);
                    } else {
                        tableSetValue(&frame->locals, (String*)name.as.obj, val);
                        address = tableGetValue(&frame->locals, (String*)name.as.obj, &temp);
                    }
                    if (address >= 0) {
                        setLine(socket_fd, vm, (Value){NUM_VALUE, {.number = address}}, val);
                        // setValue(socket_fd, val, address);
                    }
                } else {
                    setLine(socket_fd, vm, name, val);
                }    
                // pthread_mutex_unlock(&vm->cacheLock);
                // pthread_mutex_lock(&vm->snoopLock);
                // vm->snoopRun = 1;
                // pthread_cond_signal(&vm->snoopCond);
                // pthread_mutex_unlock(&vm->snoopLock);
                // puts("Run released mutex"); 
                // puts("After assign");           
                break;
            }
            case OP_ASSIGN_ADDRESS: {
                // puts("In assign address");
                Value name2 = pop(vm);
                Value name1 = pop(vm);
                Value val = (Value){NUM_VALUE, {.number = 0}};
                // pthread_mutex_lock(&vm->vmLock);
                // while (!vm->vmRun) {
                //     pthread_cond_wait(&vm->vmCond, &vm->vmLock);
                // }
                // vm->vmRun = 0;
                // pthread_mutex_unlock(&vm->vmLock);
                // pthread_mutex_lock(&vm->cacheLock);
                // puts("Run acquired mutex");
                int index = getLine(socket_fd, vm, name2);
                if (isNumAddress) {
                    isNumAddress = false;
                    tableChangeAddress(&frame->locals, (String*)name1.as.obj, name2.as.number);
                    tableSetValue(&frame->locals, (String*)name1.as.obj, vm->cache.entries[index].value);
                    // pthread_mutex_unlock(&vm->cacheLock);
                // pthread_mutex_lock(&vm->snoopLock);
                // vm->snoopRun = 1;
                // pthread_cond_signal(&vm->snoopCond);
                // pthread_mutex_unlock(&vm->snoopLock);
                    break;
                }
                if (index == -1) {     // If variable is not globally defined
                    address = tableGetValue(&frame->locals, (String*)name2.as.obj, &val);
                    tableChangeAddress(&frame->locals, (String*)name1.as.obj, address);
                    tableSetValue(&frame->locals, (String*)name1.as.obj, val);
                } else {
                    // pthread_mutex_lock(&vm->cache.lineLock[index]);
                    tableChangeAddress(&frame->locals, (String*)name1.as.obj, vm->cache.entries[index].address);
                    tableSetValue(&frame->locals, (String*)name1.as.obj, vm->cache.entries[index].value);
                    // pthread_mutex_unlock(&vm->cache.lineLock[index]);
                }
                // puts("Run released mutex");
                // pthread_mutex_unlock(&vm->cacheLock);
                // pthread_mutex_lock(&vm->snoopLock);
                // vm->snoopRun = 1;
                // pthread_cond_signal(&vm->snoopCond);
                // pthread_mutex_unlock(&vm->snoopLock);
                // puts("After assign address");
                break;
            }
            case OP_JUMP: {
                Value name = frame->function->sequence.constants.values[*frame->ip++];
                frame->ip += 2;
                int address = (int)(frame->ip[-2] << 8 | frame->ip[-1]);
                Value jumpToAddress;
                if (tableGetValue(&frame->function->labels, (String*)name.as.obj, &jumpToAddress) == -2) {
                    int offset = jumpToAddress.as.number - address - 3;
                    frame->ip += offset;
                } else {
                    runtimeError(vm, "No label defined for this jump.");
                }
                break;
            }
            case OP_JUMP_IF_TRUE: {
                Value name = frame->function->sequence.constants.values[*frame->ip++];
                frame->ip += 2;
                if (pop(vm).as.number != 0) {
                    int address = (int)(frame->ip[-2] << 8 | frame->ip[-1]);
                    Value jumpToAddress;
                    if (tableGetValue(&frame->function->labels, (String*)name.as.obj, &jumpToAddress) == -2) {
                        int offset = jumpToAddress.as.number - address - 3;
                        frame->ip += offset;
                    } else {
                        runtimeError(vm, "No label defined for this jump.");
                    }
                }
                break;
            }
            case OP_JUMP_IF_FALSE: {
                Value name = frame->function->sequence.constants.values[*frame->ip++];
                frame->ip += 2;
                if (pop(vm).as.number == 0) {
                    int address = (int)(frame->ip[-2] << 8 | frame->ip[-1]);
                    Value jumpToAddress;
                    if (tableGetValue(&frame->function->labels, (String*)name.as.obj, &jumpToAddress) == -2) {
                        int offset = jumpToAddress.as.number - address - 3;
                        frame->ip += offset;
                    } else {
                        runtimeError(vm, "No label defined for this jump.");
                    }
                }
                break;
            }
            case OP_BEGIN: {
                tempFrame = &vm->frames[vm->frameCount];
                initTable(&tempFrame->locals);
                break;
            }
            case OP_END: {
                freeTable(&tempFrame->locals);
                tempFrame = frame;
                inReturn = false;
                break;
            }
            case OP_CALL: {
                Value name = frame->function->sequence.constants.values[*frame->ip++];
                Value function;
                if (tableGetValue(&(&vm->frames[0])->function->labels, (String*)name.as.obj, &function) == -1) {
                    runtimeError(vm, "No function with that name.");
                }
                if (!callValue(vm, function)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = tempFrame;
                break;
            }
            case OP_RETURN: {
                vm->frameCount--;
                frame = &vm->frames[vm->frameCount - 1];
                inReturn = true;
                break;
            }
            case OP_HALT: {
                // If any cache line is dirty at end of program, write back to memory,
                // Invalidate everything
                for (int i = 0; i < vm->cache.count; i++) {
                    if (vm->cache.entries[i].key != NULL && vm->cache.states[i] == MODIFIED) {
                        sendWriteBack(socket_fd, vm->cache.entries[i].address, vm->cache.entries[i].value);
                    }
                }
                char buffer[1024] = "ret";
                if (write(socket_fd, buffer, 1024) <= 0) {
                    perror("Could not write to bus");
                    exit(-1);
                }
                // printf("\nCache contents: \n");
                // for (int i = 0; i < 3; i++) {
                //     if (vm->cache.entries[i].key != NULL) {
                //         printf("key: %s, address: %d, ", vm->cache.entries[i].key->characters, vm->cache.entries[i].address);
                //         if (vm->cache.states[i] == INVALID) {
                //             printf("state: INVALID\n");
                //         } else if (vm->cache.states[i] == SHARED) {
                //             printf("state: SHARED\n");
                //         } else {
                //             printf("state: MODIFIED\n");
                //         }
                //     }
                // }
                return INTERPRET_OK;
            }
        }
    }
}

InterpretResult interpret(int socket_fd, VM* vm, const char* source) {
    FunctionObject* function = compile(socket_fd, vm, source);
    if (function == NULL) {
        return INTERPRET_COMPILE_ERROR;
    }
    
    push(vm, (Value){OBJ_VALUE, {.obj = (Object*)function}});
    call(vm, function);

    return run(socket_fd, vm);
}