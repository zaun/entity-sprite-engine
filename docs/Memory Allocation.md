# Memory Allocation

This document explains how memory allocation works in ESE, focusing on:

- The core tagged memory manager (`memory_manager`)
- The shared (cross-thread) allocator API
- The Lua engine’s custom allocator (`_lua_engine_limited_alloc`) and how it integrates with the memory manager

The goal is to make it clear **how** allocations are tracked and constrained, and **why** the design looks the way it does.

---

## Design goals

The memory manager is designed to:

- Provide **subsystem-level tagging** for allocations (renderer, Lua, entities, etc.)
- Maintain **per-thread** allocation tracking and statistics
- Offer **optional backtraces** for allocations to help track leaks
- Detect **common misuse** (freeing unknown pointers, double frees when enabled)
- Enforce **per-thread isolation** by default; cross-thread frees are treated as fatal misuse
- Provide a **separate shared allocator** for truly cross-thread ownership
- Support **16-byte alignment** globally

The Lua engine builds on this to:

- Enforce a **hard per-engine memory limit** (default 10MB)
- Track **Lua-visible heap usage** (`memory_used`)
- Detect **corruption** in Lua allocations via header/tail canaries
- Plug into Lua’s `lua_Alloc` interface without leaking implementation details

---

## Core memory manager (`src/core/memory_manager.*`)

### Tags (`MemTag`)

All engine allocations that go through the memory manager are categorized by `MemTag`:

```c
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
    MMTAG_RS_SHAPE,
    MMTAG_S_SPRITE,
    MMTAG_RS_SPRITE,
    MMTAG_RS_TEXT,
    MMTAG_RS_COLLIDER,
    MMTAG_RS_MAP,
    MMTAG_TEMP,
    MMTAG_COUNT
} MemTag;
```

These tags index **per-tag statistics** inside each thread’s memory manager (bytes in use, largest allocation, etc.), and are also used when printing reports.

### API surface

Externally, the memory manager is exposed via a single global struct:

```c
struct memory_manager_shared_api {
    void *(*malloc)(size_t size, MemTag tag);
    void *(*calloc)(size_t count, size_t size, MemTag tag);
    void *(*realloc)(void *ptr, size_t size, MemTag tag);
    void (*free)(void *ptr);
    char *(*strdup)(const char *str, MemTag tag);
};

struct memory_manager_api {
    void *(*malloc)(size_t size, MemTag tag);
    void *(*calloc)(size_t count, size_t size, MemTag tag);
    void *(*realloc)(void *ptr, size_t size, MemTag tag);
    void (*free)(void *ptr);
    char *(*strdup)(const char *str, MemTag tag);
    void (*report)(bool all_threads);
    void (*destroy)(bool all_threads);

    const struct memory_manager_shared_api shared;
};

extern const struct memory_manager_api memory_manager;
```

Usage pattern throughout the engine:

- Per-thread allocations: `memory_manager.malloc(size, MMTAG_*)`
- Shared (cross-thread) allocations: `memory_manager.shared.malloc(size, MMTAG_*)`
- Cleanup/reporting: `memory_manager.destroy(all_threads)`, `memory_manager.report(all_threads)`

### Internal structure

At runtime there are two levels of state:

1. **Global manager (`MemoryManager`)**

   ```c
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
   ```

   - Lazily initialized on first allocation (`_get_thread_manager`).
   - Holds a dynamically-growing array of `MemoryManagerThread*` indexed by thread number.
   - Protected by `g_mm_mutex` when resizing/creating `MemoryManagerThread` instances or when reporting/destroying.

2. **Per-thread manager (`MemoryManagerThread`)**

   - `alloc_table[65536]`: hash table keyed by pointer address (with chaining via `AllocEntry->next`).
   - Optional `freed_table` (only when `MEMORY_TRACK_FREE == 1`) to catch double-frees.
   - `MemStats global;` with:
     - `current_usage`, `max_usage`, `total_allocs`, `total_frees`
     - `total_bytes_alloced`, `largest_alloc`
   - `MemStats tags[MMTAG_COUNT];` with the same counters, but per tag.

Each **allocation** tracked by a thread is represented by:

```c
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
```

When tracking is enabled (`MEMORY_TRACKING == 1`), a short backtrace is captured with `backtrace()` at allocation time.

