#include "graphics/draw_list.h"
#include "core/memory_manager.h"
#include "utility/log.h"
#include "utility/thread.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DRAW_LIST_INITIAL_CAPACITY 256
#define TEXTURE_ID_MAX_LEN 256
#define POLYLINE_MAX_POINTS 1024
#define MESH_MAX_VERTS 4096
#define MESH_MAX_INDICES 8192

#define EDL_OBJ_MAGIC 0xE5E5E5E5u

/**
 * @brief Color data for draw list objects.
 *
 * @details This structure stores RGBA color values for rendering objects.
 */
typedef struct EseDrawListColor {
    unsigned char r; /** Red component (0-255) */
    unsigned char g; /** Green component (0-255) */
    unsigned char b; /** Blue component (0-255) */
    unsigned char a; /** Alpha component (0-255) */
} EseDrawListColor;

/**
 * @brief Point data for draw list objects.
 *
 * @details This structure stores x,y coordinates for polyline points.
 */
typedef struct EseDrawListPoint {
    float x; /** X coordinate */
    float y; /** Y coordinate */
} EseDrawListPoint;

/**
 * @brief Polyline data for draw list objects.
 *
 * @details This structure stores point data and styling for rendering
 *          polyline objects.
 */
typedef struct EseDrawListPolyLine {
    EseDrawListPoint points[POLYLINE_MAX_POINTS]; /** Array of points defining
                                                     the polyline */
    size_t point_count;                           /** Number of points in the polyline */
    EseDrawListColor fill_color;                  /** Fill color for the polyline */
    EseDrawListColor stroke_color;                /** Stroke color for the polyline */
    float stroke_width;                           /** Width of the stroke in pixels */
} EseDrawListPolyLine;

/**
 * @brief Texture data for draw list objects.
 *
 * @details This structure stores texture coordinates and ID for rendering
 *          textured objects. The texture coordinates are normalized values
 *          that define the region of the texture to sample.
 */
typedef struct EseDrawListTexture {
    // Texture to draw
    char texture_id[TEXTURE_ID_MAX_LEN]; /** ID of the texture to render */
    float texture_x1;                    /** Left texture coordinate (normalized) */
    float texture_y1;                    /** Top texture coordinate (normalized) */
    float texture_x2;                    /** Right texture coordinate (normalized) */
    float texture_y2;                    /** Bottom texture coordinate (normalized) */
    int w;                               /** Width of the texture in pixels */
    int h;                               /** Height of the texture in pixels */
} EseDrawListTexture;

/**
 * @brief Rectangle data for draw list objects.
 *
 * @details This structure stores color and fill information for rendering
 *          rectangular objects. Colors are stored as 8-bit RGBA values.
 */
typedef struct EseDrawListRect {
    EseDrawListColor color; /** Color of the rectangle */
    bool filled;            /** Whether the rectangle is filled or outlined */
    int w;                  /** Width of the rectangle in pixels */
    int h;                  /** Height of the rectangle in pixels */
} EseDrawListRect;

typedef struct EseDrawListMesh {
    EseDrawListVertex verts[MESH_MAX_VERTS];
    size_t vert_count;
    uint32_t indices[MESH_MAX_INDICES];
    size_t idx_count;
    char texture_id[TEXTURE_ID_MAX_LEN];
} EseDrawListMesh;

/**
 * @brief Represents a drawable object in the render list.
 *
 * @details This structure contains all the information needed to render
 *          an object, including position, size, rotation, z-index, and
 *          type-specific data (texture or rectangle). It supports both
 *          textured sprites and colored rectangles.
 */
struct EseDrawListObject {
    uint32_t magic;
    EseDrawListObjectType type; /** Type of object (texture, rectangle, or polyline) */
    union {
        EseDrawListTexture texture;   /** Texture data for DL_TEXTURE type */
        EseDrawListRect rect;         /** Rectangle data for DL_RECT type */
        EseDrawListPolyLine polyline; /** Polyline data for DL_POLYLINE type */
        EseDrawListMesh mesh;         /** Mesh data for DL_MESH type */
    } data;                           /** Union containing type-specific data */

