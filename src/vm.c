#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "compiler.h"
#include "memmng.h"
#include "vm.h"

static void initStack(VM* vm) {
    vm->stackTop = vm->stack;
}

static void runtimeError(VM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf(format, args);
    va_end(args);
    fputs("\n", stdout);
    
    for (int i = vm->frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm->frames[i];
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

void initVM(VM* vm) {
    initStack(vm);
    vm->objects = NULL;
    vm->frameCount = 0;
    initTable(&vm->strings);
}

void freeVM(VM* vm) {
    freeTable(&vm->strings);
    freeObjects(vm->objects);
}

void push(VM* vm, Value value) {
    *vm->stackTop = value;
    vm->stackTop++;
}

Value pop(VM* vm) {
    vm->stackTop--;
    return *vm->stackTop;
}

static Value peek(VM* vm, int distance) {
    return vm->stackTop[-1 - distance];
}

static bool call(VM* vm, FunctionObject* function) {
    if(vm->frameCount == FRAMES_CAPACITY) {
        runtimeError(vm, "Stack overflow.");
        return false;
    }
    CallFrame* frame = &vm->frames[vm->frameCount++];
    frame->function = function;
    frame->ip = function->sequence.code;
    return true;
}

static bool callValue(VM* vm, Value callee) {
    if (callee.as.obj->type == FUNCTION_OBJECT) {
        return call(vm, (FunctionObject*)callee.as.obj);
    }
    runtimeError(vm, "Can only call functions.");
    return false;
}

/**
 * @brief Given the name of a variable, sends a message to the bus
 * to retrieve its address. The address will be stored in address
 * parametre. If name cannot be sent to bus, address will be set to -3.
 * If variable not found, the address will be set to -1.
 * 
 * @param socket_fd 
 * @param name 
 * @param address 
 */
static void getAddress(int socket_fd, Value name, int* address) {
    String* key = (String*)name.as.obj;
    char buffer[1024] = "rad ";
    // set address to -3 to indicate runtime error
    if (key->length > 1019) {
        *address = -3;
        return;
    }
    int i;
    for (i = 0; i < key->length; i++) {
        buffer[i + 4] = key->characaters[i];
    }
    buffer[i + 4] = '\0';
    if (write(socket_fd, buffer, 1024) <= 0) {
        perror("Could not write to bus");
        exit(-1);
    }
	memset(buffer, '\0', sizeof(buffer));
	if (read(socket_fd, buffer, sizeof(buffer)) <= 0) {
        perror("Could not read from bus");
        exit(-1);
    }
    // set address to -1 to indicate entry not set
    if (memcmp(buffer, "nfd", 3)  == 0) {
        *address = -1;
        return;
    } else {
        char num[1024];
        strncpy(num, buffer, 1024);
        *address = atoi(num);
    }
}

/**
 * @brief Sends message to the bus to retrieve the value stored
 * in the given address. The retrieved value will be stored in
 * value.
 * 
 * @param socket_fd 
 * @param address 
 * @param value 
 */
static void getValue(int socket_fd, int address, Value* value) {
    char buffer[1024] = "rvl ";
    char num[256];
    memset(num, '\0', sizeof(num));
    sprintf(num, "%d", address);
    int i = 0;
    while (num[i] != '\0') {
        buffer[i + 4] = num[i];
        i++;
    }
    buffer[i + 4] = '\0';
    if (write(socket_fd, buffer, 1024) <= 0) {
        perror("Could not write to bus");
        exit(-1);
    }
    memset(buffer, '\0', sizeof(buffer));
    if (read(socket_fd, buffer, sizeof(buffer)) <= 0) {
        perror("Could not read from bus");
        exit(-1);
    }
    int val = atoi(buffer);
    *value = (Value){NUM_VALUE, {.number = val}};
}

/**
 * @brief Sends message to the bus to get the alternative address and
 * boundaries of the variable stored in the given address. Stores the
 * informations provided by the bus in the array data. The data array 
 * will be assumed to contain 3 elements.
 * 
 * @param socket_fd 
 * @param address 
 * @param data 
 */
static void getAddressData(int socket_fd, int address, int* data) {
    char buffer[1024] = "rda ";
    char num[256];
    memset(num, '\0', sizeof(num));
    sprintf(num, "%d", address);
    int i = 0;
    while (num[i] != '\0') {
        buffer[i + 4] = num[i];
        i++;
    }
    buffer[i + 4] = '\0';
    i = 0;
    if (write(socket_fd, buffer, 1024) <= 0) {
        perror("Could not write to bus");
        exit(-1);
    }
	memset(buffer, '\0', sizeof(buffer));
	if (read(socket_fd, buffer, sizeof(buffer)) <= 0) {
        perror("Could not read from bus");
        exit(-1);
    }
    // set address to -1 to indicate entry not set
    if (memcmp(buffer, "nfd", 3) == 0) {
        data[0] = -1;
        return;
    } else {
        int j = 0, k = 0;
        char num[256];
        while (buffer[i] != '\0') {
            while (buffer[i] != ' ') {
                num[j++] = buffer[i++];
            }
            num[j] = '\0';
            data[k++] = atoi(num);
            j = 0;
            i++;
        }
    }
}

/**
 * @brief Sends message to the bus to write the given Value in the given
 * address
 * 
 * @param socket_fd 
 * @param value 
 * @param address 
 */
static void setValue(int socket_fd, Value value, int address) {
    char buffer[1024] = "wrv ";
    int i = 4, j = 0;
    char num[256];
    memset(num, '\0', sizeof(num));
    sprintf(num, "%d", address);
    while (num[j] != '\0') {
        buffer[i++] = num[j++]; 
    }
    buffer[i++] = ' ';
    memset(num, '\0', sizeof(num));
    sprintf(num, "%d", value.as.number);
    j = 0;
    while (num[j] != '\0') {
        buffer[i++] = num[j++]; 
    }
    buffer[i] = '\0';
    if (write(socket_fd, buffer, 1024) <= 0) {
        perror("Could not write to bus");
        exit(-1);
    }
}

static InterpretResult run(int socket_fd, VM* vm) {
    bool inReturn = false, isNumAddress = false;
    int address;
    CallFrame* frame = &vm->frames[vm->frameCount - 1];
    CallFrame* tempFrame = frame;
    for (;;) {
        uint8_t instruction;
        switch (instruction = (*frame->ip++)) {
            case OP_LVALUE: {
                Value name = frame->function->sequence.constants.values[*frame->ip++];
                Value val = (Value){NUM_VALUE, {.number = 0}};
                getAddress(socket_fd, name, &address);
                // If cannot search for name in global memory
                if (address == -3) {
                    runtimeError(vm, "Variable name cannot be larger than 1019 characters");
                } else if (address == -1) {     // If not defined in global memory
                    if (!inReturn) {
                        if (tableGetValue(&tempFrame->locals, (String*)name.as.obj, &val) == -1) {
                            tableSetValue(&tempFrame->locals, (String*)name.as.obj, val);
                        }
                    } else {
                        if (tableGetValue(&frame->locals, (String*)name.as.obj, &val) == -1) {
                            tableSetValue(&frame->locals, (String*)name.as.obj, val);
                        }
                    }
                }
                push(vm, name);
                break;
            }
            case OP_RVALUE: {
                Value name = frame->function->sequence.constants.values[*frame->ip++];
                Value val = (Value){NUM_VALUE, {.number = 0}};
                getAddress(socket_fd, name, &address);
                // If cannot search for name in global memory
                if (address == -3) {
                    runtimeError(vm, "Variable name cannot be larger than 1019 characters");
                } else if (address == -1) {     // If not defined in global memory
                    if (!inReturn) {
                        if (tableGetValue(&frame->locals, (String*)name.as.obj, &val) == -1) {
                            tableSetValue(&frame->locals, (String*)name.as.obj, val);
                        }
                    } else {
                        if (tableGetValue(&tempFrame->locals, (String*)name.as.obj, &val) == -1) {
                            tableSetValue(&tempFrame->locals, (String*)name.as.obj, val);
                        }
                    }
                } else {    // If found in global memory
                    getValue(socket_fd, address, &val);
                }
                push(vm, val);
                break;
            }
            case OP_ADD: {
                Value b = pop(vm);
                Value a = pop(vm);
                if (a.type == OBJ_VALUE) {
                    int address;
                    Value val;
                    // data[0] -> altAddress, data[1] = upper boundary, data[2] = lower boundary
                    int data[3];
                    getAddress(socket_fd, a, &address);
                    // If cannot search for name in global memory
                    if (address == -3) {
                        runtimeError(vm, "Variable name cannot be larger than 1019 characters");
                    } else if (address == -1) {     // If not defined in global memory 
                        address = tableGetValue(&frame->locals, (String*)a.as.obj, &val);
                        if (address < 0) {          // If not found in local memory either
                            runtimeError(vm, "Variable doesn't have an address");
                        }
                    } 
                    getAddressData(socket_fd, address, data);
                    if (address + b.as.number > data[1]) {
                        address = data[0];
                        getAddressData(socket_fd, data[0], data);
                        if (address + b.as.number > data[1]) {
                            runtimeError(vm, "Cannot access memory location");
                        } else {
                            push(vm, (Value){NUM_VALUE, {.number = address + b.as.number}});
                        }
                    } else {
                        push(vm, (Value){NUM_VALUE, {.number = address + b.as.number}});
                    }
                    isNumAddress = true;
                } else {
                    push(vm, (Value){NUM_VALUE, {.number = (a.as.number + b.as.number)}});
                }
                break;
            }
            case OP_SUBTRACT: {
                Value b = pop(vm);
                Value a = pop(vm);
                if (a.type == OBJ_VALUE) {
                    int address;
                    Value val;
                    // data[0] -> altAddress, data[1] = upper boundary, data[2] = lower boundary
                    int data[3];
                    getAddress(socket_fd, a, &address);
                    // If cannot search for name in global memory
                    if (address == -3) {
                        runtimeError(vm, "Variable name cannot be larger than 1019 characters");
                    } else if (address == -1) {     // If not defined in global memory 
                        address = tableGetValue(&frame->locals, (String*)a.as.obj, &val);
                        if (address < 0) {          // If not found in local memory either
                            runtimeError(vm, "Variable doesn't have an address");
                        }
                    } 
                    getAddressData(socket_fd, address, data);
                    if (address - b.as.number < data[2]) {
                        address = data[0];
                        getAddressData(socket_fd, data[0], data);
                        if (address - b.as.number < data[2]) {
                            runtimeError(vm, "Cannot access memory location");
                        } else {
                            push(vm, (Value){NUM_VALUE, {.number = address - b.as.number}});
                        }
                    } else {
                        push(vm, (Value){NUM_VALUE, {.number = address - b.as.number}});
                    }
                    isNumAddress = true;
                } else {
                    push(vm, (Value){NUM_VALUE, {.number = (a.as.number - b.as.number)}});
                }
                break;
            }
            case OP_MULTIPLY: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                push(vm, (Value){NUM_VALUE, {.number = (a * b)}});
                break;
            }
            case OP_DIVIDE: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                push(vm, (Value){NUM_VALUE, {.number = (a / b)}});
                break;
            }
            case OP_REMAINDER: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                push(vm, (Value){NUM_VALUE, {.number = (a % b)}});
                break;
            }
            case OP_AND: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                push(vm, (Value){NUM_VALUE, {.number = (a && b)}});
                break;
            }
            case OP_OR: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                push(vm, (Value){NUM_VALUE, {.number = (a || b)}});
                break;
            }
            case OP_NOT: {
                push(vm, (Value){NUM_VALUE, {.number = !(pop(vm).as.number)}});
                break;
            }
            case OP_NOT_EQUAL: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                int result = 0;
                if (a != b) {
                    result = 1;
                }
                push(vm, (Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_EQUAL: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                int result = 0;
                if (a == b) {
                    result = 1;
                }
                push(vm, (Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_LESS: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                int result = 0;
                if (a < b) {
                    result = 1;
                }
                push(vm, (Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_LESS_EQUAL: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                int result = 0;
                if (a <= b) {
                    result = 1;
                }
                push(vm, (Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_GREATER: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                int result = 0;
                if (a > b) {
                    result = 1;
                }
                push(vm, (Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_GREATER_EQUAL: {
                int b = pop(vm).as.number;
                int a = pop(vm).as.number;
                int result = 0;
                if (a >= b) {
                    result = 1;
                }
                push(vm, (Value){NUM_VALUE, {.number = result}});
                break;
            }
            case OP_PRINT: 
                printf("%d\n", peek(vm, 0).as.number);
                break;
            case OP_SHOW: {
                Value string = frame->function->sequence.constants.values[*frame->ip++];
                printf("%.*s", ((String*)string.as.obj)->length, ((String*)string.as.obj)->characaters);
                break;
            }
            case OP_POP: 
                pop(vm);
                break;
            case OP_PUSH: {
                Value val = frame->function->sequence.constants.values[*frame->ip++];
                push(vm, val);
                break;
            }
            case OP_COPY:
                push(vm, peek(vm, 0));
                break;
            case OP_ASSIGN: {
                Value val = pop(vm);
                Value name = pop(vm);
                Value temp;
                int address;
                getAddress(socket_fd, name, &address);
                // If cannot search for name in global memory
                if (address == -3) {
                    runtimeError(vm, "Variable name cannot be larger than 1019 characters");
                } else if (address == -1) {     // If variable is not globally defined
                    if (!inReturn) {
                        tableSetValue(&tempFrame->locals, (String*)name.as.obj, val);
                        address = tableGetValue(&tempFrame->locals, (String*)name.as.obj, &temp);
                    } else {
                        tableSetValue(&frame->locals, (String*)name.as.obj, val);
                        address = tableGetValue(&frame->locals, (String*)name.as.obj, &temp);
                    }
                    if (address >= 0) {
                        setValue(socket_fd, val, address);
                    }
                } else {
                    setValue(socket_fd, val, address);
                }                
                break;
            }
            case OP_ASSIGN_ADDRESS: {
                Value name2 = pop(vm);
                Value name1 = pop(vm);
                Value val = (Value){NUM_VALUE, {.number = 0}};
                if (isNumAddress) {
                    isNumAddress = false;
                    tableChangeAddress(&frame->locals, (String*)name1.as.obj, name2.as.number);
                    getValue(socket_fd, name2.as.number, &val);
                    tableSetValue(&frame->locals, (String*)name1.as.obj, val);
                    break;
                }
                int address;
                getAddress(socket_fd, name2, &address);
                // If cannot search for name in global memory
                if (address == -3) {
                    runtimeError(vm, "Variable name cannot be larger than 1019 characters");
                } else if (address == -1) {     // If variable is not globally defined
                    address = tableGetValue(&frame->locals, (String*)name2.as.obj, &val);
                } 
                if (address >= 0) {
                    tableChangeAddress(&frame->locals, (String*)name1.as.obj, address);
                    tableSetValue(&frame->locals, (String*)name1.as.obj, val);
                }
                break;
            }
            case OP_JUMP: {
                Value name = frame->function->sequence.constants.values[*frame->ip++];
                frame->ip += 2;
                int address = (int)(frame->ip[-2] << 8 | frame->ip[-1]);
                Value jumpToAddress;
                if (tableGetValue(&frame->function->labels, (String*)name.as.obj, &jumpToAddress) == -2) {
                    int offset = jumpToAddress.as.number - address - 3;
                    frame->ip += offset;
                } else {
                    runtimeError(vm, "No label defined for this jump.");
                }
                break;
            }
            case OP_JUMP_IF_TRUE: {
                Value name = frame->function->sequence.constants.values[*frame->ip++];
                frame->ip += 2;
                if (pop(vm).as.number != 0) {
                    int address = (int)(frame->ip[-2] << 8 | frame->ip[-1]);
                    Value jumpToAddress;
                    if (tableGetValue(&frame->function->labels, (String*)name.as.obj, &jumpToAddress) == -2) {
                        int offset = jumpToAddress.as.number - address - 3;
                        frame->ip += offset;
                    } else {
                        runtimeError(vm, "No label defined for this jump.");
                    }
                }
                break;
            }
            case OP_JUMP_IF_FALSE: {
                Value name = frame->function->sequence.constants.values[*frame->ip++];
                frame->ip += 2;
                if (pop(vm).as.number == 0) {
                    int address = (int)(frame->ip[-2] << 8 | frame->ip[-1]);
                    Value jumpToAddress;
                    if (tableGetValue(&frame->function->labels, (String*)name.as.obj, &jumpToAddress) == -2) {
                        int offset = jumpToAddress.as.number - address - 3;
                        frame->ip += offset;
                    } else {
                        runtimeError(vm, "No label defined for this jump.");
                    }
                }
                break;
            }
            case OP_BEGIN: {
                tempFrame = &vm->frames[vm->frameCount];
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
                if (tableGetValue(&(&vm->frames[0])->function->labels, (String*)name.as.obj, &function) == -1) {
                    runtimeError(vm, "No function with that name.");
                }
                if (!callValue(vm, function)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = tempFrame;
                break;
            }
            case OP_RETURN: {
                vm->frameCount--;
                frame = &vm->frames[vm->frameCount - 1];
                inReturn = true;
                break;
            }
            case OP_HALT: {
                return INTERPRET_OK;
            }
        }
    }
}

InterpretResult interpret(int socket_fd, VM* vm, const char* source) {
    FunctionObject* function = compile(socket_fd, vm, source);
    if (function == NULL) {
        return INTERPRET_COMPILE_ERROR;
    }
    
    push(vm, (Value){OBJ_VALUE, {.obj = (Object*)function}});
    call(vm, function);

    return run(socket_fd, vm);
}