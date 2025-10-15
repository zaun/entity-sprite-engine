#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/types.h"
#include "types/point.h"
#include "types/point_lua.h"
#include "vendor/json/cJSON.h"

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Lua metamethods
static int _ese_point_lua_gc(lua_State *L);
static int _ese_point_lua_index(lua_State *L);
static int _ese_point_lua_newindex(lua_State *L);
static int _ese_point_lua_tostring(lua_State *L);

// Lua constructors
static int _ese_point_lua_new(lua_State *L);
static int _ese_point_lua_zero(lua_State *L);
static int _ese_point_lua_distance(lua_State *L);

// Lua JSON methods
static int _ese_point_lua_from_json(lua_State *L);
static int _ese_point_lua_to_json(lua_State *L);

// Forward declarations for private functions
extern EsePoint *_ese_point_make(void);
extern void _ese_point_make_point_notify_watchers(EsePoint *point);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EsePoint
 * 
 * Handles cleanup when a Lua proxy table for an EsePoint is garbage collected.
 * Only frees the underlying EsePoint if it has no C-side references.
 * 
 * @param L Lua state
 * @return 0 (no return values)
 */
static int _ese_point_lua_gc(lua_State *L) {
    // Get from userdata
    EsePoint **ud = (EsePoint **)luaL_testudata(L, 1, POINT_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EsePoint *point = *ud;
    if (point) {
        // If lua_ref == LUA_NOREF, there are no more references to this point, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this point was referenced from C and should not be freed.
        if (ese_point_get_lua_ref(point) == LUA_NOREF) {
            ese_point_destroy(point);
        }
    }

    return 0;
}

/**
 * @brief Lua __index metamethod for EsePoint property access
 * 
 * Provides read access to point properties (x, y) from Lua. When a Lua script
 * accesses point.x or point.y, this function is called to retrieve the values.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid properties, 0 for invalid)
 */
static int _ese_point_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_POINT_INDEX);
    EsePoint *point = ese_point_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!point || !key) {
        profile_cancel(PROFILE_LUA_POINT_INDEX);
        return 0;
    }

    if (strcmp(key, "x") == 0) {
        lua_pushnumber(L, ese_point_get_x(point));
        profile_stop(PROFILE_LUA_POINT_INDEX, "point_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(L, ese_point_get_y(point));
        profile_stop(PROFILE_LUA_POINT_INDEX, "point_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "toJSON") == 0) {
        lua_pushlightuserdata(L, point);
        lua_pushcclosure(L, _ese_point_lua_to_json, 1);
        profile_stop(PROFILE_LUA_POINT_INDEX, "point_lua_index (method)");
        return 1;
    }

    profile_stop(PROFILE_LUA_POINT_INDEX, "point_lua_index (invalid)");
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EsePoint property assignment
 * 
 * Provides write access to point properties (x, y) from Lua. When a Lua script
 * assigns to point.x or point.y, this function is called to update the values
 * and notify any registered watchers of the change.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_point_lua_newindex(lua_State *L) {
    profile_start(PROFILE_LUA_POINT_NEWINDEX);
    EsePoint *point = ese_point_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!point || !key) {
        profile_cancel(PROFILE_LUA_POINT_INDEX);
        return 0;
    }

    if (strcmp(key, "x") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_POINT_NEWINDEX);
            return luaL_error(L, "point.x must be a number");
        }
        ese_point_set_x(point, (float)lua_tonumber(L, 3));
        _ese_point_make_point_notify_watchers(point);
        profile_stop(PROFILE_LUA_POINT_NEWINDEX, "point_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "y") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_POINT_NEWINDEX);
            return luaL_error(L, "point.y must be a number");
        }
        ese_point_set_y(point, (float)lua_tonumber(L, 3));
        _ese_point_make_point_notify_watchers(point);
        profile_stop(PROFILE_LUA_POINT_NEWINDEX, "point_lua_newindex (setter)");
        return 0;
    }
    profile_stop(PROFILE_LUA_POINT_NEWINDEX, "point_lua_newindex (invalid)");
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __tostring metamethod for EsePoint string representation
 * 
 * Converts an EsePoint to a human-readable string for debugging and display.
 * The format includes the memory address and current x,y coordinates.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