    // Where to draw
    float x; /** The x-coordinate of the object's top-left corner */
    float y; /** The y-coordinate of the object's top-left corner */

    float rotation; /** The rotation of the object around the pivot point in
                       radians */
    float rot_x;    /** The x coordinate for the rotation pivot point (normalized) */
    float rot_y;    /** The y coordinate for the rotation pivot point (normalized) */

    uint64_t z_index; /** The z-index / draw order of the object */

    // Clipping/scissor rectangle
    bool scissor_active; /** Whether scissor clipping is enabled */
    float scissor_x, scissor_y, scissor_w, scissor_h;
};

/**
 * @brief Manages a collection of drawable objects for rendering.
 *
 * @details This structure implements an object pool for efficient rendering.
 *          It pre-allocates objects and reuses them across frames to avoid
 *          memory allocation overhead during rendering.
 */
struct EseDrawList {
    EseDrawListObject **objects;   /** Pool of pre-allocated object pointers */
    EseAtomicSizeT *objects_count; /** Number of objects in use this frame (atomic) */
    size_t objects_capacity;       /** Total allocated capacity for objects */
    EseMutex *mutex;               /** Mutex for thread safety */
};

static int _compare_draw_list_object_z(const void *a, const void *b) {
    const EseDrawListObject *obj_a = *(const EseDrawListObject **)a;
    const EseDrawListObject *obj_b = *(const EseDrawListObject **)b;
    uint64_t za = draw_list_object_get_z_index(obj_a);
    uint64_t zb = draw_list_object_get_z_index(obj_b);
    return (za > zb) - (za < zb);
}

static void _init_new_object(EseDrawListObject *obj) {
    if (!obj)
        return;

    obj->magic = EDL_OBJ_MAGIC;

    obj->type = DL_RECT;
    obj->x = 0.0f;
    obj->y = 0.0f;
    obj->rotation = 0.0f;
    obj->rot_x = 0.5f; /* default pivot at center */
    obj->rot_y = 0.5f; /* default pivot at center */
    obj->z_index = 0;

    /* Initialize scissor to no clipping */
    obj->scissor_active = false;
    obj->scissor_x = 0.0f;
    obj->scissor_y = 0.0f;
    obj->scissor_w = 0.0f;
    obj->scissor_h = 0.0f;

    /* zero union data */
    memset(&obj->data, 0, sizeof(obj->data));
}

EseDrawList *draw_list_create(void) {
    EseDrawList *draw_list = memory_manager.malloc(sizeof(EseDrawList), MMTAG_DRAWLIST);
    draw_list->objects = memory_manager.malloc(
        sizeof(EseDrawListObject *) * DRAW_LIST_INITIAL_CAPACITY, MMTAG_DRAWLIST);

    draw_list->objects_capacity = DRAW_LIST_INITIAL_CAPACITY;

    // Create atomic counter for thread safety
    draw_list->objects_count = ese_atomic_size_t_create(0);
    if (!draw_list->objects_count) {
        memory_manager.free(draw_list->objects);
        memory_manager.free(draw_list);
        return NULL;
    }

    // Create mutex for thread safety
    draw_list->mutex = ese_mutex_create();

    // Pre-allocate objects
    for (size_t i = 0; i < draw_list->objects_capacity; ++i) {
        draw_list->objects[i] = memory_manager.calloc(1, sizeof(EseDrawListObject), MMTAG_DRAWLIST);
        _init_new_object(draw_list->objects[i]);
    }
    return draw_list;
}

void draw_list_destroy(EseDrawList *draw_list) {
    log_assert("RENDER_LIST", draw_list, "draw_list_destroy called with NULL draw_list");

    for (size_t i = 0; i < draw_list->objects_capacity; ++i) {
        memory_manager.free(draw_list->objects[i]);
    }
    memory_manager.free(draw_list->objects);
    ese_atomic_size_t_destroy(draw_list->objects_count);
    ese_mutex_destroy(draw_list->mutex);
    memory_manager.free(draw_list);
}

