#ifndef ESE_WINDOW_H
#define ESE_WINDOW_H

#include <stdbool.h>

// Forward declarations - platform agnostic
typedef struct EseRenderer EseRenderer;
typedef struct EseInputState EseInputState;

/**
 * @brief Platform-agnostic window interface.
 * 
 * @details This structure provides a unified interface for window management
 *          across different platforms (macOS, Linux). It stores platform-specific
 *          window handles, dimensions, and references to renderer and input
 *          state for consistent window behavior.
 */
typedef struct EseWindow {
    void* platform_window;          /**< Platform-specific window handle (GLFWwindow* or NSWindow*) */
    int width;                      /**< Window width in pixels */
    int height;                     /**< Window height in pixels */
    EseRenderer* renderer;          /**< Reference to the renderer for this window */
    EseInputState* input_state;     /**< Reference to the input state for this window */
    bool should_close;              /**< Flag indicating if the window should close */
} EseWindow;

// Window management functions - platform agnostic interface
EseWindow* window_create(int width, int height, const char* title);
void window_destroy(EseWindow* window);
void window_set_renderer(EseWindow* window, EseRenderer* renderer);
void window_process(EseWindow* window, EseInputState* out_input_state);
void window_close(EseWindow* window);
bool window_should_close(EseWindow* window);

#endif // ESE_WINDOW_H