static int _ese_point_lua_tostring(lua_State *L) {
    EsePoint *point = ese_point_lua_get(L, 1);

    if (!point) {
        lua_pushstring(L, "Point: (invalid)");
        return 1;
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "(x=%.3f, y=%.3f)", ese_point_get_x(point), ese_point_get_y(point));
    lua_pushstring(L, buf);

    return 1;
}

// Lua constructors
/**
 * @brief Lua constructor function for creating new EsePoint instances
 * 
 * Creates a new EsePoint from Lua with specified x,y coordinates. This function
 * is called when Lua code executes `Point.new(x, y)`. It validates the arguments,
 * creates the underlying EsePoint, and returns a proxy table that provides
 * access to the point's properties and methods.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_point_lua_new(lua_State *L) {
    profile_start(PROFILE_LUA_POINT_NEW);

    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 2) {
        profile_cancel(PROFILE_LUA_POINT_NEW);
        return luaL_error(L, "Point.new(number, number) takes 2 arguments");
    }
    
    if (lua_type(L, 1) != LUA_TNUMBER) {
        profile_cancel(PROFILE_LUA_POINT_NEW);
        return luaL_error(L, "Point.new(number, number) arguments must be numbers");
    }
    if (lua_type(L, 2) != LUA_TNUMBER) {
        profile_cancel(PROFILE_LUA_POINT_NEW);  
        return luaL_error(L, "Point.new(number, number) arguments must be numbers");
    }

    float x = (float)lua_tonumber(L, 1);
    float y = (float)lua_tonumber(L, 2);

    // Create the point
    EsePoint *point = _ese_point_make();
    ese_point_set_x(point, x);
    ese_point_set_y(point, y);
    ese_point_set_state(point, L);

    // Create userdata directly
    EsePoint **ud = (EsePoint **)lua_newuserdata(L, sizeof(EsePoint *));
    *ud = point;

    // Attach metatable
    luaL_getmetatable(L, POINT_PROXY_META);
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_POINT_NEW, "point_lua_new");
    return 1;
}

/**
 * @brief Lua constructor function for creating EsePoint at origin
 * 
 * Creates a new EsePoint at the origin (0,0) from Lua. This function is called
 * when Lua code executes `Point.zero()`. It's a convenience constructor for
 * creating points at the default position.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_point_lua_zero(lua_State *L) {
    profile_start(PROFILE_LUA_POINT_ZERO);

    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 0) {
        profile_cancel(PROFILE_LUA_POINT_NEW);
        return luaL_error(L, "Point.zero() takes 0 arguments");
    }
    
    // Create the point using the standard creation function
    EsePoint *point = _ese_point_make();
    ese_point_set_state(point, L);

    // Create userdata directly
    EsePoint **ud = (EsePoint **)lua_newuserdata(L, sizeof(EsePoint *));
    *ud = point;

    // Attach metatable
    luaL_getmetatable(L, POINT_PROXY_META);
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_POINT_ZERO, "point_lua_zero");
    return 1;
}

/**
 * @brief Lua static method for calculating distance between two points
 * 
 * Calculates the Euclidean distance between two EsePoint objects. This function
 * is called when Lua code executes `Point.distance(point1, point2)`.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the distance)
 */
static int _ese_point_lua_distance(lua_State *L) {
    profile_start(PROFILE_LUA_POINT_ZERO);

    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 2) {
        profile_cancel(PROFILE_LUA_POINT_ZERO);
        return luaL_error(L, "Point.distance(point, point) takes 2 arguments");
    }

    EsePoint *point1 = ese_point_lua_get(L, 1);
    EsePoint *point2 = ese_point_lua_get(L, 2);

    if (!point1 || !point2) {
        profile_cancel(PROFILE_LUA_POINT_ZERO);
        return luaL_error(L, "Point.distance(point, point) arguments must be points");
    }

    float distance = ese_point_distance(point1, point2);
    lua_pushnumber(L, (double)distance);

    profile_stop(PROFILE_LUA_POINT_ZERO, "point_lua_distance");
    return 1;
}

