/*
 * Project: Entity Sprite Engine
 *
 * Tagged, per-thread allocator with leak and double-free tracking. 16-byte
 * aligned allocations, backtraces, and per-tag/global stats. Reports leaks with
 * sample backtraces; aborts on fatal misuse. Thread-safe resize and teardown;
 * cross-thread frees are rejected.
 *
 * Details:
 * Each thread lazily owns a MemoryManagerThread with hash tables for live and
 * freed allocations. Entries store pointer, size, tag, and optional backtrace.
 * A global array of per-thread managers grows under a mutex to stay
 * thread-safe.
 *
 * Allocations use aligned_alloc(16) and are tracked immediately. calloc checks
 * for overflow. realloc allocates a new block, copies min(old,new) bytes, then
 * frees the old block through the same tracking path to keep stats consistent.
 *
 * Frees remove entries from the live table. With MEMORY_TRACK_FREE enabled, a
 * freed-table captures the first-free backtrace to detect double-free. Any free
 * of an unknown pointer, or a double-free, triggers a detailed report and
 * abort.
 *
 * Reports print per-thread totals, per-tag stats, and a sample of outstanding
 * leaks with backtraces when enabled. destroy() can clean the current thread or
 * all threads; internal teardown is serialized to avoid concurrent mutation.
 *
 * Configuration is via MEMORY_TRACKING, and MEMORY_TRACK_FREE. Backtraces use
 * execinfo on supported platforms. All allocations must be freed on the
 * allocating thread by design.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#include "core/memory_manager.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "utility/thread.h"
#include <execinfo.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ========================================
// Defines and Structs
// ========================================

#define MEMORY_TRACKING 1
#define MEMORY_TRACK_FREE 0

#define ALLOC_TABLE_SIZE 65536
#define ALLOC_TABLE_MASK (ALLOC_TABLE_SIZE - 1)
#define INITIAL_MM_THREADS_CAPACITY 4

typedef struct AllocEntry {
  void *ptr;
  size_t size;
  MemTag tag;
  struct AllocEntry *next;
#if MEMORY_TRACKING == 1
  void *bt[16];
  int bt_size;
#endif
} AllocEntry;

typedef struct {
  size_t current_usage;
  size_t max_usage;
  size_t total_allocs;
  size_t total_frees;
  size_t total_bytes_alloced;
  size_t largest_alloc;
} MemStats;

typedef struct MemoryManagerThread {
  AllocEntry *alloc_table[ALLOC_TABLE_SIZE];
#if MEMORY_TRACKING == 1 && MEMORY_TRACK_FREE == 1
  AllocEntry *freed_table[ALLOC_TABLE_SIZE];
#endif
  MemStats global;
  MemStats tags[MMTAG_COUNT];
  int32_t thread_number;
} MemoryManagerThread;

typedef struct MemoryManager {
  MemoryManagerThread **threads;
  size_t capacity;
} MemoryManager;

static MemoryManager *g_memory_manager = NULL;
static EseMutex *g_mm_mutex = NULL;
static const char *mem_tag_names[MMTAG_COUNT] = {
    "GENERAL         ", "ENGINE          ", "GUI             ",
    "GUI_STYLE       ", "ASSET           ", "ENTITY          ",
    "COMP_LUA        ", "COMP_MAP        ", "LUA             ",
    "LUA_VALUE       ", "LUA_SCRIPT      ", "RENDERER        ",
    "SPRITE          ", "DRAWLIST        ", "RENDERLIST      ",
    "SHADER          ", "WINDOW          ", "ARRAY           ",
    "HASHMAP         ", "GROUP_HASHMAP   ", "LINKED_LIST     ",
    "LINKED_LIST_ITER", "CONSOLE         ", "ARC             ",
    "CAMERA          ", "DISPLAY         ", "INPUT_STATE     ",
    "MAP_CELL        ", "MAP             ", "POINT           ",
    "RAY             ", "RECT            ", "TILESET         ",
    "UUID            ", "VECTOR          ", "AUDIO           ",
    "COLLISION_INDEX ", "COLOR           ", "POLY_LINE       ",
    "PUB_SUB         ", "THREAD          ", "HTTP            ",
    "REN_SYS_SHAPE   ", "SYS_SPRITE      ", "REN_SYS_SPRITE  ",
    "TEMP            ",
};

// ========================================
// PRIVATE FUNCTIONS
// ========================================

/**
 * @brief Get or create the memory manager thread for the current thread.
 *
 * @return MemoryManagerThread* Pointer to the thread's memory manager
 */
