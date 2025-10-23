#include <stdbool.h>
#include <string.h>

#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "graphics/gui/gui_widget_button.h"
#include "graphics/gui/gui_widget.h"
#include "graphics/gui/gui_private.h"
#include "graphics/gui/gui.h"
#include "graphics/draw_list.h"
#include "graphics/font.h"
#include "scripting/lua_engine.h"
#include "types/gui_style.h"
#include "utility/log.h"

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

static void _button_draw(EseGui *gui, EseGuiWidget *widget, EseDrawList *draw_list, size_t depth);
static void _button_process_mouse_hover(EseGuiWidget *widget, int mouse_x, int mouse_y);
static bool _button_process_mouse_click(EseGuiWidget *widget, int mouse_x, int mouse_y, EseInputMouseButton button);
static void _button_layout(EseGuiWidget *widget);
static EseGuiWidget *_button_create(EseGuiWidget *parent, EseGuiStyle *style);
static void _button_destroy(EseGuiWidget *widget);
static int _button_lua_init(EseLuaEngine *engine);
static int _button_lua_push(lua_State *L);

// Callback bridge for Lua
typedef struct LuaButtonCallbackData { lua_State *L; int ref; void *userdata; } LuaButtonCallbackData;
static void _button_invoke_lua_callback(void *ud);

// Font texture callback for drawing text to draw list
static void _button_font_texture_callback(float screen_x, float screen_y, float screen_w, float screen_h, uint64_t z_index,
                                         const char *texture_id, float texture_x1, float texture_y1, float texture_x2, float texture_y2,
                                         int width, int height, void *user_data);

// ========================================
// PRIVATE TYPES
// ========================================

typedef struct GuiButtonData {
	char *text;
	void (*callback)(void *userdata);
	void *userdata;
} GuiButtonData;

static GuiButtonData *_button_data_create(void) {
GuiButtonData *data = (GuiButtonData *)memory_manager.calloc(1, sizeof(GuiButtonData), MMTAG_GUI);
	data->text = NULL;
	data->callback = NULL;
	data->userdata = NULL;
	return data;
}

static void _button_data_destroy(GuiButtonData *data) {
	if (!data) {
		return;
	}
	if (data->text) {
		memory_manager.free(data->text);
	}
	if (data->userdata) {
		memory_manager.free(data->userdata);
	}
	memory_manager.free(data);
}

// ========================================
// PRIVATE STATE
// ========================================

static GuiWidgetVTable g_button_vtable = {
	.id = "BUTTON",
	.is_container = false,
	.draw = _button_draw,
	.process_mouse_hover = _button_process_mouse_hover,
	.process_mouse_click = _button_process_mouse_click,
	.layout = _button_layout,
	.create = _button_create,
	.destroy = _button_destroy,
	.lua_init = _button_lua_init,
};

// ========================================
// PUBLIC FUNCTIONS
// ========================================

GuiWidgetVTable *ese_widget_button_get_vtable(void) {
	return &g_button_vtable;
}

// ========================================
// VTABLE CALLBACKS
// ========================================

