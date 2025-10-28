#ifndef ESE_GUI_PRIVATE_H
#define ESE_GUI_PRIVATE_H

#include "graphics/gui/gui.h"
#include "graphics/gui/gui_widget.h"
#include "types/types.h"
#include <stdbool.h>
#include <stdlib.h>

#define MAX_DRAW_COMMANDS 1024
#define MAX_LAYOUT_STACK 64
#define MAX_VARIANT_STACK 32
#define GUI_AUTO_SIZE -1

typedef struct EseColor EseColor;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseGuiStyle EseGuiStyle;

// Back-compat alias for legacy tests/usage that referenced layout nodes
typedef struct EseGuiWidget EseGuiLayoutNode;

typedef struct EseGuiLayout {
  uint64_t z_index;
  int x, y, width, height;

  // Root of the layout tree for this frame session
  struct EseGuiWidget *root;

  // Current open container in this frame
  struct EseGuiWidget *current_widget;

  // Draw scissor state for the layout
  bool draw_scissors_active;
  float draw_scissors_x, draw_scissors_y, draw_scissors_w, draw_scissors_h;

  // Variant stack for styling
  EseGuiStyleVariant variant_stack[MAX_VARIANT_STACK];
  size_t variant_stack_count;
} EseGuiLayout;

struct EseGui {
  // A GUI Layout in a region of the screen
  // A GUI can have multiple layouts, each in a different region of the screen
  // The open layout is the one that is currently being created
  // as this is an immediate mode system, we only have one open layout at a time
  EseGuiLayout *layouts;
  size_t layouts_count;
  size_t layouts_capacity;
  EseGuiLayout *open_layout;

  // Input state
  EseInputState *input_state;

  // Draw command iteration state
  size_t draw_iterator;
  bool iterator_started;

  EseGuiStyle *default_style;

  EseLuaEngine *engine;
};

void _ese_gui_layout_destroy(EseGuiLayout *layout);

#endif // ESE_GUI_PRIVATE_H
