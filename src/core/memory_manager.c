#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <execinfo.h>
#include <stdalign.h>
#include "core/memory_manager.h"
#include "utility/log.h"

#undef MEM_USE_SYS

#define MEM_MAGIC_HEADER 0xDEADC0DECAFEBABEULL
#define MEM_MAGIC_FOOTER 0xBEEFCAFE12345678ULL

#define DEBUG_MEMORY_MANAGER 0

#if DEBUG_MEMORY_MANAGER
#define DEBUG_PRINTF(fmt, ...) do { printf(fmt, ##__VA_ARGS__); fflush(stdout); } while(0)
#else
#define DEBUG_PRINTF(fmt, ...) do { } while(0)
#endif

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#define BUCKET_COUNT 6

/**
 * @brief Header structure for allocated memory blocks.
 * 
 * @details This structure is prepended to every allocated memory block
 *          and contains metadata for memory management, including size,
 *          magic number for corruption detection, memory tag, and link
 *          to the next allocated block.
 */
typedef struct MemHeader {
    size_t size;                    /**< Size of the allocated block in bytes */
    uint64_t magic;                 /**< Magic number for corruption detection */
    int tag;                        /**< Memory tag for categorization and tracking */
    struct MemHeader *next;         /**< Pointer to next allocated block in list */
} MemHeader;

/**
 * @brief Footer structure for allocated memory blocks.
 * 
 * @details This structure is appended to every allocated memory block
 *          and contains a magic number for corruption detection at the
 *          end of the block.
 */
typedef struct MemFooter {
    uint64_t magic;                 /**< Magic number for corruption detection */
} MemFooter;

/**
 * @brief Structure for free memory chunks in the free list.
 * 
 * @details This structure represents a contiguous block of free memory
 *          and is used to link free chunks together in size-sorted
 *          free lists for efficient allocation.
 */
typedef struct FreeChunk {
    size_t size;                    /**< Size of the free chunk in bytes */
    struct FreeChunk *next;         /**< Pointer to next free chunk in list */
} FreeChunk;

/**
 * @brief Memory block structure for the memory manager.
 * 
 * @details This structure represents a large block of memory allocated
 *          from the system, which is then subdivided into smaller chunks
 *          for allocation requests. The data array contains the actual
 *          memory available for allocation.
 */
typedef struct Block {
    struct Block *next;             /**< Pointer to next memory block */
    size_t size;                    /**< Total size of this memory block */
    char data[];                    /**< Flexible array member containing the memory */
} Block;

/**
 * @brief Statistics structure for memory usage tracking.
 * 
 * @details This structure tracks various memory usage statistics including
 *          current and maximum usage, allocation counts, and bucket-specific
 *          statistics for different size ranges.
 */
typedef struct {
    size_t current_usage;           /**< Current memory usage in bytes */
    size_t max_usage;               /**< Peak memory usage in bytes */
    size_t total_allocs;            /**< Total number of allocations */
    size_t total_frees;             /**< Total number of deallocations */
    size_t total_bytes_alloced;     /**< Total bytes allocated over time */
    size_t total_blocks_alloced[BUCKET_COUNT]; /**< Allocations per size bucket */
    size_t largest_alloc;           /**< Largest single allocation size */
} MemStats;

/**
 * @brief Main memory manager structure.
 * 
 * @details This structure manages all memory allocation and deallocation
 *          operations. It maintains free lists for different size ranges,
 *          tracks allocated blocks, and provides statistics for monitoring
 *          memory usage patterns.
 */
struct MemoryManager {
    Block *blocks;                          /**< Linked list of allocated memory blocks */
    FreeChunk *free_lists[BUCKET_COUNT];    /**< Free lists for different size ranges */
    MemHeader *allocated_list;              /**< Linked list of allocated memory blocks */
    MemStats global;                        /**< Global memory usage statistics */
    MemStats tags[MMTAG_COUNT];             /**< Per-tag memory usage statistics */
};

typedef struct MemoryManager MemoryManager;

MemoryManager *g_memory_manager = NULL;

