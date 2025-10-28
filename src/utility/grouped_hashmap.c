#include "utility/grouped_hashmap.h"
#include "core/memory_manager.h"
#include "utility/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// You must provide this function somewhere in your project.
void log_error(const char *tag, const char *fmt, ...);

#define INITIAL_CAPACITY 16
#define LOAD_FACTOR 0.75

/**
 * @brief Composite key structure for grouped hash maps.
 *
 * @details This structure represents a two-part key consisting of a group
 *          identifier and a specific ID within that group. Both parts are
 *          heap-allocated strings that must be freed when the key is destroyed.
 */
typedef struct EseGroupedKey {
  char *group; /** Group identifier string */
  char *id;    /** Specific ID within the group */
} EseGroupedKey;

/**
 * @brief Node structure for grouped hash map entries.
 *
 * @details Each node contains a grouped key, a value pointer, and a pointer
 *          to the next node in the same bucket for collision resolution.
 */
typedef struct EseHashNode {
  EseGroupedKey key;        /** Composite key for the hash map entry */
  void *value;              /** Value associated with the key */
  struct EseHashNode *next; /** Pointer to next node in collision chain */
} EseHashNode;

/**
 * @brief Grouped hash map data structure for hierarchical key-value storage.
 *
 * @details This structure implements a hash table that organizes entries by
 *          groups, allowing efficient lookup by both group and ID. It supports
 *          dynamic resizing and collision resolution using chaining.
 */
struct EseGroupedHashMap {
  EseHashNode **buckets;           /** Array of bucket pointers */
  size_t capacity;                 /** Number of buckets in the hash map */
  size_t size;                     /** Number of key-value pairs stored */
  EseGroupedHashMapFreeFn free_fn; /** Function to free stored values */
};

/**
 * @brief Iterator structure for traversing grouped hash map entries.
 *
 * @details This structure maintains the current position in the grouped hash
 * map for iteration, tracking the current bucket and node within that bucket.
 */
struct EseGroupedHashMapIter {
  EseGroupedHashMap
      *map;          /** Reference to the grouped hash map being iterated */
  size_t bucket;     /** Current bucket index */
  EseHashNode *node; /** Current node within the bucket */
};

static unsigned int _hash_grouped(const char *group, const char *id) {
  unsigned int h = 5381;
  if (group)
    while (*group)
      h = ((h << 5) + h) + (unsigned char)(*group++);
  if (id)
    while (*id)
      h = ((h << 5) + h) + (unsigned char)(*id++);
  return h;
}

static int _key_equals(const EseGroupedKey *a, const char *group,
                       const char *id) {
  return strcmp(a->group, group) == 0 && strcmp(a->id, id) == 0;
}

static EseHashNode *_create_node(const char *group, const char *id,
                                 void *value) {
  if (!group || !id || !value) {
    log_error("HASHMAP",
              "Error: _create_node received null group, id, or value");
    return NULL;
  }
  EseHashNode *node =
      memory_manager.malloc(sizeof(EseHashNode), MMTAG_GROUP_HASHMAP);
  node->key.group = memory_manager.strdup(group, MMTAG_GROUP_HASHMAP);
  node->key.id = memory_manager.strdup(id, MMTAG_GROUP_HASHMAP);
  node->value = value;
  node->next = NULL;
  return node;
}

static void _free_node(EseHashNode *node, EseGroupedHashMapFreeFn free_fn) {
  if (!node)
    return;
  if (free_fn)
    free_fn(node->value);
  memory_manager.free(node->key.group);
  memory_manager.free(node->key.id);
  memory_manager.free(node);
}

static void hashmap_resize(EseGroupedHashMap *map) {
  if (!map) {
    log_error("HASHMAP", "Error: hashmap_resize received null map");
    return;
  }
  size_t new_capacity = map->capacity * 2;
  EseHashNode **new_buckets = memory_manager.calloc(
      new_capacity, sizeof(EseHashNode *), MMTAG_GROUP_HASHMAP);
  if (!new_buckets) {
    log_error("HASHMAP",
              "Error: memory_manager.calloc failed during resize to %zu",
              new_capacity);
    return;
  }
  for (size_t i = 0; i < map->capacity; i++) {
    EseHashNode *node = map->buckets[i];
    while (node) {
      EseHashNode *next = node->next;
      unsigned int idx =
          _hash_grouped(node->key.group, node->key.id) % new_capacity;
      node->next = new_buckets[idx];
      new_buckets[idx] = node;
      node = next;
    }
  }
  memory_manager.free(map->buckets);
  map->buckets = new_buckets;
  map->capacity = new_capacity;
}

EseGroupedHashMap *grouped_hashmap_create(EseGroupedHashMapFreeFn free_fn) {
  EseGroupedHashMap *map =
      memory_manager.malloc(sizeof(EseGroupedHashMap), MMTAG_GROUP_HASHMAP);
  map->capacity = INITIAL_CAPACITY;
  map->size = 0;
  map->buckets = memory_manager.calloc(map->capacity, sizeof(EseHashNode *),
                                       MMTAG_GROUP_HASHMAP);
  if (!map->buckets) {
    log_error("HASHMAP", "Error: memory_manager.calloc failed for buckets");
    memory_manager.free(map);
    return NULL;
  }
  map->free_fn = free_fn;
  return map;
}

