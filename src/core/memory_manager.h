#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <stddef.h>

#define MM_BLOCK_SIZE (5 * 1024 * 1024)

typedef enum {
    MMTAG_GENERAL = 0,
    MMTAG_ENGINE,
    MMTAG_ASSET,
    MMTAG_ENTITY,
    MMTAG_LUA,
    MMTAG_RENDERER,
    MMTAG_MAP,
    MMTAG_SPRITE,
    MMTAG_DRAWLIST,
    MMTAG_RENDERLIST,
    MMTAG_SHADER,
    MMTAG_WINDOW,
    MMTAG_HASHMAP,
    MMTAG_GROUP_HASHMAP,
    MMTAG_LINKED_LIST,
    MMTAG_COUNT
} MemTag;

// API struct for "memory_manager.malloc" style usage
struct memory_manager_api {
    void *(*malloc)(size_t size, MemTag tag);
    void *(*calloc)(size_t count, size_t size, MemTag tag);
    void *(*realloc)(void *ptr, size_t size, MemTag tag);
    void  (*free)(void *ptr);
    char *(*strdup)(const char *str, MemTag tag);
    void  (*report)(void);
    size_t (*get_current_usage)(void);
    size_t (*get_max_usage)(void);
    void  (*destroy)(void);
};

extern const struct memory_manager_api memory_manager;

void debug_check_memory(void *ptr, const char *location);


#endif // MEMORY_MANAGER_H
