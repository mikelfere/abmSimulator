#ifndef value_h
#define value_h

// Forward declarations of Object and String structs
typedef struct Object Object;
typedef struct String String;

typedef enum {
    NUM_VALUE,
    OBJ_VALUE,
} ValueType;

/**
 * struct Value - Stores an integer or object
 * @a: type -> ValueType : either NUM_VALUE or OBJ_VALUE
 * @b: as -> union { int number; Object* obj } : actual data
 */
typedef struct {
    ValueType type;
    union {
        int number;
        Object* obj;
    } as;
} Value;

/**
 * struct ValueArray - An array of Value(s)
 * @a: capacity -> int : maximum number of Value(s)
 * @b: count -> int : current number of Value(s)
 * @c: values -> Value* : The array of values
 */
typedef struct { 
    int capacity;
    int count;
    Value* values;
} ValueArray;

/**
 * @brief Initializes members of the given value array to default 
 * values.
 * 
 * @param array 
 */
void initValueArray(ValueArray* array);

/**
 * @brief Free memory used by the given value array and reinitialize
 * its members.
 * 
 * @param array 
 */
void freeValueArray(ValueArray* array);

/**
 * @brief Add given value to the given array. If array is full
 * reallocate the array and update capacity as necessary.
 * 
 * @param array 
 * @param value 
 */
void writeValueArray(ValueArray* array, Value value);

#endif