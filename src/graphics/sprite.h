#ifndef ESE_SPRITE_H
#define ESE_SPRITE_H

#include <stdbool.h>
#include <stdlib.h>

// Forward declarations
typedef struct EseSprite EseSprite;

EseSprite *sprite_create();
void sprite_free(EseSprite *sprite);

void sprite_add_frame(
    EseSprite* sprite, const char* texture_id,
    float x1, float y1, float x2, float y2,
    int w, int h
);
void sprite_get_frame(
    EseSprite *sprite, size_t frame, const char **out_texture_id,
    float *out_x1, float *out_y1, float *out_x2, float *out_y2,
    int *out_w, int *out_h
);
int sprite_get_frame_count(EseSprite *sprite);

bool sprite_set_speed(EseSprite *sprite, float speed);
float sprite_get_speed(EseSprite *sprite);

#endif // ESE_SPRITE_H