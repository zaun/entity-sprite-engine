#include <stdbool.h>
#include <string.h>

#include "core/memory_manager.h"
#include "core/engine.h"
#include "graphics/gui/gui.h"
#include "graphics/gui/gui_private.h"
#include "graphics/gui/gui_widget_flex.h"
#include "graphics/draw_list.h"
#include "scripting/lua_engine.h"
#include "types/gui_style.h"
#include "utility/log.h"

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

static void _flex_draw(EseGui *gui, EseGuiWidget *widget, EseDrawList *draw_list, size_t depth);
static void _flex_process_mouse_hover(EseGuiWidget *widget, int mouse_x, int mouse_y);
static bool _flex_process_mouse_click(EseGuiWidget *widget, int mouse_x, int mouse_y, EseInputMouseButton button);
static void _flex_layout(EseGuiWidget *widget);
static EseGuiWidget *_flex_create(EseGuiWidget *parent, EseGuiStyle *style);
static void _flex_destroy(EseGuiWidget *widget);
static int _flex_lua_init(EseLuaEngine *engine);
static int _flex_lua_open(lua_State *L);
static int _flex_lua_close(lua_State *L);

// ========================================
// PRIVATE TYPES
// ========================================

typedef struct GuiFlexData {
	EseGuiFlexDirection direction;
	EseGuiFlexJustify justify;
	EseGuiFlexAlignItems align_items;
	int spacing;
} GuiFlexData;

static GuiFlexData *_flex_data_create(void) {
    GuiFlexData *data = (GuiFlexData *)memory_manager.calloc(1, sizeof(GuiFlexData), MMTAG_GUI);
	data->direction = FLEX_DIRECTION_ROW;
	data->justify = FLEX_JUSTIFY_START;
	data->align_items = FLEX_ALIGN_ITEMS_START;
	data->spacing = 0;
	return data;
}

static void _flex_data_destroy(GuiFlexData *data) {
	if (!data) {
		return;
	}
	memory_manager.free(data);
}

// ========================================
// PRIVATE STATE
// ========================================

static GuiWidgetVTable g_flex_vtable = {
	.id = "FLEX",
	.is_container = true,
	.draw = _flex_draw,
	.process_mouse_hover = _flex_process_mouse_hover,
	.process_mouse_click = _flex_process_mouse_click,
	.layout = _flex_layout,
	.create = _flex_create,
	.destroy = _flex_destroy,
	.lua_init = _flex_lua_init,
};

// ========================================
// PUBLIC FUNCTIONS
// ========================================

GuiWidgetVTable *ese_widget_flex_get_vtable(void) {
	return &g_flex_vtable;
}

// ========================================
// VTABLE CALLBACKS
// ========================================

static void _flex_draw(EseGui *gui, EseGuiWidget *widget, EseDrawList *draw_list, size_t depth) {
	log_assert("GUI", gui, "_flex_draw called with NULL gui");
	log_assert("GUI", widget, "_flex_draw called with NULL widget");
	log_assert("GUI", draw_list, "_flex_draw called with NULL draw_list");

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

	for (size_t i = 0; i < widget->children_count; i++) {
		widget->children[i]->type.draw(gui, widget->children[i], draw_list, depth + 1);
	}
}

static void _flex_process_mouse_hover(EseGuiWidget *widget, int mouse_x, int mouse_y) {
	log_assert("GUI", widget, "_flex_process_mouse_hover called with NULL widget");

	const bool inside = (mouse_x >= widget->x && mouse_x < widget->x + widget->width && mouse_y >= widget->y && mouse_y < widget->y + widget->height);
	widget->is_hovered = inside;

	for (size_t i = 0; i < widget->children_count; i++) {
		widget->children[i]->type.process_mouse_hover(widget->children[i], mouse_x, mouse_y);
	}
}

