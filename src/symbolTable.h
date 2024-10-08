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
 * true and assigns corresponding value to given value argument.
 * 
 * @param table 
 * @param key 
 * @param value 
 * @return true 
 * @return false 
 */
bool tableGet(Table* table, String* key, Value* value);

/**
 * @brief Checks if table contains more entries than 75% of its capacity.
 * Reallocates a new table with double the capacity if true. If the given 
 * key isn't found in the table, create new entry and assign given value.
 * Update count as necessary.
 * 
 * @param table 
 * @param key 
 * @param value 
 * @return true 
 * @return false 
 */
bool tableSet(Table* table, String* key, Value value);

/**
 * @brief Handles entry deletion by creation of "tombstones", so as to keep
 * the table in working condition without need of reallocation with each
 * deletion. A tombstone is defined as an entry with key = NULL and 
 * value = INT_MIN.
 * 
 * @param table 
 * @param key 
 * @return true 
 * @return false 
 */
bool deleteEntry(Table* table, String* key);

/**
 * @brief Copies all entries from the first table to the second table.
 * 
 * @param from 
 * @param to 
 */
void tableCopyEntries(Table* from, Table* to);

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