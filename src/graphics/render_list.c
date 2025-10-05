#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "utility/log.h"
#include "core/memory_manager.h"
#include "graphics/render_list.h"
#include "graphics/draw_list.h"

#define RENDER_LIST_INITIAL_CAPACITY 32
#define BATCH_INITIAL_CAPACITY 256

/**
 * @brief Iterator structure for traversing render batches.
 * 
 * @details This structure maintains the current position in the render list
 *          for iteration, tracking the current batch and vertex count within
 *          that batch for efficient batch processing.
 */
typedef struct EseRenderBatchIterator {
    EseRenderList *render_list;     /** Reference to the render list being iterated */
    size_t batch_index;             /** Current batch index */
    size_t vertex_index;            /** Current vertex index within the batch */
    size_t total_vertices;          /** Total vertices processed so far */
} EseRenderBatchIterator;

/**
 * @brief Manages a collection of render batches for GPU rendering.
 * 
 * @details This structure organizes drawable objects into batches by type
 *          and shared state for efficient GPU rendering. It manages memory
 *          allocation for batches and provides iteration capabilities.
 */
typedef struct EseRenderList {
    EseRenderBatch **batches;       /** Array of render batch pointers */
    size_t batch_count;             /** Number of batches in the list */
    size_t batch_capacity;          /** Allocated capacity for batches array */
    int width;                      /** Viewport width for coordinate conversion */
    int height;                     /** Viewport height for coordinate conversion */
} EseRenderList;

// Helper to create a new batch
static EseRenderBatch* _render_batch_create(void) {
    EseRenderBatch *batch = memory_manager.malloc(sizeof(EseRenderBatch), MMTAG_RENDERLIST);

    memset(batch, 0, sizeof(EseRenderBatch));
    batch->vertex_buffer = memory_manager.malloc(sizeof(EseVertex) * BATCH_INITIAL_CAPACITY * 4, MMTAG_RENDERLIST);
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
        EseRenderBatch **new_batches = memory_manager.realloc(render_list->batches, sizeof(EseRenderBatch*) * new_capacity, MMTAG_RENDERLIST);
        if (new_batches) {
            render_list->batches = new_batches;
            render_list->batch_capacity = new_capacity;
        }
    }
    render_list->batches[render_list->batch_count++] = batch;
}

static void _rotate_point(
    float lx, float ly,
    float px, float py,
    float rot,
    float *ox, float *oy
) {
    float dx = lx - px;
    float dy = ly - py;
    float cr = cosf(rot);
    float sr = sinf(rot);
    *ox = px + (cr * dx - sr * dy);
    *oy = py + (sr * dx + cr * dy);
}

static inline void _pixel_to_ndc(float px, float py, float view_w, float view_h, float *nx, float *ny) {
    *nx = (px / view_w) * 2.0f - 1.0f;
    *ny = 1.0f - (py / view_h) * 2.0f;
}


