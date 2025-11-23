#include <stdbool.h>
#include <string.h>

#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "graphics/draw_list.h"
#include "graphics/font.h"
#include "graphics/gui/gui.h"
#include "graphics/gui/gui_private.h"
#include "graphics/gui/gui_widget.h"
#include "graphics/gui/gui_widget_label.h"
#include "scripting/lua_engine.h"
#include "types/color.h"
#include "types/gui_style.h"
#include "utility/log.h"

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

static void _label_draw(EseGui *gui, EseGuiWidget *widget, EseDrawList *draw_list, size_t depth);
static void _label_process_mouse_hover(EseGuiWidget *widget, int mouse_x, int mouse_y);
static bool _label_process_mouse_click(EseGuiWidget *widget, int mouse_x, int mouse_y,
                                       EseInputMouseButton button);
static void _label_layout(EseGuiWidget *widget);
static EseGuiWidget *_label_create(EseGuiWidget *parent, EseGuiStyle *style);
static void _label_destroy(EseGuiWidget *widget);
static int _label_lua_init(EseLuaEngine *engine);
static int _label_lua_push(lua_State *L);

// Font texture callback for drawing text to draw list
// NOTE: We intentionally use DL_TEXTURE objects here (same as BUTTON)
// so we don't introduce new draw list types that the renderer isn't
// expecting in existing batching logic.
static void _label_font_texture_callback(float screen_x, float screen_y, float screen_w,
                                         float screen_h, uint64_t z_index, const char *texture_id,
                                         float texture_x1, float texture_y1, float texture_x2,
                                         float texture_y2, int width, int height, void *user_data);

// ========================================
// PRIVATE TYPES
// ========================================

typedef struct GuiLabelData {
    char *text;
    EseGuiFlexJustify justify;
    EseGuiFlexAlignItems align_items;
} GuiLabelData;

static GuiLabelData *_label_data_create(void) {
    GuiLabelData *data =
        (GuiLabelData *)memory_manager.calloc(1, sizeof(GuiLabelData), MMTAG_GUI);
    data->text = NULL;
    data->justify = FLEX_JUSTIFY_START;
    data->align_items = FLEX_ALIGN_ITEMS_START;
    return data;
}

static void _label_data_destroy(GuiLabelData *data) {
    if (!data) {
        return;
    }
    if (data->text) {
        memory_manager.free(data->text);
    }
    memory_manager.free(data);
}

// ========================================
// PRIVATE STATE
// ========================================

static GuiWidgetVTable g_label_vtable = {
    .id = "LABEL",
    .is_container = false,
    .draw = _label_draw,
    .process_mouse_hover = _label_process_mouse_hover,
    .process_mouse_click = _label_process_mouse_click,
    .layout = _label_layout,
    .create = _label_create,
    .destroy = _label_destroy,
    .lua_init = _label_lua_init,
};

// ========================================
// PUBLIC FUNCTIONS
// ========================================

GuiWidgetVTable *ese_widget_label_get_vtable(void) { return &g_label_vtable; }

void ese_widget_label_set(EseGuiWidget *label, const char *text, EseGuiFlexJustify justify,
                          EseGuiFlexAlignItems align_items) {
    log_assert("GUI", label, "ese_widget_label_set called with NULL label");
    GuiLabelData *data = (GuiLabelData *)label->data;
    if (!data) {
        return;
    }
    if (data->text) {
        memory_manager.free(data->text);
    }
    data->text = text ? memory_manager.strdup(text, MMTAG_GUI) : NULL;
    data->justify = justify;
    data->align_items = align_items;
}

// ========================================
// VTABLE CALLBACKS
// ========================================