### Per-thread allocator lifecycle

Every per-thread operation starts by obtaining the calling thread’s manager:

```c
static MemoryManagerThread *_get_thread_manager(void) {
    int tid = ese_thread_get_number();

    if (!g_memory_manager) {
        // alloc g_memory_manager, threads array, mutex
        ...
    }

    ese_mutex_lock(g_mm_mutex);
    // Grow threads array if tid >= capacity, then create thread entry as needed
    ...
    ese_mutex_unlock(g_mm_mutex);

    return mt;
}
```

Key points:

- **Lazy creation**: both `g_memory_manager` and each `MemoryManagerThread` are created on demand.
- **Thread identity**: thread IDs come from `ese_thread_get_number()`.
- Each thread owns its own `MemoryManagerThread` and is expected to **free everything it allocates**.

#### Allocation (`_mm_malloc`)

```c
static void *_mm_malloc(MemoryManagerThread *thread, size_t size, MemTag tag) {
    size_t aligned_size = _align_up(size, 16);
    void *ptr = aligned_alloc(16, aligned_size);
    if (!ptr) {
        _abort_with_report(thread, "Failed to allocate memory");
    }

#if MEMORY_TRACKING == 1 && MEMORY_TRACK_FREE == 1
    _remove_from_freed_table(thread, ptr);
#endif

    _track_alloc(thread, ptr, size, tag);

    // Update global + per-tag stats
    ...
    return ptr;
}
```

Important details:

- **Alignment**: all allocations are rounded up to 16 bytes and use `aligned_alloc(16, aligned_size)`. Clients see the requested size, but the underlying block is 16-byte aligned and potentially slightly larger.
- **Tracking**:
  - `_track_alloc` inserts an `AllocEntry` into the `alloc_table` bucket computed by `_hash_ptr(ptr)`.
  - When `MEMORY_TRACKING == 1`, capturing a backtrace is part of tracking.
- **Stats**:
  - `global.current_usage` and `tags[tag].current_usage` are incremented by the requested size (not the padded size).
  - `max_usage` and `largest_alloc` are updated.

#### Free (`_mm_free`)

```c
static void _mm_free(MemoryManagerThread *thread, void *ptr) {
    if (!ptr) return;

    AllocEntry *entry = NULL;

#if MEMORY_TRACKING == 1 && MEMORY_TRACK_FREE == 1
    // double-free detection via freed_table
    ...
#endif

    if (!entry) {
        entry = _find_and_remove_alloc(thread, ptr);
    }

    if (entry) {
        size_t size = entry->size;
        MemTag tag = entry->tag;

        // Update global + tag stats (current_usage, total_frees, etc.)
        ...

#if MEMORY_TRACKING == 1 && MEMORY_TRACK_FREE == 1
        _add_to_freed_table(thread, entry);
#endif
        free(entry);
    } else {
        _abort_with_report(thread, "Free of untracked pointer");
    }

    free(ptr);
}
```

Behavior:

- `NULL` frees are allowed and become no-ops.
- The manager **must** find a corresponding `AllocEntry` in the *current thread’s* `alloc_table`. If not:
  - The process prints a detailed report and **aborts**.
  - This catches:
    - Cross-thread frees (freeing from a different thread than the allocating one)
    - Incorrect pointers (stack memory, foreign library pointers, etc.)
- If `MEMORY_TRACK_FREE == 1`:
  - Freed pointers are recorded in `freed_table` with their first-free backtrace.
  - A second free of the same pointer is treated as a **double-free** and aborts.

#### Realloc (`_mm_realloc`)

```c
static void *_mm_realloc(MemoryManagerThread *thread, void *ptr, size_t size, MemTag tag) {
    if (!ptr)
        return _mm_malloc(thread, size, tag);

    // Find the old entry
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

    void *new_ptr = _mm_malloc(thread, size, tag);
    size_t copy_size = (old_size > 0 && old_size < size) ? old_size : size;
    if (old_size > 0) {
        memcpy(new_ptr, ptr, copy_size);
    }

    _mm_free(thread, ptr);
    return new_ptr;
}
```

Notes:

- This is **not** in-place: `realloc` always performs a new allocation and then frees the old one via the same tracked path.
- Stats remain consistent and all the usual tracking / overflow detection continues to work.

