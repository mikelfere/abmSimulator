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
    // printf("in runFile\n");
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

int main(int argc, char* argv[]) {
    // printf("in main\n");
    initNameList(&functionNames);

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

    if (connect(socket_fd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        perror("Connect error");
        exit(-1);
    }

    printf("Connected to bus...\n");

    // to write to bus use this format: 
    // if (write(sockfd, books[i], strlen(books[i])) > 0) {
	// 		/* get confirmation echoed from server and print */
	// 		char buffer[BuffSize + 1];
	// 		memset(buffer, '\0', sizeof(buffer));
	// 		if (read(sockfd, buffer, sizeof(buffer)) > 0)
	// 			puts(buffer);

    VM vm;
    initVM(&vm);
    // printf("after initVM\n");
    if (argc == 2) {
        runFile(socket_fd, &vm, argv[1]);
    } else {
        printf("Usage: abm [path]\n");
    }
    freeVM(&vm);

    printf("Core finished executing...\n");
    close(socket_fd);
    return 0;
}