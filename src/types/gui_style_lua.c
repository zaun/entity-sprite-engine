#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/gui_style.h"
#include "types/color.h"
#include "graphics/gui/gui.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"
#include "types/gui_style_lua.h"

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
extern EseGuiStyle *_ese_gui_style_make(void);
extern void _ese_gui_style_notify_watchers(EseGuiStyle *style);

// Lua metamethods
static int _ese_gui_style_lua_gc(lua_State *L);
static int _ese_gui_style_lua_index(lua_State *L);
static int _ese_gui_style_lua_newindex(lua_State *L);
static int _ese_gui_style_lua_tostring(lua_State *L);

// Lua constructors
static int _ese_gui_style_lua_new(lua_State *L);

// Lua JSON methods
static int _ese_gui_style_lua_from_json(lua_State *L);
static int _ese_gui_style_lua_to_json(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseGuiStyle
 * 
 * Handles cleanup when a Lua proxy table for an EseGuiStyle is garbage collected.
 * Only frees the underlying EseGuiStyle if it has no C-side references.
 * 
 * @param L Lua state
 * @return 0 (no return values)
 */
static int _ese_gui_style_lua_gc(lua_State *L) {
    // Get from userdata
    EseGuiStyle **ud = (EseGuiStyle **)luaL_testudata(L, 1, GUI_STYLE_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseGuiStyle *style = *ud;
    if (style) {
        // If lua_ref == LUA_NOREF, there are no more references to this style, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this style was referenced from C and should not be freed.
        if (ese_gui_style_get_lua_ref(style) == LUA_NOREF) {
            printf("ese_gui_style_lua_gc: No Lua references, safe to free immediately\n");
            ese_gui_style_destroy(style);
        }
    }

    return 0;
}

/**
 * @brief Lua __index metamethod for EseGuiStyle property access
 * 
 * Provides read access to style properties from Lua. When a Lua script
 * accesses style.property, this function is called to retrieve the values.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid properties, 0 for invalid)
 */
static int _ese_gui_style_lua_index(lua_State *L) {
	profile_start(PROFILE_LUA_GUI_STYLE_INDEX);
	EseGuiStyle *style = ese_gui_style_lua_get(L, 1);
	const char *key = lua_tostring(L, 2);
	if (!style || !key) {
		profile_cancel(PROFILE_LUA_GUI_STYLE_INDEX);
		return 0;
	}

	// Numeric properties
	if (strcmp(key, "direction") == 0) {
		lua_pushnumber(L, ese_gui_style_get_direction(style));
		profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
		return 1;
	} else if (strcmp(key, "justify") == 0) {
		lua_pushnumber(L, ese_gui_style_get_justify(style));
		profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
		return 1;
	} else if (strcmp(key, "align_items") == 0) {
		lua_pushnumber(L, ese_gui_style_get_align_items(style));
		profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
		return 1;
	} else if (strcmp(key, "border_width") == 0) {
		lua_pushnumber(L, ese_gui_style_get_border_width(style));
		profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
		return 1;
	} else if (strcmp(key, "padding_left") == 0) {
		lua_pushnumber(L, ese_gui_style_get_padding_left(style));
		profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
		return 1;
	} else if (strcmp(key, "padding_top") == 0) {
		lua_pushnumber(L, ese_gui_style_get_padding_top(style));
		profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
		return 1;
	} else if (strcmp(key, "padding_right") == 0) {
		lua_pushnumber(L, ese_gui_style_get_padding_right(style));
		profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
		return 1;
	} else if (strcmp(key, "padding_bottom") == 0) {
		lua_pushnumber(L, ese_gui_style_get_padding_bottom(style));
		profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
		return 1;
	} else if (strcmp(key, "spacing") == 0) {
		lua_pushnumber(L, ese_gui_style_get_spacing(style));
		profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
		return 1;
	} else if (strcmp(key, "font_size") == 0) {
		lua_pushnumber(L, ese_gui_style_get_font_size(style));
		profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
		return 1;
	}

	// Theme/context colors
	if (strcmp(key, "primary") == 0) { ese_color_lua_push(ese_gui_style_get_primary(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "primary_hover") == 0) { ese_color_lua_push(ese_gui_style_get_primary_hover(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "primary_active") == 0) { ese_color_lua_push(ese_gui_style_get_primary_active(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "secondary") == 0) { ese_color_lua_push(ese_gui_style_get_secondary(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "secondary_hover") == 0) { ese_color_lua_push(ese_gui_style_get_secondary_hover(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "secondary_active") == 0) { ese_color_lua_push(ese_gui_style_get_secondary_active(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "success") == 0) { ese_color_lua_push(ese_gui_style_get_success(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "success_hover") == 0) { ese_color_lua_push(ese_gui_style_get_success_hover(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "success_active") == 0) { ese_color_lua_push(ese_gui_style_get_success_active(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "info") == 0) { ese_color_lua_push(ese_gui_style_get_info(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "info_hover") == 0) { ese_color_lua_push(ese_gui_style_get_info_hover(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "info_active") == 0) { ese_color_lua_push(ese_gui_style_get_info_active(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "warning") == 0) { ese_color_lua_push(ese_gui_style_get_warning(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "warning_hover") == 0) { ese_color_lua_push(ese_gui_style_get_warning_hover(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "warning_active") == 0) { ese_color_lua_push(ese_gui_style_get_warning_active(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "danger") == 0) { ese_color_lua_push(ese_gui_style_get_danger(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "danger_hover") == 0) { ese_color_lua_push(ese_gui_style_get_danger_hover(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "danger_active") == 0) { ese_color_lua_push(ese_gui_style_get_danger_active(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "light") == 0) { ese_color_lua_push(ese_gui_style_get_light(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "light_hover") == 0) { ese_color_lua_push(ese_gui_style_get_light_hover(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "light_active") == 0) { ese_color_lua_push(ese_gui_style_get_light_active(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "dark") == 0) { ese_color_lua_push(ese_gui_style_get_dark(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "dark_hover") == 0) { ese_color_lua_push(ese_gui_style_get_dark_hover(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "dark_active") == 0) { ese_color_lua_push(ese_gui_style_get_dark_active(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }

	// Alerts
	if (strcmp(key, "alert_success_bg") == 0) { ese_color_lua_push(ese_gui_style_get_alert_success_bg(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "alert_success_text") == 0) { ese_color_lua_push(ese_gui_style_get_alert_success_text(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "alert_success_border") == 0) { ese_color_lua_push(ese_gui_style_get_alert_success_border(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "alert_info_bg") == 0) { ese_color_lua_push(ese_gui_style_get_alert_info_bg(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "alert_info_text") == 0) { ese_color_lua_push(ese_gui_style_get_alert_info_text(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "alert_info_border") == 0) { ese_color_lua_push(ese_gui_style_get_alert_info_border(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "alert_warning_bg") == 0) { ese_color_lua_push(ese_gui_style_get_alert_warning_bg(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "alert_warning_text") == 0) { ese_color_lua_push(ese_gui_style_get_alert_warning_text(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "alert_warning_border") == 0) { ese_color_lua_push(ese_gui_style_get_alert_warning_border(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "alert_danger_bg") == 0) { ese_color_lua_push(ese_gui_style_get_alert_danger_bg(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "alert_danger_text") == 0) { ese_color_lua_push(ese_gui_style_get_alert_danger_text(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "alert_danger_border") == 0) { ese_color_lua_push(ese_gui_style_get_alert_danger_border(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }

	// Backgrounds
	if (strcmp(key, "bg_primary") == 0) { ese_color_lua_push(ese_gui_style_get_bg_primary(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "bg_secondary") == 0) { ese_color_lua_push(ese_gui_style_get_bg_secondary(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "bg_success") == 0) { ese_color_lua_push(ese_gui_style_get_bg_success(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "bg_info") == 0) { ese_color_lua_push(ese_gui_style_get_bg_info(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "bg_warning") == 0) { ese_color_lua_push(ese_gui_style_get_bg_warning(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "bg_danger") == 0) { ese_color_lua_push(ese_gui_style_get_bg_danger(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "bg_light") == 0) { ese_color_lua_push(ese_gui_style_get_bg_light(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "bg_dark") == 0) { ese_color_lua_push(ese_gui_style_get_bg_dark(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "bg_white") == 0) { ese_color_lua_push(ese_gui_style_get_bg_white(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "bg_transparent") == 0) { ese_color_lua_push(ese_gui_style_get_bg_transparent(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }

	// Text colors
	if (strcmp(key, "text_primary") == 0) { ese_color_lua_push(ese_gui_style_get_text_primary(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "text_secondary") == 0) { ese_color_lua_push(ese_gui_style_get_text_secondary(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "text_success") == 0) { ese_color_lua_push(ese_gui_style_get_text_success(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "text_info") == 0) { ese_color_lua_push(ese_gui_style_get_text_info(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "text_warning") == 0) { ese_color_lua_push(ese_gui_style_get_text_warning(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "text_danger") == 0) { ese_color_lua_push(ese_gui_style_get_text_danger(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "text_light") == 0) { ese_color_lua_push(ese_gui_style_get_text_light(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "text_dark") == 0) { ese_color_lua_push(ese_gui_style_get_text_dark(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "text_body") == 0) { ese_color_lua_push(ese_gui_style_get_text_body(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "text_muted") == 0) { ese_color_lua_push(ese_gui_style_get_text_muted(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "text_white") == 0) { ese_color_lua_push(ese_gui_style_get_text_white(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "text_black") == 0) { ese_color_lua_push(ese_gui_style_get_text_black(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "text_reset") == 0) { ese_color_lua_push(ese_gui_style_get_text_reset(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }

	// Borders
	if (strcmp(key, "border_primary") == 0) { ese_color_lua_push(ese_gui_style_get_border_primary(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "border_secondary") == 0) { ese_color_lua_push(ese_gui_style_get_border_secondary(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "border_success") == 0) { ese_color_lua_push(ese_gui_style_get_border_success(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "border_info") == 0) { ese_color_lua_push(ese_gui_style_get_border_info(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "border_warning") == 0) { ese_color_lua_push(ese_gui_style_get_border_warning(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "border_danger") == 0) { ese_color_lua_push(ese_gui_style_get_border_danger(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "border_light") == 0) { ese_color_lua_push(ese_gui_style_get_border_light(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "border_dark") == 0) { ese_color_lua_push(ese_gui_style_get_border_dark(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "border_white") == 0) { ese_color_lua_push(ese_gui_style_get_border_white(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "border_gray_200") == 0) { ese_color_lua_push(ese_gui_style_get_border_gray_200(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "border_gray_300") == 0) { ese_color_lua_push(ese_gui_style_get_border_gray_300(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }

	// Tooltips and misc
	if (strcmp(key, "tooltip_bg") == 0) { ese_color_lua_push(ese_gui_style_get_tooltip_bg(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "tooltip_color") == 0) { ese_color_lua_push(ese_gui_style_get_tooltip_color(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "selection_bg") == 0) { ese_color_lua_push(ese_gui_style_get_selection_bg(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "selection_color") == 0) { ese_color_lua_push(ese_gui_style_get_selection_color(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "focus_ring") == 0) { ese_color_lua_push(ese_gui_style_get_focus_ring(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "highlight") == 0) { ese_color_lua_push(ese_gui_style_get_highlight(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }
	if (strcmp(key, "list_group_action") == 0) { ese_color_lua_push(ese_gui_style_get_list_group_action(style)); profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)"); return 1; }

	// Methods
	if (strcmp(key, "toJSON") == 0) {
		lua_pushlightuserdata(L, style);
		lua_pushcclosure(L, _ese_gui_style_lua_to_json, 1);
		profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (method)");
		return 1;
	}
	if (strcmp(key, "fromJSON") == 0) {
		lua_pushlightuserdata(L, style);
		lua_pushcclosure(L, _ese_gui_style_lua_from_json, 1);
		profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (method)");
		return 1;
	}

	profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (not found)");
	return 0;
}

/**
 * @brief Lua __newindex metamethod for EseGuiStyle property assignment
 * 
 * Provides write access to style properties from Lua. When a Lua script
 * assigns style.property = value, this function is called to set the values.
 * 
 * @param L Lua state
 * @return 0 (no return values)
 */
static int _ese_gui_style_lua_newindex(lua_State *L) {
    profile_start(PROFILE_LUA_GUI_STYLE_NEWINDEX);
    EseGuiStyle *style = ese_gui_style_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!style || !key) {
        profile_cancel(PROFILE_LUA_GUI_STYLE_NEWINDEX);
        return 0;
    }

    if (strcmp(key, "direction") == 0) {
        if (lua_isnumber(L, 3)) {
            ese_gui_style_set_direction(style, (EseGuiFlexDirection)lua_tointeger(L, 3));
        }
    } else if (strcmp(key, "justify") == 0) {
        if (lua_isnumber(L, 3)) {
            ese_gui_style_set_justify(style, (EseGuiFlexJustify)lua_tointeger(L, 3));
        }
    } else if (strcmp(key, "align_items") == 0) {
        if (lua_isnumber(L, 3)) {
            ese_gui_style_set_align_items(style, (EseGuiFlexAlignItems)lua_tointeger(L, 3));
        }
    } else if (strcmp(key, "border_width") == 0) {
        if (lua_isnumber(L, 3)) {
            ese_gui_style_set_border_width(style, (int)lua_tointeger(L, 3));
        }
    } else if (strcmp(key, "padding_left") == 0) {
        if (lua_isnumber(L, 3)) {
            ese_gui_style_set_padding_left(style, (int)lua_tointeger(L, 3));
        }
    } else if (strcmp(key, "padding_top") == 0) {
        if (lua_isnumber(L, 3)) {
            ese_gui_style_set_padding_top(style, (int)lua_tointeger(L, 3));
        }
    } else if (strcmp(key, "padding_right") == 0) {
        if (lua_isnumber(L, 3)) {
            ese_gui_style_set_padding_right(style, (int)lua_tointeger(L, 3));
        }
    } else if (strcmp(key, "padding_bottom") == 0) {
        if (lua_isnumber(L, 3)) {
            ese_gui_style_set_padding_bottom(style, (int)lua_tointeger(L, 3));
        }
    } else if (strcmp(key, "spacing") == 0) {
        if (lua_isnumber(L, 3)) {
            ese_gui_style_set_spacing(style, (int)lua_tointeger(L, 3));
        }
    } else if (strcmp(key, "font_size") == 0) {
        if (lua_isnumber(L, 3)) {
            ese_gui_style_set_font_size(style, (int)lua_tointeger(L, 3));
        }
    // Theme/context setters
    } else if (strcmp(key, "primary") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_primary(style, c);
    } else if (strcmp(key, "primary_hover") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_primary_hover(style, c);
    } else if (strcmp(key, "primary_active") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_primary_active(style, c);
    } else if (strcmp(key, "secondary") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_secondary(style, c);
    } else if (strcmp(key, "secondary_hover") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_secondary_hover(style, c);
    } else if (strcmp(key, "secondary_active") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_secondary_active(style, c);
    } else if (strcmp(key, "success") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_success(style, c);
    } else if (strcmp(key, "success_hover") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_success_hover(style, c);
    } else if (strcmp(key, "success_active") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_success_active(style, c);
    } else if (strcmp(key, "info") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_info(style, c);
    } else if (strcmp(key, "info_hover") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_info_hover(style, c);
    } else if (strcmp(key, "info_active") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_info_active(style, c);
    } else if (strcmp(key, "warning") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_warning(style, c);
    } else if (strcmp(key, "warning_hover") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_warning_hover(style, c);
    } else if (strcmp(key, "warning_active") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_warning_active(style, c);
    } else if (strcmp(key, "danger") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_danger(style, c);
    } else if (strcmp(key, "danger_hover") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_danger_hover(style, c);
    } else if (strcmp(key, "danger_active") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_danger_active(style, c);
    } else if (strcmp(key, "light") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_light(style, c);
    } else if (strcmp(key, "light_hover") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_light_hover(style, c);
    } else if (strcmp(key, "light_active") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_light_active(style, c);
    } else if (strcmp(key, "dark") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_dark(style, c);
    } else if (strcmp(key, "dark_hover") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_dark_hover(style, c);
    } else if (strcmp(key, "dark_active") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_dark_active(style, c);

    // Alerts
    } else if (strcmp(key, "alert_success_bg") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_alert_success_bg(style, c);
    } else if (strcmp(key, "alert_success_text") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_alert_success_text(style, c);
    } else if (strcmp(key, "alert_success_border") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_alert_success_border(style, c);
    } else if (strcmp(key, "alert_info_bg") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_alert_info_bg(style, c);
    } else if (strcmp(key, "alert_info_text") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_alert_info_text(style, c);
    } else if (strcmp(key, "alert_info_border") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_alert_info_border(style, c);
    } else if (strcmp(key, "alert_warning_bg") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_alert_warning_bg(style, c);
    } else if (strcmp(key, "alert_warning_text") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_alert_warning_text(style, c);
    } else if (strcmp(key, "alert_warning_border") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_alert_warning_border(style, c);
    } else if (strcmp(key, "alert_danger_bg") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_alert_danger_bg(style, c);
    } else if (strcmp(key, "alert_danger_text") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_alert_danger_text(style, c);
    } else if (strcmp(key, "alert_danger_border") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_alert_danger_border(style, c);

    // Backgrounds
    } else if (strcmp(key, "bg_primary") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_bg_primary(style, c);
    } else if (strcmp(key, "bg_secondary") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_bg_secondary(style, c);
    } else if (strcmp(key, "bg_success") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_bg_success(style, c);
    } else if (strcmp(key, "bg_info") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_bg_info(style, c);
    } else if (strcmp(key, "bg_warning") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_bg_warning(style, c);
    } else if (strcmp(key, "bg_danger") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_bg_danger(style, c);
    } else if (strcmp(key, "bg_light") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_bg_light(style, c);
    } else if (strcmp(key, "bg_dark") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_bg_dark(style, c);
    } else if (strcmp(key, "bg_white") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_bg_white(style, c);
    } else if (strcmp(key, "bg_transparent") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_bg_transparent(style, c);

    // Text colors
    } else if (strcmp(key, "text_primary") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_text_primary(style, c);
    } else if (strcmp(key, "text_secondary") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_text_secondary(style, c);
    } else if (strcmp(key, "text_success") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_text_success(style, c);
    } else if (strcmp(key, "text_info") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_text_info(style, c);
    } else if (strcmp(key, "text_warning") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_text_warning(style, c);
    } else if (strcmp(key, "text_danger") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_text_danger(style, c);
    } else if (strcmp(key, "text_light") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_text_light(style, c);
    } else if (strcmp(key, "text_dark") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_text_dark(style, c);
    } else if (strcmp(key, "text_body") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_text_body(style, c);
    } else if (strcmp(key, "text_muted") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_text_muted(style, c);
    } else if (strcmp(key, "text_white") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_text_white(style, c);
    } else if (strcmp(key, "text_black") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_text_black(style, c);
    } else if (strcmp(key, "text_reset") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_text_reset(style, c);

    // Borders
    } else if (strcmp(key, "border_primary") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_border_primary(style, c);
    } else if (strcmp(key, "border_secondary") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_border_secondary(style, c);
    } else if (strcmp(key, "border_success") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_border_success(style, c);
    } else if (strcmp(key, "border_info") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_border_info(style, c);
    } else if (strcmp(key, "border_warning") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_border_warning(style, c);
    } else if (strcmp(key, "border_danger") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_border_danger(style, c);
    } else if (strcmp(key, "border_light") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_border_light(style, c);
    } else if (strcmp(key, "border_dark") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_border_dark(style, c);
    } else if (strcmp(key, "border_white") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_border_white(style, c);
    } else if (strcmp(key, "border_gray_200") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_border_gray_200(style, c);
    } else if (strcmp(key, "border_gray_300") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_border_gray_300(style, c);

    // Tooltips and misc
    } else if (strcmp(key, "tooltip_bg") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_tooltip_bg(style, c);
    } else if (strcmp(key, "tooltip_color") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_tooltip_color(style, c);
    } else if (strcmp(key, "selection_bg") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_selection_bg(style, c);
    } else if (strcmp(key, "selection_color") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_selection_color(style, c);
    } else if (strcmp(key, "focus_ring") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_focus_ring(style, c);
    } else if (strcmp(key, "highlight") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_highlight(style, c);
    } else if (strcmp(key, "list_group_action") == 0) { EseColor *c = ese_color_lua_get(L, 3); if (c) ese_gui_style_set_list_group_action(style, c);
    }

    profile_stop(PROFILE_LUA_GUI_STYLE_NEWINDEX, "gui_style_lua_newindex");
    return 0;
}

/**
 * @brief Lua __tostring metamethod for EseGuiStyle
 * 
 * Provides string representation of the style for debugging and display.
 * 
 * @param L Lua state
 * @return 1 (one return value - the string representation)
 */
static int _ese_gui_style_lua_tostring(lua_State *L) {
    EseGuiStyle *style = ese_gui_style_lua_get(L, 1);
    if (!style) {
        lua_pushstring(L, "GuiStyle: <invalid>");
        return 1;
    }

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "GuiStyle: direction=%d, justify=%d, align_items=%d, spacing=%d, font_size=%d", 
             ese_gui_style_get_direction(style),
             ese_gui_style_get_justify(style),
             ese_gui_style_get_align_items(style),
             ese_gui_style_get_spacing(style),
             ese_gui_style_get_font_size(style));
    
    lua_pushstring(L, buffer);
    return 1;
}

// Lua constructors
/**
 * @brief Lua constructor for creating new EseGuiStyle instances
 * 
 * Creates a new EseGuiStyle and pushes it to the Lua stack as a proxy table.
 * 
 * @param L Lua state
 * @return 1 (one return value - the new GuiStyle)
 */
static int _ese_gui_style_lua_new(lua_State *L) {
    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 0) {
        return luaL_error(L, "GuiStyle.new() takes 0 arguments");
    }
    
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);

    EseGuiStyle *style = ese_gui_style_create(engine);
    ese_gui_style_lua_push(style);
    return 1;
}

// Lua JSON methods
/**
 * @brief Lua method for converting EseGuiStyle to JSON
 * 
 * @param L Lua state
 * @return 1 (one return value - the JSON string)
 */
static int _ese_gui_style_lua_to_json(lua_State *L) {
    EseGuiStyle *style = (EseGuiStyle *)lua_touserdata(L, lua_upvalueindex(1));
    if (!style) {
        luaL_error(L, "Invalid GuiStyle");
        return 0;
    }

    cJSON *json = ese_gui_style_serialize(style);
    if (!json) {
        luaL_error(L, "Failed to serialize GuiStyle");
        return 0;
    }

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);
    
    if (!json_string) {
        luaL_error(L, "Failed to convert to JSON string");
        return 0;
    }

    lua_pushstring(L, json_string);
    free(json_string);
    return 1;
}

/**
 * @brief Lua method for creating EseGuiStyle from JSON
 * 
 * @param L Lua state
 * @return 1 (one return value - the new GuiStyle)
 */
static int _ese_gui_style_lua_from_json(lua_State *L) {
    if (lua_gettop(L) < 2 || !lua_isstring(L, 2)) {
        luaL_error(L, "Expected JSON string");
        return 0;
    }

    const char *json_string = lua_tostring(L, 2);
    cJSON *json = cJSON_Parse(json_string);
    if (!json) {
        luaL_error(L, "Invalid JSON");
        return 0;
    }

    // Get the engine from the registry
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    
    if (!engine) {
        cJSON_Delete(json);
        luaL_error(L, "Invalid engine");
        return 0;
    }

    EseGuiStyle *style = ese_gui_style_deserialize(engine, json);
    cJSON_Delete(json);
    
    if (!style) {
        luaL_error(L, "Failed to deserialize GuiStyle from JSON");
        return 0;
    }

    ese_gui_style_lua_push(style);
    return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

void _ese_gui_style_lua_init(EseLuaEngine *engine) {
    // Create metatable
    lua_engine_new_object_meta(engine, GUI_STYLE_PROXY_META, 
        _ese_gui_style_lua_index, 
        _ese_gui_style_lua_newindex, 
        _ese_gui_style_lua_gc, 
        _ese_gui_style_lua_tostring);
    
    // Create global GuiStyle table with functions
    const char *keys[] = {"new", "fromJSON"};
    lua_CFunction functions[] = {_ese_gui_style_lua_new, _ese_gui_style_lua_from_json};
    lua_engine_new_object(engine, "GuiStyle", 2, keys, functions);
}
