#include "types/tileset_lua.h"
#include "core/memory_manager.h"
#include "graphics/sprite.h"
#include "scripting/lua_engine.h"
#include "types/tileset.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define INITIAL_SPRITE_CAPACITY 4

// Forward declarations for helper functions from tileset.c
extern EseTileSet *_ese_tileset_make(void);

// ========================================
// PRIVATE LUA FUNCTIONS
// ========================================

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseTileSet
 *
 * Handles cleanup when a Lua userdata for an EseTileSet is garbage collected.
 * Only frees the underlying EseTileSet if it has no C-side references.
 *
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static int _ese_tileset_lua_gc(lua_State *L) {
  // Get from userdata
  EseTileSet **ud = (EseTileSet **)luaL_testudata(L, 1, TILESET_PROXY_META);
  if (!ud) {
    return 0; // Not our userdata
  }

  EseTileSet *tiles = *ud;
  if (tiles) {
    // If lua_ref == LUA_NOREF, there are no more references to this tileset,
    // so we can free it.
    // If lua_ref != LUA_NOREF, this tileset was referenced from C and should
    // not be freed.
    if (ese_tileset_get_lua_ref(tiles) == LUA_NOREF) {
      ese_tileset_destroy(tiles);
    }
  }

  return 0;
}

/* ----------------- Lua Methods ----------------- */

static int _ese_tileset_lua_add_sprite(lua_State *L) {
  EseTileSet *tiles = ese_tileset_lua_get(L, 1);
  if (!tiles)
    return luaL_error(L, "Invalid Tiles in add_sprite");

  if (!lua_isnumber(L, 2) || !lua_isstring(L, 3))
    return luaL_error(L, "add_sprite(tile_id, sprite_id, [weight]) requires "
                         "number, string, [number]");

  uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
  const char *sprite_str = lua_tostring(L, 3);
  uint16_t weight = lua_isnumber(L, 4) ? (uint16_t)lua_tonumber(L, 4) : 1;

  if (!sprite_str || strlen(sprite_str) == 0)
    return luaL_error(L, "sprite_id cannot be empty");
  if (weight == 0)
    return luaL_error(L, "weight must be > 0");

  lua_pushboolean(L,
                  ese_tileset_add_sprite(tiles, tile_id, sprite_str, weight));
  return 1;
}

static int _ese_tileset_lua_remove_sprite(lua_State *L) {
  EseTileSet *tiles = ese_tileset_lua_get(L, 1);
  if (!tiles)
    return luaL_error(L, "Invalid Tiles in remove_sprite");

  if (!lua_isnumber(L, 2) || !lua_isstring(L, 3))
    return luaL_error(
        L, "remove_sprite(tile_id, sprite_id) requires number, string");

  uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
  const char *sprite_str = lua_tostring(L, 3);

  if (!sprite_str || strlen(sprite_str) == 0)
    return luaL_error(L, "sprite_id cannot be empty");

  lua_pushboolean(L, ese_tileset_remove_sprite(tiles, tile_id, sprite_str));
  return 1;
}

static int _ese_tileset_lua_get_sprite(lua_State *L) {
  EseTileSet *tiles = ese_tileset_lua_get(L, 1);
  if (!tiles)
    return luaL_error(L, "Invalid Tiles in get_sprite");

  if (!lua_isnumber(L, 2))
    return luaL_error(L, "get_sprite(tile_id) requires a number");

  uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
  const char *sprite = ese_tileset_get_sprite(tiles, tile_id);

  if (!sprite)
    lua_pushnil(L);
  else
    lua_pushstring(L, sprite);
  return 1;
}

static int _ese_tileset_lua_clear_mapping(lua_State *L) {
  EseTileSet *tiles = ese_tileset_lua_get(L, 1);
  if (!tiles)
    return luaL_error(L, "Invalid Tiles in clear_mapping");

  if (!lua_isnumber(L, 2))
    return luaL_error(L, "clear_mapping(tile_id) requires a number");

  uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
  ese_tileset_clear_mapping(tiles, tile_id);
  return 0;
}

static int _ese_tileset_lua_get_sprite_count(lua_State *L) {
  EseTileSet *tiles = ese_tileset_lua_get(L, 1);
  if (!tiles)
    return luaL_error(L, "Invalid Tiles in get_sprite_count");

  if (!lua_isnumber(L, 2))
    return luaL_error(L, "get_sprite_count(tile_id) requires a number");

  uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
  lua_pushnumber(L, ese_tileset_get_sprite_count(tiles, tile_id));
  return 1;
}

static int _ese_tileset_lua_update_sprite_weight(lua_State *L) {
  EseTileSet *tiles = ese_tileset_lua_get(L, 1);
  if (!tiles)
    return luaL_error(L, "Invalid Tiles in update_sprite_weight");

  if (!lua_isnumber(L, 2) || !lua_isstring(L, 3) || !lua_isnumber(L, 4))
    return luaL_error(L, "update_sprite_weight(tile_id, sprite_id, weight) "
                         "requires number, string, number");

  uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
  const char *sprite_str = lua_tostring(L, 3);
  uint16_t weight = (uint16_t)lua_tonumber(L, 4);

  if (!sprite_str || strlen(sprite_str) == 0)
    return luaL_error(L, "sprite_id cannot be empty");
  if (weight == 0)
    return luaL_error(L, "weight must be > 0");

  lua_pushboolean(
      L, ese_tileset_update_sprite_weight(tiles, tile_id, sprite_str, weight));
  return 1;
}

/**
 * @brief Lua __index metamethod for EseTileSet property access
 *
 * Provides read access to tileset methods from Lua. When a Lua script
 * accesses tileset.method, this function is called to retrieve the methods.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid methods, 0 for
 * invalid)
 */