static bool _flex_process_mouse_click(EseGuiWidget *widget, int mouse_x, int mouse_y, EseInputMouseButton button) {
	log_assert("GUI", widget, "_flex_process_mouse_click called with NULL widget");
	(void)button;

	widget->is_hovered = (mouse_x >= widget->x && mouse_x < widget->x + widget->width && mouse_y >= widget->y && mouse_y < widget->y + widget->height);
	if (!widget->is_hovered) {
		return false;
	}

	for (size_t i = 0; i < widget->children_count; i++) {
		if (widget->children[i]->type.process_mouse_click(widget->children[i], mouse_x, mouse_y, button)) {
			return true;
		}
	}

	return false;
}

static void _flex_layout(EseGuiWidget *widget) {
	log_assert("GUI", widget, "_flex_layout called with NULL widget");

	GuiFlexData *data = (GuiFlexData *)widget->data;
	const int inner_w = widget->width - ese_gui_style_get_padding_left(widget->style) - ese_gui_style_get_padding_right(widget->style);
	const int inner_h = widget->height - ese_gui_style_get_padding_top(widget->style) - ese_gui_style_get_padding_bottom(widget->style);
	const int child_count = (int)widget->children_count;
	const int total_spacing = (child_count > 0) ? data->spacing * (child_count - 1) : 0;
    log_debug("GUI", "_flex_layout: widget=%p type=%s children=%zu inner=%dx%d spacing=%d\n", (void*)widget, widget->type.id, widget->children_count, inner_w, inner_h, data->spacing);

	if (data->direction == FLEX_DIRECTION_ROW) {
		int fixed_width = 0;
		int auto_count = 0;
		for (size_t i = 0; i < widget->children_count; i++) {
			EseGuiWidget *child = widget->children[i];
			if (child->width == GUI_AUTO_SIZE) {
				auto_count++;
			} else {
				fixed_width += child->width;
			}
		}
		int total_free_width = inner_w - total_spacing;
		if (total_free_width < 0) total_free_width = 0;
		int auto_width = (auto_count > 0) ? (total_free_width - fixed_width) / auto_count : 0;
		if (auto_width < 0) auto_width = 0;

		int start_x = widget->x;
		if (data->justify == FLEX_JUSTIFY_START) {
			start_x += ese_gui_style_get_padding_left(widget->style);
		} else if (data->justify == FLEX_JUSTIFY_END) {
			start_x += widget->width - ese_gui_style_get_padding_right(widget->style) - (fixed_width + total_spacing + auto_count * auto_width);
		} else if (data->justify == FLEX_JUSTIFY_CENTER) {
			start_x += ese_gui_style_get_padding_left(widget->style) + (inner_w - (fixed_width + total_spacing + auto_count * auto_width)) / 2;
		}
		int start_y = widget->y + ese_gui_style_get_padding_top(widget->style);

        for (size_t i = 0; i < widget->children_count; i++) {
            EseGuiWidget *child = widget->children[i];
            log_debug("GUI", "_flex_layout: child[%zu]=%p type=%s size=%dx%d\n", i, (void*)child, child ? child->type.id : "<null>", child ? child->width : -9999, child ? child->height : -9999);
			child->x = start_x;
			if (child->width == GUI_AUTO_SIZE) {
				child->width = auto_width;
			}
			if (child->height == GUI_AUTO_SIZE) {
				child->height = inner_h;
			}
			if (data->align_items == FLEX_ALIGN_ITEMS_START) {
				child->y = start_y;
			} else if (data->align_items == FLEX_ALIGN_ITEMS_END) {
				child->y = start_y + inner_h - child->height;
			} else if (data->align_items == FLEX_ALIGN_ITEMS_CENTER) {
				child->y = start_y + (inner_h - child->height) / 2;
			}

			child->type.layout(child);

			start_x += child->width + data->spacing;
		}
	} else if (data->direction == FLEX_DIRECTION_COLUMN) {
		int fixed_height = 0;
		int auto_count = 0;
		for (size_t i = 0; i < widget->children_count; i++) {
			EseGuiWidget *child = widget->children[i];
			if (child->height == GUI_AUTO_SIZE) {
				auto_count++;
			} else {
				fixed_height += child->height;
			}
		}
		int total_free_height = inner_h - total_spacing;
		if (total_free_height < 0) total_free_height = 0;
		int auto_height = (auto_count > 0) ? (total_free_height - fixed_height) / auto_count : 0;
		if (auto_height < 0) auto_height = 0;

		int start_y = widget->y;
		if (data->justify == FLEX_JUSTIFY_START) {
			start_y += ese_gui_style_get_padding_top(widget->style);
		} else if (data->justify == FLEX_JUSTIFY_END) {
			start_y += widget->height - ese_gui_style_get_padding_bottom(widget->style) - (fixed_height + total_spacing + auto_count * auto_height);
		} else if (data->justify == FLEX_JUSTIFY_CENTER) {
			start_y += ese_gui_style_get_padding_top(widget->style) + (inner_h - (fixed_height + total_spacing + auto_count * auto_height)) / 2;
		}
		int start_x = widget->x + ese_gui_style_get_padding_left(widget->style);

        for (size_t i = 0; i < widget->children_count; i++) {
            EseGuiWidget *child = widget->children[i];
            log_debug("GUI", "_flex_layout: child[%zu]=%p type=%s size=%dx%d\n", i, (void*)child, child ? child->type.id : "<null>", child ? child->width : -9999, child ? child->height : -9999);
            if (!child) {
                continue;
            }
			child->y = start_y;
			if (child->height == GUI_AUTO_SIZE) {
				child->height = auto_height;
			}
			if (child->width == GUI_AUTO_SIZE) {
				child->width = inner_w;
			}
			if (data->align_items == FLEX_ALIGN_ITEMS_START) {
				child->x = start_x;
			} else if (data->align_items == FLEX_ALIGN_ITEMS_END) {
				child->x = start_x + inner_w - child->width;
			} else if (data->align_items == FLEX_ALIGN_ITEMS_CENTER) {
				child->x = start_x + (inner_w - child->width) / 2;
			}

			child->type.layout(child);

			start_y += child->height + data->spacing;
		}
	}
}

