#include <string.h>
#include <limits.h>
#include "console.h"
#include "memory_manager.h"
#include "utility/log.h"
#include "graphics/sprite.h"
#include "core/asset_manager.h"

#define ESE_CONSOLE_MAX_HISTORY 1000

#ifndef ESE_CONSOLE_ESE_CONSOLE_PREFIX_SIZE
#define ESE_CONSOLE_ESE_CONSOLE_PREFIX_SIZE 6
#endif

/**
 * @brief Internal structure representing a single console line.
 * 
 * @details Contains the type, prefix, and message for a console line.
 *          The prefix is limited to ESE_CONSOLE_ESE_CONSOLE_PREFIX_SIZE characters plus null terminator.
 */
typedef struct {
    EseConsoleLineType type;        /**< The type of console line */
    char prefix[ESE_CONSOLE_ESE_CONSOLE_PREFIX_SIZE + 1];   /**< ESE_CONSOLE_ESE_CONSOLE_PREFIX_SIZE chars + null terminator */
    char* message;                  /**< The message text (dynamically allocated) */
} EseConsoleLine;

/**
 * @brief Internal console structure.
 * 
 * @details Manages a circular buffer of console lines with automatic
 *          history management and display parameters.
 */
struct EseConsole {
    EseConsoleLine* history;    /**< Array of console lines */
    size_t history_size;        /**< Current number of lines in history */
    size_t history_capacity;    /**< Maximum number of lines (1000) */
    size_t draw_line_count;     /**< Number of lines to display */
    size_t start_at_index;      /**< Starting index for display */

    int font_char_width;         /**< Width of a single character in pixels */
    int font_char_height;        /**< Height of a single character in pixels */
    int font_spacing;            /**< Spacing between characters and between lines in pixels */
};

EseConsole* console_create(void) {
    EseConsole* console = memory_manager.malloc(sizeof(EseConsole), MMTAG_CONSOLE);
    console->history = memory_manager.malloc(sizeof(EseConsoleLine) * ESE_CONSOLE_MAX_HISTORY, MMTAG_CONSOLE);
    
    console->history_size = 0;
    console->history_capacity = ESE_CONSOLE_MAX_HISTORY;
    console->draw_line_count = 10;
    console->start_at_index = 0;

    console->font_char_width = 10;
    console->font_char_height = 20;
    console->font_spacing = 2;
    
    return console;
}

void console_destroy(EseConsole* console) {
    if (!console) {
        return;
    }
    
    // Free all message strings
    for (size_t i = 0; i < console->history_size; i++) {
        memory_manager.free(console->history[i].message);
    }
    
    // Free history array
    memory_manager.free(console->history);
    
    // Free console structure
    memory_manager.free(console);
}

void console_add_line(EseConsole* console, EseConsoleLineType type, const char* prefix, const char* message) {
    log_assert("CONSOLE", console, "console_add_line called with NULL console");
    log_assert("CONSOLE", prefix, "console_add_line called with NULL prefix");
    log_assert("CONSOLE", message, "console_add_line called with NULL message");
    
    // If we're at capacity, remove the oldest line
    if (console->history_size >= console->history_capacity) {
        // Free the oldest message
        memory_manager.free(console->history[0].message);
        
        // Shift all lines down by one
        memmove(&console->history[0], &console->history[1], 
                (console->history_capacity - 1) * sizeof(EseConsoleLine));
        
        console->history_size--;
        
        // Adjust start_at_index if it was pointing to the removed line
        if (console->start_at_index > 0) {
            console->start_at_index--;
        }
    }
    
    // Add new line at the end
    EseConsoleLine* new_line = &console->history[console->history_size];
    
    new_line->type = type;
    
    // Copy prefix (ensure it fits in ESE_CONSOLE_ESE_CONSOLE_PREFIX_SIZE characters + null terminator)
    strncpy(new_line->prefix, prefix, ESE_CONSOLE_ESE_CONSOLE_PREFIX_SIZE);
    new_line->prefix[ESE_CONSOLE_ESE_CONSOLE_PREFIX_SIZE] = '\0';
    
    // Copy message
    size_t message_len = strlen(message) + 1;
    new_line->message = memory_manager.malloc(message_len, MMTAG_CONSOLE);
    strcpy(new_line->message, message);
    console->history_size++;
}

