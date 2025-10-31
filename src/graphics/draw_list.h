/*
 * Project: Entity Sprite Engine
 *
 * Public API for the draw list: an object pool of renderable items and
 * utilities to construct frame render data.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef ESE_DRAW_LIST_H
#define ESE_DRAW_LIST_H

#include <stdbool.h>
#include <stdlib.h>

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Forward-declared draw list object type.
 */
typedef struct EseDrawListObject EseDrawListObject;

/**
 * @brief Forward-declared draw list container type.
 */
typedef struct EseDrawList EseDrawList;

/**
 * @brief Types of drawable objects that can be stored in the draw list.
 */
typedef enum EseDrawListObjectType {
    DL_TEXTURE,  /** Textured quad/sprite */
    DL_RECT,     /** Solid or outlined rectangle */
    DL_POLYLINE, /** Polyline made of N points */
    DL_MESH,     /** Indexed mesh with per-vertex color and UVs */
} EseDrawListObjectType;

/**
 * @brief Vertex format for DL_MESH objects.
 */
typedef struct EseDrawListVertex {
    float x, y;               /** Position in pixels */
    float u, v;               /** Texture UV coordinates */
    unsigned char r, g, b, a; /** Vertex color (RGBA 0-255) */
} EseDrawListVertex;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Create a new draw list with an internal object pool.
 *
 * @return Pointer to a new `EseDrawList` instance.
 */
EseDrawList *draw_list_create(void);

/**
 * @brief Destroy the draw list and free all associated memory.
 *
 * @param draw_list Draw list to destroy.
 */
void draw_list_destroy(EseDrawList *draw_list);

/**
 * @brief Reset the draw list for a new frame.
 *
 * Objects are reused (pooled) and not freed.
 *
 * @param draw_list Target draw list.
 */
void draw_list_clear(EseDrawList *draw_list);

/**
 * @brief Request a writable object for the current frame.
 *
 * The object is owned by the draw list and reused each frame.
 *
 * @param draw_list Target draw list.
 * @return Pointer to a writable `EseDrawListObject`.
 */
EseDrawListObject *draw_list_request_object(EseDrawList *draw_list);

/**
 * @brief Sort objects by their z-index (ascending).
 *
 * @param draw_list Target draw list.
 */
void draw_list_sort(EseDrawList *draw_list);

/**
 * @brief Get the number of active objects in the draw list.
 *
 * @param draw_list Target draw list.
 * @return Number of active objects.
 */
size_t draw_list_get_object_count(const EseDrawList *draw_list);

/**
 * @brief Get the i-th object in the draw list.
 *
 * @param draw_list Target draw list.
 * @param index Object index (0 <= index < count).
 * @return Pointer to the object, or NULL if out of range.
 */
EseDrawListObject *draw_list_get_object(const EseDrawList *draw_list, size_t index);

/**
 * @brief Ensure capacity for at least `count` more objects without changing count.
 *
 * Thread-safe. Grows the internal object pool so that
 * `objects_count + count <= objects_capacity`. Does not modify `objects_count`.
 *
 * @param draw_list Target draw list.
 * @param count Number of additional objects to ensure capacity for.
 * @return Current starting index (objects_count) on success, or (size_t)-1 on failure.
 */
size_t draw_list_reserve_count(EseDrawList *draw_list, size_t count);

/**
 * @brief Set texture properties on an object and switch its type to DL_TEXTURE.
 *
 * @param object Target object.
 * @param texture_id Texture identifier string.
 * @param texture_x1 Left UV.
 * @param texture_y1 Top UV.
 * @param texture_x2 Right UV.
 * @param texture_y2 Bottom UV.
 */
void draw_list_object_set_texture(EseDrawListObject *object, const char *texture_id,
                                  float texture_x1, float texture_y1, float texture_x2,
                                  float texture_y2);

/**
 * @brief Get texture properties from a DL_TEXTURE object.
 *
 * @param object Source object (must be DL_TEXTURE).
 * @param texture_id Out: texture ID pointer.
 * @param texture_x1 Out: left UV.
 * @param texture_y1 Out: top UV.
 * @param texture_x2 Out: right UV.
 * @param texture_y2 Out: bottom UV.
 */
void draw_list_object_get_texture(const EseDrawListObject *object, const char **texture_id,
                                  float *texture_x1, float *texture_y1, float *texture_x2,
                                  float *texture_y2);

