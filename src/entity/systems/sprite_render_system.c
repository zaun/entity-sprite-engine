/*
 * Project: Entity Sprite Engine
 *
 * Implementation of the Sprite Render System. Collects sprite components and
 * renders them to the draw list in the LATE phase, converting world coordinates
 * to screen coordinates using the camera.
 *
 * Details:
 * The system maintains a dynamic array of sprite component pointers for
 * efficient rendering. Components are added/removed via callbacks. During
 * update, sprites are rendered with proper frame lookup and camera-relative
 * positioning.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#include "entity/systems/sprite_render_system.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "core/system_manager.h"
#include "core/system_manager_private.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component_sprite.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "graphics/draw_list.h"
#include "graphics/sprite.h"
#include "types/point.h"
#include "utility/log.h"

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Internal data for the sprite render system.
 *
 * Maintains a dynamically-sized array of sprite component pointers for
 * efficient rendering during the LATE phase.
 */
typedef struct {
    EseEntityComponentSprite **sprites; /** Array of sprite component pointers */
    size_t count;                       /** Current number of tracked sprites */
    size_t capacity;                    /** Allocated capacity of the array */
} SpriteRenderSystemData;

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
static bool sprite_render_sys_accepts(EseSystemManager *self, const EseEntityComponent *comp) {
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
static void sprite_render_sys_on_add(EseSystemManager *self, EseEngine *eng,
                                     EseEntityComponent *comp) {
    (void)eng;
    SpriteRenderSystemData *d = (SpriteRenderSystemData *)self->data;

    // Expand array if needed
    if (d->count == d->capacity) {
        d->capacity = d->capacity ? d->capacity * 2 : 64;
        d->sprites = memory_manager.realloc(
            d->sprites, sizeof(EseEntityComponentSprite *) * d->capacity, MMTAG_RS_SPRITE);
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
static void sprite_render_sys_on_remove(EseSystemManager *self, EseEngine *eng,
                                        EseEntityComponent *comp) {
    (void)eng;
    SpriteRenderSystemData *d = (SpriteRenderSystemData *)self->data;
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
 * @brief Initialize the sprite render system.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 */
static void sprite_render_sys_init(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    SpriteRenderSystemData *d =
        memory_manager.calloc(1, sizeof(SpriteRenderSystemData), MMTAG_RS_SPRITE);
    self->data = d;
}

/**
 * @brief Render all sprites.
 *
 * Iterates through all tracked sprites and submits them to the renderer.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param dt Delta time (unused)
 */
static void sprite_render_sys_update(EseSystemManager *self, EseEngine *eng, float dt) {
    (void)dt;
    SpriteRenderSystemData *d = (SpriteRenderSystemData *)self->data;

    for (size_t i = 0; i < d->count; i++) {
        EseEntityComponentSprite *sp = d->sprites[i];

        // Skip sprites without a sprite name or inactive entities
        if (!sp->sprite_name || !sp->base.entity || !sp->base.entity->active ||
            !sp->base.entity->visible) {
            continue;
        }

        // Look up sprite by name
        EseSprite *sprite = engine_get_sprite(eng, sp->sprite_name);
        if (!sprite) {
            continue; // Skip if sprite not found
        }

        // Get sprite frame data
        const char *texture_id;
        float x1, y1, x2, y2;
        int w, h;
        sprite_get_frame(sprite, sp->current_frame, &texture_id, &x1, &y1, &x2, &y2, &w, &h);

        // Get entity world position
        float entity_x = ese_point_get_x(sp->base.entity->position);
        float entity_y = ese_point_get_y(sp->base.entity->position);

        // Convert world coordinates to screen coordinates using camera
        EseCamera *camera = engine_get_camera(eng);
        EseDisplay *display = engine_get_display(eng);
        float camera_x = ese_point_get_x(camera->position);
        float camera_y = ese_point_get_y(camera->position);
        float view_width = ese_display_get_viewport_width(display);
        float view_height = ese_display_get_viewport_height(display);

        float view_left = camera_x - view_width / 2.0f;
        float view_top = camera_y - view_height / 2.0f;

        float screen_x = entity_x - view_left;
        float screen_y = entity_y - view_top;

        // Submit to draw list using the public API
        EseDrawList *draw_list = engine_get_draw_list(eng);
        _engine_add_texture_to_draw_list(screen_x, screen_y, w, h, sp->base.entity->draw_order,
                                         texture_id, x1, y1, x2, y2, w, h, draw_list);
    }
}

/**
 * @brief Clean up the sprite render system.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 */
static void sprite_render_sys_shutdown(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    SpriteRenderSystemData *d = (SpriteRenderSystemData *)self->data;
    if (d) {
        if (d->sprites) {
            memory_manager.free(d->sprites);
        }
        memory_manager.free(d);
    }
}

/**
 * @brief Virtual table for the sprite render system.
 */
static const EseSystemManagerVTable SpriteRenderSystemVTable = {
    .init = sprite_render_sys_init,
    .update = sprite_render_sys_update,
    .accepts = sprite_render_sys_accepts,
    .on_component_added = sprite_render_sys_on_add,
    .on_component_removed = sprite_render_sys_on_remove,
    .shutdown = sprite_render_sys_shutdown};

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Create a sprite render system.
 *
 * @return EseSystemManager* Created system
 */
EseSystemManager *sprite_render_system_create(void) {
    return system_manager_create(&SpriteRenderSystemVTable, SYS_PHASE_LATE, NULL);
}

/**
 * @brief Register the sprite render system with the engine.
 *
 * @param eng Engine pointer
 */
void engine_register_sprite_render_system(EseEngine *eng) {
    log_assert("SPRITE_RENDER_SYS", eng,
               "engine_register_sprite_render_system called with NULL engine");
    EseSystemManager *sys = sprite_render_system_create();
    engine_add_system(eng, sys);
}