void draw_list_clear(EseDrawList *draw_list) {
    log_assert("RENDER_LIST", draw_list, "draw_list_clear called with NULL draw_list");

    ese_atomic_size_t_store(draw_list->objects_count, 0);
}

EseDrawListObject *draw_list_request_object(EseDrawList *draw_list) {
    log_assert("RENDER_LIST", draw_list, "draw_list_request_object called with NULL draw_list");
    ese_mutex_lock(draw_list->mutex);
    /* reserve */
    size_t index = ese_atomic_size_t_fetch_add(draw_list->objects_count, 1);
    if (index >= draw_list->objects_capacity) {
        size_t new_capacity = draw_list->objects_capacity ? draw_list->objects_capacity * 2
                                                          : DRAW_LIST_INITIAL_CAPACITY;
        if (new_capacity <= index)
            new_capacity = index + 1;
        EseDrawListObject **new_objs = memory_manager.realloc(
            draw_list->objects, sizeof(EseDrawListObject *) * new_capacity, MMTAG_DRAWLIST);
        if (!new_objs) {
            ese_atomic_size_t_fetch_sub_inplace(draw_list->objects_count, 1);
            ese_mutex_unlock(draw_list->mutex);
            return NULL;
        }
        draw_list->objects = new_objs;
        for (size_t i = draw_list->objects_capacity; i < new_capacity; ++i) {
            draw_list->objects[i] =
                memory_manager.calloc(1, sizeof(EseDrawListObject), MMTAG_DRAWLIST);
            _init_new_object(draw_list->objects[i]);
        }
        draw_list->objects_capacity = new_capacity;
    }
    EseDrawListObject *obj = draw_list->objects[index];
    memset(obj, 0, sizeof(EseDrawListObject));
    _init_new_object(obj);
    ese_mutex_unlock(draw_list->mutex);

    if (obj->magic != EDL_OBJ_MAGIC) {
        log_error("RENDER_LIST", "object magic corrupted idx=%zu magic=0x%x", index, obj->magic);
        abort();
    }
    return obj;
}

void draw_list_sort(EseDrawList *draw_list) {
    log_assert("RENDER_LIST", draw_list, "draw_list_sort called with NULL draw_list");
    log_assert("RENDER_LIST", draw_list->objects,
               "draw_list_sort called with NULL draw_list->objects");

    // Use mutex to ensure the objects array doesn't change during sorting
    ese_mutex_lock(draw_list->mutex);
    size_t count = ese_atomic_size_t_load(draw_list->objects_count);
    qsort(draw_list->objects, count, sizeof(EseDrawListObject *), _compare_draw_list_object_z);
    ese_mutex_unlock(draw_list->mutex);
}

size_t draw_list_get_object_count(const EseDrawList *draw_list) {
    log_assert("RENDER_LIST", draw_list, "draw_list_get_object_count called with NULL draw_list");

    return ese_atomic_size_t_load(draw_list->objects_count);
}

EseDrawListObject *draw_list_get_object(const EseDrawList *draw_list, size_t index) {
    log_assert("RENDER_LIST", draw_list, "draw_list_get_object called with NULL draw_list");
    /* Lock to avoid racing with realloc / growth */
    ese_mutex_lock((EseMutex *)draw_list->mutex);

    size_t count = ese_atomic_size_t_load(draw_list->objects_count);
    if (index >= count) {
        ese_mutex_unlock((EseMutex *)draw_list->mutex);
        return NULL;
    }

    EseDrawListObject *obj = draw_list->objects[index];

    if (obj->magic != EDL_OBJ_MAGIC) {
        log_error("RENDER_LIST", "object magic corrupted idx=%zu magic=0x%x", index, obj->magic);
        /* abort under the mutex to keep the state intact for post-mortem */
        abort();
    }

    ese_mutex_unlock((EseMutex *)draw_list->mutex);
    return obj;
}

/*
 * Object functions
 */

