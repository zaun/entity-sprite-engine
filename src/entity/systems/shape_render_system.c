/*
 * Project: Entity Sprite Engine
 *
 * Implementation of the Shape Render System. Collects shape components and
 * renders them to the draw list in the LATE phase, converting world coordinates
 * to screen coordinates using the camera.
 *
 * Details:
 * The system maintains a dynamic array of shape component pointers for
 * efficient rendering. Components are added/removed via callbacks. During
 * update, shapes are rendered with proper rotation, fill, and stroke support
 * based on polyline type.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#include "entity/systems/shape_render_system.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "core/system_manager.h"
#include "core/system_manager_private.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component_shape.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "types/color.h"
#include "types/point.h"
#include "types/poly_line.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <math.h>

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Internal data for the shape render system.
 *
 * Maintains a dynamically-sized array of shape component pointers for efficient
 * rendering during the LATE phase.
 */
typedef struct {
    EseEntityComponentShape **shapes; /** Array of shape component pointers */
    size_t count;                     /** Current number of tracked shapes */
    size_t capacity;                  /** Allocated capacity of the array */
} ShapeRenderSystemData;

// ========================================
// PRIVATE FUNCTIONS
// ========================================

/**
 * @brief Checks if the system accepts this component type.
 *
 * @param self System manager instance
 * @param comp Component to check
 * @return true if component type is ENTITY_COMPONENT_SHAPE
 */
static bool shape_render_sys_accepts(EseSystemManager *self, const EseEntityComponent *comp) {
    (void)self;
    if (!comp) {
        return false;
    }
    return comp->type == ENTITY_COMPONENT_SHAPE;
}

/**
 * @brief Called when a shape component is added to an entity.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param comp Component that was added
 */
static void shape_render_sys_on_add(EseSystemManager *self, EseEngine *eng,
                                    EseEntityComponent *comp) {
    (void)eng;
    ShapeRenderSystemData *d = (ShapeRenderSystemData *)self->data;

    // Expand array if needed
    if (d->count == d->capacity) {
        d->capacity = d->capacity ? d->capacity * 2 : 64;
        d->shapes = memory_manager.realloc(
            d->shapes, sizeof(EseEntityComponentShape *) * d->capacity, MMTAG_RS_SHAPE);
    }

    // Add shape to tracking array
    d->shapes[d->count++] = (EseEntityComponentShape *)comp->data;
}

/**
 * @brief Called when a shape component is removed from an entity.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param comp Component that was removed
 */
static void shape_render_sys_on_remove(EseSystemManager *self, EseEngine *eng,
                                       EseEntityComponent *comp) {
    (void)eng;
    ShapeRenderSystemData *d = (ShapeRenderSystemData *)self->data;
    EseEntityComponentShape *sp = (EseEntityComponentShape *)comp->data;

    // Find and remove shape from tracking array (swap with last element)
    for (size_t i = 0; i < d->count; i++) {
        if (d->shapes[i] == sp) {
            d->shapes[i] = d->shapes[--d->count];
            return;
        }
    }
}

/**
 * @brief Initialize the shape render system.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 */
static void shape_render_sys_init(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    ShapeRenderSystemData *d =
        memory_manager.calloc(1, sizeof(ShapeRenderSystemData), MMTAG_RS_SHAPE);
    d->shapes = NULL;
    d->count = 0;
    d->capacity = 0;
    self->data = d;
}

/**
 * @brief Convert degrees to radians.
 *
 * @param degrees Angle in degrees
 * @return Angle in radians
 */
static float _degrees_to_radians(float degrees) { return degrees * M_PI / 180.0f; }

/**
 * @brief Rotate a point around the origin.
 *
 * @param x X coordinate to rotate (in/out)
 * @param y Y coordinate to rotate (in/out)
 * @param angle_radians Rotation angle in radians
 */
static void _rotate_point(float *x, float *y, float angle_radians) {
    float cos_angle = cosf(angle_radians);
    float sin_angle = sinf(angle_radians);

    float new_x = *x * cos_angle - *y * sin_angle;
    float new_y = *x * sin_angle + *y * cos_angle;

    *x = new_x;
    *y = new_y;
}

