#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "utility/log.h"
#include "core/memory_manager.h"
#include "graphics/draw_list.h"

#define DRAW_LIST_INITIAL_CAPACITY 256
#define TEXTURE_ID_MAX_LEN 256

/**
 * @brief Texture data for draw list objects.
 * 
 * @details This structure stores texture coordinates and ID for rendering
 *          textured objects. The texture coordinates are normalized values
 *          that define the region of the texture to sample.
 */
typedef struct EseDrawListTexture {
    // Texture to draw
    char texture_id[TEXTURE_ID_MAX_LEN]; /**< ID of the texture to render */
    float texture_x1;                    /**< Left texture coordinate (normalized) */
    float texture_y1;                    /**< Top texture coordinate (normalized) */
    float texture_x2;                    /**< Right texture coordinate (normalized) */
    float texture_y2;                    /**< Bottom texture coordinate (normalized) */
} EseDrawListTexture;

/**
 * @brief Rectangle data for draw list objects.
 * 
 * @details This structure stores color and fill information for rendering
 *          rectangular objects. Colors are stored as 8-bit RGBA values.
 */
typedef struct EseDrawListRect {
    unsigned char r;                      /**< Red component (0-255) */
    unsigned char g;                      /**< Green component (0-255) */
    unsigned char b;                      /**< Blue component (0-255) */
    unsigned char a;                      /**< Alpha component (0-255) */
    bool filled;                          /**< Whether the rectangle is filled or outlined */
} EseDrawListRect;

/**
 * @brief Represents a drawable object in the render list.
 * 
 * @details This structure contains all the information needed to render
 *          an object, including position, size, rotation, z-index, and
 *          type-specific data (texture or rectangle). It supports both
 *          textured sprites and colored rectangles.
 */
struct EseDrawListObject {
    EseDrawListObjectType type;           /**< Type of object (texture or rectangle) */
    union {
        EseDrawListTexture texture;       /**< Texture data for RL_TEXTURE type */
        EseDrawListRect rect;             /**< Rectangle data for RL_RECT type */
    } data;                               /**< Union containing type-specific data */

    // Where to draw
    float x;                              /**< The x-coordinate of the object's top-left corner */
    float y;                              /**< The y-coordinate of the object's top-left corner */
    int w;                                /**< The width of the object in pixels */
    int h;                                /**< The height of the object in pixels */

    float rotation;                       /**< The rotation of the object around the pivot point in radians */
    float rot_x;                          /**< The x coordinate for the rotation pivot point (normalized) */
    float rot_y;                          /**< The y coordinate for the rotation pivot point (normalized) */

    int z_index;                          /**< The z-index / draw order of the object */
};

/**
 * @brief Manages a collection of drawable objects for rendering.
 * 
 * @details This structure implements an object pool for efficient rendering.
 *          It pre-allocates objects and reuses them across frames to avoid
 *          memory allocation overhead during rendering.
 */
struct EseDrawList {
    EseDrawListObject **objects;          /**< Pool of pre-allocated object pointers */
    size_t objects_count;                 /**< Number of objects in use this frame */
    size_t objects_capacity;              /**< Total allocated capacity for objects */
};

static int _compare_draw_list_object_z(const void *a, const void *b) {
    const EseDrawListObject *obj_a = *(const EseDrawListObject **)a;
    const EseDrawListObject *obj_b = *(const EseDrawListObject **)b;
    int za = draw_list_object_get_z_index(obj_a);
    int zb = draw_list_object_get_z_index(obj_b);
    return (za > zb) - (za < zb);
}

static void _init_new_object(EseDrawListObject *obj) {
    if (!obj) return;

    obj->type = RL_RECT;
    obj->x = 0.0f;
    obj->y = 0.0f;
    obj->w = 0;
    obj->h = 0;
    obj->rotation = 0.0f;
    obj->rot_x = 0.5f; /* default pivot at center */
    obj->rot_y = 0.5f; /* default pivot at center */
    obj->z_index = 0;

    /* zero union data */
    memset(&obj->data, 0, sizeof(obj->data));
}

EseDrawList* draw_list_create(void) {
    EseDrawList *draw_list = memory_manager.malloc(sizeof(EseDrawList), MMTAG_DRAWLIST);
    draw_list->objects = memory_manager.malloc(sizeof(EseDrawListObject*) * DRAW_LIST_INITIAL_CAPACITY, MMTAG_DRAWLIST);

    draw_list->objects_capacity = DRAW_LIST_INITIAL_CAPACITY;
    draw_list->objects_count = 0;

    // Pre-allocate objects
    for (size_t i = 0; i < draw_list->objects_capacity; ++i) {
        draw_list->objects[i] = memory_manager.calloc(1, sizeof(EseDrawListObject), MMTAG_DRAWLIST);
        _init_new_object(draw_list->objects[i]);
    }
    return draw_list;
}