static MemoryManagerThread *_get_thread_manager(void) {
  int tid = ese_thread_get_number();
  if (!g_memory_manager) {
    g_memory_manager = (MemoryManager *)calloc(1, sizeof(MemoryManager));
    if (!g_memory_manager) {
      fprintf(stderr, "FATAL: Out of memory (manager struct)\n");
      abort();
    }
    g_memory_manager->capacity = INITIAL_MM_THREADS_CAPACITY;
    g_memory_manager->threads = (MemoryManagerThread **)calloc(
        g_memory_manager->capacity, sizeof(MemoryManagerThread *));
    if (!g_memory_manager->threads) {
      fprintf(stderr, "FATAL: Out of memory (threads table)\n");
      abort();
    }
    g_mm_mutex = ese_mutex_create();
    if (!g_mm_mutex) {
      fprintf(stderr, "FATAL: Out of memory (resize mutex)\n");
      abort();
    }
  }

  ese_mutex_lock(g_mm_mutex);
  if ((size_t)tid >= g_memory_manager->capacity) {
    size_t new_cap = g_memory_manager->capacity;
    while ((size_t)tid >= new_cap) {
      new_cap = (new_cap == 0) ? INITIAL_MM_THREADS_CAPACITY
                               : (size_t)(new_cap * 3 / 2);
    }
    MemoryManagerThread **new_arr = (MemoryManagerThread **)realloc(
        g_memory_manager->threads, new_cap * sizeof(MemoryManagerThread *));
    if (!new_arr) {
      ese_mutex_unlock(g_mm_mutex);
      fprintf(stderr, "FATAL: Out of memory (resize threads)\n");
      abort();
    }
    for (size_t i = g_memory_manager->capacity; i < new_cap; ++i) {
      new_arr[i] = NULL;
    }
    g_memory_manager->threads = new_arr;
    g_memory_manager->capacity = new_cap;
  }

  MemoryManagerThread *mt = g_memory_manager->threads[tid];
  if (!mt) {
    mt = (MemoryManagerThread *)calloc(1, sizeof(MemoryManagerThread));
    if (!mt) {
      fprintf(stderr, "FATAL: Out of memory (thread manager)\n");
      abort();
    }
    mt->thread_number = tid;
    g_memory_manager->threads[tid] = mt;
    for (size_t i = 0; i < ALLOC_TABLE_SIZE; i++) {
      mt->alloc_table[i] = NULL;
#if MEMORY_TRACKING == 1 && MEMORY_TRACK_FREE == 1
      mt->freed_table[i] = NULL;
#endif
    }
  }
  ese_mutex_unlock(g_mm_mutex);

  return mt;
}

/**
 * @brief Align size up to the specified alignment boundary.
 *
 * @param n The size to align
 * @param align The alignment boundary
 * @return size_t Aligned size
 */
static inline size_t _align_up(size_t n, size_t align) {
  return (n + align - 1) & ~(align - 1);
}

/**
 * @brief Hash a pointer to get a table index.
 *
 * @param ptr The pointer to hash
 * @return size_t Hash index
 */
static inline size_t _hash_ptr(void *ptr) {
  uintptr_t addr = (uintptr_t)ptr;
  return (addr >> 4) & ALLOC_TABLE_MASK;
}

