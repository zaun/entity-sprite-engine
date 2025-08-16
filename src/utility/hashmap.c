#include <stdlib.h>
#include <string.h>
#include "utility/hashmap.h"
#include "core/memory_manager.h"

#define INITIAL_CAPACITY 16
#define LOAD_FACTOR 0.75

typedef struct EseHashNode {
    char* key;
    void* value;
    struct EseHashNode* next;
} EseHashNode;

typedef struct EseHashMap {
    EseHashNode** buckets;
    size_t capacity;
    size_t size;
} EseHashMap;

struct EseHashMapIter {
    EseHashMap* map;
    size_t bucket;
    struct EseHashNode* node;
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

    EseHashNode* node = memory_manager.malloc(sizeof(EseHashNode), MMTAG_GENERAL);
    node->key = memory_manager.strdup(key, MMTAG_GENERAL);
    node->value = value;
    node->next = NULL;
    return node;
}

static void free_node(EseHashNode* node) {
    if (!node) return;

    memory_manager.free(node->key);
    memory_manager.free(node);
}

static void hashmap_resize(EseHashMap* map) {
    if (!map) return;

    size_t new_capacity = map->capacity * 2;
    EseHashNode** new_buckets = memory_manager.calloc(new_capacity, sizeof(EseHashNode*), MMTAG_GENERAL);

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

EseHashMap* hashmap_create(void) {
    EseHashMap* map = memory_manager.malloc(sizeof(EseHashMap), MMTAG_GENERAL);
    map->capacity = INITIAL_CAPACITY;
    map->size = 0;
    map->buckets = memory_manager.calloc(map->capacity, sizeof(EseHashNode*), MMTAG_GENERAL);
    return map;
}

void hashmap_free(EseHashMap* map) {
    if (!map) return;

    for (size_t i = 0; i < map->capacity; i++) {
        EseHashNode* node = map->buckets[i];
        while (node) {
            EseHashNode* next = node->next;
            free_node(node);
            node = next;
        }
    }
    memory_manager.free(map->buckets);
    memory_manager.free(map);
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
            free_node(node);
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

    EseHashMapIter* iter = memory_manager.malloc(sizeof(EseHashMapIter), MMTAG_GENERAL);
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