#### String duplication (`_mm_strdup`)

A convenience wrapper around `_mm_malloc`, used heavily in string-heavy code paths:

```c
static char *_mm_strdup(MemoryManagerThread *thread, const char *str, MemTag tag) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char *copy = (char *)_mm_malloc(thread, len, tag);
    memcpy(copy, str, len);
    return copy;
}
```

### Shared (cross-thread) allocator

For objects that are created on one thread and legitimately freed on another, the memory manager exposes a **shared** allocator:

```c
static void _init_shared_manager(void) {
    if (!g_shared_thread) {
        g_shared_thread = (MemoryManagerThread *)calloc(1, sizeof(MemoryManagerThread));
        g_shared_thread->thread_number = -1;
    }
    if (!g_shared_mutex) {
        g_shared_mutex = ese_mutex_create();
    }
}

static void *_mm_malloc_shared(size_t size, MemTag tag) {
    _init_shared_manager();
    ese_mutex_lock(g_shared_mutex);
    void *ptr = _mm_malloc(g_shared_thread, size, tag);
    ese_mutex_unlock(g_shared_mutex);
    return ptr;
}

static void _mm_free_shared(void *ptr) {
    _init_shared_manager();
    ese_mutex_lock(g_shared_mutex);
    _mm_free(g_shared_thread, ptr);
    ese_mutex_unlock(g_shared_mutex);
}
```

Characteristics:

- All shared allocations are owned by a single `MemoryManagerThread` with `thread_number == -1`.
- All operations are serialized by `g_shared_mutex`.
- These allocations **can** be freed from any thread, because the tracking always goes through the same `g_shared_thread`.

Usage rule of thumb:

- If the ownership of a block **never leaves a single thread**, use the normal per-thread API (`memory_manager.*`).
- If ownership is **shared across threads** or passed to a different thread, allocate via `memory_manager.shared.*`.

### Reporting and teardown

#### Reporting

```c
static void _mm_report_wrapper(bool all_threads) {
    if (!all_threads) {
        int tid = ese_thread_get_number();
        MemoryManagerThread *thread = g_memory_manager->threads[tid];
        _mm_report(thread);
        return;
    }

    // Report all threads
    for (size_t i = 0; i < g_memory_manager->capacity; i++) {
        MemoryManagerThread *thread = g_memory_manager->threads[i];
        if (thread) _mm_report(thread);
    }

    // Report the shared memory manager
    _mm_report(g_shared_thread);
}
```

`_mm_report` prints:

- Thread name (main / shared / numbered thread)
- Global stats (current usage, max usage, total allocs/frees, etc.)
- Leak summary: total leaked blocks & bytes
- Up to 10 sample leaks, with per-tag info and (optionally) backtraces
- Per-tag stats for any tag that has had at least one allocation

This function is used:

- Explicitly in tests (`memory_manager.report(false)`).
- Implicitly when `_abort_with_report` handles fatal misuse.

#### Destroy

```c
static void _mm_destroy_wrapper(bool all_threads) {
    if (!all_threads) {
        int tid = ese_thread_get_number();
        MemoryManagerThread *thread = g_memory_manager->threads[tid];
        if (thread) {
            _mm_report(thread);
            ese_mutex_lock(g_mm_mutex);
            _mm_destroy(thread);
            g_memory_manager->threads[tid] = NULL;
            ese_mutex_unlock(g_mm_mutex);
        }
        return;
    }

    // Destroy all threads
    for (size_t i = 0; i < g_memory_manager->capacity; i++) {
        MemoryManagerThread *thread = g_memory_manager->threads[i];
        if (thread) {
            _mm_report(thread);
            ese_mutex_lock(g_mm_mutex);
            _mm_destroy(thread);
            g_memory_manager->threads[i] = NULL;
            ese_mutex_unlock(g_mm_mutex);
        }
    }

    // Destroy shared manager, then global manager
    ...
}
```

Notes:

- For `all_threads == false`, only the current thread’s manager is destroyed, after reporting.
- For `all_threads == true`:
  - All per-thread managers and the shared manager are destroyed.
  - Final reports are emitted for each.
