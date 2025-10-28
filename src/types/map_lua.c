#include "types/map_lua.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/map.h"
#include "types/map_private.h"
#include "types/tileset.h"
#include "types/types.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <stdio.h>
#include <string.h>

// Forward declarations for private functions
extern EseMap *_ese_map_make(uint32_t width, uint32_t height, EseMapType type);
extern bool _allocate_cells_array(EseMap *map);
extern void _ese_map_notify_watchers(EseMap *map);

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Lua metamethods
static int _ese_map_lua_gc(lua_State *L);
static int _ese_map_lua_index(lua_State *L);
static int _ese_map_lua_newindex(lua_State *L);
static int _ese_map_lua_tostring(lua_State *L);

// Lua methods
static int _ese_map_lua_get_cell(lua_State *L);
static int _ese_map_lua_resize(lua_State *L);
static int _ese_map_lua_set_tileset(lua_State *L);

// Lua constructors
static int _ese_map_lua_new(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseMap
 *
 * Handles cleanup when a Lua proxy table for an EseMap is garbage collected.
 * Only frees the underlying EseMap if it has no C-side references.
 *
 * @param L Lua state
 * @return 0 (no return values)
 */
static int _ese_map_lua_gc(lua_State *L) {
  // Get from userdata
  EseMap **ud = (EseMap **)luaL_testudata(L, 1, MAP_PROXY_META);
  if (!ud) {
    return 0; // Not our userdata
  }

  EseMap *map = *ud;
  if (map && !map->destroyed) {
    // If lua_ref == LUA_NOREF, there are no more references to this map,
    // so we can free it.
    // If lua_ref != LUA_NOREF, this map was referenced from C and should not be
    // freed.
    if (ese_map_get_lua_ref(map) == LUA_NOREF) {
      ese_map_destroy(map);
    }
  }

  return 0;
}

/**
 * @brief Lua __index metamethod for EseMap
 *
 * Handles property access on EseMap objects from Lua.
 * Supports accessing title, author, version, type, width, height, tileset
 * properties and methods.
 *
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_map_lua_index(lua_State *L) {
  EseMap *map = ese_map_lua_get(L, 1);
  const char *key = lua_tostring(L, 2);
  if (!map || !key)
    return 0;

  if (strcmp(key, "title") == 0) {
    lua_pushstring(L, ese_map_get_title(map) ? ese_map_get_title(map) : "");
    return 1;
  } else if (strcmp(key, "author") == 0) {
    lua_pushstring(L, ese_map_get_author(map) ? ese_map_get_author(map) : "");
    return 1;
  } else if (strcmp(key, "version") == 0) {
    lua_pushnumber(L, ese_map_get_version(map));
    return 1;
  } else if (strcmp(key, "type") == 0) {
    lua_pushstring(L, ese_map_type_to_string(ese_map_get_type(map)));
    return 1;
  } else if (strcmp(key, "width") == 0) {
    lua_pushnumber(L, ese_map_get_width(map));
    return 1;
  } else if (strcmp(key, "height") == 0) {
    lua_pushnumber(L, ese_map_get_height(map));
    return 1;
  } else if (strcmp(key, "tileset") == 0) {
    if (ese_map_get_tileset(map)) {
      ese_tileset_lua_push(ese_map_get_tileset(map));
    } else {
      lua_pushnil(L);
    }
    return 1;
  } else if (strcmp(key, "get_cell") == 0) {
    lua_pushcfunction(L, _ese_map_lua_get_cell);
    return 1;
  } else if (strcmp(key, "resize") == 0) {
    lua_pushcfunction(L, _ese_map_lua_resize);
    return 1;
  } else if (strcmp(key, "set_tileset") == 0) {
    lua_pushcfunction(L, _ese_map_lua_set_tileset);
    return 1;
  }

  return 0;
}

/**
 * @brief Lua __newindex metamethod for EseMap
 *
 * Handles property assignment on EseMap objects from Lua.
 * Supports setting title, author, version, and type properties.
 *
 * @param L Lua state
 * @return 0 (no return values)
 */
static int _ese_map_lua_newindex(lua_State *L) {
  EseMap *map = ese_map_lua_get(L, 1);
  const char *key = lua_tostring(L, 2);
  if (!map || !key)
    return 0;

  if (strcmp(key, "title") == 0) {
    ese_map_set_title(map, lua_tostring(L, 3));
    return 0;
  } else if (strcmp(key, "author") == 0) {
    ese_map_set_author(map, lua_tostring(L, 3));
    return 0;
  } else if (strcmp(key, "version") == 0) {
    ese_map_set_version(map, (int)lua_tonumber(L, 3));
    return 0;
  } else if (strcmp(key, "type") == 0) {
    const char *type_str = lua_tostring(L, 3);
    if (type_str) {
      ese_map_set_type(map, ese_map_type_from_string(type_str));
      _ese_map_notify_watchers(map);
    }
    return 0;
  }

  return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __tostring metamethod for EseMap
 *
 * Converts an EseMap to a string representation for debugging.
 *
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_map_lua_tostring(lua_State *L) {
  EseMap *map = ese_map_lua_get(L, 1);
  if (!map) {
    lua_pushstring(L, "Map: (invalid)");
    return 1;
  }

  char buf[160];
  snprintf(buf, sizeof(buf),
           "Map: %p (title=%s, width=%zu, height=%zu, type=%s)", (void *)map,
           ese_map_get_title(map) ? ese_map_get_title(map) : "(null)",
           ese_map_get_width(map), ese_map_get_height(map),
           ese_map_type_to_string(ese_map_get_type(map)));
  lua_pushstring(L, buf);
  return 1;
}

// Lua methods
/**
 * @brief Lua method to get a cell at specific coordinates
 *
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_map_lua_get_cell(lua_State *L) {
  EseMap *map = ese_map_lua_get(L, 1);
  if (!map)
    return luaL_error(L, "Invalid Map in get_cell");

  if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3))
    return luaL_error(L, "get_cell(x, y) requires two numbers");

  uint32_t x = (uint32_t)lua_tonumber(L, 2);
  uint32_t y = (uint32_t)lua_tonumber(L, 3);

  if (x >= ese_map_get_width(map) || y >= ese_map_get_height(map)) {
    lua_pushnil(L);
    return 1;
  }

  EseMapCell *cell = ese_map_get_cell(map, x, y);
  if (!cell) {
    lua_pushnil(L);
    return 1;
  }

  // Check if the cell has a valid state pointer for Lua operations
  if (!ese_map_cell_get_state(cell)) {
    lua_pushnil(L);
    return 1;
  }

  ese_map_cell_lua_push(cell);
  return 1;
}

/**
 * @brief Lua method to resize the map
 *
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_map_lua_resize(lua_State *L) {
  EseMap *map = ese_map_lua_get(L, 1);
  if (!map)
    return luaL_error(L, "Invalid Map in resize");

  if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3))
    return luaL_error(L, "resize(width, height) requires two numbers");

  uint32_t new_width = (uint32_t)lua_tonumber(L, 2);
  uint32_t new_height = (uint32_t)lua_tonumber(L, 3);

  lua_pushboolean(L, ese_map_resize(map, new_width, new_height));
  return 1;
}

/**
 * @brief Lua method to set the tileset
 *
 * @param L Lua state
 * @return 0 (no return values)
 */
static int _ese_map_lua_set_tileset(lua_State *L) {
  EseMap *map = ese_map_lua_get(L, 1);
  if (!map)
    return luaL_error(L, "Invalid Map in set_tileset");

  EseTileSet *tileset = ese_tileset_lua_get(L, 2);
  if (!tileset)
    return luaL_error(L, "set_tileset requires a valid Tileset");

  ese_map_set_tileset(map, tileset);
  return 0;
}

// Lua constructors
/**
 * @brief Lua constructor for EseMap
 *
 * Creates a new EseMap with width, height, and optional type parameters.
 * Usage: Map.new(width, height, [type])
 *
 * @param L Lua state
 * @return 1 (one return value)
 */
static int _ese_map_lua_new(lua_State *L) {
  profile_start(PROFILE_LUA_MAP_NEW);

  if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
    profile_cancel(PROFILE_LUA_MAP_NEW);
    return luaL_error(
        L, "Map.new(width, height, [type]) requires at least two numbers");
  }

  size_t width = (size_t)lua_tonumber(L, 1);
  size_t height = (size_t)lua_tonumber(L, 2);
  EseMapType type = MAP_TYPE_GRID;

  if (width == 0 || height == 0) {
    profile_cancel(PROFILE_LUA_MAP_NEW);
    return luaL_error(L, "Map.new(width, height, [type]) width and height must "
                         "be greater than 0");
  }

  if (lua_isstring(L, 3)) {
    type = ese_map_type_from_string(lua_tostring(L, 3));
  }

  EseLuaEngine *engine =
      (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
  EseMap *map = _ese_map_make(width, height, type);
  ese_map_set_engine(map, engine);
  ese_map_set_state(map, engine->runtime);

  // Allocate cells after setting the correct state
  _allocate_cells_array(map);

  // Create userdata directly
  EseMap **ud = (EseMap **)lua_newuserdata(L, sizeof(EseMap *));
  *ud = map;

  // Attach metatable
  luaL_getmetatable(L, MAP_PROXY_META);
  lua_setmetatable(L, -2);

  profile_stop(PROFILE_LUA_MAP_NEW, "ese_map_lua_new");
  return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Internal Lua initialization function for EseMap
 *
 * Sets up the Lua metatable and global Map table with constructors and methods.
 * This function is called by the public ese_map_lua_init function.
 *
 * @param engine EseLuaEngine pointer where the EseMap type will be registered
 */
void _ese_map_lua_init(EseLuaEngine *engine) {
  // Create metatable
  lua_engine_new_object_meta(engine, MAP_PROXY_META, _ese_map_lua_index,
                             _ese_map_lua_newindex, _ese_map_lua_gc,
                             _ese_map_lua_tostring);

  // Create global Map table with functions
  const char *keys[] = {"new"};
  lua_CFunction functions[] = {_ese_map_lua_new};
  lua_engine_new_object(engine, "Map", 1, keys, functions);
}
