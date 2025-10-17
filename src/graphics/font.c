// font.c
#include "graphics/font.h"
#include "core/asset_manager.h"
#include "core/memory_manager.h"
#include "graphics/sprite.h"
#include "platform/renderer.h"
#include "utility/log.h"
#include <stdio.h>
#include <string.h>

// Font constants (matching console font)
#define FONT_CHAR_WIDTH 10
#define FONT_CHAR_HEIGHT 20
#define FONT_SPACING 1

void font_draw_text(EseAssetManager *am, const char *font, const char *text,
                                 float start_x, float start_y, uint64_t draw_order,
                                 FontDrawTextureCallback texCallback, void *callback_user_data)
{
    log_assert("FONT", am, "font_draw_text called with NULL asset manager");
    log_assert("FONT", font, "font_draw_text called with NULL font name");
    log_assert("FONT", text, "font_draw_text called with NULL text");
    log_assert("FONT", texCallback, "font_draw_text called with NULL callback");

    if (strlen(text) == 0) {
        return;
    }

    // Draw each character
    float char_x = start_x;
    for (int i = 0; text[i]; i++) {
        char c = text[i];
        if (c >= 32 && c <= 126) { // Printable ASCII
            char sprite_name[64];
            snprintf(sprite_name, sizeof(sprite_name), "fonts:%s_%03d", font, (int)c);
            
            // Get the sprite from asset manager (same as original)
            EseSprite *letter = asset_manager_get_sprite(am, sprite_name);
            
            if (letter) {
                const char *texture_id;
                float x1, y1, x2, y2;
                int w, h;
                
                sprite_get_frame(letter, 0, &texture_id, &x1, &y1, &x2, &y2, &w, &h);
                texCallback(
                    (int)char_x, (int)start_y, w, h,
                    draw_order,
                    texture_id, x1, y1, x2, y2, w, h,
                    callback_user_data
                );
            }
        }
        char_x += FONT_CHAR_WIDTH + FONT_SPACING;
    }
}

void font_draw_text_scaled(EseAssetManager *am, const char *font, const char *text,
                           float start_x, float start_y, uint64_t draw_order,
                           float target_height, FontDrawTextureCallback texCallback, void *user_data)
{
    log_assert("FONT", am, "font_draw_text_scaled called with NULL asset manager");
    log_assert("FONT", font, "font_draw_text_scaled called with NULL font name");
    log_assert("FONT", text, "font_draw_text_scaled called with NULL text");
    log_assert("FONT", texCallback, "font_draw_text_scaled called with NULL callback");

    if (strlen(text) == 0) {
        return;
    }

    // Calculate scaling factor based on target height
    float scale = target_height / FONT_CHAR_HEIGHT;
    float scaled_char_width = (FONT_CHAR_WIDTH + FONT_SPACING) * scale;

    // Draw each character
    float char_x = start_x;
    for (int i = 0; text[i]; i++) {
        char c = text[i];
        if (c >= 32 && c <= 126) { // Printable ASCII
            char sprite_name[64];
            snprintf(sprite_name, sizeof(sprite_name), "fonts:%s_%03d", font, (int)c);
            
            // Get the sprite from asset manager
            EseSprite *letter = asset_manager_get_sprite(am, sprite_name);
            
            if (letter) {
                const char *texture_id;
                float x1, y1, x2, y2;
                int w, h;
                
                sprite_get_frame(letter, 0, &texture_id, &x1, &y1, &x2, &y2, &w, &h);
                
                // Scale the dimensions
                int scaled_w = (int)(w * scale);
                int scaled_h = (int)(h * scale);
                
                texCallback(
                    (int)char_x, (int)start_y, scaled_w, scaled_h,
                    draw_order,
                    texture_id, x1, y1, x2, y2, w, h,
                    user_data
                );
            }
        }
        char_x += scaled_char_width;
    }
}