static int _ese_tileset_lua_index(lua_State *L) {
  profile_start(PROFILE_LUA_TILESET_INDEX);
  EseTileSet *tiles = ese_tileset_lua_get(L, 1);
  const char *key = lua_tostring(L, 2);
  if (!tiles || !key) {
    profile_cancel(PROFILE_LUA_TILESET_INDEX);
    return 0;
  }

  if (strcmp(key, "add_sprite") == 0) {
    lua_pushcfunction(L, _ese_tileset_lua_add_sprite);
    profile_stop(PROFILE_LUA_TILESET_INDEX, "ese_tileset_lua_index (method)");
    return 1;
  } else if (strcmp(key, "remove_sprite") == 0) {
    lua_pushcfunction(L, _ese_tileset_lua_remove_sprite);
    profile_stop(PROFILE_LUA_TILESET_INDEX, "ese_tileset_lua_index (method)");
    return 1;
  } else if (strcmp(key, "get_sprite") == 0) {
    lua_pushcfunction(L, _ese_tileset_lua_get_sprite);
    profile_stop(PROFILE_LUA_TILESET_INDEX, "ese_tileset_lua_index (method)");
    return 1;
  } else if (strcmp(key, "clear_mapping") == 0) {
    lua_pushcfunction(L, _ese_tileset_lua_clear_mapping);
    profile_stop(PROFILE_LUA_TILESET_INDEX, "ese_tileset_lua_index (method)");
    return 1;
  } else if (strcmp(key, "get_sprite_count") == 0) {
    lua_pushcfunction(L, _ese_tileset_lua_get_sprite_count);
    profile_stop(PROFILE_LUA_TILESET_INDEX, "ese_tileset_lua_index (method)");
    return 1;
  } else if (strcmp(key, "update_sprite_weight") == 0) {
    lua_pushcfunction(L, _ese_tileset_lua_update_sprite_weight);
    profile_stop(PROFILE_LUA_TILESET_INDEX, "ese_tileset_lua_index (method)");
    return 1;
  }
  profile_stop(PROFILE_LUA_TILESET_INDEX, "ese_tileset_lua_index (invalid)");
  return 0;
}

/**
 * @brief Lua __newindex metamethod for EseTileSet property assignment
 *
 * Provides write access to tileset properties from Lua. Currently returns
 * an error as direct assignment is not supported - use methods instead.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_tileset_lua_newindex(lua_State *L) {
  return luaL_error(L, "Direct assignment not supported - use methods");
}

/**
 * @brief Lua __tostring metamethod for EseTileSet string representation
 *
 * Converts an EseTileSet to a human-readable string for debugging and display.
 * The format includes the memory address and total sprite count.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
static int _ese_tileset_lua_tostring(lua_State *L) {
  EseTileSet *tiles = ese_tileset_lua_get(L, 1);

  if (!tiles) {
    lua_pushstring(L, "Tileset: (invalid)");
    return 1;
  }

  size_t total = 0;
  for (int i = 0; i < 256; i++) {
    total += ese_tileset_get_sprite_count(tiles, i);
  }

  char buf[128];
  snprintf(buf, sizeof(buf), "Tileset: %p (total_sprites=%zu)", (void *)tiles,
           total);
  lua_pushstring(L, buf);
  return 1;
}

// Lua constructors
/**
 * @brief Lua constructor function for creating new EseTileSet instances
 *
 * Creates a new EseTileSet from Lua. This function is called when Lua code
 * executes `Tileset.new()`. It creates the underlying EseTileSet and returns
 * a userdata that provides access to the tileset's methods.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the userdata)
 */
static int _ese_tileset_lua_new(lua_State *L) {
  profile_start(PROFILE_LUA_TILESET_NEW);

  // Get argument count
  int argc = lua_gettop(L);
  if (argc != 0) {
    profile_cancel(PROFILE_LUA_TILESET_NEW);
    return luaL_error(L, "Tileset.new() takes 0 arguments");
  }

  // Create the tileset
  EseTileSet *tiles = _ese_tileset_make();
  if (!tiles) {
    profile_cancel(PROFILE_LUA_TILESET_NEW);
    return luaL_error(L, "Failed to create Tileset");
  }

  // Set the Lua state
  EseLuaEngine *engine =
      (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
  if (engine) {
    ese_tileset_set_state(tiles, L);
  }

  // Create userdata directly
  EseTileSet **ud = (EseTileSet **)lua_newuserdata(L, sizeof(EseTileSet *));
  *ud = tiles;

  // Attach metatable
  luaL_getmetatable(L, TILESET_PROXY_META);
  lua_setmetatable(L, -2);

  profile_stop(PROFILE_LUA_TILESET_NEW, "ese_tileset_lua_new");
  return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Initializes the EseTileSet userdata type in the Lua state.
 *
 * @details
 * Creates and registers the "TilesetProxyMeta" metatable with
 * __index, __newindex, __gc, and __tostring metamethods.
 * This allows EseTileSet objects to be used naturally from Lua
 * with dot notation and automatic garbage collection.
 *
 * @param engine EseLuaEngine pointer where the EseTileSet type will be
 * registered
 */
void _ese_tileset_lua_init(EseLuaEngine *engine) {
  log_assert("TILESET", engine,
             "_ese_tileset_lua_init called with NULL engine");

  // Create metatable
  lua_engine_new_object_meta(engine, TILESET_PROXY_META, _ese_tileset_lua_index,
                             _ese_tileset_lua_newindex, _ese_tileset_lua_gc,
                             _ese_tileset_lua_tostring);

  // Create global Tileset table with functions
  const char *keys[] = {"new"};
  lua_CFunction functions[] = {_ese_tileset_lua_new};
  lua_engine_new_object(engine, "Tileset", 1, keys, functions);
}