- `_mm_destroy` **does not free outstanding user blocks**; it frees only internal `AllocEntry` structures. Leaks are reported, not auto-recovered.

### Misuse detection semantics

The manager treats certain operations as fatal:

- Free of an unknown pointer:
  - "FREE OF UNTRACKED POINTER", report, then abort.
- (When enabled) Double free:
  - Prints backtrace of the first free, then aborts.
- Allocation failure:
  - Logs, prints full report, and aborts.
- Certain invalid `calloc` parameters:
  - On overflow check failure, `_abort_with_report` aborts.

This is intentional: the memory manager is part of the engine’s **safety net** in debug/development builds.

---

## Lua engine allocator (`_lua_engine_limited_alloc`)

The Lua engine integrates with the memory manager via Lua’s `lua_Alloc` callback:

```c
EseLuaEngine *lua_engine_create() {
    EseLuaEngine *engine = memory_manager.malloc(sizeof(EseLuaEngine), MMTAG_LUA);
    engine->internal = memory_manager.malloc(sizeof(EseLuaEngineInternal), MMTAG_LUA);

    engine->internal->memory_limit = 1024 * 1024 * 10; // 10MB
    engine->internal->memory_used = 0;
    ...
    engine->runtime = lua_newstate(_lua_engine_limited_alloc, engine);
    ...
}
```

Key points:

- Each `EseLuaEngine` instance has its own `EseLuaEngineInternal`:
  - `size_t memory_limit` – max permitted Lua heap usage (default 10MB).
  - `size_t memory_used` – tracked usage of Lua-managed heap.
- Lua’s runtime is created with `_lua_engine_limited_alloc` as the allocator and the `EseLuaEngine*` as `ud` (user data).
- All Lua allocations are ultimately backed by `memory_manager` allocations tagged with `MMTAG_LUA`.

### Layout of Lua allocations

In `lua_engine_private.c`:

```c
static const uint64_t LUA_HDR_MAGIC = 0xD15EA5E5C0FFEE01ULL;
static const uint64_t LUA_TAIL_CANARY = 0xA11C0FFEEA11C0DEULL;

typedef struct LuaAllocHdr {
    size_t size;  /** User-visible size in bytes (requested by Lua) */
    uint64_t pad; /** 8-byte padding to make the header 16 bytes total */
} LuaAllocHdr;

_Static_assert(sizeof(LuaAllocHdr) == 16, "LuaAllocHdr must be 16 bytes for alignment");
```

Every Lua allocation looks like:

```text
[ LuaAllocHdr ][ Lua payload (size bytes) ][ uint64_t tail canary ]
        ^                                  ^
        |                                  |
     hdr pointer                      tail pointer
```

Helpers:

```c
static inline LuaAllocHdr *lua_hdr_from_user(void *user_ptr) {
    return (LuaAllocHdr *)((char *)user_ptr - sizeof(LuaAllocHdr));
}

static inline uint64_t *lua_tail_from_hdr(LuaAllocHdr *hdr) {
    return (uint64_t *)((char *)hdr + sizeof(LuaAllocHdr) + hdr->size);
}

static inline int lua_hdr_valid(LuaAllocHdr *hdr, size_t mem_limit) {
    if (!hdr) return 0;
    if (hdr->size > mem_limit) return 0;
    if (hdr->pad != LUA_HDR_MAGIC) return 0;
    uint64_t *tail = lua_tail_from_hdr(hdr);
    return (*tail == LUA_TAIL_CANARY);
}
```

These allow the allocator to:

- Validate that a pointer passed back by Lua points to a block that **we** created.
- Detect overwrites of:
  - The header (e.g., double frees, invalid pointers).
  - The tail-canary (e.g., buffer overruns past the end of the payload).

### Allocator implementation

Lua’s allocator contract:

- `ptr == NULL`, `osize` irrelevant, `nsize > 0`: allocate a new block.
- `ptr != NULL`, `nsize == 0`: free the block.
- `ptr != NULL`, `nsize > 0`: reallocate (grow/shrink) the block.

```c
void *_lua_engine_limited_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    EseLuaEngine *engine = (EseLuaEngine *)ud;
    EseLuaEngineInternal *internal = engine->internal;

    if (nsize == 0) {
        // free
        ...
    }

    if (ptr == NULL) {
        // malloc
        ...
    }

    // realloc
    ...
}
```

