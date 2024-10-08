#include <stdio.h>
#include <string.h>

#include <stdlib.h>

#include "scanner.h"

Scanner scanner;

void initScanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
    scanner.isInShow = false;
    scanner.isNextIdentifer = false;
}

static char peekNext();

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static bool isNumber(char c) {
    if ((c == '-' && isDigit(peekNext())) || \
        isDigit(c)) {
        return true;
    }
    return false;
}

static bool reachedEnd() {
    return *scanner.current == '\0';
}

static char advance() {
    scanner.current++;
    return scanner.current[-1];
}

static char peek() {
    return *scanner.current;
}

static char peekNext() {
    if (reachedEnd()) {
        return '\0';
    }
    return scanner.current[1];
}

static bool match(char character) {
    if (reachedEnd()) {
        return false;
    }
    if (*scanner.current != character) {
        return false;
    }
    scanner.current++;
    return true;
}

static Token makeToken(TokenType type) {
    Token token;
    token.type = type;
    token.line = scanner.line;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    return token;
}

static Token errorToken(const char* errorMessage) {
    Token token;
    token.type = TOKEN_ERROR;
    token.line = scanner.line;
    token.start = errorMessage;
    token.length = (int)(scanner.current - scanner.start);
    return token;
}

static void ignoreWhitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if (peekNext() == '/') {
                    while (peek() != '\n' && !reachedEnd()) {
                        advance();
                    }
                } else if (peekNext() == '*') {
                    while (!reachedEnd()) {
                        advance();
                        if (peek() == '*' && peekNext() == '/') {
                            advance();
                            advance();
                            break;
                        }
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static TokenType checkKeyword(int start, int length, \
    const char* word, TokenType type) {
    if (scanner.current - scanner.start == start + length && \
        memcmp(scanner.start + start, word, length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {
    switch (scanner.start[0]) {
        case 'b':
            return checkKeyword(1, 4, "egin", TOKEN_BEGIN);
        case 'c':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a':
                        if (scanner.current - scanner.start == 4 && \
                            memcmp(scanner.start + 2, "ll", 2) == 0) {
                                scanner.isNextIdentifer = true;
                                return TOKEN_CALL;
                            }
                        break;
                    case 'o':
                        return checkKeyword(2, 2, "py", TOKEN_COPY);
                }
            }
            break;
        case 'd':
            return checkKeyword(1, 2, "iv", TOKEN_DIV);
        case 'e':
            return checkKeyword(1, 2, "nd", TOKEN_END);
        case 'g':
            if (scanner.current - scanner.start > 1 && scanner.start[1] == 'o') {
                if (scanner.current - scanner.start > 2) {
                    switch (scanner.start[2]) {
                        case 'f':  
                            if (scanner.current - scanner.start == 7 && \
                                memcmp(scanner.start + 3, "alse", 4) == 0) {
                                scanner.isNextIdentifer = true;
                                return TOKEN_GOFALSE;
                            }
                            break;
                        case 't':
                            if (scanner.current - scanner.start > 3) {
                                switch (scanner.start[3]) {
                                    case 'o':  
                                        scanner.isNextIdentifer = true;
                                        return TOKEN_GOTO;
                                    case 'r':
                                        if (scanner.current - scanner.start == 6 && \
                                            memcmp(scanner.start + 4, "ue", 2) == 0) {
                                            scanner.isNextIdentifer = true;
                                            return TOKEN_GOTRUE;
                                        }
                                        break;
                                }
                            }
                            break;
                    }
                }
            }
            break;
        case 'h':
            return checkKeyword(1, 3, "alt", TOKEN_HALT);
        case 'l':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a':
                        if (scanner.current - scanner.start == 5 && \
                            memcmp(scanner.start + 2, "bel", 3) == 0) {
                                scanner.isNextIdentifer = true;
                                return TOKEN_LABEL;
                            }
                        break;
                    case 'v':
                        return checkKeyword(2, 4, "alue", TOKEN_LVALUE);
                }
            }
            break;
        case 'p':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'o':
                        return checkKeyword(2, 1, "p", TOKEN_POP);
                    case 'r':
                        return checkKeyword(2, 3, "int", TOKEN_PRINT);
                    case 'u':
                        return checkKeyword(2, 2, "sh", TOKEN_PUSH);
                }
            }
            break;
        case 'r':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'e':
                        return checkKeyword(2, 4, "turn", TOKEN_RETURN);
                    case 'v':
                        return checkKeyword(2, 4, "alue", TOKEN_RVALUE);
                }
            }
            break;
        case 's':
            if (scanner.current - scanner.start == 4 && \
                memcmp(scanner.start + 1, "how", 3) == 0) {
                scanner.isInShow = true;
                return TOKEN_SHOW;
            }
            break;
    }
    return TOKEN_IDENTIFIER;
}

