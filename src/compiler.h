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
 * struct Local - Stores information about a local variable
 * @a: name -> Token
 * @b: depth -> int
 */
typedef struct {
    Token name;
    int depth;
} Local;

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef struct Compiler {
    struct Compiler* enclosing;
    FunctionObject* function;
    FunctionType type;
    NameList labelList;
} Compiler;

FunctionObject* compile(const char* source);

#endif