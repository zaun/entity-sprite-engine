#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "utility/log.h"
#include "core/memory_manager.h"
#include "graphics/render_list.h"

#define RENDER_LIST_INITIAL_CAPACITY 32
#define BATCH_INITIAL_CAPACITY 256

typedef struct EseRenderBatchIterator {
    EseRenderBatch *batch;
    size_t current_vertex_index;
    size_t total_vertices;
} EseRenderBatchIterator;

typedef struct EseRenderList {
    EseRenderBatch **batches;
    size_t batch_count;
    size_t batch_capacity;
    int width;
    int height;
} EseRenderList;

// Helper to create a new batch
static EseRenderBatch* _render_batch_create(void) {
    EseRenderBatch *batch = memory_manager.malloc(sizeof(EseRenderBatch), MMTAG_USER1);

    memset(batch, 0, sizeof(EseRenderBatch));
    batch->vertex_buffer = memory_manager.malloc(sizeof(EseVertex) * BATCH_INITIAL_CAPACITY * 4, MMTAG_USER1);
    batch->vertex_capacity = BATCH_INITIAL_CAPACITY * 4;

    return batch;
}

// Helper to free a batch
static void _render_batch_destroy(EseRenderBatch *batch) {
    log_assert("RENDER_LIST", batch, "_render_batch_destroy called with NULL batch");

    memory_manager.free(batch->vertex_buffer);
    memory_manager.free(batch);
}

// Helper to add a new batch to the draw list
static void _render_list_add_batch(EseRenderList *render_list, EseRenderBatch *batch) {
    log_assert("RENDER_LIST", render_list, "_render_list_add_batch called with NULL render_list");
    log_assert("RENDER_LIST", batch, "_render_list_add_batch called with NULL batch");

    if (render_list->batch_count >= render_list->batch_capacity) {
        // Grow draw list
        size_t new_capacity = render_list->batch_capacity * 2;
        EseRenderBatch **new_batches = memory_manager.realloc(render_list->batches, sizeof(EseRenderBatch*) * new_capacity, MMTAG_USER1);
        if (new_batches) {
            render_list->batches = new_batches;
            render_list->batch_capacity = new_capacity;
        }
    }
    render_list->batches[render_list->batch_count++] = batch;
}

