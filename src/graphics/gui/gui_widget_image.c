#include <stdbool.h>
#include <string.h>

#include "core/engine.h"
#include "core/memory_manager.h"
#include "graphics/gui/gui_widget_image.h"
#include "graphics/gui/gui_widget.h"
#include "graphics/gui/gui_private.h"
#include "graphics/gui/gui.h"
#include "graphics/draw_list.h"
#include "scripting/lua_engine.h"
#include "types/gui_style.h"
#include "utility/log.h"

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

static void _image_draw(EseGui *gui, EseGuiWidget *widget, EseDrawList *draw_list, size_t depth);
static void _image_process_mouse_hover(EseGuiWidget *widget, int mouse_x, int mouse_y);
static bool _image_process_mouse_click(EseGuiWidget *widget, int mouse_x, int mouse_y, EseInputMouseButton button);
static void _image_layout(EseGuiWidget *widget);
static EseGuiWidget *_image_create(EseGuiWidget *parent, EseGuiStyle *style);
static void _image_destroy(EseGuiWidget *widget);
static int _image_lua_init(EseLuaEngine *engine);
static int _image_lua_push(lua_State *L);

// ========================================
// PRIVATE TYPES
// ========================================

typedef struct GuiImageData {
    char *sprite_id;
    EseGuiImageFit fit;
} GuiImageData;

static GuiImageData *_image_data_create(void) {
    GuiImageData *data = (GuiImageData *)memory_manager.calloc(1, sizeof(GuiImageData), MMTAG_GUI);
    data->sprite_id = NULL;
    data->fit = IMAGE_FIT_CONTAIN;
    return data;
}

static void _image_data_destroy(GuiImageData *data) {
    if (!data) {
        return;
    }
    if (data->sprite_id) {
        memory_manager.free(data->sprite_id);
    }
    memory_manager.free(data);
}

// ========================================
// PRIVATE STATE
// ========================================

static GuiWidgetVTable g_image_vtable = {
    .id = "IMAGE",
    .is_container = false,
    .draw = _image_draw,
    .process_mouse_hover = _image_process_mouse_hover,
    .process_mouse_click = _image_process_mouse_click,
    .layout = _image_layout,
    .create = _image_create,
    .destroy = _image_destroy,
    .lua_init = _image_lua_init,
};

// ========================================
// PUBLIC FUNCTIONS
// ========================================

GuiWidgetVTable *ese_widget_image_get_vtable(void) {
    return &g_image_vtable;
}

// ========================================
// VTABLE CALLBACKS
// ========================================

static void _image_draw(EseGui *gui, EseGuiWidget *widget, EseDrawList *draw_list, size_t depth) {
    (void)depth;
    log_assert("GUI", gui, "_image_draw called with NULL gui");
    log_assert("GUI", widget, "_image_draw called with NULL widget");
    log_assert("GUI", draw_list, "_image_draw called with NULL draw_list");

	EseGuiStyleVariant variant = widget->variant;
	if (variant == GUI_STYLE_VARIANT_DEFAULT) {
		variant = GUI_STYLE_VARIANT_TRANSPARENT;
	}

	if (variant != GUI_STYLE_VARIANT_TRANSPARENT) {
		// Background fill
		EseDrawListObject *bg = draw_list_request_object(draw_list);
		if (bg) {
			EseColor *bg_color = ese_gui_style_get_bg(widget->style, variant);
			draw_list_object_set_rect_color(bg, ese_color_get_r(bg_color), ese_color_get_g(bg_color), ese_color_get_b(bg_color), ese_color_get_a(bg_color), true);
			draw_list_object_set_bounds(bg, widget->x, widget->y, widget->width, widget->height);
		}

		// Border
		EseDrawListObject *bd = draw_list_request_object(draw_list);
		if (bd) {
			EseColor *bd_color = ese_gui_style_get_border(widget->style, variant);
			draw_list_object_set_rect_color(bd, ese_color_get_r(bd_color), ese_color_get_g(bd_color), ese_color_get_b(bd_color), ese_color_get_a(bd_color), false);
			draw_list_object_set_bounds(bd, widget->x, widget->y, widget->width, widget->height);
		}
	}

    GuiImageData *data = (GuiImageData *)widget->data;
    log_assert("GUI", data, "_image_draw called with NULL data");
    if (data->sprite_id == NULL) {
        return;
    }

    EseDrawListObject *img_obj = draw_list_request_object(draw_list);
    // For now, map all fits to full UVs as in old implementation
    draw_list_object_set_texture(img_obj, data->sprite_id, 0.0f, 0.0f, 1.0f, 1.0f);
    draw_list_object_set_bounds(img_obj, widget->x, widget->y, widget->width, widget->height);
    // Z is handled by traversal ordering upstream if needed; not set here
}

static void _image_process_mouse_hover(EseGuiWidget *widget, int mouse_x, int mouse_y) {
    log_assert("GUI", widget, "_image_process_mouse_hover called with NULL widget");
    const bool inside = (mouse_x >= widget->x && mouse_x < widget->x + widget->width && mouse_y >= widget->y && mouse_y < widget->y + widget->height);
    widget->is_hovered = inside;
}

