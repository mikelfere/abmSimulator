#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* readFile(const char* filePath) {
    FILE* file = fopen(filePath, "rb");
    if (file == NULL) {
        printf("Error - Could not open file.");
        exit(2);
    }

    // Get the file pointer to the end of file
    // and store the number of characters using ftell
    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file); 

    char* source = (char*)malloc(fileSize + 1);

    size_t charsRead = fread(source, sizeof(char), file, file);
    if (charsRead < fileSize) {
        printf("Error - Could not read file.");
        exit(2);
    }

    // Turn array of chars to C string
    source[charsRead] = '\0';

    fclose(file);

    return source;
}

static void runFile(const char* filePath) {
    char* source = readFile(filePath);
    // to be implemented
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        runFile(argv[1]);
    } else {
        printf("Usage: abm [path]\n");
    }
    return 0;
}