#include "entity/entity.h"
#include "scripting/lua_engine.h"
#include "scripting/lua_engine_private.h"
#include "scripting/lua_value.h"
#include "types/collision_hit.h"
#include "types/map.h"
#include "types/rect.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <stdio.h>
#include <string.h>

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Lua metamethods
static int _ese_collision_hit_lua_gc(lua_State *L);
static int _ese_collision_hit_lua_index(lua_State *L);
static int _ese_collision_hit_lua_newindex(lua_State *L);
static int _ese_collision_hit_lua_tostring(lua_State *L);
static void _register_collision_hit_constants(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

/**
 * @brief Registers constant tables for EseCollisionHit in the active Lua table.
 *
 * Pushes TYPE (COLLIDER, MAP) and STATE (ENTER, STAY, LEAVE) sub-tables.
 */
static void _register_collision_hit_constants(lua_State *L) {
  // EseCollisionHit.TYPE
  lua_newtable(L);
  lua_pushinteger(L, COLLISION_KIND_COLLIDER);
  lua_setfield(L, -2, "COLLIDER");
  lua_pushinteger(L, COLLISION_KIND_MAP);
  lua_setfield(L, -2, "MAP");
  lua_setfield(L, -2, "TYPE");

  // EseCollisionHit.STATE
  lua_newtable(L);
  lua_pushinteger(L, COLLISION_STATE_ENTER);
  lua_setfield(L, -2, "ENTER");
  lua_pushinteger(L, COLLISION_STATE_STAY);
  lua_setfield(L, -2, "STAY");
  lua_pushinteger(L, COLLISION_STATE_LEAVE);
  lua_setfield(L, -2, "LEAVE");
  lua_setfield(L, -2, "STATE");
}

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseCollisionHit
 *
 * Frees the underlying hit when there are no C-side references.
 */
static int _ese_collision_hit_lua_gc(lua_State *L) {
  EseCollisionHit **ud =
      (EseCollisionHit **)luaL_testudata(L, 1, COLLISION_HIT_META);
  if (!ud)
    return 0;
  EseCollisionHit *hit = *ud;
  if (hit && ese_collision_hit_get_lua_ref(hit) == LUA_NOREF) {
    ese_collision_hit_destroy(hit);
  }
  return 0;
}

/**
 * @brief Lua __index metamethod for EseCollisionHit property access
 *
 * Provides read access to hit properties (kind, state, entity, target) and
 * type-specific data (rect, map, cell_x, cell_y).
 */
static int _ese_collision_hit_lua_index(lua_State *L) {
  profile_start(PROFILE_LUA_COLLISION_HIT_INDEX);
  EseCollisionHit *hit = ese_collision_hit_lua_get(L, 1);
  const char *key = lua_tostring(L, 2);
  if (!hit || !key) {
    profile_cancel(PROFILE_LUA_COLLISION_HIT_INDEX);
    return 0;
  }

  if (strcmp(key, "kind") == 0) {
    lua_pushinteger(L, ese_collision_hit_get_kind(hit));
    profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX,
                 "collision_hit_lua_index (getter)");
    return 1;
  } else if (strcmp(key, "state") == 0) {
    lua_pushinteger(L, ese_collision_hit_get_state(hit));
    profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX,
                 "collision_hit_lua_index (getter)");
    return 1;
  } else if (strcmp(key, "entity") == 0) {
    entity_lua_push(ese_collision_hit_get_entity(hit));
    profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX,
                 "collision_hit_lua_index (getter)");
    return 1;
  } else if (strcmp(key, "target") == 0) {
    entity_lua_push(ese_collision_hit_get_target(hit));
    profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX,
                 "collision_hit_lua_index (getter)");
    return 1;
  } else if (strcmp(key, "rect") == 0) {
    if (ese_collision_hit_get_kind(hit) == COLLISION_KIND_COLLIDER &&
        ese_collision_hit_get_rect(hit)) {
      ese_rect_lua_push(ese_collision_hit_get_rect(hit));
      profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX,
                   "collision_hit_lua_index (getter)");
      return 1;
    }
    profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX,
                 "collision_hit_lua_index (invalid)");
    return 0;
  } else if (strcmp(key, "map") == 0) {
    if (ese_collision_hit_get_kind(hit) == COLLISION_KIND_MAP &&
        ese_collision_hit_get_map(hit)) {
      ese_map_lua_push(ese_collision_hit_get_map(hit));
      profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX,
                   "collision_hit_lua_index (getter)");
      return 1;
    }
    profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX,
                 "collision_hit_lua_index (invalid)");
    return 0;
  } else if (strcmp(key, "cell_x") == 0) {
    if (ese_collision_hit_get_kind(hit) == COLLISION_KIND_MAP) {
      // For cell_x, we need to create a Lua value from the integer
      int cell_x = ese_collision_hit_get_cell_x(hit);
      lua_pushinteger(L, cell_x);
      profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX,
                   "collision_hit_lua_index (getter)");
      return 1;
    }
    profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX,
                 "collision_hit_lua_index (invalid)");
    return 0;
  } else if (strcmp(key, "cell_y") == 0) {
    if (ese_collision_hit_get_kind(hit) == COLLISION_KIND_MAP) {
      // For cell_y, we need to create a Lua value from the integer
      int cell_y = ese_collision_hit_get_cell_y(hit);
      lua_pushinteger(L, cell_y);
      profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX,
                   "collision_hit_lua_index (getter)");
      return 1;
    }
    profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX,
                 "collision_hit_lua_index (invalid)");
    return 0;
  }

  profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX,
               "collision_hit_lua_index (invalid)");
  return 0;
}

/**
 * @brief Lua __newindex metamethod for EseCollisionHit (read-only)
 */
static int _ese_collision_hit_lua_newindex(lua_State *L) {
  return luaL_error(L, "EseCollisionHit is read-only");
}

/**
 * @brief Lua __tostring metamethod for EseCollisionHit string representation
 */
static int _ese_collision_hit_lua_tostring(lua_State *L) {
  EseCollisionHit *hit = ese_collision_hit_lua_get(L, 1);
  if (!hit) {
    lua_pushstring(L, "EseCollisionHit: (invalid)");
    return 1;
  }
  char buf[160];
  snprintf(buf, sizeof(buf), "EseCollisionHit: %p (kind=%d, state=%d)",
           (void *)hit, (int)ese_collision_hit_get_kind(hit),
           (int)ese_collision_hit_get_state(hit));
  lua_pushstring(L, buf);
  return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Internal Lua initialization function for EseCollisionHit
 *
 * @details This function is called by the public ese_collision_hit_lua_init
 * function to set up all the private Lua metamethods and methods for
 * EseCollisionHit.
 *
 * @param engine EseLuaEngine pointer where the EseCollisionHit type will be
 * registered
 */
void _ese_collision_hit_lua_init(EseLuaEngine *engine) {
  // Create metatable
  lua_engine_new_object_meta(
      engine, COLLISION_HIT_META, _ese_collision_hit_lua_index,
      _ese_collision_hit_lua_newindex, _ese_collision_hit_lua_gc,
      _ese_collision_hit_lua_tostring);

  // Create global EseCollisionHit table with only constants
  lua_State *L = engine->runtime;
  lua_getglobal(L, "EseCollisionHit");
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    lua_newtable(L);
    _register_collision_hit_constants(L);
    lua_setglobal(L, "EseCollisionHit");
  } else {
    // augment existing table
    _register_collision_hit_constants(L);
    lua_pop(L, 1);
  }
}
