/*
 * Project: Entity Sprite Engine
 *
 * Implementation of the Component Cleanup System. Handles deferred component
 * removal to prevent race conditions with parallel systems.
 *
 * Details:
 * Components are queued for removal when entity_component_remove is called.
 * During the CLEANUP phase (after all parallel systems complete), the system
 * processes the queue: notifies other systems, removes components from entities,
 * and destroys them.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#include "entity/systems/cleanup_system.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "core/system_manager.h"
#include "core/system_manager_private.h"
#include "entity/components/entity_component_private.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "utility/double_linked_list.h"
#include "utility/log.h"

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Structure to hold deferred component removal information.
 */
typedef struct {
    EseEntity *entity;           /** Entity that owns the component */
    EseEntityComponent *component; /** Component to be removed */
} DeferredComponentRemoval;

/**
 * @brief Internal data for the cleanup system.
 */
typedef struct {
    EseDoubleLinkedList *removal_queue; /** Queue of components to be removed */
} CleanupSystemData;

// ========================================
// PRIVATE FUNCTIONS
// ========================================

/**
 * @brief Checks if the system accepts this component type.
 *
 * @param self System manager instance
 * @param comp Component to check
 * @return true (accepts all component types)
 */
static bool cleanup_sys_accepts(EseSystemManager *self, const EseEntityComponent *comp) {
    (void)self;
    (void)comp;
    // Accept all component types
    return true;
}

/**
 * @brief Called when a component is being removed.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param comp Component that is being removed
 */
static void cleanup_sys_on_remove(EseSystemManager *self, EseEngine *eng,
                                   EseEntityComponent *comp) {
    (void)eng;
    CleanupSystemData *d = (CleanupSystemData *)self->data;

    if (!comp || !comp->entity) {
        return;
    }

    if (comp->entity->destroyed) {
        return;
    }

    // Queue the component for deferred removal (don't remove from entity yet)
    DeferredComponentRemoval *removal =
        memory_manager.malloc(sizeof(DeferredComponentRemoval), MMTAG_ENGINE);
    removal->entity = comp->entity;
    removal->component = comp;
    dlist_append(d->removal_queue, removal);
}

/**
 * @brief Initialize the cleanup system.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 */
static void cleanup_sys_init(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    CleanupSystemData *d = memory_manager.calloc(1, sizeof(CleanupSystemData), MMTAG_ENGINE);
    d->removal_queue = dlist_create(NULL);
    self->data = d;
}

/**
 * @brief Process deferred component removals.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param dt Delta time (unused)
 */
static EseSystemJobResult cleanup_sys_update(EseSystemManager *self, EseEngine *eng, float dt) {
    (void)dt;
    CleanupSystemData *d = (CleanupSystemData *)self->data;

    // Process all queued component removals
    void *v;
    while ((v = dlist_pop_front(d->removal_queue)) != NULL) {
        DeferredComponentRemoval *removal = (DeferredComponentRemoval *)v;
        EseEntity *entity = removal->entity;
        EseEntityComponent *comp = removal->component;

        // Skip if entity is already destroyed (will be cleaned up with entity)
        if (entity->destroyed) {
            memory_manager.free(removal);
            continue;
        }

        // Verify component still exists in entity (might have been removed already)
        bool found = false;
        int idx = -1;
        for (size_t i = 0; i < entity->component_count; i++) {
            if (entity->components[i] == comp) {
                found = true;
                idx = (int)i;
                break;
            }
        }

        if (found) {
            log_debug("CLEANUP_SYS",
                "Removing component %s from entity %s",
                comp->id, entity->id);

            // Remove from entity's component array
            comp->vtable->unref(comp);
            entity_component_destroy(comp);

            entity->components[idx] = entity->components[entity->component_count - 1];
            entity->components[entity->component_count - 1] = NULL;
            entity->component_count--;
        }

        memory_manager.free(removal);
    }

    EseSystemJobResult res = {0};
    return res;
}

/**
 * @brief Clean up the cleanup system.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 */
static void cleanup_sys_shutdown(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    CleanupSystemData *d = (CleanupSystemData *)self->data;
    if (d) {
        // Free any remaining queued removals
        void *v;
        while ((v = dlist_pop_front(d->removal_queue)) != NULL) {
            memory_manager.free(v);
        }
        dlist_free(d->removal_queue);
        memory_manager.free(d);
    }
}

/**
 * @brief Virtual table for the cleanup system.
 */
static const EseSystemManagerVTable CleanupSystemVTable = {
    .init = cleanup_sys_init,
    .update = cleanup_sys_update,
    .accepts = cleanup_sys_accepts,
    .on_component_added = NULL,
    .on_component_removed = cleanup_sys_on_remove,
    .shutdown = cleanup_sys_shutdown};

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Create a cleanup system.
 *
 * @return EseSystemManager* Created system
 */
EseSystemManager *cleanup_system_create(void) {
    return system_manager_create(&CleanupSystemVTable, SYS_PHASE_CLEANUP, NULL);
}

/**
 * @brief Register the cleanup system with the engine.
 *
 * @param eng Engine pointer
 */
void engine_register_cleanup_system(EseEngine *eng) {
    log_assert("CLEANUP_SYS", eng, "engine_register_cleanup_system called with NULL engine");
    EseSystemManager *sys = cleanup_system_create();
    engine_add_system(eng, sys);
}


