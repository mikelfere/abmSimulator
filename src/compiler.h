#ifndef compiler_h
#define compiler_h

#include <stdbool.h>
#include <stdint.h>

#include "object.h"
#include "scanner.h"
#include "vm.h"

typedef enum {
    TYPE_DEFAULT,
    TYPE_FUNCTION,
} FunctionType;

/**
 * struct Parser - Stores parsing data
 * @a: current -> Token : currently scanned Token
 * @b: previous -> Token : previously scanned Token
 * @c: hadError -> bool : indicates whether an error token was encountered
 */
typedef struct {
    Token current;
    Token previous;
    bool hadError;
} Parser;

/**
 * struct Compiler - Stores information to be used during compilation
 * @a: previousCompiler -> Compiler* : stores compiler of enclosing function
 * @b: function -> FunctionObject* : function to be returned when compilation is done, all OpCodes are stored here
 * @c: type -> FunctionType : checks if we are inside a user defined function
 * @d: labelList -> NameList : stores all names of labels inside a function
 * @e: encounteredLabel -> bool[] : keeps track if any label stored in labelList is encountered at least twice
 */
typedef struct Compiler {
    struct Compiler* previousCompiler;
    FunctionObject* function;
    FunctionType type;
    NameList labelList;
    bool encounteredLabel[256];
} Compiler;

/**
 * @brief 
 * 
 * @param vm 
 * @param source 
 * @return FunctionObject* 
 */
FunctionObject* compile(VM* vm, const char* source);

#endif