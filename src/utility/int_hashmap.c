#include <stdlib.h>
#include "utility/int_hashmap.h"
#include "core/memory_manager.h"

#define INITIAL_CAPACITY 16
#define LOAD_FACTOR 0.75

/**
 * @brief Node structure for integer hash map entries.
 * 
 * @details Each node contains a key-value pair and a pointer to the next
 *          node in the same bucket for collision resolution using chaining.
 */
typedef struct EseIntHashNode {
    uint64_t key;                    /**< Integer key for the hash map entry */
    void* value;                     /**< Value associated with the key */
    struct EseIntHashNode* next;     /**< Pointer to next node in collision chain */
} EseIntHashNode;

/**
 * @brief Integer hash map data structure for key-value storage.
 * 
 * @details This structure implements a hash table with dynamic resizing
 *          and collision resolution using chaining. It stores an array
 *          of buckets, each containing a linked list of hash nodes.
 */
typedef struct EseIntHashMap {
    EseIntHashNode** buckets;        /**< Array of bucket pointers */
    size_t capacity;                 /**< Number of buckets in the hash map */
    size_t size;                     /**< Number of key-value pairs stored */
    EseIntHashMapFreeFn free_fn;     /**< Function to free stored values */
} EseIntHashMap;

/**
 * @brief Iterator structure for traversing integer hash map entries.
 * 
 * @details This structure maintains the current position in the hash map
 *          for iteration, tracking the current bucket and node within
 *          that bucket.
 */
struct EseIntHashMapIter {
    EseIntHashMap* map;              /**< Reference to the hash map being iterated */
    size_t bucket;                   /**< Current bucket index */
    struct EseIntHashNode* node;     /**< Current node within the bucket */
};

static unsigned int int_hash(uint64_t key) {
    // Simple hash function for 64-bit integers
    // Using a combination of bit shifting and multiplication
    key = (~key) + (key << 21);
    key = key ^ (key >> 24);
    key = (key + (key << 3)) + (key << 8);
    key = key ^ (key >> 14);
    key = (key + (key << 2)) + (key << 4);
    key = key ^ (key >> 28);
    key = key + (key << 31);
    return (unsigned int)key;
}

static EseIntHashNode* create_int_node(uint64_t key, void* value) {
    if (!value) return NULL;

    EseIntHashNode* node = memory_manager.malloc(sizeof(EseIntHashNode), MMTAG_HASHMAP);
    node->key = key;
    node->value = value;
    node->next = NULL;
    return node;
}

static void free_int_node(EseIntHashNode* node, EseIntHashMapFreeFn free_fn) {
    if (!node) return;
    if (free_fn) free_fn(node->value);
    memory_manager.free(node);
}

static void int_hashmap_resize(EseIntHashMap* map) {
    if (!map) return;

    size_t new_capacity = map->capacity * 2;
    EseIntHashNode** new_buckets = memory_manager.calloc(new_capacity, sizeof(EseIntHashNode*), MMTAG_HASHMAP);

    for (size_t i = 0; i < map->capacity; i++) {
        EseIntHashNode* node = map->buckets[i];
        while (node) {
            EseIntHashNode* next = node->next;
            unsigned int idx = int_hash(node->key) % new_capacity;
            node->next = new_buckets[idx];
            new_buckets[idx] = node;
            node = next;
        }
    }
    memory_manager.free(map->buckets);
    map->buckets = new_buckets;
    map->capacity = new_capacity;
}

EseIntHashMap* int_hashmap_create(EseIntHashMapFreeFn free_fn) {
    EseIntHashMap* map = memory_manager.malloc(sizeof(EseIntHashMap), MMTAG_HASHMAP);
    map->capacity = INITIAL_CAPACITY;
    map->size = 0;
    map->free_fn = free_fn;
    map->buckets = memory_manager.calloc(map->capacity, sizeof(EseIntHashNode*), MMTAG_HASHMAP);
    return map;
}

void int_hashmap_free(EseIntHashMap* map) {
    if (!map) return;

    for (size_t i = 0; i < map->capacity; i++) {
        EseIntHashNode* node = map->buckets[i];
        while (node) {
            EseIntHashNode* next = node->next;
            free_int_node(node, map->free_fn);
            node = next;
        }
    }
    memory_manager.free(map->buckets);
    memory_manager.free(map);
}

void int_hashmap_clear(EseIntHashMap* map) {
    if (!map) return;

    for (size_t i = 0; i < map->capacity; i++) {
        EseIntHashNode* node = map->buckets[i];
        while (node) {
            EseIntHashNode* next = node->next;
            free_int_node(node, map->free_fn);
            node = next;
        }
        map->buckets[i] = NULL;
    }
    map->size = 0;
}

void int_hashmap_set(EseIntHashMap* map, uint64_t key, void* value) {
    if (!map || !value) return;

    if ((double)(map->size + 1) / map->capacity > LOAD_FACTOR) {
        int_hashmap_resize(map);
    }
    unsigned int idx = int_hash(key) % map->capacity;
    EseIntHashNode* node = map->buckets[idx];
    while (node) {
        if (node->key == key) {
            node->value = value;
            return;
        }
        node = node->next;
    }
    node = create_int_node(key, value);
    node->next = map->buckets[idx];
    map->buckets[idx] = node;
    map->size++;
}

void* int_hashmap_get(EseIntHashMap* map, uint64_t key) {
    if (!map) return NULL;

    unsigned int idx = int_hash(key) % map->capacity;
    EseIntHashNode* node = map->buckets[idx];
    while (node) {
        if (node->key == key) {
            return node->value;
        }
        node = node->next;
    }
    return NULL;
}

void* int_hashmap_remove(EseIntHashMap* map, uint64_t key) {
    if (!map) return NULL;

    unsigned int idx = int_hash(key) % map->capacity;
    EseIntHashNode* node = map->buckets[idx];
    EseIntHashNode* prev = NULL;
    while (node) {
        if (node->key == key) {
            if (prev) {
                prev->next = node->next;
            } else {
                map->buckets[idx] = node->next;
            }
            void* value = node->value;
            // Don't call free_fn here since we're returning the value
            memory_manager.free(node);
            map->size--;
            return value;
        }
        prev = node;
        node = node->next;
    }
    return NULL;
}

size_t int_hashmap_size(EseIntHashMap* map) {
    return map ? map->size : 0;
}

EseIntHashMapIter* int_hashmap_iter_create(EseIntHashMap* map) {
    if (!map) return NULL;

    EseIntHashMapIter* iter = memory_manager.malloc(sizeof(EseIntHashMapIter), MMTAG_HASHMAP);
    iter->map = map;
    iter->bucket = 0;
    iter->node = NULL;
    return iter;
}

void int_hashmap_iter_free(EseIntHashMapIter* iter) {
    if (!iter) return;

    memory_manager.free(iter);
}

int int_hashmap_iter_next(EseIntHashMapIter* iter, uint64_t* key, void** value) {
    if (!iter) return 0;

    if (iter->node) {
        iter->node = iter->node->next;
    }
    while (!iter->node && iter->bucket < iter->map->capacity) {
        iter->node = iter->map->buckets[iter->bucket++];
    }
    if (iter->node) {
        if (key) *key = iter->node->key;
        if (value) *value = iter->node->value;
        return 1;
    }
    return 0;
}
