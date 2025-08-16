#ifndef ESE_RENDER_LIST_H
#define ESE_RENDER_LIST_H

#include <stdbool.h>
#include "draw_list.h"

// Forward declarations
typedef struct EseRenderList EseRenderList;
typedef struct EseRenderBatchIterator EseRenderBatchIterator;

typedef struct {
    float x, y, z;              // Position
    float u, v;                 // Texture coordinates
} EseVertex;

typedef struct EseRenderBatch {
    EseDrawListObjectType type;
    union {
        const char *texture_id;
        struct {
            unsigned char r, g, b, a;
            bool filled;
        } rect_color;
    } shared_state;

    EseVertex *vertex_buffer;
    size_t vertex_count;
    size_t vertex_capacity;
} EseRenderBatch;


EseRenderList* render_list_create(void);
void render_list_destroy(EseRenderList *render_list);
void render_list_set_size(EseRenderList *render_list, int width, int height);
void render_list_clear(EseRenderList *render_list);
void render_list_fill(EseRenderList *render_list, EseDrawList *draw_list);
size_t render_list_get_batch_count(const EseRenderList *render_list);
const EseRenderBatch *render_list_get_batch(const EseRenderList *render_list, size_t batch_number);

#endif // ESE_RENDER_LIST_H
