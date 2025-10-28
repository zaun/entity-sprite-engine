#include "types/ray_lua.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/point.h"
#include "types/ray.h"
#include "types/rect.h"
#include "types/types.h"
#include "types/vector.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
extern EseRay *_ese_ray_make(void);

// Lua metamethods
static int _ese_ray_lua_gc(lua_State *L);
static int _ese_ray_lua_index(lua_State *L);
static int _ese_ray_lua_newindex(lua_State *L);
static int _ese_ray_lua_tostring(lua_State *L);

// Lua constructors
static int _ese_ray_lua_new(lua_State *L);
static int _ese_ray_lua_zero(lua_State *L);

// Lua methods
static int _ese_ray_lua_intersects_rect(lua_State *L);
static int _ese_ray_lua_get_point_at_distance(lua_State *L);
static int _ese_ray_lua_normalize(lua_State *L);
static int _ese_ray_lua_to_json(lua_State *L);
static int _ese_ray_lua_from_json(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseRay
 *
 * Handles cleanup when a Lua proxy table for an EseRay is garbage collected.
 * Only frees the underlying EseRay if it has no C-side references.
 *
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static int _ese_ray_lua_gc(lua_State *L) {
  // Get from userdata
  EseRay **ud = (EseRay **)luaL_testudata(L, 1, RAY_PROXY_META);
  if (!ud) {
    return 0; // Not our userdata
  }

  EseRay *ray = *ud;
  if (ray) {
    // If lua_ref == LUA_NOREF, there are no more references to this ray,
    // so we can free it.
    // If lua_ref != LUA_NOREF, this ray was referenced from C and should not be
    // freed.
    if (ese_ray_get_lua_ref(ray) == LUA_NOREF) {
      ese_ray_destroy(ray);
    }
  }

  return 0;
}

/**
 * @brief Lua __index metamethod for EseRay property access
 *
 * Provides read access to ray properties (x, y, dx, dy) from Lua. When a Lua
 * script accesses ray.x, ray.y, ray.dx, or ray.dy, this function is called to
 * retrieve the values. Also provides access to methods like intersects_rect,
 * get_point_at_distance, and normalize.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid
 * properties/methods, 0 for invalid)
 */
static int _ese_ray_lua_index(lua_State *L) {
  profile_start(PROFILE_LUA_RAY_INDEX);
  EseRay *ray = ese_ray_lua_get(L, 1);
  const char *key = lua_tostring(L, 2);
  if (!ray || !key) {
    profile_cancel(PROFILE_LUA_RAY_INDEX);
    return 0;
  }

  if (strcmp(key, "x") == 0) {
    lua_pushnumber(L, ese_ray_get_x(ray));
    profile_stop(PROFILE_LUA_RAY_INDEX, "ray_lua_index (getter)");
    return 1;
  } else if (strcmp(key, "y") == 0) {
    lua_pushnumber(L, ese_ray_get_y(ray));
    profile_stop(PROFILE_LUA_RAY_INDEX, "ray_lua_index (getter)");
    return 1;
  } else if (strcmp(key, "dx") == 0) {
    lua_pushnumber(L, ese_ray_get_dx(ray));
    profile_stop(PROFILE_LUA_RAY_INDEX, "ray_lua_index (getter)");
    return 1;
  } else if (strcmp(key, "dy") == 0) {
    lua_pushnumber(L, ese_ray_get_dy(ray));
    profile_stop(PROFILE_LUA_RAY_INDEX, "ray_lua_index (getter)");
    return 1;
  } else if (strcmp(key, "intersects_rect") == 0) {
    lua_pushlightuserdata(L, ray);
    lua_pushcclosure(L, _ese_ray_lua_intersects_rect, 1);
    profile_stop(PROFILE_LUA_RAY_INDEX, "ray_lua_index (method)");
    return 1;
  } else if (strcmp(key, "get_point_at_distance") == 0) {
    lua_pushlightuserdata(L, ray);
    lua_pushcclosure(L, _ese_ray_lua_get_point_at_distance, 1);
    profile_stop(PROFILE_LUA_RAY_INDEX, "ray_lua_index (method)");
    return 1;
  } else if (strcmp(key, "normalize") == 0) {
    lua_pushlightuserdata(L, ray);
    lua_pushcclosure(L, _ese_ray_lua_normalize, 1);
    profile_stop(PROFILE_LUA_RAY_INDEX, "ray_lua_index (method)");
    return 1;
  } else if (strcmp(key, "toJSON") == 0) {
    lua_pushlightuserdata(L, ray);
    lua_pushcclosure(L, _ese_ray_lua_to_json, 1);
    profile_stop(PROFILE_LUA_RAY_INDEX, "ray_lua_index (method)");
    return 1;
  }
  profile_stop(PROFILE_LUA_RAY_INDEX, "ray_lua_index (invalid)");
  return 0;
}

/**
 * @brief Lua __newindex metamethod for EseRay property assignment
 *
 * Provides write access to ray properties (x, y, dx, dy) from Lua. When a Lua
 * script assigns to ray.x, ray.y, ray.dx, or ray.dy, this function is called to
 * update the values.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_ray_lua_newindex(lua_State *L) {
  profile_start(PROFILE_LUA_RAY_NEWINDEX);
  EseRay *ray = ese_ray_lua_get(L, 1);
  const char *key = lua_tostring(L, 2);
  if (!ray || !key) {
    profile_cancel(PROFILE_LUA_RAY_NEWINDEX);
    return 0;
  }

  if (strcmp(key, "x") == 0) {
    if (lua_type(L, 3) != LUA_TNUMBER) {
      profile_cancel(PROFILE_LUA_RAY_NEWINDEX);
      return luaL_error(L, "ray.x must be a number");
    }
    ese_ray_set_x(ray, (float)lua_tonumber(L, 3));
    profile_stop(PROFILE_LUA_RAY_NEWINDEX, "ray_lua_newindex (setter)");
    return 0;
  } else if (strcmp(key, "y") == 0) {
    if (lua_type(L, 3) != LUA_TNUMBER) {
      profile_cancel(PROFILE_LUA_RAY_NEWINDEX);
      return luaL_error(L, "ray.y must be a number");
    }
    ese_ray_set_y(ray, (float)lua_tonumber(L, 3));
    profile_stop(PROFILE_LUA_RAY_NEWINDEX, "ray_lua_newindex (setter)");
    return 0;
  } else if (strcmp(key, "dx") == 0) {
    if (lua_type(L, 3) != LUA_TNUMBER) {
      profile_cancel(PROFILE_LUA_RAY_NEWINDEX);
      return luaL_error(L, "ray.dx must be a number");
    }
    ese_ray_set_dx(ray, (float)lua_tonumber(L, 3));
    profile_stop(PROFILE_LUA_RAY_NEWINDEX, "ray_lua_newindex (setter)");
    return 0;
  } else if (strcmp(key, "dy") == 0) {
    if (lua_type(L, 3) != LUA_TNUMBER) {
      profile_cancel(PROFILE_LUA_RAY_NEWINDEX);
      return luaL_error(L, "ray.dy must be a number");
    }
    ese_ray_set_dy(ray, (float)lua_tonumber(L, 3));
    profile_stop(PROFILE_LUA_RAY_NEWINDEX, "ray_lua_newindex (setter)");
    return 0;
  }
  profile_stop(PROFILE_LUA_RAY_NEWINDEX, "ray_lua_newindex (invalid)");
  return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __tostring metamethod for EseRay string representation
 *
 * Converts an EseRay to a human-readable string for debugging and display.
 * The format includes the memory address and current x,y,dx,dy values.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
static int _ese_ray_lua_tostring(lua_State *L) {
  EseRay *ray = ese_ray_lua_get(L, 1);

  if (!ray) {
    lua_pushstring(L, "Ray: (invalid)");
    return 1;
  }

  char buf[128];
  snprintf(buf, sizeof(buf), "Ray: %p (x=%.2f, y=%.2f, dx=%.2f, dy=%.2f)",
           (void *)ray, ese_ray_get_x(ray), ese_ray_get_y(ray),
           ese_ray_get_dx(ray), ese_ray_get_dy(ray));
  lua_pushstring(L, buf);

  return 1;
}

// Lua constructors
/**
 * @brief Lua constructor function for creating new EseRay instances
 *
 * Creates a new EseRay from Lua with specified origin and direction
 * coordinates. This function is called when Lua code executes `Ray.new(x, y,
 * dx, dy)` or `Ray.new(point, vector)`. It validates the arguments, creates the
 * underlying EseRay, and returns a proxy table that provides access to the
 * ray's properties and methods.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_ray_lua_new(lua_State *L) {
  profile_start(PROFILE_LUA_RAY_NEW);
  float x = 0.0f, y = 0.0f, dx = 1.0f, dy = 0.0f;

  int n_args = lua_gettop(L);
  if (n_args == 4) {
    if (lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER ||
        lua_type(L, 3) != LUA_TNUMBER || lua_type(L, 4) != LUA_TNUMBER) {
      profile_cancel(PROFILE_LUA_RAY_NEW);
      return luaL_error(
          L, "Ray.new(number, number, number, number) takes 4 arguments");
    }
    x = (float)lua_tonumber(L, 1);
    y = (float)lua_tonumber(L, 2);
    dx = (float)lua_tonumber(L, 3);
    dy = (float)lua_tonumber(L, 4);
  } else if (n_args == 2) {
    EsePoint *p = ese_point_lua_get(L, 1);
    EseVector *v = ese_vector_lua_get(L, 2);
    if (!p || !v) {
      profile_cancel(PROFILE_LUA_RAY_NEW);
      return luaL_error(L, "Ray.new(point, vector) takes 2 arguments");
    }
    x = ese_point_get_x(p);
    y = ese_point_get_y(p);
    dx = ese_vector_get_x(v);
    dy = ese_vector_get_y(v);
  } else {
    profile_cancel(PROFILE_LUA_RAY_NEW);
    return luaL_error(L, "Ray.new(x, y, dx, dy) or Ray.new(point, vector)");
  }

  // Get the current engine from Lua registry
  EseLuaEngine *engine =
      (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
  if (!engine) {
    profile_cancel(PROFILE_LUA_RAY_NEW);
    return luaL_error(L, "Ray.new: no engine available");
  }

  // Create the ray using the standard creation function
  EseRay *ray = ese_ray_create(engine);
  ese_ray_set_x(ray, x);
  ese_ray_set_y(ray, y);
  ese_ray_set_dx(ray, dx);
  ese_ray_set_dy(ray, dy);

  // Create userdata directly
  EseRay **ud = (EseRay **)lua_newuserdata(L, sizeof(EseRay *));
  *ud = ray;

  // Attach metatable
  luaL_getmetatable(L, RAY_PROXY_META);
  lua_setmetatable(L, -2);

  profile_stop(PROFILE_LUA_RAY_NEW, "ray_lua_new");
  return 1;
}

/**
 * @brief Lua constructor function for creating EseRay at origin
 *
 * Creates a new EseRay at the origin (0,0) with default direction (1,0) from
 * Lua. This function is called when Lua code executes `Ray.zero()`. It's a
 * convenience constructor for creating rays at the default position and
 * direction.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_ray_lua_zero(lua_State *L) {
  profile_start(PROFILE_LUA_RAY_ZERO);

  // Get argument count
  int argc = lua_gettop(L);
  if (argc != 0) {
    profile_cancel(PROFILE_LUA_RAY_ZERO);
    return luaL_error(L, "Ray.zero() takes 0 arguments");
  }

  // Get the current engine from Lua registry
  EseLuaEngine *engine =
      (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
  if (!engine) {
    profile_cancel(PROFILE_LUA_RAY_ZERO);
    return luaL_error(L, "Ray.zero: no engine available");
  }

  // Create the ray using the standard creation function
  EseRay *ray = ese_ray_create(engine);

  // Create userdata directly
  EseRay **ud = (EseRay **)lua_newuserdata(L, sizeof(EseRay *));
  *ud = ray;

  // Attach metatable
  luaL_getmetatable(L, RAY_PROXY_META);
  lua_setmetatable(L, -2);

  profile_stop(PROFILE_LUA_RAY_ZERO, "ray_lua_zero");
  return 1;
}

// Lua methods
/**
 * @brief Lua method for testing ray-rectangle intersection
 *
 * Tests whether the ray intersects with a given rectangle using efficient
 * AABB intersection algorithms. Returns a boolean result.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - boolean result)
 */
static int _ese_ray_lua_intersects_rect(lua_State *L) {
  // Get argument count
  int n_args = lua_gettop(L);
  if (n_args != 2) {
    return luaL_error(L, "ray:intersects_rect(rect) takes 1 argument");
  }

  EseRay *ray = (EseRay *)lua_touserdata(L, lua_upvalueindex(1));
  if (!ray) {
    return luaL_error(L, "Invalid EseRay object in intersects_rect method");
  }

  EseRect *rect = ese_rect_lua_get(L, 2);
  if (!rect) {
    return luaL_error(L, "ray:intersects_rect(rect) takes a Rect");
  }

  lua_pushboolean(L, ese_ray_intersects_rect(ray, rect));
  return 1;
}

/**
 * @brief Lua method for getting a point along the ray at a specified distance
 *
 * Calculates the coordinates of a point that lies along the ray at the given
 * distance from the ray's origin. Returns both x and y coordinates.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 2 - x and y
 * coordinates)
 */
static int _ese_ray_lua_get_point_at_distance(lua_State *L) {
  // Get argument count
  int n_args = lua_gettop(L);
  if (n_args != 2) {
    return luaL_error(L,
                      "ray:get_point_at_distance(distance) takes 1 argument");
  }

  EseRay *ray = (EseRay *)lua_touserdata(L, lua_upvalueindex(1));
  if (!ray) {
    return luaL_error(L,
                      "Invalid EseRay object in get_point_at_distance method");
  }

  if (lua_type(L, 2) != LUA_TNUMBER) {
    return luaL_error(L, "ray:get_point_at_distance(distance) takes a number");
  }

  float distance = (float)lua_tonumber(L, 2);
  float x, y;
  ese_ray_get_point_at_distance(ray, distance, &x, &y);

  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  return 2;
}

/**
 * @brief Lua method for normalizing the ray direction
 *
 * Normalizes the ray's direction vector to unit length while preserving
 * its direction. If the direction has zero magnitude, no change is made.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_ray_lua_normalize(lua_State *L) {
  EseRay *ray = (EseRay *)lua_touserdata(L, lua_upvalueindex(1));
  if (!ray) {
    return luaL_error(L, "Invalid EseRay object in normalize method");
  }

  ese_ray_normalize(ray);
  return 0;
}

/**
 * @brief Lua instance method for converting EseRay to JSON string
 *
 * Converts an EseRay to a JSON string representation. This function is called
 * when Lua code executes `ray:toJSON()`. It serializes the ray's origin and
 * direction to JSON format and returns the string.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the JSON string)
 */
static int _ese_ray_lua_to_json(lua_State *L) {
  profile_start(PROFILE_LUA_RAY_TO_JSON);

  EseRay *ray = ese_ray_lua_get(L, 1);
  if (!ray) {
    profile_cancel(PROFILE_LUA_RAY_TO_JSON);
    return luaL_error(L, "Ray:toJSON() called on invalid ray");
  }

  // Serialize ray to JSON
  cJSON *json = ese_ray_serialize(ray);
  if (!json) {
    profile_cancel(PROFILE_LUA_RAY_TO_JSON);
    return luaL_error(L, "Ray:toJSON() failed to serialize ray");
  }

  // Convert JSON to string
  char *json_str = cJSON_PrintUnformatted(json);
  cJSON_Delete(json); // Clean up JSON object

  if (!json_str) {
    profile_cancel(PROFILE_LUA_RAY_TO_JSON);
    return luaL_error(L, "Ray:toJSON() failed to convert to string");
  }

  // Push the JSON string onto the stack (lua_pushstring makes a copy)
  lua_pushstring(L, json_str);

  // Clean up the string (cJSON_Print uses malloc)
  // Note: We free here, but lua_pushstring should have made a copy
  free(json_str); // cJSON doesnt use the memory manager.

  profile_stop(PROFILE_LUA_RAY_TO_JSON, "ray_lua_to_json");
  return 1;
}

/**
 * @brief Lua static method for creating EseRay from JSON string
 *
 * Creates a new EseRay from a JSON string. This function is called when Lua
 * code executes `Ray.fromJSON(json_string)`. It parses the JSON string,
 * validates it contains ray data, and creates a new ray with the specified
 * origin and direction.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_ray_lua_from_json(lua_State *L) {
  profile_start(PROFILE_LUA_RAY_FROM_JSON);

  // Get argument count
  int argc = lua_gettop(L);
  if (argc != 1) {
    profile_cancel(PROFILE_LUA_RAY_FROM_JSON);
    return luaL_error(L, "Ray.fromJSON(string) takes 1 argument");
  }

  if (lua_type(L, 1) != LUA_TSTRING) {
    profile_cancel(PROFILE_LUA_RAY_FROM_JSON);
    return luaL_error(L, "Ray.fromJSON(string) argument must be a string");
  }

  const char *json_str = lua_tostring(L, 1);

  // Parse JSON string
  cJSON *json = cJSON_Parse(json_str);
  if (!json) {
    log_error("RAY", "Ray.fromJSON: failed to parse JSON string: %s",
              json_str ? json_str : "NULL");
    profile_cancel(PROFILE_LUA_RAY_FROM_JSON);
    return luaL_error(L, "Ray.fromJSON: invalid JSON string");
  }

  // Get the current engine from Lua registry
  EseLuaEngine *engine =
      (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
  if (!engine) {
    cJSON_Delete(json);
    profile_cancel(PROFILE_LUA_RAY_FROM_JSON);
    return luaL_error(L, "Ray.fromJSON: no engine available");
  }

  // Use the existing deserialization function
  EseRay *ray = ese_ray_deserialize(engine, json);
  cJSON_Delete(json); // Clean up JSON object

  if (!ray) {
    profile_cancel(PROFILE_LUA_RAY_FROM_JSON);
    return luaL_error(L, "Ray.fromJSON: failed to deserialize ray");
  }

  ese_ray_lua_push(ray);

  profile_stop(PROFILE_LUA_RAY_FROM_JSON, "ray_lua_from_json");
  return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

void _ese_ray_lua_init(EseLuaEngine *engine) {
  // Create metatable
  lua_engine_new_object_meta(engine, RAY_PROXY_META, _ese_ray_lua_index,
                             _ese_ray_lua_newindex, _ese_ray_lua_gc,
                             _ese_ray_lua_tostring);

  // Create global Ray table with functions
  const char *keys[] = {"new", "zero", "fromJSON"};
  lua_CFunction functions[] = {_ese_ray_lua_new, _ese_ray_lua_zero,
                               _ese_ray_lua_from_json};
  lua_engine_new_object(engine, "Ray", 3, keys, functions);
}
