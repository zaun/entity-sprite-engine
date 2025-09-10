#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <execinfo.h>
#include <stdalign.h>
#include "core/memory_manager.h"
#include "utility/log.h"

#define DEBUG_MEMORY_MANAGER 0
#define MEMORY_TRACKING 1
#define MEMORY_TRACK_FREE 1

#if DEBUG_MEMORY_MANAGER
#define DEBUG_PRINTF(fmt, ...) do { printf(fmt, ##__VA_ARGS__); fflush(stdout); } while(0)
#else
#define DEBUG_PRINTF(fmt, ...) do { } while(0)
#endif

// Simple hash table for tracking allocations
#define ALLOC_TABLE_SIZE 65536  // Power of 2 for fast modulo
#define ALLOC_TABLE_MASK (ALLOC_TABLE_SIZE - 1)

typedef struct AllocEntry {
    void *ptr;
    size_t size;
    MemTag tag;
    struct AllocEntry *next;
#if MEMORY_TRACKING == 1
    void *bt[16];    /* backtrace frames (optional) */
    int bt_size;     /* number of frames captured */
#endif
} AllocEntry;

typedef struct {
    size_t current_usage;           /**< Current memory usage in bytes */
    size_t max_usage;               /**< Peak memory usage in bytes */
    size_t total_allocs;            /**< Total number of allocations */
    size_t total_frees;             /**< Total number of deallocations */
    size_t total_bytes_alloced;     /**< Total bytes allocated over time */
    size_t largest_alloc;           /**< Largest single allocation size */
} MemStats;

struct MemoryManager {
    AllocEntry *alloc_table[ALLOC_TABLE_SIZE];  /**< Hash table for tracking allocations */
#if MEMORY_TRACKING == 1 && MEMORY_TRACK_FREE == 1
    AllocEntry *freed_table[ALLOC_TABLE_SIZE];  /**< Hash table for tracking freed allocations */
#endif
    MemStats global;                            /**< Global memory usage statistics */
    MemStats tags[MMTAG_COUNT];                 /**< Per-tag memory usage statistics */
};

typedef struct MemoryManager MemoryManager;

MemoryManager *g_memory_manager = NULL;

static const char *mem_tag_names[MMTAG_COUNT] = {
    "GENERAL", "ENGINE ", "ASSET  ", "ENTITY ", "ENT_LUA", "LUA    ", "LUA VAL", "LUA VM ",
    "RENDER ", "SPRITE ", "DRAWLST", "RENDLST", "SHADER ", "WINDOW ", "ARRAY  ", "HASHMAP",
    "GRPHASH", "LINKLST", "CONSOLE", "ARC    ", "CAMERA ", "DISPLAY", "INPUT  ", "MAPCELL",
    "MAP    ", "POINT  ", "RAY    ", "RECT   ", "UUID   ", "VECTOR ", "TILESET", "AUDIO  ",
    "COLLIDX", "TEMP   "
};

static inline size_t _align_up(size_t n, size_t align) {
    return (n + align - 1) & ~(align - 1);
}

static inline size_t _hash_ptr(void *ptr) {
    // Simple hash function for pointer values
    uintptr_t addr = (uintptr_t)ptr;
    return (addr >> 4) & ALLOC_TABLE_MASK;  // Shift right 4 since ptrs are aligned
}

static void _track_alloc(MemoryManager *manager, void *ptr, size_t size, MemTag tag) {
    size_t hash = _hash_ptr(ptr);
    
    AllocEntry *entry = (AllocEntry *)malloc(sizeof(AllocEntry));
    if (!entry) return; // Tracking failure, but don't fail the allocation
    
    entry->ptr = ptr;
    entry->size = size;
    entry->tag = tag;
    entry->next = manager->alloc_table[hash];

#if MEMORY_TRACKING == 1
    /* capture a small backtrace for entity allocations to help find leaks */
    entry->bt_size = 0;
    entry->bt_size = backtrace(entry->bt, sizeof(entry->bt) / sizeof(entry->bt[0]));
#endif

    manager->alloc_table[hash] = entry;
    
    DEBUG_PRINTF("TRACK_ALLOC: tracking %p -> %zu bytes\n", ptr, size);
}

static AllocEntry *_find_and_remove_alloc(MemoryManager *manager, void *ptr) {
    size_t hash = _hash_ptr(ptr);
    
    AllocEntry **pp = &manager->alloc_table[hash];
    while (*pp) {
        AllocEntry *entry = *pp;
        if (entry->ptr == ptr) {
            *pp = entry->next; // Remove from list
            DEBUG_PRINTF("TRACK_FREE: found %p -> %zu bytes\n", ptr, entry->size);
            return entry;
        }
        pp = &entry->next;
    }
    
    DEBUG_PRINTF("TRACK_FREE: WARNING - %p not found in allocation table\n", ptr);
    return NULL;
}

