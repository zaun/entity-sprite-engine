#ifndef ESE_HASHMAP_H
#define ESE_HASHMAP_H

#include <stddef.h>

// Forward declarations
typedef struct EseHashMap EseHashMap;
typedef struct EseHashMapIter EseHashMapIter;

// Value free function type
typedef void (*EseHashMapFreeFn)(void *value);

/**
 * @brief Create a new, empty hash map.
 *
 * @param free_fn Optional function to free values when they are removed or the map is cleared/freed.
 *                Pass NULL if values should not be freed automatically.
 * @return Pointer to a new EseHashMap, or NULL on allocation failure.
 */
EseHashMap* hashmap_create(EseHashMapFreeFn free_fn);

/**
 * @brief Free the hash map and all its nodes.
 *
 * Does not free the values pointed to by the map.
 *
 * @param map Pointer to the EseHashMap to free.
 */
void hashmap_destroy(EseHashMap* map);

/**
 * @brief Clear all key-value pairs from the hash map.
 *
 * Removes all entries but preserves the bucket structure and capacity.
 * Does not free the values pointed to by the map.
 *
 * @param map Pointer to the EseHashMap to clear.
 */
void hashmap_clear(EseHashMap* map);

/**
 * @brief Insert or update a key-value pair in the hash map.
 *
 * If the key already exists, its value is updated.
 *
 * @param map Pointer to the EseHashMap.
 * @param key Null-terminated string key (copied internally).
 * @param value Pointer to the value to associate with the key.
 */
void hashmap_set(EseHashMap* map, const char* key, void* value);

/**
 * @brief Retrieve the value associated with a key.
 *
 * @param map Pointer to the EseHashMap.
 * @param key Null-terminated string key.
 * @return Pointer to the value, or NULL if not found.
 */
void* hashmap_get(EseHashMap* map, const char* key);

/**
 * @brief Remove a key from the hash map.
 *
 * @param map Pointer to the EseHashMap.
 * @param key Null-terminated string key.
 * @return Pointer to the value associated with the removed key, or NULL if not found.
 */
void* hashmap_remove(EseHashMap* map, const char* key);

/**
 * @brief Get the number of elements in the hash map.
 *
 * @param map Pointer to the EseHashMap.
 * @return Number of key-value pairs in the map.
 */
size_t hashmap_size(EseHashMap* map);

/**
 * @brief Create a new iterator for the given hash map.
 *
 * @param map Pointer to the EseHashMap to iterate over.
 * @return Pointer to a new EseHashMapIter, or NULL on allocation failure.
 */
EseHashMapIter* hashmap_iter_create(EseHashMap* map);

/**
 * @brief Free the hash map iterator.
 *
 * @param iter Pointer to the EseHashMapIter to free.
 */
void hashmap_iter_free(EseHashMapIter* iter);

/**
 * @brief Advance the iterator and retrieve the next key-value pair.
 *
 * On each call, sets *key and *value to the next entry in the map.
 *
 * @param iter Pointer to the EseHashMapIter.
 * @param key Output pointer for the key (may be NULL if not needed).
 * @param value Output pointer for the value (may be NULL if not needed).
 * @return 1 if a key-value pair was found, 0 if iteration is complete.
 */
int hashmap_iter_next(EseHashMapIter* iter, const char** key, void** value);

#endif // ESE_HASHMAP_H
