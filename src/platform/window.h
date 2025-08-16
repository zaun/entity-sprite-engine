#ifndef ESE_WINDOW_H
#define ESE_WINDOW_H

#include <stdbool.h>

// Forward declarations - platform agnostic
typedef struct EseRenderer EseRenderer;
typedef struct EseInputState EseInputState;

typedef struct EseWindow {
    void* platform_window;  // GLFWwindow* or NSWindow*
    int width;
    int height;
    EseRenderer* renderer;
    EseInputState* input_state;
    bool should_close;
} EseWindow;

// Window management functions - platform agnostic interface
EseWindow* window_create(int width, int height, const char* title);
void window_destroy(EseWindow* window);
void window_set_renderer(EseWindow* window, EseRenderer* renderer);
void window_process(EseWindow* window, EseInputState* out_input_state);
void window_close(EseWindow* window);
bool window_should_close(EseWindow* window);

#endif // ESE_WINDOW_H