static void _label_draw(EseGui *gui, EseGuiWidget *widget, EseDrawList *draw_list, size_t depth) {
    log_assert("GUI", gui, "_label_draw called with NULL gui");
    log_assert("GUI", widget, "_label_draw called with NULL widget");
    log_assert("GUI", draw_list, "_label_draw called with NULL draw_list");

    GuiLabelData *data = (GuiLabelData *)widget->data;
    log_assert("GUI", data, "_label_draw called with NULL data");

    size_t z_index = depth * 10;

    // Resolve style variants for background and text
    EseGuiStyleVariant bg_variant = widget->variant;
    if (bg_variant == GUI_STYLE_VARIANT_DEFAULT) {
        bg_variant = GUI_STYLE_VARIANT_TRANSPARENT;
    }
    EseGuiStyleVariant text_variant = widget->variant;
    if (text_variant == GUI_STYLE_VARIANT_DEFAULT) {
        text_variant = GUI_STYLE_VARIANT_PRIMARY;
    }

    // --------- DRAW BACKGROUND (using style.bg) ----------
    if (bg_variant != GUI_STYLE_VARIANT_TRANSPARENT) {
        EseColor *bg = ese_gui_style_get_bg(widget->style, bg_variant);
        if (bg) {
            EseDrawListObject *bg_obj = draw_list_request_object(draw_list);
            unsigned char br = (unsigned char)(ese_color_get_r(bg) * 255.0f);
            unsigned char bg_g = (unsigned char)(ese_color_get_g(bg) * 255.0f);
            unsigned char bb = (unsigned char)(ese_color_get_b(bg) * 255.0f);
            unsigned char ba = (unsigned char)(ese_color_get_a(bg) * 255.0f);
            draw_list_object_set_rect_color(bg_obj, br, bg_g, bb, ba, true);
            draw_list_object_set_bounds(bg_obj, widget->x, widget->y, widget->width,
                                        widget->height);
            draw_list_object_set_z_index(bg_obj, z_index);
        }
    }

    // --------- DRAW LABEL TEXT (using style.text) ----------
    if (data->text != NULL && widget->width > 0 && widget->height > 0) {
        int font_size = ese_gui_style_get_font_size(widget->style);
        if (font_size == GUI_STYLE_FONT_SIZE_WIDGET_DEFAULT || font_size <= 0) {
            font_size = 20; // match console font default height
        }

        // Approximate text metrics
        float text_width = (float)strlen(data->text) * (font_size * 0.6f);
        float text_height = (float)font_size;

        int padding_left = ese_gui_style_get_padding_left(widget->style);
        int padding_right = ese_gui_style_get_padding_right(widget->style);
        int padding_top = ese_gui_style_get_padding_top(widget->style);
        int padding_bottom = ese_gui_style_get_padding_bottom(widget->style);

        float inner_w = (float)widget->width - (float)(padding_left + padding_right);
        float inner_h = (float)widget->height - (float)(padding_top + padding_bottom);
        if (inner_w < 0.0f)
            inner_w = 0.0f;
        if (inner_h < 0.0f)
            inner_h = 0.0f;

        float text_x = (float)widget->x + (float)padding_left;
        if (data->justify == FLEX_JUSTIFY_CENTER) {
            text_x = (float)widget->x + (float)padding_left + (inner_w - text_width) / 2.0f;
        } else if (data->justify == FLEX_JUSTIFY_END) {
            text_x = (float)widget->x + (float)padding_left + (inner_w - text_width);
        }

        float text_y = (float)widget->y + (float)padding_top;
        if (data->align_items == FLEX_ALIGN_ITEMS_CENTER) {
            text_y = (float)widget->y + (float)padding_top + (inner_h - text_height) / 2.0f;
        } else if (data->align_items == FLEX_ALIGN_ITEMS_END) {
            text_y = (float)widget->y + (float)padding_top + (inner_h - text_height);
        }

        EseGuiStyleVariant variant_for_text = text_variant;
        EseColor *text_color = ese_gui_style_get_text(widget->style, variant_for_text);
        (void)text_color; // Currently unused; text is rendered with the font's native color

        EseEngine *engine =
            (EseEngine *)lua_engine_get_registry_key(gui->engine->runtime, ENGINE_KEY);

        // Render text via DL_TEXTURE objects, same as BUTTON, so renderer
        // batching continues to work as before. Text color currently comes
        // from the font texture (typically white), not from style->text.
        font_draw_text_scaled(engine, "console_font_10x20", data->text, text_x, text_y,
                              z_index + 1, (float)font_size, _label_font_texture_callback,
                              draw_list);
    }
}

static void _label_process_mouse_hover(EseGuiWidget *widget, int mouse_x, int mouse_y) {
    log_assert("GUI", widget, "_label_process_mouse_hover called with NULL widget");
    const bool inside = (mouse_x >= widget->x && mouse_x < widget->x + widget->width &&
                         mouse_y >= widget->y && mouse_y < widget->y + widget->height);
    widget->is_hovered = inside;
}

static bool _label_process_mouse_click(EseGuiWidget *widget, int mouse_x, int mouse_y,
                                       EseInputMouseButton button) {
    (void)button;
    log_assert("GUI", widget, "_label_process_mouse_click called with NULL widget");
    const bool inside = (mouse_x >= widget->x && mouse_x < widget->x + widget->width &&
                         mouse_y >= widget->y && mouse_y < widget->y + widget->height);
    widget->is_hovered = inside;
    // Labels are non-interactive; we still report whether the click was inside
    return inside;
}

