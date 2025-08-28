#include <stdlib.h>
#include <string.h>
#include "utility/hashmap.h"
#include "core/memory_manager.h"

#define INITIAL_CAPACITY 16
#define LOAD_FACTOR 0.75

/**
 * @brief Node structure for hash map entries.
 * 
 * @details Each node contains a key-value pair and a pointer to the next
 *          node in the same bucket for collision resolution using chaining.
 */
typedef struct EseHashNode {
    char* key;                      /**< String key for the hash map entry */
    void* value;                    /**< Value associated with the key */
    struct EseHashNode* next;       /**< Pointer to next node in collision chain */
} EseHashNode;

/**
 * @brief Hash map data structure for key-value storage.
 * 
 * @details This structure implements a hash table with dynamic resizing
 *          and collision resolution using chaining. It stores an array
 *          of buckets, each containing a linked list of hash nodes.
 */
typedef struct EseHashMap {
    EseHashNode** buckets;          /**< Array of bucket pointers */
    size_t capacity;                /**< Number of buckets in the hash map */
    size_t size;                    /**< Number of key-value pairs stored */
    EseHashMapFreeFn free_fn;       /**< Function to free stored values */
} EseHashMap;

/**
 * @brief Iterator structure for traversing hash map entries.
 * 
 * @details This structure maintains the current position in the hash map
 *          for iteration, tracking the current bucket and node within
 *          that bucket.
 */
struct EseHashMapIter {
    EseHashMap* map;                /**< Reference to the hash map being iterated */
    size_t bucket;                  /**< Current bucket index */
    struct EseHashNode* node;       /**< Current node within the bucket */
};

static unsigned int hash(const char* key) {
    if (!key) return 0;

    unsigned int h = 5381;
    while (*key) {
        h = ((h << 5) + h) + (unsigned char)(*key++);
    }
    return h;
}

static EseHashNode* create_node(const char* key, void* value) {
    if (!key || !value) return NULL;

    EseHashNode* node = memory_manager.malloc(sizeof(EseHashNode), MMTAG_HASHMAP);
    node->key = memory_manager.strdup(key, MMTAG_HASHMAP);
    node->value = value;
    node->next = NULL;
    return node;
}

static void free_node(EseHashNode* node, EseHashMapFreeFn free_fn) {
    if (!node) return;
    if (free_fn) free_fn(node->value);
    memory_manager.free(node->key);
    memory_manager.free(node);
}

static void hashmap_resize(EseHashMap* map) {
    if (!map) return;

    size_t new_capacity = map->capacity * 2;
    EseHashNode** new_buckets = memory_manager.calloc(new_capacity, sizeof(EseHashNode*), MMTAG_HASHMAP);

    for (size_t i = 0; i < map->capacity; i++) {
        EseHashNode* node = map->buckets[i];
        while (node) {
            EseHashNode* next = node->next;
            unsigned int idx = hash(node->key) % new_capacity;
            node->next = new_buckets[idx];
            new_buckets[idx] = node;
            node = next;
        }
    }
    memory_manager.free(map->buckets);
    map->buckets = new_buckets;
    map->capacity = new_capacity;
}

EseHashMap* hashmap_create(EseHashMapFreeFn free_fn) {
    EseHashMap* map = memory_manager.malloc(sizeof(EseHashMap), MMTAG_HASHMAP);
    map->capacity = INITIAL_CAPACITY;
    map->size = 0;
    map->free_fn = free_fn;
    map->buckets = memory_manager.calloc(map->capacity, sizeof(EseHashNode*), MMTAG_HASHMAP);
    return map;
}

void hashmap_free(EseHashMap* map) {
    if (!map) return;

    for (size_t i = 0; i < map->capacity; i++) {
        EseHashNode* node = map->buckets[i];
        while (node) {
            EseHashNode* next = node->next;
            free_node(node, map->free_fn);
            node = next;
        }
    }
    memory_manager.free(map->buckets);
    memory_manager.free(map);
}

void hashmap_clear(EseHashMap* map) {
    if (!map) return;

    for (size_t i = 0; i < map->capacity; i++) {
        EseHashNode* node = map->buckets[i];
        while (node) {
            EseHashNode* next = node->next;
            free_node(node, map->free_fn);
            node = next;
        }
        map->buckets[i] = NULL;
    }
    map->size = 0;
}

void hashmap_set(EseHashMap* map, const char* key, void* value) {
    if (!map || !key || !value) return;

    if ((double)(map->size + 1) / map->capacity > LOAD_FACTOR) {
        hashmap_resize(map);
    }
    unsigned int idx = hash(key) % map->capacity;
    EseHashNode* node = map->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0) {
            node->value = value;
            return;
        }
        node = node->next;
    }
    node = create_node(key, value);
    node->next = map->buckets[idx];
    map->buckets[idx] = node;
    map->size++;
}

void* hashmap_get(EseHashMap* map, const char* key) {
    if (!map || !key) return NULL;

    unsigned int idx = hash(key) % map->capacity;
    EseHashNode* node = map->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0) {
            return node->value;
        }
        node = node->next;
    }
    return NULL;
}

void* hashmap_remove(EseHashMap* map, const char* key) {
    if (!map || !key) return NULL;

    unsigned int idx = hash(key) % map->capacity;
    EseHashNode* node = map->buckets[idx];
    EseHashNode* prev = NULL;
    while (node) {
        if (strcmp(node->key, key) == 0) {
            if (prev) {
                prev->next = node->next;
            } else {
                map->buckets[idx] = node->next;
            }
            void* value = node->value;
            // Don't call free_fn here since we're returning the value
            memory_manager.free(node->key);
            memory_manager.free(node);
            map->size--;
            return value;
        }
        prev = node;
        node = node->next;
    }
    return NULL;
}

size_t hashmap_size(EseHashMap* map) {
    return map ? map->size : 0;
}

EseHashMapIter* hashmap_iter_create(EseHashMap* map) {
    if (!map) return NULL;

    EseHashMapIter* iter = memory_manager.malloc(sizeof(EseHashMapIter), MMTAG_HASHMAP);
    iter->map = map;
    iter->bucket = 0;
    iter->node = NULL;
    return iter;
}

void hashmap_iter_free(EseHashMapIter* iter) {
    if (!iter) return;

    memory_manager.free(iter);
}

int hashmap_iter_next(EseHashMapIter* iter, const char** key, void** value) {
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