static const char *mem_tag_names[MMTAG_COUNT] = {
    "GENERAL", "ENGINE ", "ASSET  ", "ENTITY ", "LUA    ", "LUA VAL", "LUA VM ", "RENDER ",
    "SPRITE ", "DRAWLST", "RENDLST", "SHADER ", "WINDOW ", "ARRAY  ", "HASHMAP", "GRPHASH",
    "LINKLST", "CONSOLE", "ARC    ", "CAMERA ", "DISPLAY", "INPUT  ", "MAPCELL", "MAP    ",
    "POINT  ", "RAY    ", "RECT   ", "UUID   ", "VECTOR ", "TILESET", "TEMP   "
};

static size_t _align_up(size_t n, size_t align) {
    return (n + align - 1) & ~(align - 1);
}

// Forward declaration for strategy implementation
static void *_alloc_slab(MemoryManager *manager, size_t total_size, size_t bucket);
static void *_alloc_best_fit(MemoryManager *manager, size_t total_size, size_t bucket);

/**
 * @brief Get the bucket for a given payload size.
 *
 * @details Based on payload size pick the bucket: 32, 64, 128, 256, 512, 1024.
 *          This function takes the user's requested size (payload) and determines
 *          which bucket it belongs to, ensuring slabs are allocated and freed
 *          to/from the correct bucket.
 */
static size_t _get_bucket(size_t payload_size) {
    if (payload_size <= 32) return 0;
    if (payload_size <= 64) return 1;
    if (payload_size <= 128) return 2;
    if (payload_size <= 256) return 3;
    if (payload_size <= 512) return 4;
    return 5;
}

static size_t _get_slab_size(size_t bucket) {
    if (bucket == 0) return 32;
    if (bucket == 1) return 64;
    if (bucket == 2) return 128;
    return 0;
}

static size_t _get_slab_total(size_t bucket) {
    if (bucket == 0) return _align_up(sizeof(MemHeader), 16) + _align_up(32, 16)  + sizeof(MemFooter);
    if (bucket == 1) return _align_up(sizeof(MemHeader), 16) + _align_up(64, 16)  + sizeof(MemFooter);
    if (bucket == 2) return _align_up(sizeof(MemHeader), 16) + _align_up(128, 16) + sizeof(MemFooter);
    return 0;
}

/**
 * @brief Get allocator strategy function for a bucket.
 *
 * @details For now, all buckets use the same allocator implementation
 *          that allocates from the appropriate free list. This function
 *          allows plugging different strategies per bucket in the future.
 */
typedef void *(*AllocStrategyFn)(MemoryManager *manager, size_t total_size, size_t bucket);

static AllocStrategyFn _get_allocator(size_t bucket) {
    if (bucket <= 2) return _alloc_slab;
    return _alloc_best_fit;
}

static void _set_footer(MemHeader *header) {
    // MemFooter *footer = (MemFooter *)((char *)(header + 1) + header->size);
    size_t padded_size = _align_up(header->size, 16);
    MemFooter *footer = (MemFooter *)((char *)(header + 1) + padded_size);
    footer->magic = MEM_MAGIC_FOOTER;
}

static int _check_footer(MemHeader *header) {
    // MemFooter *footer = (MemFooter *)((char *)(header + 1) + header->size);
    size_t padded_size = _align_up(header->size, 16);
    MemFooter *footer = (MemFooter *)((char *)(header + 1) + padded_size);
    return footer->magic == MEM_MAGIC_FOOTER;
}

static void _debug_print_free_list(MemoryManager *manager, size_t bucket, const char *context) {
    DEBUG_PRINTF("FREE_LIST_DEBUG [%s]: ", context);
    FreeChunk *chunk = manager->free_lists[bucket];
    int count = 0;
    while (chunk && count < 10) {
        DEBUG_PRINTF("(%p:%zu)->", (void*)chunk, chunk->size);
        chunk = chunk->next;
        count++;
    }
    if (chunk) DEBUG_PRINTF("...");
    DEBUG_PRINTF("NULL\n");
    fflush(stdout);
}

