#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "nameList.h"
#include "memmng.h"

void initNameList(NameList* list) {
    list->capacity = 0;
    list->count = 0;
}

int getNameIndex(NameList* list, const char* name, int length) {
    for (int i = 0; i < list->count; i++) {
        if(list->length[i] == length &&
            memcmp(list->names[i], name, length) == 0) {
            return i;
        }
    }
    return -1;
}

bool isInNameList(NameList* list, const char* name, int length) {
    return getNameIndex(list, name, length) != -1;
}

void addName(NameList* list, const char* name, int length) {
    if (isInNameList(list, name, length)) {
        return;
    }
    strncpy(list->names[list->count], name, length);
    list->length[list->count] = length;
    list->count++;
}

void removeName(NameList* list, const char* name, int length) {
    int index = getNameIndex(list, name, length);
    if (index != -1) {
        list->names[index][0] = '\0';
    }
}