void grouped_hashmap_destroy(EseGroupedHashMap *map) {
  if (!map)
    return;
  for (size_t i = 0; i < map->capacity; i++) {
    EseHashNode *node = map->buckets[i];
    while (node) {
      EseHashNode *next = node->next;
      _free_node(node, map->free_fn);
      node = next;
    }
  }
  memory_manager.free(map->buckets);
  memory_manager.free(map);
}

void grouped_hashmap_set(EseGroupedHashMap *map, const char *group,
                         const char *id, void *value) {
  if (!map || !group || !id || !value) {
    log_error("HASHMAP", "Error: grouped_hashmap_set received null argument");
    return;
  }
  if ((double)(map->size + 1) / map->capacity > LOAD_FACTOR) {
    hashmap_resize(map);
  }
  unsigned int idx = _hash_grouped(group, id) % map->capacity;
  EseHashNode *node = map->buckets[idx];
  while (node) {
    if (_key_equals(&node->key, group, id)) {
      if (map->free_fn)
        map->free_fn(node->value);
      node->value = value;
      return;
    }
    node = node->next;
  }
  node = _create_node(group, id, value);
  if (!node) {
    log_error("HASHMAP", "Error: _create_node failed for group '%s', id '%s'",
              group, id);
    return;
  }
  node->next = map->buckets[idx];
  map->buckets[idx] = node;
  map->size++;
}

void *grouped_hashmap_get(EseGroupedHashMap *map, const char *group,
                          const char *id) {
  if (!map || !group || !id) {
    log_error("HASHMAP", "Error: grouped_hashmap_get received null argument");
    return NULL;
  }
  unsigned int idx = _hash_grouped(group, id) % map->capacity;
  EseHashNode *node = map->buckets[idx];
  while (node) {
    if (_key_equals(&node->key, group, id)) {
      return node->value;
    }
    node = node->next;
  }
  return NULL;
}

void *grouped_hashmap_remove(EseGroupedHashMap *map, const char *group,
                             const char *id) {
  if (!map || !group || !id) {
    log_error("HASHMAP",
              "Error: grouped_hashmap_remove received null argument");
    return NULL;
  }
  unsigned int idx = _hash_grouped(group, id) % map->capacity;
  EseHashNode *node = map->buckets[idx];
  EseHashNode *prev = NULL;
  while (node) {
    if (_key_equals(&node->key, group, id)) {
      if (prev)
        prev->next = node->next;
      else
        map->buckets[idx] = node->next;
      void *value = node->value;
      memory_manager.free(node->key.group);
      memory_manager.free(node->key.id);
      memory_manager.free(node);
      map->size--;
      return value;
    }
    prev = node;
    node = node->next;
  }
  log_error(
      "HASHMAP",
      "Warning: grouped_hashmap_remove could not find group '%s', id '%s'",
      group, id);
  return NULL;
}

void grouped_hashmap_remove_group(EseGroupedHashMap *map, const char *group) {
  if (!map || !group) {
    log_error("HASHMAP",
              "Error: grouped_hashmap_remove_group received null argument");
    return;
  }
  for (size_t i = 0; i < map->capacity; i++) {
    EseHashNode *node = map->buckets[i];
    EseHashNode *prev = NULL;
    while (node) {
      if (strcmp(node->key.group, group) == 0) {
        EseHashNode *to_remove = node;
        if (prev)
          prev->next = node->next;
        else
          map->buckets[i] = node->next;
        node = node->next;
        _free_node(to_remove, map->free_fn);
        map->size--;
      } else {
        prev = node;
        node = node->next;
      }
    }
  }
}

size_t grouped_hashmap_size(EseGroupedHashMap *map) {
  if (!map) {
    log_error("HASHMAP", "Error: grouped_hashmap_size received null map");
    return 0;
  }
  return map->size;
}

EseGroupedHashMapIter *grouped_hashmap_iter_create(EseGroupedHashMap *map) {
  if (!map) {
    log_error("HASHMAP",
              "Error: grouped_hashmap_iter_create received null map");
    return NULL;
  }

  EseGroupedHashMapIter *iter =
      memory_manager.malloc(sizeof(EseGroupedHashMapIter), MMTAG_GROUP_HASHMAP);
  iter->map = map;
  iter->bucket = 0;
  iter->node = NULL;
  return iter;
}

void grouped_hashmap_iter_free(EseGroupedHashMapIter *iter) {
  if (!iter)
    return;
  memory_manager.free(iter);
}

int grouped_hashmap_iter_next(EseGroupedHashMapIter *iter, const char **group,
                              const char **id, void **value) {
  if (!iter) {
    log_error("HASHMAP", "Error: grouped_hashmap_iter_next received null iter");
    return 0;
  }
  if (iter->node)
    iter->node = iter->node->next;
  while (!iter->node && iter->bucket < iter->map->capacity) {
    iter->node = iter->map->buckets[iter->bucket++];
  }
  if (iter->node) {
    if (group)
      *group = iter->node->key.group;
    if (id)
      *id = iter->node->key.id;
    if (value)
      *value = iter->node->value;
    return 1;
  }
  return 0;
}