/**
 * @brief Render all shapes.
 *
 * Iterates through all tracked shapes and submits them to the renderer.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param dt Delta time (unused)
 */
static void shape_render_sys_update(EseSystemManager *self, EseEngine *eng, float dt) {
    (void)dt;
    ShapeRenderSystemData *d = (ShapeRenderSystemData *)self->data;

    for (size_t i = 0; i < d->count; i++) {
        EseEntityComponentShape *shape = d->shapes[i];

        // Skip inactive entities
        if (!shape->base.entity || !shape->base.entity->active || !shape->base.entity->visible) {
            continue;
        }

        // Get entity world position
        float entity_x = ese_point_get_x(shape->base.entity->position);
        float entity_y = ese_point_get_y(shape->base.entity->position);

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

        // Get draw list for user data
        EseDrawList *draw_list = engine_get_draw_list(eng);

        // Render the shape directly
        profile_start(PROFILE_ENTITY_COMP_SHAPE_DRAW);

        // Convert rotation from degrees to radians
        float rotation_radians = _degrees_to_radians(shape->rotation);

        for (size_t idx = 0; idx < shape->polylines_count; ++idx) {
            EsePolyLine *polyline = shape->polylines[idx];
            if (!polyline)
                continue;

            size_t point_count = ese_poly_line_get_point_count(polyline);
            if (point_count < 2)
                continue;

            EsePolyLineType polyline_type = ese_poly_line_get_type(polyline);
            float stroke_width = ese_poly_line_get_stroke_width(polyline);

            float *points_to_use = NULL;
            size_t point_count_to_use = point_count;
            bool needs_cleanup = false;

            if ((polyline_type == POLY_LINE_CLOSED || polyline_type == POLY_LINE_FILLED) &&
                point_count >= 3) {
                points_to_use =
                    memory_manager.malloc(sizeof(float) * (point_count + 1) * 2, MMTAG_RS_SHAPE);
                if (points_to_use) {
                    const float *original_points = ese_poly_line_get_points(polyline);
                    for (size_t i = 0; i < point_count; i++) {
                        float x = original_points[i * 2];
                        float y = original_points[i * 2 + 1];
                        if (rotation_radians != 0.0f) {
                            _rotate_point(&x, &y, rotation_radians);
                        }
                        points_to_use[i * 2] = x;
                        points_to_use[i * 2 + 1] = y;
                    }
                    float x = original_points[0];
                    float y = original_points[1];
                    if (rotation_radians != 0.0f) {
                        _rotate_point(&x, &y, rotation_radians);
                    }
                    points_to_use[point_count * 2] = x;
                    points_to_use[point_count * 2 + 1] = y;
                    point_count_to_use = point_count + 1;
                    needs_cleanup = true;
                } else {
                    points_to_use =
                        memory_manager.malloc(sizeof(float) * point_count * 2, MMTAG_RS_SHAPE);
                    if (points_to_use) {
                        const float *original_points = ese_poly_line_get_points(polyline);
                        for (size_t i = 0; i < point_count; i++) {
                            float x = original_points[i * 2];
                            float y = original_points[i * 2 + 1];
                            if (rotation_radians != 0.0f) {
                                _rotate_point(&x, &y, rotation_radians);
                            }
                            points_to_use[i * 2] = x;
                            points_to_use[i * 2 + 1] = y;
                        }
                        needs_cleanup = true;
                    } else {
                        points_to_use = (float *)ese_poly_line_get_points(polyline);
                    }
                }
            } else {
                points_to_use =
                    memory_manager.malloc(sizeof(float) * point_count * 2, MMTAG_RS_SHAPE);
                if (points_to_use) {
                    const float *original_points = ese_poly_line_get_points(polyline);
                    for (size_t i = 0; i < point_count; i++) {
                        float x = original_points[i * 2];
                        float y = original_points[i * 2 + 1];
                        if (rotation_radians != 0.0f) {
                            _rotate_point(&x, &y, rotation_radians);
                        }
                        points_to_use[i * 2] = x;
                        points_to_use[i * 2 + 1] = y;
                    }
                    needs_cleanup = true;
                } else {
                    points_to_use = (float *)ese_poly_line_get_points(polyline);
                }
            }

            EseColor *fill_color = ese_poly_line_get_fill_color(polyline);
            EseColor *stroke_color = ese_poly_line_get_stroke_color(polyline);

            unsigned char fill_r =
                (unsigned char)(fill_color ? ese_color_get_r(fill_color) * 255 : 0);
            unsigned char fill_g =
                (unsigned char)(fill_color ? ese_color_get_g(fill_color) * 255 : 0);
            unsigned char fill_b =
                (unsigned char)(fill_color ? ese_color_get_b(fill_color) * 255 : 0);
            unsigned char fill_a =
                (unsigned char)(fill_color ? ese_color_get_a(fill_color) * 255 : 255);

            unsigned char stroke_r =
                (unsigned char)(stroke_color ? ese_color_get_r(stroke_color) * 255 : 0);
            unsigned char stroke_g =
                (unsigned char)(stroke_color ? ese_color_get_g(stroke_color) * 255 : 0);
            unsigned char stroke_b =
                (unsigned char)(stroke_color ? ese_color_get_b(stroke_color) * 255 : 0);
            unsigned char stroke_a =
                (unsigned char)(stroke_color ? ese_color_get_a(stroke_color) * 255 : 255);

            bool should_draw_fill = false;
            bool should_draw_stroke = false;

            switch (polyline_type) {
            case POLY_LINE_OPEN:
                should_draw_stroke = true;
                break;
            case POLY_LINE_CLOSED: {
                should_draw_stroke = true;
                // If a non-transparent fill color is set, fill closed paths,
                // too
                if (fill_color && ese_color_get_a(fill_color) > 0.0f) {
                    should_draw_fill = true;
                }
                break;
            }
            case POLY_LINE_FILLED:
                should_draw_fill = true;
                should_draw_stroke = true;
                break;
            }

            if (!should_draw_fill)
                fill_a = 0;
            if (!should_draw_stroke)
                stroke_a = 0;

            _engine_add_polyline_to_draw_list(
                screen_x, screen_y, 0, points_to_use, point_count_to_use, stroke_width, fill_r,
                fill_g, fill_b, fill_a, stroke_r, stroke_g, stroke_b, stroke_a, draw_list);

            if (needs_cleanup) {
                memory_manager.free(points_to_use);
            }
        }

        profile_stop(PROFILE_ENTITY_COMP_SHAPE_DRAW, "entity_component_shape_draw");
    }
}