// Helper to add vertices to a batch
static void _render_batch_add_object_vertices(EseRenderBatch *batch, const EseDrawListObject *obj, int view_w, int view_h) {
    log_assert("RENDER_LIST", batch, "_render_batch_add_object_vertices called with NULL batch");
    log_assert("RENDER_LIST", obj, "_render_batch_add_object_vertices called with NULL obj");

    float x, y;
    int w, h;
    draw_list_object_get_bounds(obj, &x, &y, &w, &h);

    // Convert screen coordinates to Normalized Device Coordinates (NDC)
    // NDC is a coordinate system where (0,0) is the center of the screen
    // x ranges from -1 to 1 (left to right), y ranges from -1 to 1 (bottom to top)
    float ndc_x = (2.0f * (x / view_w)) - 1.0f;
    float ndc_y = 1.0f - (2.0f * (y / view_h));
    float ndc_w = (2.0f * w / view_w);
    float ndc_h = (2.0f * h / view_h);

    if (batch->vertex_count + 6 > batch->vertex_capacity) { // Each quad has 6 vertices
        // Grow vertex buffer
        size_t new_capacity = batch->vertex_capacity * 2;
        EseVertex *new_buffer = memory_manager.realloc(batch->vertex_buffer, sizeof(EseVertex) * new_capacity, MMTAG_USER1);
        if (new_buffer) {
            batch->vertex_buffer = new_buffer;
            batch->vertex_capacity = new_capacity;
        }
    }

    EseVertex *v = &batch->vertex_buffer[batch->vertex_count];
    batch->vertex_count += 6;

    if (draw_list_object_get_type(obj) == RL_TEXTURE) {
        const char *texture_id;
        float sx1, sy1, sx2, sy2;
        draw_list_object_get_texture(obj, &texture_id, &sx1, &sy1, &sx2, &sy2);
        
        // Texture rect values are already normalized (0.0 - 1.0)
        float u0 = (float)sx1;
        float v0 = (float)sy1;  
        float u1 = (float)sx2;
        float v1 = (float)sy2;

        // Quad vertices (2 triangles = 6 vertices)
        // Triangle 1: top-left, bottom-left, bottom-right
        v[0] = (EseVertex){ndc_x, ndc_y, 0.0f, u0, v0};
        v[1] = (EseVertex){ndc_x, ndc_y - ndc_h, 0.0f, u0, v1};
        v[2] = (EseVertex){ndc_x + ndc_w, ndc_y - ndc_h, 0.0f, u1, v1};
        
        // Triangle 2: top-left, bottom-right, top-right  
        v[3] = (EseVertex){ndc_x, ndc_y, 0.0f, u0, v0};
        v[4] = (EseVertex){ndc_x + ndc_w, ndc_y - ndc_h, 0.0f, u1, v1};
        v[5] = (EseVertex){ndc_x + ndc_w, ndc_y, 0.0f, u1, v0};
        
    } else if (draw_list_object_get_type(obj) == RL_RECT) {
        unsigned char r, g, b, a;
        bool filled;
        draw_list_object_get_rect_color(obj, &r, &g, &b, &a, &filled);
        
        if (filled) {
            // Filled rectangle - single quad (6 vertices)
            v[0] = (EseVertex){ndc_x, ndc_y, 0.0f, 0.0f, 0.0f};
            v[1] = (EseVertex){ndc_x, ndc_y - ndc_h, 0.0f, 0.0f, 0.0f};
            v[2] = (EseVertex){ndc_x + ndc_w, ndc_y - ndc_h, 0.0f, 0.0f, 0.0f};
            
            v[3] = (EseVertex){ndc_x, ndc_y, 0.0f, 0.0f, 0.0f};
            v[4] = (EseVertex){ndc_x + ndc_w, ndc_y - ndc_h, 0.0f, 0.0f, 0.0f};
            v[5] = (EseVertex){ndc_x + ndc_w, ndc_y, 0.0f, 0.0f, 0.0f};
        } else {
            // Hollow rectangle - need to reallocate for 24 vertices (4 borders * 6 vertices each)
            if (batch->vertex_count - 6 + 24 > batch->vertex_capacity) {
                // We already reserved 6, but need 24 total, so check for 18 more
                size_t new_capacity = batch->vertex_capacity * 2;
                while (batch->vertex_count - 6 + 24 > new_capacity) {
                    new_capacity *= 2;
                }
                EseVertex *new_buffer = memory_manager.realloc(batch->vertex_buffer, sizeof(EseVertex) * new_capacity, MMTAG_USER1);
                if (new_buffer) {
                    batch->vertex_buffer = new_buffer;
                    batch->vertex_capacity = new_capacity;
                }
                // Update v pointer after potential reallocation
                v = &batch->vertex_buffer[batch->vertex_count - 6];
            }
            
            // Adjust vertex count for hollow rectangle (24 vertices instead of 6)
            batch->vertex_count += 18; // We already added 6, so add 18 more for total of 24
            
            // Define border thickness in NDC
            float border_thickness_ndc_w = (2.0f * 2.0f) / view_w;
            float border_thickness_ndc_h = (2.0f * 2.0f) / view_h;
            
            // Calculate inner rectangle bounds in NDC
            float inner_ndc_x = ndc_x + border_thickness_ndc_w;
            float inner_ndc_y = ndc_y - border_thickness_ndc_h;
            float inner_ndc_w = ndc_w - 2 * border_thickness_ndc_w;
            float inner_ndc_h = ndc_h - 2 * border_thickness_ndc_h;
            
            // Ensure inner rectangle is valid
            if (inner_ndc_w <= 0 || inner_ndc_h <= 0) {
                // If inner rectangle would be invalid, draw as filled
                batch->vertex_count -= 18; // Reset vertex count back to 6
                v[0] = (EseVertex){ndc_x, ndc_y, 0.0f, 0.0f, 0.0f};
                v[1] = (EseVertex){ndc_x, ndc_y - ndc_h, 0.0f, 0.0f, 0.0f};
                v[2] = (EseVertex){ndc_x + ndc_w, ndc_y - ndc_h, 0.0f, 0.0f, 0.0f};
                v[3] = (EseVertex){ndc_x, ndc_y, 0.0f, 0.0f, 0.0f};
                v[4] = (EseVertex){ndc_x + ndc_w, ndc_y - ndc_h, 0.0f, 0.0f, 0.0f};
                v[5] = (EseVertex){ndc_x + ndc_w, ndc_y, 0.0f, 0.0f, 0.0f};
            } else {
                // Top border (6 vertices)
                v[0] = (EseVertex){ndc_x, ndc_y, 0.0f, 0.0f, 0.0f};
                v[1] = (EseVertex){ndc_x, inner_ndc_y, 0.0f, 0.0f, 0.0f};
                v[2] = (EseVertex){ndc_x + ndc_w, inner_ndc_y, 0.0f, 0.0f, 0.0f};
                v[3] = (EseVertex){ndc_x, ndc_y, 0.0f, 0.0f, 0.0f};
                v[4] = (EseVertex){ndc_x + ndc_w, inner_ndc_y, 0.0f, 0.0f, 0.0f};
                v[5] = (EseVertex){ndc_x + ndc_w, ndc_y, 0.0f, 0.0f, 0.0f};
                
                // Bottom border (6 vertices)
                v[6] = (EseVertex){ndc_x, inner_ndc_y - inner_ndc_h, 0.0f, 0.0f, 0.0f};
                v[7] = (EseVertex){ndc_x, ndc_y - ndc_h, 0.0f, 0.0f, 0.0f};
                v[8] = (EseVertex){ndc_x + ndc_w, ndc_y - ndc_h, 0.0f, 0.0f, 0.0f};
                v[9] = (EseVertex){ndc_x, inner_ndc_y - inner_ndc_h, 0.0f, 0.0f, 0.0f};
                v[10] = (EseVertex){ndc_x + ndc_w, ndc_y - ndc_h, 0.0f, 0.0f, 0.0f};
                v[11] = (EseVertex){ndc_x + ndc_w, inner_ndc_y - inner_ndc_h, 0.0f, 0.0f, 0.0f};
                
                // Left border (6 vertices)
                v[12] = (EseVertex){ndc_x, inner_ndc_y, 0.0f, 0.0f, 0.0f};
                v[13] = (EseVertex){ndc_x, inner_ndc_y - inner_ndc_h, 0.0f, 0.0f, 0.0f};
                v[14] = (EseVertex){inner_ndc_x, inner_ndc_y - inner_ndc_h, 0.0f, 0.0f, 0.0f};
                v[15] = (EseVertex){ndc_x, inner_ndc_y, 0.0f, 0.0f, 0.0f};
                v[16] = (EseVertex){inner_ndc_x, inner_ndc_y - inner_ndc_h, 0.0f, 0.0f, 0.0f};
                v[17] = (EseVertex){inner_ndc_x, inner_ndc_y, 0.0f, 0.0f, 0.0f};
                
                // Right border (6 vertices)
                v[18] = (EseVertex){inner_ndc_x + inner_ndc_w, inner_ndc_y, 0.0f, 0.0f, 0.0f};
                v[19] = (EseVertex){inner_ndc_x + inner_ndc_w, inner_ndc_y - inner_ndc_h, 0.0f, 0.0f, 0.0f};
                v[20] = (EseVertex){ndc_x + ndc_w, inner_ndc_y - inner_ndc_h, 0.0f, 0.0f, 0.0f};
                v[21] = (EseVertex){inner_ndc_x + inner_ndc_w, inner_ndc_y, 0.0f, 0.0f, 0.0f};
                v[22] = (EseVertex){ndc_x + ndc_w, inner_ndc_y - inner_ndc_h, 0.0f, 0.0f, 0.0f};
                v[23] = (EseVertex){ndc_x + ndc_w, ndc_y, 0.0f, 0.0f, 0.0f};
            }
        }
    }
}

