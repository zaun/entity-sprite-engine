#include "platform/window.h"
#include "core/memory_manager.h"
#include "platform/glfw/renderer_private.h"
#include "platform/renderer.h"
#include "platform/renderer_private.h"
#include "types/input_state.h"
#include "types/input_state_private.h"
#include "utility/log.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Platform-specific window structure for GLFW implementation.
 *
 * @details This structure wraps the GLFW window handle and input state
 *          for the GLFW platform. It provides the bridge between GLFW
 *          window events and the engine's input system.
 */
typedef struct EseGLFWWindow {
  GLFWwindow *glfw_window;   /** GLFW window handle */
  EseInputState *inputState; /** Reference to the engine's input state */
} EseGLFWWindow;

// Map GLFW keycodes to your engine's keys
static EseInputKey mapGLFWKeyToInputKey(int key) {
  switch (key) {
  case GLFW_KEY_A:
    return InputKey_A;
  case GLFW_KEY_B:
    return InputKey_B;
  case GLFW_KEY_C:
    return InputKey_C;
  case GLFW_KEY_D:
    return InputKey_D;
  case GLFW_KEY_E:
    return InputKey_E;
  case GLFW_KEY_F:
    return InputKey_F;
  case GLFW_KEY_G:
    return InputKey_G;
  case GLFW_KEY_H:
    return InputKey_H;
  case GLFW_KEY_I:
    return InputKey_I;
  case GLFW_KEY_J:
    return InputKey_J;
  case GLFW_KEY_K:
    return InputKey_K;
  case GLFW_KEY_L:
    return InputKey_L;
  case GLFW_KEY_M:
    return InputKey_M;
  case GLFW_KEY_N:
    return InputKey_N;
  case GLFW_KEY_O:
    return InputKey_O;
  case GLFW_KEY_P:
    return InputKey_P;
  case GLFW_KEY_Q:
    return InputKey_Q;
  case GLFW_KEY_R:
    return InputKey_R;
  case GLFW_KEY_S:
    return InputKey_S;
  case GLFW_KEY_T:
    return InputKey_T;
  case GLFW_KEY_U:
    return InputKey_U;
  case GLFW_KEY_V:
    return InputKey_V;
  case GLFW_KEY_W:
    return InputKey_W;
  case GLFW_KEY_X:
    return InputKey_X;
  case GLFW_KEY_Y:
    return InputKey_Y;
  case GLFW_KEY_Z:
    return InputKey_Z;

  case GLFW_KEY_0:
    return InputKey_0;
  case GLFW_KEY_1:
    return InputKey_1;
  case GLFW_KEY_2:
    return InputKey_2;
  case GLFW_KEY_3:
    return InputKey_3;
  case GLFW_KEY_4:
    return InputKey_4;
  case GLFW_KEY_5:
    return InputKey_5;
  case GLFW_KEY_6:
    return InputKey_6;
  case GLFW_KEY_7:
    return InputKey_7;
  case GLFW_KEY_8:
    return InputKey_8;
  case GLFW_KEY_9:
    return InputKey_9;

  case GLFW_KEY_F1:
    return InputKey_F1;
  case GLFW_KEY_F2:
    return InputKey_F2;
  case GLFW_KEY_F3:
    return InputKey_F3;
  case GLFW_KEY_F4:
    return InputKey_F4;
  case GLFW_KEY_F5:
    return InputKey_F5;
  case GLFW_KEY_F6:
    return InputKey_F6;
  case GLFW_KEY_F7:
    return InputKey_F7;
  case GLFW_KEY_F8:
    return InputKey_F8;
  case GLFW_KEY_F9:
    return InputKey_F9;
  case GLFW_KEY_F10:
    return InputKey_F10;
  case GLFW_KEY_F11:
    return InputKey_F11;
  case GLFW_KEY_F12:
    return InputKey_F12;

  case GLFW_KEY_UP:
    return InputKey_UP;
  case GLFW_KEY_DOWN:
    return InputKey_DOWN;
  case GLFW_KEY_LEFT:
    return InputKey_LEFT;
  case GLFW_KEY_RIGHT:
    return InputKey_RIGHT;

  case GLFW_KEY_SPACE:
    return InputKey_SPACE;
  case GLFW_KEY_ENTER:
    return InputKey_ENTER;
  case GLFW_KEY_ESCAPE:
    return InputKey_ESCAPE;
  case GLFW_KEY_TAB:
    return InputKey_TAB;
  case GLFW_KEY_BACKSPACE:
    return InputKey_BACKSPACE;
  case GLFW_KEY_CAPS_LOCK:
    return InputKey_CAPSLOCK;

  default:
    return InputKey_UNKNOWN;
  }
}

