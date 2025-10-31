/*
 * Project: Entity Sprite Engine
 *
 * Implementation of the Sprite Animation System. Manages sprite animation by
 * advancing animation frames for all active sprite components based on elapsed
 * time.
 *
 * Details:
 * The system maintains a dynamic array of sprite component pointers for
 * efficient iteration during updates. Components are added/removed via
 * callbacks. During update, animation frames advance based on each sprite's
 * animation speed and elapsed time.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#include "entity/systems/sprite_system.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "core/system_manager.h"
#include "core/system_manager_private.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component_sprite.h"
#include "graphics/sprite.h"
#include "utility/log.h"

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Internal data for the sprite system.
 *
 * Maintains a dynamically-sized array of sprite component pointers for
 * efficient iteration during updates.
 */
typedef struct {
    EseEntityComponentSprite **sprites; /** Array of sprite component pointers */
    size_t count;                       /** Current number of tracked sprites */
    size_t capacity;                    /** Allocated capacity of the array */
} SpriteSystemData;

// ========================================
// PRIVATE FUNCTIONS
// ========================================

/**
 * @brief Checks if the system accepts this component type.
 *
 * @param self System manager instance
 * @param comp Component to check
 * @return true if component type is ENTITY_COMPONENT_SPRITE
 */
static bool sprite_sys_accepts(EseSystemManager *self, const EseEntityComponent *comp) {
    (void)self;
    if (!comp) {
        return false;
    }
    return comp->type == ENTITY_COMPONENT_SPRITE;
}

/**
 * @brief Called when a sprite component is added to an entity.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param comp Component that was added
 */
static void sprite_sys_on_add(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp) {
    (void)eng;
    SpriteSystemData *d = (SpriteSystemData *)self->data;

    // Expand array if needed
    if (d->count == d->capacity) {
        d->capacity = d->capacity ? d->capacity * 2 : 64;
        d->sprites = memory_manager.realloc(
            d->sprites, sizeof(EseEntityComponentSprite *) * d->capacity, MMTAG_S_SPRITE);
    }

    // Add sprite to tracking array
    d->sprites[d->count++] = (EseEntityComponentSprite *)comp->data;
}

/**
 * @brief Called when a sprite component is removed from an entity.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param comp Component that was removed
 */
static void sprite_sys_on_remove(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp) {
    (void)eng;
    SpriteSystemData *d = (SpriteSystemData *)self->data;
    EseEntityComponentSprite *sp = (EseEntityComponentSprite *)comp->data;

    // Find and remove sprite from tracking array (swap with last element)
    for (size_t i = 0; i < d->count; i++) {
        if (d->sprites[i] == sp) {
            d->sprites[i] = d->sprites[--d->count];
            return;
        }
    }
}

/**
 * @brief Initialize the sprite system.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 */
static void sprite_sys_init(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    SpriteSystemData *d = memory_manager.calloc(1, sizeof(SpriteSystemData), MMTAG_S_SPRITE);
    self->data = d;
}

/**
 * @brief Update all sprite animations.
 *
 * Advances animation frames based on elapsed time for all tracked sprites.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param dt Delta time in seconds
 */
static void sprite_sys_update(EseSystemManager *self, EseEngine *eng, float dt) {
    (void)eng;
    SpriteSystemData *d = (SpriteSystemData *)self->data;

    for (size_t i = 0; i < d->count; i++) {
        EseEntityComponentSprite *sp = d->sprites[i];

        // Skip sprites without a sprite name
        if (!sp->sprite_name) {
            sp->current_frame = 0;
            sp->sprite_ellapse_time = 0;
            continue;
        }

        // Look up sprite by name
        EseSprite *sprite = engine_get_sprite(eng, sp->sprite_name);
        if (!sprite) {
            sp->current_frame = 0;
            sp->sprite_ellapse_time = 0;
            continue; // Skip if sprite not found
        }

        // Advance animation time
        sp->sprite_ellapse_time += dt;
        float speed = sprite_get_speed(sprite);

        // Check if it's time to advance the frame
        if (sp->sprite_ellapse_time >= speed) {
            sp->sprite_ellapse_time = 0.0f;
            int frame_count = sprite_get_frame_count(sprite);
            if (frame_count > 0) {
                sp->current_frame = (sp->current_frame + 1) % (size_t)frame_count;
            }
        }
    }
}

/**
 * @brief Clean up the sprite system.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 */
static void sprite_sys_shutdown(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    SpriteSystemData *d = (SpriteSystemData *)self->data;
    if (d) {
        if (d->sprites) {
            memory_manager.free(d->sprites);
        }
        memory_manager.free(d);
    }
}

/**
 * @brief Virtual table for the sprite system.
 */
static const EseSystemManagerVTable SpriteSystemVTable = {
    .init = sprite_sys_init,
    .update = sprite_sys_update,
    .accepts = sprite_sys_accepts,
    .on_component_added = sprite_sys_on_add,
    .on_component_removed = sprite_sys_on_remove,
    .shutdown = sprite_sys_shutdown};

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Create a sprite animation system.
 *
 * @return EseSystemManager* Created system
 */
EseSystemManager *sprite_system_create(void) {
    return system_manager_create(&SpriteSystemVTable, SYS_PHASE_EARLY, NULL);
}

/**
 * @brief Register the sprite system with the engine.
 *
 * @param eng Engine pointer
 */
void engine_register_sprite_system(EseEngine *eng) {
    log_assert("SPRITE_SYS", eng, "engine_register_sprite_system called with NULL engine");
    EseSystemManager *sys = sprite_system_create();
    engine_add_system(eng, sys);
}