void draw_list_object_set_texture(EseDrawListObject *object, const char *texture_id,
                                  float texture_x1, float texture_y1, float texture_x2,
                                  float texture_y2) {
    log_assert("RENDER_LIST", object, "draw_list_object_set_texture called with NULL object");
    log_assert("RENDER_LIST", texture_id,
               "draw_list_object_set_texture called with NULL texture_id");

    object->type = DL_TEXTURE;
    EseDrawListTexture *texture_data = &object->data.texture;

    strncpy(texture_data->texture_id, texture_id, TEXTURE_ID_MAX_LEN - 1);
    texture_data->texture_id[TEXTURE_ID_MAX_LEN - 1] = '\0';

    texture_data->texture_x1 = texture_x1;
    texture_data->texture_y1 = texture_y1;
    texture_data->texture_x2 = texture_x2;
    texture_data->texture_y2 = texture_y2;
}

void draw_list_object_set_rect_color(EseDrawListObject *object, unsigned char r, unsigned char g,
                                     unsigned char b, unsigned char a, bool filled) {
    log_assert("RENDER_LIST", object, "draw_list_object_set_rect_color called with NULL object");

    object->type = DL_RECT;
    EseDrawListRect *rect_data = &object->data.rect;

    rect_data->color.r = r;
    rect_data->color.g = g;
    rect_data->color.b = b;
    rect_data->color.a = a;
    rect_data->filled = filled;
}

void draw_list_object_set_bounds(EseDrawListObject *object, float x, float y, int w, int h) {
    log_assert("RENDER_LIST", object, "draw_list_object_set_bounds called with NULL object");

    object->x = x;
    object->y = y;

    // Set width and height in the appropriate union member based on object type
    if (object->type == DL_TEXTURE) {
        object->data.texture.w = w;
        object->data.texture.h = h;
    } else if (object->type == DL_RECT) {
        object->data.rect.w = w;
        object->data.rect.h = h;
    }
    // Note: Polyline objects don't use width/height as they are defined by
    // their points
}

void draw_list_object_set_z_index(EseDrawListObject *object, uint64_t z_index) {
    log_assert("RENDER_LIST", object, "draw_list_object_set_z_index called with NULL object");

    object->z_index = z_index;
}

uint64_t draw_list_object_get_z_index(const EseDrawListObject *object) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_z_index called with NULL object");

    return object->z_index;
}

EseDrawListObjectType draw_list_object_get_type(const EseDrawListObject *object) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_z_index called with NULL object");

    return object->type;
}

void draw_list_object_get_bounds(const EseDrawListObject *object, float *x, float *y, int *w,
                                 int *h) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_bounds called with NULL object");

    if (x)
        *x = object->x;
    if (y)
        *y = object->y;

    // Get width and height from the appropriate union member based on object
    // type
    if (w || h) {
        if (object->type == DL_TEXTURE) {
            if (w)
                *w = object->data.texture.w;
            if (h)
                *h = object->data.texture.h;
        } else if (object->type == DL_RECT) {
            if (w)
                *w = object->data.rect.w;
            if (h)
                *h = object->data.rect.h;
        } else if (object->type == DL_POLYLINE) {
            // For polylines, calculate bounds from points
            if (w || h) {
                const EseDrawListPolyLine *polyline_data = &object->data.polyline;
                if (polyline_data->point_count > 0) {
                    float min_x = polyline_data->points[0].x;
                    float max_x = polyline_data->points[0].x;
                    float min_y = polyline_data->points[0].y;
                    float max_y = polyline_data->points[0].y;

                    for (size_t i = 1; i < polyline_data->point_count; i++) {
                        if (polyline_data->points[i].x < min_x)
                            min_x = polyline_data->points[i].x;
                        if (polyline_data->points[i].x > max_x)
                            max_x = polyline_data->points[i].x;
                        if (polyline_data->points[i].y < min_y)
                            min_y = polyline_data->points[i].y;
                        if (polyline_data->points[i].y > max_y)
                            max_y = polyline_data->points[i].y;
                    }

                    if (w)
                        *w = (int)(max_x - min_x);
                    if (h)
                        *h = (int)(max_y - min_y);
                } else {
                    if (w)
                        *w = 0;
                    if (h)
                        *h = 0;
                }
            }
        } else {
            // Default to 0 if type is not set
            if (w)
                *w = 0;
            if (h)
                *h = 0;
        }
    }
}