void console_set_draw_line_count(EseConsole* console, int line_count) {
    log_assert("CONSOLE", console, "console_set_draw_line_count called with NULL console");
    
    if (line_count < 0) {
        line_count = 0;
    }
    
    console->draw_line_count = line_count;
}

void console_draw(EseConsole* console,
    EseAssetManager *manager,
    int view_width,
    int view_height,
    EntityDrawCallbacks *callbacks,
    void *user_data)
{
    log_assert("CONSOLE", console, "console_draw called with NULL console");
    log_assert("CONSOLE", callbacks, "console_draw called with NULL callbacks");
    log_assert("CONSOLE", callbacks->draw_rect, "console_draw called with NULL draw_rect callback");
    

    // Dark mode console colors
    const unsigned char bg_r = 20;   // Very dark gray
    const unsigned char bg_g = 20;   // Very dark gray  
    const unsigned char bg_b = 20;   // Very dark gray
    const unsigned char bg_a = 230;  // Slightly transparent
    
    const unsigned char border_r = 60;  // Dark gray border
    const unsigned char border_g = 60;  // Dark gray border
    const unsigned char border_b = 60;  // Dark gray border
    const unsigned char border_a = 255; // Solid border
    
    // Calculate console height based on draw_line_count
    int line_height = console->font_char_height + console->font_spacing;
    int console_height = console->draw_line_count * line_height;
    
    // Draw main console background rectangle at top of screen
    callbacks->draw_rect(
        0, 0, INT_MAX - 1,
        view_width, console_height, 0.0f, true,
        bg_r, bg_g, bg_b, bg_a, user_data
    );
    
    // Draw bottom border rectangle (2 pixels high)
    int border_y = console_height;
    callbacks->draw_rect(0, border_y, INT_MAX - 1, view_width, 2, 0.0f, true, border_r, border_g, border_b, border_a, user_data);
    
    // Draw console text lines
    int bottom_padding = 5; // Padding from the bottom of the console area
    int num_lines_to_draw = console->draw_line_count < console->history_size ? console->draw_line_count : console->history_size;

    for (int i = 0; i < num_lines_to_draw; i++) {
        // Calculate line_index to show oldest lines first (at the top of the text block)
        int line_index = console->history_size - num_lines_to_draw + i;
        if (line_index < 0) break;

        EseConsoleLine* line = &console->history[line_index];

        // Calculate y_pos to anchor the text block to the bottom of the console area
        // The newest line (i = num_lines_to_draw - 1) will be at console_height - bottom_padding - line_height
        // The oldest line (i = 0) will be at console_height - bottom_padding - (num_lines_to_draw * line_height)
        int y_pos = console_height - bottom_padding - ((num_lines_to_draw - i) * line_height);
        
        // Draw type indicator dot at start of line
        int dot_x = 5;
        int dot_y = y_pos + (console->font_char_height / 2);  // Center vertically with text
        int dot_radius = console->font_char_width / 2;
        
        switch (line->type) {
            case ESE_CONSOLE_INFO:
                // Blue dot
                callbacks->draw_rect(dot_x, dot_y - dot_radius, INT_MAX - 1, dot_radius * 2, dot_radius * 2, 0.0f, true, 0, 100, 255, 255, user_data);
                break;
            case ESE_CONSOLE_WARN:
                // Orange dot
                callbacks->draw_rect(dot_x, dot_y - dot_radius, INT_MAX - 1, dot_radius * 2, dot_radius * 2, 0.0f, true, 255, 165, 0, 255, user_data);
                break;
            case ESE_CONSOLE_ERROR:
                // Red dot
                callbacks->draw_rect(dot_x, dot_y - dot_radius, INT_MAX - 1, dot_radius * 2, dot_radius * 2, 0.0f, true, 255, 0, 0, 255, user_data);
                break;
            case ESE_CONSOLE_NORMAL:
            default:
                // No dot for normal lines
                break;
        }
        
        // Draw prefix (e.g., "SYSTEM:", "INFO:") - always ESE_CONSOLE_ESE_CONSOLE_PREFIX_SIZE chars wide
        int prefix_x = 5 + (dot_radius * 2) + 4;  // Start after the dot with some spacing
        for (int j = 0; j < ESE_CONSOLE_ESE_CONSOLE_PREFIX_SIZE; j++) {  // Always draw exactly ESE_CONSOLE_ESE_CONSOLE_PREFIX_SIZE characters
            char c = line->prefix[j];
            if (c >= 32 && c <= 126) {  // Printable ASCII
                char sprite_name[64];
                snprintf(sprite_name, sizeof(sprite_name), "fonts:console_font_10x20_%03d", (int)c);
                
                EseSprite* letter = asset_manager_get_sprite(manager, sprite_name);
                if (letter) {
                    const char* texture_id;
                    float x1, y1, x2, y2;
                    int w, h;
                    
                    sprite_get_frame(letter, 0, &texture_id, &x1, &y1, &x2, &y2, &w, &h);
                    callbacks->draw_texture(prefix_x, y_pos, w, h, INT_MAX, texture_id, x1, y1, x2, y2, w, h, user_data);
                }
            } else {
                // Draw space for non-printable or missing characters
                char sprite_name[64];
                snprintf(sprite_name, sizeof(sprite_name), "fonts:console_font_10x20_032");  // Space character
                
                EseSprite* letter = asset_manager_get_sprite(manager, sprite_name);
                if (letter) {
                    const char* texture_id;
                    float x1, y1, x2, y2;
                    int w, h;
                    
                    sprite_get_frame(letter, 0, &texture_id, &x1, &y1, &x2, &y2, &w, &h);
                    callbacks->draw_texture(prefix_x, y_pos, w, h, INT_MAX, texture_id, x1, y1, x2, y2, w, h, user_data);
                }
            }
            prefix_x += console->font_char_width + 1;
        }
        
        // Draw separator ": "
        prefix_x += 2;
        
        // Draw message text with line wrapping
        int message_x = prefix_x;
        int current_y = y_pos;
        int available_width = view_width - message_x - 10; // Leave some margin
        int char_width = console->font_char_width + 1;
        int max_chars_per_line = available_width / char_width;
        
        int char_count = 0;
        for (int j = 0; line->message[j]; j++) {
            char c = line->message[j];
            
            // Handle line wrapping
            if (char_count >= max_chars_per_line) {
                // Move to next line
                current_y += console->font_char_height + console->font_spacing;
                message_x = prefix_x;
                char_count = 0;
            }
            
            if (c >= 32 && c <= 126) {  // Printable ASCII
                char sprite_name[64];
                snprintf(sprite_name, sizeof(sprite_name), "fonts:console_font_10x20_%03d", (int)c);
                
                EseSprite* letter = asset_manager_get_sprite(manager, sprite_name);
                if (letter) {
                    const char* texture_id;
                    float x1, y1, x2, y2;
                    int w, h;
                    
                    sprite_get_frame(letter, 0, &texture_id, &x1, &y1, &x2, &y2, &w, &h);
                    callbacks->draw_texture(message_x, current_y, w, h, INT_MAX, texture_id, x1, y1, x2, y2, w, h, user_data);
                }
                message_x += char_width;
                char_count++;
            } else if (c == '\n') {
                // Handle explicit newlines
                current_y += console->font_char_height + console->font_spacing;
                message_x = prefix_x;
                char_count = 0;
            }
        }
    }
}