// Callback: key events
static void glfw_key_callback(GLFWwindow *w, int key, int scancode, int action,
                              int mods) {
  EseGLFWWindow *pw = (EseGLFWWindow *)glfwGetWindowUserPointer(w);
  if (!pw || !pw->inputState)
    return;

  // update modifiers
  pw->inputState->keys_down[InputKey_LSHIFT] = (mods & GLFW_MOD_SHIFT) != 0;
  pw->inputState->keys_down[InputKey_RSHIFT] = (mods & GLFW_MOD_SHIFT) != 0;

  pw->inputState->keys_down[InputKey_LCTRL] = (mods & GLFW_MOD_CONTROL) != 0;
  pw->inputState->keys_down[InputKey_RCTRL] = (mods & GLFW_MOD_CONTROL) != 0;

  pw->inputState->keys_down[InputKey_LALT] = (mods & GLFW_MOD_ALT) != 0;
  pw->inputState->keys_down[InputKey_RALT] = (mods & GLFW_MOD_ALT) != 0;

  pw->inputState->keys_down[InputKey_LCMD] = (mods & GLFW_MOD_SUPER) != 0;
  pw->inputState->keys_down[InputKey_RCMD] = (mods & GLFW_MOD_SUPER) != 0;

  pw->inputState->keys_down[InputKey_CAPSLOCK] =
      (mods & GLFW_MOD_CAPS_LOCK) != 0;

  if (key == GLFW_KEY_UNKNOWN)
    return;

  EseInputKey ik = mapGLFWKeyToInputKey(key);
  if (ik == InputKey_UNKNOWN || ik >= InputKey_MAX)
    return;

  if (action == GLFW_PRESS) {
    pw->inputState->keys_down[ik] = true;
    pw->inputState->keys_pressed[ik] = true;
  } else if (action == GLFW_RELEASE) {
    pw->inputState->keys_down[ik] = false;
    pw->inputState->keys_released[ik] = true;
  }
}

// Callback: mouse buttons
static void glfw_mouse_button_callback(GLFWwindow *w, int button, int action,
                                       int mods) {
  EseGLFWWindow *pw = (EseGLFWWindow *)glfwGetWindowUserPointer(w);
  if (!pw || !pw->inputState)
    return;

  int i = -1;
  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    i = InputMouse_LEFT;
  } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
    i = InputMouse_RIGHT;
  } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
    i = InputMouse_MIDDLE;
  } else if (button == GLFW_MOUSE_BUTTON_X1) {
    i = InputMouse_X1;
  } else if (button == GLFW_MOUSE_BUTTON_X2) {
    i = InputMouse_X2;
  }

  if (i == -1)
    return;

  if (action == GLFW_PRESS) {
    if (!pw->inputState->mouse_down[i]) {
      pw->inputState->mouse_clicked[i] = true; // first down edge
    }
    pw->inputState->mouse_down[i] = true;
  } else if (action == GLFW_RELEASE) {
    pw->inputState->mouse_down[i] = false;
    pw->inputState->mouse_released[i] = true; // release edge
  }
}

