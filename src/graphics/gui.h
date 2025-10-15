#ifndef ESE_GUI_H
#define ESE_GUI_H

typedef struct EseGui EseGui;
typedef struct EseColor EseColor;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseInputState EseInputState;
typedef struct EseDrawList EseDrawList;

typedef enum EseGuiFlexDirection {
    FLEX_DIRECTION_ROW,
    FLEX_DIRECTION_COLUMN,
    ESE_GUI_FLEX_DIRECTION_MAX,
} EseGuiFlexDirection;

typedef enum EseGuiFlexJustify {
    FLEX_JUSTIFY_START,
    FLEX_JUSTIFY_CENTER,
    FLEX_JUSTIFY_END,
    ESE_GUI_FLEX_JUSTIFY_MAX,
} EseGuiFlexJustify;

typedef enum EseGuiFlexAlignItems {
    FLEX_ALIGN_ITEMS_START,
    FLEX_ALIGN_ITEMS_CENTER,
    FLEX_ALIGN_ITEMS_END,
    ESE_GUI_FLEX_ALIGN_ITEMS_MAX,
} EseGuiFlexAlignItems;

typedef enum EseGuiImageFit {
    IMAGE_FIT_COVER,
    IMAGE_FIT_CONTAIN,
    IMAGE_FIT_FILL,
    IMAGE_FIT_REPEAT,
    IMAGE_FIT_NONE,
    ESE_GUI_IMAGE_FIT_MAX,
} EseGuiImageFit;

/* --- Context management ---------------------------------------------------------------------- */
EseGui *ese_gui_create(EseLuaEngine *engine);
void ese_gui_destroy(EseGui *gui);

/* --- UI Processing --------------------------------------------------------------------------- */
// Injects raw input events into the GUI system for processing within a specific context.
void ese_gui_input(EseGui *gui, EseInputState *input_state);
// Processes all layout calculations, handles input, executes callbacks for interactive elements,
// and queues drawing commands for the current session.
void ese_gui_process(EseGui *gui, EseDrawList *draw_list);

/* --- Layout Management ------------------------------------------------------------------------ */
void ese_gui_begin(EseGui *gui, uint64_t z_index, int x, int y, int width, int height);
void ese_gui_end(EseGui *gui);

/* --- Container Management --------------------------------------------------------------------- */
void ese_gui_open_flex(EseGui *gui, EseGuiFlexDirection direction,
    EseGuiFlexJustify justify, EseGuiFlexAlignItems align_items, int spacing,
    int padding_left, int padding_top, int padding_right, int padding_bottom,
    EseColor *background_color);
void ese_gui_close_flex(EseGui *gui);
void ese_gui_open_box(EseGui *gui, int width, int height, EseColor *background_color);
void ese_gui_close_box(EseGui *gui);

/* --- Widget Management ------------------------------------------------------------------------ */
void ese_gui_push_button(EseGui *gui, const char* text, void (*callback)(void));
void ese_gui_push_image(EseGui *gui, EseGuiImageFit fit, const char *sprite_id);

#endif // ESE_GUI_H
