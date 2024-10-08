#include <stdio.h>
#include <stddef.h>

#include "value.h"
#include "memmng.h"

void initValueArray(ValueArray* array) {
    array->capacity = 0;
    array->count = 0;
    array->values = NULL;
}

void freeValueArray(ValueArray* array) {
    reallocate(array->values, sizeof(Value) * array->capacity, 0);
    initValueArray(array);
}

void writeValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int prevCapacity = array->capacity;
        array->capacity = prevCapacity < 16 ? 16 : prevCapacity * 2;
        array->values = (Value*)reallocate(array->values, \
        sizeof(Value) * prevCapacity, sizeof(Value) * array->capacity);
    }
    array->values[array->count++] = value;
}