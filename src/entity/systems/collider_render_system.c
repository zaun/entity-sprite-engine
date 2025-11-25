/*
 * Project: Entity Sprite Engine
 *
 * Implementation of the Collider Render System. Collects collider components
 * and renders them to the draw list in the LATE phase, converting world
 * coordinates to screen coordinates using the camera.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#include "entity/systems/collider_render_system.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "core/system_manager.h"
#include "core/system_manager_private.h"
#include "entity/components/collider.h"
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
 * @brief Internal data for the collider render system.
 *
 * Maintains a dynamically-sized array of collider component pointers for
 * efficient rendering during the LATE phase.
 */
typedef struct {
    EseEntityComponentCollider **colliders; /** Array of collider component pointers */
    size_t count;                           /** Current number of tracked colliders */
    size_t capacity;                        /** Allocated capacity of the array */
} ColliderRenderSystemData;

// ========================================
// FORWARD DECLARATIONS
// ========================================

static void _collider_rect_callback(float screen_x, float screen_y, uint64_t z_index, float width,
                                    float height, float rotation, bool filled, uint8_t r, uint8_t g,
                                    uint8_t b, uint8_t a, void *user_data);

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
static bool collider_render_sys_accepts(EseSystemManager *self, const EseEntityComponent *comp) {
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
static void collider_render_sys_on_add(EseSystemManager *self, EseEngine *eng,
                                       EseEntityComponent *comp) {
    (void)eng;
    ColliderRenderSystemData *d = (ColliderRenderSystemData *)self->data;

    // Expand array if needed
    if (d->count == d->capacity) {
        d->capacity = d->capacity ? d->capacity * 2 : 64;
        d->colliders = memory_manager.realloc(
            d->colliders, sizeof(EseEntityComponentCollider *) * d->capacity, MMTAG_RS_COLLIDER);
    }

    // Add collider to tracking array
    d->colliders[d->count++] = (EseEntityComponentCollider *)comp->data;
}

/**
 * @brief Called when a collider component is removed from an entity.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param comp Component that was removed
 */
static void collider_render_sys_on_remove(EseSystemManager *self, EseEngine *eng,
                                          EseEntityComponent *comp) {
    (void)eng;
    ColliderRenderSystemData *d = (ColliderRenderSystemData *)self->data;
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
 * @brief Initialize the collider render system.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 */
static void collider_render_sys_init(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    ColliderRenderSystemData *d =
        memory_manager.calloc(1, sizeof(ColliderRenderSystemData), MMTAG_RS_COLLIDER);
    self->data = d;
}

/**
 * @brief Render all collider components.
 *
 * Iterates through all tracked collider components and submits them to the
 * renderer.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param dt Delta time (unused)
 */
static EseSystemJobResult collider_render_sys_update(EseSystemManager *self, EseEngine *eng,
                                                      float dt) {
    (void)dt;
    ColliderRenderSystemData *d = (ColliderRenderSystemData *)self->data;

    for (size_t i = 0; i < d->count; i++) {
        EseEntityComponentCollider *cc = d->colliders[i];

        // Skip colliders without debug drawing or inactive entities
        if (!cc->draw_debug || !cc->base.entity || !cc->base.entity->active ||
            !cc->base.entity->visible) {
            continue;
        }

        // Get entity world position
        float entity_x = ese_point_get_x(cc->base.entity->position);
        float entity_y = ese_point_get_y(cc->base.entity->position);

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

        // Render all collider rectangles
        for (size_t j = 0; j < cc->rects_count; j++) {
            EseRect *rect = cc->rects[j];
            _collider_rect_callback(screen_x + ese_point_get_x(cc->offset) + ese_rect_get_x(rect),
                                    screen_y + ese_point_get_y(cc->offset) + ese_rect_get_y(rect),
                                    cc->base.entity->draw_order, ese_rect_get_width(rect),
                                    ese_rect_get_height(rect), ese_rect_get_rotation(rect), false,
                                    0, 0, 255, 255, engine_get_draw_list(eng));
        }
    }

    EseSystemJobResult res = {0};
    return res;
}

/**
 * @brief Clean up the collider render system.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 */
static void collider_render_sys_shutdown(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    ColliderRenderSystemData *d = (ColliderRenderSystemData *)self->data;
    if (d) {
        if (d->colliders) {
            memory_manager.free(d->colliders);
        }
        memory_manager.free(d);
    }
}

/**
 * @brief Callback function for collider rectangle rendering.
 *
 * @param screen_x Screen X position
 * @param screen_y Screen Y position
 * @param z_index Z index for draw order
 * @param width Rectangle width
 * @param height Rectangle height
 * @param rotation Rectangle rotation
 * @param filled Whether rectangle is filled
 * @param r Red color component
 * @param g Green color component
 * @param b Blue color component
 * @param a Alpha color component
 * @param user_data Draw list pointer
 */
static void _collider_rect_callback(float screen_x, float screen_y, uint64_t z_index, float width,
                                    float height, float rotation, bool filled, uint8_t r, uint8_t g,
                                    uint8_t b, uint8_t a, void *user_data) {
    EseDrawList *draw_list = (EseDrawList *)user_data;
    EseDrawListObject *rect_obj = draw_list_request_object(draw_list);
    draw_list_object_set_bounds(rect_obj, screen_x, screen_y, (int)width, (int)height);
    draw_list_object_set_rect_color(rect_obj, r, g, b, a, filled);
    draw_list_object_set_z_index(rect_obj, z_index);
    if (rotation != 0.0f) {
        draw_list_object_set_rotation(rect_obj, rotation);
    }
}

/**
 * @brief Virtual table for the collider render system.
 */
static const EseSystemManagerVTable ColliderRenderSystemVTable = {
    .init = collider_render_sys_init,
    .update = collider_render_sys_update,
    .accepts = collider_render_sys_accepts,
    .on_component_added = collider_render_sys_on_add,
    .on_component_removed = collider_render_sys_on_remove,
    .shutdown = collider_render_sys_shutdown};

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Create a collider render system.
 *
 * @return EseSystemManager* Created system
 */
EseSystemManager *collider_render_system_create(void) {
    return system_manager_create(&ColliderRenderSystemVTable, SYS_PHASE_LATE, NULL);
}

/**
 * @brief Register the collider render system with the engine.
 *
 * @param eng Engine pointer
 */
void engine_register_collider_render_system(EseEngine *eng) {
    log_assert("COLLIDER_RENDER_SYS", eng,
               "engine_register_collider_render_system called with NULL engine");

    EseSystemManager *sys = collider_render_system_create();
    engine_add_system(eng, sys);
}
