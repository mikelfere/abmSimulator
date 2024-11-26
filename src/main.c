#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scanner.h"
#include "socket.h"
#include "vm.h"

NameList functionNames;

typedef struct {
    int socket_fd;
    VM* vm;
    char* argv;
} ThreadData;

static char* readFile(const char* filePath) {
    // printf("readFile\n");
    FILE* file = fopen(filePath, "rb");
    if (file == NULL) {
        printf("Error - Could not open file.");
        exit(1);
    }

    // Get the file pointer to the end of file
    // and store the number of characters using ftell
    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file); 

    char* source = (char*)malloc(fileSize + 1);

    size_t charsRead = fread(source, sizeof(char), fileSize, file);
    if (charsRead < fileSize) {
        printf("Error - Could not read file.");
        exit(2);
    }

    // Turn array of chars to C string
    source[charsRead] = '\0';

    fclose(file);

    return source;
}

static void runFile(int socket_fd, VM* vm, const char* filePath) {
    char* source = readFile(filePath);
    getFunctionNames(&functionNames, source);
    InterpretResult result = interpret(socket_fd, vm, source);
    free(source);
    if (result == INTERPRET_COMPILE_ERROR) {
        exit(1);
    }
    if (result == INTERPRET_RUNTIME_ERROR) {
        exit(1);
    }
    
}

void* snoop(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    char buffer[1024], name[1024];
    int valread, i, j, address;
    String* key;
    while ((valread = read(data->socket_fd, buffer, 1024)) > 0) {
        // sleep(1);
        // printf("Snoop - Bus sent %s\n", buffer);
        // pthread_mutex_lock(&data->vm->cacheLock);
        // puts("Snoop acquired mutex");
        if (memcmp(buffer, "0", 1) == 0) {      // If invalidate
            if (memcmp(buffer + 1, "a", 1) == 0) {
                i = 3; j = 0;
                while (buffer[i] != '\0') {
                    name[j++] = buffer[i++];
                }
                name[j] = '\0';
                address = atoi(name);
                for (i = 0; i < 3; i++) {
                    // pthread_mutex_lock(&data->vm->cache.lineLock[i]);
                    if (data->vm->cache.entries[i].address == address || \
                        data->vm->cache.addressData[i][2] == address) {
                        // pthread_mutex_lock(&data->vm->cache.lineLock[i]);
                        data->vm->cache.states[i] = INVALID;
                        // pthread_mutex_unlock(&data->vm->cache.lineLock[i]);
                        break;
                    }
                    // pthread_mutex_unlock(&data->vm->cache.lineLock[i]);
                } 
            } else {
                i = 2, j = 0;
                while (buffer[i] != '\0') {
                    name[j++] = buffer[i++];
                }
                name[j] = '\0';
                key = copyString(&data->vm->strings, data->vm->objects, name, j);
                for (i = 0; i < 3; i++) {
                    // pthread_mutex_lock(&data->vm->cache.lineLock[i]);
                    if (data->vm->cache.entries[i].key == key) {
                        // pthread_mutex_lock(&data->vm->cache.lineLock[i]);
                        data->vm->cache.states[i] = INVALID;
                        // pthread_mutex_unlock(&data->vm->cache.lineLock[i]);
                        break;
                    }
                    // pthread_mutex_unlock(&data->vm->cache.lineLock[i]);
                } 
            }
        } else if (memcmp(buffer, "3", 1) == 0) {       // If write back
            i = 2, j = 0;
            while (buffer[i] != '\0') {
                name[j] = buffer[i];
                i++;
                j++;
            }
            name[j] = '\0';
            key = copyString(&data->vm->strings, data->vm->objects, name, j);
            for (i = 0; i < 3; i++) {
                // pthread_mutex_lock(&data->vm->cache.lineLock[i]);
                if (data->vm->cache.entries[i].key == key) {
                    Value value = data->vm->cache.entries[i].value;
                    // pthread_mutex_unlock(&data->vm->cache.lineLock[i]);
                    memset(buffer, '\0', sizeof(buffer));
                    sprintf(buffer, "%d", value.as.number);
                    if (write(data->socket_fd, buffer, 1024) <= 0) {
                        perror("Could not write to core");
                        exit(-1);
                    }
                    // pthread_mutex_lock(&data->vm->cache.lineLock[i]);
                    data->vm->cache.states[i] = SHARED;
                    // pthread_mutex_unlock(&data->vm->cache.lineLock[i]);
                    break;
                }
                // pthread_mutex_unlock(&data->vm->cache.lineLock[i]);
            }
        }
        // puts("Snoop released mutex");
        // pthread_mutex_unlock(&data->vm->cacheLock);
        // pthread_mutex_lock(&data->vm->vmLock);
        // data->vm->vmRun = 1;
        // pthread_cond_signal(&data->vm->vmCond);
        // pthread_mutex_unlock(&data->vm->vmLock);

        // pthread_mutex_lock(&data->vm->snoopLock);
        // while (!data->vm->snoopRun) {
        //     pthread_cond_wait(&data->vm->snoopCond, &data->vm->snoopLock);
        // }
        // data->vm->snoopRun = 0;
        // pthread_mutex_unlock(&data->vm->snoopLock);
    }
}