static void _label_layout(EseGuiWidget *widget) { (void)widget; }

static EseGuiWidget *_label_create(EseGuiWidget *parent, EseGuiStyle *style) {
    EseGuiWidget *widget =
        (EseGuiWidget *)memory_manager.calloc(1, sizeof(EseGuiWidget), MMTAG_GUI);
    widget->parent = parent;
    memcpy(&widget->type, &g_label_vtable, sizeof(GuiWidgetVTable));
    widget->data = _label_data_create();
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

static void _label_destroy(EseGuiWidget *widget) {
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
    _label_data_destroy((GuiLabelData *)widget->data);
    memory_manager.free(widget);
}

// ========================================
// LUA BINDINGS
// ========================================

static int _label_lua_init(EseLuaEngine *engine) {
    log_assert("GUI", engine, "_label_lua_init called with NULL engine");
    log_assert("GUI", engine->runtime, "_label_lua_init called with NULL lua runtime");

    // Attach to existing GUI table
    lua_getglobal(engine->runtime, "GUI");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);
        log_error("GUI", "GUI table not found during label widget lua_init");
        return 0;
    }

    // GUI.push_label(text[, justify[, align_items[, style]]])
    lua_pushcfunction(engine->runtime, _label_lua_push);
    lua_setfield(engine->runtime, -2, "push_label");

    // pop GUI table
    lua_pop(engine->runtime, 1);

    return 0;
}

static int _label_lua_push(lua_State *L) {
    log_assert("GUI_LUA", L, "_label_lua_push called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args < 1 || n_args > 4) {
        return luaL_error(L, "GUI.push_label(text[, justify[, align_items[, style]]]) "
                             "takes 1 to 4 arguments");
    }

    const char *text = luaL_checkstring(L, 1);

    EseGuiFlexJustify justify = FLEX_JUSTIFY_START;
    if (n_args >= 2 && !lua_isnoneornil(L, 2)) {
        if (!lua_isnumber(L, 2)) {
            return luaL_error(L, "justify must be a number");
        }
        justify = (EseGuiFlexJustify)lua_tonumber(L, 2);
    }

    EseGuiFlexAlignItems align_items = FLEX_ALIGN_ITEMS_START;
    if (n_args >= 3 && !lua_isnoneornil(L, 3)) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "align_items must be a number");
        }
        align_items = (EseGuiFlexAlignItems)lua_tonumber(L, 3);
    }

    EseGuiStyle *opt_style = NULL;
    if (n_args == 4) {
        opt_style = ese_gui_style_lua_get(L, 4);
        if (!opt_style) {
            return luaL_error(L, "style must be a GuiStyle");
        }
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout == NULL) {
        return luaL_error(L, "GUI.push_label() called with no open GUI active");
    }

    // Ensure there is a current container
    if (gui->open_layout->root == NULL && gui->open_layout->current_widget == NULL) {
        return luaL_error(L, "GUI.push_label() called with no open container");
    }
    if (gui->open_layout->current_widget == NULL) {
        return luaL_error(L, "GUI.push_label() called with no open container");
    }

    EseGuiWidget *parent = gui->open_layout->current_widget;
    EseGuiWidget *label =
        _label_create(parent, ese_gui_style_copy(opt_style ? opt_style : gui->default_style));
    label->width = GUI_AUTO_SIZE;
    label->height = GUI_AUTO_SIZE;
    label->variant = ese_gui_get_top_variant(gui);

    GuiLabelData *data = (GuiLabelData *)label->data;
    if (data->text) {
        memory_manager.free(data->text);
    }
    data->text = memory_manager.strdup(text, MMTAG_GUI);
    data->justify = justify;
    data->align_items = align_items;

    return 0;
}

static void _label_font_texture_callback(float screen_x, float screen_y, float screen_w,
                                         float screen_h, uint64_t z_index, const char *texture_id,
                                         float texture_x1, float texture_y1, float texture_x2,
                                         float texture_y2, int width, int height, void *user_data) {
    (void)width;
    (void)height;

    EseDrawList *draw_list = (EseDrawList *)user_data;
    log_assert("GUI", draw_list, "_label_font_texture_callback called with NULL draw_list");

    EseDrawListObject *obj = draw_list_request_object(draw_list);
    draw_list_object_set_texture(obj, texture_id, texture_x1, texture_y1, texture_x2, texture_y2);
    draw_list_object_set_bounds(obj, screen_x, screen_y, (int)screen_w, (int)screen_h);
    draw_list_object_set_z_index(obj, z_index);
}