static void _debug_check_allocation_integrity(void *ptr, const char *context) {
    if (!ptr) return;
    MemHeader *header = (MemHeader *)((char *)ptr - _align_up(sizeof(MemHeader), 16));
    DEBUG_PRINTF("INTEGRITY_CHECK [%s]: ptr=%p, header=%p, magic=0x%llx, size=%zu\n",
           context, ptr, (void*)header, (unsigned long long)header->magic, header->size);

    if (header->magic != MEM_MAGIC_HEADER) {
        DEBUG_PRINTF("CORRUPTION DETECTED [%s]: BAD HEADER MAGIC!\n", context);
        abort();
    }

    // MemFooter *footer = (MemFooter *)((char *)(header + 1) + header->size);
    size_t padded_size = _align_up(header->size, 16);
    MemFooter *footer = (MemFooter *)((char *)(header + 1) + padded_size);
    DEBUG_PRINTF("INTEGRITY_CHECK [%s]: footer=%p, footer_magic=0x%llx\n",
           context, (void*)footer, (unsigned long long)footer->magic);

    if (footer->magic != MEM_MAGIC_FOOTER) {
        DEBUG_PRINTF("CORRUPTION DETECTED [%s]: BAD FOOTER MAGIC!\n", context);
        abort();
    }

    char *user_data = (char*)ptr;
    DEBUG_PRINTF("INTEGRITY_CHECK [%s]: user_data[0-9]='%.10s'\n", context, user_data);
    fflush(stdout);
}

void debug_check_memory(void *ptr, const char *location) {
    DEBUG_PRINTF("DEBUG_CHECK_MEMORY [%s]: Starting check for ptr=%p\n", location, ptr);
    fflush(stdout);
    _debug_check_allocation_integrity(ptr, location);
    DEBUG_PRINTF("DEBUG_CHECK_MEMORY [%s]: PASSED\n", location);
    fflush(stdout);
}

static void _print_hex_dump(const char *data, size_t size, FILE *out) {
    size_t to_print = (size > 16) ? 16 : size;
    fprintf(out, "[");
    for (size_t i = 0; i < to_print; ++i) {
        fprintf(out, "%02X ", (unsigned char)data[i]);
    }
    if (size > 16) {
        fprintf(out, "...");
    }
    fprintf(out, "]  ");
    fprintf(out, "[");
    for (size_t i = 0; i < to_print; ++i) {
        fprintf(out, "%c", (unsigned char)data[i] < 40 || (unsigned char)data[i] > 126 ? '.' : (unsigned char)data[i]);
    }
    if (size > 16) {
        fprintf(out, " ...");
    }
    fprintf(out, "]\n");
}