// Lua JSON methods
/**
 * @brief Lua static method for creating EsePoint from JSON string
 *
 * Creates a new EsePoint from a JSON string. This function is called when Lua code
 * executes `Point.fromJSON(json_string)`. It parses the JSON string, validates it
 * contains point data, and creates a new point with the specified coordinates.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_point_lua_from_json(lua_State *L) {
    profile_start(PROFILE_LUA_POINT_FROM_JSON);

    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 1) {
        profile_cancel(PROFILE_LUA_POINT_FROM_JSON);
        return luaL_error(L, "Point.fromJSON(string) takes 1 argument");
    }

    if (lua_type(L, 1) != LUA_TSTRING) {
        profile_cancel(PROFILE_LUA_POINT_FROM_JSON);
        return luaL_error(L, "Point.fromJSON(string) argument must be a string");
    }

    const char *json_str = lua_tostring(L, 1);

    // Parse JSON string
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        log_error("POINT", "Point.fromJSON: failed to parse JSON string: %s", json_str ? json_str : "NULL");
        profile_cancel(PROFILE_LUA_POINT_FROM_JSON);
        return luaL_error(L, "Point.fromJSON: invalid JSON string");
    }

    // Get the current engine from Lua registry
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (!engine) {
        cJSON_Delete(json);
        profile_cancel(PROFILE_LUA_POINT_FROM_JSON);
        return luaL_error(L, "Point.fromJSON: no engine available");
    }

    // Use the existing deserialization function
    EsePoint *point = ese_point_deserialize(engine, json);
    cJSON_Delete(json); // Clean up JSON object

    if (!point) {
        profile_cancel(PROFILE_LUA_POINT_FROM_JSON);
        return luaL_error(L, "Point.fromJSON: failed to deserialize point");
    }

    ese_point_lua_push(point);

    profile_stop(PROFILE_LUA_POINT_FROM_JSON, "point_lua_from_json");
    return 1;
}

/**
 * @brief Lua instance method for converting EsePoint to JSON string
 *
 * Converts an EsePoint to a JSON string representation. This function is called when
 * Lua code executes `point:toJSON()`. It serializes the point's coordinates to JSON
 * format and returns the string.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the JSON string)
 */
static int _ese_point_lua_to_json(lua_State *L) {
    profile_start(PROFILE_LUA_POINT_TO_JSON);

    EsePoint *point = ese_point_lua_get(L, 1);
    if (!point) {
        profile_cancel(PROFILE_LUA_POINT_TO_JSON);
        return luaL_error(L, "Point:toJSON() called on invalid point");
    }

    // Serialize point to JSON
    cJSON *json = ese_point_serialize(point);
    if (!json) {
        profile_cancel(PROFILE_LUA_POINT_TO_JSON);
        return luaL_error(L, "Point:toJSON() failed to serialize point");
    }

    // Convert JSON to string
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json); // Clean up JSON object

    if (!json_str) {
        profile_cancel(PROFILE_LUA_POINT_TO_JSON);
        return luaL_error(L, "Point:toJSON() failed to convert to string");
    }

    // Push the JSON string onto the stack (lua_pushstring makes a copy)
    lua_pushstring(L, json_str);

    // Clean up the string (cJSON_PrintUnformatted uses malloc)
    // Note: We free here, but lua_pushstring should have made a copy
    free(json_str); // cJSON doesnt use the memory manager.

    profile_stop(PROFILE_LUA_POINT_TO_JSON, "point_lua_to_json");
    return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Internal Lua initialization function for EsePoint
 * 
 * Sets up the Lua metatable and global Point table with constructors and methods.
 * This function is called by the public ese_point_lua_init function.
 * 
 * @param engine EseLuaEngine pointer where the EsePoint type will be registered
 */
void _ese_point_lua_init(EseLuaEngine *engine) {
    // Create metatable
    lua_engine_new_object_meta(engine, POINT_PROXY_META, 
        _ese_point_lua_index, 
        _ese_point_lua_newindex, 
        _ese_point_lua_gc, 
        _ese_point_lua_tostring);
    
    // Create global Point table with functions
    const char *keys[] = {"new", "zero", "distance", "fromJSON"};
    lua_CFunction functions[] = {_ese_point_lua_new, _ese_point_lua_zero, 
                                _ese_point_lua_distance, _ese_point_lua_from_json};
    lua_engine_new_object(engine, "Point", 4, keys, functions);
}
