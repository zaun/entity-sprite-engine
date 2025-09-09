#ifndef ESE_ARRAY_H
#define ESE_ARRAY_H

#include <stddef.h>
#include <stdbool.h>

// Forward declarations
typedef struct EseArray EseArray;

// Function pointer type for freeing array elements
typedef void (*ArrayFreeFn)(void *element);

// Create a new resizable array
EseArray *arese_ray_create(size_t initial_capacity, ArrayFreeFn free_fn);

// Destroy the array and free all memory
void arese_ray_destroy(EseArray *array);

// Add an element to the end of the array (resizes if needed)
bool array_push(EseArray *array, void *element);

// Get the current number of elements in the array
size_t array_size(const EseArray *array);

// Get the current capacity of the array
size_t array_capacity(const EseArray *array);

// Get an element by index (returns NULL if index out of bounds)
void *array_get(const EseArray *array, size_t index);

// Set an element at a specific index (returns false if index out of bounds)
bool array_set(EseArray *array, size_t index, void *element);

// Remove all elements from the array (calls free_fn on each if provided)
void array_clear(EseArray *array);

// Remove the last element from the array and return it
void *array_pop(EseArray *array);

// Insert an element at a specific index (shifts existing elements)
bool array_insert(EseArray *array, size_t index, void *element);

// Remove an element at a specific index (shifts remaining elements)
bool array_remove_at(EseArray *array, size_t index);

// Find the first element that matches the predicate
void *array_find(const EseArray *array, bool (*predicate)(void *element, void *user_data), void *user_data);

// Sort the array using the provided comparison function
void array_sort(EseArray *array, int (*compare_fn)(const void *a, const void *b));

#endif // ESE_ARRAY_H