// Helper to tessellate a polyline into triangles for fill rendering
static void _tessellate_polyline_fill(
    const EseDrawListObject *obj,
    EseVertex *vertices,
    size_t *vertex_count,
    int view_w, int view_h
) {
    const float *points;
    size_t point_count;
    float stroke_width;
    draw_list_object_get_polyline(obj, &points, &point_count, &stroke_width);
    
    if (point_count < 3) return; // Need at least 3 points for a triangle
    
    // Get screen position from bounds
    float screen_x, screen_y;
    int w, h;
    draw_list_object_get_bounds(obj, &screen_x, &screen_y, &w, &h);
    
    // Check if the polygon is closed (last point equals first point)
    bool is_closed = (point_count > 3 && 
                     points[0] == points[(point_count-1)*2] && 
                     points[1] == points[(point_count-1)*2+1]);
    
    // Use simple, reliable fan triangulation from centroid
    // This is the standard approach that works for most shapes
    
    // Calculate centroid
    float centroid_x = 0.0f, centroid_y = 0.0f;
    for (size_t i = 0; i < point_count; i++) {
        centroid_x += points[i * 2];
        centroid_y += points[i * 2 + 1];
    }
    centroid_x /= point_count;
    centroid_y /= point_count;
    
    // Convert centroid to NDC
    float center_x, center_y;
    _pixel_to_ndc(centroid_x + screen_x, centroid_y + screen_y, view_w, view_h, &center_x, &center_y);
    
    // Triangulate the entire polygon with a simple fan around the centroid.
    // If the last point equals the first, treat it as a duplicate and ignore it in the fan.
    size_t limit = point_count;
    if (is_closed) {
        // Avoid degenerate triangle using duplicate last point
        limit = point_count - 1;
    }

    // Need at least a triangle
    if (limit < 3) return;

    for (size_t i = 0; i < limit; ++i) {
        size_t idx1 = i;
        size_t idx2 = (i + 1) % limit;

        float x1, y1, x2, y2;
        _pixel_to_ndc(points[idx1 * 2] + screen_x, points[idx1 * 2 + 1] + screen_y, view_w, view_h, &x1, &y1);
        _pixel_to_ndc(points[idx2 * 2] + screen_x, points[idx2 * 2 + 1] + screen_y, view_w, view_h, &x2, &y2);

        vertices[(*vertex_count)++] = (EseVertex){center_x, center_y, 0.0f, 0.0f, 0.0f};
        vertices[(*vertex_count)++] = (EseVertex){x1, y1, 0.0f, 0.0f, 0.0f};
        vertices[(*vertex_count)++] = (EseVertex){x2, y2, 0.0f, 0.0f, 0.0f};
    }
}

// Helper to tessellate a polyline into quads for stroke rendering
static void _tessellate_polyline_stroke(
    const EseDrawListObject *obj,
    EseVertex *vertices,
    size_t *vertex_count,
    int view_w, int view_h
) {
    const float *points;
    size_t point_count;
    float stroke_width;
    draw_list_object_get_polyline(obj, &points, &point_count, &stroke_width);
    
    if (point_count < 2) return; // Need at least 2 points for a line
    
    // Get screen position from bounds
    float screen_x, screen_y;
    int w, h;
    draw_list_object_get_bounds(obj, &screen_x, &screen_y, &w, &h);
    
    float half_width = stroke_width * 0.5f;
    
    // For closed polygons, if last point is duplicate of first, process (point_count - 1) lines
    // Otherwise process (point_count - 1) lines
    size_t line_count;
    if (point_count > 2 && points[0] == points[(point_count-1)*2] && points[1] == points[(point_count-1)*2+1]) {
        line_count = point_count - 1; // Last point is duplicate of first, so process one fewer line
    } else {
        line_count = point_count - 1; // Normal case
    }
    
    for (size_t i = 0; i < line_count; i++) {
        float x1, y1, x2, y2;
        _pixel_to_ndc(points[i * 2] + screen_x, points[i * 2 + 1] + screen_y, view_w, view_h, &x1, &y1);
        _pixel_to_ndc(points[(i + 1) * 2] + screen_x, points[(i + 1) * 2 + 1] + screen_y, view_w, view_h, &x2, &y2);
        
        // Calculate perpendicular vector for stroke width
        float dx = x2 - x1;
        float dy = y2 - y1;
        float length = sqrtf(dx * dx + dy * dy);
        
        if (length < 1e-6f) {
            continue; // Skip degenerate segments
        }
        
        // Normalize and get perpendicular
        dx /= length;
        dy /= length;
        float perp_x = -dy;
        float perp_y = dx;
        
        // Scale by half stroke width (convert to NDC)
        float stroke_ndc = (half_width / view_w) * 2.0f; // Approximate conversion
        perp_x *= stroke_ndc;
        perp_y *= stroke_ndc;
        
        // Create quad vertices
        float x1_left = x1 + perp_x;
        float y1_left = y1 + perp_y;
        float x1_right = x1 - perp_x;
        float y1_right = y1 - perp_y;
        float x2_left = x2 + perp_x;
        float y2_left = y2 + perp_y;
        float x2_right = x2 - perp_x;
        float y2_right = y2 - perp_y;
        
        // Add quad as two triangles
        vertices[(*vertex_count)++] = (EseVertex){x1_left, y1_left, 0.0f, 0.0f, 0.0f};
        vertices[(*vertex_count)++] = (EseVertex){x1_right, y1_right, 0.0f, 0.0f, 0.0f};
        vertices[(*vertex_count)++] = (EseVertex){x2_left, y2_left, 0.0f, 0.0f, 0.0f};
        
        vertices[(*vertex_count)++] = (EseVertex){x1_right, y1_right, 0.0f, 0.0f, 0.0f};
        vertices[(*vertex_count)++] = (EseVertex){x2_right, y2_right, 0.0f, 0.0f, 0.0f};
        vertices[(*vertex_count)++] = (EseVertex){x2_left, y2_left, 0.0f, 0.0f, 0.0f};
    }
    
}

