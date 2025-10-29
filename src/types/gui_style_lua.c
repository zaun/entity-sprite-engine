#include "types/gui_style_lua.h"
#include "core/memory_manager.h"
#include "graphics/gui/gui.h"
#include "scripting/lua_engine.h"
#include "types/color.h"
#include "types/gui_style.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Color property proxy metamethods
static int _ese_gui_style_color_proxy_index(lua_State *L);
static int _ese_gui_style_color_proxy_newindex(lua_State *L);

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
 * Handles cleanup when a Lua proxy table for an EseGuiStyle is garbage
 * collected. Only frees the underlying EseGuiStyle if it has no C-side
 * references.
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
        // If lua_ref != LUA_NOREF, this style was referenced from C and should
        // not be freed.
        if (ese_gui_style_get_lua_ref(style) == LUA_NOREF) {
            ese_gui_style_destroy(style);
        }
    }

    return 0;
}

/**
 * @brief Creates a color property proxy table for indexed access by variant
 *
 * @param L Lua state
 * @param style Pointer to the EseGuiStyle object
 * @param getter_func Pointer to the getter function
 * @param setter_func Pointer to the setter function
 */
static void _ese_gui_style_create_color_proxy(lua_State *L, EseGuiStyle *style,
                                              EseColor *(*getter_func)(const EseGuiStyle *,
                                                                       EseGuiStyleVariant),
                                              void (*setter_func)(EseGuiStyle *, EseGuiStyleVariant,
                                                                  const EseColor *)) {
    // Create a new table for this color property
    lua_newtable(L);

    // Create metatable for color property proxy
    lua_newtable(L);

    // Set __index metamethod
    lua_pushlightuserdata(L, style);
    lua_pushlightuserdata(L, (void *)getter_func);
    lua_pushcclosure(L, _ese_gui_style_color_proxy_index, 2);
    lua_setfield(L, -2, "__index");

    // Set __newindex metamethod
    lua_pushlightuserdata(L, style);
    lua_pushlightuserdata(L, (void *)setter_func);
    lua_pushcclosure(L, _ese_gui_style_color_proxy_newindex, 2);
    lua_setfield(L, -2, "__newindex");

    // Set the metatable on the table
    lua_setmetatable(L, -2);
}

/**
 * @brief __index metamethod for color property proxy
 *
 * @param L Lua state
 * @return 1 (the color value)
 */
static int _ese_gui_style_color_proxy_index(lua_State *L) {
    // Get style and getter from upvalues
    EseGuiStyle *style = (EseGuiStyle *)lua_touserdata(L, lua_upvalueindex(1));
    EseColor *(*getter)(const EseGuiStyle *, EseGuiStyleVariant) =
        (EseColor * (*)(const EseGuiStyle *, EseGuiStyleVariant))
            lua_touserdata(L, lua_upvalueindex(2));

    if (!style || !getter) {
        return luaL_error(L, "Invalid color property proxy");
    }

    // Get variant from index
    if (!lua_isnumber(L, 2)) {
        return luaL_error(L, "Color property index must be a variant number");
    }

    int variant = lua_tointeger(L, 2);
    if (variant < 0 || variant >= GUI_STYLE_VARIANT_MAX) {
        return luaL_error(L, "Invalid variant: %d", variant);
    }

    // Call getter and push result
    EseColor *color = getter(style, variant);
    if (color) {
        ese_color_lua_push(color);
        return 1;
    }

    return 0;
}

/**
 * @brief __newindex metamethod for color property proxy
 *
 * @param L Lua state
 * @return 0
 */
static int _ese_gui_style_color_proxy_newindex(lua_State *L) {
    // Get style and setter from upvalues
    EseGuiStyle *style = (EseGuiStyle *)lua_touserdata(L, lua_upvalueindex(1));
    void (*setter)(EseGuiStyle *, EseGuiStyleVariant, const EseColor *) = (void (*)(
        EseGuiStyle *, EseGuiStyleVariant, const EseColor *))lua_touserdata(L, lua_upvalueindex(2));

    if (!style || !setter) {
        return luaL_error(L, "Invalid color property proxy");
    }

    // Get variant from index
    if (!lua_isnumber(L, 2)) {
        return luaL_error(L, "Color property index must be a variant number");
    }

    int variant = lua_tointeger(L, 2);
    if (variant < 0 || variant >= GUI_STYLE_VARIANT_MAX) {
        return luaL_error(L, "Invalid variant: %d", variant);
    }

    // Get color value
    EseColor *color = ese_color_lua_get(L, 3);
    if (!color) {
        return luaL_error(L, "Value must be a Color");
    }

    // Call setter
    setter(style, variant, color);

    return 0;
}