static void _memory_manager_report(MemoryManager *manager, FILE *out) {
    if (!manager) return;
    fprintf(out, "=== Memory Usage Report ===\n");
    fprintf(out, "Global:\n");
    fprintf(out, "  Current usage:  %zu bytes\n", manager->global.current_usage);
    fprintf(out, "  Max usage:      %zu bytes\n", manager->global.max_usage);
    fprintf(out, "  Total allocs:   %zu\n", manager->global.total_allocs);
    fprintf(out, "  Total frees:    %zu\n", manager->global.total_frees);
    fprintf(out, "  Largest alloc:  %zu bytes\n", manager->global.largest_alloc);
    fprintf(out, "  Average alloc:  %zu bytes\n", manager->global.total_allocs ? manager->global.total_bytes_alloced / manager->global.total_allocs : 0);
    fprintf(out, "  Blocks (<=32):  %zu\n", manager->global.total_blocks_alloced[0]);
    fprintf(out, "  Blocks (<=64):  %zu\n", manager->global.total_blocks_alloced[1]);
    fprintf(out, "  Blocks (<=128): %zu\n", manager->global.total_blocks_alloced[2]);
    fprintf(out, "  Blocks (<=256): %zu\n", manager->global.total_blocks_alloced[3]);
    fprintf(out, "  Blocks (<=512): %zu\n", manager->global.total_blocks_alloced[4]);
    fprintf(out, "  Blocks (>512):  %zu\n", manager->global.total_blocks_alloced[5]);
    if (manager->global.current_usage != 0) {
        fprintf(out, "  WARNING: Memory leak detected!\n");
    }

    fprintf(out, "\nPer-Tag:\n");
    for (int i = 0; i < MMTAG_COUNT; ++i) {
        const MemStats *s = &manager->tags[i];
        fprintf(out, "  [%s] current=%zu, max=%zu, allocs=%zu, frees=%zu, largest=%zu, avg=%zu\n",
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

static void _abort_with_report(MemoryManager *manager, const char *msg) {
    _memory_manager_report(manager, stderr);
    fprintf(stderr, "\nFATAL: %s\n\n", msg);
    
    // Check for leaks and print details
    // if (manager->allocated_list) {
    //     fprintf(stderr, "Unfreed Allocations:\n");
    //     int leak_count = 1;
    //     MemHeader *current = manager->allocated_list;
    //     while (current) {
    //         char *user_ptr = (char *)current + _align_up(sizeof(MemHeader), 16);
    //         fprintf(stderr, "%d) %zu Bytes ", leak_count, current->size);
    //         _print_hex_dump(user_ptr, current->size, stderr);
    //         current = current->next;
    //         leak_count++;
    //     }
    // }

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

static int _freechunk_addr_cmp(const void *pa, const void *pb) {
    const FreeChunk *a = *(const FreeChunk *const *)pa;
    const FreeChunk *b = *(const FreeChunk *const *)pb;
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

static void _coalesce_free_list(MemoryManager *manager, size_t bucket) {    
    // Count free chunks
    size_t count = 0;
    for (FreeChunk *c = manager->free_lists[bucket]; c; c = c->next) {
        count++;
    }
    if (count < 2) {
        log_verbose("MEM_COALESCE", "nothing to coalesce (count=%zu)", count);
        return;
    }
    
    // Log ALL chunks if there are multiple
    if (count >= 2) {
        log_verbose("MEM_COALESCE", "COALESCE: Found %zu chunks to check:", count);
        int i = 0;
        for (FreeChunk *c = manager->free_lists[bucket]; c && i < 20; c = c->next, i++) {
            log_verbose("MEM_COALESCE", "  [%d] addr=%p size=%zu", i, (void*)c, c->size);
        }
    } else {
        log_verbose("MEM_COALESCE", "nothing to coalesce (count=%zu)", count);
        return;
    }

    // Collect pointers into an array
    FreeChunk **arr = (FreeChunk **)malloc(count * sizeof(FreeChunk *));
    if (!arr) {
        log_verbose("MEM_COALESCE", "malloc failed, skipping");
        return;
    }
    size_t i = 0;
    for (FreeChunk *c = manager->free_lists[bucket]; c; c = c->next) {
        arr[i++] = c;
    }

    // Sort by address
    qsort(arr, count, sizeof(FreeChunk *), _freechunk_addr_cmp);

    // Merge adjacent chunks in-place over arr
    size_t w = 0;
    for (size_t r = 0; r < count; ++r) {
        if (w == 0) {
            arr[w++] = arr[r];
        } else {
            FreeChunk *prev = arr[w - 1];
            char *prev_end = (char *)prev + prev->size;
            if (prev_end == (char *)arr[r]) {
                // Adjacent: merge sizes
                prev->size += arr[r]->size;
            } else {
                arr[w++] = arr[r];
            }
        }
    }

    // Rebuild free list (LIFO is fine)
    manager->free_lists[bucket] = NULL;
    for (size_t k = 0; k < w; ++k) {
        arr[k]->next = manager->free_lists[bucket];
        manager->free_lists[bucket] = arr[k];
    }

    free(arr);
    log_verbose("MEM_COALESCE", "chunks=%zu -> %zu\n", count, w);
    fflush(stdout);
}

static void _add_block(MemoryManager *manager, size_t bucket, size_t min_size) {
    DEBUG_PRINTF("ADD_BLOCK: min_size=%zu\n", min_size);
    fflush(stdout);

    size_t block_size = (min_size > MM_BLOCK_SIZE) ? _align_up(min_size, 16) : _align_up(MM_BLOCK_SIZE, 16);
    Block *block = (Block *)malloc(sizeof(Block) + block_size);
    if (!block) {
        _abort_with_report(manager, "Out of memory (system malloc failed)");
    }

    DEBUG_PRINTF("ADD_BLOCK: allocated block=%p, size=%zu\n", (void*)block, block_size);
    fflush(stdout);

    block->next = manager->blocks;
    block->size = block_size;
    manager->blocks = block;
    manager->global.total_blocks_alloced[bucket] += 1;

    size_t slab_total = _get_slab_total(bucket);
    if (slab_total > 0) {
        uintptr_t base = (uintptr_t)(block->data);
        uintptr_t aligned_base = (base + 15u) & ~((uintptr_t)15u);
        size_t usable = block_size - (aligned_base - base);
        size_t slabs_per_block = usable / slab_total;

        char *cursor = (char *)aligned_base;
        for (size_t i = 0; i < slabs_per_block; i++) {
            FreeChunk *chunk = (FreeChunk *)cursor;
            chunk->size = slab_total;
            chunk->next = manager->free_lists[bucket];
            manager->free_lists[bucket] = chunk;
            cursor += slab_total;
        }
    } else {
        // Add the entire block to the free list as one big chunk
        FreeChunk *chunk = (FreeChunk *)block->data;
        chunk->size = block_size;
        chunk->next = manager->free_lists[bucket];
        manager->free_lists[bucket] = chunk;
    }

    DEBUG_PRINTF("ADD_BLOCK: added entire block to free list as chunk=%p size=%zu\n", 
                 (void*)chunk, block_size);
    fflush(stdout);
}

static void *_alloc_slab(MemoryManager *manager, size_t total_size, size_t bucket) {
    // Check if we have any slabs available
    FreeChunk *slab = manager->free_lists[bucket];
    if (!slab) {
        return NULL;
    }

    // Safety: ensure the slab is large enough
    if (slab->size < total_size) {
        _abort_with_report(manager, "Slab too small for requested allocation");
    }
    
    // Remove slab from free list
    manager->free_lists[bucket] = slab->next;
    
    DEBUG_PRINTF("ALLOC_SLAB: Returning slab %p (size=%zu)\n", (void*)slab, slab->size);
    return (void*)slab;
}

static void *_alloc_best_fit(MemoryManager *manager, size_t total_size, size_t bucket) {
    DEBUG_PRINTF("ALLOC_FREE_LIST: Looking for size=%zu\n", total_size);
    fflush(stdout);
    FreeChunk **best_pp = NULL;
    FreeChunk *best_chunk = NULL;
    size_t best_size = SIZE_MAX;
    
    // Find the best-fitting chunk (smallest that fits)
    FreeChunk **pp = &manager->free_lists[bucket];
    while (*pp) {
        FreeChunk *chunk = *pp;
        if (chunk->size >= total_size && chunk->size < best_size) {
            best_pp = pp;
            best_chunk = chunk;
            best_size = chunk->size;
            
            // Perfect fit? Use it immediately
            if (chunk->size == total_size) {
                break;
            }
        }
        pp = &(*pp)->next;
    }
    
    if (!best_chunk) {
        DEBUG_PRINTF("ALLOC_FREE_LIST: No suitable chunk found\n");
        fflush(stdout);
        return NULL;
    }

    MemHeader *maybe_header = (MemHeader *)best_chunk;
    if (maybe_header->magic == MEM_MAGIC_HEADER) {
        _abort_with_report(manager, "Allocated memory in free list");
    }

    // Remove chosen chunk from list
    *best_pp = best_chunk->next;
    
    // Split if there's significant leftover space
    size_t remain = best_chunk->size - total_size;
    if (remain >= sizeof(FreeChunk) + 64) {  // Increased minimum split size to reduce fragments
        FreeChunk *tail = (FreeChunk *)((char *)best_chunk + total_size);
        tail->size = remain;
        tail->next = manager->free_lists[bucket];
        manager->free_lists[bucket] = tail;
    } else if (remain > 0) {
        // Don't split tiny remainders - give the caller slightly more than requested
        DEBUG_PRINTF("ALLOC_FREE_LIST: Using whole chunk to avoid tiny fragment (waste=%zu)\n", remain);
        fflush(stdout);
    }
    
    _debug_print_free_list(manager, bucket, "after_alloc");
    DEBUG_PRINTF("ALLOC_FREE_LIST: Returning %p (size=%zu, waste=%zu)\n", 
                 (void *)best_chunk, best_chunk->size, 
                 best_chunk->size - total_size);
    fflush(stdout);
    return (void *)best_chunk;
}

static MemoryManager *_get_manager(void) {
    if (!g_memory_manager) {
        DEBUG_PRINTF("GET_MANAGER: Creating new manager\n");
        fflush(stdout);
        g_memory_manager = (MemoryManager *)calloc(1, sizeof(MemoryManager));
        if (!g_memory_manager) {
            fprintf(stderr, "FATAL: Out of memory (manager struct)\n");
            abort();
        }
    }
    return g_memory_manager;
}

static void *_mm_malloc(size_t size, MemTag tag) {
#ifdef MEM_USE_SYS
    return malloc(size);
#else
    DEBUG_PRINTF("MALLOC: size=%zu, tag=%d\n", size, tag);
    fflush(stdout);

    MemoryManager *manager = _get_manager();
    // size_t total = _align_up(sizeof(MemHeader), 16) + size + sizeof(MemFooter);
    size_t total = _align_up(sizeof(MemHeader), 16) + _align_up(size, 16) + sizeof(MemFooter);

    DEBUG_PRINTF("MALLOC: total_with_headers=%zu\n", total);
    fflush(stdout);

    void *mem = NULL;


    // Try allocating from free list
    size_t bucket = _get_bucket(size);  // Use payload size for bucket selection
    mem = _get_allocator(bucket)(manager, total, bucket);
    if (mem) {
        log_verbose("MEM_MALLOC", "Allocated %p = %zu bytes from free list", mem, total);
    } else {
        // Add new block to free list and try again
        _add_block(manager, bucket, max(total, MM_BLOCK_SIZE));
        mem = _get_allocator(bucket)(manager, total, bucket);
        if (!mem) {
            _abort_with_report(manager, "Failed to allocate from free list after adding new block");
        }
        log_verbose("MEM_MALLOC", "Allocated %p = %zu bytes from free list after adding block", mem, total);
    }

    // Setup header and footer (unchanged)
    MemHeader *header = (MemHeader *)mem;
    header->size = size;
    header->magic = MEM_MAGIC_HEADER;
    header->tag = tag;
    header->next = manager->allocated_list;
    manager->allocated_list = header;
    
    _set_footer(header);

    // void *user_ptr = (void *)((char *)header + _align_up(sizeof(MemHeader), 16));
    void *user_ptr = (void *)((char *)header + _align_up(sizeof(MemHeader), 16));

    // Update stats (unchanged)
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

    DEBUG_PRINTF("MALLOC: returning user_ptr=%p\n", user_ptr);
    fflush(stdout);

    size_t clear_size = _align_up(sizeof(MemHeader), 16);
    if (clear_size < sizeof(FreeChunk)) {
        // Zero out any remaining FreeChunk metadata
        char *clear_start = (char*)header + sizeof(MemHeader);
        size_t clear_len = sizeof(FreeChunk) - sizeof(MemHeader);
        if (clear_len > 0 && clear_len < size) {
            memset(clear_start, 0, clear_len);
        }
    }

    return user_ptr;
#endif
}

static void *_mm_calloc(size_t count, size_t size, MemTag tag) {
    if (size != 0 && count > SIZE_MAX / size) {
        _abort_with_report(_get_manager(), "calloc size overflow");
    }
    size_t requested_total = count * size;
    void *ptr = _mm_malloc(requested_total, tag);
    if (ptr) {
        // ONLY zero the requested size, not the entire allocated chunk
        memset(ptr, 0, requested_total);
    }
    return ptr;
}

static void _mm_free(void *ptr) {
    if (!ptr) return;

#ifdef MEM_USE_SYS
    free(ptr);
    return;
#else

    log_verbose("MEM_FREE", "freeing ptr=%p", ptr);
    fflush(stdout);

    MemoryManager *manager = _get_manager();
    MemHeader *header = (MemHeader *)((char *)ptr - _align_up(sizeof(MemHeader), 16));
    
    // Calculate the same total size used in allocation  
    size_t padded_size = _align_up(header->size, 16);
    size_t total = _align_up(sizeof(MemHeader), 16) + padded_size + sizeof(MemFooter);
    size_t bucket = _get_bucket(header->size);  // Use payload size for bucket selection

    DEBUG_PRINTF("FREE: header=%p, checking integrity\n", (void*)header);
    fflush(stdout);

    if (header->magic != MEM_MAGIC_HEADER) {
        _abort_with_report(manager, "Header corruption detected in free");
    }
    if (!_check_footer(header)) {
        _abort_with_report(manager, "Footer corruption detected in free (buffer over/underflow)");
    }
    
    // Remove from the allocated list
    MemHeader **pp = &manager->allocated_list;
    while (*pp && *pp != header) {
        pp = &(*pp)->next;
    }
    if (*pp) {
        *pp = header->next;
    } else {
        _abort_with_report(manager, "Attempted to free unmanaged or already freed memory.");
    }

    size_t size = header->size;
    int tag = header->tag;

    DEBUG_PRINTF("FREE: size=%zu, tag=%d\n", size, tag);
    fflush(stdout);

    manager->global.current_usage -= size;
    manager->global.total_frees++;
    if (tag >= 0 && tag < MMTAG_COUNT) {
        MemStats *s = &manager->tags[tag];
        s->current_usage -= size;
        s->total_frees++;
    }

    // Clear magic FIRST
    header->magic = 0;
    MemFooter *footer = (MemFooter *)((char *)(header + 1) + padded_size);
    // MemFooter *footer = (MemFooter *)((char *)(header + 1) + size);
    footer->magic = 0;

    // THEN create the FreeChunk
    FreeChunk *chunk = (FreeChunk *)header;
    
    // For slab buckets, use the actual slab size to maintain bucket consistency
    size_t slab_total = _get_slab_total(bucket);
    if (slab_total > 0) {
        chunk->size = slab_total;
    } else {
        chunk->size = total;
    }
    
    chunk->next = manager->free_lists[bucket];
    manager->free_lists[bucket] = chunk;

    if (slab_total == 0) {
        static int free_count = 0;
        if (++free_count % 100 == 0) {
            _coalesce_free_list(manager, bucket);
        }
    }

    DEBUG_PRINTF("FREE: completed for ptr=%p\n", ptr);
    fflush(stdout);
#endif
}

static void *_mm_realloc(void *ptr, size_t size, MemTag tag) {
    if (!ptr) return _mm_malloc(size, tag);

#ifdef MEM_USE_SYS
    return realloc(ptr, size);
#else

    MemoryManager *manager = _get_manager();
    MemHeader *old_header = (MemHeader *)((char *)ptr - _align_up(sizeof(MemHeader), 16));
    if (old_header->magic != MEM_MAGIC_HEADER) {
        _abort_with_report(manager, "Header corruption detected in realloc");
    }
    if (!_check_footer(old_header)) {
        _abort_with_report(manager, "Footer corruption detected in realloc");
    }
    size_t old_size = old_header->size;
    
    void *new_ptr = _mm_malloc(size, old_header->tag);
    if (!new_ptr) _abort_with_report(manager, "Out of memory in realloc");
    memcpy(new_ptr, ptr, (size < old_size) ? size : old_size);
    _mm_free(ptr);
    return new_ptr;
#endif
}

static char *_mm_strdup(const char *str, MemTag tag) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char *copy = (char *)_mm_malloc(len, tag);
    if (copy) memcpy(copy, str, len);
    return copy;
}

static void _mm_report(void) {
    MemoryManager *manager = _get_manager();
    _memory_manager_report(manager, stdout);
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

    struct timespec t0, t_report, t_before_free, t_after_free;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    _memory_manager_report(g_memory_manager, stdout);
    clock_gettime(CLOCK_MONOTONIC, &t_report);

    size_t block_count = 0;
    size_t total_block_bytes = 0;
    Block *b = g_memory_manager->blocks;
    while (b) {
        block_count++;
        total_block_bytes += b->size;
        b = b->next;
    }
    fflush(stdout);

    clock_gettime(CLOCK_MONOTONIC, &t_before_free);

    Block *block = g_memory_manager->blocks;
    while (block) {
        Block *next = block->next;
        free(block);
        block = next;
    }

    clock_gettime(CLOCK_MONOTONIC, &t_after_free);

    double s_report = (t_report.tv_sec - t0.tv_sec) +
                      (t_report.tv_nsec - t0.tv_nsec) * 1e-9;
    double s_count  = (t_before_free.tv_sec - t_report.tv_sec) +
                      (t_before_free.tv_nsec - t_report.tv_nsec) * 1e-9;
    double s_free   = (t_after_free.tv_sec - t_before_free.tv_sec) +
                      (t_after_free.tv_nsec - t_before_free.tv_nsec) * 1e-9;

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