/**
 * @brief Track a new allocation in the thread's allocation table.
 *
 * @param thread The thread's memory manager
 * @param ptr The allocated pointer
 * @param size The allocation size
 * @param tag The memory tag
 */
static void _track_alloc(MemoryManagerThread *thread, void *ptr, size_t size,
                         MemTag tag) {
  size_t hash = _hash_ptr(ptr);

  AllocEntry *entry = (AllocEntry *)malloc(sizeof(AllocEntry));
  if (!entry)
    return;

  entry->ptr = ptr;
  entry->size = size;
  entry->tag = tag;
  entry->next = thread->alloc_table[hash];

#if MEMORY_TRACKING == 1
  entry->bt_size = 0;
  entry->bt_size =
      backtrace(entry->bt, (int)(sizeof(entry->bt) / sizeof(entry->bt[0])));
#endif

  thread->alloc_table[hash] = entry;

  log_verbose("MEMORY_MANAGER", "TRACK_ALLOC: tracking %p -> %zu bytes", ptr,
              size);
}

/**
 * @brief Find and remove an allocation entry from the thread's table.
 *
 * @param thread The thread's memory manager
 * @param ptr The pointer to find
 * @return AllocEntry* The removed entry, or NULL if not found
 */
static AllocEntry *_find_and_remove_alloc(MemoryManagerThread *thread,
                                          void *ptr) {
  if (!thread || !ptr) {
    log_error("MEMORY_MANAGER", "Invalid parameters: thread=%p, ptr=%p", thread,
              ptr);
    return NULL;
  }

  size_t hash = _hash_ptr(ptr);
  if (hash >= ALLOC_TABLE_SIZE) {
    log_error("MEMORY_MANAGER", "Hash out of bounds: %zu", hash);
    return NULL;
  }

  AllocEntry **pp = &thread->alloc_table[hash];
  while (*pp) {
    AllocEntry *entry = *pp;
    if (entry->ptr == ptr) {
      *pp = entry->next;
      log_verbose("MEMORY_MANAGER", "TRACK_FREE: found %p -> %zu bytes", ptr,
                  entry->size);
      return entry;
    }
    pp = &entry->next;
  }

  log_error("MEMORY_MANAGER", "TRACK_FREE: %p not found in allocation table",
            ptr);
  return NULL;
}

#if MEMORY_TRACKING == 1 && MEMORY_TRACK_FREE == 1
/**
 * @brief Find an entry in the freed table.
 *
 * @param thread The thread's memory manager
 * @param ptr The pointer to find
 * @return AllocEntry* The freed entry, or NULL if not found
 */
static AllocEntry *_find_in_freed_table(MemoryManagerThread *thread,
                                        void *ptr) {
  size_t hash = _hash_ptr(ptr);
  AllocEntry *entry = thread->freed_table[hash];
  while (entry) {
    if (entry->ptr == ptr)
      return entry;
    entry = entry->next;
  }
  return NULL;
}

/**
 * @brief Remove an entry from the freed table.
 *
 * @param thread The thread's memory manager
 * @param ptr The pointer to remove
 */
static void _remove_from_freed_table(MemoryManagerThread *thread, void *ptr) {
  if (!thread || !ptr)
    return;
  size_t hash = _hash_ptr(ptr);
  AllocEntry **pp = &thread->freed_table[hash];
  while (*pp) {
    AllocEntry *e = *pp;
    if (e->ptr == ptr) {
      *pp = e->next;
      free(e);
      return;
    }
    pp = &e->next;
  }
}

/**
 * @brief Add an entry to the freed table.
 *
 * @param thread The thread's memory manager
 * @param entry The allocation entry to add
 */
