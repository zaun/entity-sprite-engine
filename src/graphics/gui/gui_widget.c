#include "graphics/gui/gui_widget.h"
#include "graphics/gui/gui_widget_button.h"
#include "graphics/gui/gui_widget_flex.h"
#include "graphics/gui/gui_widget_image.h"
#include "graphics/gui/gui_widget_stack.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include <stdbool.h>
#include <string.h>

void _ese_widget_init(EseLuaEngine *engine, GuiWidgetVTable *type);

void _ese_widget_register(EseLuaEngine *engine) {
  log_assert("GUI", engine, "_ese_widget_register called with NULL engine");

  // Register widgets
  _ese_widget_init(engine, ese_widget_flex_get_vtable());
  _ese_widget_init(engine, ese_widget_stack_get_vtable());
  _ese_widget_init(engine, ese_widget_button_get_vtable());
  _ese_widget_init(engine, ese_widget_image_get_vtable());
}

void _ese_widget_init(EseLuaEngine *engine, GuiWidgetVTable *type) {
  log_assert("GUI", engine, "_ese_widget_init called with NULL engine");
  log_assert("GUI", type, "_ese_widget_init called with NULL type");

  type->lua_init(engine);

  log_debug("GUI", "Registered widget type: %s", type->id);
}

void _ese_widget_draw(EseGui *gui, EseGuiWidget *widget, EseDrawList *draw_list,
                      size_t depth) {
  log_assert("GUI", gui, "ese_widget_draw called with NULL gui");
  log_assert("GUI", widget, "ese_widget_draw called with NULL widget");
  log_assert("GUI", draw_list, "ese_widget_draw called with NULL draw_list");

  widget->type.draw(gui, widget, draw_list, depth);
}

void _ese_widget_process_mouse_hover(EseGuiWidget *widget, int mouse_x,
                                     int mouse_y) {
  log_assert("GUI", widget,
             "_ese_widget_process_mouse_hover called with NULL widget");

  widget->type.process_mouse_hover(widget, mouse_x, mouse_y);

  for (size_t i = 0; i < widget->children_count; i++) {
    _ese_widget_process_mouse_hover(widget->children[i], mouse_x, mouse_y);
  }
}

static bool
_ese_widget_process_mouse_clicked_worker(EseGuiWidget *widget, int mouse_x,
                                         int mouse_y,
                                         EseInputMouseButton button) {
  log_assert(
      "GUI", widget,
      "_ese_widget_process_mouse_clicked_worker called with NULL widget");

  // Process children first so the deepest children can handle the click first
  for (size_t i = 0; i < widget->children_count; i++) {
    if (_ese_widget_process_mouse_clicked_worker(widget->children[i], mouse_x,
                                                 mouse_y, button)) {
      return true;
    }
  }

  return widget->type.process_mouse_click(widget, mouse_x, mouse_y, button);
}
void _ese_widget_process_mouse_clicked(EseGuiWidget *widget, int mouse_x,
                                       int mouse_y,
                                       EseInputMouseButton button) {
  log_assert("GUI", widget,
             "ese_widget_process_mouse_click called with NULL widget");

  _ese_widget_process_mouse_clicked_worker(widget, mouse_x, mouse_y, button);
}

void _ese_widget_layout(EseGuiWidget *widget, int x, int y, int width,
                        int height) {
  log_assert("GUI", widget, "ese_widget_layout called with NULL widget");

  widget->x = x;
  widget->y = y;
  widget->width = width;
  widget->height = height;

  if (widget->children_count > 0) {
    widget->type.layout(widget);
  }
}
