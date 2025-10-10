#ifndef ESE_INT_HASHMAP_H
#define ESE_INT_HASHMAP_H

#include <stddef.h>
#include <stdint.h>

// Forward declarations
typedef struct EseIntHashMap EseIntHashMap;
typedef struct EseIntHashMapIter EseIntHashMapIter;

// Value free function type
typedef void (*EseIntHashMapFreeFn)(void *value);

/**
 * @brief Create a new, empty integer-keyed hash map.
 *
 * @param free_fn Optional function to free values when they are removed or the map is cleared/freed.
 *                Pass NULL if values should not be freed automatically.
 * @return Pointer to a new EseIntHashMap, or NULL on allocation failure.
 */
EseIntHashMap* int_hashmap_create(EseIntHashMapFreeFn free_fn);

/**
 * @brief Free the hash map and all its nodes.
 *
 * Does not free the values pointed to by the map.
 *
 * @param map Pointer to the EseIntHashMap to free.
 */
void int_hashmap_destroy(EseIntHashMap* map);

/**
 * @brief Clear all key-value pairs from the hash map.
 *
 * Removes all entries but preserves the bucket structure and capacity.
 * Does not free the values pointed to by the map.
 *
 * @param map Pointer to the EseIntHashMap to clear.
 */
void int_hashmap_clear(EseIntHashMap* map);

/**
 * @brief Insert or update a key-value pair in the hash map.
 *
 * If the key already exists, its value is updated.
 *
 * @param map Pointer to the EseIntHashMap.
 * @param key Integer key.
 * @param value Pointer to the value to associate with the key.
 */
void int_hashmap_set(EseIntHashMap* map, uint64_t key, void* value);

/**
 * @brief Retrieve the value associated with a key.
 *
 * @param map Pointer to the EseIntHashMap.
 * @param key Integer key.
 * @return Pointer to the value, or NULL if not found.
 */
void* int_hashmap_get(EseIntHashMap* map, uint64_t key);

/**
 * @brief Remove a key from the hash map.
 *
 * @param map Pointer to the EseIntHashMap.
 * @param key Integer key.
 * @return Pointer to the value associated with the removed key, or NULL if not found.
 */
void* int_hashmap_remove(EseIntHashMap* map, uint64_t key);

/**
 * @brief Get the number of elements in the hash map.
 *
 * @param map Pointer to the EseIntHashMap.
 * @return Number of key-value pairs in the map.
 */
size_t int_hashmap_size(EseIntHashMap* map);

/**
 * @brief Create a new iterator for the given hash map.
 *
 * @param map Pointer to the EseIntHashMap to iterate over.
 * @return Pointer to a new EseIntHashMapIter, or NULL on allocation failure.
 */
EseIntHashMapIter* int_hashmap_iter_create(EseIntHashMap* map);

/**
 * @brief Free the hash map iterator.
 *
 * @param iter Pointer to the EseIntHashMapIter to free.
 */
void int_hashmap_iter_free(EseIntHashMapIter* iter);

/**
 * @brief Advance the iterator and retrieve the next key-value pair.
 *
 * On each call, sets *key and *value to the next entry in the map.
 *
 * @param iter Pointer to the EseIntHashMapIter.
 * @param key Output pointer for the key (may be NULL if not needed).
 * @param value Output pointer for the value (may be NULL if not needed).
 * @return 1 if a key-value pair was found, 0 if iteration is complete.
 */
int int_hashmap_iter_next(EseIntHashMapIter* iter, uint64_t* key, void** value);

#endif // ESE_INT_HASHMAP_H