static void _add_to_freed_table(MemoryManagerThread *thread,
                                AllocEntry *entry) {
  size_t hash = _hash_ptr(entry->ptr);
  AllocEntry *freed_entry = (AllocEntry *)malloc(sizeof(AllocEntry));
  if (!freed_entry)
    return;

  freed_entry->ptr = entry->ptr;
  freed_entry->size = entry->size;
  freed_entry->tag = entry->tag;
  freed_entry->next = thread->freed_table[hash];
#if MEMORY_TRACKING == 1
  freed_entry->bt_size = entry->bt_size;
  for (int i = 0; i < entry->bt_size && i < 16; i++) {
    freed_entry->bt[i] = entry->bt[i];
  }
#endif
  thread->freed_table[hash] = freed_entry;
}

/**
 * @brief Print a backtrace from an allocation entry.
 *
 * @param entry The allocation entry containing the backtrace
 */
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

/**
 * @brief Abort with a detailed memory manager report.
 *
 * @param thread The thread's memory manager
 * @param msg The abort message
 */
static void _abort_with_report(MemoryManagerThread *thread, const char *msg) {
  if (thread) {
    fprintf(stderr, "\n=== Memory Manager Thread %d State ===\n",
            thread->thread_number);
    fprintf(stderr, "Current usage: %zu bytes\n", thread->global.current_usage);
    fprintf(stderr, "Max usage:     %zu bytes\n", thread->global.max_usage);
    fprintf(stderr, "Total allocs:  %zu\n", thread->global.total_allocs);
    fprintf(stderr, "Total frees:   %zu\n", thread->global.total_frees);
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
  abort();
}

/**
 * @brief Generate and print a memory usage report.
 *
 * @param all_threads If true, report for all threads; otherwise just current
 * thread
 */
static void _mm_report(bool all_threads) {
  MemoryManager *mm = g_memory_manager;
  if (!mm) {
    printf("=== Memory Usage Report ===\n");
    printf("No memory manager initialized yet.\n");
    return;
  }

  // Lock the memory manager
  if (g_mm_mutex)
    ese_mutex_lock(g_mm_mutex);

  if (all_threads) {
    printf("=== Memory Usage Report (All Threads) ===\n");
    for (size_t i = 0; i < mm->capacity; ++i) {
      MemoryManagerThread *t = mm->threads[i];
      if (!t)
        continue;
      printf("Thread %d: current=%zu, max=%zu, allocs=%zu, frees=%zu, "
             "largest=%zu, avg=%zu\n",
             t->thread_number, t->global.current_usage, t->global.max_usage,
             t->global.total_allocs, t->global.total_frees,
             t->global.largest_alloc,
             t->global.total_allocs
                 ? t->global.total_bytes_alloced / t->global.total_allocs
                 : 0);
    }
  } else {
    MemoryManagerThread *thread = _get_thread_manager();
    printf("=== Memory Usage Report (Current Thread %d) ===\n",
           thread->thread_number);
    printf("Current usage: %zu bytes\n", thread->global.current_usage);
    printf("Max usage:     %zu bytes\n", thread->global.max_usage);
    printf("Total allocs:  %zu\n", thread->global.total_allocs);
    printf("Total frees:   %zu\n", thread->global.total_frees);
    printf("Largest alloc: %zu bytes\n", thread->global.largest_alloc);
    printf("Total allocated: %zu bytes\n", thread->global.total_bytes_alloced);

    size_t leak_count = 0;
    size_t leak_bytes = 0;
    for (size_t i = 0; i < ALLOC_TABLE_SIZE; i++) {
      AllocEntry *entry = thread->alloc_table[i];
      while (entry) {
        leak_count++;
        leak_bytes += entry->size;
        entry = entry->next;
      }
    }

    if (leak_count > 0) {
      printf("  WARNING: %zu memory leaks detected (%zu bytes leaked)!\n",
             leak_count, leak_bytes);
      printf("  Sample leaks:\n");
      size_t shown = 0;
#if MEMORY_TRACKING != 1
      for (size_t i = 0; i < ALLOC_TABLE_SIZE && shown < 10; i++) {
        AllocEntry *entry = thread->alloc_table[i];
        while (entry && shown < 10) {
          const char *tagname = (entry->tag >= 0 && entry->tag < MMTAG_COUNT)
                                    ? mem_tag_names[entry->tag]
                                    : "UNKNOWN";
          printf("    %p: %zu bytes (%s)\n", entry->ptr, entry->size, tagname);
          entry = entry->next;
          shown++;
        }
      }
#else
      for (size_t i = 0; i < ALLOC_TABLE_SIZE && shown < 10; i++) {
        AllocEntry *entry = thread->alloc_table[i];
        while (entry && shown < 10) {
          const char *tagname = (entry->tag >= 0 && entry->tag < MMTAG_COUNT)
                                    ? mem_tag_names[entry->tag]
                                    : "UNKNOWN";
          printf("    %p: %zu bytes (%s)\n", entry->ptr, entry->size, tagname);
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
      const MemStats *s = &thread->tags[i];
      if (s->total_allocs > 0) {
        printf("  [%s] current=%zu, max=%zu, allocs=%zu, frees=%zu, "
               "largest=%zu, avg=%zu\n",
               mem_tag_names[i], s->current_usage, s->max_usage,
               s->total_allocs, s->total_frees, s->largest_alloc,
               s->total_allocs ? s->total_bytes_alloced / s->total_allocs : 0);
      }
    }
  }

  // Unlock the memory manager
  if (g_mm_mutex)
    ese_mutex_unlock(g_mm_mutex);
}

/**
 * @brief Allocate memory with tracking.
 *
 * @param size The size to allocate
 * @param tag The memory tag
 * @return void* The allocated pointer
 */
static void *_mm_malloc(size_t size, MemTag tag) {
  MemoryManagerThread *thread = _get_thread_manager();

  size_t aligned_size = _align_up(size, 16);
  void *ptr = aligned_alloc(16, aligned_size);
  if (!ptr) {
    log_error("MEMORY_MANAGER", "MALLOC: aligned_alloc failed for size %zu",
              aligned_size);
    fflush(stdout);
    _abort_with_report(thread, "Failed to allocate memory");
    return NULL;
  }

#if MEMORY_TRACKING == 1 && MEMORY_TRACK_FREE == 1
  _remove_from_freed_table(thread, ptr);
#endif

  _track_alloc(thread, ptr, size, tag);

  thread->global.current_usage += size;
  thread->global.total_bytes_alloced += size;
  if (thread->global.current_usage > thread->global.max_usage)
    thread->global.max_usage = thread->global.current_usage;
  if (size > thread->global.largest_alloc)
    thread->global.largest_alloc = size;
  thread->global.total_allocs++;

  if (tag >= 0 && tag < MMTAG_COUNT) {
    MemStats *s = &thread->tags[tag];
    s->current_usage += size;
    s->total_bytes_alloced += size;
    if (s->current_usage > s->max_usage)
      s->max_usage = s->current_usage;
    if (size > s->largest_alloc)
      s->largest_alloc = size;
    s->total_allocs++;
  }

  log_verbose("MEMORY_MANAGER",
              "MALLOC: returning ptr=%p (16-byte aligned), size=%zu, tag=%d",
              ptr, size, tag);
  return ptr;
}

/**
 * @brief Allocate and zero-initialize memory with tracking.
 *
 * @param count Number of elements
 * @param size Size of each element
 * @param tag The memory tag
 * @return void* The allocated and zeroed pointer
 */
static void *_mm_calloc(size_t count, size_t size, MemTag tag) {
  if (size != 0 && count > SIZE_MAX / size) {
    MemoryManagerThread *thread = _get_thread_manager();
    _abort_with_report(thread, "Invalid calloc parameters");
    return NULL;
  }
  size_t total_size = count * size;
  void *ptr = _mm_malloc(total_size, tag);
  if (ptr) {
    memset(ptr, 0, total_size);
  }
  return ptr;
}

/**
 * @brief Free allocated memory with tracking.
 *
 * @param ptr The pointer to free
 */
static void _mm_free(void *ptr) {
  if (!ptr)
    return;

  log_verbose("MEMORY_MANAGER", "FREE: freeing ptr=%p", ptr);

  MemoryManagerThread *thread = _get_thread_manager();
  AllocEntry *entry = NULL;

#if MEMORY_TRACKING == 1 && MEMORY_TRACK_FREE == 1
  AllocEntry *freed_entry = _find_in_freed_table(thread, ptr);
  if (freed_entry) {
    AllocEntry *current_entry = _find_and_remove_alloc(thread, ptr);
    if (!current_entry) {
      const char *tagname =
          (freed_entry->tag >= 0 && freed_entry->tag < MMTAG_COUNT)
              ? mem_tag_names[freed_entry->tag]
              : "UNKNOWN";
      fprintf(stderr, "\n=== DOUBLE-FREE DETECTED ===\n");
      fprintf(stderr, "Pointer: %p\n", ptr);
      fprintf(stderr, "Previously freed size: %zu bytes\n", freed_entry->size);
      fprintf(stderr, "Previously freed tag: %s\n", tagname);
      fprintf(stderr, "This pointer was already freed previously. "
                      "Backtrace of first free:\n");
      _print_backtrace(freed_entry);
      _abort_with_report(thread, "Double-free detected");
    } else {
      entry = current_entry;
    }
  }
#endif

  if (!entry) {
    entry = _find_and_remove_alloc(thread, ptr);
  }

  if (entry) {
    size_t size = entry->size;
    MemTag tag = entry->tag;

    thread->global.current_usage -= size;
    thread->global.total_frees++;

    if (tag >= 0 && tag < MMTAG_COUNT) {
      MemStats *s = &thread->tags[tag];
      s->current_usage -= size;
#if MEMORY_TRACK_FREE == 1
      if (s->total_frees + 1 > s->total_allocs) {
        log_error("MEMORY_MANAGER", "%s UNDERFLOW: allocs=%zu frees=%zu",
                  mem_tag_names[tag], s->total_allocs, s->total_frees);
      }
#else
      log_assert("MEMORY_MANAGER", s->total_frees + 1 <= s->total_allocs,
                 "%s UNDERFLOW: allocs=%zu frees=%zu", mem_tag_names[tag],
                 s->total_allocs, s->total_frees);
#endif
      s->total_frees++;
    }

#if MEMORY_TRACKING == 1 && MEMORY_TRACK_FREE == 1
    _add_to_freed_table(thread, entry);
#endif
    free(entry);
  } else {
    fprintf(stderr, "\n=== FREE OF UNTRACKED POINTER ===\n");
    fprintf(stderr, "Pointer: %p\n", ptr);
    _abort_with_report(thread, "Free of untracked pointer");
  }

  free(ptr);
  log_verbose("MEMORY_MANAGER", "FREE: completed");
}

/**
 * @brief Reallocate memory with tracking.
 *
 * @param ptr The existing pointer
 * @param size The new size
 * @param tag The memory tag
 * @return void* The reallocated pointer
 */
static void *_mm_realloc(void *ptr, size_t size, MemTag tag) {
  if (!ptr)
    return _mm_malloc(size, tag);

  MemoryManagerThread *thread = _get_thread_manager();

  size_t hash = _hash_ptr(ptr);
  AllocEntry *entry = thread->alloc_table[hash];
  size_t old_size = 0;

  while (entry) {
    if (entry->ptr == ptr) {
      old_size = entry->size;
      break;
    }
    entry = entry->next;
  }

  void *new_ptr = _mm_malloc(size, tag);
  size_t copy_size = (old_size > 0 && old_size < size) ? old_size : size;
  if (old_size > 0) {
    memcpy(new_ptr, ptr, copy_size);
  }

  _mm_free(ptr);
  return new_ptr;
}

/**
 * @brief Duplicate a string with tracking.
 *
 * @param str The string to duplicate
 * @param tag The memory tag
 * @return char* The duplicated string
 */
static char *_mm_strdup(const char *str, MemTag tag) {
  if (!str)
    return NULL;
  size_t len = strlen(str) + 1;
  char *copy = (char *)_mm_malloc(len, tag);
  memcpy(copy, str, len);
  return copy;
}

/**
 * @brief Destroy the memory manager and clean up resources.
 *
 * @param all_threads If true, destroy all threads; otherwise just current
 * thread
 */
static void _mm_destroy(bool all_threads) {
  /* If manager never created, nothing to do. */
  if (!g_memory_manager)
    return;

  /* Serialize all destroy operations to avoid concurrent frees. */
  if (g_mm_mutex)
    ese_mutex_lock(g_mm_mutex);

  /* Re-check under lock in case another thread freed it. */
  if (!g_memory_manager) {
    if (g_mm_mutex)
      ese_mutex_unlock(g_mm_mutex);
    return;
  }

  if (all_threads) {
    log_verbose("MEMORY_MANAGER", "Destroyed all threads' memory managers");
    for (size_t i = 0; i < g_memory_manager->capacity; ++i) {
      MemoryManagerThread *t = g_memory_manager->threads[i];
      if (!t)
        continue;
      for (size_t j = 0; j < ALLOC_TABLE_SIZE; j++) {
        AllocEntry *e = t->alloc_table[j];
        while (e) {
          AllocEntry *n = e->next;
          free(e);
          e = n;
        }
#if MEMORY_TRACK_FREE == 1
        AllocEntry *fe = t->freed_table[j];
        while (fe) {
          AllocEntry *nf = fe->next;
          free(fe);
          fe = nf;
        }
#endif
      }
      free(t);
      g_memory_manager->threads[i] = NULL;
    }
    free(g_memory_manager->threads);
    g_memory_manager->threads = NULL;
    g_memory_manager->capacity = 0;
  } else {
    int tid = ese_thread_get_number();
    if ((size_t)tid < g_memory_manager->capacity) {
      MemoryManagerThread *t = g_memory_manager->threads[tid];
      if (t) {
        log_verbose("MEMORY_MANAGER", "Destroyed thread %d's memory manager",
                    t->thread_number);
        for (size_t j = 0; j < ALLOC_TABLE_SIZE; j++) {
          AllocEntry *e = t->alloc_table[j];
          while (e) {
            AllocEntry *n = e->next;
            free(e);
            e = n;
          }
#if MEMORY_TRACK_FREE == 1
          AllocEntry *fe = t->freed_table[j];
          while (fe) {
            AllocEntry *nf = fe->next;
            free(fe);
            fe = nf;
          }
#endif
        }
        free(t);
        g_memory_manager->threads[tid] = NULL;
      }
    }
  }

  /* If nothing left, free the container. */
  bool any_left = false;
  if (g_memory_manager) {
    for (size_t i = 0; i < g_memory_manager->capacity; ++i) {
      if (g_memory_manager->threads && g_memory_manager->threads[i]) {
        any_left = true;
        break;
      }
    }
    if (!any_left) {
      free(g_memory_manager->threads);
      g_memory_manager->threads = NULL;
      free(g_memory_manager);
      g_memory_manager = NULL;
      /* Intentionally leave g_mm_mutex alive; it may be used by
         late logging or other shutdown paths. */
    }
  }

  if (g_mm_mutex)
    ese_mutex_unlock(g_mm_mutex);
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

const struct memory_manager_api memory_manager = {.malloc = _mm_malloc,
                                                  .calloc = _mm_calloc,
                                                  .realloc = _mm_realloc,
                                                  .free = _mm_free,
                                                  .strdup = _mm_strdup,
                                                  .report = _mm_report,
                                                  .destroy = _mm_destroy};
