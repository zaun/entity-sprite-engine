#include "types/display_lua.h"
#include "scripting/lua_engine.h"
#include "types/display.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <stdio.h>
#include <string.h>

// ========================================
// PRIVATE LUA HELPER FUNCTIONS
// ========================================

// Lua viewport helpers
/**
 * @brief Lua helper function for accessing viewport properties
 *
 * Provides read-only access to viewport properties (width, height) from Lua.
 * This function is used as the __index metamethod for the viewport proxy table.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid properties, 0 for
 * invalid)
 */
static int _ese_display_viewport_index(lua_State *L) {
  EseViewport *viewport = (EseViewport *)lua_touserdata(L, lua_upvalueindex(1));
  const char *key = lua_tostring(L, 2);
  if (!viewport || !key)
    return 0;

  if (strcmp(key, "width") == 0) {
    lua_pushinteger(L, viewport->width);
    return 1;
  }
  if (strcmp(key, "height") == 0) {
    lua_pushinteger(L, viewport->height);
    return 1;
  }

  return 0;
}

/**
 * @brief Lua helper function for read-only error handling
 *
 * Called when any attempt is made to modify read-only display state tables.
 * Always returns an error indicating that the tables are read-only.
 *
 * @param L Lua state
 * @return Never returns (always calls luaL_error)
 */
static int _ese_display_readonly_error(lua_State *L) {
  return luaL_error(L, "Display tables are read-only");
}

// ========================================
// PRIVATE LUA FUNCTIONS
// ========================================

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseDisplay
 *
 * Handles cleanup when a Lua proxy table for an EseDisplay is garbage
 * collected. Only frees the underlying EseDisplay if it has no C-side
 * references.
 *
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static int _ese_display_lua_gc(lua_State *L) {
  // Get from userdata
  EseDisplay **ud = (EseDisplay **)luaL_testudata(L, 1, DISPLAY_META);
  if (!ud) {
    return 0; // Not our userdata
  }

  EseDisplay *display = *ud;
  if (display) {
    // If lua_ref == LUA_NOREF, there are no more references to this display,
    // so we can free it.
    // If lua_ref != LUA_NOREF, this display was referenced from C and should
    // not be freed.
    if (ese_display_get_lua_ref(display) == LUA_NOREF) {
      ese_display_destroy(display);
    }
  }

  return 0;
}

/**
 * @brief Lua __index metamethod for EseDisplay property access
 *
 * Provides read access to display properties from Lua. When a Lua script
 * accesses display.width, display.height, display.fullscreen, etc., this
 * function is called to retrieve the values. Creates read-only proxy tables for
 * viewport data.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid properties, 0 for
 * invalid)
 */
static int _ese_display_lua_index(lua_State *L) {
  profile_start(PROFILE_LUA_DISPLAY_INDEX);
  EseDisplay *display = ese_display_lua_get(L, 1);
  const char *key = lua_tostring(L, 2);
  if (!display || !key) {
    profile_cancel(PROFILE_LUA_DISPLAY_INDEX);
    return 0;
  }

  // Simple properties
  if (strcmp(key, "fullscreen") == 0) {
    lua_pushboolean(L, ese_display_get_fullscreen(display));
    profile_stop(PROFILE_LUA_DISPLAY_INDEX,
                 "ese_display_lua_index (fullscreen)");
    return 1;
  }
  if (strcmp(key, "width") == 0) {
    lua_pushinteger(L, ese_display_get_width(display));
    profile_stop(PROFILE_LUA_DISPLAY_INDEX, "ese_display_lua_index (width)");
    return 1;
  }
  if (strcmp(key, "height") == 0) {
    lua_pushinteger(L, ese_display_get_height(display));
    profile_stop(PROFILE_LUA_DISPLAY_INDEX, "ese_display_lua_index (height)");
    return 1;
  }
  if (strcmp(key, "aspect_ratio") == 0) {
    lua_pushnumber(L, ese_display_get_aspect_ratio(display));
    profile_stop(PROFILE_LUA_DISPLAY_INDEX,
                 "ese_display_lua_index (aspect_ratio)");
    return 1;
  }

  // viewport table proxy (read-only)
  if (strcmp(key, "viewport") == 0) {
    // Create the table
    lua_newtable(L);

    // Create and set the metatable
    lua_newtable(L);

    // Set __index closure with the viewport pointer as upvalue
    lua_pushlightuserdata(L, (void *)ese_display_get_viewport(display));
    lua_pushcclosure(L, _ese_display_viewport_index, 1);
    lua_setfield(L, -2, "__index");

    // Set __newindex to error
    lua_pushcfunction(L, _ese_display_readonly_error);
    lua_setfield(L, -2, "__newindex");

    // Apply metatable to the table
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_DISPLAY_INDEX, "ese_display_lua_index (viewport)");
    return 1;
  }

  profile_stop(PROFILE_LUA_DISPLAY_INDEX, "ese_display_lua_index (invalid)");
  return 0;
}

/**
 * @brief Lua __newindex metamethod for EseDisplay property assignment
 *
 * Provides write access to display properties from Lua. Since display state is
 * read-only, this function always returns an error.
 *
 * @param L Lua state
 * @return Never returns (always calls luaL_error)
 */
static int _ese_display_lua_newindex(lua_State *L) {
  profile_start(PROFILE_LUA_DISPLAY_NEWINDEX);
  profile_stop(PROFILE_LUA_DISPLAY_NEWINDEX,
               "ese_display_lua_newindex (error)");
  return luaL_error(L, "Display object is read-only");
}

/**
 * @brief Lua __tostring metamethod for EseDisplay string representation
 *
 * Converts an EseDisplay to a human-readable string for debugging and display.
 * The format includes the memory address, dimensions, fullscreen status, and
 * viewport size.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
static int _ese_display_lua_tostring(lua_State *L) {
  EseDisplay *display = ese_display_lua_get(L, 1);
  if (!display) {
    lua_pushstring(L, "Display: (invalid)");
    return 1;
  }
  char buf[256];
  snprintf(buf, sizeof(buf), "Display: %p (%dx%d, %s, viewport: %dx%d)",
           (void *)display, ese_display_get_width(display),
           ese_display_get_height(display),
           ese_display_get_fullscreen(display) ? "fullscreen" : "windowed",
           ese_display_get_viewport_width(display),
           ese_display_get_viewport_height(display));
  lua_pushstring(L, buf);
  return 1;
}

// ========================================
// PUBLIC LUA FUNCTIONS
// ========================================

/**
 * @brief Initializes the Display userdata type in the Lua state.
 *
 * @details This function registers the 'DisplayMeta' metatable in the Lua
 * registry, which defines how the `EseDisplay` C object interacts with Lua
 * scripts.
 *
 * @param engine A pointer to the `EseLuaEngine` where the Display type will be
 * registered.
 * @return void
 *
 * @note This function should be called once during engine initialization.
 */
void _ese_display_lua_init(EseLuaEngine *engine) {
  log_assert("DISPLAY_STATE", engine,
             "_ese_display_lua_init called with NULL engine");
  log_assert("DISPLAY_STATE", engine->runtime,
             "_ese_display_lua_init called with NULL engine->runtime");

  // Create metatable
  lua_engine_new_object_meta(engine, DISPLAY_META, _ese_display_lua_index,
                             _ese_display_lua_newindex, _ese_display_lua_gc,
                             _ese_display_lua_tostring);
}
