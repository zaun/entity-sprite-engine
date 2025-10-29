#ifndef ESE_CONSOLE_H
#define ESE_CONSOLE_H

#include "entity/entity.h"
#include <stddef.h>

// Forward declarations
typedef struct EseAssetManager EseAssetManager;
typedef struct EseDrawList EseDrawList;

/**
 * @brief Console line type enumeration.
 *
 * @details Defines the different types of console lines that can be displayed,
 *          each with different visual styling and importance levels.
 */
typedef enum {
    ESE_CONSOLE_NORMAL, /** Standard console output line */
    ESE_CONSOLE_INFO,   /** Informational message */
    ESE_CONSOLE_WARN,   /** Warning message */
    ESE_CONSOLE_ERROR   /** Error message */
} EseConsoleLineType;

/**
 * @brief Opaque console structure.
 *
 * @details The console manages a history of text lines with configurable
 * display parameters. Internal structure is private to the implementation.
 */
typedef struct EseConsole EseConsole;

/**
 * @brief Creates a new console instance.
 *
 * @details Allocates and initializes a new console with default settings.
 *          The console starts with an empty history and default display
 * parameters.
 *
 * @return A pointer to the newly created console instance.
 */
EseConsole *console_create(void);

/**
 * @brief Destroys a console instance and frees its resources.
 *
 * @details Safely deallocates all memory associated with the console, including
 *          the history buffer and all stored messages. Safe to call with NULL.
 *
 * @param console Pointer to the console to destroy, or NULL.
 */
void console_destroy(EseConsole *console);

/**
 * @brief Adds a new line to the console history.
 *
 * @details Adds a new console line with the specified type, prefix, and
 * message. If the history exceeds 1000 lines, the oldest line is automatically
 * removed. The prefix is truncated to 16 characters if longer.
 *
 * @param console Pointer to the console instance.
 * @param type The type of console line (normal, info, warn, error).
 * @param prefix The prefix for the line (max 16 characters).
 * @param message The message text to display.
 */
void console_add_line(EseConsole *console, EseConsoleLineType type, const char *prefix,
                      const char *message);

/**
 * @brief Draws the console to the screen.
 *
 * @details Renders the console's visible lines according to the current display
 *          parameters (draw_line_count and start_at_index).
 *
 * @param console Pointer to the console instance.
 * @param manager Pointer to the asset manager.
 * @param view_width The width of the view.
 * @param view_height The height of the view.
 * @param texCallback Callback function for drawing textures.
 * @param rectCallback Callback function for drawing rectangles.
 * @param draw_list Pointer to the draw list.
 */
void console_draw(EseConsole *console, EseAssetManager *manager, int view_width, int view_height,
                  EntityDrawCallbacks *callbacks, void *user_data);

/**
 * @brief Sets the number of console lines to display.
 *
 * @param console Pointer to the console instance.
 * @param line_count Number of lines to display (0 to hide console).
 */
void console_set_draw_line_count(EseConsole *console, int line_count);

#endif // ESE_CONSOLE_H
