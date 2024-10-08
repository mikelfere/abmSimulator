#ifndef sequence_h
#define sequence_h

#include <stdint.h>

#include "value.h"

typedef enum {
    OP_LVALUE,
    OP_RVALUE,
    OP_PUSH,
    OP_POP,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_REMAINDER,
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_GREATER_EQUAL,
    OP_AND,
    OP_OR,
    OP_NOT,
    OP_PRINT,
    OP_SHOW,
    OP_COPY,
    OP_ASSIGN,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_JUMP_IF_TRUE,
    OP_CALL,
    OP_RETURN,
    OP_BEGIN,
    OP_END,
    OP_HALT,
} OpCode;

/**
 * struct Sequence - A sequence of byte OpCodes
 * @a: count -> int : current number of bytes
 * @b: capacity -> int : maximum number of bytes
 * @c: code -> uint8_t* : array of bytes
 * @d: constants -> ValueArray : stores all constants of current sequence
 * @e: line -> int* : stores line for each byte
 */
typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    ValueArray constants;
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
 * @brief Making use of writeValueArray(), add given value to
 * the values array contained in the given sequence. Return index
 * where value was added.
 * 
 * @param sequence 
 * @param value 
 * @return int 
 */
int addValue(Sequence* sequence, Value value);

#endif