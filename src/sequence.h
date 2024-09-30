#ifndef sequence_h
#define sequence_h

#include <stdint.h>

#include "literal.h"

typedef enum {
    OP_CONSTANT,
    OP_HALT,
} OpCode;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    LiteralArray constants;
    int* lines;
} Sequence;

/**
 * @brief Initializes members of the given sequence to default values.
 * 
 * @param sequence 
 */
void initSequence(Sequence* sequence);

/**
 * @brief Free memory used by the given sequence and reinitialize its 
 * members.
 * 
 * @param sequence 
 */
void freeSequence(Sequence* sequence);

/**
 * @brief Add given byte to the code array of given sequence. If array
 * is full, reallocate the array and update capacity as necessary.
 * 
 * @param sequence 
 * @param byte 
 * @param line
 */
void writeSequence(Sequence* sequence, uint8_t byte, int line);

/**
 * @brief Making use of writeLiteralArray(), add given literal to
 * the literals array contained in the given sequence. Return index
 * where literal was added.
 * 
 * @param sequence 
 * @param literal 
 * @return int 
 */
int addLiteral(Sequence* sequence, Literal literal);

#endif