void draw_list_free(EseDrawList *draw_list) {
    log_assert("RENDER_LIST", draw_list, "draw_list_free called with NULL draw_list");

    for (size_t i = 0; i < draw_list->objects_capacity; ++i) {
        memory_manager.free(draw_list->objects[i]);
    }
    memory_manager.free(draw_list->objects);
    memory_manager.free(draw_list);
}

void draw_list_clear(EseDrawList *draw_list) {
    log_assert("RENDER_LIST", draw_list, "draw_list_clear called with NULL draw_list");
    draw_list->objects_count = 0;
}

EseDrawListObject* draw_list_request_object(EseDrawList *draw_list) {
    log_assert("RENDER_LIST", draw_list, "draw_list_request_object called with NULL draw_list");

    if (draw_list->objects_count == draw_list->objects_capacity) {
        // Grow pool
        size_t new_capacity = draw_list->objects_capacity * 2;
        EseDrawListObject **new_objs = memory_manager.realloc(draw_list->objects, sizeof(EseDrawListObject*) * new_capacity, MMTAG_DRAWLIST);
        if (!new_objs) {
            log_error("RENDER_LIST", "Error: Failed to grow object pool");
            return NULL;
        }
        draw_list->objects = new_objs;
        // Allocate new objects
        for (size_t i = draw_list->objects_capacity; i < new_capacity; ++i) {
            draw_list->objects[i] = memory_manager.calloc(1, sizeof(EseDrawListObject), MMTAG_DRAWLIST);
        }
        draw_list->objects_capacity = new_capacity;
    }
    EseDrawListObject *obj = draw_list->objects[draw_list->objects_count++];

    memset(obj, 0, sizeof(EseDrawListObject));
    _init_new_object(obj);
    return obj;
}

void draw_list_sort(EseDrawList *draw_list) {
    log_assert("RENDER_LIST", draw_list, "draw_list_sort called with NULL draw_list");
    log_assert("RENDER_LIST", draw_list->objects, "draw_list_sort called with NULL draw_list->objects");

    qsort(draw_list->objects, draw_list->objects_count, sizeof(EseDrawListObject *), _compare_draw_list_object_z);
}

size_t draw_list_get_object_count(const EseDrawList *draw_list) {
    log_assert("RENDER_LIST", draw_list, "draw_list_get_object_count called with NULL draw_list");

    return draw_list->objects_count;
}

EseDrawListObject* draw_list_get_object(const EseDrawList *draw_list, size_t index) {
    log_assert("RENDER_LIST", draw_list, "draw_list_get_object called with NULL draw_list");
    log_assert("RENDER_LIST", draw_list->objects_count > 0, "draw_list_get_object called with draw_list->objects_count <= 0");

    return draw_list->objects[index];
}

/*
 * Object functions
 */

void draw_list_object_set_texture(
    EseDrawListObject* object, const char *texture_id,
    float texture_x1, float texture_y1, float texture_x2, float texture_y2
) {
    log_assert("RENDER_LIST", object, "draw_list_object_set_texture called with NULL object");
    log_assert("RENDER_LIST", texture_id, "draw_list_object_set_texture called with NULL texture_id");

    object->type = RL_TEXTURE;
    EseDrawListTexture* texture_data = &object->data.texture;

    strncpy(texture_data->texture_id, texture_id, TEXTURE_ID_MAX_LEN - 1);
    texture_data->texture_id[TEXTURE_ID_MAX_LEN - 1] = '\0';

    texture_data->texture_x1 = texture_x1;
    texture_data->texture_y1 = texture_y1;
    texture_data->texture_x2 = texture_x2;
    texture_data->texture_y2 = texture_y2;
}

void draw_list_object_set_rect_color(EseDrawListObject* object, unsigned char r, unsigned char g, unsigned char b, unsigned char a, bool filled) {
    log_assert("RENDER_LIST", object, "draw_list_object_set_rect_ese_color_color called with NULL object");

    object->type = RL_RECT;
    EseDrawListRect* rect_data = &object->data.rect;

    rect_data->r = r;
    rect_data->g = g;
    rect_data->b = b;
    rect_data->a = a;
    rect_data->filled = filled;
}

void draw_list_object_set_bounds(EseDrawListObject* object, float x, float y, int w, int h) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_bounds called with NULL object");

    object->x = x;
    object->y = y;
    object->w = w;
    object->h = h;
}

void draw_list_object_set_z_index(EseDrawListObject* object, int z_index) {
    log_assert("RENDER_LIST", object, "draw_list_object_set_z_index called with NULL object");

    object->z_index = z_index;
}

int draw_list_object_get_z_index(const EseDrawListObject* object) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_z_index called with NULL object");

    return object->z_index;
}

EseDrawListObjectType draw_list_object_get_type(const EseDrawListObject* object) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_z_index called with NULL object");

    return object->type;
}

void draw_list_object_get_bounds(const EseDrawListObject* object, float* x, float* y, int* w, int* h) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_bounds called with NULL object");

    if (x) *x = object->x;
    if (y) *y = object->y;
    if (w) *w = object->w;
    if (h) *h = object->h;
}

