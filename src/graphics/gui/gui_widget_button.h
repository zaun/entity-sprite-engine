#ifndef ESE_GUI_WIDGET_BUTTON_H
#define ESE_GUI_WIDGET_BUTTON_H

#include "graphics/gui/gui_widget.h"

typedef struct GuiWidgetVTable GuiWidgetVTable;

GuiWidgetVTable *ese_widget_button_get_vtable(void);

// Helper to configure a button created via vtable
void ese_widget_button_set(struct EseGuiWidget *button, const char *text, void (*callback)(void *),
                           void *userdata);

#endif // ESE_GUI_WIDGET_BUTTON_H
