#ifndef ESE_RENDER_LIST_H
#define ESE_RENDER_LIST_H

#include <stdbool.h>
#include "draw_list.h"

// Forward declarations
typedef struct EseRenderList EseRenderList;
typedef struct EseRenderBatchIterator EseRenderBatchIterator;

/**
 * @brief Represents a vertex in 3D space with texture coordinates.
 * 
 * @details This structure stores the position (x, y, z) and texture
 *          coordinates (u, v) for a single vertex in the render pipeline.
 *          Used for building vertex buffers for GPU rendering.
 */
typedef struct {
    float x, y, z;                  /**< 3D position coordinates */
    float u, v;                     /**< Texture coordinates (normalized) */
} EseVertex;

/**
 * @brief Represents a batch of renderable objects with shared state.
 * 
 * @details This structure groups objects of the same type (texture or rectangle)
 *          that share common properties. It stores the shared state, vertex buffer,
 *          and manages memory allocation for efficient GPU rendering.
 */
typedef struct EseRenderBatch {
    EseDrawListObjectType type;     /**< Type of objects in this batch */
    union {
        const char *texture_id;     /**< Texture ID for texture batches */
        /**
         * @brief Color and fill information for rectangle batches.
         * 
         * @details This structure stores the RGBA color components and fill
         *          style for batches of rectangular objects.
         */
        struct {
            unsigned char r, g, b, a; /**< RGBA color for rectangle batches */
            bool filled;             /**< Whether rectangles are filled or outlined */
        } rect_color;               /**< Color data for rectangle batches */
    } shared_state;                 /**< State shared by all objects in the batch */

    EseVertex *vertex_buffer;       /**< Buffer containing vertex data */
    size_t vertex_count;            /**< Number of vertices currently stored */
    size_t vertex_capacity;         /**< Allocated capacity for vertex buffer */
} EseRenderBatch;


EseRenderList* render_list_create(void);
void render_list_destroy(EseRenderList *render_list);
void render_list_set_size(EseRenderList *render_list, int width, int height);
void render_list_clear(EseRenderList *render_list);
void render_list_fill(EseRenderList *render_list, EseDrawList *draw_list);
size_t render_list_get_batch_count(const EseRenderList *render_list);
const EseRenderBatch *render_list_get_batch(const EseRenderList *render_list, size_t batch_number);

#endif // ESE_RENDER_LIST_H