// Callback: cursor position
static void glfw_cursor_pos_callback(GLFWwindow *w, double xpos, double ypos) {
  EseGLFWWindow *pw = (EseGLFWWindow *)glfwGetWindowUserPointer(w);
  if (!pw || !pw->inputState)
    return;

  pw->inputState->mouse_x = (float)xpos;
  pw->inputState->mouse_y = (float)ypos;
}

// Callback: scroll
static void glfw_scroll_callback(GLFWwindow *w, double xoffset,
                                 double yoffset) {
  EseGLFWWindow *pw = (EseGLFWWindow *)glfwGetWindowUserPointer(w);
  if (!pw || !pw->inputState)
    return;

  pw->inputState->mouse_scroll_dx += (float)xoffset;
  pw->inputState->mouse_scroll_dy += (float)yoffset;
}

// Callback: window close
static void glfw_window_close_callback(GLFWwindow *w) {
  EseGLFWWindow *pw = (EseGLFWWindow *)glfwGetWindowUserPointer(w);
  if (!pw)
    return;

  if (pw->glfw_window) {
    glfwSetWindowShouldClose(pw->glfw_window, GLFW_TRUE);
  }
}

EseWindow *window_create(int width, int height, const char *title) {
  if (!glfwInit()) {
    log_error("Failed to initialize GLFW\n");
    return NULL;
  }

  // OpenGL hints (keep compatible with previous file)
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  GLFWwindow *glfwWin =
      glfwCreateWindow(width, height, title ? title : "EseWindow", NULL, NULL);
  if (!glfwWin) {
    fprintf(stderr, "Failed to create GLFW window\n");
    glfwTerminate();
    return NULL;
  }

  glfwMakeContextCurrent(glfwWin);
  glfwSwapInterval(1);

  glewExperimental = GL_TRUE;
  GLenum err = glewInit();
  if (err != GLEW_OK) {
    fprintf(stderr, "Failed to initialize GLEW: %s\n", glewGetErrorString(err));
    glfwDestroyWindow(glfwWin);
    glfwTerminate();
    return NULL;
  }

  EseWindow *win =
      (EseWindow *)memory_manager.malloc(sizeof(EseWindow), MMTAG_WINDOW);
  memset(win, 0, sizeof(EseWindow));
  EseGLFWWindow *pw = (EseGLFWWindow *)memory_manager.malloc(
      sizeof(EseGLFWWindow), MMTAG_WINDOW);
  memset(pw, 0, sizeof(EseGLFWWindow));

  pw->glfw_window = glfwWin;
  // create input state
  pw->inputState = ese_input_state_create(NULL);

  // attach user pointer and callbacks
  glfwSetWindowUserPointer(glfwWin, pw);
  glfwSetKeyCallback(glfwWin, glfw_key_callback);
  glfwSetMouseButtonCallback(glfwWin, glfw_mouse_button_callback);
  glfwSetCursorPosCallback(glfwWin, glfw_cursor_pos_callback);
  glfwSetScrollCallback(glfwWin, glfw_scroll_callback);
  glfwSetWindowCloseCallback(glfwWin, glfw_window_close_callback);

  win->platform_window = pw;
  win->width = width;
  win->height = height;
  win->should_close = false;

  return win;
}

void window_destroy(EseWindow *window) {
  if (!window)
    return;
  if (window->platform_window) {
    EseGLFWWindow *pw = (EseGLFWWindow *)window->platform_window;
    if (pw->glfw_window) {
      glfwDestroyWindow(pw->glfw_window);
      pw->glfw_window = NULL;
    }
    if (pw->inputState) {
      ese_input_state_destroy(pw->inputState);
      pw->inputState = NULL;
    }
    memory_manager.free(pw);
  }
  glfwTerminate();
  memory_manager.free(window);
}