/**
 * @brief Lua __index metamethod for EseGuiStyle property access
 *
 * Provides read access to style properties from Lua. When a Lua script
 * accesses style.property, this function is called to retrieve the values.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid properties, 0 for
 * invalid)
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
    if (strcmp(key, "border_width") == 0) {
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
    } else if (strcmp(key, "font_size") == 0) {
        lua_pushnumber(L, ese_gui_style_get_font_size(style));
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;

        // Theme/context colors - return indexable tables
    } else if (strcmp(key, "color") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_color,
                                          ese_gui_style_set_color);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "color_hover") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_color_hover,
                                          ese_gui_style_set_color_hover);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "color_active") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_color_active,
                                          ese_gui_style_set_color_active);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "alert_background") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_alert_bg,
                                          ese_gui_style_set_alert_bg);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "alert_text") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_alert_text,
                                          ese_gui_style_set_alert_text);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "alert_border") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_alert_border,
                                          ese_gui_style_set_alert_border);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "background") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_bg, ese_gui_style_set_bg);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "background_hover") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_bg_hover,
                                          ese_gui_style_set_bg_hover);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "background_active") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_bg_active,
                                          ese_gui_style_set_bg_active);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "text") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_text, ese_gui_style_set_text);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "text_hover") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_text_hover,
                                          ese_gui_style_set_text_hover);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "text_active") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_text_active,
                                          ese_gui_style_set_text_active);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "border") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_border,
                                          ese_gui_style_set_border);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "border_hover") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_border_hover,
                                          ese_gui_style_set_border_hover);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "border_active") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_border_active,
                                          ese_gui_style_set_border_active);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "tooltip_background") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_tooltip_bg,
                                          ese_gui_style_set_tooltip_bg);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "tooltip_color") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_tooltip_color,
                                          ese_gui_style_set_tooltip_color);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "selection_background") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_selection_bg,
                                          ese_gui_style_set_selection_bg);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "selection_color") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_selection_color,
                                          ese_gui_style_set_selection_color);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "focus_ring") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_focus_ring,
                                          ese_gui_style_set_focus_ring);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "highlight") == 0) {
        _ese_gui_style_create_color_proxy(L, style, ese_gui_style_get_highlight,
                                          ese_gui_style_set_highlight);
        profile_stop(PROFILE_LUA_GUI_STYLE_INDEX, "gui_style_lua_index (getter)");
        return 1;
    }

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

    // Border width
    if (strcmp(key, "border_width") == 0) {

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
    } else if (strcmp(key, "font_size") == 0) {
        if (lua_isnumber(L, 3)) {
            ese_gui_style_set_font_size(style, (int)lua_tointeger(L, 3));
        }

        // Color properties are handled by their proxy tables
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
    snprintf(buffer, sizeof(buffer), "GuiStyle: border_width=%d, font_size=%d",
             ese_gui_style_get_border_width(style), ese_gui_style_get_font_size(style));

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
    lua_State *L = engine->runtime;

    // Create metatable
    lua_engine_new_object_meta(engine, GUI_STYLE_PROXY_META, _ese_gui_style_lua_index,
                               _ese_gui_style_lua_newindex, _ese_gui_style_lua_gc,
                               _ese_gui_style_lua_tostring);

    // Create global GuiStyle table with functions
    const char *keys[] = {"new", "fromJSON"};
    lua_CFunction functions[] = {_ese_gui_style_lua_new, _ese_gui_style_lua_from_json};
    lua_engine_new_object(engine, "GuiStyle", 2, keys, functions);

    // Add VARIANT table to GuiStyle
    lua_getglobal(L, "GuiStyle");
    lua_newtable(L); // Create VARIANT table
    // GUI_STYLE_VARIANT_DEFAULT is not a valid variant, so we don't include it
    lua_pushinteger(L, GUI_STYLE_VARIANT_PRIMARY);
    lua_setfield(L, -2, "PRIMARY");
    lua_pushinteger(L, GUI_STYLE_VARIANT_SECONDARY);
    lua_setfield(L, -2, "SECONDARY");
    lua_pushinteger(L, GUI_STYLE_VARIANT_SUCCESS);
    lua_setfield(L, -2, "SUCCESS");
    lua_pushinteger(L, GUI_STYLE_VARIANT_INFO);
    lua_setfield(L, -2, "INFO");
    lua_pushinteger(L, GUI_STYLE_VARIANT_WARNING);
    lua_setfield(L, -2, "WARNING");
    lua_pushinteger(L, GUI_STYLE_VARIANT_DANGER);
    lua_setfield(L, -2, "DANGER");
    lua_pushinteger(L, GUI_STYLE_VARIANT_LIGHT);
    lua_setfield(L, -2, "LIGHT");
    lua_pushinteger(L, GUI_STYLE_VARIANT_DARK);
    lua_setfield(L, -2, "DARK");
    lua_pushinteger(L, GUI_STYLE_VARIANT_WHITE);
    lua_setfield(L, -2, "WHITE");
    lua_pushinteger(L, GUI_STYLE_VARIANT_TRANSPARENT);
    lua_setfield(L, -2, "TRANSPARENT");
    lua_setfield(L, -2, "VARIANT"); // Set GuiStyle.VARIANT = VARIANT table
    lua_pop(L, 1);                  // Pop GuiStyle table
}
