#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scanner.h"
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

static void runFile(VM* vm, const char* filePath) {
    printf("in runFile\n");
    char* source = readFile(filePath);
    getFunctionNames(&functionNames, source);
    InterpretResult result = interpret(vm, source);
    free(source);
    if (result == INTERPRET_COMPILE_ERROR) {
        exit(1);
    }
    if (result == INTERPRET_RUNTIME_ERROR) {
        exit(1);
    }
    
}

int main(int argc, char* argv[]) {
    printf("in main\n");
    initNameList(&functionNames);
    VM vm;
    initVM(&vm);
    printf("after initVM\n");
    if (argc == 2) {
        runFile(&vm, argv[1]);
    } else {
        printf("Usage: abm [path]\n");
    }
    freeVM(&vm);
    return 0;
}