void window_set_renderer(EseWindow *window, EseRenderer *renderer) {
  if (!window)
    return;
  EseGLFWWindow *pw = (EseGLFWWindow *)window->platform_window;
  if (!pw || !pw->glfw_window)
    return;

  window->renderer = renderer;

  if (!renderer) {
    return;
  }

  // Make context current for any GL calls
  glfwMakeContextCurrent(pw->glfw_window);

  if (renderer->hiDPI) {
    // Get the actual framebuffer size (pixels) immediately after window
    // creation
    int framebuffer_width, framebuffer_height;
    glfwGetFramebufferSize(pw->glfw_window, &framebuffer_width,
                           &framebuffer_height);

    // Set the renderer dimensions to the actual framebuffer size
    renderer->view_w = framebuffer_width;
    renderer->view_h = framebuffer_height;

    // Set OpenGL viewport to match
    glViewport(0, 0, framebuffer_width, framebuffer_height);
  } else {
    renderer->view_w = window->width;
    renderer->view_h = window->height;
  }

  // store glfw window in renderer internal if present
  if (renderer->internal) {
    EseGLRenderer *internal = (EseGLRenderer *)renderer->internal;
    internal->window = pw->glfw_window;
  }
}

void window_process(EseWindow *window, EseInputState *out_input_state) {
  if (!window || !out_input_state)
    return;
  EseGLFWWindow *pw = (EseGLFWWindow *)window->platform_window;
  if (!pw || !pw->glfw_window) {
    // Cleanup input state
    memset(pw->inputState->keys_pressed, 0,
           sizeof(pw->inputState->keys_pressed));
    memset(pw->inputState->keys_down, 0, sizeof(pw->inputState->keys_down));
    memset(pw->inputState->keys_released, 0,
           sizeof(pw->inputState->keys_released));
    pw->inputState->mouse_scroll_dx = 0;
    pw->inputState->mouse_scroll_dy = 0;
    memset(pw->inputState->mouse_down, 0, sizeof(pw->inputState->mouse_down));
    memset(pw->inputState->mouse_clicked, 0,
           sizeof(pw->inputState->mouse_clicked));
    memset(pw->inputState->mouse_released, 0,
           sizeof(pw->inputState->mouse_released));

    // copy prefix of input state like macOS code
    size_t prefix = offsetof(EseInputState, state);
    memmove(out_input_state, pw->inputState, prefix);

    return;
  }

  // Poll and swap
  glfwPollEvents();
  glfwMakeContextCurrent(pw->glfw_window);
  glfwSwapBuffers(pw->glfw_window);

  // copy prefix of input state like macOS code
  size_t prefix = offsetof(EseInputState, state);
  memmove(out_input_state, pw->inputState, prefix);

  // reset per-frame data
  memset(pw->inputState->keys_pressed, 0, sizeof(pw->inputState->keys_pressed));
  memset(pw->inputState->keys_released, 0,
         sizeof(pw->inputState->keys_released));
  pw->inputState->mouse_scroll_dx = 0;
  pw->inputState->mouse_scroll_dy = 0;
  memset(pw->inputState->mouse_clicked, 0,
         sizeof(pw->inputState->mouse_clicked));
  memset(pw->inputState->mouse_released, 0,
         sizeof(pw->inputState->mouse_released));

  // Handle window close
  window->should_close = glfwWindowShouldClose(pw->glfw_window);
  if (window->should_close) {
    glfwSetWindowShouldClose(pw->glfw_window, GLFW_TRUE);
    glfwPostEmptyEvent();
    glfwDestroyWindow(pw->glfw_window);
    pw->glfw_window = NULL;
    return;
  }

  if (window->renderer) {
    renderer_draw(window->renderer);
  }
}

void window_close(EseWindow *window) {
  if (!window)
    return;
  EseGLFWWindow *pw = (EseGLFWWindow *)window->platform_window;

  if (pw->glfw_window) {
    glfwSetWindowShouldClose(pw->glfw_window, GLFW_TRUE);
  }

  window->should_close = true;
}

bool window_should_close(EseWindow *window) {
  if (!window)
    return true;
  return window->should_close;
}
