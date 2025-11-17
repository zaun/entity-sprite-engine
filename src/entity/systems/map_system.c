/*
 * Project: Entity Sprite Engine
 *
 * Implementation of the Map System. Collects map components and runs their
 * update behavior each frame in the LUA phase. This moves the
 * _entity_component_map_update logic out of the component vtable so that
 * map components themselves remain POD + Lua bindings and all behavior
 * lives in systems.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#include "entity/systems/map_system.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "core/system_manager.h"
#include "core/system_manager_private.h"
#include "entity/components/entity_component_map.h"
#include "entity/components/entity_component_private.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "types/rect.h"
#include "utility/log.h"
#include <math.h>

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Internal data for the map system.
 *
 * Maintains a dynamically-sized array of map component pointers for
 * efficient per-frame updates.
 */
typedef struct {
    EseEntityComponentMap **maps; /** Array of map component pointers */
    size_t count;                 /** Current number of tracked maps */
    size_t capacity;              /** Allocated capacity of the array */
} MapSystemData;

/**
 * @brief Per-component world-bounds result.
 *
 * All members are plain data so they can be safely copied between threads.
 */
typedef struct {
    EseEntityComponentMap *component; /** Map component processed */
    bool has_map;                     /** Whether the component had a map set */
    bool has_solid_cells;             /** Whether any solid cells were found */
    float px;                         /** Entity position X at computation time */
    float py;                         /** Entity position Y at computation time */
    float min_x, min_y;               /** World-space minimum bounds */
    float max_x, max_y;               /** World-space maximum bounds */
} MapBoundsResult;

/**
 * @brief Batch payload of bounds results for all tracked map components.
 */
typedef struct {
    size_t count;
    MapBoundsResult *items;
} MapBoundsBatch;

// ========================================
// PRIVATE FUNCTIONS
// ========================================

/**
 * @brief Deep-copy function for MapBoundsBatch used by JobResult on main thread.
 */
static void *_map_bounds_batch_copy(const void *worker_result, size_t worker_size,
                                    size_t *out_size) {
    (void)worker_size;
    const MapBoundsBatch *src = (const MapBoundsBatch *)worker_result;
    if (!src) {
        return NULL;
    }

    MapBoundsBatch *dst = memory_manager.calloc(1, sizeof(MapBoundsBatch), MMTAG_S_MAP);
    if (!dst) {
        return NULL;
    }

    dst->count = src->count;
    if (src->count > 0 && src->items) {
        dst->items =
            memory_manager.malloc(sizeof(MapBoundsResult) * src->count, MMTAG_S_MAP);
        if (!dst->items) {
            memory_manager.free(dst);
            return NULL;
        }
        memcpy(dst->items, src->items, sizeof(MapBoundsResult) * src->count);
    }

    if (out_size) {
        *out_size = sizeof(MapBoundsBatch);
    }
    return dst;
}

/**
 * @brief Free function for MapBoundsBatch on the worker thread.
 */
static void _map_bounds_batch_free(void *worker_result) {
    MapBoundsBatch *batch = (MapBoundsBatch *)worker_result;
    if (!batch) {
        return;
    }
    if (batch->items) {
        memory_manager.free(batch->items);
    }
    memory_manager.free(batch);
}

/**
 * @brief Apply a MapBoundsBatch result on the main thread.
 */
static void map_sys_apply_result(EseSystemManager *self, EseEngine *eng, void *result) {
    (void)self;
    (void)eng;
    MapBoundsBatch *batch = (MapBoundsBatch *)result;
    if (!batch || !batch->items) {
        if (batch) {
            memory_manager.free(batch);
        }
        return;
    }

    for (size_t i = 0; i < batch->count; i++) {
        MapBoundsResult *b = &batch->items[i];
        EseEntityComponentMap *component = b->component;
        EseEntity *entity = component ? component->base.entity : NULL;

        if (!component || !entity) {
            continue;
        }

        if (!b->has_map) {
            if (entity->collision_world_bounds) {
                ese_rect_destroy(entity->collision_world_bounds);
                entity->collision_world_bounds = NULL;
            }
            continue;
        }

        if (!entity->collision_world_bounds) {
            entity->collision_world_bounds = ese_rect_create(component->base.lua);
        }

        EseRect *world_bounds = entity->collision_world_bounds;

        if (b->has_solid_cells) {
            ese_rect_set_x(world_bounds, b->min_x);
            ese_rect_set_y(world_bounds, b->min_y);
            ese_rect_set_width(world_bounds, b->max_x - b->min_x);
            ese_rect_set_height(world_bounds, b->max_y - b->min_y);
            ese_rect_set_rotation(world_bounds, 0.0f);
        } else {
            ese_rect_set_x(world_bounds, b->px);
            ese_rect_set_y(world_bounds, b->py);
            ese_rect_set_width(world_bounds, 0.0f);
            ese_rect_set_height(world_bounds, 0.0f);
            ese_rect_set_rotation(world_bounds, 0.0f);
        }
    }

    // Free the main-thread copy of the batch and its items now that we've
    // applied the results. The worker-thread copy is freed separately via
    // the JobResult.free_fn hook.
    if (batch->items) {
        memory_manager.free(batch->items);
    }
    memory_manager.free(batch);
}

/**
 * @brief Checks if the system accepts this component type.
 */
static bool map_sys_accepts(EseSystemManager *self, const EseEntityComponent *comp) {
    (void)self;
    if (!comp) {
        return false;
    }
    return comp->type == ENTITY_COMPONENT_MAP;
}

