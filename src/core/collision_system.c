/*
 * Project: Entity Sprite Engine
 *
 * Collision System using non-blocking JobResult handoff.
 *
 * Workers:
 * - Build a HitBatch (array of EseCollisionHit*) using per-thread allocator.
 * - Return HitBatch via JobResult with free_fn to destroy worker-side batch.
 *
 * Main thread:
 * - copy_fn deep-copies worker hits to main-owned hits.
 * - callback moves copied hits into eng->collision_hits (main-owned).
 * - cleanup frees the main-side temporary batch container and decrements
 *   pending job counter; when it reaches zero, pairs are destroyed.
 *
 * No shared allocator and no cross-thread frees. No main-thread blocking.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#include <string.h>
#include "core/collision_system.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "core/system_manager.h"
#include "core/system_manager_private.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "types/collision_hit.h"
#include "types/rect.h"
#include "utility/array.h"
#include "utility/job_queue.h"
#include "utility/log.h"
#include "utility/spatial_index.h"

// ========================================
// Internal types
// ========================================

typedef struct HitBatch {
    EseCollisionHit** items;
    size_t count;
    size_t cap;
} HitBatch;

typedef struct CollisionSystemData {
    SpatialIndex* spatial;
    EseArray* pairs;     // SpatialPair* array (main-owned)
    size_t pending_jobs; // Outstanding worker jobs for current frame
    size_t worker_count;
} CollisionSystemData;

typedef struct CollisionJobUD {
    CollisionSystemData* cs; // system data (for pending_jobs and pairs ownership)
    EseEngine* eng;          // engine (target array)
    EseArray* pairs;         // read-only for worker
    size_t start;
    size_t end;
} CollisionJobUD;

// ========================================
// HitBatch helpers
// ========================================

static HitBatch* hit_batch_create(size_t cap_hint) {
    HitBatch* b = (HitBatch*)memory_manager.calloc(1, sizeof(HitBatch), MMTAG_COLLISION_INDEX);
    b->cap = cap_hint ? cap_hint : 8;
    b->items = (EseCollisionHit**)memory_manager.malloc(
        sizeof(EseCollisionHit*) * b->cap, MMTAG_COLLISION_INDEX);
    b->count = 0;
    return b;
}

static void hit_batch_push(HitBatch* b, EseCollisionHit* h) {
    if (b->count == b->cap) {
        size_t ncap = b->cap ? b->cap * 2 : 8;
        b->items = (EseCollisionHit**)memory_manager.realloc(
            b->items, sizeof(EseCollisionHit*) * ncap, MMTAG_COLLISION_INDEX);
        b->cap = ncap;
    }
    b->items[b->count++] = h;
}

static void hit_batch_destroy_worker(HitBatch* b) {
    if (!b)
        return;
    for (size_t i = 0; i < b->count; i++) {
        if (b->items[i]) {
            ese_collision_hit_destroy(b->items[i]);
        }
    }
    memory_manager.free(b->items);
    memory_manager.free(b);
}

static HitBatch* hit_batch_copy_main(const HitBatch* src) {
    if (!src)
        return NULL;
    HitBatch* out = (HitBatch*)memory_manager.calloc(1, sizeof(HitBatch), MMTAG_COLLISION_INDEX);
    out->cap = src->count ? src->count : 1;
    out->count = src->count;
    out->items = (EseCollisionHit**)memory_manager.malloc(
        sizeof(EseCollisionHit*) * out->cap, MMTAG_COLLISION_INDEX);
    for (size_t i = 0; i < src->count; i++) {
        out->items[i] = ese_collision_hit_copy(src->items[i]);
    }
    return out;
}

static void hit_batch_destroy_main_container(HitBatch* b) {
    if (!b)
        return;
    memory_manager.free(b->items);
    memory_manager.free(b);
}

// ========================================
// JobResult copy/free/callback/cleanup
// ========================================

static void* hit_batch_copy_fn(const void* worker_result, size_t worker_size, size_t* out_size) {
    (void)worker_size;
    const HitBatch* wb = (const HitBatch*)worker_result;
    HitBatch* mb = hit_batch_copy_main(wb);
    if (out_size)
        *out_size = sizeof(HitBatch);
    return mb;
}

static void hit_batch_free_fn(void* worker_result) {
    HitBatch* wb = (HitBatch*)worker_result;
    hit_batch_destroy_worker(wb);
}

static void _collision_job_callback(ese_job_id_t job_id, void* user_data, void* result) {
    (void)job_id;
    CollisionJobUD* ud = (CollisionJobUD*)user_data;
    HitBatch* mb = (HitBatch*)result;
    if (!ud || !ud->eng || !mb)
        return;

    if (!ud->eng->collision_hits) {
        ud->eng->collision_hits =
            array_create(128, (void (*)(void*))ese_collision_hit_destroy);
    }

    for (size_t i = 0; i < mb->count; i++) {
        array_push(ud->eng->collision_hits, mb->items[i]);
        mb->items[i] = NULL;
    }
}

static void _collision_job_cleanup(ese_job_id_t job_id, void* user_data, void* result) {
    (void)job_id;
    CollisionJobUD* ud = (CollisionJobUD*)user_data;

    if (result) {
        hit_batch_destroy_main_container((HitBatch*)result);
    }

    if (ud && ud->cs) {
        if (ud->cs->pending_jobs > 0) {
            ud->cs->pending_jobs--;
        }
        if (ud->cs->pending_jobs == 0) {
            if (ud->cs->pairs) {
                array_destroy(ud->cs->pairs);
                ud->cs->pairs = NULL;
            }
        }
    }

    if (ud) {
        memory_manager.free(ud);
    }
}

// ========================================
// Worker
// ========================================

static JobResult _collision_worker(void* thread_data, const void* user_data,
                                   volatile bool* canceled) {
    (void)thread_data;
    (void)canceled;

    const CollisionJobUD* ud = (const CollisionJobUD*)user_data;

    HitBatch* batch = hit_batch_create(8);

    for (size_t i = ud->start; i < ud->end; i++) {
        SpatialPair* sp = (SpatialPair*)array_get(ud->pairs, i);
        EseEntity* a = sp->a;
        EseEntity* b = sp->b;

        if (!a->collision_world_bounds || !b->collision_world_bounds)
            continue;

        if (!ese_rect_intersects(a->collision_world_bounds, b->collision_world_bounds))
            continue;

        EseArray* hits = array_create(4, NULL);
        bool colliding = entity_test_collision(a, b, hits);
        if (colliding) {
            for (size_t j = 0; j < array_size(hits); j++) {
                EseCollisionHit* h = (EseCollisionHit*)array_get(hits, j);
                EseCollisionHit* cpy = ese_collision_hit_copy(h);
                hit_batch_push(batch, cpy);
            }
        }
        array_destroy(hits);
    }

    JobResult r;
    r.result = batch;
    r.size = sizeof(HitBatch);
    r.copy_fn = hit_batch_copy_fn;
    r.free_fn = hit_batch_free_fn;
    return r;
}

// ========================================
// System setup/update/teardown
// ========================================

static void _collision_system_setup(EseSystemManager* self, EseEngine* eng) {
    CollisionSystemData* cs = (CollisionSystemData*)self->data;

    spatial_index_clear(cs->spatial);

    EseDListIter* it = dlist_iter_create(eng->entities);
    void* val = NULL;
    while (dlist_iter_next(it, &val)) {
        EseEntity* e = (EseEntity*)val;
        if (e->active && e->collision_world_bounds) {
            spatial_index_insert(cs->spatial, e);
        }
    }
    dlist_iter_free(it);

    // Prepare target array for this frame (main-owned)
    if (!eng->collision_hits)
        eng->collision_hits =
            array_create(128, (void (*)(void*))ese_collision_hit_destroy);
    array_clear(eng->collision_hits);

    // Build pairs for workers to read (kept alive until last job cleanup)
    cs->pairs = spatial_index_get_pairs(cs->spatial);
    cs->pending_jobs = 0;

    log_debug("COLLISION_SYSTEM", "Collision system setup complete, %zu pairs ready",
              array_size(cs->pairs));
}

static void _collision_system_update(EseSystemManager* self, EseEngine* eng, float dt) {
    (void)dt;
    CollisionSystemData* cs = (CollisionSystemData*)self->data;

    size_t total = array_size(cs->pairs);
    if (!total) {
        return;
    }

    size_t workers = cs->worker_count > 0 ? cs->worker_count : 1;
    if (workers > total)
        workers = total;
    size_t slice = (total + workers - 1) / workers;

    cs->pending_jobs = workers;

    log_debug("COLLISION_SYSTEM", "Dispatching %zu workers for %zu pairs", workers, total);

    for (size_t w = 0; w < workers; w++) {
        CollisionJobUD* ud =
            (CollisionJobUD*)memory_manager.malloc(sizeof(CollisionJobUD), MMTAG_COLLISION_INDEX);
        ud->cs = cs;
        ud->eng = eng;
        ud->pairs = cs->pairs;
        ud->start = w * slice;
        ud->end = ud->start + slice > total ? total : ud->start + slice;

        (void)ese_job_queue_push(eng->job_queue, _collision_worker, _collision_job_callback,
                                 _collision_job_cleanup, ud);
    }

    // Do not wait here. Main thread remains responsive; callbacks will merge hits.
}

static void _collision_system_teardown(EseSystemManager* self, EseEngine* eng) {
    (void)self;
    (void)eng;
    // No blocking or merging here; callbacks handled merging already.
    // Pairs are destroyed in cleanup when the last job finishes.
}

// ========================================
// System Manager Lifecycle and API
// ========================================

static void _collision_system_shutdown(EseSystemManager* self, EseEngine* eng) {
    (void)eng;
    CollisionSystemData* cs = (CollisionSystemData*)self->data;
    // If any pair array remains (e.g., shutdown mid-frame), destroy it
    if (cs->pairs) {
        array_destroy(cs->pairs);
        cs->pairs = NULL;
    }
    memory_manager.free(cs);
}

static const EseSystemManagerVTable COLLISION_SYSTEM_VT = {
    .init = NULL,                               // Run once per system
    .setup = _collision_system_setup,           // Run once per frame on main thread
    .teardown = _collision_system_teardown,     // Run once per frame on main thread
    .update = _collision_system_update,         // Run once per frame on worker threads
    .accepts = NULL,                            // Not interested in components
    .on_component_added = NULL,                 // Not interested in components
    .on_component_removed = NULL,               // Not interested in components
    .shutdown = _collision_system_shutdown,     // Run once per system
};

EseSystemManager* _collision_system_create(SpatialIndex* spatial, size_t worker_count) {
    CollisionSystemData* data = (CollisionSystemData*)memory_manager.calloc(
        1, sizeof(CollisionSystemData), MMTAG_ENGINE);
    data->spatial = spatial;
    data->worker_count = worker_count ? worker_count : 1;
    return system_manager_create(&COLLISION_SYSTEM_VT, SYS_PHASE_EARLY, data);
}

void engine_register_collision_system(EseEngine* eng, size_t worker_count) {
    log_assert("COLLISION_SYSTEM", eng, "engine_register_collision_system called with NULL engine");
    log_assert("COLLISION_SYSTEM", eng->spatial_index, "engine has NULL spatial_index");

    size_t workers = worker_count ? worker_count : 1;
    EseSystemManager* sys = _collision_system_create(eng->spatial_index, workers);
    engine_add_system(eng, sys);
    log_debug("COLLISION_SYSTEM", "Registered collision system (%zu workers)", workers);
}