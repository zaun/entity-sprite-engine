#ifndef ESE_GUI_WIDGET_LABEL_H
#define ESE_GUI_WIDGET_LABEL_H

#include "graphics/gui/gui_widget.h"
#include "graphics/gui/gui_widget_flex.h" // for EseGuiFlexJustify / EseGuiFlexAlignItems

typedef struct GuiWidgetVTable GuiWidgetVTable;
struct EseGuiWidget;

GuiWidgetVTable *ese_widget_label_get_vtable(void);

// Helper to configure a label created via vtable
void ese_widget_label_set(struct EseGuiWidget *label, const char *text,
                          EseGuiFlexJustify justify, EseGuiFlexAlignItems align_items);

#endif // ESE_GUI_WIDGET_LABEL_H