void draw_list_object_get_texture(
    const EseDrawListObject* object, const char **texture_id,
    float *texture_x1, float *texture_y1, float *texture_x2, float *texture_y2
) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_texture called with NULL object");
    log_assert("RENDER_LIST", object->type == RL_TEXTURE, "draw_list_object_get_texture called with non-texture object");
    log_assert("RENDER_LIST", texture_id, "draw_list_object_get_texture called with NULL texture_id");

    const EseDrawListTexture* texture_data = &object->data.texture;

    if (texture_id) *texture_id = texture_data->texture_id;
    if (texture_x1) *texture_x1 = texture_data->texture_x1;
    if (texture_y1) *texture_y1 = texture_data->texture_y1;
    if (texture_x2) *texture_x2 = texture_data->texture_x2;
    if (texture_y2) *texture_y2 = texture_data->texture_y2;
}

void draw_list_object_get_rect_color(const EseDrawListObject* object, unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a, bool* filled) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_rect_drawable called with NULL object");
    log_assert("RENDER_LIST", object->type == RL_RECT, "draw_list_object_get_rect_drawable called with non-rect object");

    const EseDrawListRect* rect_data = &object->data.rect;

    if (r) *r = rect_data->r;
    if (g) *g = rect_data->g;
    if (b) *b = rect_data->b;
    if (a) *a = rect_data->a;
    if (filled) *filled = rect_data->filled;
}

void draw_list_object_set_rotation(EseDrawListObject* object, float radians) {
    log_assert("RENDER_LIST", object, "draw_list_object_set_rotation called with NULL object");
    object->rotation = radians;
}

float draw_list_object_get_rotation(const EseDrawListObject* object) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_rotation called with NULL object");
    return object->rotation;
}

void draw_list_object_set_pivot(EseDrawListObject* object, float nx, float ny) {
    log_assert("RENDER_LIST", object, "draw_list_object_set_pivot called with NULL object");
    /* clamp to [0,1] */
    if (nx < 0.0f) nx = 0.0f;
    if (nx > 1.0f) nx = 1.0f;
    if (ny < 0.0f) ny = 0.0f;
    if (ny > 1.0f) ny = 1.0f;
    object->rot_x = nx;
    object->rot_y = ny;
}

void draw_list_object_get_pivot(const EseDrawListObject* object, float *nx, float *ny) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_pivot called with NULL object");
    if (nx) *nx = object->rot_x;
    if (ny) *ny = object->rot_y;
}

/* Compute rotated AABB for the object; outputs optional minx,miny,maxx,maxy */
void draw_list_object_get_rotated_aabb(const EseDrawListObject* object, float *minx, float *miny, float *maxx, float *maxy) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_rotated_aabb called with NULL object");

    /* axis-aligned fast-path */
    if (fabsf(object->rotation) < 1e-6f) {
        float lx = object->x;
        float ty = object->y;
        float rx = object->x + (float)object->w;
        float by = object->y + (float)object->h;
        if (minx) *minx = lx;
        if (miny) *miny = ty;
        if (maxx) *maxx = rx;
        if (maxy) *maxy = by;
        return;
    }

    /* compute pivot in world coords */
    float px = object->x + object->rot_x * (float)object->w;
    float py = object->y + object->rot_y * (float)object->h;

    /* compute four corners relative to pivot, rotate them, and find bounds */
    float cosr = cosf(object->rotation);
    float sinr = sinf(object->rotation);

    float local_x[4];
    float local_y[4];

    /* corners relative to top-left */
    float tlx = -object->rot_x * (float)object->w;
    float tly = -object->rot_y * (float)object->h;
    float trx = (1.0f - object->rot_x) * (float)object->w;
    float tryy = -object->rot_y * (float)object->h;
    float brx = (1.0f - object->rot_x) * (float)object->w;
    float bry = (1.0f - object->rot_y) * (float)object->h;
    float blx = -object->rot_x * (float)object->w;
    float bly = (1.0f - object->rot_y) * (float)object->h;

    local_x[0] = tlx; local_y[0] = tly; /* TL */
    local_x[1] = trx; local_y[1] = tryy; /* TR */
    local_x[2] = brx; local_y[2] = bry; /* BR */
    local_x[3] = blx; local_y[3] = bly; /* BL */

    float min_x = FLT_MAX;
    float min_y = FLT_MAX;
    float max_x = -FLT_MAX;
    float max_y = -FLT_MAX;

    for (int i = 0; i < 4; ++i) {
        float rx = local_x[i] * cosr - local_y[i] * sinr;
        float ry = local_x[i] * sinr + local_y[i] * cosr;
        float vx = px + rx;
        float vy = py + ry;
        if (vx < min_x) min_x = vx;
        if (vy < min_y) min_y = vy;
        if (vx > max_x) max_x = vx;
        if (vy > max_y) max_y = vy;
    }

    if (minx) *minx = min_x;
    if (miny) *miny = min_y;
    if (maxx) *maxx = max_x;
    if (maxy) *maxy = max_y;
}
