#ifndef ESE_GUI_WIDGET_STACK_H
#define ESE_GUI_WIDGET_STACK_H

#include "graphics/gui/gui_widget.h"

typedef struct GuiWidgetVTable GuiWidgetVTable;

GuiWidgetVTable *ese_widget_stack_get_vtable(void);

#endif // ESE_GUI_WIDGET_STACK_H
