#ifndef symbolTable_h
#define symbolTable_h

#include "value.h"

/**
 * struct Entry - Represents a hash table entry
 * @a: key -> String* : "index" of entry (has to be unique)
 * @b: value -> Value : stores the actual value corresponding to the "index"
 */
typedef struct {
    String* key;
    Value value;
    int address;
} Entry;

/**
 * struct Table - Hash table that stores all variable names in program
 * @a: count -> int : number of entries
 * @b: capacity -> int : maximum number of entries
 * @c: entries -> Entry* : Array of entries
 */
typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

/**
 * @brief Initializes the given table's members to default values.
 * 
 * @param table 
 */
void initTable(Table* table);

/**
 * @brief Frees the entries array of the table and reinitializes all
 * members.
 * 
 * @param table 
 */
void freeTable(Table* table);

/**
 * @brief Searches the given table to find the given key. If found returns
 * true and assigns corresponding value to given value argument. Returns -2
 * if entry is declared in local scope, -1 if entry is not defined, and the
 * address if it refers to global memory.
 * 
 * @param table 
 * @param key 
 * @param value 
 * @return int  
 */
int tableGetValue(Table* table, String* key, Value* value);

/**
 * @brief Checks if table contains more entries than 75% of its capacity.
 * Reallocates a new table with double the capacity if true. If the given 
 * key isn't found in the table, create new entry and assign given value.
 * Update count as necessary. Returns true if set Entry is new.
 * 
 * @param table 
 * @param key 
 * @param value 
 * @return true 
 * @return false 
 */
bool tableSetValue(Table* table, String* key, Value value);

/**
 * @brief Checks if table contains more entries than 75% of its capacity.
 * Reallocates a new table with double the capacity if true. If the given 
 * key isn't found in the table, create new entry and assign given value.
 * Set address of entry to given address. Update count as necessary.
 * 
 * @param table 
 * @param key 
 * @param value 
 * @param address 
 * @return true 
 * @return false 
 */
bool tableSetValueAddress(Table* table, String* key, Value value, int* address);

bool tableSetAddress(Table* table, String* key, int* address);

bool tableChangeAddress(Table* table, String* key, int address);

/**
 * @brief Check if any entry's key in the given table is equal to the given
 * string. If found, returns the String object, else returns NULL
 * 
 * @param table 
 * @param characaters 
 * @param length 
 * @param hash 
 * @return String* 
 */
String* findString(Table* table, const char* characaters, int length, uint32_t hash);

#endif