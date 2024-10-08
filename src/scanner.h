#ifndef scanner_h
#define scanner_h

#include <stdbool.h>

#include "NameList.h"

extern NameList functionNames;

typedef enum {
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_AMPERSAND,
    TOKEN_EXCALMATION,
    TOKEN_VERTICAL_LINE,
    TOKEN_NOT_EQUAL,
    TOKEN_EQUAL,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER_EQUAL,
    TOKEN_LESS,
    TOKEN_GREATER,
    TOKEN_PUSH,
    TOKEN_POP,
    TOKEN_RVALUE,
    TOKEN_LVALUE,
    TOKEN_ASSIGN,
    TOKEN_COPY,
    TOKEN_LABEL,
    TOKEN_GOTO,
    TOKEN_GOFALSE,
    TOKEN_GOTRUE,
    TOKEN_HALT,
    TOKEN_DIV,
    TOKEN_PRINT,
    TOKEN_SHOW,
    TOKEN_BEGIN,
    TOKEN_END,
    TOKEN_RETURN,
    TOKEN_CALL,
    TOKEN_STRING,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_ERROR,
    TOKEN_EOF,
} TokenType;

/**
 * struct Token - Stores a token
 * @a: type -> TokenType (enum)
 * @b: start -> const char*
 * @c: length -> int
 * @d: line -> int
 *
 * Beginning from start to start + length, contains the 
 * name of Token. line stores the in-program line of token.
 */
typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

/**
 * struct Scanner - Scanner that goes through source code
 * @a: start -> const char*
 * @b: current -> const char*
 * @c: line -> int
 *
 * Keeps track of the first character address in current
 * iteration and also of the current character being read.
 * It also keeps track of the in-program line.
 */
typedef struct {
    const char* start;
    const char* current;
    int line;
    bool isInShow;
    bool isNextIdentifer;
} Scanner;


/**
 * @brief Initializes the members of Scanner to default values
 * beginning from the very first character of source code.
 * 
 * @param source 
 */
void initScanner(const char* source);

/**
 * @brief Reads next character using members of Scanner and makes
 * tokens as necessary along the way.
 * 
 * @return Token 
 */
Token scanToken();

void getFunctionNames(NameList* functionNames, const char* source);

#endif