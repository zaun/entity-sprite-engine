#ifndef ESE_GUI_WIDGET_FLEX_H
#define ESE_GUI_WIDGET_FLEX_H

#include "graphics/gui/gui_widget.h"

typedef struct GuiWidgetVTable GuiWidgetVTable;

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

GuiWidgetVTable *ese_widget_flex_get_vtable(void);

#endif // ESE_GUI_WIDGET_FLEX_H
