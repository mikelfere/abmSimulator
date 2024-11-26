#include <stdio.h>
#include <string.h>

#include "memmng.h"
#include "object.h"
#include "symbolTable.h"
#include "vm.h"

static Object* allocateObject(Object* objects, size_t size, ObjectType type) {
    Object* obj = (Object*)reallocate(NULL, 0, size);
    obj->type = type;
    obj->next = objects;
    objects = obj;
    return obj;
}

static String* allocateString(Table* table, Object* objects, char* characters, int length, uint32_t hash) {
    String* string = (String*)allocateObject(objects, sizeof(String), STRING_OBJECT);
    string->characters = characters;
    string->length = length;
    string->hash = hash;
    tableSetValue(table, string, (Value){NUM_VALUE, {.number = 0}});
    return string;
}

FunctionObject* newFunction(VM* vm) {
    FunctionObject* function = (FunctionObject*)allocateObject(vm->objects, sizeof(FunctionObject), FUNCTION_OBJECT);
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

String* copyString(Table* table, Object* objects, const char* characters, int length) {
    uint32_t hash = hashString(characters, length);
    String* interned = findString(table, characters, length, hash);
    if (interned != NULL) {
        return interned;
    }
    char* newCharacters = (char*)reallocate(NULL, 0, sizeof(char) * (length + 1));
    memcpy(newCharacters, characters, length);
    newCharacters[length] = '\0';
    return allocateString(table, objects, newCharacters, length, hash);
}
