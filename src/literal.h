#ifndef literal_h
#define literal_h

typedef int Literal;

typedef struct { 
    int capacity;
    int count;
    Literal* literals;
} LiteralArray;

/**
 * @brief Initializes members of the given literal array to default 
 * values.
 * 
 * @param array 
 */
void initLiteralArray(LiteralArray* array);

/**
 * @brief Free memory used by the given literal array and reinitialize
 * its members.
 * 
 * @param array 
 */
void freeLiteralArray(LiteralArray* array);

/**
 * @brief Add given literal to the given array. If array is full
 * reallocate the array and update capacity as necessary.
 * 
 * @param array 
 * @param literal 
 */
void writeLiteralArray(LiteralArray* array, Literal literal);

/**
 * @brief Handles printing of a literal value.
 * 
 * @param literal 
 */
void printLiteral(Literal literal);

#endif