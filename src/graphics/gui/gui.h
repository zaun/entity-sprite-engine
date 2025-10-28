#ifndef ESE_GUI_H
#define ESE_GUI_H

#include "graphics/gui/gui_widget_image.h" // for EseGuiImageFit

typedef struct EseGui EseGui;
typedef struct EseColor EseColor;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseInputState EseInputState;
typedef struct EseDrawList EseDrawList;
typedef struct EseGuiStyle EseGuiStyle;
typedef enum EseGuiStyleVariant EseGuiStyleVariant;

/* --- Context management
 * ---------------------------------------------------------------------- */
EseGui *ese_gui_create(EseLuaEngine *engine);
void ese_gui_destroy(EseGui *gui);

/* --- UI Processing
 * ---------------------------------------------------------------------------
 */
// Injects raw input events into the GUI system for processing within a specific
// context.
void ese_gui_input(EseGui *gui, EseInputState *input_state);
// Processes all layout calculations, handles input, executes callbacks for
// interactive elements, and queues drawing commands for the current session.
void ese_gui_process(EseGui *gui, EseDrawList *draw_list);
// Cleans up the GUI system after processing a frame.
void ese_gui_cleanup(EseGui *gui);

/* --- Style Management
 * ------------------------------------------------------------------------- */
EseGuiStyle *ese_gui_get_default_style(EseGui *gui);
void ese_gui_set_default_style(EseGui *gui, EseGuiStyle *style);
void ese_gui_reset_default_style(EseGui *gui);

/* --- Variant Management
 * ----------------------------------------------------------------------- */
EseGuiStyleVariant ese_gui_get_top_variant(EseGui *gui);
void ese_gui_push_variant(EseGui *gui, EseGuiStyleVariant variant);
void ese_gui_pop_variant(EseGui *gui);

#endif // ESE_GUI_H
