# Compiler and flags
CC = gcc
CFLAGS = -Wall -g -pthread

# Project directory
PD = src

# Executable names
TARGETS = abm.exe bus.exe

# Source files
ABM_SOURCES = $(PD)/main.c $(PD)/compiler.c $(PD)/memmng.c \
$(PD)/nameList.c $(PD)/object.c $(PD)/scanner.c $(PD)/sequence.c \
$(PD)/symbolTable.c $(PD)/value.c $(PD)/vm.c $(PD)/cache.c

BUS_SOURCES = $(PD)/memoryBus.c $(PD)/object.c $(PD)/sequence.c \
$(PD)/memmng.c $(PD)/symbolTable.c $(PD)/value.c \
$(PD)/nameList.c $(PD)/scanner.c $(PD)/cache.c

# Object files
ABM_OBJECTS = $(ABM_SOURCES:.c=.o)
BUS_OBJECTS = $(BUS_SOURCES:.c=.o)

# Rule to build all targets
all: $(TARGETS)

# Rule to build abm.exe
abm.exe: $(ABM_OBJECTS)
	$(CC) $(CFLAGS) -o abm.exe $(ABM_OBJECTS)

# Rule to build bus.exe
bus.exe: $(BUS_OBJECTS)
	$(CC) -o bus.exe $(BUS_OBJECTS)

# Rule to build object files
%.o: $(PD)/%.c
	$(CC) -c $< -o %@

# Clean up build files
clean:
	rm -f $(ABM_OBJECTS) $(BUS_OBJECTS) $(TARGETS)