void draw_list_object_get_texture(const EseDrawListObject *object, const char **texture_id,
                                  float *texture_x1, float *texture_y1, float *texture_x2,
                                  float *texture_y2) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_texture called with NULL object");
    log_assert("RENDER_LIST", object->type == DL_TEXTURE,
               "draw_list_object_get_texture called with non-texture object");
    log_assert("RENDER_LIST", texture_id,
               "draw_list_object_get_texture called with NULL texture_id");

    const EseDrawListTexture *texture_data = &object->data.texture;

    if (texture_id)
        *texture_id = texture_data->texture_id;
    if (texture_x1)
        *texture_x1 = texture_data->texture_x1;
    if (texture_y1)
        *texture_y1 = texture_data->texture_y1;
    if (texture_x2)
        *texture_x2 = texture_data->texture_x2;
    if (texture_y2)
        *texture_y2 = texture_data->texture_y2;
}

void draw_list_object_get_rect_color(const EseDrawListObject *object, unsigned char *r,
                                     unsigned char *g, unsigned char *b, unsigned char *a,
                                     bool *filled) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_rect_color called with NULL object");
    log_assert("RENDER_LIST", object->type == DL_RECT,
               "draw_list_object_get_rect_color called with non-rect object");

    const EseDrawListRect *rect_data = &object->data.rect;

    if (r)
        *r = rect_data->color.r;
    if (g)
        *g = rect_data->color.g;
    if (b)
        *b = rect_data->color.b;
    if (a)
        *a = rect_data->color.a;
    if (filled)
        *filled = rect_data->filled;
}

void draw_list_object_set_polyline(EseDrawListObject *object, const float *points,
                                   size_t point_count, float stroke_width) {
    log_assert("RENDER_LIST", object, "draw_list_object_set_polyline called with NULL object");
    log_assert("RENDER_LIST", points, "draw_list_object_set_polyline called with NULL points");
    log_assert("RENDER_LIST", point_count > 0,
               "draw_list_object_set_polyline called with point_count <= 0");
    log_assert("RENDER_LIST", point_count <= POLYLINE_MAX_POINTS,
               "draw_list_object_set_polyline called with point_count > "
               "POLYLINE_MAX_POINTS");

    object->type = DL_POLYLINE;
    EseDrawListPolyLine *polyline_data = &object->data.polyline;

    // Copy points (points array is x1,y1,x2,y2,... format)
    for (size_t i = 0; i < point_count; i++) {
        polyline_data->points[i].x = points[i * 2];
        polyline_data->points[i].y = points[i * 2 + 1];
    }
    polyline_data->point_count = point_count;

    // Set default colors
    polyline_data->fill_color.r = 0; // Transparent fill
    polyline_data->fill_color.g = 0;
    polyline_data->fill_color.b = 0;
    polyline_data->fill_color.a = 0;

    polyline_data->stroke_color.r = 0; // Black stroke
    polyline_data->stroke_color.g = 0;
    polyline_data->stroke_color.b = 0;
    polyline_data->stroke_color.a = 255;

    polyline_data->stroke_width = stroke_width;
}

void draw_list_object_get_polyline(const EseDrawListObject *object, const float **points,
                                   size_t *point_count, float *stroke_width) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_polyline called with NULL object");
    log_assert("RENDER_LIST", object->type == DL_POLYLINE,
               "draw_list_object_get_polyline called with non-polyline object");

    const EseDrawListPolyLine *polyline_data = &object->data.polyline;

    if (points)
        *points = (const float *)polyline_data->points; // Cast to float* for x1,y1,x2,y2,... format
    if (point_count)
        *point_count = polyline_data->point_count;
    if (stroke_width)
        *stroke_width = polyline_data->stroke_width;
}

