#include "types/vector_lua.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/vector.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations for helper functions from vector.c
extern EseVector *_ese_vector_make(void);

// Forward declarations for Lua methods
static int _ese_vector_lua_set_direction(lua_State *L);
static int _ese_vector_lua_magnitude(lua_State *L);
static int _ese_vector_lua_normalize(lua_State *L);
static int _ese_vector_lua_to_json(lua_State *L);
static int _ese_vector_lua_from_json(lua_State *L);

// ========================================
// PRIVATE LUA FUNCTIONS
// ========================================

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseVector
 *
 * Handles cleanup when a Lua proxy table for an EseVector is garbage collected.
 * Only frees the underlying EseVector if it has no C-side references.
 *
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static int _ese_vector_lua_gc(lua_State *L) {
    // Get from userdata
    EseVector **ud = (EseVector **)luaL_testudata(L, 1, VECTOR_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }

    EseVector *vector = *ud;
    if (vector) {
        // If lua_ref == LUA_NOREF, there are no more references to this vector,
        // so we can free it.
        // If lua_ref != LUA_NOREF, this vector was referenced from C and should
        // not be freed.
        if (ese_vector_get_lua_ref(vector) == LUA_NOREF) {
            ese_vector_destroy(vector);
        }
    }

    return 0;
}

/**
 * @brief Lua __index metamethod for EseVector property access
 *
 * Provides read access to vector properties (x, y) from Lua. When a Lua script
 * accesses vector.x or vector.y, this function is called to retrieve the
 * values. Also provides access to methods like set_direction, magnitude, and
 * normalize.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid
 * properties/methods, 0 for invalid)
 */
static int _ese_vector_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_VECTOR_INDEX);
    EseVector *vector = ese_vector_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!vector || !key) {
        profile_cancel(PROFILE_LUA_VECTOR_INDEX);
        return 0;
    }

    if (strcmp(key, "x") == 0) {
        lua_pushnumber(L, ese_vector_get_x(vector));
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(L, ese_vector_get_y(vector));
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "set_direction") == 0) {
        lua_pushlightuserdata(L, vector);
        lua_pushcclosure(L, _ese_vector_lua_set_direction, 1);
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "magnitude") == 0) {
        lua_pushlightuserdata(L, vector);
        lua_pushcclosure(L, _ese_vector_lua_magnitude, 1);
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "normalize") == 0) {
        lua_pushlightuserdata(L, vector);
        lua_pushcclosure(L, _ese_vector_lua_normalize, 1);
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "toJSON") == 0) {
        lua_pushlightuserdata(L, vector);
        lua_pushcclosure(L, _ese_vector_lua_to_json, 1);
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (method)");
        return 1;
    }
    profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index");
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseVector property assignment
 *
 * Provides write access to vector properties (x, y) from Lua. When a Lua script
 * assigns to vector.x or vector.y, this function is called to update the
 * values.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_vector_lua_newindex(lua_State *L) {
    profile_start(PROFILE_LUA_VECTOR_NEWINDEX);
    EseVector *vector = ese_vector_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!vector || !key) {
        profile_cancel(PROFILE_LUA_VECTOR_NEWINDEX);
        return 0;
    }

    if (strcmp(key, "x") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_VECTOR_NEWINDEX);
            return luaL_error(L, "vector.x must be a number");
        }
        ese_vector_set_x(vector, (float)lua_tonumber(L, 3));
        profile_stop(PROFILE_LUA_VECTOR_NEWINDEX, "vector_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "y") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_VECTOR_NEWINDEX);
            return luaL_error(L, "vector.y must be a number");
        }
        ese_vector_set_y(vector, (float)lua_tonumber(L, 3));
        profile_stop(PROFILE_LUA_VECTOR_NEWINDEX, "vector_lua_newindex (setter)");
        return 0;
    }
    profile_stop(PROFILE_LUA_VECTOR_NEWINDEX, "vector_lua_newindex (setter)");
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __tostring metamethod for EseVector string representation
 *
 * Converts an EseVector to a human-readable string for debugging and display.
 * The format includes the memory address and current x,y coordinates.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
