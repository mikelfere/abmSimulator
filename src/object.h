#ifndef object_h
#define object_h

#include <stdbool.h>

#include "sequence.h"
#include "symbolTable.h"
#include "value.h"

typedef struct VM VM;

typedef enum {
    FUNCTION_OBJECT,
    STRING_OBJECT,
} ObjectType;

/**
 * struct Object - Linked list of objects
 * @a: type -> ObjectType : either FUNCTION_OBJECT or STRING_OBJECT
 * @b: next -> Object* : pointer to the next Object (linked list)
 * 
 * Description: The Object struct stores pointers to String structs
 * and FunctionObject structs. An Object pointer can be safely 
 * cast to either of those 2 thanks to the C standard definition,
 * which mandates that struct members' addresses increase in the
 * order in which they are declared.
 */
typedef struct Object {
    ObjectType type;
    struct Object* next;
} Object;

/**
 * struct String - Representation of a string object
 * @a: obj -> Object : the Object pointing to the String
 * @b: length -> int : number of characters in string
 * @c: characters -> char* : the string itself
 * @d: hash -> uint32_t : the hash value of the string
 */
typedef struct String {
    Object obj;
    int length;
    char* characaters;
    uint32_t hash;
} String;

/**
 * struct FunctionObject - Representation of a function
 * @a: obj -> Object : the Object pointing to the function
 * @b: sequence -> Sequence : the sequence of bytes contained in the function
 * @c: name -> String* : the name of the function
 */
typedef struct {
    Object obj;
    Sequence sequence;
    String* name;
    Table labels;
} FunctionObject;

/**
 * @brief Allocates space for a new FunctionObject* and initializes
 * its members to default values. Returns the new FunctionObject*.
 * 
 * @param vm
 * @return FunctionObject* 
 */
FunctionObject* newFunction(VM* vm);

/**
 * @brief Creates a new String* object by copying the given array
 * of characters, under the assumption that it may be needed by the
 * caller.
 * 
 * @param table
 * @param objects
 * @param characters 
 * @param length 
 * @return String* 
 */
String* copyString(Table* table, Object* objects, const char* characters, int length);

static inline bool isObjectType(Value value, ObjectType type) {
    return value.type == OBJ_VALUE && value.as.obj->type == type;
}

#endif