EseRenderList* render_list_create(void) {
    EseRenderList *render_list = memory_manager.malloc(sizeof(EseRenderList), MMTAG_USER1);

    render_list->batches = memory_manager.malloc(sizeof(EseRenderBatch*) * RENDER_LIST_INITIAL_CAPACITY, MMTAG_USER1);
    render_list->batch_capacity = RENDER_LIST_INITIAL_CAPACITY;
    render_list->batch_count = 0;

    return render_list;
}

void render_list_destroy(EseRenderList *render_list) {
    log_assert("RENDER_LIST", render_list, "render_list_destroy called with NULL render_list");

    for (size_t i = 0; i < render_list->batch_count; ++i) {
        _render_batch_destroy(render_list->batches[i]);
    }

    memory_manager.free(render_list->batches);
    memory_manager.free(render_list);
}

void render_list_set_size(EseRenderList *render_list, int width, int height) {
    log_assert("RENDER_LIST", render_list, "render_list_destroy called with NULL render_list");
    render_list->width = width;
    render_list->height = height;
}

void render_list_clear(EseRenderList *render_list) {
    log_assert("RENDER_LIST", render_list, "render_list_clear called with NULL render_list");

    // Actually destroy and free all batches
    for (size_t i = 0; i < render_list->batch_count; ++i) {
        _render_batch_destroy(render_list->batches[i]);
    }

    render_list->batch_count = 0;
}

