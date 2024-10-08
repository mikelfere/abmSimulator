#include "memmng.h"
#include "sequence.h"

void initSequence(Sequence* sequence) {
    sequence->count = 0;
    sequence->capacity = 0;
    sequence->code = NULL;
    sequence->lines = NULL;
    initValueArray(&sequence->constants);
}

void freeSequence(Sequence* sequence) {
    reallocate(sequence->code, sizeof(uint8_t) * sequence->capacity, 0);
    reallocate(sequence->lines, sizeof(int) * sequence->capacity, 0);
    freeValueArray(&sequence->constants);
    initSequence(sequence);
}

void writeSequence(Sequence* sequence, uint8_t byte, int line) {
    if (sequence->capacity < sequence->count + 1) {
        int prevCapacity = sequence->capacity;
        sequence->capacity = prevCapacity < 16 ? 16 : prevCapacity * 2;
        sequence->code = (uint8_t*)reallocate((void*)sequence->code, \
        sizeof(uint8_t) * prevCapacity, \
        sizeof(uint8_t) * sequence->capacity);
        sequence->lines = (int*)reallocate((void*)sequence->lines, \
        sizeof(int) * prevCapacity, sizeof(int) * sequence->capacity);
    }

    sequence->code[sequence->count] = byte;
    sequence->lines[sequence->count] = line;
    sequence->count++;
}

int addValue(Sequence* sequence, Value value){
    writeValueArray(&sequence->constants, value);
    return sequence->constants.count - 1;
}