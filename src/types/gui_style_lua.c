#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/gui_style.h"
#include "types/color.h"
#include "graphics/gui.h"
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
    } else if (strcmp(key, "background") == 0) {
        EseColor *color = ese_gui_style_get_background(style);
        ese_color_lua_push(color);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "background_hovered") == 0) {
        EseColor *color = ese_gui_style_get_background_hovered(style);
        ese_color_lua_push(color);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "background_pressed") == 0) {
        EseColor *color = ese_gui_style_get_background_pressed(style);
        ese_color_lua_push(color);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "border") == 0) {
        EseColor *color = ese_gui_style_get_border(style);
        ese_color_lua_push(color);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "border_hovered") == 0) {
        EseColor *color = ese_gui_style_get_border_hovered(style);
        ese_color_lua_push(color);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "border_pressed") == 0) {
        EseColor *color = ese_gui_style_get_border_pressed(style);
        ese_color_lua_push(color);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "text") == 0) {
        EseColor *color = ese_gui_style_get_text(style);
        ese_color_lua_push(color);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "text_hovered") == 0) {
        EseColor *color = ese_gui_style_get_text_hovered(style);
        ese_color_lua_push(color);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "text_pressed") == 0) {
        EseColor *color = ese_gui_style_get_text_pressed(style);
        ese_color_lua_push(color);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "toJSON") == 0) {
        lua_pushlightuserdata(L, style);
        lua_pushcclosure(L, _ese_gui_style_lua_to_json, 1);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (method)");
        return 1;
    } else if (strcmp(key, "fromJSON") == 0) {
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
    } else if (strcmp(key, "background") == 0) {
        EseColor *color = ese_color_lua_get(L, 3);
        if (color) {
            ese_gui_style_set_background(style, color);
        }
    } else if (strcmp(key, "background_hovered") == 0) {
        EseColor *color = ese_color_lua_get(L, 3);
        if (color) {
            ese_gui_style_set_background_hovered(style, color);
        }
    } else if (strcmp(key, "background_pressed") == 0) {
        EseColor *color = ese_color_lua_get(L, 3);
        if (color) {
            ese_gui_style_set_background_pressed(style, color);
        }
    } else if (strcmp(key, "border") == 0) {
        EseColor *color = ese_color_lua_get(L, 3);
        if (color) {
            ese_gui_style_set_border(style, color);
        }
    } else if (strcmp(key, "border_hovered") == 0) {
        EseColor *color = ese_color_lua_get(L, 3);
        if (color) {
            ese_gui_style_set_border_hovered(style, color);
        }
    } else if (strcmp(key, "border_pressed") == 0) {
        EseColor *color = ese_color_lua_get(L, 3);
        if (color) {
            ese_gui_style_set_border_pressed(style, color);
        }
    } else if (strcmp(key, "text") == 0) {
        EseColor *color = ese_color_lua_get(L, 3);
        if (color) {
            ese_gui_style_set_text(style, color);
        }
    } else if (strcmp(key, "text_hovered") == 0) {
        EseColor *color = ese_color_lua_get(L, 3);
        if (color) {
            ese_gui_style_set_text_hovered(style, color);
        }
    } else if (strcmp(key, "text_pressed") == 0) {
        EseColor *color = ese_color_lua_get(L, 3);
        if (color) {
            ese_gui_style_set_text_pressed(style, color);
        }
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
    snprintf(buffer, sizeof(buffer), "GuiStyle: direction=%d, justify=%d, align_items=%d, spacing=%d", 
             ese_gui_style_get_direction(style),
             ese_gui_style_get_justify(style),
             ese_gui_style_get_align_items(style),
             ese_gui_style_get_spacing(style));
    
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

    // Get the engine from the global registry
    lua_getglobal(L, "Engine");
    if (!lua_isuserdata(L, -1)) {
        cJSON_Delete(json);
        luaL_error(L, "Engine not found in global scope");
        return 0;
    }
    
    EseLuaEngine *engine = (EseLuaEngine *)lua_touserdata(L, -1);
    lua_pop(L, 1); // Remove engine from stack
    
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
