#include <stdbool.h>
#include <string.h>

#include "core/engine.h"
#include "core/memory_manager.h"
#include "graphics/draw_list.h"
#include "graphics/gui/gui.h"
#include "graphics/gui/gui_private.h"
#include "graphics/gui/gui_widget_stack.h"
#include "scripting/lua_engine.h"
#include "types/gui_style.h"
#include "utility/log.h"

// ========================================
// Defines
// ========================================

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

static void _stack_draw(EseGui *gui, EseGuiWidget *widget, EseDrawList *draw_list, size_t depth);
static void _stack_process_mouse_hover(EseGuiWidget *widget, int mouse_x, int mouse_y);
static bool _stack_process_mouse_click(EseGuiWidget *widget, int mouse_x, int mouse_y,
                                       EseInputMouseButton button);
static void _stack_layout(EseGuiWidget *widget);
static EseGuiWidget *_stack_create(EseGuiWidget *parent, EseGuiStyle *style);
static void _stack_destroy(EseGuiWidget *widget);
static int _stack_lua_init(EseLuaEngine *engine);
static int _stack_lua_open(lua_State *L);
static int _stack_lua_close(lua_State *L);

// ========================================
// PRIVATE STATE
// ========================================

static GuiWidgetVTable g_stack_vtable = {
    .id = "STACK",
    .is_container = true,
    .draw = _stack_draw,
    .process_mouse_hover = _stack_process_mouse_hover,
    .process_mouse_click = _stack_process_mouse_click,
    .layout = _stack_layout,
    .create = _stack_create,
    .destroy = _stack_destroy,
    .lua_init = _stack_lua_init,
};

// ========================================
// PUBLIC FUNCTIONS
// ========================================

GuiWidgetVTable *ese_widget_stack_get_vtable(void) { return &g_stack_vtable; }

// (no private helpers needed for Stack at this time)

// ========================================
// VTABLE CALLBACKS
// ========================================

static void _stack_draw(EseGui *gui, EseGuiWidget *widget, EseDrawList *draw_list, size_t depth) {
    log_assert("GUI", gui, "_stack_draw called with NULL gui");
    log_assert("GUI", widget, "_stack_draw called with NULL widget");
    log_assert("GUI", draw_list, "_stack_draw called with NULL draw_list");

    EseGuiStyleVariant variant = widget->variant;
    if (variant == GUI_STYLE_VARIANT_DEFAULT) {
        variant = GUI_STYLE_VARIANT_TRANSPARENT;
    }

    if (variant != GUI_STYLE_VARIANT_TRANSPARENT) {
        // Background fill
        EseDrawListObject *bg = draw_list_request_object(draw_list);
        if (bg) {
            EseColor *bg_color = ese_gui_style_get_bg(widget->style, variant);
            draw_list_object_set_rect_color(bg, ese_color_get_r(bg_color),
                                            ese_color_get_g(bg_color), ese_color_get_b(bg_color),
                                            ese_color_get_a(bg_color), true);
            draw_list_object_set_bounds(bg, widget->x, widget->y, widget->width, widget->height);
        }

        // Border
        EseDrawListObject *bd = draw_list_request_object(draw_list);
        if (bd) {
            EseColor *bd_color = ese_gui_style_get_border(widget->style, variant);
            draw_list_object_set_rect_color(bd, ese_color_get_r(bd_color),
                                            ese_color_get_g(bd_color), ese_color_get_b(bd_color),
                                            ese_color_get_a(bd_color), false);
            draw_list_object_set_bounds(bd, widget->x, widget->y, widget->width, widget->height);
        }
    }

    for (size_t i = 0; i < widget->children_count; i++) {
        widget->children[i]->type.draw(gui, widget->children[i], draw_list, depth + 1);
    }
}

static void _stack_process_mouse_hover(EseGuiWidget *widget, int mouse_x, int mouse_y) {
    log_assert("GUI", widget, "_stack_process_mouse_hover called with NULL widget");

    const bool inside = (mouse_x >= widget->x && mouse_x < widget->x + widget->width &&
                         mouse_y >= widget->y && mouse_y < widget->y + widget->height);
    widget->is_hovered = inside;

    for (size_t i = 0; i < widget->children_count; i++) {
        widget->children[i]->type.process_mouse_hover(widget->children[i], mouse_x, mouse_y);
    }
}

static bool _stack_process_mouse_click(EseGuiWidget *widget, int mouse_x, int mouse_y,
                                       EseInputMouseButton button) {
    log_assert("GUI", widget, "_stack_process_mouse_click called with NULL widget");
    (void)button;

    widget->is_hovered = (mouse_x >= widget->x && mouse_x < widget->x + widget->width &&
                          mouse_y >= widget->y && mouse_y < widget->y + widget->height);
    if (!widget->is_hovered) {
        return false;
    }

    for (size_t i = 0; i < widget->children_count; i++) {
        if (widget->children[i]->type.process_mouse_click(widget->children[i], mouse_x, mouse_y,
                                                          button)) {
            return true;
        }
    }

    return false;
}

static void _stack_layout(EseGuiWidget *widget) {
    log_assert("GUI", widget, "_stack_layout called with NULL widget");
    const int child_x = widget->x + ese_gui_style_get_padding_left(widget->style);
    const int child_y = widget->y + ese_gui_style_get_padding_top(widget->style);
    const int child_w = widget->width - (ese_gui_style_get_padding_left(widget->style) +
                                         ese_gui_style_get_padding_right(widget->style));
    const int child_h = widget->height - (ese_gui_style_get_padding_top(widget->style) +
                                          ese_gui_style_get_padding_bottom(widget->style));

    for (size_t i = 0; i < widget->children_count; i++) {
        EseGuiWidget *child = widget->children[i];
        child->x = child_x;
        child->y = child_y;
        child->width = child_w;
        child->height = child_h;
        if (child->children_count > 0 && child->type.layout) {
            child->type.layout(child);
        }
    }
}

