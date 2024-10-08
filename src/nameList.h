#ifndef nameList_h
#define nameList_h

typedef struct{
    int capacity;
    int count;
    int removedCount;
    int length[256];
    char names[256][256];
} NameList;

void initNameList(NameList* list);
void addName(NameList* list, const char* name, int length);
void removeName(NameList* list, const char* name, int length);
bool isInNameList(NameList* list, const char* name, int length);
void printNames(NameList* list);
#endif