/**
 * @brief Called when a map component is added to an entity.
 */
static void map_sys_on_add(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp) {
    (void)eng;
    MapSystemData *d = (MapSystemData *)self->data;

    if (!comp || !comp->data) {
        return;
    }

    if (d->count == d->capacity) {
        d->capacity = d->capacity ? d->capacity * 2 : 64;
        d->maps = memory_manager.realloc(d->maps, sizeof(EseEntityComponentMap *) * d->capacity,
                                         MMTAG_S_MAP);
    }

    d->maps[d->count++] = (EseEntityComponentMap *)comp->data;
}

/**
 * @brief Called when a map component is removed from an entity.
 */
static void map_sys_on_remove(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp) {
    (void)eng;
    MapSystemData *d = (MapSystemData *)self->data;

    if (!comp || !comp->data || d->count == 0) {
        return;
    }

    EseEntityComponentMap *mc = (EseEntityComponentMap *)comp->data;

    for (size_t i = 0; i < d->count; i++) {
        if (d->maps[i] == mc) {
            d->maps[i] = d->maps[--d->count];
            return;
        }
    }
}

/**
 * @brief Initialize the map system.
 */
static void map_sys_init(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    MapSystemData *d = memory_manager.calloc(1, sizeof(MapSystemData), MMTAG_S_MAP);
    self->data = d;
}

/**
 * @brief Update all map components each frame.
 *
 * Computes world bounds for all tracked map components and returns a
 * MapBoundsBatch result so that rect updates happen only on the main thread
 * via the system's apply_result callback.
 */
static EseSystemJobResult map_sys_update(EseSystemManager *self, EseEngine *eng, float dt) {
    (void)dt;
    (void)eng;
    MapSystemData *d = (MapSystemData *)self->data;

    EseSystemJobResult job_res = {0};

    if (!d || d->count == 0) {
        return job_res;
    }

    MapBoundsBatch *batch = memory_manager.calloc(1, sizeof(MapBoundsBatch), MMTAG_S_MAP);
    if (!batch) {
        return job_res;
    }

    batch->count = d->count;
    if (batch->count > 0) {
        batch->items =
            memory_manager.malloc(sizeof(MapBoundsResult) * batch->count, MMTAG_S_MAP);
        if (!batch->items) {
            memory_manager.free(batch);
            return job_res;
        }
    }

    for (size_t i = 0; i < d->count; i++) {
        MapBoundsResult *out = &batch->items[i];
        memset(out, 0, sizeof(MapBoundsResult));
        out->component = d->maps[i];

        EseEntityComponentMap *component = out->component;
        EseEntity *entity = component ? component->base.entity : NULL;

        if (!component || !entity || !entity->active) {
            out->has_map = false;
            continue;
        }

        if (!component->map) {
            out->has_map = false;
            continue;
        }

        out->has_map = true;

        int mw = ese_map_get_width(component->map);
        int mh = ese_map_get_height(component->map);
        float px = ese_point_get_x(entity->position);
        float py = ese_point_get_y(entity->position);
        out->px = px;
        out->py = py;

        float min_x = INFINITY, min_y = INFINITY, max_x = -INFINITY, max_y = -INFINITY;
        bool has_solid_cells = false;

        for (int y = 0; y < mh; y++) {
            for (int x = 0; x < mw; x++) {
                EseMapCell *cell = ese_map_get_cell(component->map, x, y);
                if (cell && (ese_map_cell_get_flags(cell) & MAP_CELL_FLAG_SOLID)) {
                    float cell_x = px + (float)x * (float)component->size;
                    float cell_y = py + (float)y * (float)component->size;
                    min_x = fminf(min_x, cell_x);
                    min_y = fminf(min_y, cell_y);
                    max_x = fmaxf(max_x, cell_x + (float)component->size);
                    max_y = fmaxf(max_y, cell_y + (float)component->size);
                    has_solid_cells = true;
                }
            }
        }

        out->has_solid_cells = has_solid_cells;
        if (has_solid_cells) {
            out->min_x = min_x;
            out->min_y = min_y;
            out->max_x = max_x;
            out->max_y = max_y;
        }
    }

    job_res.result = batch;
    job_res.size = sizeof(MapBoundsBatch);
    job_res.copy_fn = _map_bounds_batch_copy;
    job_res.free_fn = _map_bounds_batch_free;
    return job_res;
}

/**
 * @brief Clean up the map system.
 */
static void map_sys_shutdown(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    MapSystemData *d = (MapSystemData *)self->data;
    if (d) {
        if (d->maps) {
            memory_manager.free(d->maps);
        }
        memory_manager.free(d);
    }
}

static const EseSystemManagerVTable MapSystemVTable = {
    .init = map_sys_init,
    .update = map_sys_update,
    .accepts = map_sys_accepts,
    .on_component_added = map_sys_on_add,
    .on_component_removed = map_sys_on_remove,
    .shutdown = map_sys_shutdown,
    .apply_result = map_sys_apply_result};

// ========================================
// PUBLIC FUNCTIONS
// ========================================

EseSystemManager *map_system_create(void) {
    return system_manager_create(&MapSystemVTable, SYS_PHASE_EARLY, NULL);
}

void engine_register_map_system(EseEngine *eng) {
    log_assert("MAP_SYS", eng, "engine_register_map_system called with NULL engine");

    EseSystemManager *sys = map_system_create();
    engine_add_system(eng, sys);
}