void draw_list_object_set_polyline_color(EseDrawListObject *object, unsigned char r,
                                         unsigned char g, unsigned char b, unsigned char a) {
    log_assert("RENDER_LIST", object,
               "draw_list_object_set_polyline_color called with NULL object");
    log_assert("RENDER_LIST", object->type == DL_POLYLINE,
               "draw_list_object_set_polyline_color called with non-polyline object");

    EseDrawListPolyLine *polyline_data = &object->data.polyline;
    polyline_data->fill_color.r = r;
    polyline_data->fill_color.g = g;
    polyline_data->fill_color.b = b;
    polyline_data->fill_color.a = a;
}

void draw_list_object_get_polyline_color(const EseDrawListObject *object, unsigned char *r,
                                         unsigned char *g, unsigned char *b, unsigned char *a) {
    log_assert("RENDER_LIST", object,
               "draw_list_object_get_polyline_color called with NULL object");
    log_assert("RENDER_LIST", object->type == DL_POLYLINE,
               "draw_list_object_get_polyline_color called with non-polyline object");

    const EseDrawListPolyLine *polyline_data = &object->data.polyline;
    if (r)
        *r = polyline_data->fill_color.r;
    if (g)
        *g = polyline_data->fill_color.g;
    if (b)
        *b = polyline_data->fill_color.b;
    if (a)
        *a = polyline_data->fill_color.a;
}

void draw_list_object_set_polyline_stroke_color(EseDrawListObject *object, unsigned char r,
                                                unsigned char g, unsigned char b, unsigned char a) {
    log_assert("RENDER_LIST", object,
               "draw_list_object_set_polyline_stroke_color called with NULL object");
    log_assert("RENDER_LIST", object->type == DL_POLYLINE,
               "draw_list_object_set_polyline_stroke_color called with "
               "non-polyline object");

    EseDrawListPolyLine *polyline_data = &object->data.polyline;
    polyline_data->stroke_color.r = r;
    polyline_data->stroke_color.g = g;
    polyline_data->stroke_color.b = b;
    polyline_data->stroke_color.a = a;
}

void draw_list_object_get_polyline_stroke_color(const EseDrawListObject *object, unsigned char *r,
                                                unsigned char *g, unsigned char *b,
                                                unsigned char *a) {
    log_assert("RENDER_LIST", object,
               "draw_list_object_get_polyline_stroke_color called with NULL object");
    log_assert("RENDER_LIST", object->type == DL_POLYLINE,
               "draw_list_object_get_polyline_stroke_color called with "
               "non-polyline object");

    const EseDrawListPolyLine *polyline_data = &object->data.polyline;
    if (r)
        *r = polyline_data->stroke_color.r;
    if (g)
        *g = polyline_data->stroke_color.g;
    if (b)
        *b = polyline_data->stroke_color.b;
    if (a)
        *a = polyline_data->stroke_color.a;
}

void draw_list_object_set_rotation(EseDrawListObject *object, float radians) {
    log_assert("RENDER_LIST", object, "draw_list_object_set_rotation called with NULL object");
    object->rotation = radians;
}

float draw_list_object_get_rotation(const EseDrawListObject *object) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_rotation called with NULL object");
    return object->rotation;
}

void draw_list_object_set_pivot(EseDrawListObject *object, float nx, float ny) {
    log_assert("RENDER_LIST", object, "draw_list_object_set_pivot called with NULL object");
    /* clamp to [0,1] */
    if (nx < 0.0f)
        nx = 0.0f;
    if (nx > 1.0f)
        nx = 1.0f;
    if (ny < 0.0f)
        ny = 0.0f;
    if (ny > 1.0f)
        ny = 1.0f;
    object->rot_x = nx;
    object->rot_y = ny;
}

void draw_list_object_get_pivot(const EseDrawListObject *object, float *nx, float *ny) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_pivot called with NULL object");
    if (nx)
        *nx = object->rot_x;
    if (ny)
        *ny = object->rot_y;
}