static Token makeIdentifier() {
    while(isAlpha(peek()) || isDigit(peek()) || peek() == '_') {
        advance();
    }
    return makeToken(identifierType());
}

static Token makeNumber() {
    do {
        advance();
    } while (isDigit(peek()));

    return makeToken(TOKEN_NUMBER);
}

static Token makeString() {
    while(peek() != '\n') {
        advance();
    }
    Token token;
    token.type = TOKEN_STRING;
    token.line = scanner.line;
    token.start = scanner.start + 1;    // Add 1 to skip the first space after show keyword
    token.length = (int)(scanner.current - scanner.start);
    return token;
}

Token scanToken() {
    if (scanner.isInShow) {
        scanner.start = scanner.current;
        scanner.isInShow = false;
        return makeString();
    }

    ignoreWhitespace();
    scanner.start = scanner.current;

    if (reachedEnd()) {
        return makeToken(TOKEN_EOF);
    }

    char c = advance();

    if (isAlpha(c) || scanner.isNextIdentifer) {
        scanner.isNextIdentifer = false;
        return makeIdentifier();
    }

    if (isNumber(c)) {
        return makeNumber();
    }

    switch (c) {
        case '+':
            return makeToken(TOKEN_PLUS);
        case '-':
            return makeToken(TOKEN_MINUS);
        case '*':
            return makeToken(TOKEN_STAR);
        case '/':
            return makeToken(TOKEN_SLASH);
        case '&':
            return makeToken(TOKEN_AMPERSAND);
        case '|':
            return makeToken(TOKEN_VERTICAL_LINE);
        case '!':
            return makeToken(TOKEN_EXCALMATION);
        case '<':
            return makeToken(
                match('>') ? TOKEN_NOT_EQUAL : \
                    (match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS)
            );
        case '>':
            return makeToken(
                match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER
            );
        case '=':
            return makeToken(TOKEN_EQUAL);
        case ':':
            if (match('=')) {
                return makeToken(TOKEN_ASSIGN);
            }
            break;
    }
    
    return errorToken("Unexpected character.");
}

void getFunctionNames(NameList* functionNames, const char* source) {
    initScanner(source);
    for (;;) {
        ignoreWhitespace();
        scanner.start = scanner.current;

        if (reachedEnd()) {
            return;
        }

        char c = advance();

        if (isAlpha(c)) {
            while(isAlpha(peek()) || isDigit(peek()) || peek() == '_') {
                advance();
            }

            switch (c) {
                case 'c':
                    if (scanner.current - scanner.start == 4 && \
                                memcmp(scanner.start + 1, "all", 3) == 0) {
                        while (peek() == ' ') {
                            advance();
                        }
                        scanner.start = scanner.current;
                        while (isAlpha(peek()) || isDigit(peek()) || peek() == '_') {
                            advance();
                        }
                        
                        addName(functionNames, scanner.start, (int)(scanner.current - scanner.start));
                        
                    }
                    break;
                case 's':
                    if (scanner.current - scanner.start == 4 && \
                        memcmp(scanner.start + 1, "how", 3) == 0) {
                        while (peek() != '\n') {
                            advance();
                        }
                        advance();
                    }
                    break;
            }
        }

    }
}