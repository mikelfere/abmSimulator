#include <stdio.h>
#include <string.h>

#include "memmng.h"
#include "object.h"
#include "symbolTable.h"
#include "vm.h"

static Object* allocateObject(VM* vm, size_t size, ObjectType type) {
    Object* obj = (Object*)reallocate(NULL, 0, size);
    obj->type = type;
    obj->next = vm->objects;
    vm->objects = obj;
    return obj;
}

static String* allocateString(VM* vm, char* characters, int length, uint32_t hash) {
    String* string = (String*)allocateObject(vm, sizeof(String), STRING_OBJECT);
    string->characaters = characters;
    string->length = length;
    string->hash = hash;
    tableSetValue(&vm->strings, string, (Value){NUM_VALUE, {.number = 0}});
    return string;
}

FunctionObject* newFunction(VM* vm) {
    FunctionObject* function = (FunctionObject*)allocateObject(vm, sizeof(FunctionObject), FUNCTION_OBJECT);
    function->name = NULL;
    initSequence(&function->sequence);
    initTable(&function->labels);
    return function;
}

/**
 * @brief FNV-1a hashing algorithm, to get a hash value for a passed string.
 * 
 * @param key 
 * @param length 
 * @return uint32_t 
 */
static uint32_t hashString(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

String* copyString(VM* vm, const char* characters, int length) {
    uint32_t hash = hashString(characters, length);
    String* interned = findString(&vm->strings, characters, length, hash);
    if (interned != NULL) {
        return interned;
    }
    char* newCharacters = (char*)reallocate(NULL, 0, sizeof(char) * (length + 1));
    memcpy(newCharacters, characters, length);
    newCharacters[length] = '\0';
    return allocateString(vm, newCharacters, length, hash);
}