/**
 * @brief Set rectangle color and fill; switches object type to DL_RECT.
 *
 * @param object Target object.
 * @param r Red [0-255].
 * @param g Green [0-255].
 * @param b Blue [0-255].
 * @param a Alpha [0-255].
 * @param filled True for filled rect, false for outline.
 */
void draw_list_object_set_rect_color(EseDrawListObject *object, unsigned char r, unsigned char g,
                                     unsigned char b, unsigned char a, bool filled);

/**
 * @brief Get rectangle color and fill from a DL_RECT object.
 *
 * @param object Source object (must be DL_RECT).
 * @param r Out: red.
 * @param g Out: green.
 * @param b Out: blue.
 * @param a Out: alpha.
 * @param filled Out: filled flag.
 */
void draw_list_object_get_rect_color(const EseDrawListObject *object, unsigned char *r,
                                     unsigned char *g, unsigned char *b, unsigned char *a,
                                     bool *filled);

/**
 * @brief Get the object type.
 *
 * @param object Target object.
 * @return Object type.
 */
EseDrawListObjectType draw_list_object_get_type(const EseDrawListObject *object);

/**
 * @brief Set object bounds (x, y, w, h).
 *
 * Applies to the active union member based on object type.
 *
 * @param object Target object.
 * @param x Left position in pixels.
 * @param y Top position in pixels.
 * @param w Width in pixels.
 * @param h Height in pixels.
 */
void draw_list_object_set_bounds(EseDrawListObject *object, float x, float y, int w, int h);

/**
 * @brief Get object bounds (x, y, w, h).
 *
 * For polylines, width/height are computed from points.
 *
 * @param object Source object.
 * @param x Out: left position (optional).
 * @param y Out: top position (optional).
 * @param w Out: width (optional).
 * @param h Out: height (optional).
 */
void draw_list_object_get_bounds(const EseDrawListObject *object, float *x, float *y, int *w,
                                 int *h);

/**
 * @brief Set the object's z-index.
 *
 * @param object Target object.
 * @param z_index Z value for sorting.
 */
void draw_list_object_set_z_index(EseDrawListObject *object, uint64_t z_index);

/**
 * @brief Get the object's z-index.
 *
 * @param object Target object.
 * @return Z value for sorting.
 */
uint64_t draw_list_object_get_z_index(const EseDrawListObject *object);

/**
 * @brief Set rotation in radians around the pivot point.
 *
 * @param object Target object.
 * @param radians Rotation in radians.
 */
void draw_list_object_set_rotation(EseDrawListObject *object, float radians);

/**
 * @brief Get rotation in radians.
 *
 * @param object Target object.
 * @return Rotation in radians.
 */
float draw_list_object_get_rotation(const EseDrawListObject *object);

/**
 * @brief Set pivot point for rotation in normalized coordinates [0..1].
 *
 * (0,0) is top-left, (0.5,0.5) is center, (1,1) is bottom-right.
 *
 * @param object Target object.
 * @param nx Normalized x pivot.
 * @param ny Normalized y pivot.
 */
void draw_list_object_set_pivot(EseDrawListObject *object, float nx, float ny);

/**
 * @brief Get pivot in normalized coordinates [0..1].
 *
 * @param object Target object.
 * @param nx Out: normalized x pivot (optional).
 * @param ny Out: normalized y pivot (optional).
 */
void draw_list_object_get_pivot(const EseDrawListObject *object, float *nx, float *ny);

/**
 * @brief Compute the axis-aligned bounding box containing the rotated object.
 *
 * @param object Source object.
 * @param minx Out: min x (optional).
 * @param miny Out: min y (optional).
 * @param maxx Out: max x (optional).
 * @param maxy Out: max y (optional).
 */
void draw_list_object_get_rotated_aabb(const EseDrawListObject *object, float *minx, float *miny,
                                       float *maxx, float *maxy);

/**
 * @brief Set polyline data and switch type to DL_POLYLINE.
 *
 * @param object Target object.
 * @param points Array of [x1,y1,x2,y2,...].
 * @param point_count Number of points.
 * @param stroke_width Stroke width in pixels.
 */
void draw_list_object_set_polyline(EseDrawListObject *object, const float *points,
                                   size_t point_count, float stroke_width);

