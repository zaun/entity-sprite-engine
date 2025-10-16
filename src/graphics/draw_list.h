#ifndef ESE_DRAW_LIST_H
#define ESE_DRAW_LIST_H

#include <stdbool.h>
#include <stdlib.h>

typedef struct EseDrawListObject EseDrawListObject;
typedef struct EseDrawList EseDrawList;

typedef enum EseDrawListObjectType {
    DL_TEXTURE,
    DL_RECT,
    DL_POLYLINE,
    DL_MESH,
} EseDrawListObjectType;

typedef struct EseDrawListVertex {
    float x, y;
    float u, v;
    unsigned char r, g, b, a;
} EseDrawListVertex;

// Create a new, empty draw_list (with internal object pool)
EseDrawList* draw_list_create(void);

// Free the draw_list and all its objects
void draw_list_destroy(EseDrawList *draw_list);

// Reset the list for a new frame (objects are reused, not freed)
void draw_list_clear(EseDrawList *draw_list);

// Request a writable object for this frame (owned by the list)
EseDrawListObject* draw_list_request_object(EseDrawList *draw_list);

// Sort objects by z-index
void draw_list_sort(EseDrawList *draw_list);

// Get the number of objects in the list
size_t draw_list_get_object_count(const EseDrawList *draw_list);

// Get the i-th object (0 <= i < count)
EseDrawListObject* draw_list_get_object(const EseDrawList *draw_list, size_t index);

void draw_list_object_set_texture(
    EseDrawListObject* object, const char *texture_id,
    float texture_x1, float texture_y1, float texture_x2, float texture_y2
);

void draw_list_object_get_texture(
    const EseDrawListObject* object, const char **texture_id,
    float *texture_x1, float *texture_y1, float *texture_x2, float *texture_y2
);

void draw_list_object_set_rect_color(
    EseDrawListObject* object,
    unsigned char r, unsigned char g, unsigned char b, unsigned char a,
    bool filled
);

void draw_list_object_get_rect_color(
    const EseDrawListObject* object,
    unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a,
    bool* filled)
;

EseDrawListObjectType draw_list_object_get_type(const EseDrawListObject* object);

void draw_list_object_set_bounds(EseDrawListObject* object, float x, float y, int w, int h);

void draw_list_object_get_bounds(const EseDrawListObject* object, float* x, float* y, int* w, int* h);

void draw_list_object_set_z_index(EseDrawListObject* object, uint64_t z_index);

uint64_t draw_list_object_get_z_index(const EseDrawListObject* object);

/* Set rotation in radians (rotation around pivot point) */
void draw_list_object_set_rotation(EseDrawListObject* object, float radians);

/* Get rotation in radians */
float draw_list_object_get_rotation(const EseDrawListObject* object);

/* Set pivot point for rotation in normalized coordinates [0..1].
   (0,0) is top-left, (0.5,0.5) is center, (1,1) is bottom-right */
void draw_list_object_set_pivot(EseDrawListObject* object, float nx, float ny);

/* Get pivot normalized coordinates [0..1] */
void draw_list_object_get_pivot(const EseDrawListObject* object, float *nx, float *ny);

/* Compute the axis-aligned bounding box that contains the rotated object.
   Outputs minx, miny, maxx, maxy (all optional pointers). */
void draw_list_object_get_rotated_aabb(const EseDrawListObject* object, float *minx, float *miny, float *maxx, float *maxy);

void draw_list_object_set_polyline(
    EseDrawListObject* object,
    const float* points, size_t point_count,
    float stroke_width
);

void draw_list_object_get_polyline(
    const EseDrawListObject* object,
    const float** points, size_t* point_count,
    float* stroke_width
);

void draw_list_object_set_polyline_color(
    EseDrawListObject* object,
    unsigned char r, unsigned char g, unsigned char b, unsigned char a
);

void draw_list_object_get_polyline_color(
    const EseDrawListObject* object,
    unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a
);

void draw_list_object_set_polyline_stroke_color(
    EseDrawListObject* object,
    unsigned char r, unsigned char g, unsigned char b, unsigned char a
);

void draw_list_object_get_polyline_stroke_color(
    const EseDrawListObject* object,
    unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a
);

void draw_list_object_set_mesh(
    EseDrawListObject* object,
    EseDrawListVertex* verts, size_t vert_count,
    uint32_t* indices, size_t idx_count,
    const char* texture_id
);

void draw_list_object_get_mesh(
    const EseDrawListObject* object,
    const EseDrawListVertex** verts, size_t* vert_count,
    const uint32_t** indices, size_t* idx_count,
    const char** texture_id
);

// Scissor/clipping functions
void draw_list_object_set_scissor(
    EseDrawListObject* object,
    float scissor_x, float scissor_y, float scissor_w, float scissor_h
);

void draw_list_object_get_scissor(
    const EseDrawListObject* object,
    bool* scissor_active,
    float* scissor_x, float* scissor_y, float* scissor_w, float* scissor_h
);

void draw_list_object_clear_scissor(EseDrawListObject* object);


#endif // ESE_DRAW_LIST_H