#if MEMORY_TRACKING == 1 && MEMORY_TRACK_FREE == 1
static AllocEntry *_find_in_freed_table(MemoryManager *manager, void *ptr) {
    size_t hash = _hash_ptr(ptr);
    
    AllocEntry *entry = manager->freed_table[hash];
    while (entry) {
        if (entry->ptr == ptr) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

static void _add_to_freed_table(MemoryManager *manager, AllocEntry *entry) {
    size_t hash = _hash_ptr(entry->ptr);
    
    // Create a copy of the entry for the freed table
    AllocEntry *freed_entry = (AllocEntry *)malloc(sizeof(AllocEntry));
    if (!freed_entry) return; // Tracking failure, but don't fail the free
    
    // Copy all the data
    freed_entry->ptr = entry->ptr;
    freed_entry->size = entry->size;
    freed_entry->tag = entry->tag;
    freed_entry->next = manager->freed_table[hash];
#if MEMORY_TRACKING == 1
    // Copy backtrace data
    freed_entry->bt_size = entry->bt_size;
    for (int i = 0; i < entry->bt_size && i < 16; i++) {
        freed_entry->bt[i] = entry->bt[i];
    }
#endif
    
    manager->freed_table[hash] = freed_entry;
}

static void _print_backtrace(AllocEntry *entry) {
    if (entry->bt_size > 0) {
        char **symbols = backtrace_symbols(entry->bt, entry->bt_size);
        if (symbols) {
            fprintf(stderr, "      Backtrace (most recent first):\n");
            for (int bi = 0; bi < entry->bt_size; bi++) {
                fprintf(stderr, "        %s\n", symbols[bi]);
            }
            free(symbols);
        }
    }
}
#endif

static void _abort_with_report(MemoryManager *manager, const char *msg) {
    if (manager) {
        fprintf(stderr, "\n=== Memory Manager State ===\n");
        fprintf(stderr, "Current usage: %zu bytes\n", manager->global.current_usage);
        fprintf(stderr, "Max usage: %zu bytes\n", manager->global.max_usage);
        fprintf(stderr, "Total allocs: %zu\n", manager->global.total_allocs);
        fprintf(stderr, "Total frees: %zu\n", manager->global.total_frees);
    }
    
    fprintf(stderr, "\nFATAL: %s\n\n", msg);
    
    void *buffer[32];
    int nptrs = backtrace(buffer, 32);
    char **strings = backtrace_symbols(buffer, nptrs);
    if (strings) {
        fprintf(stderr, "---- BACKTRACE START ----\n");
        for (int i = 0; i < nptrs; i++) {
            fprintf(stderr, "%s\n", strings[i]);
        }
        fprintf(stderr, "---- BACKTRACE  END  ----\n");
        free(strings);
    }
    fprintf(stderr, "\n");
    abort();
}

static MemoryManager *_get_manager(void) {
    if (!g_memory_manager) {
        DEBUG_PRINTF("GET_MANAGER: Creating new manager\n");
        g_memory_manager = (MemoryManager *)calloc(1, sizeof(MemoryManager));
        if (!g_memory_manager) {
            fprintf(stderr, "FATAL: Out of memory (manager struct)\n");
            abort();
        }
    }
    return g_memory_manager;
}

static void *_mm_malloc(size_t size, MemTag tag) {
    DEBUG_PRINTF("MALLOC: size=%zu, tag=%d\n", size, tag);
    
    MemoryManager *manager = _get_manager();
    
    // Use aligned_alloc for guaranteed 16-byte alignment
    size_t aligned_size = _align_up(size, 16);
    void *ptr = aligned_alloc(16, aligned_size);
    
    if (!ptr) {
        DEBUG_PRINTF("MALLOC: aligned_alloc failed for size %zu\n", aligned_size);
        _abort_with_report(manager, "Failed to allocate memory");
        return NULL; // never reached
    }
    
    // Track the allocation for leak detection
    _track_alloc(manager, ptr, size, tag);
    
    // Update stats
    manager->global.current_usage += size;
    manager->global.total_bytes_alloced += size;
    if (manager->global.current_usage > manager->global.max_usage)
        manager->global.max_usage = manager->global.current_usage;
    if (size > manager->global.largest_alloc)
        manager->global.largest_alloc = size;
    manager->global.total_allocs++;
    
    if (tag >= 0 && tag < MMTAG_COUNT) {
        MemStats *s = &manager->tags[tag];
        s->current_usage += size;
        s->total_bytes_alloced += size;
        if (s->current_usage > s->max_usage) s->max_usage = s->current_usage;
        if (size > s->largest_alloc) s->largest_alloc = size;
        s->total_allocs++;
    }
    
    DEBUG_PRINTF("MALLOC: returning ptr=%p (16-byte aligned)\n", ptr);
    return ptr;
}

static void *_mm_calloc(size_t count, size_t size, MemTag tag) {
    if (size != 0 && count > SIZE_MAX / size) {
        MemoryManager *manager = _get_manager();
        _abort_with_report(manager, "Invalid calloc parameters");
        return NULL; // never reached
    }
    size_t total_size = count * size;
    void *ptr = _mm_malloc(total_size, tag);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

static void _mm_free(void *ptr) {
    if (!ptr) return;
    
    DEBUG_PRINTF("FREE: freeing ptr=%p\n", ptr);
    
    MemoryManager *manager = _get_manager();
    AllocEntry *entry = NULL;
    
#if MEMORY_TRACKING == 1 && MEMORY_TRACK_FREE == 1
    // Check for double-free: pointer must be in freed table AND not in allocation table
    AllocEntry *freed_entry = _find_in_freed_table(manager, ptr);
    if (freed_entry) {
        // Double-check that it's not currently allocated (realloc case)
        AllocEntry *current_entry = _find_and_remove_alloc(manager, ptr);
        if (!current_entry) {
            // Not currently allocated, so this is a true double-free
            const char *tagname = (freed_entry->tag >= 0 && freed_entry->tag < MMTAG_COUNT) ? 
                                 mem_tag_names[freed_entry->tag] : "UNKNOWN";
            fprintf(stderr, "\n=== DOUBLE-FREE DETECTED ===\n");
            fprintf(stderr, "Pointer: %p\n", ptr);
            fprintf(stderr, "Size: %zu bytes\n", freed_entry->size);
            fprintf(stderr, "Tag: %s\n", tagname);
            fprintf(stderr, "This pointer was already freed previously.\n");
            _print_backtrace(freed_entry);
            _abort_with_report(manager, "Double-free detected");
        } else {
            // It was in allocation table, so this is a legitimate free
            // We already removed it above, so continue with normal free processing
            entry = current_entry;
        }
    }
#endif

    // Find and remove the allocation entry (if not already found above)
    if (!entry) {
        entry = _find_and_remove_alloc(manager, ptr);
    }
    if (entry) {
        size_t size = entry->size;
        MemTag tag = entry->tag;
        
        // Update stats
        manager->global.current_usage -= size;
        manager->global.total_frees++;
        
        if (tag >= 0 && tag < MMTAG_COUNT) {
            MemStats *s = &manager->tags[tag];
            s->current_usage -= size;
            s->total_frees++;
        }
        
#if MEMORY_TRACKING == 1 && MEMORY_TRACK_FREE == 1
        // Add to freed table for double-free detection
        _add_to_freed_table(manager, entry);
        DEBUG_PRINTF("FREE: added to freed table for double-free detection\n");
#endif
        free(entry); // Free the original tracking entry
        DEBUG_PRINTF("FREE: updated stats for %zu bytes, tag %d\n", size, tag);
    } else {
        // Not tracked (shouldn't happen) - this should never happen
        log_assert(0, "Attempting to free untracked pointer %p", ptr);
    }
    
    free(ptr);
    DEBUG_PRINTF("FREE: completed\n");
}

static void *_mm_realloc(void *ptr, size_t size, MemTag tag) {
    if (!ptr) return _mm_malloc(size, tag);
    
    MemoryManager *manager = _get_manager();
    
    // Find the old allocation to get its size
    size_t hash = _hash_ptr(ptr);
    AllocEntry *entry = manager->alloc_table[hash];
    size_t old_size = 0;
    
    while (entry) {
        if (entry->ptr == ptr) {
            old_size = entry->size;
            break;
        }
        entry = entry->next;
    }
    
    // Allocate new memory - always succeeds
    void *new_ptr = _mm_malloc(size, tag);
    
    // Copy data (use smaller of old/new size)
    size_t copy_size = (old_size > 0 && old_size < size) ? old_size : size;
    if (old_size > 0) {
        memcpy(new_ptr, ptr, copy_size);
    }
    
    _mm_free(ptr);
    return new_ptr;
}

static char *_mm_strdup(const char *str, MemTag tag) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char *copy = (char *)_mm_malloc(len, tag);
    memcpy(copy, str, len);
    return copy;
}

static void _mm_report(void) {
    MemoryManager *manager = _get_manager();
    printf("=== Memory Usage Report ===\n");
    printf("Global:\n");
    printf("  Current usage:  %zu bytes\n", manager->global.current_usage);
    printf("  Max usage:      %zu bytes\n", manager->global.max_usage);
    printf("  Total allocs:   %zu\n", manager->global.total_allocs);
    printf("  Total frees:    %zu\n", manager->global.total_frees);
    printf("  Largest alloc:  %zu bytes\n", manager->global.largest_alloc);
    printf("  Average alloc:  %zu bytes\n", 
           manager->global.total_allocs ? manager->global.total_bytes_alloced / manager->global.total_allocs : 0);
    printf("  Total allocated: %zu bytes\n", manager->global.total_bytes_alloced);
    
    // Check for leaks
    size_t leak_count = 0;
    size_t leak_bytes = 0;
    for (size_t i = 0; i < ALLOC_TABLE_SIZE; i++) {
        AllocEntry *entry = manager->alloc_table[i];
        while (entry) {
            leak_count++;
            leak_bytes += entry->size;
            entry = entry->next;
        }
    }

    if (leak_count > 0) {
        printf("  WARNING: %zu memory leaks detected (%zu bytes leaked)!\n", leak_count, leak_bytes);
        
        // Print some leak details (limit to avoid spam)
        printf("  Sample leaks:\n");
        size_t shown = 0;
#if MEMORY_TRACKING != 1
        for (size_t i = 0; i < ALLOC_TABLE_SIZE && shown < 10; i++) {
            AllocEntry *entry = manager->alloc_table[i];
            while (entry && shown < 10) {
                printf("    %p: %zu bytes (%s)\n", entry->ptr, entry->size,
                       (entry->tag >= 0 && entry->tag < MMTAG_COUNT) ? mem_tag_names[entry->tag] : "UNKNOWN");
                entry = entry->next;
                shown++;
            }
        }
#else
        for (size_t i = 0; i < ALLOC_TABLE_SIZE && shown < 10; i++) {
            AllocEntry *entry = manager->alloc_table[i];
            while (entry && shown < 10) {
                const char *tagname = (entry->tag >= 0 && entry->tag < MMTAG_COUNT) ? mem_tag_names[entry->tag] : "UNKNOWN";
                printf("    %p: %zu bytes (%s)\n", entry->ptr, entry->size, tagname);
                /* If we captured a backtrace for this allocation, print it */
                if (entry->bt_size > 0) {
                    char **symbols = backtrace_symbols(entry->bt, entry->bt_size);
                    if (symbols) {
                        printf("      Backtrace (most recent first):\n");
                        for (int bi = 0; bi < entry->bt_size; bi++) {
                            printf("        %s\n", symbols[bi]);
                        }
                        free(symbols);
                    }
                }
                entry = entry->next;
                shown++;
            }
        }
#endif
        if (leak_count > 10) {
            printf("    ... and %zu more leaks\n", leak_count - 10);
        }
    }
    
    printf("\nPer-Tag:\n");
    for (int i = 0; i < MMTAG_COUNT; ++i) {
        const MemStats *s = &manager->tags[i];
        if (s->total_allocs > 0) {
            printf("  [%s] current=%zu, max=%zu, allocs=%zu, frees=%zu, largest=%zu, avg=%zu\n",
                mem_tag_names[i],
                s->current_usage,
                s->max_usage,
                s->total_allocs,
                s->total_frees,
                s->largest_alloc,
                s->total_allocs ? s->total_bytes_alloced / s->total_allocs : 0
            );
        }
    }
}

static size_t _mm_get_current_usage(void) {
    MemoryManager *manager = _get_manager();
    return manager->global.current_usage;
}

static size_t _mm_get_max_usage(void) {
    MemoryManager *manager = _get_manager();
    return manager->global.max_usage;
}

static void _mm_destroy(void) {
    if (!g_memory_manager) return;
    
    _mm_report();
    
    // Free any remaining allocation tracking entries
    for (size_t i = 0; i < ALLOC_TABLE_SIZE; i++) {
        AllocEntry *entry = g_memory_manager->alloc_table[i];
        while (entry) {
            AllocEntry *next = entry->next;
            free(entry);
            entry = next;
        }
    }
    
#if MEMORY_TRACKING == 1 && MEMORY_TRACK_FREE == 1
    // Free any remaining freed tracking entries
    for (size_t i = 0; i < ALLOC_TABLE_SIZE; i++) {
        AllocEntry *entry = g_memory_manager->freed_table[i];
        while (entry) {
            AllocEntry *next = entry->next;
            free(entry);
            entry = next;
        }
    }
#endif
    
    free(g_memory_manager);
    g_memory_manager = NULL;
}

const struct memory_manager_api memory_manager = {
    .malloc = _mm_malloc,
    .calloc = _mm_calloc,
    .realloc = _mm_realloc,
    .free = _mm_free,
    .strdup = _mm_strdup,
    .report = _mm_report,
    .get_current_usage = _mm_get_current_usage,
    .get_max_usage = _mm_get_max_usage,
    .destroy = _mm_destroy
};

void debug_check_memory(void *ptr, const char *location) {
    // No validation possible without headers
    (void)ptr;
    (void)location;
}