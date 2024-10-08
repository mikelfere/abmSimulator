#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "compiler.h"
#include "memmng.h"
#include "vm.h"

// Initial version is a global variable
// To use multiple instances we will have to handle a VM* pointer instead
VM vm;

static void initStack() {
    vm.stackTop = vm.stack;
}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf(format, args);
    va_end(args);
    fputs("\n", stdout);
    
    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        FunctionObject* function = frame->function;
        size_t instruction = frame->ip - function->sequence.code - 1;
        printf("--line %d-- in ", function->sequence.lines[instruction]);
        if (function->name == NULL) {
            printf("main program\n");
        } else {
            printf("%s()\n", function->name->characaters);
        }
    }
}

void initVM() {
    initStack();
    vm.objects = NULL;
    vm.frameCount = 0;
    initTable(&vm.strings);
}

void freeVM() {
    freeTable(&vm.strings);
    freeObjects();
}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static bool call(FunctionObject* function) {
    if(vm.frameCount == FRAMES_CAPACITY) {
        runtimeError("Stack overflow.");
        return false;
    }
    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->function = function;
    frame->ip = function->sequence.code;
    return true;
}

static bool callValue(Value callee) {
    if (callee.as.obj->type == FUNCTION_OBJECT) {
        return call((FunctionObject*)callee.as.obj);
    }
    runtimeError("Can only call functions.");
    return false;
}