#### Free path (`nsize == 0`)

```c
if (nsize == 0) {
    if (ptr) {
        LuaAllocHdr *hdr = lua_hdr_from_user(ptr);
        if (!lua_hdr_valid(hdr, internal->memory_limit)) {
            log_error("LUA_ALLOC", "free(): header/canary invalid for %p", ptr);
            abort();
        }

        if (internal->memory_used >= hdr->size) {
            internal->memory_used -= hdr->size;
        } else {
            internal->memory_used = 0;
        }
        memory_manager.free((void *)hdr);
    }
    return NULL;
}
```

Behavior:

- Validates header and tail before freeing:
  - If invalid, logs and **aborts** immediately.
- Decrements `memory_used` by the old payload size (saturating at 0).
- Frees the entire block (header + payload + tail) through `memory_manager.free`.
- Returns `NULL` as required by `lua_Alloc`.

#### New allocation (`ptr == NULL`)

```c
if (ptr == NULL) {
    if (internal->memory_used + nsize > internal->memory_limit) {
        log_error("LUA_ENGINE", "Memory limit exceeded: %zu + %zu > %zu",
                  internal->memory_used, nsize, internal->memory_limit);
        return NULL;
    }

    size_t total_size = sizeof(LuaAllocHdr) + nsize + sizeof(uint64_t);
    LuaAllocHdr *hdr = (LuaAllocHdr *)memory_manager.malloc(total_size, MMTAG_LUA);
    if (!hdr) {
        return NULL;
    }

    hdr->size = nsize;
    hdr->pad = LUA_HDR_MAGIC;
    uint64_t *tail = lua_tail_from_hdr(hdr);
    *tail = LUA_TAIL_CANARY;

    internal->memory_used += nsize;

    return (char *)hdr + sizeof(LuaAllocHdr);
}
```

Key points:

- Enforces the per-engine **memory limit** before allocating:
  - If exceeded, logs an error and returns `NULL`. Lua’s VM will propagate this as an allocation failure.
- Requests a single block from the core memory manager (with tag `MMTAG_LUA`).
- Writes header and tail canary, then increments `memory_used`.
- Returns a pointer to the payload (immediately after the header).

#### Realloc path (`ptr != NULL && nsize > 0`)

```c
LuaAllocHdr *old_hdr = lua_hdr_from_user(ptr);
if (!lua_hdr_valid(old_hdr, internal->memory_limit)) {
    log_error("LUA_ALLOC", "realloc(): header/canary invalid for %p", ptr);
    abort();
}

size_t old_size = old_hdr->size;

if (nsize <= old_size) {
    // Shrink in place
    size_t delta = old_size - nsize;
    if (internal->memory_used >= delta) {
        internal->memory_used -= delta;
    } else {
        internal->memory_used = 0;
    }
    old_hdr->size = nsize;
    uint64_t *new_tail = lua_tail_from_hdr(old_hdr);
    *new_tail = LUA_TAIL_CANARY;
    return ptr;
}

// Grow
size_t grow = nsize - old_size;
if (internal->memory_used + grow > internal->memory_limit) {
    log_error("LUA_ALLOC", "realloc limit exceeded: %zu + %zu > %zu", internal->memory_used,
              grow, internal->memory_limit);
    return NULL;
}

size_t total_size = sizeof(LuaAllocHdr) + nsize + sizeof(uint64_t);
LuaAllocHdr *new_hdr = (LuaAllocHdr *)memory_manager.malloc(total_size, MMTAG_LUA);
if (!new_hdr) {
    return NULL;
}

// Safe copy
memcpy((char *)new_hdr + sizeof(LuaAllocHdr), ptr, old_size);

new_hdr->size = nsize;
new_hdr->pad = LUA_HDR_MAGIC;
uint64_t *new_tail = lua_tail_from_hdr(new_hdr);
*new_tail = LUA_TAIL_CANARY;

internal->memory_used += grow;

memory_manager.free((void *)old_hdr);

return (char *)new_hdr + sizeof(LuaAllocHdr);
```

Behavior:

- Validates the old block (header + canary) before doing anything.
- Two cases:
  1. **Shrink in place**
  2. **Grow** (allocate new, copy old payload, free old)

