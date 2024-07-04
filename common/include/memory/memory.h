#ifndef COMMON_MEMORY_H
#define COMMON_MEMORY_H

#include <stddef.h>

/// @brief Increases capacity by a factor of two.
/// @param capacity to increase.
#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

/// @brief Reallocates array from current capacity(old_count) to requested capacity(new_count).
/// @param type of data.
/// @param pointer to current memory.
/// @param old_count is the current capacity.
/// @param new_count is the requested capacity.
/// @return pointer to new memory(might be the same address or new). 
#define GROW_ARRAY(type, pointer, old_count, new_count)         \
    (type *)Reallocate(pointer, sizeof(type) * (old_count), \
                           sizeof(type) * (new_count))

/// @brief Reallocates the array to size 0(free it).
/// @param type of array element.
/// @param pointer to array.
/// @param old_count is the current capacity of the array.
#define FREE_ARRAY(type, pointer, old_count) \
    Reallocate(pointer, sizeof(type) * (old_count), 0)

/// @brief Reallocates data memory to size 0(free it).
/// @param type of data.
/// @param pointer to data memory.
#define FREE(type, pointer) Reallocate(pointer, sizeof(type), 0)

/// @brief Allocates data memory with size specified by sizeof(type) * count.
/// @param type of data.
/// @param count is the amount of elements of type.
/// @return pointer to allocated memory.
#define ALLOCATE(type, count) \
    (type *)Reallocate(NULL, 0, sizeof(type) * (count))

void *Reallocate(void *pointer, size_t old_size, size_t new_size);

#endif
