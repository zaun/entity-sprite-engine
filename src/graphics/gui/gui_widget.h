#ifndef ESE_GUI_WIDGET_H
#define ESE_GUI_WIDGET_H

#include "types/gui_style.h"   // for EseGuiStyleVariant
#include "types/input_state.h" // for EseInputMouseButton
#include <stdbool.h>

typedef struct EseDrawList EseDrawList;
typedef struct EseGui EseGui;
typedef struct EseGuiLayout EseGuiLayout;
typedef struct EseGuiWidget EseGuiWidget;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseGuiStyle EseGuiStyle;

/**
 * @brief Virtual function table for widget operations.
 *
 * @details This structure contains function pointers for all widget operations,
 *          allowing polymorphic behavior without large switch statements.
 */
typedef struct GuiWidgetVTable {
    char id[64];
    bool is_container;
    void (*draw)(EseGui *gui, EseGuiWidget *widget, EseDrawList *draw_list, size_t depth);
    void (*process_mouse_hover)(EseGuiWidget *widget, int mouse_x, int mouse_y);
    bool (*process_mouse_click)(EseGuiWidget *widget, int mouse_x, int mouse_y,
                                EseInputMouseButton button);
    void (*layout)(EseGuiWidget *widget);
    EseGuiWidget *(*create)(EseGuiWidget *parent, EseGuiStyle *style);
    void (*destroy)(EseGuiWidget *widget);
    int (*lua_init)(EseLuaEngine *engine);
} GuiWidgetVTable;

typedef struct EseGuiWidget {
    // Widget bounding box
    int x, y, width, height;

    // Parent
    struct EseGuiWidget *parent;

    // Children
    struct EseGuiWidget **children;
    size_t children_count;
    size_t children_capacity;

    // Common widget data
    EseGuiStyle *style;
    EseGuiStyleVariant variant;
    bool is_hovered;
    bool is_down;

    // Widget-specific data
    GuiWidgetVTable type;
    void *data;
} EseGuiWidget;

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Widget registry
void _ese_widget_register(EseLuaEngine *engine);

// Widget operations
void _ese_widget_draw(EseGui *gui, EseGuiWidget *widget, EseDrawList *draw_list, size_t depth);
void _ese_widget_process_mouse_hover(EseGuiWidget *widget, int mouse_x, int mouse_y);
void _ese_widget_process_mouse_clicked(EseGuiWidget *widget, int mouse_x, int mouse_y,
                                       EseInputMouseButton button);

// Widget creation
void _ese_widget_layout(EseGuiWidget *widget, int x, int y, int width, int height);

#endif // ESE_GUI_WIDGET_H