static InterpretResult run() {
    bool inReturn = false;
    CallFrame* frame = &vm.frames[vm.frameCount - 1];
    CallFrame* tempFrame = frame;
    for (;;) {
        uint8_t instruction;
        switch (instruction = (*frame->ip++)) {
            case OP_LVALUE: {
                Value name = frame->function->sequence.constants.values[*frame->ip++];
                Value val = (Value){NUM_VALUE, {.number = 0}};
                if (!inReturn) {
                    if (!tableGet(&tempFrame->locals, (String*)name.as.obj, &val)) {
                        tableSet(&tempFrame->locals, (String*)name.as.obj, val);
                    }
                } else {
                    if (!tableGet(&frame->locals, (String*)name.as.obj, &val)) {
                        tableSet(&frame->locals, (String*)name.as.obj, val);
                    }
                }
                push(name);
                break;
            }
            case OP_RVALUE: {
                Value name = frame->function->sequence.constants.values[*frame->ip++];
                Value val = (Value){NUM_VALUE, {.number = 0}};
                if (!inReturn) {
                    if (!tableGet(&frame->locals, (String*)name.as.obj, &val)) {
                        tableSet(&frame->locals, (String*)name.as.obj, val);
                    }
                } else {
                    if (!tableGet(&tempFrame->locals, (String*)name.as.obj, &val)) {
                        tableSet(&tempFrame->locals, (String*)name.as.obj, val);
                    }
                }
                push(val);
                break;
            }
            case OP_ADD: {
                int b = pop().as.number;
                int a = pop().as.number;
                push((Value){NUM_VALUE, {.number = (a + b)}});
                break;
            }
            case OP_SUBTRACT: {
                int b = pop().as.number;
                int a = pop().as.number;
                push((Value){NUM_VALUE, {.number = (a - b)}});
                break;
            }
            case OP_MULTIPLY: {
                int b = pop().as.number;
                int a = pop().as.number;
                push((Value){NUM_VALUE, {.number = (a * b)}});
                break;
            }
            case OP_DIVIDE: {
                int b = pop().as.number;
                int a = pop().as.number;
                push((Value){NUM_VALUE, {.number = (a / b)}});
                break;
            }
            case OP_REMAINDER: {
                int b = pop().as.number;
                int a = pop().as.number;
                push((Value){NUM_VALUE, {.number = (a % b)}});
                break;
            }
            case OP_AND: {
                int b = pop().as.number;
                int a = pop().as.number;
                push((Value){NUM_VALUE, {.number = (a && b)}});
                break;
            }
            case OP_OR: {
                int b = pop().as.number;
                int a = pop().as.number;
                push((Value){NUM_VALUE, {.number = (a || b)}});
                break;
            }
            case OP_NOT: {
                push((Value){NUM_VALUE, {.number = !(pop().as.number)}});
                break;
            }
            case OP_NOT_EQUAL: {
                int b = pop().as.number;
                int a = pop().as.number;
                int result = 0;
                if (a != b) {
                    result = 1;
                }
                push((Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_EQUAL: {
                int b = pop().as.number;
                int a = pop().as.number;
                int result = 0;
                if (a == b) {
                    result = 1;
                }
                push((Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_LESS: {
                int b = pop().as.number;
                int a = pop().as.number;
                int result = 0;
                if (a < b) {
                    result = 1;
                }
                push((Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_LESS_EQUAL: {
                int b = pop().as.number;
                int a = pop().as.number;
                int result = 0;
                if (a <= b) {
                    result = 1;
                }
                push((Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_GREATER: {
                int b = pop().as.number;
                int a = pop().as.number;
                int result = 0;
                if (a > b) {
                    result = 1;
                }
                push((Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_GREATER_EQUAL: {
                int b = pop().as.number;
                int a = pop().as.number;
                int result = 0;
                if (a >= b) {
                    result = 1;
                }
                push((Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_PRINT: 
                printf("%d\n", peek(0).as.number);
                break;
            case OP_SHOW: {
                Value string = frame->function->sequence.constants.values[*frame->ip++];
                printf("%.*s", ((String*)string.as.obj)->length, ((String*)string.as.obj)->characaters);
                break;
            }
            case OP_POP: 
                pop();
                break;
            case OP_PUSH: {
                Value val = frame->function->sequence.constants.values[*frame->ip++];
                push(val);
                break;
            }
            case OP_COPY:
                push(peek(0));
                break;
            case OP_ASSIGN: {
                Value val = pop();
                Value name = pop();
                if (!inReturn) {
                    tableSet(&tempFrame->locals, (String*)name.as.obj, val);
                } else {
                    tableSet(&frame->locals, (String*)name.as.obj, val);
                }
                break;
            }
            case OP_JUMP: {
                Value name = frame->function->sequence.constants.values[*frame->ip++];
                frame->ip += 2;
                int address = (int)(frame->ip[-2] << 8 | frame->ip[-1]);
                Value jumpToAddress;
                if (tableGet(&frame->function->labels, (String*)name.as.obj, &jumpToAddress)) {
                    int offset = jumpToAddress.as.number - address - 3;
                    frame->ip += offset;
                } else {
                    runtimeError("No label defined for this jump.");
                }
                break;
            }
            case OP_JUMP_IF_TRUE: {
                Value name = frame->function->sequence.constants.values[*frame->ip++];
                frame->ip += 2;
                if (pop().as.number != 0) {
                    int address = (int)(frame->ip[-2] << 8 | frame->ip[-1]);
                    Value jumpToAddress;
                    if (tableGet(&frame->function->labels, (String*)name.as.obj, &jumpToAddress)) {
                        int offset = jumpToAddress.as.number - address - 3;
                        frame->ip += offset;
                    } else {
                        runtimeError("No label defined for this jump.");
                    }
                }
                break;
            }
            case OP_JUMP_IF_FALSE: {
                Value name = frame->function->sequence.constants.values[*frame->ip++];
                frame->ip += 2;
                if (pop().as.number == 0) {
                    int address = (int)(frame->ip[-2] << 8 | frame->ip[-1]);
                    Value jumpToAddress;
                    if (tableGet(&frame->function->labels, (String*)name.as.obj, &jumpToAddress)) {
                        int offset = jumpToAddress.as.number - address - 3;
                        frame->ip += offset;
                    } else {
                        runtimeError("No label defined for this jump.");
                    }
                }
                break;
            }
            case OP_BEGIN: {
                tempFrame = &vm.frames[vm.frameCount];
                initTable(&tempFrame->locals);
                break;
            }
            case OP_END: {
                freeTable(&tempFrame->locals);
                tempFrame = frame;
                inReturn = false;
                break;
            }
            case OP_CALL: {
                Value name = frame->function->sequence.constants.values[*frame->ip++];
                Value function;
                if (!tableGet(&(&vm.frames[0])->function->labels, (String*)name.as.obj, &function)) {
                    runtimeError("No function with that name.");
                }
                if (!callValue(function)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = tempFrame;
                break;
            }
            case OP_RETURN: {
                vm.frameCount--;
                frame = &vm.frames[vm.frameCount - 1];
                inReturn = true;
                break;
            }
            case OP_HALT: {
                return INTERPRET_OK;
            }
        }
    }
}

InterpretResult interpret(const char* source) {
    FunctionObject* function = compile(source);
    if (function == NULL) {
        return INTERPRET_COMPILE_ERROR;
    }
    
    push((Value){OBJ_VALUE, {.obj = (Object*)function}});
    call(function);

    return run();
}