Again, `osize` is ignored; the allocator relies solely on its internal header instead of trusting the caller.

### Why Lua uses a separate header and canaries

Reasons for the custom header + tail instead of relying purely on the core manager:

1. **Per-engine accounting**

   Global stats in the memory manager are per-thread and per-tag, not per Lua engine. The Lua engine needs to enforce a **per-engine** `memory_limit`, so it must know the size of every Lua-visible block it owns. Storing `size` in `LuaAllocHdr` gives an authoritative value independent of any external caller.

2. **Corruption detection localized to Lua**

   The core memory manager knows the total size of each allocation but doesn’t know “which ones belong to Lua” vs “which ones belong to some other subsystem using `MMTAG_LUA`”. The Lua allocator:

   - Uses `LUA_HDR_MAGIC` in `pad` as a magic number.
   - Places a tail canary (`LUA_TAIL_CANARY`) immediately after the payload.

   This lets it detect:

   - Frees/reallocs on non-Lua blocks (bad header magic).
   - Overwrites past the end of Lua’s payload (corrupted tail canary).

3. **Independence from `osize`**

   Lua’s `lua_Alloc` interface passes an `osize` parameter, but it is not guaranteed to be precise or trustworthy across all builds / subsystems. The allocator instead relies entirely on **its own header**:

   - It ignores `osize` completely.
   - For realloc, it reads `old_size` from `old_hdr->size`.
   - For free, it subtracts `hdr->size` from `memory_used`.

   This makes accounting robust even if `osize` is 0 or inconsistent.

4. **Clear ownership model**

   The header establishes a clear “this block is Lua-owned” identity:

   - Only `_lua_engine_limited_alloc` should see / create valid `LuaAllocHdr`.
   - If any other subsystem passes arbitrary pointers into Lua’s allocator, the invalid header/canary check will trigger immediately.

   This matches the engine’s safety philosophy: **fail early and loudly** when misuse happens.

---

## Interaction between memory manager and Lua

### Tags and stats

All Lua allocations for the runtime heap go through:

```c
size_t total_size = sizeof(LuaAllocHdr) + nsize + sizeof(uint64_t);
LuaAllocHdr *hdr = (LuaAllocHdr *)memory_manager.malloc(total_size, MMTAG_LUA);
```

That means:

- Global stats for Lua’s runtime memory show up under:
  - `thread->tags[MMTAG_LUA]` for the thread running Lua.
  - The per-thread `global` stats as well.
- The **per-engine** `memory_used` is additionally tracked inside `EseLuaEngineInternal`, using the header’s `size` field.

Other Lua-adjacent allocations use different tags:

- `MMTAG_LUA_VALUE` – for `EseLuaValue` wrappers and their nested data.
- `MMTAG_LUA_SCRIPT` – for script buffers during loading / processing.

This split allows you to:

- Use `memory_manager.report(true)` to see **global** Lua-related memory usage broken down by subsystem (runtime, value wrappers, scripts, etc.).
- Inspect `engine->internal->memory_used` to answer “how much of the configured Lua heap limit is currently in use”.

### Threading considerations

The Lua allocator itself is **not cross-thread**:

- All Lua runtime operations are expected to happen on the **same thread**.
- The underlying memory is allocated with the normal (per-thread) API (`memory_manager.malloc`, not `memory_manager.shared.malloc`).

That means:

- The same thread that runs Lua is the one that will free the Lua heap.
- If you ever attempted to free Lua’s allocations from another thread, the core memory manager would see a free of an **untracked pointer in that thread** and abort.

This is by design and matches the typical “Lua state is not shared between threads” model.

---

## Tests and validation

The memory manager is covered by `tests/test_memory_manager.c`, which validates:

- Basic allocation APIs:
  - `malloc`, `calloc`, `realloc`, `free`, `strdup`
- Alignment:
  - `malloc` returns pointers that are 16-byte aligned.
- Tag usage:
  - Multiple tags can be allocated and freed without errors.
- Load behavior:
  - Many allocations of various sizes.
  - Large allocations (10MB).
- Thread behavior:
  - `test_memory_manager_thread_isolation`: ensures allocations work correctly in worker threads.
  - `test_memory_manager_concurrent_threads`: concurrent allocations/free/realloc across multiple threads.
