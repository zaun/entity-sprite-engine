/*
 * Project: Entity Sprite Engine
 *
 * Implementation of the Collider System. Collects collider components and
 * updates their world bounds each frame based on the owning entity's
 * position. This replaces the per-component _collider_vtable_update logic so
 * that colliders themselves remain POD + Lua bindings and all behavior
 * lives in systems.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#include "entity/systems/collider_system.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "core/system_manager.h"
#include "core/system_manager_private.h"
#include "entity/components/entity_component_collider.h"
#include "entity/components/entity_component_private.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "utility/log.h"

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Internal data for the collider system.
 *
 * Maintains a dynamically-sized array of collider component pointers for
 * efficient per-frame world-bounds updates.
 */
typedef struct {
    EseEntityComponentCollider **colliders; /** Array of collider component pointers */
    size_t count;                           /** Current number of tracked colliders */
    size_t capacity;                        /** Allocated capacity of the array */
} ColliderSystemData;

// ========================================
// PRIVATE FUNCTIONS
// ========================================

/**
 * @brief Checks if the system accepts this component type.
 *
 * @param self System manager instance
 * @param comp Component to check
 * @return true if component type is ENTITY_COMPONENT_COLLIDER
 */
static bool collider_sys_accepts(EseSystemManager *self, const EseEntityComponent *comp) {
    (void)self;
    if (!comp) {
        return false;
    }
    return comp->type == ENTITY_COMPONENT_COLLIDER;
}

/**
 * @brief Called when a collider component is added to an entity.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param comp Component that was added
 */
static void collider_sys_on_add(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp) {
    (void)eng;
    ColliderSystemData *d = (ColliderSystemData *)self->data;

    if (!comp || !comp->data) {
        return;
    }

    // Expand array if needed
    if (d->count == d->capacity) {
        d->capacity = d->capacity ? d->capacity * 2 : 64;
        d->colliders = memory_manager.realloc(
            d->colliders, sizeof(EseEntityComponentCollider *) * d->capacity, MMTAG_ENGINE);
    }

    d->colliders[d->count++] = (EseEntityComponentCollider *)comp->data;
}

/**
 * @brief Called when a collider component is removed from an entity.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param comp Component that was removed
 */
static void collider_sys_on_remove(EseSystemManager *self, EseEngine *eng,
                                   EseEntityComponent *comp) {
    (void)eng;
    ColliderSystemData *d = (ColliderSystemData *)self->data;

    if (!comp || !comp->data || d->count == 0) {
        return;
    }

    EseEntityComponentCollider *cc = (EseEntityComponentCollider *)comp->data;

    // Find and remove collider from tracking array (swap with last element)
    for (size_t i = 0; i < d->count; i++) {
        if (d->colliders[i] == cc) {
            d->colliders[i] = d->colliders[--d->count];
            return;
        }
    }
}

/**
 * @brief Initialize the collider system.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 */
static void collider_sys_init(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    ColliderSystemData *d = memory_manager.calloc(1, sizeof(ColliderSystemData), MMTAG_ENGINE);
    self->data = d;
}

/**
 * @brief Update all collider components' world bounds.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param dt Delta time (unused)
 */
static EseSystemJobResult collider_sys_update(EseSystemManager *self, EseEngine *eng, float dt) {
    (void)eng;
    (void)dt;
    ColliderSystemData *d = (ColliderSystemData *)self->data;

    for (size_t i = 0; i < d->count; i++) {
        EseEntityComponentCollider *cc = d->colliders[i];
        if (!cc || !cc->base.entity || !cc->base.entity->active) {
            continue;
        }

        if (!cc->base.entity->collision_bounds) {
            continue;
        }

        EseRect *entity_bounds = cc->base.entity->collision_bounds;

        if (!cc->base.entity->collision_world_bounds) {
            cc->base.entity->collision_world_bounds = ese_rect_create(cc->base.lua);
        }

        EseRect *world_bounds = cc->base.entity->collision_world_bounds;

        ese_rect_set_x(world_bounds,
                       ese_rect_get_x(entity_bounds) + ese_point_get_x(cc->base.entity->position));
        ese_rect_set_y(world_bounds,
                       ese_rect_get_y(entity_bounds) + ese_point_get_y(cc->base.entity->position));
        ese_rect_set_width(world_bounds, ese_rect_get_width(entity_bounds));
        ese_rect_set_height(world_bounds, ese_rect_get_height(entity_bounds));
        ese_rect_set_rotation(world_bounds, ese_rect_get_rotation(entity_bounds));
    }

    EseSystemJobResult res = {0};
    return res;
}

/**
 * @brief Clean up the collider system.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 */
static void collider_sys_shutdown(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    ColliderSystemData *d = (ColliderSystemData *)self->data;
    if (d) {
        if (d->colliders) {
            memory_manager.free(d->colliders);
        }
        memory_manager.free(d);
    }
}

/**
 * @brief Virtual table for the collider system.
 */
static const EseSystemManagerVTable ColliderSystemVTable = {
    .init = collider_sys_init,
    .update = collider_sys_update,
    .accepts = collider_sys_accepts,
    .on_component_added = collider_sys_on_add,
    .on_component_removed = collider_sys_on_remove,
    .shutdown = collider_sys_shutdown};

// ========================================
// PUBLIC FUNCTIONS
// ========================================

EseSystemManager *collider_system_create(void) {
    // Collider bounds updates can run in EARLY (before Lua/entity update) or
    // LATE (after Lua) depending on how you want collision data to be consumed.
    // For now, default to EARLY so all subsequent logic sees fresh bounds.
    return system_manager_create(&ColliderSystemVTable, SYS_PHASE_EARLY, NULL);
}

void engine_register_collider_system(EseEngine *eng) {
    log_assert("COLLIDER_SYS", eng, "engine_register_collider_system called with NULL engine");

    EseSystemManager *sys = collider_system_create();
    engine_add_system(eng, sys);
}
