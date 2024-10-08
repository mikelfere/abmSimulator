#include <stdio.h>
#include <stdlib.h>

// #include<unistd.h>
// #include<windows.h>

#include "compiler.h"
#include "scanner.h"

Parser parser;
Compiler* currentCompiler = NULL;

static Sequence* currentSequence() {
    return &currentCompiler->function->sequence;
}

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) {
        return;
    }

    parser.panicMode = true;
    printf("--line %d-- Error", token->line);

    if (token->type == TOKEN_EOF) {
        printf(" at end");
    } else if (token->type != TOKEN_ERROR) {
        printf(" at '%.*s", token->length, token->start);
    }

    printf(": %s\n", message);
    parser.hadError = true;
}

static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

static void error(const char* message) {
    errorAt(&parser.previous, message);
}

static void advanceParser() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) {
            break;
        }

        errorAtCurrent(parser.current.start);
    }
}

static void consumeToken(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advanceParser();
        return;
    }
    errorAtCurrent(message);
}

static bool checkToken(TokenType type) {
    return parser.current.type == type;
}

static bool matchToken(TokenType type) {
    if (!checkToken(type)) {
        return false;
    }
    advanceParser();
    return true;
}

static void emitByte(uint8_t byte) {
    writeSequence(currentSequence(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static uint8_t makeConstant(Value value) {
    int index = addValue(currentSequence(), value);
    if (index > UINT8_MAX) {
        error("Too many constants in one sequence.");
        return 0;
    }
    return (uint8_t)index;
}

static void initCompiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = currentCompiler;
    compiler->function = NULL;
    compiler->type = type;
    compiler->function = newFunction();
    currentCompiler = compiler;
    if (type != TYPE_DEFAULT) {
        currentCompiler->function->name = copyString(parser.previous.start, parser.previous.length);
    }
    initNameList(&currentCompiler->labelList);
}

static FunctionObject* endCompiler() {
    FunctionObject* function = currentCompiler->function;
    currentCompiler = currentCompiler->enclosing;
    return function;
}

static void declaration();

static void statement() {
    // printf("statement\n");
    if (matchToken(TOKEN_PUSH)) {
        emitByte(OP_PUSH);
        consumeToken(TOKEN_NUMBER, "Expected a number.");
        int num = atoi(parser.previous.start);
        uint8_t index = makeConstant((Value){NUM_VALUE, {.number = num}});
        emitByte(index);
        // printf("After push\n");
    } else if (matchToken(TOKEN_POP)) {
        emitByte(OP_POP);
        // printf("After pop\n");
    } else if (matchToken(TOKEN_SHOW)) {
        consumeToken(TOKEN_STRING, "Expected a string.");
        Token* name = &parser.previous; 
        uint8_t index = makeConstant((Value){OBJ_VALUE, {.obj = (Object*)copyString(name->start, name->length)}});
        emitBytes(OP_SHOW, index);
        // printf("After show\n");
    } else if (matchToken(TOKEN_COPY)) {
        emitByte(OP_COPY);
        // printf("After copy\n");
    } else if (matchToken(TOKEN_ASSIGN)) {
        emitByte(OP_ASSIGN);
        // printf("After assign\n");
    } else if (matchToken(TOKEN_HALT)) {
        emitByte(OP_HALT);
        // printf("After halt\n");
    } else if (matchToken(TOKEN_PLUS)) {
        emitByte(OP_ADD);
        // printf("After add\n");
    } else if (matchToken(TOKEN_MINUS)) {
        emitByte(OP_SUBTRACT);
        // printf("After subtract\n");
    } else if (matchToken(TOKEN_STAR)) {
        emitByte(OP_MULTIPLY);
        // printf("After multiply\n");
    } else if (matchToken(TOKEN_SLASH)) {
        emitByte(OP_DIVIDE);
        // printf("After divide\n");
    } else if (matchToken(TOKEN_DIV)) {
        emitByte(OP_REMAINDER);
        // printf("After remainder\n");
    } else if (matchToken(TOKEN_AMPERSAND)) {
        emitByte(OP_AND);
        // printf("After and\n");
    } else if (matchToken(TOKEN_VERTICAL_LINE)) {
        emitByte(OP_OR);
        // printf("After or\n");
    } else if (matchToken(TOKEN_EXCALMATION)) {
        emitByte(OP_NOT);
        // printf("After not\n");
    } else if (matchToken(TOKEN_NOT_EQUAL)) {
        emitByte(OP_NOT_EQUAL);
        // printf("After not equal\n");
    } else if (matchToken(TOKEN_EQUAL)) {
        emitByte(OP_EQUAL);
        // printf("After equal\n");
    } else if (matchToken(TOKEN_LESS_EQUAL)) {
        emitByte(OP_LESS_EQUAL);
        // printf("After less equal\n");
    } else if (matchToken(TOKEN_GREATER_EQUAL)) {
        emitByte(OP_GREATER_EQUAL);
        // printf("After greater equal\n");
    } else if (matchToken(TOKEN_LESS)) {
        emitByte(OP_LESS);
        // printf("After less\n");
    } else if (matchToken(TOKEN_GREATER)) {
        emitByte(OP_GREATER);
        // printf("After greater\n");
    } else if (matchToken(TOKEN_PRINT)) {
        emitByte(OP_PRINT);
        // printf("After print\n");
    } else if (matchToken(TOKEN_CALL)) {
        consumeToken(TOKEN_IDENTIFIER, "Expected a name.");
        Token* name = & parser.previous;
        uint8_t index = makeConstant((Value){OBJ_VALUE, {.obj = (Object*)copyString(name->start, name->length)}});
        emitBytes(OP_CALL, index);
        // printf("After call\n");
    } else if (matchToken(TOKEN_RETURN)) {
        emitByte(OP_RETURN);
        // printf("After return\n");
    } else if (matchToken(TOKEN_LVALUE)) {
        consumeToken(TOKEN_IDENTIFIER, "Expected a name.");
        Token* name = &parser.previous;
        uint8_t index = makeConstant((Value){OBJ_VALUE, {.obj = (Object*)copyString(name->start, name->length)}});
        emitBytes(OP_LVALUE, index);
        // printf("After lvalue\n");
    } else if (matchToken(TOKEN_RVALUE)) {
        consumeToken(TOKEN_IDENTIFIER, "Expected a name.");
        Token* name = &parser.previous;
        uint8_t index = makeConstant((Value){OBJ_VALUE, {.obj = (Object*)copyString(name->start, name->length)}});
        emitBytes(OP_RVALUE, index);
        // printf("After rvalue\n");
    } else if (matchToken(TOKEN_GOTO)) {
        consumeToken(TOKEN_IDENTIFIER, "Expected a name.");
        Token* name = &parser.previous;
        uint8_t index = makeConstant((Value){OBJ_VALUE, {.obj = (Object*)copyString(name->start, name->length)}});
        emitBytes(OP_JUMP, index);
        int jumpPoint = currentSequence()->count - 1;
        emitBytes(((jumpPoint >> 8) & 0xff), (jumpPoint & 0xff));
        if (isInNameList(&currentCompiler->labelList, name->start, name->length)) {
            removeName(&currentCompiler->labelList, name->start, name->length);
        } else {
            addName(&currentCompiler->labelList, name->start, name->length);
        }
        // printf("After goto\n");
    } else if (matchToken(TOKEN_GOFALSE)) {
        consumeToken(TOKEN_IDENTIFIER, "Expected a name.");
        Token* name = &parser.previous;
        uint8_t index = makeConstant((Value){OBJ_VALUE, {.obj = (Object*)copyString(name->start, name->length)}});
        emitBytes(OP_JUMP_IF_FALSE, index);
        int jumpPoint = currentSequence()->count - 1;
        emitBytes(((jumpPoint >> 8) & 0xff), (jumpPoint & 0xff));
        if (isInNameList(&currentCompiler->labelList, name->start, name->length)) {
            removeName(&currentCompiler->labelList, name->start, name->length);
        } else {
            addName(&currentCompiler->labelList, name->start, name->length);
        }
        // printf("After gofalse\n");
    } else if (matchToken(TOKEN_GOTRUE)) {
        consumeToken(TOKEN_IDENTIFIER, "Expected a name.");
        Token* name = &parser.previous;
        uint8_t index = makeConstant((Value){OBJ_VALUE, {.obj = (Object*)copyString(name->start, name->length)}});
        emitBytes(OP_JUMP_IF_TRUE, index);
        int jumpPoint = currentSequence()->count - 1;
        emitBytes(((jumpPoint >> 8) & 0xff), (jumpPoint & 0xff));
        if (isInNameList(&currentCompiler->labelList, name->start, name->length)) {
            removeName(&currentCompiler->labelList, name->start, name->length);
        } else {
            addName(&currentCompiler->labelList, name->start, name->length);
        }
        // printf("After gotrue\n");
    } else if (matchToken(TOKEN_BEGIN)) {
        emitByte(OP_BEGIN);
        // printf("After begin\n");
    } else if (matchToken(TOKEN_END)) {
        emitByte(OP_END);
        // printf("After end\n");
    }
}

static FunctionObject* compileFunction(FunctionType type) {
    Compiler compiler;
    // printf("init function Compiler\n");
    initCompiler(&compiler, type);
    for (;;) {
        declaration();

        if (checkToken(TOKEN_RETURN)) {
            if (currentCompiler->labelList.removedCount == currentCompiler->labelList.count) {
                break;
            }
        } else if (checkToken(TOKEN_EOF)) {
            break;
        }
    }
    consumeToken(TOKEN_RETURN, "Expected a return before end of function.");
    emitByte(OP_RETURN);
    // printf("ended function compiler\n");
    return endCompiler();
}

static void declaration() {
    // printf("declaration\n");
    if(matchToken(TOKEN_LABEL)){
        // printf("In label\n");
        consumeToken(TOKEN_IDENTIFIER, "Expected a name.");
        Token* name = &parser.previous;
        String* key = copyString(name->start, name->length);
        uint8_t index = makeConstant((Value){OBJ_VALUE, {.obj = (Object*)key}});
        if (isInNameList(&functionNames, name->start, name->length)) {
            // printf("New function\n");
            FunctionObject* function = compileFunction(TYPE_FUNCTION);
            Value fun = (Value){OBJ_VALUE, {.obj = (Object*) function}};
            if (tableGet(&currentCompiler->function->labels, key, &fun)) {
                error("Function already defined.");
            }
            tableSet(&currentCompiler->function->labels, key, fun);
        } else {
            // printf("New Label\n");
            if (isInNameList(&currentCompiler->labelList, name->start, name->length)) {
                removeName(&currentCompiler->labelList, name->start, name->length);
            } else {
                addName(&currentCompiler->labelList, name->start, name->length);
            }
            Value value = (Value){NUM_VALUE, {.number = currentSequence()->count}};
            if (tableGet(&currentCompiler->function->labels, key, &value)) {
                error("Label already defined.");
            }
            tableSet(&currentCompiler->function->labels, key, value);
        }
        // printf("After label\n");
    } else {
        statement();
    }
}

FunctionObject* compile(const char* source) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_DEFAULT);

    parser.hadError = false;
    parser.panicMode = false;

    advanceParser();
    while (!matchToken(TOKEN_EOF)) {
        declaration();
    }
    
    FunctionObject* function = endCompiler();
    return parser.hadError ? NULL : function;
}