- Reporting:
  - `memory_manager.report(false)` runs successfully.
- Destroy behavior:
  - `memory_manager.destroy(true)` can be called even with leaked allocations; leaks are **reported**, not silently fixed.

Lua-specific allocation correctness is indirectly tested via:

- Lua engine tests (where present) that run scripts, allocate tables/strings, and stress the VM.
- The engine itself, which will crash loudly if header/canary validation fails.

---

## Usage guidelines for engine contributors

### 1. Always go through the memory manager

Inside engine code (excluding carefully-isolated third-party integrations):

- Do **not** call `malloc`, `calloc`, `realloc`, or `free` directly.
- Instead, use:

  ```c
  void *p = memory_manager.malloc(size, MMTAG_SOMETHING);
  ...
  memory_manager.free(p);
  ```

This ensures:

- All allocations are tracked.
- Tags and per-thread stats stay accurate.
- Misuse (double-free, free of foreign pointers) is caught early.

### 2. Choose the correct tag

Pick a `MemTag` that matches the subsystem:

- Renderer-related data: `MMTAG_RENDERER`, `MMTAG_SPRITE`, `MMTAG_DRAWLIST`, etc.
- Entities and components: `MMTAG_ENTITY`, `MMTAG_COMP_LUA`, `MMTAG_COMP_MAP`, etc.
- Lua engine runtime: `MMTAG_LUA`
- Lua value wrappers: `MMTAG_LUA_VALUE`
- Maps/geometry types: `MMTAG_MAP`, `MMTAG_RECT`, `MMTAG_POINT`, etc.
- Ad-hoc scratch/debug allocations: `MMTAG_TEMP` (but don’t leave them in production code long-term).

Using tags consistently makes leak reports significantly more readable.

### 3. Respect thread ownership

- If a block will be **owned and freed by a single thread**, use the normal API (`memory_manager.malloc/free`).
- If ownership crosses thread boundaries (queues, worker threads, etc.), allocate via the shared API:

  ```c
  void *p = memory_manager.shared.malloc(size, MMTAG_THREAD);
  ...
  memory_manager.shared.free(p); // from any thread
  ```

Never free a per-thread allocation from a different thread; the manager will treat that as fatal misuse.

### 4. When integrating with Lua

- Do **not** bypass `_lua_engine_limited_alloc`:
  - All `lua_State` allocations are already routed through it.
  - The engine enforces memory limits and performs header/canary checks for you.
- For large engine-managed data structures that are merely *referenced* by Lua (e.g. maps, sprites, collision data):
  - Allocate them via the appropriate engine tag (`MMTAG_MAP`, `MMTAG_SPRITE`, etc.).
  - Expose them to Lua via userdata or lightuserdata.
- For **Lua-side** buffers or values you manage manually in C (e.g. custom Lua-side tables or string copies that live outside Lua’s own heap), use `MMTAG_LUA_VALUE` or another appropriate tag, and make sure their lifetime is tied to either:
  - The Lua state (destroyed in `lua_engine_destroy`), or
  - The owning engine object (destroyed when the object is destroyed).

### 5. Debugging leaks and corruption

- To inspect leaks and usage from the engine:

  ```c
  memory_manager.report(true);
  ```

  This prints per-thread and per-tag stats, plus sample leaks and backtraces (when enabled).

- If you hit assertions or aborts from the memory manager or Lua allocator:
  - Look at the **tag** and **backtrace** in the log to find who allocated or freed the offending pointer.
  - For Lua-specific issues:
    - “LUA_ALLOC header/canary invalid” usually means a bug in a C binding or userdata handling (e.g. using a freed pointer, or writing past a buffer).

---

## Summary

- The core memory manager provides **tagged, per-thread** allocation with leak tracking, integrity checks, and statistics.
- A separate **shared allocator** exists for cross-thread ownership.
- The Lua engine uses a custom allocator built on top of `memory_manager.malloc/free`, adding:
  - Per-engine **memory limits**
  - Header + canary for **Lua-specific corruption detection**
  - Accurate per-engine **usage accounting**

When writing engine code:

- Always go through `memory_manager`.
- Choose the right tag.
- Respect thread ownership and the separation between engine-managed and Lua-managed memory.
