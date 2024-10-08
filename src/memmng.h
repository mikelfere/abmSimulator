#ifndef memmng_h
#define memmng_h

#include <stddef.h>

/**
 * @brief Using the C standard realloc() function, reallocate the given
 * array to the specified new size. If new size is 0, then free the
 * array. All sizes passed must be in number of bytes.
 * 
 * @param pointer 
 * @param oldSize 
 * @param newSize 
 * @return void* 
 */
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

/**
 * @brief Frees all Object objects in vm. Traverses through the linked
 * list of objects and depending on the type, frees and resets all
 * members of the object.
 */
void freeObjects();

#endif