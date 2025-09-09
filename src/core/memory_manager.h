#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <stddef.h>

#define MM_BLOCK_SIZE (10 * 1024 * 1024)

typedef enum {
    MMTAG_GENERAL = 0,
    MMTAG_ENGINE,
    MMTAG_ASSET,
    MMTAG_ENTITY,
    MMTAG_ENTITY_COMP_LUA,
    MMTAG_LUA,
    MMTAG_LUA_VALUE,
    MMTAG_LUA_SCRIPT,
    MMTAG_RENDERER,
    MMTAG_SPRITE,
    MMTAG_DRAWLIST,
    MMTAG_RENDERLIST,
    MMTAG_SHADER,
    MMTAG_WINDOW,
    MMTAG_ARRAY,
    MMTAG_HASHMAP,
    MMTAG_GROUP_HASHMAP,
    MMTAG_LINKED_LIST,
    MMTAG_CONSOLE,
    MMTAG_ARC,
    MMTAG_CAMERA,
    MMTAG_DISPLAY,
    MMTAG_INPUT_STATE,
    MMTAG_MAP_CELL,
    MMTAG_MAP,
    MMTAG_POINT,
    MMTAG_RAY,
    MMTAG_RECT,
    MMTAG_TILESET,
    MMTAG_UUID,
    MMTAG_VECTOR,
    MMTAG_AUDIO,
    MMTAG_COLLISION_INDEX,
    MMTAG_COLOR,
    MMTAG_POLY_LINE,
    MMTAG_TEMP,
    MMTAG_COUNT
} MemTag;

/**
 * @brief API structure for memory management operations.
 * 
 * @details This structure provides function pointers for all memory management
 *          operations including allocation, deallocation, and reporting. It enables
 *          the "memory_manager.malloc" style usage pattern throughout the codebase.
 *          All functions are implemented by the memory manager and provide consistent
 *          memory tracking and management capabilities.
 */
struct memory_manager_api {
    void *(*malloc)(size_t size, MemTag tag);      /**< Allocate memory with tag tracking */
    void *(*calloc)(size_t count, size_t size, MemTag tag); /**< Allocate zeroed memory with tag tracking */
    void *(*realloc)(void *ptr, size_t size, MemTag tag);  /**< Reallocate memory with tag tracking */
    void  (*free)(void *ptr);                      /**< Free allocated memory */
    char *(*strdup)(const char *str, MemTag tag);  /**< Duplicate string with tag tracking */
    void  (*report)(void);                         /**< Print memory usage report */
    size_t (*get_current_usage)(void);             /**< Get current memory usage in bytes */
    size_t (*get_max_usage)(void);                 /**< Get peak memory usage in bytes */
    void  (*destroy)(void);                        /**< Cleanup memory manager resources */
};

extern const struct memory_manager_api memory_manager;

void debug_check_memory(void *ptr, const char *location);


#endif // MEMORY_MANAGER_H
