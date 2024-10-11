#ifndef nameList_h
#define nameList_h

/**
 * struct NameList
 * @a: capacity -> int : stores capacity of list
 * @b: count -> int : stores number of elements
 * @c: removedCount -> int : stores number of deleted elements
 * @d: length -> int[256] : stores length of every string in names
 * @e: names -> char[256][256] : array of strings (names)
 */
typedef struct{
    int capacity;
    int count;
    int removedCount;
    int length[256];
    char names[256][256];
} NameList;

/**
 * @brief Initializes members of NameList to default values
 * 
 * @param list 
 */
void initNameList(NameList* list);

/**
 * @brief Adds a string to the name list if not currently stored
 * 
 * @param list 
 * @param name 
 * @param length 
 */
void addName(NameList* list, const char* name, int length);

/**
 * @brief Removes the given name from the list if present
 * 
 * @param list 
 * @param name 
 * @param length 
 */
void removeName(NameList* list, const char* name, int length);

/**
 * @brief Checks if the name of the given length is in given list
 * 
 * @param list 
 * @param name 
 * @param length 
 * @return true 
 * @return false 
 */
bool isInNameList(NameList* list, const char* name, int length);
// void printNames(NameList* list);
#endif