static bool _image_process_mouse_click(EseGuiWidget *widget, int mouse_x, int mouse_y, EseInputMouseButton button) {
    (void)button;
    log_assert("GUI", widget, "_image_process_mouse_click called with NULL widget");
    const bool inside = (mouse_x >= widget->x && mouse_x < widget->x + widget->width && mouse_y >= widget->y && mouse_y < widget->y + widget->height);
    return inside;
}

static void _image_layout(EseGuiWidget *widget) {
    (void)widget;
}

static EseGuiWidget *_image_create(EseGuiWidget *parent, EseGuiStyle *style) {
    EseGuiWidget *widget = (EseGuiWidget *)memory_manager.calloc(1, sizeof(EseGuiWidget), MMTAG_GUI);
    widget->parent = parent;
    memcpy(&widget->type, &g_image_vtable, sizeof(GuiWidgetVTable));
    widget->data = _image_data_create();
    widget->style = style;

    if (parent != NULL) {
        if (parent->children_count >= parent->children_capacity) {
            size_t new_cap = parent->children_capacity * 2 + 1;
            parent->children = (EseGuiWidget **)memory_manager.realloc(
                parent->children,
                new_cap * sizeof(EseGuiWidget *),
                MMTAG_GUI
            );
            parent->children_capacity = new_cap;
        }
        parent->children[parent->children_count++] = widget;
    }
    return widget;
}

static void _image_destroy(EseGuiWidget *widget) {
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
    _image_data_destroy((GuiImageData *)widget->data);
    memory_manager.free(widget);
}

void ese_widget_image_set(EseGuiWidget *image, const char *sprite_id, EseGuiImageFit fit) {
    log_assert("GUI", image, "ese_widget_image_set called with NULL image");
    GuiImageData *data = (GuiImageData *)image->data;
    if (data->sprite_id) {
        memory_manager.free(data->sprite_id);
    }
    data->sprite_id = memory_manager.strdup(sprite_id, MMTAG_GUI);
    data->fit = fit;
}

// ========================================
// LUA BINDINGS
// ========================================

static int _image_lua_init(EseLuaEngine *engine) {
    log_assert("GUI", engine, "_image_lua_init called with NULL engine");
    log_assert("GUI", engine->runtime, "_image_lua_init called with NULL lua runtime");

    // Attach to existing GUI table
    lua_getglobal(engine->runtime, "GUI");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);
        log_error("GUI", "GUI table not found during image widget lua_init");
        return 0;
    }

    // GUI.push_image(sprite_id[, fit[, style]])
    lua_pushcfunction(engine->runtime, _image_lua_push);
    lua_setfield(engine->runtime, -2, "push_image");

    // GUI.IMAGE_FIT = { COVER=..., CONTAIN=..., FILL=..., REPEAT=... }
    lua_newtable(engine->runtime);
    lua_pushinteger(engine->runtime, IMAGE_FIT_COVER);
    lua_setfield(engine->runtime, -2, "COVER");
    lua_pushinteger(engine->runtime, IMAGE_FIT_CONTAIN);
    lua_setfield(engine->runtime, -2, "CONTAIN");
    lua_pushinteger(engine->runtime, IMAGE_FIT_FILL);
    lua_setfield(engine->runtime, -2, "FILL");
    lua_pushinteger(engine->runtime, IMAGE_FIT_REPEAT);
    lua_setfield(engine->runtime, -2, "REPEAT");
    lua_setfield(engine->runtime, -2, "IMAGE_FIT");

    // pop GUI table
    lua_pop(engine->runtime, 1);

    return 0;
}

static int _image_lua_push(lua_State *L) {
    log_assert("GUI_LUA", L, "_image_lua_push called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args < 1 || n_args > 3) {
        return luaL_error(L, "GUI.push_image(sprite_id[, fit[, style]]) takes 1 to 3 arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout == NULL) {
        return luaL_error(L, "GUI.push_image() called with no open GUI active");
    }

    const char *sprite_id = luaL_checkstring(L, 1);
    if (sprite_id == NULL) {
        return luaL_error(L, "GUI.push_image() sprite_id must be a string");
    }

    EseGuiImageFit fit = IMAGE_FIT_CONTAIN;
    if (n_args >= 2) {
        EseLuaValue *value = lua_value_from_stack(L, 2);
        if (value->type == LUA_VAL_NUMBER) {
            fit = (EseGuiImageFit)value->value.number;
        }
    }

    EseGuiStyle *opt_style = NULL;
    if (n_args == 3) {
        opt_style = ese_gui_style_lua_get(L, 3);
        if (!opt_style) {
            return luaL_error(L, "style must be a GuiStyle");
        }
    }

    // Ensure there is a current container
    if (gui->open_layout->root == NULL && gui->open_layout->current_widget == NULL) {
        return luaL_error(L, "GUI.push_image() called with no open container");
    }
    if (gui->open_layout->current_widget == NULL) {
        return luaL_error(L, "GUI.push_image() called with no open container");
    }

    EseGuiWidget *parent = gui->open_layout->current_widget;
    EseGuiWidget *image = _image_create(parent, ese_gui_style_copy(opt_style ? opt_style : gui->default_style));
    image->width = GUI_AUTO_SIZE;
    image->height = GUI_AUTO_SIZE;
    image->variant = ese_gui_get_top_variant(gui);

    GuiImageData *data = (GuiImageData *)image->data;
    data->sprite_id = memory_manager.strdup(sprite_id, MMTAG_GUI);
    data->fit = fit;

    return 0;
}