/* Compute rotated AABB for the object; outputs optional minx,miny,maxx,maxy */
void draw_list_object_get_rotated_aabb(const EseDrawListObject *object, float *minx, float *miny,
                                       float *maxx, float *maxy) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_rotated_aabb called with NULL object");

    // Handle polyline objects differently as they don't have width/height
    if (object->type == DL_POLYLINE) {
        const EseDrawListPolyLine *polyline_data = &object->data.polyline;
        if (polyline_data->point_count == 0) {
            if (minx)
                *minx = object->x;
            if (miny)
                *miny = object->y;
            if (maxx)
                *maxx = object->x;
            if (maxy)
                *maxy = object->y;
            return;
        }

        // Calculate bounds from all points
        float min_x = object->x + polyline_data->points[0].x;
        float max_x = object->x + polyline_data->points[0].x;
        float min_y = object->y + polyline_data->points[0].y;
        float max_y = object->y + polyline_data->points[0].y;

        for (size_t i = 1; i < polyline_data->point_count; i++) {
            float px = object->x + polyline_data->points[i].x;
            float py = object->y + polyline_data->points[i].y;
            if (px < min_x)
                min_x = px;
            if (px > max_x)
                max_x = px;
            if (py < min_y)
                min_y = py;
            if (py > max_y)
                max_y = py;
        }

        if (minx)
            *minx = min_x;
        if (miny)
            *miny = min_y;
        if (maxx)
            *maxx = max_x;
        if (maxy)
            *maxy = max_y;
        return;
    }

    // Get width and height from the appropriate union member
    int w, h;
    if (object->type == DL_TEXTURE) {
        w = object->data.texture.w;
        h = object->data.texture.h;
    } else if (object->type == DL_RECT) {
        w = object->data.rect.w;
        h = object->data.rect.h;
    } else {
        w = h = 0; // Default to 0 if type is not set
    }

    /* axis-aligned fast-path */
    if (fabsf(object->rotation) < 1e-6f) {
        float lx = object->x;
        float ty = object->y;
        float rx = object->x + (float)w;
        float by = object->y + (float)h;
        if (minx)
            *minx = lx;
        if (miny)
            *miny = ty;
        if (maxx)
            *maxx = rx;
        if (maxy)
            *maxy = by;
        return;
    }

    /* compute pivot in world coords */
    float px = object->x + object->rot_x * (float)w;
    float py = object->y + object->rot_y * (float)h;

    /* compute four corners relative to pivot, rotate them, and find bounds */
    float cosr = cosf(object->rotation);
    float sinr = sinf(object->rotation);

    float local_x[4];
    float local_y[4];

    /* corners relative to top-left */
    float tlx = -object->rot_x * (float)w;
    float tly = -object->rot_y * (float)h;
    float trx = (1.0f - object->rot_x) * (float)w;
    float tryy = -object->rot_y * (float)h;
    float brx = (1.0f - object->rot_x) * (float)w;
    float bry = (1.0f - object->rot_y) * (float)h;
    float blx = -object->rot_x * (float)w;
    float bly = (1.0f - object->rot_y) * (float)h;

    local_x[0] = tlx;
    local_y[0] = tly; /* TL */
    local_x[1] = trx;
    local_y[1] = tryy; /* TR */
    local_x[2] = brx;
    local_y[2] = bry; /* BR */
    local_x[3] = blx;
    local_y[3] = bly; /* BL */

    float min_x = FLT_MAX;
    float min_y = FLT_MAX;
    float max_x = -FLT_MAX;
    float max_y = -FLT_MAX;

    for (int i = 0; i < 4; ++i) {
        float rx = local_x[i] * cosr - local_y[i] * sinr;
        float ry = local_x[i] * sinr + local_y[i] * cosr;
        float vx = px + rx;
        float vy = py + ry;
        if (vx < min_x)
            min_x = vx;
        if (vy < min_y)
            min_y = vy;
        if (vx > max_x)
            max_x = vx;
        if (vy > max_y)
            max_y = vy;
    }

    if (minx)
        *minx = min_x;
    if (miny)
        *miny = min_y;
    if (maxx)
        *maxx = max_x;
    if (maxy)
        *maxy = max_y;
}