/**
 * @brief Get polyline data from a DL_POLYLINE object.
 *
 * @param object Source object (must be DL_POLYLINE).
 * @param points Out: pointer to internal points (float*[x1,y1,...]).
 * @param point_count Out: number of points.
 * @param stroke_width Out: stroke width.
 */
void draw_list_object_get_polyline(const EseDrawListObject *object, const float **points,
                                   size_t *point_count, float *stroke_width);

/**
 * @brief Set fill color for a DL_POLYLINE object.
 *
 * @param object Target object (must be DL_POLYLINE).
 * @param r Red [0-255].
 * @param g Green [0-255].
 * @param b Blue [0-255].
 * @param a Alpha [0-255].
 */
void draw_list_object_set_polyline_color(EseDrawListObject *object, unsigned char r,
                                         unsigned char g, unsigned char b, unsigned char a);

/**
 * @brief Get fill color from a DL_POLYLINE object.
 *
 * @param object Source object (must be DL_POLYLINE).
 * @param r Out: red.
 * @param g Out: green.
 * @param b Out: blue.
 * @param a Out: alpha.
 */
void draw_list_object_get_polyline_color(const EseDrawListObject *object, unsigned char *r,
                                         unsigned char *g, unsigned char *b, unsigned char *a);

/**
 * @brief Set stroke color for a DL_POLYLINE object.
 *
 * @param object Target object (must be DL_POLYLINE).
 * @param r Red [0-255].
 * @param g Green [0-255].
 * @param b Blue [0-255].
 * @param a Alpha [0-255].
 */
void draw_list_object_set_polyline_stroke_color(EseDrawListObject *object, unsigned char r,
                                                unsigned char g, unsigned char b, unsigned char a);

/**
 * @brief Get stroke color from a DL_POLYLINE object.
 *
 * @param object Source object (must be DL_POLYLINE).
 * @param r Out: red.
 * @param g Out: green.
 * @param b Out: blue.
 * @param a Out: alpha.
 */
void draw_list_object_get_polyline_stroke_color(const EseDrawListObject *object, unsigned char *r,
                                                unsigned char *g, unsigned char *b,
                                                unsigned char *a);

/**
 * @brief Set mesh data and switch type to DL_MESH.
 *
 * @param object Target object.
 * @param verts Vertex array.
 * @param vert_count Number of vertices.
 * @param indices Index array (uint32_t).
 * @param idx_count Number of indices.
 * @param texture_id Texture identifier string.
 */
void draw_list_object_set_mesh(EseDrawListObject *object, EseDrawListVertex *verts,
                               size_t vert_count, uint32_t *indices, size_t idx_count,
                               const char *texture_id);

/**
 * @brief Get mesh data from a DL_MESH object.
 *
 * @param object Source object (must be DL_MESH).
 * @param verts Out: vertex array pointer.
 * @param vert_count Out: vertex count.
 * @param indices Out: index array pointer.
 * @param idx_count Out: index count.
 * @param texture_id Out: texture ID pointer.
 */
void draw_list_object_get_mesh(const EseDrawListObject *object, const EseDrawListVertex **verts,
                               size_t *vert_count, const uint32_t **indices, size_t *idx_count,
                               const char **texture_id);

/**
 * @brief Enable scissor and set scissor rectangle.
 *
 * @param object Target object.
 * @param scissor_x Left in pixels.
 * @param scissor_y Top in pixels.
 * @param scissor_w Width in pixels.
 * @param scissor_h Height in pixels.
 */
void draw_list_object_set_scissor(EseDrawListObject *object, float scissor_x, float scissor_y,
                                  float scissor_w, float scissor_h);

/**
 * @brief Get scissor settings from an object.
 *
 * @param object Source object.
 * @param scissor_active Out: whether scissor is active.
 * @param scissor_x Out: left.
 * @param scissor_y Out: top.
 * @param scissor_w Out: width.
 * @param scissor_h Out: height.
 */
void draw_list_object_get_scissor(const EseDrawListObject *object, bool *scissor_active,
                                  float *scissor_x, float *scissor_y, float *scissor_w,
                                  float *scissor_h);

/**
 * @brief Disable scissor on an object.
 *
 * @param object Target object.
 */
void draw_list_object_clear_scissor(EseDrawListObject *object);

#endif // ESE_DRAW_LIST_H