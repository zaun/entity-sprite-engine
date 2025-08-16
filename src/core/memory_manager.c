#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <execinfo.h>
#include <stdalign.h>
#include "core/memory_manager.h"

// Remove C11 alignof requirement with a fallback
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
  #include <stdalign.h>
  #define MM_ALIGN alignof(max_align_t)
#else
  // C99 or earlier: safe fallback
  #define MM_ALIGN sizeof(void*)
#endif

#define MEM_MAGIC_HEADER 0xDEADC0DECAFEBABEULL
#define MEM_MAGIC_FOOTER 0xBEEFCAFE12345678ULL

#define DEBUG_MEMORY_MANAGER 0

#if DEBUG_MEMORY_MANAGER
#define DEBUG_PRINTF(fmt, ...) do { printf(fmt, ##__VA_ARGS__); fflush(stdout); } while(0)
#else
#define DEBUG_PRINTF(fmt, ...) do { } while(0)
#endif

typedef struct MemHeader {
    size_t size;
    uint64_t magic;
    int tag;
    struct MemHeader *next;
} MemHeader;

typedef struct MemFooter {
    uint64_t magic;
} MemFooter;

typedef struct FreeChunk {
    size_t size;
    struct FreeChunk *next;
} FreeChunk;

typedef struct Block {
    struct Block *next;
    size_t size;
    size_t used;
    char data[];
} Block;

typedef struct {
    size_t current_usage;
    size_t max_usage;
    size_t total_allocs;
    size_t total_frees;
    size_t total_bytes_alloced;
    size_t largest_alloc;
} MemStats;

struct MemoryManager {
    Block *blocks;
    FreeChunk *free_list;
    MemHeader *allocated_list; // Added to track all active allocations
    MemStats global;
    MemStats tags[MMTAG_COUNT];
};

typedef struct MemoryManager MemoryManager;

MemoryManager *g_memory_manager = NULL;

static const char *mem_tag_names[MMTAG_COUNT] = {
    "GENERAL", "ENGINE ", "ASSET  ", "ENTITY ", "LUA    ", "RENDER ", "MAP    ", "SPRITE ",
    "USER1  ", "USER2  ", "USER3  ", "USER4  ", "USER5  "
};

static size_t _align_up(size_t n, size_t align) {
    return (n + align - 1) & ~(align - 1);
}

static void _set_footer(MemHeader *header) {
    MemFooter *footer = (MemFooter *)((char *)(header + 1) + header->size);
    footer->magic = MEM_MAGIC_FOOTER;
}

static int _check_footer(MemHeader *header) {
    MemFooter *footer = (MemFooter *)((char *)(header + 1) + header->size);
    return footer->magic == MEM_MAGIC_FOOTER;
}

