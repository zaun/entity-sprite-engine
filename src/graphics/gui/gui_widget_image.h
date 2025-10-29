#ifndef ESE_GUI_WIDGET_IMAGE_H
#define ESE_GUI_WIDGET_IMAGE_H

#include "graphics/gui/gui_widget.h"

typedef struct GuiWidgetVTable GuiWidgetVTable;

typedef enum EseGuiImageFit {
    IMAGE_FIT_COVER,
    IMAGE_FIT_CONTAIN,
    IMAGE_FIT_FILL,
    IMAGE_FIT_REPEAT,
    ESE_GUI_IMAGE_FIT_MAX,
} EseGuiImageFit;

GuiWidgetVTable *ese_widget_image_get_vtable(void);

// Helper to configure image widget
void ese_widget_image_set(struct EseGuiWidget *image, const char *sprite_id, EseGuiImageFit fit);

#endif // ESE_GUI_WIDGET_IMAGE_H