static void _button_draw(EseGui *gui, EseGuiWidget *widget, EseDrawList *draw_list, size_t depth) {
	log_assert("GUI", gui, "_button_draw called with NULL gui");
	log_assert("GUI", widget, "_button_draw called with NULL widget");
	log_assert("GUI", draw_list, "_button_draw called with NULL draw_list");

	GuiButtonData *data = (GuiButtonData *)widget->data;
	log_assert("GUI", data, "_button_draw called with NULL data");

    size_t z_index = depth * 10;

	EseGuiStyleVariant variant = widget->variant;
	if (variant == GUI_STYLE_VARIANT_DEFAULT) {
		variant = GUI_STYLE_VARIANT_PRIMARY;
	}

    // --------- SELECT COLORS BY STATE (bootstrap logic) ----------
    EseColor *bg = ese_gui_style_get_bg(widget->style, variant);
    EseColor *border = ese_gui_style_get_border(widget->style, variant);
    EseColor *text = ese_gui_style_get_text(widget->style, variant);

    if (widget->is_down) {
        bg = ese_gui_style_get_bg_active(widget->style, variant);
        border = ese_gui_style_get_border_active(widget->style, variant);
		text = ese_gui_style_get_text_active(widget->style, variant);
    } else if (widget->is_hovered) {
        bg = ese_gui_style_get_bg_hover(widget->style, variant);
        border = ese_gui_style_get_border_hover(widget->style, variant);
		text = ese_gui_style_get_text_hover(widget->style, variant);
    }

    // Border width (default 1px in Bootstrap)
    int border_width = ese_gui_style_get_border_width(widget->style);
	if (border_width == GUI_STYLE_BORDER_WIDTH_WIDGET_DEFAULT) {
		border_width = 1;
	} else if (border_width < 0) {
		border_width = 0;
	}

	int font_size = ese_gui_style_get_font_size(widget->style);
	if (font_size == GUI_STYLE_FONT_SIZE_WIDGET_DEFAULT) {
		font_size = 20;
	} else if (font_size < 0) {
		font_size = 0;
	}

    // --------- DRAW BACKGROUND ----------
	EseDrawListObject *bg_obj = draw_list_request_object(draw_list);
	draw_list_object_set_rect_color(
		bg_obj,
		(unsigned char)(ese_color_get_r(bg) * 255),
		(unsigned char)(ese_color_get_g(bg) * 255),
		(unsigned char)(ese_color_get_b(bg) * 255),
		(unsigned char)(ese_color_get_a(bg) * 255),
		true // filled
	);
	draw_list_object_set_bounds(
		bg_obj, widget->x, widget->y, widget->width, widget->height
	);
	draw_list_object_set_z_index(bg_obj, z_index);

    // --------- DRAW BORDER ----------
    if (border_width > 0) {
        EseDrawListObject *border_obj = draw_list_request_object(draw_list);
        draw_list_object_set_rect_color(
            border_obj,
            (unsigned char)(ese_color_get_r(border) * 255),
            (unsigned char)(ese_color_get_g(border) * 255),
            (unsigned char)(ese_color_get_b(border) * 255),
            (unsigned char)(ese_color_get_a(border) * 255),
            false // border only
        );
        draw_list_object_set_bounds(
            border_obj, widget->x, widget->y, widget->width, widget->height
        );
        // draw_list_object_set_border_width(border_obj, border_width);
        draw_list_object_set_z_index(border_obj, z_index + 1);
    }

    // --------- DRAW BUTTON TEXT ----------
    if (data->text != NULL && font_size > 0) {
		float text_width = strlen(data->text) * (font_size * 0.6f); // Approximate character width
		float text_x = widget->x + (widget->width - text_width) / 2.0f;
		float text_y = widget->y + (widget->height - font_size) / 2.0f;

		EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(gui->engine->runtime, ENGINE_KEY);
		// Use font_draw_text_scaled to draw the text
		font_draw_text_scaled(engine, "console_font_10x20", data->text,
							text_x, text_y, z_index + 2, (float)font_size,
							_button_font_texture_callback, draw_list);
    }
}

static void _button_process_mouse_hover(EseGuiWidget *widget, int mouse_x, int mouse_y) {
	log_assert("GUI", widget, "_button_process_mouse_hover called with NULL widget");
	const bool inside = (mouse_x >= widget->x && mouse_x < widget->x + widget->width && mouse_y >= widget->y && mouse_y < widget->y + widget->height);
	widget->is_hovered = inside;
}

static bool _button_process_mouse_click(EseGuiWidget *widget, int mouse_x, int mouse_y, EseInputMouseButton button) {
	log_assert("GUI", widget, "_button_process_mouse_click called with NULL widget");
	(void)button;

	widget->is_hovered = (mouse_x >= widget->x && mouse_x < widget->x + widget->width && mouse_y >= widget->y && mouse_y < widget->y + widget->height);
	if (!widget->is_hovered) {
		return false;
	}

	GuiButtonData *data = (GuiButtonData *)widget->data;
	if (data && data->callback) {
		data->callback(data->userdata);
		memory_manager.free(data->userdata);
		data->userdata = NULL;
	}
	return true;
}

static void _button_layout(EseGuiWidget *widget) {
	(void)widget;
}