// Helper to add polyline fill vertices directly to a batch
static void _render_batch_add_polyline_fill_vertices(EseRenderBatch *batch, const EseDrawListObject *obj, int view_w, int view_h) {
    log_assert("RENDER_LIST", batch, "_render_batch_add_polyline_fill_vertices called with NULL batch");
    log_assert("RENDER_LIST", obj, "_render_batch_add_polyline_fill_vertices called with NULL obj");

    const float *points;
    size_t point_count;
    float stroke_width;
    draw_list_object_get_polyline(obj, &points, &point_count, &stroke_width);
    
    if (point_count < 3) return; // Need at least 3 points for a triangle
    
    // Get screen position from bounds
    float screen_x, screen_y;
    int w, h;
    draw_list_object_get_bounds(obj, &screen_x, &screen_y, &w, &h);
    
    // Calculate triangle count
    size_t triangle_count = (point_count > 3 && points[0] == points[(point_count-1)*2] && points[1] == points[(point_count-1)*2+1]) 
                           ? point_count - 3  // Last point is duplicate of first
                           : point_count - 2; // Normal case
    
    size_t fill_vertices = triangle_count * 3;
    
    // Ensure we have enough capacity
    if (batch->vertex_count + fill_vertices > batch->vertex_capacity) {
        size_t new_capacity = batch->vertex_capacity * 2;
        while (batch->vertex_count + fill_vertices > new_capacity) new_capacity *= 2;
        EseVertex *new_buffer = memory_manager.realloc(batch->vertex_buffer, sizeof(EseVertex) * new_capacity, MMTAG_RENDERLIST);
        if (new_buffer) {
            batch->vertex_buffer = new_buffer;
            batch->vertex_capacity = new_capacity;
        }
    }
    
    EseVertex *v = &batch->vertex_buffer[batch->vertex_count];
    size_t vertex_offset = 0;
    
    // Tessellate fill vertices
    _tessellate_polyline_fill(obj, &v[vertex_offset], &vertex_offset, view_w, view_h);
    
    batch->vertex_count += vertex_offset;
}