static EseGuiWidget *_stack_create(EseGuiWidget *parent, EseGuiStyle *style) {
    EseGuiWidget *widget =
        (EseGuiWidget *)memory_manager.calloc(1, sizeof(EseGuiWidget), MMTAG_GUI);
    widget->parent = parent;
    memcpy(&widget->type, &g_stack_vtable, sizeof(GuiWidgetVTable));
    widget->data = NULL;
    widget->style = style;

    // Append to parent's children list if parent provided
    if (parent != NULL) {
        if (parent->children_count >= parent->children_capacity) {
            size_t new_cap = parent->children_capacity * 2 + 1;
            parent->children = (EseGuiWidget **)memory_manager.realloc(
                parent->children, new_cap * sizeof(EseGuiWidget *), MMTAG_GUI);
            parent->children_capacity = new_cap;
        }
        parent->children[parent->children_count++] = widget;
    }
    return widget;
}

static void _stack_destroy(EseGuiWidget *widget) {
    if (!widget) {
        return;
    }
    if (widget->style) {
        ese_gui_style_destroy(widget->style);
    }
    if (widget->children_count > 0) {
        for (size_t i = 0; i < widget->children_count; i++) {
            widget->children[i]->type.destroy(widget->children[i]);
        }
    }
    memory_manager.free(widget->children);
    memory_manager.free(widget);
}

static int _stack_lua_init(EseLuaEngine *engine) {
    log_assert("GUI", engine, "_stack_lua_init called with NULL engine");
    log_assert("GUI", engine->runtime, "_stack_lua_init called with NULL lua runtime");

    // Attach to existing GUI table
    lua_getglobal(engine->runtime, "GUI");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);
        log_error("GUI", "GUI table not found during stack widget lua_init");
        return 0;
    }

    // GUI.open_stack
    lua_pushcfunction(engine->runtime, _stack_lua_open);
    lua_setfield(engine->runtime, -2, "open_stack");

    // GUI.close_stack
    lua_pushcfunction(engine->runtime, _stack_lua_close);
    lua_setfield(engine->runtime, -2, "close_stack");

    // pop GUI table
    lua_pop(engine->runtime, 1);

    return 0;
}

// ========================================
// LUA FUNCTIONS
// ========================================

static int _stack_lua_open(lua_State *L) {
    log_assert("GUI_LUA", L, "_stack_lua_open called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args < 0 || n_args > 3) {
        return luaL_error(L, "GUI.open_stack([width[, height[, style]]]) takes "
                             "up to 3 arguments");
    }

    if (n_args >= 1 && lua_type(L, 1) != LUA_TNUMBER) {
        return luaL_error(L, "width must be a number or GUI.AUTO_SIZE");
    }

    if (n_args >= 2 && lua_type(L, 2) != LUA_TNUMBER) {
        return luaL_error(L, "height must be a number or GUI.AUTO_SIZE");
    }

    int width = GUI_AUTO_SIZE;
    int height = GUI_AUTO_SIZE;
    if (n_args >= 1) {
        width = (int)lua_tonumber(L, 1);
    }
    if (n_args >= 2) {
        height = (int)lua_tonumber(L, 2);
    }

    EseGuiStyle *opt_style = NULL;
    if (n_args == 3) {
        opt_style = ese_gui_style_lua_get(L, 3);
        if (!opt_style) {
            return luaL_error(L, "style must be a GuiStyle");
        }
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout == NULL) {
        return luaL_error(L, "GUI.open_stack() called with no open GUI active");
    }

    if (gui->open_layout->root == NULL && gui->open_layout->current_widget == NULL) {
        // first widget in layout
        gui->open_layout->current_widget =
            _stack_create(NULL, ese_gui_style_copy(opt_style ? opt_style : gui->default_style));
        gui->open_layout->root = gui->open_layout->current_widget;
    } else if (gui->open_layout->current_widget == NULL ||
               gui->open_layout->current_widget->type.is_container == false) {
        return luaL_error(L, "GUI.open_stack() called with no open container active");
    } else {
        EseGuiWidget *new_stack =
            _stack_create(gui->open_layout->current_widget,
                          ese_gui_style_copy(opt_style ? opt_style : gui->default_style));
        gui->open_layout->current_widget = new_stack;
    }

    gui->open_layout->current_widget->width = width;
    gui->open_layout->current_widget->height = height;
    gui->open_layout->current_widget->variant = ese_gui_get_top_variant(gui);

    return 0;
}

static int _stack_lua_close(lua_State *L) {
    log_assert("GUI_LUA", L, "_stack_lua_close called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 0) {
        return luaL_error(L, "GUI.close_stack() takes no arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout == NULL) {
        return luaL_error(L, "GUI.close_stack() called with no open GUI active");
    }

    EseGuiWidget *current = gui->open_layout->current_widget;
    if (current == NULL || strncmp(current->type.id, "STACK", sizeof(current->type.id)) != 0) {
        return luaL_error(L, "GUI.close_stack() called but current container is not a Stack");
    }

    // Move current to parent (or clear if none)
    gui->open_layout->current_widget = current->parent;
    return 0;
}
