#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "utility/log.h"
#include "core/memory_manager.h"
#include "graphics/draw_list.h"

#define DRAW_LIST_INITIAL_CAPACITY 256
#define TEXTURE_ID_MAX_LEN 256

typedef struct EseDrawListTexture {
    // Texture to draw
    char texture_id[TEXTURE_ID_MAX_LEN];
    float texture_x1;
    float texture_y1;
    float texture_x2;
    float texture_y2;
} EseDrawListTexture;

typedef struct EseDrawListRect {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    bool filled;
} EseDrawListRect;

struct EseDrawListObject {
    EseDrawListObjectType type;
    union {
        EseDrawListTexture texture;
        EseDrawListRect rect;
    } data;

    // Where to draw
    float x;
    float y;
    int w;
    int h;

    int z_index;
};

struct EseDrawList {
    EseDrawListObject **objects;    // Pool of pointers to objects
    size_t objects_count;           // Number of objects in use this frame
    size_t objects_capacity;        // Total allocated
};

static int _compare_draw_list_object_z(const void *a, const void *b) {
    const EseDrawListObject *obj_a = *(const EseDrawListObject **)a;
    const EseDrawListObject *obj_b = *(const EseDrawListObject **)b;
    int za = draw_list_object_get_z_index(obj_a);
    int zb = draw_list_object_get_z_index(obj_b);
    return (za > zb) - (za < zb);
}

EseDrawList* draw_list_create(void) {
    EseDrawList *draw_list = memory_manager.malloc(sizeof(EseDrawList), MMTAG_RENDER);
    draw_list->objects = memory_manager.malloc(sizeof(EseDrawListObject*) * DRAW_LIST_INITIAL_CAPACITY, MMTAG_RENDER);

    draw_list->objects_capacity = DRAW_LIST_INITIAL_CAPACITY;
    draw_list->objects_count = 0;

    // Pre-allocate objects
    for (size_t i = 0; i < draw_list->objects_capacity; ++i) {
        draw_list->objects[i] = memory_manager.calloc(1, sizeof(EseDrawListObject), MMTAG_RENDER);
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
        EseDrawListObject **new_objs = memory_manager.realloc(draw_list->objects, sizeof(EseDrawListObject*) * new_capacity, MMTAG_RENDER);
        if (!new_objs) {
            log_error("RENDER_LIST", "Error: Failed to grow object pool");
            return NULL;
        }
        draw_list->objects = new_objs;
        // Allocate new objects
        for (size_t i = draw_list->objects_capacity; i < new_capacity; ++i) {
            draw_list->objects[i] = calloc(1, sizeof(EseDrawListObject));
        }
        draw_list->objects_capacity = new_capacity;
    }
    EseDrawListObject *obj = draw_list->objects[draw_list->objects_count++];

    memset(obj, 0, sizeof(EseDrawListObject));
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

void draw_list_object_set_rect(EseDrawListObject* object, unsigned char r, unsigned char g, unsigned char b, unsigned char a, bool filled) {
    log_assert("RENDER_LIST", object, "draw_list_object_set_rect_color called with NULL object");

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