static int _ese_vector_lua_tostring(lua_State *L) {
    EseVector *vector = ese_vector_lua_get(L, 1);

    if (!vector) {
        lua_pushstring(L, "Vector: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Vector: %p (x=%.2f, y=%.2f)", (void *)vector,
             ese_vector_get_x(vector), ese_vector_get_y(vector));
    lua_pushstring(L, buf);

    return 1;
}

// Lua constructors
/**
 * @brief Lua constructor function for creating new EseVector instances
 *
 * Creates a new EseVector from Lua with specified x,y coordinates.
 * This function is called when Lua code executes `Vector.new(x, y)`.
 * It validates the arguments, creates the underlying EseVector, and returns a
 * proxy table that provides access to the vector's properties and methods.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_vector_lua_new(lua_State *L) {
    profile_start(PROFILE_LUA_VECTOR_NEW);

    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 2) {
        profile_cancel(PROFILE_LUA_VECTOR_NEW);
        return luaL_error(L, "Vector.new(number, number) takes 2 arguments");
    }

    if (lua_type(L, 1) != LUA_TNUMBER) {
        profile_cancel(PROFILE_LUA_VECTOR_NEW);
        return luaL_error(L, "Vector.new(number, number) arguments must be numbers");
    }
    if (lua_type(L, 2) != LUA_TNUMBER) {
        profile_cancel(PROFILE_LUA_VECTOR_NEW);
        return luaL_error(L, "Vector.new(number, number) arguments must be numbers");
    }

    float x = (float)lua_tonumber(L, 1);
    float y = (float)lua_tonumber(L, 2);

    // Create the vector using the standard creation function
    EseVector *vector = _ese_vector_make();
    ese_vector_set_x(vector, x);
    ese_vector_set_y(vector, y);

    // Set the Lua state
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (engine) {
        ese_vector_set_state(vector, L);
    }

    // Create userdata directly
    EseVector **ud = (EseVector **)lua_newuserdata(L, sizeof(EseVector *));
    *ud = vector;

    // Attach metatable
    luaL_getmetatable(L, VECTOR_PROXY_META);
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_VECTOR_NEW, "vector_lua_new");
    return 1;
}

/**
 * @brief Lua constructor function for creating EseVector at origin
 *
 * Creates a new EseVector at the origin (0,0) from Lua.
 * This function is called when Lua code executes `Vector.zero()`.
 * It's a convenience constructor for creating vectors at the default position.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_vector_lua_zero(lua_State *L) {
    profile_start(PROFILE_LUA_VECTOR_ZERO);

    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 0) {
        profile_cancel(PROFILE_LUA_VECTOR_ZERO);
        return luaL_error(L, "Vector.zero() takes 0 arguments");
    }

    // Create the vector using the standard creation function
    EseVector *vector = _ese_vector_make();

    // Set the Lua state
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (engine) {
        ese_vector_set_state(vector, L);
    }

    // Create userdata directly
    EseVector **ud = (EseVector **)lua_newuserdata(L, sizeof(EseVector *));
    *ud = vector;

    // Attach metatable
    luaL_getmetatable(L, VECTOR_PROXY_META);
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_VECTOR_ZERO, "vector_lua_zero");
    return 1;
}

// Lua methods
/**
 * @brief Lua method for setting vector direction and magnitude
 *
 * Sets the vector's direction using cardinal direction strings (N, S, E, W)
 * and applies the specified magnitude. Handles diagonal directions
 * automatically.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_vector_lua_set_direction(lua_State *L) {
EseVector *vector = (EseVector *)lua_engine_instance_method_normalize(
        L, (EseLuaGetSelfFn)ese_vector_lua_get, "Vector");

    // After normalization, logical args start at index 1.
    int n_args = lua_gettop(L);
    if (n_args != 2) {
        return luaL_error(L, "vector:set_direction(string, number) takes 2 arguments");
    }
    if (lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TNUMBER) {
        return luaL_error(L, "vector:set_direction(string, number) takes a string and a number");
    }

    const char *direction = lua_tostring(L, 1);
    float magnitude = (float)lua_tonumber(L, 2);

    ese_vector_set_direction(vector, direction, magnitude);
    return 0;
}

/**
 * @brief Lua method for calculating vector magnitude
 *
 * Calculates and returns the magnitude (length) of the vector using the
 * Pythagorean theorem (sqrt(x² + y²)).
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the magnitude
 * value)
 */
static int _ese_vector_lua_magnitude(lua_State *L) {
EseVector *vector = (EseVector *)lua_engine_instance_method_normalize(
        L, (EseLuaGetSelfFn)ese_vector_lua_get, "Vector");

    // After normalization, vector:magnitude() takes 0 arguments.
    int n_args = lua_gettop(L);
    if (n_args != 0) {
        return luaL_error(L, "vector:magnitude() takes 0 arguments");
    }

    lua_pushnumber(L, ese_vector_magnitude(vector));
    return 1;
}

/**
 * @brief Lua method for normalizing the vector
 *
 * Normalizes the vector to unit length (magnitude = 1.0) while preserving
 * its direction. If the vector has zero magnitude, no change is made.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_vector_lua_normalize(lua_State *L) {
EseVector *vector = (EseVector *)lua_engine_instance_method_normalize(
        L, (EseLuaGetSelfFn)ese_vector_lua_get, "Vector");

    // After normalization, vector:normalize() takes 0 arguments.
    int n_args = lua_gettop(L);
    if (n_args != 0) {
        return luaL_error(L, "vector:normalize() takes 0 arguments");
    }

    ese_vector_normalize(vector);
    return 0;
}

/**
 * @brief Lua instance method for converting EseVector to JSON string
 */
static int _ese_vector_lua_to_json(lua_State *L) {
    EseVector *vector = ese_vector_lua_get(L, 1);
    if (!vector) {
        return luaL_error(L, "Vector:toJSON() called on invalid vector");
    }

    cJSON *json = ese_vector_serialize(vector);
    if (!json) {
        return luaL_error(L, "Vector:toJSON() failed to serialize vector");
    }

    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!json_str) {
        return luaL_error(L, "Vector:toJSON() failed to convert to string");
    }

    lua_pushstring(L, json_str);
    free(json_str);
    return 1;
}

