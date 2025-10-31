/*
 * Project: Entity Sprite Engine
 *
 * Tagged, per-thread allocator with leak and double-free tracking. 16-byte
 * aligned allocations, backtraces, and per-tag/global stats. Reports leaks with
 * sample backtraces; aborts on fatal misuse. Thread-safe resize and teardown;
 * cross-thread frees are rejected.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Memory tag identifiers used to categorize allocations.
 *
 * @details Each tag corresponds to a subsystem or memory consumer. Values are
 * used as indices into per-tag statistics in the memory manager.
 */
typedef enum {
    MMTAG_GENERAL = 0,
    MMTAG_ENGINE,
    MMTAG_GUI,
    MMTAG_GUI_STYLE,
    MMTAG_ASSET,
    MMTAG_ENTITY,
    MMTAG_COMP_LUA,
    MMTAG_COMP_MAP,
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
    MMTAG_LINKED_LIST_ITER,
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
    MMTAG_PUB_SUB,
    MMTAG_THREAD,
    MMTAG_HTTP,
    MMTAG_RS_SHAPE,    /** Renderer System - Shape */
    MMTAG_S_SPRITE,    /** System - Sprite */
    MMTAG_RS_SPRITE,   /** Renderer System - Sprite */
    MMTAG_RS_TEXT,     /** Renderer System - Text */
    MMTAG_RS_COLLIDER, /** Renderer System - Collider */
    MMTAG_RS_MAP,      /** Renderer System - Map */
    MMTAG_TEMP,        /** Temporary - Should only be used while actively debugging and testing. */
    MMTAG_COUNT
} MemTag;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Thread-safe shared memory allocator for cross-thread usage.
 *
 * These functions use a global mutex-protected allocator,
 * allowing allocations to be freed from any thread safely.
 */
 struct memory_manager_shared_api {
    void *(*malloc)(size_t size, MemTag tag);
    void *(*calloc)(size_t count, size_t size, MemTag tag);
    void *(*realloc)(void *ptr, size_t size, MemTag tag);
    void (*free)(void *ptr);
    char *(*strdup)(const char *str, MemTag tag);
};

/**
 * @brief API structure for memory management operations.
 *
 * @details This structure provides function pointers for all memory management
 *          operations including allocation, deallocation, and reporting. It
 * enables the "memory_manager.malloc" style usage pattern throughout the
 * codebase. All functions are implemented by the memory manager and provide
 * consistent memory tracking and management capabilities.
 */
struct memory_manager_api {
    void *(*malloc)(size_t size, MemTag tag);               /** Allocate memory */
    void *(*calloc)(size_t count, size_t size, MemTag tag); /** Allocate zeroed memory */
    void *(*realloc)(void *ptr, size_t size, MemTag tag);   /** Reallocate memory */
    void (*free)(void *ptr);                                /** Free allocated memory */
    char *(*strdup)(const char *str, MemTag tag);           /** Duplicate string */
    void (*report)(bool all_threads);                       /** Print memory usage report */
    void (*destroy)(bool all_threads);                      /** Cleanup memory manager resources */

    const struct memory_manager_shared_api shared;          /** Shared cross-thread API */
};

extern const struct memory_manager_api memory_manager;

#endif // MEMORY_MANAGER_H
