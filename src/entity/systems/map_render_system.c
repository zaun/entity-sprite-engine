/*
 * Project: Entity Sprite Engine
 *
 * Implementation of the Map Render System. Collects map components and renders
 * them to the draw list in the LATE phase, converting world coordinates to
 * screen coordinates using the camera.
 *
 * Details:
 * The system maintains a dynamic array of map component pointers for efficient
 * rendering. Components are added/removed via callbacks. During update, maps
 * are rendered with proper camera-relative positioning.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#include "entity/systems/map_render_system.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "core/system_manager.h"
#include "core/system_manager_private.h"
#include "entity/components/entity_component_map.h"
#include "entity/components/entity_component_private.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "graphics/draw_list.h"
#include "types/point.h"
#include "types/rect.h"
#include "utility/log.h"

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Internal data for the map render system.
 *
 * Maintains a dynamically-sized array of map component pointers for efficient
 * rendering during the LATE phase.
 */
typedef struct {
    EseEntityComponentMap **maps; /** Array of map component pointers */
    size_t count;                 /** Current number of tracked maps */
    size_t capacity;              /** Allocated capacity of the array */
} MapRenderSystemData;

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

static bool map_render_sys_accepts(EseSystemManager *self, const EseEntityComponent *comp);
static void map_render_sys_on_add(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp);
static void map_render_sys_on_remove(EseSystemManager *self, EseEngine *eng,
                                     EseEntityComponent *comp);
static void map_render_sys_init(EseSystemManager *self, EseEngine *eng);
static void map_render_sys_update(EseSystemManager *self, EseEngine *eng, float dt);
static void map_render_sys_shutdown(EseSystemManager *self, EseEngine *eng);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

/**
 * @brief Checks if the system accepts this component type.
 *
 * @param self System manager instance
 * @param comp Component to check
 * @return true if component type is ENTITY_COMPONENT_MAP
 */
static bool map_render_sys_accepts(EseSystemManager *self, const EseEntityComponent *comp) {
    (void)self;

    if (!comp) {
        return false;
    }

    return comp->type == ENTITY_COMPONENT_MAP;
}

/**
 * @brief Called when a map component is added to an entity.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param comp Component that was added
 */
static void map_render_sys_on_add(EseSystemManager *self, EseEngine *eng,
                                  EseEntityComponent *comp) {
    (void)eng;
    MapRenderSystemData *d = (MapRenderSystemData *)self->data;

    if (!d) {
        return;
    }

    // Expand array if needed
    if (d->count == d->capacity) {
        d->capacity = d->capacity ? d->capacity * 2 : 64;
        d->maps = memory_manager.realloc(d->maps, sizeof(EseEntityComponentMap *) * d->capacity,
                                         MMTAG_RS_MAP);
    }

    // Add map to tracking array
    d->maps[d->count++] = (EseEntityComponentMap *)comp->data;
}

/**
 * @brief Called when a map component is removed from an entity.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param comp Component that was removed
 */
static void map_render_sys_on_remove(EseSystemManager *self, EseEngine *eng,
                                     EseEntityComponent *comp) {
    (void)eng;
    MapRenderSystemData *d = (MapRenderSystemData *)self->data;
    EseEntityComponentMap *mc = (EseEntityComponentMap *)comp->data;

    if (!d) {
        return;
    }

    // Find and remove map from tracking array (swap with last element)
    for (size_t i = 0; i < d->count; i++) {
        if (d->maps[i] == mc) {
            d->maps[i] = d->maps[--d->count];
            return;
        }
    }
}

/**
 * @brief Initialize the map render system.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 */
static void map_render_sys_init(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    MapRenderSystemData *d = memory_manager.calloc(1, sizeof(MapRenderSystemData), MMTAG_RS_MAP);
    self->data = d;
}

/**
 * @brief Render all map components.
 *
 * Iterates through all tracked map components and submits them to the renderer.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param dt Delta time (unused)
 */
static void map_render_sys_update(EseSystemManager *self, EseEngine *eng, float dt) {
    (void)eng;
    (void)dt;
    MapRenderSystemData *d = (MapRenderSystemData *)self->data;

    if (!d || d->count == 0) {
        return;
    }

    // TODO: Render all tracked map components
}

/**
 * @brief Shutdown the map render system.
 *
 * Cleans up allocated resources.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 */
static void map_render_sys_shutdown(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    MapRenderSystemData *d = (MapRenderSystemData *)self->data;

    if (d) {
        if (d->maps) {
            memory_manager.free(d->maps);
        }
        memory_manager.free(d);
        self->data = NULL;
    }
}

// ========================================
// VTABLE
// ========================================

static const EseSystemManagerVTable MapRenderSystemVTable = {
    .init = map_render_sys_init,
    .update = map_render_sys_update,
    .accepts = map_render_sys_accepts,
    .on_component_added = map_render_sys_on_add,
    .on_component_removed = map_render_sys_on_remove,
    .shutdown = map_render_sys_shutdown};

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Create a map render system.
 *
 * @return EseSystemManager* Created system
 */
EseSystemManager *map_render_system_create(void) {
    return system_manager_create(&MapRenderSystemVTable, SYS_PHASE_LATE, NULL);
}

/**
 * @brief Register the map render system with the engine.
 *
 * @param eng Engine pointer
 */
void engine_register_map_render_system(EseEngine *eng) {
    log_assert("MAP_RENDER_SYS", eng, "engine_register_map_render_system called with NULL engine");
    EseSystemManager *sys = map_render_system_create();
    engine_add_system(eng, sys);
}
