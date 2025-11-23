#ifndef ESE_FONT_H
#define ESE_FONT_H

#include <stdbool.h>
#include <stdint.h>

// Forward declarations
typedef struct EseEngine EseEngine;
typedef struct EseRenderer EseRenderer;

/**
 * @file font.h
 *
 * @brief External declaration for fonts and rendering functions.
 */

// External declaration for fonts
extern unsigned char console_font_10x20[];
extern unsigned char console_font_8x8_basic[];

// Forward declaration for texture callback (matches EntityDrawTextureCallback)
typedef void (*FontDrawTextureCallback)(float screen_x, float screen_y, float screen_w,
                                        float screen_h, uint64_t z_index, const char *texture_id,
                                        float texture_x1, float texture_y1, float texture_x2,
                                        float texture_y2, int width, int height, void *user_data);

// Font rendering functions
void font_draw_text(EseEngine *engine, const char *font, const char *text, float start_x,
                    float start_y, uint64_t draw_order, FontDrawTextureCallback texCallback,
                    void *callback_user_data);

void font_draw_text_scaled(EseEngine *engine, const char *font, const char *text, float start_x,
                           float start_y, uint64_t draw_order, float target_height,
                           FontDrawTextureCallback texCallback, void *user_data);

#endif // ESE_FONT_H
