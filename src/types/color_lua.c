#include <string.h>
#include <stdio.h>
#include <math.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/types.h"
#include "types/color.h"
#include "types/color_lua.h"
#include "vendor/json/cJSON.h"

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Lua metamethods
static int _ese_color_lua_gc(lua_State *L);
static int _ese_color_lua_index(lua_State *L);
static int _ese_color_lua_newindex(lua_State *L);
static int _ese_color_lua_tostring(lua_State *L);

// Lua constructors
static int _ese_color_lua_new(lua_State *L);
static int _ese_color_lua_white(lua_State *L);
static int _ese_color_lua_black(lua_State *L);
static int _ese_color_lua_red(lua_State *L);
static int _ese_color_lua_green(lua_State *L);
static int _ese_color_lua_blue(lua_State *L);

// Lua methods
static int _ese_color_lua_set_hex(lua_State *L);
static int _ese_color_lua_set_byte(lua_State *L);
static int _ese_color_lua_to_json(lua_State *L);
static int _ese_color_lua_from_json(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseColor
 * 
 * Handles cleanup when a Lua proxy table for an EseColor is garbage collected.
 * Only frees the underlying EseColor if it has no C-side references.
 * 
 * @param L Lua state
 * @return 0 (no return values)
 */
static int _ese_color_lua_gc(lua_State *L) {
    EseColor **ud = (EseColor **)luaL_checkudata(L, 1, COLOR_META);
    if (ud && *ud) {
        EseColor *color = *ud;
        
        // Only destroy if no C-side references
        if (ese_color_get_lua_ref_count(color) == 0) {
            ese_color_destroy(color);
        }
    }
    
    return 0;
}

/**
 * @brief Lua __index metamethod for EseColor
 * 
 * Handles property access on EseColor objects from Lua.
 * Supports accessing r, g, b, a properties and methods.
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_color_lua_index(lua_State *L) {
    EseColor **ud = (EseColor **)luaL_checkudata(L, 1, COLOR_META);
    EseColor *color = *ud;
    const char *key = luaL_checkstring(L, 2);
    
    if (strcmp(key, "r") == 0) {
        lua_pushnumber(L, ese_color_get_r(color));
    } else if (strcmp(key, "g") == 0) {
        lua_pushnumber(L, ese_color_get_g(color));
    } else if (strcmp(key, "b") == 0) {
        lua_pushnumber(L, ese_color_get_b(color));
    } else if (strcmp(key, "a") == 0) {
        lua_pushnumber(L, ese_color_get_a(color));
    } else if (strcmp(key, "set_hex") == 0) {
        lua_pushcfunction(L, _ese_color_lua_set_hex);
    } else if (strcmp(key, "set_byte") == 0) {
        lua_pushcfunction(L, _ese_color_lua_set_byte);
    } else if (strcmp(key, "to_json") == 0) {
        lua_pushcfunction(L, _ese_color_lua_to_json);
    } else if (strcmp(key, "from_json") == 0) {
        lua_pushcfunction(L, _ese_color_lua_from_json);
    } else {
        lua_pushnil(L);
    }
    
    return 1;
}

/**
 * @brief Lua __newindex metamethod for EseColor
 * 
 * Handles property assignment on EseColor objects from Lua.
 * Supports setting r, g, b, a properties.
 * 
 * @param L Lua state
 * @return 0 (no return values)
 */
static int _ese_color_lua_newindex(lua_State *L) {
    EseColor **ud = (EseColor **)luaL_checkudata(L, 1, COLOR_META);
    EseColor *color = *ud;
    const char *key = luaL_checkstring(L, 2);
    float value = (float)luaL_checknumber(L, 3);
    
    if (strcmp(key, "r") == 0) {
        ese_color_set_r(color, value);
    } else if (strcmp(key, "g") == 0) {
        ese_color_set_g(color, value);
    } else if (strcmp(key, "b") == 0) {
        ese_color_set_b(color, value);
    } else if (strcmp(key, "a") == 0) {
        ese_color_set_a(color, value);
    } else {
        luaL_error(L, "Cannot set property '%s' on EseColor", key);
    }
    
    return 0;
}

/**
 * @brief Lua __tostring metamethod for EseColor
 * 
 * Converts an EseColor to a string representation for debugging.
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_color_lua_tostring(lua_State *L) {
    EseColor **ud = (EseColor **)luaL_checkudata(L, 1, COLOR_META);
    EseColor *color = *ud;
    
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "EseColor(%.3f, %.3f, %.3f, %.3f)", 
             ese_color_get_r(color), ese_color_get_g(color), 
             ese_color_get_b(color), ese_color_get_a(color));
    
    lua_pushstring(L, buffer);
    return 1;
}

// Lua constructors
/**
 * @brief Lua constructor for EseColor
 * 
 * Creates a new EseColor with optional r, g, b, a parameters.
 * Usage: Color.new() or Color.new(r, g, b, a)
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_color_lua_new(lua_State *L) {
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    EseColor *color = ese_color_create(engine);
    
    int argc = lua_gettop(L);
    if (argc >= 1) {
        float r = (float)luaL_checknumber(L, 1);
        ese_color_set_r(color, r);
    }
    if (argc >= 2) {
        float g = (float)luaL_checknumber(L, 2);
        ese_color_set_g(color, g);
    }
    if (argc >= 3) {
        float b = (float)luaL_checknumber(L, 3);
        ese_color_set_b(color, b);
    }
    if (argc >= 4) {
        float a = (float)luaL_checknumber(L, 4);
        ese_color_set_a(color, a);
    }
    
    ese_color_lua_push(color);
    return 1;
}

/**
 * @brief Lua constructor for white EseColor
 * 
 * Creates a new white EseColor (1, 1, 1, 1).
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_color_lua_white(lua_State *L) {
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    EseColor *color = ese_color_create(engine);
    ese_color_set_r(color, 1.0f);
    ese_color_set_g(color, 1.0f);
    ese_color_set_b(color, 1.0f);
    ese_color_set_a(color, 1.0f);
    
    ese_color_lua_push(color);
    return 1;
}

/**
 * @brief Lua constructor for black EseColor
 * 
 * Creates a new black EseColor (0, 0, 0, 1).
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_color_lua_black(lua_State *L) {
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    EseColor *color = ese_color_create(engine);
    ese_color_set_r(color, 0.0f);
    ese_color_set_g(color, 0.0f);
    ese_color_set_b(color, 0.0f);
    ese_color_set_a(color, 1.0f);
    
    ese_color_lua_push(color);
    return 1;
}

/**
 * @brief Lua constructor for red EseColor
 * 
 * Creates a new red EseColor (1, 0, 0, 1).
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_color_lua_red(lua_State *L) {
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    EseColor *color = ese_color_create(engine);
    ese_color_set_r(color, 1.0f);
    ese_color_set_g(color, 0.0f);
    ese_color_set_b(color, 0.0f);
    ese_color_set_a(color, 1.0f);
    
    ese_color_lua_push(color);
    return 1;
}

/**
 * @brief Lua constructor for green EseColor
 * 
 * Creates a new green EseColor (0, 1, 0, 1).
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_color_lua_green(lua_State *L) {
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    EseColor *color = ese_color_create(engine);
    ese_color_set_r(color, 0.0f);
    ese_color_set_g(color, 1.0f);
    ese_color_set_b(color, 0.0f);
    ese_color_set_a(color, 1.0f);
    
    ese_color_lua_push(color);
    return 1;
}

/**
 * @brief Lua constructor for blue EseColor
 * 
 * Creates a new blue EseColor (0, 0, 1, 1).
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_color_lua_blue(lua_State *L) {
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    EseColor *color = ese_color_create(engine);
    ese_color_set_r(color, 0.0f);
    ese_color_set_g(color, 0.0f);
    ese_color_set_b(color, 1.0f);
    ese_color_set_a(color, 1.0f);
    
    ese_color_lua_push(color);
    return 1;
}

// Lua methods
/**
 * @brief Lua method to set color from hex string
 * 
 * Sets the color from a hex string like "#FF0000" or "#FF0000FF".
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_color_lua_set_hex(lua_State *L) {
    EseColor **ud = (EseColor **)luaL_checkudata(L, 1, COLOR_META);
    EseColor *color = *ud;
    const char *hex_string = luaL_checkstring(L, 2);
    
    bool success = ese_color_set_hex(color, hex_string);
    lua_pushboolean(L, success);
    return 1;
}

/**
 * @brief Lua method to set color from byte values
 * 
 * Sets the color from byte values (0-255) which are converted to float (0-1).
 * 
 * @param L Lua state
 * @return 0 (no return values)
 */
static int _ese_color_lua_set_byte(lua_State *L) {
    EseColor **ud = (EseColor **)luaL_checkudata(L, 1, COLOR_META);
    EseColor *color = *ud;
    int r = (int)luaL_checkinteger(L, 2);
    int g = (int)luaL_checkinteger(L, 3);
    int b = (int)luaL_checkinteger(L, 4);
    int a = (int)luaL_optinteger(L, 5, 255);
    
    ese_color_set_r(color, (float)r / 255.0f);
    ese_color_set_g(color, (float)g / 255.0f);
    ese_color_set_b(color, (float)b / 255.0f);
    ese_color_set_a(color, (float)a / 255.0f);
    
    return 0;
}

/**
 * @brief Lua method to convert color to JSON
 * 
 * Converts the color to a JSON string representation.
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_color_lua_to_json(lua_State *L) {
    EseColor **ud = (EseColor **)luaL_checkudata(L, 1, COLOR_META);
    EseColor *color = *ud;
    
    cJSON *json = ese_color_serialize(color);
    char *json_string = cJSON_Print(json);
    lua_pushstring(L, json_string);
    free(json_string);
    cJSON_Delete(json);
    
    return 1;
}

/**
 * @brief Lua method to create color from JSON
 * 
 * Creates a new color from a JSON string representation.
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_color_lua_from_json(lua_State *L) {
    const char *json_string = luaL_checkstring(L, 1);
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    
    cJSON *json = cJSON_Parse(json_string);
    if (!json) {
        luaL_error(L, "Color.fromJSON: invalid JSON string");
        return 0;
    }
    
    EseColor *color = ese_color_deserialize(engine, json);
    cJSON_Delete(json);
    
    if (color) {
        ese_color_lua_push(color);
        return 1;
    } else {
        lua_pushnil(L);
        return 1;
    }
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Internal Lua initialization function for EseColor
 * 
 * Sets up the Lua metatable and global Color table with constructors and methods.
 * This function is called by the public ese_color_lua_init function.
 * 
 * @param engine EseLuaEngine pointer where the EseColor type will be registered
 */
void _ese_color_lua_init(EseLuaEngine *engine) {
    // Create metatable
    lua_engine_new_object_meta(engine, COLOR_META, 
        _ese_color_lua_index, 
        _ese_color_lua_newindex, 
        _ese_color_lua_gc, 
        _ese_color_lua_tostring);
    
    // Create global Color table with functions
    const char *keys[] = {"new", "white", "black", "red", "green", "blue", "set_hex", "set_byte", "to_json", "from_json"};
    lua_CFunction functions[] = {_ese_color_lua_new, _ese_color_lua_white, _ese_color_lua_black, _ese_color_lua_red, _ese_color_lua_green, _ese_color_lua_blue, _ese_color_lua_set_hex, _ese_color_lua_set_byte, _ese_color_lua_to_json, _ese_color_lua_from_json};
    lua_engine_new_object(engine, "Color", 10, keys, functions);
}
