#include "array.h"
#include "core/memory_manager.h"
#include "utility/log.h"
#include <string.h>

// Internal array structure
struct EseArray {
    void **elements;           // Array of void pointers
    size_t size;              // Current number of elements
    size_t capacity;          // Current allocated capacity
    ArrayFreeFn free_fn;      // Function to free individual elements
};

EseArray *array_create(size_t initial_capacity, ArrayFreeFn free_fn) {
    EseArray *array = memory_manager.malloc(sizeof(EseArray), MMTAG_GENERAL);
    if (!array) return NULL;
    
    array->elements = memory_manager.malloc(sizeof(void*) * initial_capacity, MMTAG_GENERAL);
    if (!array->elements) {
        memory_manager.free(array);
        return NULL;
    }
    
    array->size = 0;
    array->capacity = initial_capacity;
    array->free_fn = free_fn;
    
    return array;
}

void array_destroy(EseArray *array) {
    if (!array) return;
    
    if (array->free_fn) {
        for (size_t i = 0; i < array->size; i++) {
            if (array->elements[i]) {
                array->free_fn(array->elements[i]);
            }
        }
    }
    
    memory_manager.free(array->elements);
    memory_manager.free(array);
}

static bool array_resize(EseArray *array, size_t new_capacity) {
    void **new_elements = memory_manager.realloc(array->elements, sizeof(void*) * new_capacity, MMTAG_GENERAL);
    if (!new_elements) return false;
    
    array->elements = new_elements;
    array->capacity = new_capacity;
    return true;
}

bool array_push(EseArray *array, void *element) {
    log_assert("ARRAY", array, "array_push called with NULL array");
    
    if (array->size >= array->capacity) {
        size_t new_capacity = array->capacity == 0 ? 1 : array->capacity * 2;
        if (!array_resize(array, new_capacity)) {
            return false;
        }
    }
    
    array->elements[array->size] = element;
    array->size++;
    return true;
}

size_t array_size(const EseArray *array) {
    log_assert("ARRAY", array, "array_size called with NULL array");
    return array->size;
}

size_t array_capacity(const EseArray *array) {
    log_assert("ARRAY", array, "array_capacity called with NULL array");
    return array->capacity;
}

void *array_get(const EseArray *array, size_t index) {
    log_assert("ARRAY", array, "array_get called with NULL array");
    
    if (index >= array->size) return NULL;
    return array->elements[index];
}

bool array_set(EseArray *array, size_t index, void *element) {
    log_assert("ARRAY", array, "array_set called with NULL array");
    
    if (index >= array->size) return false;
    
    // Free old element if free_fn is provided
    if (array->free_fn && array->elements[index]) {
        array->free_fn(array->elements[index]);
    }
    
    array->elements[index] = element;
    return true;
}

void array_clear(EseArray *array) {
    log_assert("ARRAY", array, "array_clear called with NULL array");
    
    if (array->free_fn) {
        for (size_t i = 0; i < array->size; i++) {
            if (array->elements[i]) {
                array->free_fn(array->elements[i]);
                array->elements[i] = NULL;
            }
        }
    } else {
        // Just set all pointers to NULL
        memset(array->elements, 0, sizeof(void*) * array->size);
    }
    
    array->size = 0;
}

void *array_pop(EseArray *array) {
    log_assert("ARRAY", array, "array_pop called with NULL array");
    
    if (array->size == 0) return NULL;
    
    array->size--;
    void *element = array->elements[array->size];
    array->elements[array->size] = NULL;
    
    return element;
}

bool array_insert(EseArray *array, size_t index, void *element) {
    log_assert("ARRAY", array, "array_insert called with NULL array");
    
    if (index > array->size) return false;
    
    if (array->size >= array->capacity) {
        size_t new_capacity = array->capacity == 0 ? 1 : array->capacity * 2;
        if (!array_resize(array, new_capacity)) {
            return false;
        }
    }
    
    // Shift elements to make room
    for (size_t i = array->size; i > index; i--) {
        array->elements[i] = array->elements[i - 1];
    }
    
    array->elements[index] = element;
    array->size++;
    return true;
}

bool array_remove_at(EseArray *array, size_t index) {
    log_assert("ARRAY", array, "array_remove_at called with NULL array");
    
    if (index >= array->size) return false;
    
    // Free element if free_fn is provided
    if (array->free_fn && array->elements[index]) {
        array->free_fn(array->elements[index]);
    }
    
    // Shift remaining elements
    for (size_t i = index; i < array->size - 1; i++) {
        array->elements[i] = array->elements[i + 1];
    }
    
    array->size--;
    array->elements[array->size] = NULL;
    return true;
}

void *array_find(const EseArray *array, bool (*predicate)(void *element, void *user_data), void *user_data) {
    log_assert("ARRAY", array, "array_find called with NULL array");
    log_assert("ARRAY", predicate, "array_find called with NULL predicate");
    
    for (size_t i = 0; i < array->size; i++) {
        if (array->elements[i] && predicate(array->elements[i], user_data)) {
            return array->elements[i];
        }
    }
    
    return NULL;
}

void array_sort(EseArray *array, int (*compare_fn)(const void *a, const void *b)) {
    log_assert("ARRAY", array, "array_sort called with NULL array");
    log_assert("ARRAY", compare_fn, "array_sort called with NULL compare_fn");
    
    if (array->size <= 1) return;
    
    // Simple bubble sort for now - can be optimized later
    for (size_t i = 0; i < array->size - 1; i++) {
        for (size_t j = 0; j < array->size - i - 1; j++) {
            if (compare_fn(array->elements[j], array->elements[j + 1]) > 0) {
                // Swap elements
                void *temp = array->elements[j];
                array->elements[j] = array->elements[j + 1];
                array->elements[j + 1] = temp;
            }
        }
    }
}
