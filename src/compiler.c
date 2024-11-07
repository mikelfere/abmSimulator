#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "memoryBus.h"
#include "scanner.h"

Parser parser;
Compiler* currentCompiler = NULL;

static Sequence* currentSequence() {
    return &currentCompiler->function->sequence;
}

static void errorAt(Token* token, const char* message) {
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

static void writeByte(uint8_t byte) {
    writeSequence(currentSequence(), byte, parser.previous.line);
}

static void writeBytes(uint8_t byte1, uint8_t byte2) {
    writeByte(byte1);
    writeByte(byte2);
}

static uint8_t makeConstant(Value value) {
    int index = addValue(currentSequence(), value);
    if (index > UINT8_MAX) {
        error("Too many constants in one sequence.");
        return 0;
    }
    return (uint8_t)index;
}

static void initCompiler(VM* vm, Compiler* compiler, FunctionType type) {
    compiler->previousCompiler = currentCompiler;
    compiler->function = NULL;
    compiler->type = type;
    compiler->function = newFunction(vm);
    currentCompiler = compiler;
    if (type != TYPE_DEFAULT) {
        currentCompiler->function->name = copyString(&vm->strings, vm->objects, parser.previous.start, parser.previous.length);
    }
    initNameList(&currentCompiler->labelList);
}

static FunctionObject* endCompiler() {
    FunctionObject* function = currentCompiler->function;
    currentCompiler = currentCompiler->previousCompiler;
    return function;
}

static void declaration(VM* vm);

static void statement(VM* vm) {
    // printf("statement\n");
    if (matchToken(TOKEN_PUSH)) {
        writeByte(OP_PUSH);
        consumeToken(TOKEN_NUMBER, "Expected a number.");
        int num = atoi(parser.previous.start);
        uint8_t index = makeConstant((Value){NUM_VALUE, {.number = num}});
        writeByte(index);
        // printf("After push\n");
    } else if (matchToken(TOKEN_POP)) {
        writeByte(OP_POP);
        // printf("After pop\n");
    } else if (matchToken(TOKEN_SHOW)) {
        consumeToken(TOKEN_STRING, "Expected a string.");
        Token* name = &parser.previous; 
        uint8_t index = makeConstant((Value){OBJ_VALUE, {.obj = (Object*)copyString(&vm->strings, vm->objects, name->start, name->length)}});
        writeBytes(OP_SHOW, index);
        // printf("After show\n");
    } else if (matchToken(TOKEN_COPY)) {
        writeByte(OP_COPY);
        // printf("After copy\n");
    } else if (matchToken(TOKEN_ASSIGN)) {
        writeByte(OP_ASSIGN);
        // printf("After assign\n");
    } else if (matchToken(TOKEN_ASSIGN_ADDRESS)) {
        writeByte(OP_ASSIGN_ADDRESS);
    } else if (matchToken(TOKEN_HALT)) {
        writeByte(OP_HALT);
        // printf("After halt\n");
    } else if (matchToken(TOKEN_PLUS)) {
        writeByte(OP_ADD);
        // printf("After add\n");
    } else if (matchToken(TOKEN_MINUS)) {
        writeByte(OP_SUBTRACT);
        // printf("After subtract\n");
    } else if (matchToken(TOKEN_STAR)) {
        writeByte(OP_MULTIPLY);
        // printf("After multiply\n");
    } else if (matchToken(TOKEN_SLASH)) {
        writeByte(OP_DIVIDE);
        // printf("After divide\n");
    } else if (matchToken(TOKEN_DIV)) {
        writeByte(OP_REMAINDER);
        // printf("After remainder\n");
    } else if (matchToken(TOKEN_AMPERSAND)) {
        writeByte(OP_AND);
        // printf("After and\n");
    } else if (matchToken(TOKEN_VERTICAL_LINE)) {
        writeByte(OP_OR);
        // printf("After or\n");
    } else if (matchToken(TOKEN_EXCALMATION)) {
        writeByte(OP_NOT);
        // printf("After not\n");
    } else if (matchToken(TOKEN_NOT_EQUAL)) {
        writeByte(OP_NOT_EQUAL);
        // printf("After not equal\n");
    } else if (matchToken(TOKEN_EQUAL)) {
        writeByte(OP_EQUAL);
        // printf("After equal\n");
    } else if (matchToken(TOKEN_LESS_EQUAL)) {
        writeByte(OP_LESS_EQUAL);
        // printf("After less equal\n");
    } else if (matchToken(TOKEN_GREATER_EQUAL)) {
        writeByte(OP_GREATER_EQUAL);
        // printf("After greater equal\n");
    } else if (matchToken(TOKEN_LESS)) {
        writeByte(OP_LESS);
        // printf("After less\n");
    } else if (matchToken(TOKEN_GREATER)) {
        writeByte(OP_GREATER);
        // printf("After greater\n");
    } else if (matchToken(TOKEN_PRINT)) {
        writeByte(OP_PRINT);
        // printf("After print\n");
    } else if (matchToken(TOKEN_CALL)) {
        consumeToken(TOKEN_IDENTIFIER, "Expected a name.");
        Token* name = & parser.previous;
        uint8_t index = makeConstant((Value){OBJ_VALUE, {.obj = (Object*)copyString(&vm->strings, vm->objects, name->start, name->length)}});
        writeBytes(OP_CALL, index);
        // printf("After call\n");
    } else if (matchToken(TOKEN_RETURN)) {
        writeByte(OP_RETURN);
        // printf("After return\n");
    } else if (matchToken(TOKEN_LVALUE)) {
        consumeToken(TOKEN_IDENTIFIER, "Expected a name.");
        Token* name = &parser.previous;
        uint8_t index = makeConstant((Value){OBJ_VALUE, {.obj = (Object*)copyString(&vm->strings, vm->objects, name->start, name->length)}});
        writeBytes(OP_LVALUE, index);
        // printf("After lvalue\n");
    } else if (matchToken(TOKEN_RVALUE)) {
        consumeToken(TOKEN_IDENTIFIER, "Expected a name.");
        Token* name = &parser.previous;
        uint8_t index = makeConstant((Value){OBJ_VALUE, {.obj = (Object*)copyString(&vm->strings, vm->objects, name->start, name->length)}});
        writeBytes(OP_RVALUE, index);
        // printf("After rvalue\n");
    } else if (matchToken(TOKEN_GOTO)) {
        consumeToken(TOKEN_IDENTIFIER, "Expected a name.");
        Token* name = &parser.previous;
        uint8_t index = makeConstant((Value){OBJ_VALUE, {.obj = (Object*)copyString(&vm->strings, vm->objects, name->start, name->length)}});
        writeBytes(OP_JUMP, index);
        int jumpPoint = currentSequence()->count - 1;
        writeBytes(((jumpPoint >> 8) & 0xff), (jumpPoint & 0xff));
        int labelIndex = getNameIndex(&currentCompiler->labelList, name->start, name->length);
        if (labelIndex != -1) {
            currentCompiler->encounteredLabel[labelIndex] = true;
        } else {
            addName(&currentCompiler->labelList, name->start, name->length);
        }
        // printf("After goto\n");
    } else if (matchToken(TOKEN_GOFALSE)) {
        consumeToken(TOKEN_IDENTIFIER, "Expected a name.");
        Token* name = &parser.previous;
        uint8_t index = makeConstant((Value){OBJ_VALUE, {.obj = (Object*)copyString(&vm->strings, vm->objects, name->start, name->length)}});
        writeBytes(OP_JUMP_IF_FALSE, index);
        int jumpPoint = currentSequence()->count - 1;
        writeBytes(((jumpPoint >> 8) & 0xff), (jumpPoint & 0xff));
        int labelIndex = getNameIndex(&currentCompiler->labelList, name->start, name->length);
        if (labelIndex != -1) {
            currentCompiler->encounteredLabel[labelIndex] = true;
        } else {
            addName(&currentCompiler->labelList, name->start, name->length);
        }
        // printf("After gofalse\n");
    } else if (matchToken(TOKEN_GOTRUE)) {
        consumeToken(TOKEN_IDENTIFIER, "Expected a name.");
        Token* name = &parser.previous;
        uint8_t index = makeConstant((Value){OBJ_VALUE, {.obj = (Object*)copyString(&vm->strings, vm->objects, name->start, name->length)}});
        writeBytes(OP_JUMP_IF_TRUE, index);
        int jumpPoint = currentSequence()->count - 1;
        writeBytes(((jumpPoint >> 8) & 0xff), (jumpPoint & 0xff));
        int labelIndex = getNameIndex(&currentCompiler->labelList, name->start, name->length);
        if (labelIndex != -1) {
            currentCompiler->encounteredLabel[labelIndex] = true;
        } else {
            addName(&currentCompiler->labelList, name->start, name->length);
        }
        // printf("After gotrue\n");
    } else if (matchToken(TOKEN_BEGIN)) {
        writeByte(OP_BEGIN);
        // printf("After begin\n");
    } else if (matchToken(TOKEN_END)) {
        writeByte(OP_END);
        // printf("After end\n");
    }
}

static bool encounteredAllLabels() {
    for (int i = 0; i < currentCompiler->labelList.count; i++) {
        if (!currentCompiler->encounteredLabel[i]) {
            return false;
        }
    }
    return true;
}

static FunctionObject* compileFunction(VM* vm, FunctionType type) {
    Compiler compiler;
    // printf("init function Compiler\n");
    initCompiler(vm, &compiler, type);
    for (;;) {
        declaration(vm);

        if (checkToken(TOKEN_RETURN)) {
            if (encounteredAllLabels()) {
                break;
            }
        } else if (checkToken(TOKEN_EOF)) {
            break;
        }
    }
    consumeToken(TOKEN_RETURN, "Expected a return before end of function.");
    writeByte(OP_RETURN);
    // printf("ended function compiler\n");
    return endCompiler();
}

static void declaration(VM* vm) {
    // printf("declaration\n");
    if(matchToken(TOKEN_LABEL)){
        // printf("In label\n");
        consumeToken(TOKEN_IDENTIFIER, "Expected a name.");
        Token* name = &parser.previous;
        String* key = copyString(&vm->strings, vm->objects, name->start, name->length);
        // uint8_t index = makeConstant((Value){OBJ_VALUE, {.obj = (Object*)key}});
        if (isInNameList(&functionNames, name->start, name->length)) {
            // printf("New function\n");
            FunctionObject* function = compileFunction(vm, TYPE_FUNCTION);
            Value fun = (Value){OBJ_VALUE, {.obj = (Object*) function}};
            if (tableGetValue(&currentCompiler->function->labels, key, &fun) == -2) {
                error("Function already defined.");
            }
            tableSetValue(&currentCompiler->function->labels, key, fun);
        } else {
            // printf("New Label\n");
            int labelIndex = getNameIndex(&currentCompiler->labelList, name->start, name->length);
            if (labelIndex != -1) {
                currentCompiler->encounteredLabel[labelIndex] = true;
            } else {
                addName(&currentCompiler->labelList, name->start, name->length);
            }
            Value value = (Value){NUM_VALUE, {.number = currentSequence()->count}};
            if (tableGetValue(&currentCompiler->function->labels, key, &value) == -2) {
                error("Label already defined.");
            }
            tableSetValue(&currentCompiler->function->labels, key, value);
        }
        // printf("After label\n");
    } else {
        statement(vm);
    }
}

/**
 * @brief Sends a message to the bus to write the read names to
 * a chunk of memory.
 * 
 * @param socket_fd 
 */
static void writeName(int socket_fd) {
    char buffer[1024] = "add ";
    int j = 4;
    while (matchToken(TOKEN_IDENTIFIER)) {
        Token* name = &parser.previous;
        for (int i = 0; i < name->length; i++, j++) {
            buffer[j] = name->start[i];
        }
        buffer[j++] = ' ';
    }
    buffer[j] = '\0';
    if (write(socket_fd, buffer, 1024) <= 0) {
        perror("Could not write to bus");
        exit(-1);
    }
}

FunctionObject* compile(int socket_fd, VM* vm, const char* source) {
    initScanner(source);
    Compiler compiler;
    initCompiler(vm, &compiler, TYPE_DEFAULT);

    parser.hadError = false;

    advanceParser();
    if (matchToken(TOKEN_DATA)) {
        while (!matchToken(TOKEN_TEXT)) {
            while (matchToken(TOKEN_INT)) {
                writeName(socket_fd);
            }
        }
    }
    while (!matchToken(TOKEN_EOF)) {
        declaration(vm);
    }
    
    FunctionObject* function = endCompiler();
    return parser.hadError ? NULL : function;
}