static EseGuiWidget *_flex_create(EseGuiWidget *parent, EseGuiStyle *style) {
    EseGuiWidget *widget = (EseGuiWidget *)memory_manager.calloc(1, sizeof(EseGuiWidget), MMTAG_GUI);
	widget->parent = parent;
	memcpy(&widget->type, &g_flex_vtable, sizeof(GuiWidgetVTable));
	widget->data = _flex_data_create();
    widget->style = style;

	// Copy style properties to flex data
	if (style != NULL) {
		GuiFlexData *data = (GuiFlexData *)widget->data;
		data->direction = FLEX_DIRECTION_ROW;
		data->justify = FLEX_JUSTIFY_START;
		data->align_items = FLEX_ALIGN_ITEMS_START;
	}

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

static void _flex_destroy(EseGuiWidget *widget) {
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
	_flex_data_destroy((GuiFlexData *)widget->data);
	memory_manager.free(widget);
}

static int _flex_lua_init(EseLuaEngine *engine) {
	log_assert("GUI", engine, "_flex_lua_init called with NULL engine");
	log_assert("GUI", engine->runtime, "_flex_lua_init called with NULL lua runtime");

	// Attach to existing GUI table
	lua_getglobal(engine->runtime, "GUI");
	if (lua_isnil(engine->runtime, -1)) {
		lua_pop(engine->runtime, 1);
		log_error("GUI", "GUI table not found during flex widget lua_init");
		return 0;
	}

	// GUI.open_flex
	lua_pushcfunction(engine->runtime, _flex_lua_open);
	lua_setfield(engine->runtime, -2, "open_flex");

	// GUI.close_flex
	lua_pushcfunction(engine->runtime, _flex_lua_close);
	lua_setfield(engine->runtime, -2, "close_flex");

	// Ensure STYLE constants exist: DIRECTION, JUSTIFY, ALIGN (mirrors old GUI init)
	// Get GUI.STYLE (create if missing)
	lua_getfield(engine->runtime, -1, "STYLE");
	const bool style_exists = !lua_isnil(engine->runtime, -1);
	if (!style_exists) {
		// remove nil
		lua_pop(engine->runtime, 1);
		// create STYLE table
		lua_newtable(engine->runtime);

		// STYLE.DIRECTION
		lua_newtable(engine->runtime);
		lua_pushinteger(engine->runtime, FLEX_DIRECTION_ROW);
		lua_setfield(engine->runtime, -2, "ROW");
		lua_pushinteger(engine->runtime, FLEX_DIRECTION_COLUMN);
		lua_setfield(engine->runtime, -2, "COLUMN");
		lua_setfield(engine->runtime, -2, "DIRECTION");

		// STYLE.JUSTIFY
		lua_newtable(engine->runtime);
		lua_pushinteger(engine->runtime, FLEX_JUSTIFY_START);
		lua_setfield(engine->runtime, -2, "START");
		lua_pushinteger(engine->runtime, FLEX_JUSTIFY_CENTER);
		lua_setfield(engine->runtime, -2, "CENTER");
		lua_pushinteger(engine->runtime, FLEX_JUSTIFY_END);
		lua_setfield(engine->runtime, -2, "END");
		lua_setfield(engine->runtime, -2, "JUSTIFY");

		// STYLE.ALIGN
		lua_newtable(engine->runtime);
		lua_pushinteger(engine->runtime, FLEX_ALIGN_ITEMS_START);
		lua_setfield(engine->runtime, -2, "START");
		lua_pushinteger(engine->runtime, FLEX_ALIGN_ITEMS_CENTER);
		lua_setfield(engine->runtime, -2, "CENTER");
		lua_pushinteger(engine->runtime, FLEX_ALIGN_ITEMS_END);
		lua_setfield(engine->runtime, -2, "END");
		lua_setfield(engine->runtime, -2, "ALIGN");

		// Attach STYLE to GUI
		lua_setfield(engine->runtime, -2, "STYLE");
	} else {
		// STYLE exists; set/overwrite the three constant tables
		// STYLE table is at stack top
		// STYLE.DIRECTION
		lua_newtable(engine->runtime);
		lua_pushinteger(engine->runtime, FLEX_DIRECTION_ROW);
		lua_setfield(engine->runtime, -2, "ROW");
		lua_pushinteger(engine->runtime, FLEX_DIRECTION_COLUMN);
		lua_setfield(engine->runtime, -2, "COLUMN");
		lua_setfield(engine->runtime, -2, "DIRECTION");

		// STYLE.JUSTIFY
		lua_newtable(engine->runtime);
		lua_pushinteger(engine->runtime, FLEX_JUSTIFY_START);
		lua_setfield(engine->runtime, -2, "START");
		lua_pushinteger(engine->runtime, FLEX_JUSTIFY_CENTER);
		lua_setfield(engine->runtime, -2, "CENTER");
		lua_pushinteger(engine->runtime, FLEX_JUSTIFY_END);
		lua_setfield(engine->runtime, -2, "END");
		lua_setfield(engine->runtime, -2, "JUSTIFY");

		// STYLE.ALIGN
		lua_newtable(engine->runtime);
		lua_pushinteger(engine->runtime, FLEX_ALIGN_ITEMS_START);
		lua_setfield(engine->runtime, -2, "START");
		lua_pushinteger(engine->runtime, FLEX_ALIGN_ITEMS_CENTER);
		lua_setfield(engine->runtime, -2, "CENTER");
		lua_pushinteger(engine->runtime, FLEX_ALIGN_ITEMS_END);
		lua_setfield(engine->runtime, -2, "END");
		lua_setfield(engine->runtime, -2, "ALIGN");

		// pop STYLE
		lua_pop(engine->runtime, 1);
	}

	// pop GUI table
	lua_pop(engine->runtime, 1);

	return 0;
}

static int _flex_lua_open(lua_State *L) {
	log_assert("GUI_LUA", L, "_flex_lua_open called with NULL Lua state");

	int n_args = lua_gettop(L);
	if (n_args <= 3 || n_args > 6) {
		return luaL_error(L, "GUI.open_flex(direction, justify, align_items, [width[, height[, style]]]) takes 3 to 6 arguments");
	}

	if (lua_type(L, 1) != LUA_TNUMBER) {
		return luaL_error(L, "direction must be a number");
	}

	if (lua_type(L, 2) != LUA_TNUMBER) {
		return luaL_error(L, "justify must be a number");
	}

	if (lua_type(L, 3) != LUA_TNUMBER) {
		return luaL_error(L, "align_items must be a number");
	}

	if (n_args >= 4 && lua_type(L, 4) != LUA_TNUMBER) {
		return luaL_error(L, "width must be a number or GUI.AUTO_SIZE");
	}

	if (n_args >= 5 && lua_type(L, 5) != LUA_TNUMBER) {
		return luaL_error(L, "height must be a number or GUI.AUTO_SIZE");
	}

	EseGuiFlexDirection direction = (EseGuiFlexDirection)lua_tonumber(L, 1);
	EseGuiFlexJustify justify = (EseGuiFlexJustify)lua_tonumber(L, 2);
	EseGuiFlexAlignItems align_items = (EseGuiFlexAlignItems)lua_tonumber(L, 3);

	int width = GUI_AUTO_SIZE;
	int height = GUI_AUTO_SIZE;
	if (n_args >= 4) {
		width = (int)lua_tonumber(L, 4);
	}
	if (n_args >= 5) {
		height = (int)lua_tonumber(L, 5);
	}

	EseGuiStyle *opt_style = NULL;
	if (n_args >= 6) {
		opt_style = ese_gui_style_lua_get(L, 6);
		if (!opt_style) {
			return luaL_error(L, "style must be a GuiStyle");
		}
	}

	EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
	EseGui *gui = engine_get_gui(engine);
	if (gui->open_layout == NULL) {
		return luaL_error(L, "GUI.open_flex() called with no open GUI active");
	}

	   if (gui->open_layout->root == NULL && gui->open_layout->current_widget == NULL) {
        // first widget in layout
		gui->open_layout->current_widget = _flex_create(NULL, ese_gui_style_copy(opt_style ? opt_style : gui->default_style));
        gui->open_layout->root = gui->open_layout->current_widget;
    } else if (gui->open_layout->current_widget == NULL || gui->open_layout->current_widget->type.is_container == false) {
        return luaL_error(L, "GUI.open_flex() called with no open container active");
    } else {
		gui->open_layout->current_widget = _flex_create(gui->open_layout->current_widget, ese_gui_style_copy(opt_style ? opt_style : gui->default_style));
    }

	GuiFlexData *data = (GuiFlexData *)gui->open_layout->current_widget->data;
	data->direction = direction;
	data->justify = justify;
	data->align_items = align_items;

	gui->open_layout->current_widget->width = width;
	gui->open_layout->current_widget->height = height;
	gui->open_layout->current_widget->variant = ese_gui_get_top_variant(gui);

	return 0;
}

static int _flex_lua_close(lua_State *L) {
	log_assert("GUI_LUA", L, "_flex_lua_close called with NULL Lua state");

	int n_args = lua_gettop(L);
	if (n_args != 0) {
		return luaL_error(L, "GUI.close_flex() takes no arguments");
	}

	EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
	EseGui *gui = engine_get_gui(engine);

	if (gui->open_layout == NULL) {
		return luaL_error(L, "GUI.close_flex() called with no open GUI active");
	}

    EseGuiWidget *current = gui->open_layout->current_widget;
    if (current == NULL ||
        strncmp(current->type.id, "FLEX", sizeof(current->type.id)) != 0) {
		return luaL_error(L, "GUI.close_flex() called but current container is not a Flex");
	}

    // Move current to parent (or clear if none)
    gui->open_layout->current_widget = current->parent;
	return 0;
}


