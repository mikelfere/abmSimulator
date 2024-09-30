#include <stddef.h>

#include "literal.h"
#include "memory.h"

void initLiteralArray(LiteralArray* array) {
    array->capacity = 0;
    array->count = 0;
    array->literals = NULL;
}

void freeLiteralArray(LiteralArray* array) {
    reallocate(array->literals, sizeof(Literal) * array->capacity, 0);
    initLiteralArray(array);
}

void writeLiteralArray(LiteralArray* array, Literal literal) {
    if (array->capacity < array->count + 1) {
        int prevCapacity = array->capacity;
        array->capacity = prevCapacity < 16 ? 16 : prevCapacity * 2;
        array->literals = (Literal*)reallocate(array->literals, \
        sizeof(Literal) * prevCapacity, sizeof(Literal) * array->capacity);
    }
    array->literals[array->count++] = literal;
}

void printLiteral(Literal literal) {
    printf("%d", literal);
}