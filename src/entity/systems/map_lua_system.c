/*
 * Project: Entity Sprite Engine
 *
 * Implementation of the Map Lua System. Collects map components and runs
 * their Lua-driven behavior (map_init / map_update and related hooks) each
 * frame in the LUA phase. This allows map_system to focus on pure data
 * (world-bounds), while this system handles scripting.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#include "entity/systems/map_lua_system.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "core/system_manager.h"
#include "core/system_manager_private.h"
#include "entity/components/entity_component_map.h"
#include "entity/components/entity_component_private.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "utility/hashmap.h"
#include "utility/log.h"
#include "utility/profile.h"

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Internal data for the map Lua system.
 */
typedef struct {
    EseEntityComponentMap **maps; /** Array of map component pointers */
    size_t count;                 /** Current number of tracked maps */
    size_t capacity;              /** Allocated capacity of the array */
} MapLuaSystemData;

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Forward declaration for map function cache helper implemented in
// entity_component_map.c.
void _entity_component_map_cache_functions(EseEntityComponentMap *component);

static bool map_lua_sys_accepts(EseSystemManager *self, const EseEntityComponent *comp) {
    (void)self;
    if (!comp) {
        return false;
    }
    return comp->type == ENTITY_COMPONENT_MAP;
}

static void map_lua_sys_on_add(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp) {
    (void)eng;
    MapLuaSystemData *d = (MapLuaSystemData *)self->data;

    if (!comp || !comp->data) {
        return;
    }

    if (d->count == d->capacity) {
        d->capacity = d->capacity ? d->capacity * 2 : 64;
        d->maps = memory_manager.realloc(d->maps, sizeof(EseEntityComponentMap *) * d->capacity,
                                         MMTAG_ENGINE);
    }

    d->maps[d->count++] = (EseEntityComponentMap *)comp->data;
}

static void map_lua_sys_on_remove(EseSystemManager *self, EseEngine *eng,
                                  EseEntityComponent *comp) {
    (void)eng;
    MapLuaSystemData *d = (MapLuaSystemData *)self->data;

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

static void map_lua_sys_init(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    MapLuaSystemData *d = memory_manager.calloc(1, sizeof(MapLuaSystemData), MMTAG_ENGINE);
    self->data = d;
}

/**
 * @brief Run Lua behavior for all map components each frame.
 *
 * This is essentially the Lua half of map behavior: responsible for
 * instance creation, function caching, and calling map_init/map_update.
 */
static EseSystemJobResult map_lua_sys_update(EseSystemManager *self, EseEngine *eng, float dt) {
    (void)eng;
    MapLuaSystemData *d = (MapLuaSystemData *)self->data;

    for (size_t i = 0; i < d->count; i++) {
        EseEntityComponentMap *component = d->maps[i];
        EseEntity *entity = component ? component->base.entity : NULL;

        if (!component || !entity || !entity->active) {
            continue;
        }

        if (component->script == NULL) {
            continue;
        }

        // First-time setup: create Lua instance and cache functions
        if (component->instance_ref == LUA_NOREF) {
            component->instance_ref =
                lua_engine_instance_script(component->engine, component->script);

            if (component->instance_ref == LUA_NOREF) {
                continue;
            }

            _entity_component_map_cache_functions(component);

            if (component->function_cache && component->engine && component->map) {
                CachedLuaFunction *cached_init = hashmap_get(component->function_cache, "map_init");
                if (cached_init && cached_init->exists) {
                    lua_value_set_map(component->map_arg, component->map);
                    EseLuaValue *init_args[] = {component->map_arg};
                    lua_engine_run_function_ref(component->engine, cached_init->function_ref,
                                                entity_get_lua_ref(entity), 1, init_args, NULL);
                }
            }
        }

        if (!component->function_cache || !component->engine || !component->map) {
            continue;
        }

        CachedLuaFunction *cached_update = hashmap_get(component->function_cache, "map_update");
        if (cached_update && cached_update->exists) {
            lua_value_set_number(component->delta_time_arg, dt);
            lua_value_set_map(component->map_arg, component->map);
            EseLuaValue *update_args[] = {component->delta_time_arg, component->map_arg};
            lua_engine_run_function_ref(component->engine, cached_update->function_ref,
                                        entity_get_lua_ref(entity), 2, update_args, NULL);
        }
    }

    EseSystemJobResult res = {0};
    return res;
}

static void map_lua_sys_shutdown(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    MapLuaSystemData *d = (MapLuaSystemData *)self->data;
    if (d) {
        if (d->maps) {
            memory_manager.free(d->maps);
        }
        memory_manager.free(d);
    }
}

static const EseSystemManagerVTable MapLuaSystemVTable = {
    .init = map_lua_sys_init,
    .update = map_lua_sys_update,
    .accepts = map_lua_sys_accepts,
    .on_component_added = map_lua_sys_on_add,
    .on_component_removed = map_lua_sys_on_remove,
    .shutdown = map_lua_sys_shutdown};

// ========================================
// PUBLIC FUNCTIONS
// ========================================

EseSystemManager *map_lua_system_create(void) {
    return system_manager_create(&MapLuaSystemVTable, SYS_PHASE_LUA, NULL);
}

void engine_register_map_lua_system(EseEngine *eng) {
    log_assert("MAP_LUA_SYS", eng, "engine_register_map_lua_system called with NULL engine");

    EseSystemManager *sys = map_lua_system_create();
    engine_add_system(eng, sys);
}
