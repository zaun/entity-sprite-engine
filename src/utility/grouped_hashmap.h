#ifndef ESE_GROUPED_HASHMAP_H
#define ESE_GROUPED_HASHMAP_H

#include <stddef.h>

// Forward declarations
typedef struct EseGroupedHashMap EseGroupedHashMap;
typedef struct EseGroupedHashMapIter EseGroupedHashMapIter;

// Value free function type
typedef void (*EseGroupedHashMapFreeFn)(void *value);

// Create a new grouped EseHashMap with a value-free function
EseGroupedHashMap* grouped_hashmap_create(EseGroupedHashMapFreeFn free_fn);

// Free the EseHashMap and all its nodes (calls free_fn for each value)
void grouped_hashmap_destroy(EseGroupedHashMap* map);

// Set a value for (group, id)
void grouped_hashmap_set(EseGroupedHashMap* map, const char* group, const char* id, void* value);

// Get a value for (group, id)
void* grouped_hashmap_get(EseGroupedHashMap* map, const char* group, const char* id);

// Remove a value for (group, id), returns value (does not call free_fn)
void* grouped_hashmap_remove(EseGroupedHashMap* map, const char* group, const char* id);

// Remove all items in a group (calls free_fn for each value)
void grouped_hashmap_remove_group(EseGroupedHashMap* map, const char* group);

// Number of items
size_t grouped_hashmap_size(EseGroupedHashMap* map);

// Iterator
EseGroupedHashMapIter* grouped_hashmap_iter_create(EseGroupedHashMap* map);
void grouped_hashmap_iter_free(EseGroupedHashMapIter* iter);
int grouped_hashmap_iter_next(EseGroupedHashMapIter* iter, const char** group, const char** id, void** value);

#endif // ESE_GROUPED_HASHMAP_H
