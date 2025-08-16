#ifndef ESE_DRAW_LIST_H
#define ESE_DRAW_LIST_H

#include <stdbool.h>
#include <stdlib.h>

typedef struct EseDrawListObject EseDrawListObject;
typedef struct EseDrawList EseDrawList;

typedef enum EseDrawListObjectType {
    RL_TEXTURE,
    RL_RECT
} EseDrawListObjectType;

// Create a new, empty draw_list (with internal object pool)
EseDrawList* draw_list_create(void);

// Free the draw_list and all its objects
void draw_list_free(EseDrawList *draw_list);

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

void draw_list_object_set_rect(EseDrawListObject* object, unsigned char r, unsigned char g, unsigned char b, unsigned char a, bool filled);
void draw_list_object_set_bounds(EseDrawListObject* object, float x, float y, int w, int h);
void draw_list_object_set_z_index(EseDrawListObject* object, int z_index);
EseDrawListObjectType draw_list_object_get_type(const EseDrawListObject* object);
int  draw_list_object_get_z_index(const EseDrawListObject* object);
void draw_list_object_get_bounds(const EseDrawListObject* object, float* x, float* y, int* w, int* h);

void draw_list_object_get_texture(
    const EseDrawListObject* object, const char **texture_id,
    float *texture_x1, float *texture_y1, float *texture_x2, float *texture_y2
);

void draw_list_object_get_rect_color(const EseDrawListObject* object, unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a, bool* filled);

#endif // ESE_DRAW_LIST_H