static void _debug_print_free_list(MemoryManager *manager, const char *context) {
    DEBUG_PRINTF("FREE_LIST_DEBUG [%s]: ", context);
    FreeChunk *chunk = manager->free_list;
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
    MemHeader *header = (MemHeader *)((char *)ptr - _align_up(sizeof(MemHeader), MM_ALIGN));
    DEBUG_PRINTF("INTEGRITY_CHECK [%s]: ptr=%p, header=%p, magic=0x%llx, size=%zu\n",
           context, ptr, (void*)header, (unsigned long long)header->magic, header->size);

    if (header->magic != MEM_MAGIC_HEADER) {
        DEBUG_PRINTF("CORRUPTION DETECTED [%s]: BAD HEADER MAGIC!\n", context);
        abort();
    }

    MemFooter *footer = (MemFooter *)((char *)(header + 1) + header->size);
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
    fprintf(out, "  Current usage: %zu bytes\n", manager->global.current_usage);
    fprintf(out, "  Max usage:     %zu bytes\n", manager->global.max_usage);
    fprintf(out, "  Total allocs:  %zu\n", manager->global.total_allocs);
    fprintf(out, "  Total frees:   %zu\n", manager->global.total_frees);
    fprintf(out, "  Largest alloc: %zu bytes\n", manager->global.largest_alloc);
    fprintf(out, "  Average alloc: %zu bytes\n", manager->global.total_allocs ? manager->global.total_bytes_alloced / manager->global.total_allocs : 0);
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
    if (manager->allocated_list) {
        fprintf(stderr, "Unfreed Allocations:\n");
        int leak_count = 1;
        MemHeader *current = manager->allocated_list;
        while (current) {
            char *user_ptr = (char *)current + _align_up(sizeof(MemHeader), MM_ALIGN);
            fprintf(stderr, "%d) %zu Bytes ", leak_count, current->size);
            _print_hex_dump(user_ptr, current->size, stderr);
            current = current->next;
            leak_count++;
        }
    }

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

static void _coalesce_free_list(MemoryManager *manager) {
    DEBUG_PRINTF("COALESCE: START\n");
    fflush(stdout);

    // Count free chunks
    size_t count = 0;
    for (FreeChunk *c = manager->free_list; c; c = c->next) {
        count++;
    }
    if (count < 2) {
        DEBUG_PRINTF("COALESCE: nothing to coalesce (count=%zu)\n", count);
        fflush(stdout);
        return;
    }

    // Collect pointers into an array
    FreeChunk **arr = (FreeChunk **)malloc(count * sizeof(FreeChunk *));
    if (!arr) {
        DEBUG_PRINTF("COALESCE: malloc failed, skipping\n");
        fflush(stdout);
        return;
    }
    size_t i = 0;
    for (FreeChunk *c = manager->free_list; c; c = c->next) {
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
    manager->free_list = NULL;
    for (size_t k = 0; k < w; ++k) {
        arr[k]->next = manager->free_list;
        manager->free_list = arr[k];
    }

    free(arr);
    DEBUG_PRINTF("COALESCE: DONE, chunks=%zu -> %zu\n", count, w);
    fflush(stdout);
}

static void *_alloc_from_free_list(MemoryManager *manager, size_t total_size) {
    DEBUG_PRINTF("ALLOC_FREE_LIST: Looking for size=%zu\n", total_size);
    fflush(stdout);
    _debug_print_free_list(manager, "before_alloc");

    FreeChunk **pp = &manager->free_list;
    while (*pp) {
        FreeChunk *chunk = *pp;
        DEBUG_PRINTF("ALLOC_FREE_LIST: Checking chunk=%p, size=%zu\n",
                     (void *)chunk, chunk->size);
        fflush(stdout);

        if (chunk->size >= total_size) {
            // Remove chosen chunk from list
            *pp = chunk->next;

            // Optional split to reduce internal fragmentation
            size_t remain = chunk->size - total_size;
            if (remain >= sizeof(FreeChunk) + 16) {
                // Place the tail back on the free list
                FreeChunk *tail = (FreeChunk *)((char *)chunk + total_size);
                tail->size = remain;
                tail->next = manager->free_list;
                manager->free_list = tail;

                DEBUG_PRINTF("ALLOC_FREE_LIST: Splitting chunk=%p -> "
                             "alloc=%zu, tail=%p tail_size=%zu\n",
                             (void *)chunk, total_size, (void *)tail, remain);
                fflush(stdout);

                // Mark the allocated portion size for clarity (not strictly required)
                chunk->size = total_size;
            } else {
                DEBUG_PRINTF("ALLOC_FREE_LIST: Using whole chunk=%p (no split)\n",
                             (void *)chunk);
                fflush(stdout);
            }

            _debug_print_free_list(manager, "after_alloc");
            DEBUG_PRINTF("ALLOC_FREE_LIST: Returning %p\n", (void *)chunk);
            fflush(stdout);
            return (void *)chunk;
        }

        pp = &(*pp)->next;
    }

    DEBUG_PRINTF("ALLOC_FREE_LIST: No suitable chunk found, returning NULL\n");
    fflush(stdout);
    return NULL;
}

static Block *_add_block(MemoryManager *manager, size_t min_size) {
    DEBUG_PRINTF("ADD_BLOCK: min_size=%zu\n", min_size);
    fflush(stdout);

    size_t block_size = (min_size > MM_BLOCK_SIZE) ? _align_up(min_size, MM_ALIGN) : MM_BLOCK_SIZE;
    Block *block = (Block *)malloc(sizeof(Block) + block_size);
    if (!block) {
        _abort_with_report(manager, "Out of memory (system malloc failed)");
    }

    DEBUG_PRINTF("ADD_BLOCK: allocated block=%p, size=%zu\n", (void*)block, block_size);
    fflush(stdout);

    block->next = manager->blocks;
    block->size = block_size;
    block->used = 0;
    manager->blocks = block;

    DEBUG_PRINTF("ADD_BLOCK: added to chain, returning %p\n", (void*)block);
    fflush(stdout);
    return block;
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
        _add_block(g_memory_manager, MM_BLOCK_SIZE);
    }
    return g_memory_manager;
}

static void *_mm_malloc(size_t size, MemTag tag) {
    DEBUG_PRINTF("MALLOC: size=%zu, tag=%d\n", size, tag);
    fflush(stdout);

    MemoryManager *manager = _get_manager();
    size_t total = _align_up(sizeof(MemHeader), MM_ALIGN) + size + sizeof(MemFooter);

    DEBUG_PRINTF("MALLOC: total_with_headers=%zu\n", total);
    fflush(stdout);

    void *mem = _alloc_from_free_list(manager, total);
    if (!mem) {
        _coalesce_free_list(manager);
        mem = _alloc_from_free_list(manager, total);
    }
    if (!mem) {
        DEBUG_PRINTF("MALLOC: Using block allocator\n");
        fflush(stdout);

        Block *block = manager->blocks;
        if (!block || block->size - block->used < total) {
            block = _add_block(manager, total);
        }
        if (block->size - block->used < total) {
            _abort_with_report(manager, "Block too small after adding new block");
        }
        mem = block->data + block->used;
        block->used += total;

        DEBUG_PRINTF("MALLOC: allocated from block, mem=%p\n", mem);
        fflush(stdout);
    }

    MemHeader *header = (MemHeader *)mem;
    header->size = size;
    header->magic = MEM_MAGIC_HEADER;
    header->tag = tag;
    header->next = manager->allocated_list; // Add to head of allocated list
    manager->allocated_list = header;
    
    _set_footer(header);

    void *user_ptr = (void *)((char *)header + _align_up(sizeof(MemHeader), MM_ALIGN));

    DEBUG_PRINTF("MALLOC: header=%p, user_ptr=%p, size=%zu\n", (void*)header, user_ptr, size);
    fflush(stdout);

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

    return user_ptr;
}

static void *_mm_calloc(size_t count, size_t size, MemTag tag) {
    if (size != 0 && count > SIZE_MAX / size) {
        _abort_with_report(_get_manager(), "calloc size overflow");
    }
    size_t total = count * size;
    void *ptr = _mm_malloc(total, tag);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

static void _mm_free(void *ptr) {
    if (!ptr) return;

    DEBUG_PRINTF("FREE: freeing ptr=%p\n", ptr);
    fflush(stdout);

    MemoryManager *manager = _get_manager();
    MemHeader *header = (MemHeader *)((char *)ptr - _align_up(sizeof(MemHeader), MM_ALIGN));

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

    DEBUG_PRINTF("FREE: adding to free list\n");
    fflush(stdout);
    _debug_print_free_list(manager, "before_add_to_free_list");

    FreeChunk *chunk = (FreeChunk *)header;
    chunk->size = _align_up(sizeof(MemHeader), MM_ALIGN) + size + sizeof(MemFooter);
    chunk->next = manager->free_list;
    manager->free_list = chunk;

    DEBUG_PRINTF("FREE: added chunk=%p (size=%zu) to free list\n", (void*)chunk, chunk->size);
    fflush(stdout);
    _debug_print_free_list(manager, "after_add_to_free_list");

    header->magic = 0;
    MemFooter *footer = (MemFooter *)((char *)(header + 1) + size);
    footer->magic = 0;

    DEBUG_PRINTF("FREE: completed for ptr=%p\n", ptr);
    fflush(stdout);
}

static void *_mm_realloc(void *ptr, size_t size, MemTag tag) {
    if (!ptr) return _mm_malloc(size, tag);

    MemoryManager *manager = _get_manager();
    MemHeader *old_header = (MemHeader *)((char *)ptr - _align_up(sizeof(MemHeader), MM_ALIGN));
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
    
    _memory_manager_report(g_memory_manager, stdout);
    
    if (g_memory_manager->allocated_list) {
        fprintf(stdout, "\n=== Memory Leak Report ===\n");
        int leak_count = 1;
        MemHeader *current = g_memory_manager->allocated_list;
        while (current) {
            char *user_ptr = (char *)current + _align_up(sizeof(MemHeader), MM_ALIGN);
            fprintf(stdout, "%d) %zu Bytes ", leak_count, current->size);
            _print_hex_dump(user_ptr, current->size, stdout);
            current = current->next;
            leak_count++;
        }
    }
    
    Block *block = g_memory_manager->blocks;
    while (block) {
        Block *next = block->next;
        free(block);
        block = next;
    }
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