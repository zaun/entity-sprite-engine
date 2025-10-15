#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/types.h"
#include "types/map_cell.h"
#include "types/map_cell_lua.h"

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Lua metamethods
static int _ese_map_cell_lua_gc(lua_State *L);
static int _ese_map_cell_lua_index(lua_State *L);
static int _ese_map_cell_lua_newindex(lua_State *L);
static int _ese_map_cell_lua_tostring(lua_State *L);

// Lua methods
static int _ese_map_cell_lua_add_layer(lua_State *L);
static int _ese_map_cell_lua_remove_layer(lua_State *L);
static int _ese_map_cell_lua_get_layer(lua_State *L);
static int _ese_map_cell_lua_set_layer(lua_State *L);
static int _ese_map_cell_lua_clear_layers(lua_State *L);
static int _ese_map_cell_lua_has_flag(lua_State *L);
static int _ese_map_cell_lua_set_flag(lua_State *L);
static int _ese_map_cell_lua_clear_flag(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseMapCell
 * 
 * Handles cleanup when a Lua proxy table for an EseMapCell is garbage collected.
 * Only frees the underlying EseMapCell if it has no C-side references.
 * 
 * @param L Lua state
 * @return 0 (no return values)
 */
static int _ese_map_cell_lua_gc(lua_State *L) {
    // Get from userdata
    EseMapCell **ud = (EseMapCell **)luaL_testudata(L, 1, MAP_CELL_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseMapCell *cell = *ud;
    if (cell) {
        // If lua_ref == LUA_NOREF, there are no more references to this cell, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this cell was referenced from C and should not be freed.
        if (ese_map_cell_get_lua_ref(cell) == LUA_NOREF) {
            ese_map_cell_destroy(cell);
        }
    }

    return 0;
}

/**
 * @brief Lua __index metamethod for EseMapCell
 * 
 * Handles property access on EseMapCell objects from Lua.
 * Supports accessing isDynamic, flags, layer_count properties and methods.
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_map_cell_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_map_cell_INDEX);
    EseMapCell *cell = ese_map_cell_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!cell || !key) {
        profile_cancel(PROFILE_LUA_map_cell_INDEX);
        return 0;
    }

    if (strcmp(key, "isDynamic") == 0) {
        lua_pushboolean(L, ese_map_cell_get_is_dynamic(cell));
        profile_stop(PROFILE_LUA_map_cell_INDEX, "mapcell_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "flags") == 0) {
        lua_pushnumber(L, ese_map_cell_get_flags(cell));
        profile_stop(PROFILE_LUA_map_cell_INDEX, "mapcell_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "layer_count") == 0) {
        lua_pushnumber(L, ese_map_cell_get_layer_count(cell));
        profile_stop(PROFILE_LUA_map_cell_INDEX, "mapcell_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "add_layer") == 0) {
        lua_pushcfunction(L, _ese_map_cell_lua_add_layer);
        profile_stop(PROFILE_LUA_map_cell_INDEX, "mapcell_lua_index (method)");
        return 1;
    } else if (strcmp(key, "remove_layer") == 0) {
        lua_pushcfunction(L, _ese_map_cell_lua_remove_layer);
        profile_stop(PROFILE_LUA_map_cell_INDEX, "mapcell_lua_index (method)");
        return 1;
    } else if (strcmp(key, "get_layer") == 0) {
        lua_pushcfunction(L, _ese_map_cell_lua_get_layer);
        profile_stop(PROFILE_LUA_map_cell_INDEX, "mapcell_lua_index (method)");
        return 1;
    } else if (strcmp(key, "set_layer") == 0) {
        lua_pushcfunction(L, _ese_map_cell_lua_set_layer);
        profile_stop(PROFILE_LUA_map_cell_INDEX, "mapcell_lua_index (method)");
        return 1;
    } else if (strcmp(key, "clear_layers") == 0) {
        lua_pushcfunction(L, _ese_map_cell_lua_clear_layers);
        profile_stop(PROFILE_LUA_map_cell_INDEX, "mapcell_lua_index (method)");
        return 1;
    } else if (strcmp(key, "has_flag") == 0) {
        lua_pushcfunction(L, _ese_map_cell_lua_has_flag);
        profile_stop(PROFILE_LUA_map_cell_INDEX, "mapcell_lua_index (method)");
        return 1;
    } else if (strcmp(key, "set_flag") == 0) {
        lua_pushcfunction(L, _ese_map_cell_lua_set_flag);
        profile_stop(PROFILE_LUA_map_cell_INDEX, "mapcell_lua_index (method)");
        return 1;
    } else if (strcmp(key, "clear_flag") == 0) {
        lua_pushcfunction(L, _ese_map_cell_lua_clear_flag);
        profile_stop(PROFILE_LUA_map_cell_INDEX, "mapcell_lua_index (method)");
        return 1;
    }
    profile_stop(PROFILE_LUA_map_cell_INDEX, "mapcell_lua_index (invalid)");
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseMapCell
 * 
 * Handles property assignment on EseMapCell objects from Lua.
 * Supports setting isDynamic and flags properties.
 * 
 * @param L Lua state
 * @return 0 (no return values)
 */
static int _ese_map_cell_lua_newindex(lua_State *L) {
    profile_start(PROFILE_LUA_map_cell_NEWINDEX);
    EseMapCell *cell = ese_map_cell_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!cell || !key) {
        profile_cancel(PROFILE_LUA_map_cell_NEWINDEX);
        return 0;
    }

    if (strcmp(key, "isDynamic") == 0) {
        if (lua_type(L, 3) != LUA_TBOOLEAN) {
            profile_cancel(PROFILE_LUA_map_cell_NEWINDEX);
            return luaL_error(L, "mapcell.isDynamic must be a boolean");
        }
        ese_map_cell_set_is_dynamic(cell, lua_toboolean(L, 3));
        profile_stop(PROFILE_LUA_map_cell_NEWINDEX, "mapcell_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "flags") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_map_cell_NEWINDEX);
            return luaL_error(L, "mapcell.flags must be a number");
        }
        ese_map_cell_set_flags(cell, (uint32_t)lua_tonumber(L, 3));
        profile_stop(PROFILE_LUA_map_cell_NEWINDEX, "mapcell_lua_newindex (setter)");
        return 0;
    }
    profile_stop(PROFILE_LUA_map_cell_NEWINDEX, "mapcell_lua_newindex (invalid)");
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __tostring metamethod for EseMapCell
 * 
 * Converts an EseMapCell to a string representation for debugging.
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_map_cell_lua_tostring(lua_State *L) {
    EseMapCell *cell = ese_map_cell_lua_get(L, 1);

    if (!cell) {
        lua_pushstring(L, "MapCell: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "MapCell: %p (layers=%zu, flags=%u, dynamic=%d)",
             (void*)cell, ese_map_cell_get_layer_count(cell), ese_map_cell_get_flags(cell), ese_map_cell_get_is_dynamic(cell));
    lua_pushstring(L, buf);

    return 1;
}

// Lua method implementations
/**
 * @brief Lua method to add a tile layer to the map cell
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_map_cell_lua_add_layer(lua_State *L) {
    EseMapCell *cell = ese_map_cell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in add_layer");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "add_layer(tile_id) requires a number");

    int tile_id = (int)lua_tonumber(L, 2);
    if (tile_id < -1 || tile_id > 255) {
        return luaL_error(L, "add_layer(tile_id) requires a number >= -1 and <= 255");
    }

    lua_pushboolean(L, ese_map_cell_add_layer(cell, tile_id));
    return 1;
}

/**
 * @brief Lua method to remove a tile layer from the map cell
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_map_cell_lua_remove_layer(lua_State *L) {
    EseMapCell *cell = ese_map_cell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in remove_layer");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "remove_layer(index) requires a number");

    size_t idx = (size_t)lua_tonumber(L, 2);
    lua_pushboolean(L, ese_map_cell_remove_layer(cell, idx));
    return 1;
}

/**
 * @brief Lua method to get a tile ID from a specific layer
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_map_cell_lua_get_layer(lua_State *L) {
    EseMapCell *cell = ese_map_cell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in get_layer");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "get_layer(index) requires a number");

    size_t idx = (size_t)lua_tonumber(L, 2);
    lua_pushnumber(L, ese_map_cell_get_layer(cell, idx));
    return 1;
}

/**
 * @brief Lua method to set a tile ID for a specific layer
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_map_cell_lua_set_layer(lua_State *L) {
    EseMapCell *cell = ese_map_cell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in set_layer");

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3))
        return luaL_error(L, "set_layer(index, tile_id) requires two numbers");

    size_t idx = (size_t)lua_tonumber(L, 2);
    uint8_t tile_id = (uint8_t)lua_tonumber(L, 3);
    lua_pushboolean(L, ese_map_cell_set_layer(cell, idx, tile_id));
    return 1;
}

/**
 * @brief Lua method to clear all layers from the map cell
 * 
 * @param L Lua state
 * @return 0 (no return values)
 */
static int _ese_map_cell_lua_clear_layers(lua_State *L) {
    EseMapCell *cell = ese_map_cell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in clear_layers");

    ese_map_cell_clear_layers(cell);
    return 0;
}

/**
 * @brief Lua method to check if a specific flag is set
 * 
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_map_cell_lua_has_flag(lua_State *L) {
    EseMapCell *cell = ese_map_cell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in has_flag");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "has_flag(flag) requires a number");

    uint32_t flag = (uint32_t)lua_tonumber(L, 2);
    lua_pushboolean(L, ese_map_cell_has_flag(cell, flag));
    return 1;
}

/**
 * @brief Lua method to set a specific flag
 * 
 * @param L Lua state
 * @return 0 (no return values)
 */
static int _ese_map_cell_lua_set_flag(lua_State *L) {
    EseMapCell *cell = ese_map_cell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in set_flag");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "set_flag(flag) requires a number");

    uint32_t flag = (uint32_t)lua_tonumber(L, 2);
    ese_map_cell_set_flag(cell, flag);
    return 0;
}

/**
 * @brief Lua method to clear a specific flag
 * 
 * @param L Lua state
 * @return 0 (no return values)
 */
static int _ese_map_cell_lua_clear_flag(lua_State *L) {
    EseMapCell *cell = ese_map_cell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in clear_flag");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "clear_flag(flag) requires a number");

    uint32_t flag = (uint32_t)lua_tonumber(L, 2);
    ese_map_cell_clear_flag(cell, flag);
    return 0;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Internal Lua initialization function for EseMapCell
 * 
 * Sets up the Lua metatable and global MapCell table with constructors and methods.
 * This function is called by the public ese_map_cell_lua_init function.
 * 
 * @param engine EseLuaEngine pointer where the EseMapCell type will be registered
 */
void _ese_map_cell_lua_init(EseLuaEngine *engine) {
    // Create metatable
    lua_engine_new_object_meta(engine, MAP_CELL_PROXY_META, 
        _ese_map_cell_lua_index, 
        _ese_map_cell_lua_newindex, 
        _ese_map_cell_lua_gc, 
        _ese_map_cell_lua_tostring);
}
