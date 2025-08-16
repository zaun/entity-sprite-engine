#include <stdlib.h>
#include <string.h>
#include "utility/log.h"
#include "core/memory_manager.h"
#include "graphics/sprite.h"

typedef struct {
    const char* texture_id;
    float x1;   // normalized coord1-x texture position 
    float y1;   // normalized coord1-y texture position
    float x2;   // normalized coord2-x texture position 
    float y2;   // normalized coord2-y texture position
    int w;      // pixel width
    int h;      // pixel height
} EseSpriteFrame;

struct EseSprite {
    EseSpriteFrame** frames;
    int frame_count;
    float speed;
};

EseSprite *sprite_create() {
    EseSprite *sprite = memory_manager.malloc(sizeof(EseSprite), MMTAG_SPRITE);

    sprite->frames = NULL;
    sprite->frame_count = 0;
    sprite->speed = 100;

    return sprite;
}

void sprite_free(EseSprite *sprite) {
    log_assert("SPRITE", sprite, "sprite_free called with NULL sprite");

    if (sprite->frames) {
        for (int i = 0; i < sprite->frame_count; ++i) {
            if (sprite->frames[i]) {
                memory_manager.free((char*)sprite->frames[i]->texture_id);
                memory_manager.free(sprite->frames[i]);
            }
        }
        memory_manager.free(sprite->frames);
    }
    memory_manager.free(sprite);
}

void sprite_add_frame(
    EseSprite* sprite, const char* texture_id,
    float x1, float y1, float x2, float y2,
    int w, int h
) {
    log_assert("SPRITE", sprite, "sprite_add_frame called with NULL sprite");
    log_assert("SPRITE", texture_id, "sprite_add_frame called with NULL texture_id");

    EseSpriteFrame** new_frames = memory_manager.realloc(
        sprite->frames,
        sizeof(EseSpriteFrame*) * (sprite->frame_count + 1),
        MMTAG_SPRITE
    );

    EseSpriteFrame* frame = memory_manager.malloc(sizeof(EseSpriteFrame), MMTAG_SPRITE);
    frame->texture_id = memory_manager.strdup(texture_id, MMTAG_SPRITE);
    frame->x1 = x1;
    frame->y1 = y1;
    frame->x2 = x2;
    frame->y2 = y2;
    frame->w = w;
    frame->h = h;

    sprite->frames = new_frames;
    sprite->frames[sprite->frame_count] = frame;
    sprite->frame_count += 1;
}

void sprite_get_frame(
    EseSprite *sprite, size_t frame, const char **out_texture_id,
    float *out_x1, float *out_y1, float *out_x2, float *out_y2,
    int *out_w, int *out_h
) {
    log_assert("SPRITE", sprite, "sprite_get_frame called with NULL sprite");
    log_assert("SPRITE", sprite->frames, "sprite_get_frame called with sprite with NULL frames array");
    log_assert("SPRITE", frame < sprite->frame_count, "sprite_get_frame max frames %d", frame);

    EseSpriteFrame *sprite_frame = sprite->frames[frame];
    if (out_texture_id) *out_texture_id = sprite_frame->texture_id;
    if (out_x1) *out_x1 = sprite_frame->x1;
    if (out_y1) *out_y1 = sprite_frame->y1;
    if (out_x2) *out_x2 = sprite_frame->x2;
    if (out_y2) *out_y2 = sprite_frame->y2;
    if (out_w) *out_w = sprite_frame->w;
    if (out_h) *out_h = sprite_frame->h;
}

int sprite_get_frame_count(EseSprite *sprite) {
    log_assert("SPRITE", sprite, "sprite_get_frame_count called with NULL sprite");

    return sprite->frame_count;
}

bool sprite_set_speed(EseSprite *sprite, float speed) {
    log_assert("SPRITE", sprite, "sprite_set_speed called with NULL sprite");

    sprite->speed = speed;
    return true;
}

float sprite_get_speed(EseSprite *sprite) {
    log_assert("SPRITE", sprite, "sprite_get_speed called with NULL sprite");

    return sprite->speed;
}