/**
 * @brief Clean up the shape render system.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 */
static void shape_render_sys_shutdown(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    ShapeRenderSystemData *d = (ShapeRenderSystemData *)self->data;
    if (d) {
        if (d->shapes) {
            memory_manager.free(d->shapes);
        }
        memory_manager.free(d);
    }
}

/**
 * @brief Virtual table for the shape render system.
 */
static const EseSystemManagerVTable ShapeRenderSystemVTable = {
    .init = shape_render_sys_init,
    .update = shape_render_sys_update,
    .accepts = shape_render_sys_accepts,
    .on_component_added = shape_render_sys_on_add,
    .on_component_removed = shape_render_sys_on_remove,
    .shutdown = shape_render_sys_shutdown};

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Create a shape render system.
 *
 * @return EseSystemManager* Created system
 */
EseSystemManager *shape_render_system_create(void) {
    return system_manager_create(&ShapeRenderSystemVTable, SYS_PHASE_LATE, NULL);
}

/**
 * @brief Register the shape render system with the engine.
 *
 * @param eng Engine pointer
 */
void engine_register_shape_render_system(EseEngine *eng) {
    log_assert("SHAPE_RENDER_SYS", eng,
               "engine_register_shape_render_system called with NULL engine");
    EseSystemManager *sys = shape_render_system_create();
    engine_add_system(eng, sys);
}
