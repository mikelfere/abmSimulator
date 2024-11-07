#ifndef vm_h
#define vm_h

#include <stddef.h>

#include "object.h"
#include "sequence.h"
#include "symbolTable.h"

#define FRAMES_CAPACITY 128
#define STACK_CAPACITY (FRAMES_CAPACITY * (UINT8_MAX + 1))

/**
 * struct CallFrame - Used to manage function Calls
 * @a: function -> FunctionObject* : stores function data
 * @b: ip -> uint8_t* : stores a pointer to the first instruction
 * inside the function
 * @c: locals -> Table : stores all local variables of the function
 */
typedef struct {
    FunctionObject* function;
    uint8_t* ip;
    Table locals;
} CallFrame;

/**
 * struct VM - Stores all data the VM will need -
 * typedef for struct created as forward declaration in "object.h"
 * @a: frames -> CallFrame[] : stores all frames created from calls 
 * @b: frameCount -> int : stores the number of frames
 * @c: stack -> Value[] : contains all constants (number or object)
 * @d: stackTop -> Value* : keeps track of the top of stack
 * @e: strings -> Table : stores all strings
 * @f: objects -> Object* : Array of objects
 */
struct VM {
    CallFrame frames[FRAMES_CAPACITY];
    int frameCount;
    Value stack[STACK_CAPACITY];
    Value* stackTop;
    Table strings;
    Object* objects;
};

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

/**
 * @brief Sets up all members of vm to their default values
 * 
 * @param vm
 */
void initVM(VM* vm);

/**
 * @brief Frees all array members of vm and reinitializes everything
 * to default values
 * 
 * @param vm
 */
void freeVM(VM* vm);

/**
 * @brief The starting point of the VM. Given the source code, sets up
 * the first compiler, calls the main function, and executes run(),
 * which interprets all OpCodes in the sequence generated from compiler.
 * 
 * @param vm
 * @param source
 * @return InterpretResult 
 */
InterpretResult interpret(int socket_fd, VM* vm, const char* source);

/**
 * @brief Pushes the given value to vm's Value stack and increments 
 * stackTop to keep track.
 * 
 * @param vm
 * @param value 
 */
void push(VM* vm, Value value);

/**
 * @brief Pops and returns the value at the top of vm's Value stack.
 * Decrements stackTop to keep track.
 * 
 * @param vm
 * @return Value 
 */
Value pop(VM* vm);

#endif