static EseGuiWidget *_button_create(EseGuiWidget *parent, EseGuiStyle *style) {
    EseGuiWidget *widget = (EseGuiWidget *)memory_manager.calloc(1, sizeof(EseGuiWidget), MMTAG_GUI);
	widget->parent = parent;
	memcpy(&widget->type, &g_button_vtable, sizeof(GuiWidgetVTable));
	widget->data = _button_data_create();
    widget->style = style;

	// Append to parent's children list if parent provided
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

static void _button_destroy(EseGuiWidget *widget) {
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
	_button_data_destroy((GuiButtonData *)widget->data);
	memory_manager.free(widget);
}

// ========================================
// LUA BINDINGS
// ========================================

static int _button_lua_init(EseLuaEngine *engine) {
	log_assert("GUI", engine, "_button_lua_init called with NULL engine");
	log_assert("GUI", engine->runtime, "_button_lua_init called with NULL lua runtime");

	// Attach to existing GUI table
	lua_getglobal(engine->runtime, "GUI");
	if (lua_isnil(engine->runtime, -1)) {
		lua_pop(engine->runtime, 1);
		log_error("GUI", "GUI table not found during button widget lua_init");
		return 0;
	}

    // GUI.push_button(text, callback[, userdata[, style]])
	lua_pushcfunction(engine->runtime, _button_lua_push);
	lua_setfield(engine->runtime, -2, "push_button");

	// pop GUI table
	lua_pop(engine->runtime, 1);

	return 0;
}

static void _button_invoke_lua_callback(void *ud) {
	LuaButtonCallbackData *cbd = (LuaButtonCallbackData *)ud;
	if (!cbd || !cbd->L) {
		return;
	}
	lua_rawgeti(cbd->L, LUA_REGISTRYINDEX, cbd->ref);
	lua_call(cbd->L, 0, 0);
}

static void _button_font_texture_callback(float screen_x, float screen_y, float screen_w, float screen_h, uint64_t z_index,
                                         const char *texture_id, float texture_x1, float texture_y1, float texture_x2, float texture_y2,
                                         int width, int height, void *user_data) {
    EseDrawList *draw_list = (EseDrawList *)user_data;
    EseDrawListObject *text_obj = draw_list_request_object(draw_list);
    draw_list_object_set_texture(text_obj, texture_id, texture_x1, texture_y1, texture_x2, texture_y2);
    draw_list_object_set_bounds(text_obj, screen_x, screen_y, (int)screen_w, (int)screen_h);
    draw_list_object_set_z_index(text_obj, z_index);
}

void ese_widget_button_set(EseGuiWidget *button, const char *text, void (*callback)(void *), void *userdata) {
    log_assert("GUI", button, "ese_widget_button_set called with NULL button");
    GuiButtonData *data = (GuiButtonData *)button->data;
    if (data->text) {
        memory_manager.free(data->text);
    }
    data->text = memory_manager.strdup(text, MMTAG_GUI);
    data->callback = callback;
    data->userdata = userdata;
}

static int _button_lua_push(lua_State *L) {
	log_assert("GUI_LUA", L, "_button_lua_push called with NULL Lua state");

	int n_args = lua_gettop(L);
	if (n_args < 2 || n_args > 4) {
		return luaL_error(L, "GUI.push_button(text, callback[, userdata[, style]]) takes 2 to 4 arguments");
	}

	EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
	EseGui *gui = engine_get_gui(engine);

	if (gui->open_layout == NULL) {
		return luaL_error(L, "GUI.push_button() called with no open GUI active");
	}

	const char *text = luaL_checkstring(L, 1);
	if (!lua_isfunction(L, 2)) {
		return luaL_error(L, "GUI.push_button() callback must be a function");
	}

	void *userdata = NULL;
	if (n_args >= 3) {
		userdata = lua_touserdata(L, 3);
	}

	EseGuiStyle *opt_style = NULL;
	if (n_args == 4) {
		opt_style = ese_gui_style_lua_get(L, 4);
		if (!opt_style) {
			return luaL_error(L, "style must be a GuiStyle");
		}
	}

	// Store function reference
	lua_pushvalue(L, 2);
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	// Ensure we have a current container to add a button under
	if (gui->open_layout->root == NULL && gui->open_layout->current_widget == NULL) {
		return luaL_error(L, "GUI.push_button() called with no open container");
	}
	if (gui->open_layout->current_widget == NULL) {
		return luaL_error(L, "GUI.push_button() called with no open container");
	}

	EseGuiWidget *parent = gui->open_layout->current_widget;
	EseGuiWidget *button = _button_create(parent, ese_gui_style_copy(opt_style ? opt_style : gui->default_style));
	button->width = GUI_AUTO_SIZE;
	button->height = GUI_AUTO_SIZE;
	button->variant = ese_gui_get_top_variant(gui);

	GuiButtonData *data = (GuiButtonData *)button->data;
    data->text = memory_manager.strdup(text, MMTAG_GUI);

    LuaButtonCallbackData *cb = (LuaButtonCallbackData *)memory_manager.calloc(1, sizeof(LuaButtonCallbackData), MMTAG_GUI);
	cb->L = L;
	cb->ref = ref;
	cb->userdata = userdata;
	data->userdata = cb;
	data->callback = _button_invoke_lua_callback;

	return 0;
}
