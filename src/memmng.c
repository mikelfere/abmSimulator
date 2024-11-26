#include <stdio.h>
#include <stdlib.h>

#include "memmng.h"
#include "value.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* newPointer = realloc(pointer, newSize);
    return newPointer;
}

static void freeObject(Object* obj) {
    switch (obj->type) {
        case FUNCTION_OBJECT:
            FunctionObject* function = (FunctionObject*)obj;
            freeSequence(&function->sequence);
            reallocate(obj, sizeof(FunctionObject), 0);
            break;
        case STRING_OBJECT:
            String* string = (String*)obj;
            reallocate(string->characters, sizeof(char) * (string->length + 1), 0);
            reallocate(obj, sizeof(String), 0);
            break;
    }
}

void freeObjects(Object* objects) {
    Object* obj = objects;
    while (obj != NULL) {
        Object* next = obj->next;
        freeObject(obj);
        obj = next;
    }
}