// Helper to add polyline stroke vertices directly to a batch
static void _render_batch_add_polyline_stroke_vertices(EseRenderBatch *batch, const EseDrawListObject *obj, int view_w, int view_h) {
    log_assert("RENDER_LIST", batch, "_render_batch_add_polyline_stroke_vertices called with NULL batch");
    log_assert("RENDER_LIST", obj, "_render_batch_add_polyline_stroke_vertices called with NULL obj");

    const float *points;
    size_t point_count;
    float stroke_width;
    draw_list_object_get_polyline(obj, &points, &point_count, &stroke_width);
    
    if (point_count < 2) return; // Need at least 2 points for a line
    
    // Get screen position from bounds
    float screen_x, screen_y;
    int w, h;
    draw_list_object_get_bounds(obj, &screen_x, &screen_y, &w, &h);
    
    // Calculate line count
    size_t line_count = (point_count > 2 && points[0] == points[(point_count-1)*2] && points[1] == points[(point_count-1)*2+1]) 
                       ? point_count - 1  // Last point is duplicate of first
                       : point_count - 1; // Normal case
    
    size_t stroke_vertices = line_count * 6;
    
    // Ensure we have enough capacity
    if (batch->vertex_count + stroke_vertices > batch->vertex_capacity) {
        size_t new_capacity = batch->vertex_capacity * 2;
        while (batch->vertex_count + stroke_vertices > new_capacity) new_capacity *= 2;
        EseVertex *new_buffer = memory_manager.realloc(batch->vertex_buffer, sizeof(EseVertex) * new_capacity, MMTAG_RENDERLIST);
        if (new_buffer) {
            batch->vertex_buffer = new_buffer;
            batch->vertex_capacity = new_capacity;
        }
    }
    
    EseVertex *v = &batch->vertex_buffer[batch->vertex_count];
    size_t vertex_offset = 0;
    
    // Tessellate stroke vertices
    _tessellate_polyline_stroke(obj, &v[vertex_offset], &vertex_offset, view_w, view_h);
    
    batch->vertex_count += vertex_offset;
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
        EseVertex *new_buffer = memory_manager.realloc(batch->vertex_buffer, sizeof(EseVertex) * new_capacity, MMTAG_RENDERLIST);
        if (new_buffer) {
            batch->vertex_buffer = new_buffer;
            batch->vertex_capacity = new_capacity;
        }
    }

    EseVertex *v = &batch->vertex_buffer[batch->vertex_count];
    batch->vertex_count += 6;

    if (draw_list_object_get_type(obj) == DL_TEXTURE) {
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
        
    } else if (draw_list_object_get_type(obj) == DL_RECT) {
        unsigned char rc, gc, bc, ac;
        bool filled;
        draw_list_object_get_rect_color(obj, &rc, &gc, &bc, &ac, &filled);

        /* rotation (radians) and pivot normalized (0..1) */
        float rot = draw_list_object_get_rotation(obj);
        float pivot_nx = 0.5f, pivot_ny = 0.5f;
        draw_list_object_get_pivot(obj, &pivot_nx, &pivot_ny);

        /* Use pixel-space bounds from x,y,w,h obtained earlier */
        float sx = x;     /* top-left x in pixels */
        float sy = y;     /* top-left y in pixels */
        int pw = w;
        int ph = h;

        /* pivot in pixel coords */
        float px_pix = sx + pivot_nx * (float)pw;
        float py_pix = sy + pivot_ny * (float)ph;

        /* Outer corners in pixel space (TL, TR, BR, BL) - y grows downward */
        float oxp[4], oyp[4];
        oxp[0] = sx;         oyp[0] = sy;          /* TL */
        oxp[1] = sx + pw;    oyp[1] = sy;          /* TR */
        oxp[2] = sx + pw;    oyp[2] = sy + ph;     /* BR */
        oxp[3] = sx;         oyp[3] = sy + ph;     /* BL */

        /* Rotate outer corners in pixel space */
        float r_oxp[4], r_oyp[4];
        if (fabsf(rot) < 1e-6f) {
            for (int i = 0; i < 4; ++i) { r_oxp[i] = oxp[i]; r_oyp[i] = oyp[i]; }
        } else {
            for (int i = 0; i < 4; ++i) {
                _rotate_point(oxp[i], oyp[i], px_pix, py_pix, rot, &r_oxp[i], &r_oyp[i]);
            }
        }

        if (filled) {
            /* Convert rotated pixel points -> NDC and write two triangles */
            float ndc_px[4], ndc_py[4];
            for (int i = 0; i < 4; ++i) _pixel_to_ndc(r_oxp[i], r_oyp[i], view_w, view_h, &ndc_px[i], &ndc_py[i]);

            /* Triangles: TL, BL, BR  and  TL, BR, TR */
            v[0] = (EseVertex){ ndc_px[0], ndc_py[0], 0.0f, 0.0f, 0.0f }; /* TL */
            v[1] = (EseVertex){ ndc_px[3], ndc_py[3], 0.0f, 0.0f, 0.0f }; /* BL */
            v[2] = (EseVertex){ ndc_px[2], ndc_py[2], 0.0f, 0.0f, 0.0f }; /* BR */

            v[3] = (EseVertex){ ndc_px[0], ndc_py[0], 0.0f, 0.0f, 0.0f }; /* TL */
            v[4] = (EseVertex){ ndc_px[2], ndc_py[2], 0.0f, 0.0f, 0.0f }; /* BR */
            v[5] = (EseVertex){ ndc_px[1], ndc_py[1], 0.0f, 0.0f, 0.0f }; /* TR */
        } else {
            /* Hollow rectangle: create inner rect in pixels, rotate both outer and inner, convert to NDC. */
            const float border_px = 2.0f; /* keep same pixel thickness as before */
            float bx = border_px;
            float by = border_px;

            float inner_x = sx + bx;
            float inner_y = sy + by;
            float inner_w = (float)pw - 2.0f * bx;
            float inner_h = (float)ph - 2.0f * by;

            if (inner_w <= 0.0f || inner_h <= 0.0f) {
                /* fallback to filled if inner invalid */
                float ndc_px[4], ndc_py[4];
                for (int i = 0; i < 4; ++i) _pixel_to_ndc(r_oxp[i], r_oyp[i], view_w, view_h, &ndc_px[i], &ndc_py[i]);

                v[0] = (EseVertex){ ndc_px[0], ndc_py[0], 0.0f, 0.0f, 0.0f };
                v[1] = (EseVertex){ ndc_px[3], ndc_py[3], 0.0f, 0.0f, 0.0f };
                v[2] = (EseVertex){ ndc_px[2], ndc_py[2], 0.0f, 0.0f, 0.0f };

                v[3] = (EseVertex){ ndc_px[0], ndc_py[0], 0.0f, 0.0f, 0.0f };
                v[4] = (EseVertex){ ndc_px[2], ndc_py[2], 0.0f, 0.0f, 0.0f };
                v[5] = (EseVertex){ ndc_px[1], ndc_py[1], 0.0f, 0.0f, 0.0f };
            } else {
                /* inner corners in pixel space (TL, TR, BR, BL) */
                float ixp[4], iyp[4];
                ixp[0] = inner_x;           iyp[0] = inner_y;           /* TL */
                ixp[1] = inner_x + inner_w; iyp[1] = inner_y;           /* TR */
                ixp[2] = inner_x + inner_w; iyp[2] = inner_y + inner_h; /* BR */
                ixp[3] = inner_x;           iyp[3] = inner_y + inner_h; /* BL */

                /* rotate inner corners */
                float r_ixp[4], r_iyp[4];
                if (fabsf(rot) < 1e-6f) {
                    for (int i = 0; i < 4; ++i) { r_ixp[i] = ixp[i]; r_iyp[i] = iyp[i]; }
                } else {
                    for (int i = 0; i < 4; ++i) {
                        _rotate_point(ixp[i], iyp[i], px_pix, py_pix, rot, &r_ixp[i], &r_iyp[i]);
                    }
                }

                /* ensure vertex capacity (we already reserved 6 earlier) */
                if (batch->vertex_count - 6 + 24 > batch->vertex_capacity) {
                    size_t new_capacity = batch->vertex_capacity * 2;
                    while (batch->vertex_count - 6 + 24 > new_capacity) new_capacity *= 2;
                    EseVertex *new_buffer = memory_manager.realloc(batch->vertex_buffer, sizeof(EseVertex) * new_capacity, MMTAG_RENDERLIST);
                    if (new_buffer) {
                        batch->vertex_buffer = new_buffer;
                        batch->vertex_capacity = new_capacity;
                    }
                    v = &batch->vertex_buffer[batch->vertex_count - 6];
                }
                batch->vertex_count += 18; /* already reserved 6, add 18 more */

                /* convert rotated pixel points to NDC arrays */
                float odx[4], ody[4], idx[4], idy[4];
                for (int i = 0; i < 4; ++i) _pixel_to_ndc(r_oxp[i], r_oyp[i], view_w, view_h, &odx[i], &ody[i]);
                for (int i = 0; i < 4; ++i) _pixel_to_ndc(r_ixp[i], r_iyp[i], view_w, view_h, &idx[i], &idy[i]);

                /* Top border */
                v[0] = (EseVertex){ odx[0], ody[0], 0.0f, 0.0f, 0.0f };
                v[1] = (EseVertex){ idx[0], idy[0], 0.0f, 0.0f, 0.0f };
                v[2] = (EseVertex){ idx[1], idy[1], 0.0f, 0.0f, 0.0f };
                v[3] = (EseVertex){ odx[0], ody[0], 0.0f, 0.0f, 0.0f };
                v[4] = (EseVertex){ idx[1], idy[1], 0.0f, 0.0f, 0.0f };
                v[5] = (EseVertex){ odx[1], ody[1], 0.0f, 0.0f, 0.0f };

                /* Bottom border */
                v[6]  = (EseVertex){ odx[3], ody[3], 0.0f, 0.0f, 0.0f };
                v[7]  = (EseVertex){ odx[2], ody[2], 0.0f, 0.0f, 0.0f };
                v[8]  = (EseVertex){ idx[2], idy[2], 0.0f, 0.0f, 0.0f };
                v[9]  = (EseVertex){ odx[3], ody[3], 0.0f, 0.0f, 0.0f };
                v[10] = (EseVertex){ idx[2], idy[2], 0.0f, 0.0f, 0.0f };
                v[11] = (EseVertex){ idx[3], idy[3], 0.0f, 0.0f, 0.0f };

                /* Left border */
                v[12] = (EseVertex){ odx[0], ody[0], 0.0f, 0.0f, 0.0f };
                v[13] = (EseVertex){ odx[3], ody[3], 0.0f, 0.0f, 0.0f };
                v[14] = (EseVertex){ idx[3], idy[3], 0.0f, 0.0f, 0.0f };
                v[15] = (EseVertex){ odx[0], ody[0], 0.0f, 0.0f, 0.0f };
                v[16] = (EseVertex){ idx[3], idy[3], 0.0f, 0.0f, 0.0f };
                v[17] = (EseVertex){ idx[0], idy[0], 0.0f, 0.0f, 0.0f };

                /* Right border */
                v[18] = (EseVertex){ idx[1], idy[1], 0.0f, 0.0f, 0.0f };
                v[19] = (EseVertex){ idx[2], idy[2], 0.0f, 0.0f, 0.0f };
                v[20] = (EseVertex){ odx[2], ody[2], 0.0f, 0.0f, 0.0f };
                v[21] = (EseVertex){ idx[1], idy[1], 0.0f, 0.0f, 0.0f };
                v[22] = (EseVertex){ odx[2], ody[2], 0.0f, 0.0f, 0.0f };
                v[23] = (EseVertex){ odx[1], ody[1], 0.0f, 0.0f, 0.0f };
            }
        }
    } else if (draw_list_object_get_type(obj) == DL_POLYLINE) {
        const float *points;
        size_t point_count;
        float stroke_width;
        draw_list_object_get_polyline(obj, &points, &point_count, &stroke_width);
        
        unsigned char fill_r, fill_g, fill_b, fill_a;
        draw_list_object_get_polyline_color(obj, &fill_r, &fill_g, &fill_b, &fill_a);
        
        unsigned char stroke_r, stroke_g, stroke_b, stroke_a;
        draw_list_object_get_polyline_stroke_color(obj, &stroke_r, &stroke_g, &stroke_b, &stroke_a);
        
        
        
        // Calculate required vertex count
        size_t fill_vertices = 0;
        size_t stroke_vertices = 0;
        
        if (fill_a > 0 && point_count >= 3) {
            // For closed polygons, if last point is duplicate of first, use (point_count - 3) triangles
            // Otherwise use (point_count - 2) triangles
            size_t triangle_count = (point_count > 3 && points[0] == points[(point_count-1)*2] && points[1] == points[(point_count-1)*2+1]) 
                                   ? point_count - 3  // Last point is duplicate of first
                                   : point_count - 2; // Normal case
            fill_vertices = triangle_count * 3; // Fan triangulation
        }
        
        if (stroke_a > 0 && point_count >= 2) {
            // For closed polygons, if last point is duplicate of first, use (point_count - 1) lines
            // Otherwise use (point_count - 1) lines
            size_t line_count = (point_count > 2 && points[0] == points[(point_count-1)*2] && points[1] == points[(point_count-1)*2+1]) 
                               ? point_count - 1  // Last point is duplicate of first
                               : point_count - 1; // Normal case (same for stroke)
            stroke_vertices = line_count * 6; // Quads for stroke (6 vertices per line segment)
        }
        
        size_t total_vertices = fill_vertices + stroke_vertices;
        
        if (total_vertices == 0) return; // Nothing to render
        
        // Ensure we have enough capacity
        if (batch->vertex_count + total_vertices > batch->vertex_capacity) {
            size_t new_capacity = batch->vertex_capacity * 2;
            while (batch->vertex_count + total_vertices > new_capacity) new_capacity *= 2;
            EseVertex *new_buffer = memory_manager.realloc(batch->vertex_buffer, sizeof(EseVertex) * new_capacity, MMTAG_RENDERLIST);
            if (new_buffer) {
                batch->vertex_buffer = new_buffer;
                batch->vertex_capacity = new_capacity;
            }
        }
        
        EseVertex *v = &batch->vertex_buffer[batch->vertex_count];
        size_t vertex_offset = 0;
        
        // Add fill vertices if needed
        if (fill_vertices > 0) {
        _tessellate_polyline_fill(obj, &v[vertex_offset], &vertex_offset, view_w, view_h);
    }
    
    // Add stroke vertices if needed
    if (stroke_vertices > 0) {
        _tessellate_polyline_stroke(obj, &v[vertex_offset], &vertex_offset, view_w, view_h);
    }
        
        batch->vertex_count += vertex_offset;
    }
}