void* run(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    runFile(data->socket_fd, data->vm, data->argv);
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    initNameList(&functionNames);

    // puts("in main");

    // Setting up the socket for IPC
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("Socket error");
        exit(-1);
    }

    struct hostent* hptr = gethostbyname(HOST);
    if (!hptr) {
        perror("Host name error");
        exit(-1);
    }
    if (hptr->h_addrtype != AF_INET) {
        perror("Bad address family");
        exit(-1);
    }

    struct sockaddr_in saddr; 
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = ((struct in_addr*) hptr->h_addr_list[0])->s_addr;
    saddr.sin_port = htons(PORT);
    
    // Connecting to bus
    if (connect(socket_fd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        perror("Connect error");
        exit(-1);
    }

    int snoop_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (snoop_socket_fd < 0) {
        perror("Socket error");
        exit(-1);
    }

    struct hostent* snoop_hptr = gethostbyname(HOST);
    if (!snoop_hptr) {
        perror("Host name error");
        exit(-1);
    }
    if (snoop_hptr->h_addrtype != AF_INET) {
        perror("Bad address family");
        exit(-1);
    }

    struct sockaddr_in snoop_saddr; 
    memset(&snoop_saddr, 0, sizeof(snoop_saddr));
    snoop_saddr.sin_family = AF_INET;
    snoop_saddr.sin_addr.s_addr = ((struct in_addr*) snoop_hptr->h_addr_list[0])->s_addr;
    snoop_saddr.sin_port = htons(PORT);

    if (connect(snoop_socket_fd, (struct sockaddr*)&snoop_saddr, sizeof(snoop_saddr)) < 0) {
        perror("Connect error");
        exit(-1);
    }

    // puts("Connected to bus");
    // Running the program
    VM vm;
    initVM(&vm);
    // puts("After initVM");
    if (argc == 2) {
        pthread_t mainThread, snoopingThread;
        
        // pthread_mutex_init(&vmcacheLock);
        char* argvv = malloc(sizeof(char)*strlen(argv[1]));
        strcpy(argvv, argv[1]);
        // puts("Copied string");

        ThreadData data1 = (ThreadData){.socket_fd = socket_fd, .vm = &vm, .argv = argvv};
        ThreadData data2 = (ThreadData){.socket_fd = snoop_socket_fd, .vm = &vm, .argv = argvv};

        pthread_create(&mainThread, NULL, run, &data1);
        // puts("Running core");
        pthread_create(&snoopingThread, NULL, snoop, &data2);
        // puts("Running snoop");

        pthread_join(mainThread, NULL);
        pthread_detach(snoopingThread);
        free(argvv);
    } else {
        printf("Usage: abm [path]\n");
    }
    freeVM(&vm);
    close(socket_fd);
    return 0;
}