void render_list_fill(EseRenderList *render_list, EseDrawList *draw_list) {
    log_assert("RENDER_LIST", render_list, "render_list_fill called with NULL render_list");
    log_assert("RENDER_LIST", draw_list, "render_list_fill called with NULL draw_list");

    // Ensure the draw_list is sorted by Z-index
    draw_list_sort(draw_list);

    // Iterate through the sorted draw_list to create batches
    EseRenderBatch *current_batch = NULL;
    int draw_calls = 0;
    for (size_t i = 0; i < draw_list_get_object_count(draw_list); ++i) {
        EseDrawListObject *obj = draw_list_get_object(draw_list, i);
        draw_calls += 1;

        // Check if a new batch is needed
        bool new_batch_needed = false;

        if (!current_batch) {
            new_batch_needed = true;
        } else if (current_batch->type != draw_list_object_get_type(obj)) {
            new_batch_needed = true;
        } else if (draw_list_object_get_type(obj) == RL_TEXTURE) {
            const char *current_texture_id;
            draw_list_object_get_texture(obj, &current_texture_id, NULL, NULL, NULL, NULL);
            if (strcmp(current_texture_id, current_batch->shared_state.texture_id) != 0) {
                new_batch_needed = true;
            }
        } else if (draw_list_object_get_type(obj) == RL_RECT) {
            // Compare rect colors to see if we can batch them together
            unsigned char r, g, b, a;
            bool filled;
            draw_list_object_get_rect_color(obj, &r, &g, &b, &a, &filled);
            
            if (current_batch->shared_state.rect_color.r != r ||
                current_batch->shared_state.rect_color.g != g ||
                current_batch->shared_state.rect_color.b != b ||
                current_batch->shared_state.rect_color.a != a ||
                current_batch->shared_state.rect_color.filled != filled) {
                new_batch_needed = true;
            }
        }

        if (new_batch_needed) {
            // if (current_batch) {
            //     log_debug("RENDER_LIST", "Batch has %d draw calls", draw_calls);
            // }
            current_batch = _render_batch_create();
            current_batch->type = draw_list_object_get_type(obj);
            draw_calls = 0;
            
            if (draw_list_object_get_type(obj) == RL_TEXTURE) {
                draw_list_object_get_texture(obj, &current_batch->shared_state.texture_id, NULL, NULL, NULL, NULL);
            } else if (draw_list_object_get_type(obj) == RL_RECT) {
                draw_list_object_get_rect_color(obj, 
                    &current_batch->shared_state.rect_color.r, 
                    &current_batch->shared_state.rect_color.g, 
                    &current_batch->shared_state.rect_color.b, 
                    &current_batch->shared_state.rect_color.a, 
                    &current_batch->shared_state.rect_color.filled);
            }
            _render_list_add_batch(render_list, current_batch);
        }

        // Add the object's vertex data to the current batch
        _render_batch_add_object_vertices(current_batch, obj, render_list->width, render_list->height);
    }
    // log_debug("RENDER_LIST", "Batch has %d draw calls", draw_calls);
}

size_t render_list_get_batch_count(const EseRenderList *render_list) {
    log_assert("RENDER_LIST", render_list, "render_list_get_batch_count called with NULL render_list");

    return render_list->batch_count;
}

const EseRenderBatch *render_list_get_batch(const EseRenderList *render_list, size_t batch_number) {
    log_assert("RENDER_LIST", render_list, "render_list_get_batch_count called with NULL render_list");
    log_assert("RENDER_LIST", render_list, "render_list_get_batch_count called with batch_number %zu out of bounds (max %zu)", batch_number, render_list->batch_count - 1);

    return render_list->batches[batch_number];
}