void draw_list_object_set_mesh(EseDrawListObject *object, EseDrawListVertex *verts,
                               size_t vert_count, uint32_t *indices, size_t idx_count,
                               const char *texture_id) {
    log_assert("RENDER_LIST", object, "draw_list_object_set_mesh called with NULL object");
    log_assert("RENDER_LIST", verts, "draw_list_object_set_mesh called with NULL verts");
    log_assert("RENDER_LIST", indices, "draw_list_object_set_mesh called with NULL indices");
    log_assert("RENDER_LIST", texture_id, "draw_list_object_set_mesh called with NULL texture_id");
    log_assert("RENDER_LIST", vert_count <= MESH_MAX_VERTS,
               "draw_list_object_set_mesh called with vert_count > MESH_MAX_VERTS");
    log_assert("RENDER_LIST", idx_count <= MESH_MAX_INDICES,
               "draw_list_object_set_mesh called with idx_count > MESH_MAX_INDICES");

    object->type = DL_MESH;
    EseDrawListMesh *mesh_data = &object->data.mesh;

    // Copy vertex data
    memcpy(mesh_data->verts, verts, sizeof(EseDrawListVertex) * vert_count);
    mesh_data->vert_count = vert_count;

    // Copy index data
    memcpy(mesh_data->indices, indices, sizeof(uint32_t) * idx_count);
    mesh_data->idx_count = idx_count;
    strncpy(mesh_data->texture_id, texture_id, TEXTURE_ID_MAX_LEN - 1);
    mesh_data->texture_id[TEXTURE_ID_MAX_LEN - 1] = '\0';
}

void draw_list_object_get_mesh(const EseDrawListObject *object, const EseDrawListVertex **verts,
                               size_t *vert_count, const uint32_t **indices, size_t *idx_count,
                               const char **texture_id) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_mesh called with NULL object");
    log_assert("RENDER_LIST", object->type == DL_MESH,
               "draw_list_object_get_mesh called on non-mesh object");

    const EseDrawListMesh *mesh_data = &object->data.mesh;

    if (verts)
        *verts = mesh_data->verts;
    if (vert_count)
        *vert_count = mesh_data->vert_count;
    if (indices)
        *indices = mesh_data->indices;
    if (idx_count)
        *idx_count = mesh_data->idx_count;
    if (texture_id)
        *texture_id = mesh_data->texture_id;
}

// Scissor/clipping functions
void draw_list_object_set_scissor(EseDrawListObject *object, float scissor_x, float scissor_y,
                                  float scissor_w, float scissor_h) {
    log_assert("RENDER_LIST", object, "draw_list_object_set_scissor called with NULL object");

    object->scissor_active = true;
    object->scissor_x = scissor_x;
    object->scissor_y = scissor_y;
    object->scissor_w = scissor_w;
    object->scissor_h = scissor_h;
}

void draw_list_object_get_scissor(const EseDrawListObject *object, bool *scissor_active,
                                  float *scissor_x, float *scissor_y, float *scissor_w,
                                  float *scissor_h) {
    log_assert("RENDER_LIST", object, "draw_list_object_get_scissor called with NULL object");

    if (scissor_active)
        *scissor_active = object->scissor_active;
    if (scissor_x)
        *scissor_x = object->scissor_x;
    if (scissor_y)
        *scissor_y = object->scissor_y;
    if (scissor_w)
        *scissor_w = object->scissor_w;
    if (scissor_h)
        *scissor_h = object->scissor_h;
}

void draw_list_object_clear_scissor(EseDrawListObject *object) {
    log_assert("RENDER_LIST", object, "draw_list_object_clear_scissor called with NULL object");

    object->scissor_active = false;
    object->scissor_x = 0.0f;
    object->scissor_y = 0.0f;
    object->scissor_w = 0.0f;
    object->scissor_h = 0.0f;
}
