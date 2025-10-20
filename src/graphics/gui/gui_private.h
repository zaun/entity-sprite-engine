#ifndef ESE_GUI_PRIVATE_H
#define ESE_GUI_PRIVATE_H

#include <stdbool.h>
#include <stdlib.h>
#include "graphics/gui/gui.h"
#include "types/types.h"

#define MAX_DRAW_COMMANDS 1024
#define MAX_LAYOUT_STACK 64

typedef struct EseColor EseColor;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseGuiStyle EseGuiStyle;

#define GUI_AUTO_SIZE -1

typedef enum EseGuiWidgetType {
    ESE_GUI_WIDGET_NONE,
    ESE_GUI_WIDGET_FLEX,
    ESE_GUI_WIDGET_STACK,
    ESE_GUI_WIDGET_BUTTON,
    ESE_GUI_WIDGET_IMAGE,
} EseGuiWidgetType;

typedef enum EseGuiColorType {
    // All Widgets
    ESE_GUI_COLOR_BORDER,
    ESE_GUI_COLOR_BORDER_HOVERED,
    ESE_GUI_COLOR_BORDER_PRESSED,
    ESE_GUI_COLOR_BACKGROUND,
    ESE_GUI_COLOR_BACKGROUND_HOVERED,
    ESE_GUI_COLOR_BACKGROUND_PRESSED,

    // Buttons
    ESE_GUI_COLOR_BUTTON_TEXT,
    ESE_GUI_COLOR_BUTTON_TEXT_HOVERED,
    ESE_GUI_COLOR_BUTTON_TEXT_PRESSED,

    // Max Colors
    ESE_GUI_COLOR_MAX,
} EseGuiColorType;

typedef struct EseGuiLayout {
    uint64_t z_index;
    int x, y, width, height;

    // Root of the layout tree for this frame session
    struct EseGuiLayoutNode *root;

    // Current open container in this frame
    struct EseGuiLayoutNode *current_container;

    // Draw scissor state for the layout
    bool draw_scissors_active;
    float draw_scissors_x, draw_scissors_y, draw_scissors_w, draw_scissors_h;
} EseGuiLayout;

typedef struct EseGuiLayoutNode {
    int x, y, width, height;
    struct EseGuiLayoutNode *parent;

    // Children
    struct EseGuiLayoutNode **children;
    size_t children_count;
    size_t children_capacity;

    // Common widget data
    bool is_hovered;
    bool is_down;
    EseColor *colors[ESE_GUI_COLOR_MAX]; // we own the colors

    // Widget-specific data
    EseGuiWidgetType widget_type;
    union {
        struct {
            // Container-specific fields (FLEX and STACK)
            EseGuiFlexDirection direction;
            EseGuiFlexJustify justify;
            EseGuiFlexAlignItems align_items;
            int spacing;
            int padding_left, padding_top, padding_right, padding_bottom;
        } container;
        struct {
            // Button-specific fields
            char *text;
            void (*callback)(void *userdata);
            void *userdata;
        } button;
        struct {
            // Image-specific fields
            char *sprite_id;
            EseGuiImageFit fit;
        } image;
    } widget_data;
} EseGuiLayoutNode;

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

    EseGuiStyle *current_style;

    EseLuaEngine *engine;
};

void _ese_gui_layout_destroy(EseGuiLayout *layout);
void _ese_gui_calculate_node_position(EseGui *gui, EseGuiLayout *session, EseGuiLayoutNode *node, size_t depth);
void _ese_gui_generate_draw_commands(EseGui *gui, EseDrawList *draw_list, EseGuiLayout *session, EseGuiLayoutNode *node, size_t depth);
void _ese_gui_copy_colors_from_style(EseGuiLayoutNode *node, EseGuiStyle *style);
void _ese_gui_free_node_colors(EseGuiLayoutNode *node);

#endif // ESE_GUI_PRIVATE_H