/**
 * @brief Lua static method for creating EseVector from JSON string
 */
static int _ese_vector_lua_from_json(lua_State *L) {
    int argc = lua_gettop(L);
    if (argc != 1) {
        return luaL_error(L, "Vector.fromJSON(string) takes 1 argument");
    }
    if (lua_type(L, 1) != LUA_TSTRING) {
        return luaL_error(L, "Vector.fromJSON(string) argument must be a string");
    }

    const char *json_str = lua_tostring(L, 1);
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        log_error("VECTOR", "Vector.fromJSON: failed to parse JSON string: %s",
                  json_str ? json_str : "NULL");
        return luaL_error(L, "Vector.fromJSON: invalid JSON string");
    }

    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (!engine) {
        cJSON_Delete(json);
        return luaL_error(L, "Vector.fromJSON: no engine available");
    }

    EseVector *vector = ese_vector_deserialize(engine, json);
    cJSON_Delete(json);
    if (!vector) {
        return luaL_error(L, "Vector.fromJSON: failed to deserialize vector");
    }

    ese_vector_lua_push(vector);
    return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Initializes the EseVector userdata type in the Lua state.
 *
 * @details Creates and registers the "VectorProxyMeta" metatable with __index,
 * __newindex,
 *          __gc, __tostring metamethods for property access and garbage
 * collection. This allows EseVector objects to be used naturally from Lua with
 * dot notation. Also creates the global "Vector" table with "new" and "zero"
 * constructors.
 *
 * @param engine EseLuaEngine pointer where the EseVector type will be
 * registered
 */
void _ese_vector_lua_init(EseLuaEngine *engine) {
    log_assert("VECTOR", engine, "_ese_vector_lua_init called with NULL engine");

    // Create metatable
    lua_engine_new_object_meta(engine, VECTOR_PROXY_META, _ese_vector_lua_index,
                               _ese_vector_lua_newindex, _ese_vector_lua_gc,
                               _ese_vector_lua_tostring);

    // Create global Vector table with functions
    const char *keys[] = {"new", "zero", "fromJSON"};
    lua_CFunction functions[] = {_ese_vector_lua_new, _ese_vector_lua_zero,
                                 _ese_vector_lua_from_json};
    lua_engine_new_object(engine, "Vector", 3, keys, functions);
}
