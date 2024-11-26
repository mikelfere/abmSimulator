#ifndef symbolTable_h
#define symbolTable_h

#include <stdbool.h>
#include <stdint.h>

#include "value.h"

/**
 * struct Entry - Represents a hash table entry
 * @a: key -> String* : "index" of entry (has to be unique)
 * @b: value -> Value : stores the actual value corresponding to the "index"
 * @c: address -> int : stores the address if entry is global or if it references
 * a global variable
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
 * @brief Iterates through the array of entries in table and if matches
 * address to an entry's address, returns its key, else returns NULL
 * 
 * @param table 
 * @param address 
 * @return String* 
 */
String* tableGetKey(Table* table, int address);

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
 * Set address of entry to given address. If Entry exists, set address to 
 * found address. Update count as necessary. Returns true if set Entry is new.
 * 
 * @param table 
 * @param key 
 * @param value 
 * @param address 
 * @return true 
 * @return false 
 */
bool tableSetValueAddress(Table* table, String* key, Value value, int* address);

/**
 * @brief hecks if table contains more entries than 75% of its capacity.
 * Reallocates a new table with double the capacity if true. If the given 
 * key isn't found in the table, create new entry and assign given address.
 * If Entry exists, set address to found address. Update count as necessary. 
 * Returns true if set Entry is new.
 * 
 * @param table 
 * @param key 
 * @param address 
 * @return true 
 * @return false 
 */
bool tableSetAddress(Table* table, String* key, int* address);

/**
 * @brief Search for an entry with the given key. If found change its address
 * to the given address. Returns true if address was changed.
 * 
 * @param table 
 * @param key 
 * @param address 
 * @return true 
 * @return false 
 */
bool tableChangeAddress(Table* table, String* key, int address);

/**
 * @brief Check if any entry's key in the given table is equal to the given
 * string. If found, returns the String object, else returns NULL
 * 
 * @param table 
 * @param characters 
 * @param length 
 * @param hash 
 * @return String* 
 */
String* findString(Table* table, const char* characters, int length, uint32_t hash);

#endif