EseRenderList* render_list_create(void) {
    EseRenderList *render_list = memory_manager.malloc(sizeof(EseRenderList), MMTAG_RENDERLIST);

    render_list->batches = memory_manager.malloc(sizeof(EseRenderBatch*) * RENDER_LIST_INITIAL_CAPACITY, MMTAG_RENDERLIST);
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

        // Handle polyline objects - they need special batching logic
        if (draw_list_object_get_type(obj) == DL_POLYLINE) {
            const float *points;
            size_t point_count;
            float stroke_width;
            draw_list_object_get_polyline(obj, &points, &point_count, &stroke_width);
            
            unsigned char fill_r, fill_g, fill_b, fill_a;
            draw_list_object_get_polyline_color(obj, &fill_r, &fill_g, &fill_b, &fill_a);
            
            unsigned char stroke_r, stroke_g, stroke_b, stroke_a;
            draw_list_object_get_polyline_stroke_color(obj, &stroke_r, &stroke_g, &stroke_b, &stroke_a);
            
            // Calculate vertex counts
            size_t fill_vertices = 0;
            size_t stroke_vertices = 0;
            
            if (fill_a > 0 && point_count >= 3) {
                size_t triangle_count = (point_count > 3 && points[0] == points[(point_count-1)*2] && points[1] == points[(point_count-1)*2+1]) 
                                       ? point_count - 3  // Last point is duplicate of first
                                       : point_count - 2; // Normal case
                fill_vertices = triangle_count * 3;
            }
            
            if (stroke_a > 0 && point_count >= 2) {
                size_t line_count = (point_count > 2 && points[0] == points[(point_count-1)*2] && points[1] == points[(point_count-1)*2+1]) 
                                   ? point_count - 1  // Last point is duplicate of first
                                   : point_count - 1; // Normal case
                stroke_vertices = line_count * 6;
            }
            
            // Create fill batch if needed
            if (fill_vertices > 0) {
                bool new_fill_batch_needed = true;
                if (current_batch && current_batch->type == RL_COLOR) {
                    if (current_batch->shared_state.color.r == fill_r &&
                        current_batch->shared_state.color.g == fill_g &&
                        current_batch->shared_state.color.b == fill_b &&
                        current_batch->shared_state.color.a == fill_a &&
                        current_batch->shared_state.color.filled == true) {
                        new_fill_batch_needed = false;
                    }
                }
                
                if (new_fill_batch_needed) {
                    current_batch = _render_batch_create();
                    current_batch->type = RL_COLOR;
                    current_batch->shared_state.color.r = fill_r;
                    current_batch->shared_state.color.g = fill_g;
                    current_batch->shared_state.color.b = fill_b;
                    current_batch->shared_state.color.a = fill_a;
                    current_batch->shared_state.color.filled = true;
                    _render_list_add_batch(render_list, current_batch);
                }
                
                // Add fill vertices directly
                _render_batch_add_polyline_fill_vertices(current_batch, obj, render_list->width, render_list->height);
            }
            
            // Create stroke batch if needed
            if (stroke_vertices > 0) {
                bool new_stroke_batch_needed = true;
                if (current_batch && current_batch->type == RL_COLOR) {
                    if (current_batch->shared_state.color.r == stroke_r &&
                        current_batch->shared_state.color.g == stroke_g &&
                        current_batch->shared_state.color.b == stroke_b &&
                        current_batch->shared_state.color.a == stroke_a &&
                        current_batch->shared_state.color.filled == false) {
                        new_stroke_batch_needed = false;
                    }
                }
                
                if (new_stroke_batch_needed) {
                    current_batch = _render_batch_create();
                    current_batch->type = RL_COLOR;
                    current_batch->shared_state.color.r = stroke_r;
                    current_batch->shared_state.color.g = stroke_g;
                    current_batch->shared_state.color.b = stroke_b;
                    current_batch->shared_state.color.a = stroke_a;
                    current_batch->shared_state.color.filled = false;
                    _render_list_add_batch(render_list, current_batch);
                }
                
                // Add stroke vertices directly
                _render_batch_add_polyline_stroke_vertices(current_batch, obj, render_list->width, render_list->height);
            }
            
            continue; // Skip the normal batching logic below
        }

        // Check if a new batch is needed
        bool new_batch_needed = false;
        EseRenderListBatchType new_batch_type = RL_COLOR;
        if (draw_list_object_get_type(obj) == DL_TEXTURE) {
            new_batch_type = RL_TEXTURE;
        } else if (draw_list_object_get_type(obj) == DL_RECT) {
            new_batch_type = RL_COLOR;
        }

        if (!current_batch) {
            new_batch_needed = true;
        } else if (current_batch->type != new_batch_type) {
            new_batch_needed = true;
        } else if (new_batch_type == RL_TEXTURE) {
            const char *current_texture_id;
            draw_list_object_get_texture(obj, &current_texture_id, NULL, NULL, NULL, NULL);
            if (strcmp(current_texture_id, current_batch->shared_state.texture_id) != 0) {
                new_batch_needed = true;
            }
        } else if (new_batch_type == RL_COLOR) {
            // Compare rect colors to see if we can batch them together
            unsigned char r, g, b, a;
            bool filled;
            draw_list_object_get_rect_color(obj, &r, &g, &b, &a, &filled);
            
            if (current_batch->shared_state.color.r != r ||
                current_batch->shared_state.color.g != g ||
                current_batch->shared_state.color.b != b ||
                current_batch->shared_state.color.a != a ||
                current_batch->shared_state.color.filled != filled) {
                new_batch_needed = true;
            }
        }

        if (new_batch_needed) {
            // if (current_batch) {
            //     log_debug("RENDER_LIST", "Batch has %d draw calls", draw_calls);
            // }
            
            draw_calls = 0;
            current_batch = _render_batch_create();
            if (draw_list_object_get_type(obj) == DL_TEXTURE) {
                current_batch->type = RL_TEXTURE;
                draw_list_object_get_texture(obj, &current_batch->shared_state.texture_id, NULL, NULL, NULL, NULL);
            } else if (draw_list_object_get_type(obj) == DL_RECT) {
                current_batch->type = RL_COLOR;
                draw_list_object_get_rect_color(obj, 
                    &current_batch->shared_state.color.r, 
                    &current_batch->shared_state.color.g, 
                    &current_batch->shared_state.color.b, 
                    &current_batch->shared_state.color.a, 
                    &current_batch->shared_state.color.filled);
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
