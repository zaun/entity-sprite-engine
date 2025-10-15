#include <string.h>
#include <stdio.h>
#include <math.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/types.h"
#include "types/arc.h"
#include "types/arc_lua.h"
#include "vendor/json/cJSON.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Lua metamethods
static int _ese_arc_lua_gc(lua_State *L);
static int _ese_arc_lua_index(lua_State *L);
static int _ese_arc_lua_newindex(lua_State *L);
static int _ese_arc_lua_tostring(lua_State *L);

// Lua constructors
static int _ese_arc_lua_new(lua_State *L);
static int _ese_arc_lua_zero(lua_State *L);

// Lua methods
static int _ese_arc_lua_contains_point(lua_State *L);
static int _ese_arc_lua_intersects_rect(lua_State *L);
static int _ese_arc_lua_get_length(lua_State *L);
static int _ese_arc_lua_get_point_at_angle(lua_State *L);
static int _ese_arc_lua_to_json(lua_State *L);
static int _ese_arc_lua_from_json(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseArc
 * 
 * Handles cleanup when a Lua proxy table for an EseArc is garbage collected.
 * Only frees the underlying EseArc if it has no C-side references.
 * 
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static int _ese_arc_lua_gc(lua_State *L) {
    // Get from userdata
    EseArc **ud = (EseArc **)luaL_testudata(L, 1, ARC_META);
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseArc *arc = *ud;
    if (arc) {
        // If lua_ref == LUA_NOREF, there are no more references to this arc, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this arc was referenced from C and should not be freed.
        if (ese_arc_get_lua_ref(arc) == LUA_NOREF) {
            ese_arc_destroy(arc);
        }
    }

    return 0;
}

/**
 * @brief Lua __index metamethod for EseArc property access
 * 
 * Provides read access to arc properties (x, y, radius, start_angle, end_angle) from Lua.
 * When a Lua script accesses arc.x, arc.y, etc., this function is called to retrieve the values.
 * Also provides access to methods like contains_point, intersects_rect, get_length, and get_point_at_angle.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid properties/methods, 0 for invalid)
 */
static int _ese_arc_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_ARC_INDEX);
    EseArc *arc = ese_arc_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!arc || !key) {
        profile_cancel(PROFILE_LUA_ARC_INDEX);
        return 0;
    }

    if (strcmp(key, "x") == 0) {
        lua_pushnumber(L, ese_arc_get_x(arc));
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(L, ese_arc_get_y(arc));
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "radius") == 0) {
        lua_pushnumber(L, ese_arc_get_radius(arc));
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "start_angle") == 0) {
        lua_pushnumber(L, ese_arc_get_start_angle(arc));
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "end_angle") == 0) {
        lua_pushnumber(L, ese_arc_get_end_angle(arc));
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "contains_point") == 0) {
        lua_pushlightuserdata(L, arc);
        lua_pushcclosure(L, _ese_arc_lua_contains_point, 1);
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (method)");
        return 1;
    } else if (strcmp(key, "intersects_rect") == 0) {
        lua_pushlightuserdata(L, arc);
        lua_pushcclosure(L, _ese_arc_lua_intersects_rect, 1);
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (method)");
        return 1;
    } else if (strcmp(key, "get_length") == 0) {
        lua_pushlightuserdata(L, arc);
        lua_pushcclosure(L, _ese_arc_lua_get_length, 1);
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (method)");
        return 1;
    } else if (strcmp(key, "get_point_at_angle") == 0) {
        lua_pushlightuserdata(L, arc);
        lua_pushcclosure(L, _ese_arc_lua_get_point_at_angle, 1);
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (method)");
        return 1;
    } else if (strcmp(key, "toJSON") == 0) {
        lua_pushlightuserdata(L, arc);
        lua_pushcclosure(L, _ese_arc_lua_to_json, 1);
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (method)");
        return 1;
    }
    profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (invalid)");
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseArc property assignment
 * 
 * Provides write access to arc properties (x, y, radius, start_angle, end_angle) from Lua.
 * When a Lua script assigns to arc.x, arc.y, etc., this function is called to update the values.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_arc_lua_newindex(lua_State *L) {
    profile_start(PROFILE_LUA_ARC_NEWINDEX);
    EseArc *arc = ese_arc_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!arc || !key) {
        profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
        return 0;
    }

    if (strcmp(key, "x") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
            return luaL_error(L, "arc.x must be a number");
        }
        ese_arc_set_x(arc, (float)lua_tonumber(L, 3));
        profile_stop(PROFILE_LUA_ARC_NEWINDEX, "ese_arc_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "y") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
            return luaL_error(L, "arc.y must be a number");
        }
        ese_arc_set_y(arc, (float)lua_tonumber(L, 3));
        profile_stop(PROFILE_LUA_ARC_NEWINDEX, "ese_arc_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "radius") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
            return luaL_error(L, "arc.radius must be a number");
        }
        ese_arc_set_radius(arc, (float)lua_tonumber(L, 3));
        profile_stop(PROFILE_LUA_ARC_NEWINDEX, "ese_arc_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "start_angle") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
            return luaL_error(L, "arc.start_angle must be a number");
        }
        ese_arc_set_start_angle(arc, (float)lua_tonumber(L, 3));
        profile_stop(PROFILE_LUA_ARC_NEWINDEX, "ese_arc_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "end_angle") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
            return luaL_error(L, "arc.end_angle must be a number");
        }
        ese_arc_set_end_angle(arc, (float)lua_tonumber(L, 3));
        profile_stop(PROFILE_LUA_ARC_NEWINDEX, "ese_arc_lua_newindex (setter)");
        return 0;
    }
    profile_stop(PROFILE_LUA_ARC_NEWINDEX, "ese_arc_lua_newindex (invalid)");
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __tostring metamethod for EseArc string representation
 * 
 * Converts an EseArc to a human-readable string for debugging and display.
 * The format includes the memory address and current x,y,radius,start_angle,end_angle values.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
static int _ese_arc_lua_tostring(lua_State *L) {
    EseArc *arc = ese_arc_lua_get(L, 1);

    if (!arc) {
        lua_pushstring(L, "Arc: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Arc: %p (x=%.2f, y=%.2f, r=%.2f, start=%.2f, end=%.2f)", 
             (void*)arc, ese_arc_get_x(arc), ese_arc_get_y(arc), ese_arc_get_radius(arc), 
             ese_arc_get_start_angle(arc), ese_arc_get_end_angle(arc));
    lua_pushstring(L, buf);

    return 1;
}

// Lua constructors
/**
 * @brief Lua constructor function for creating new EseArc instances
 * 
 * Creates a new EseArc from Lua with specified center, radius, and angle parameters.
 * This function is called when Lua code executes `Arc.new(x, y, radius, start_angle, end_angle)`.
 * It validates the arguments, creates the underlying EseArc, and returns a proxy table
 * that provides access to the arc's properties and methods.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_arc_lua_new(lua_State *L) {
    profile_start(PROFILE_LUA_ARC_NEW);
    float x = 0.0f, y = 0.0f, radius = 1.0f, start_angle = 0.0f, end_angle = 2.0f * M_PI;

    int n_args = lua_gettop(L);
    if (n_args == 5) {
        if (lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER || lua_type(L, 3) != LUA_TNUMBER || 
            lua_type(L, 4) != LUA_TNUMBER || lua_type(L, 5) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_ARC_NEW);
            return luaL_error(L, "all arguments must be numbers");
        }
        x = (float)lua_tonumber(L, 1);
        y = (float)lua_tonumber(L, 2);
        radius = (float)lua_tonumber(L, 3);
        start_angle = (float)lua_tonumber(L, 4);
        end_angle = (float)lua_tonumber(L, 5);
    } else if (n_args != 0) {
        profile_cancel(PROFILE_LUA_ARC_NEW);
        return luaL_error(L, "new() takes 0 or 5 arguments (x, y, radius, start_angle, end_angle)");
    }

    // Get the engine from the Lua state
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (!engine) {
        profile_cancel(PROFILE_LUA_ARC_NEW);
        return luaL_error(L, "Arc.new: no engine available");
    }

    // Create the arc using the standard creation function
    EseArc *arc = ese_arc_create(engine);
    ese_arc_set_x(arc, x);
    ese_arc_set_y(arc, y);
    ese_arc_set_radius(arc, radius);
    ese_arc_set_start_angle(arc, start_angle);
    ese_arc_set_end_angle(arc, end_angle);
    
    // Create userdata directly
    EseArc **ud = (EseArc **)lua_newuserdata(L, sizeof(EseArc *));
    *ud = arc;

    // Attach metatable
    luaL_getmetatable(L, ARC_META);
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_ARC_NEW, "ese_arc_lua_new");
    return 1;
}

/**
 * @brief Lua constructor function for creating EseArc at origin
 * 
 * Creates a new EseArc at the origin (0,0) with unit radius and full circle angles from Lua.
 * This function is called when Lua code executes `Arc.zero()`.
 * It's a convenience constructor for creating arcs at the default position and parameters.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_arc_lua_zero(lua_State *L) {
    profile_start(PROFILE_LUA_ARC_ZERO);
    
    int n_args = lua_gettop(L);
    if (n_args != 0) {
        profile_cancel(PROFILE_LUA_ARC_ZERO);
        return luaL_error(L, "zero() takes no arguments");
    }
    
    // Get the engine from the Lua state
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (!engine) {
        profile_cancel(PROFILE_LUA_ARC_ZERO);
        return luaL_error(L, "Arc.zero: no engine available");
    }
    
    // Create the arc using the standard creation function
    EseArc *arc = ese_arc_create(engine);
    
    // Create userdata directly
    EseArc **ud = (EseArc **)lua_newuserdata(L, sizeof(EseArc *));
    *ud = arc;

    // Attach metatable
    luaL_getmetatable(L, ARC_META);
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_ARC_ZERO, "ese_arc_lua_zero");
    return 1;
}

// Lua methods
/**
 * @brief Lua method for testing if a point is contained within the arc
 * 
 * Tests whether the specified (x,y) point lies within the arc bounds using
 * distance and angle calculations with optional tolerance.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - boolean result)
 */
static int _ese_arc_lua_contains_point(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args < 3 || n_args > 4) {
        return luaL_error(L, "arc:contains_point(x, y [, tolerance]) requires 2 or 3 arguments");
    }
    
    if (lua_type(L, 2) != LUA_TNUMBER || lua_type(L, 3) != LUA_TNUMBER) {
        return luaL_error(L, "arc:contains_point(x, y [, tolerance]) requires numbers");
    }
    
    if (n_args == 4 && lua_type(L, 4) != LUA_TNUMBER) {
        return luaL_error(L, "arc:contains_point(x, y [, tolerance]) tolerance must be a number");
    }

    EseArc *arc = (EseArc *)lua_touserdata(L, lua_upvalueindex(1));
    if (!arc) {
        return luaL_error(L, "Invalid EseArc object in contains_point method");
    }
    
    float x = (float)lua_tonumber(L, 2);
    float y = (float)lua_tonumber(L, 3);
    float tolerance = 0.1f;
    
    if (n_args == 4) {
        tolerance = (float)lua_tonumber(L, 4);
    }
    
    lua_pushboolean(L, ese_arc_contains_point(arc, x, y, tolerance));
    return 1;
}

/**
 * @brief Lua method for testing arc-rectangle intersection
 * 
 * Tests whether the arc intersects with a given rectangle using
 * bounding box intersection algorithms.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - boolean result)
 */
static int _ese_arc_lua_intersects_rect(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 2) {
        return luaL_error(L, "arc:intersects_rect(rect) requires exactly 1 argument");
    }

    EseArc *arc = (EseArc *)lua_touserdata(L, lua_upvalueindex(1));
    if (!arc) {
        return luaL_error(L, "Invalid EseArc object in intersects_rect method");
    }
    
    EseRect *rect = ese_rect_lua_get(L, 2);
    if (!rect) {
        return luaL_error(L, "arc:intersects_rect(rect) argument must be an Rect object");
    }
    
    lua_pushboolean(L, ese_arc_intersects_rect(arc, rect));
    return 1;
}

/**
 * @brief Lua method for calculating arc length
 * 
 * Calculates and returns the arc length using the formula: radius * angle_difference.
 * Handles angle wrapping automatically.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the length value)
 */
static int _ese_arc_lua_get_length(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 1) {
        return luaL_error(L, "arc:get_length() takes no arguments");
    }

    EseArc *arc = (EseArc *)lua_touserdata(L, lua_upvalueindex(1));
    if (!arc) {
        return luaL_error(L, "Invalid EseArc object in get_length method");
    }
    
    lua_pushnumber(L, ese_arc_get_length(arc));
    return 1;
}

/**
 * @brief Lua method for getting a point along the arc at a specified angle
 * 
 * Calculates the coordinates of a point that lies along the arc at the given angle.
 * Returns success status and x,y coordinates if the angle is within the arc's range.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (3 for success: true, x, y; 1 for failure: false)
 */
static int _ese_arc_lua_get_point_at_angle(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 2) {
        return luaL_error(L, "arc:get_point_at_angle(angle) requires exactly 1 argument");
    }

    EseArc *arc = (EseArc *)lua_touserdata(L, lua_upvalueindex(1));
    if (!arc) {
        return luaL_error(L, "Invalid EseArc object in get_point_at_angle method");
    }
    
    if (lua_type(L, 2) != LUA_TNUMBER) {
        return luaL_error(L, "arc:get_point_at_angle(angle) requires a number");
    }
    
    float angle = (float)lua_tonumber(L, 2);
    float x, y;
    bool success = ese_arc_get_point_at_angle(arc, angle, &x, &y);
    
    if (success) {
        lua_pushboolean(L, true);
        lua_pushnumber(L, x);
        lua_pushnumber(L, y);
        return 3;
    } else {
        lua_pushboolean(L, false);
        return 1;
    }
}

/**
 * @brief Lua instance method for converting EseArc to JSON string
 *
 * Converts an EseArc to a JSON string representation. This function is called when
 * Lua code executes `arc:toJSON()`. It serializes the arc's fields to JSON and
 * returns the string.
 */
static int _ese_arc_lua_to_json(lua_State *L) {
    EseArc *arc = ese_arc_lua_get(L, 1);
    if (!arc) {
        return luaL_error(L, "Arc:toJSON() called on invalid arc");
    }

    cJSON *json = ese_arc_serialize(arc);
    if (!json) {
        return luaL_error(L, "Arc:toJSON() failed to serialize arc");
    }

    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!json_str) {
        return luaL_error(L, "Arc:toJSON() failed to convert to string");
    }

    lua_pushstring(L, json_str);
    free(json_str);
    return 1;
}

/**
 * @brief Lua static method for creating EseArc from JSON string
 *
 * Creates a new EseArc from a JSON string. Called as `Arc.fromJSON(json_string)`.
 */
static int _ese_arc_lua_from_json(lua_State *L) {
    int argc = lua_gettop(L);
    if (argc != 1) {
        return luaL_error(L, "Arc.fromJSON(string) takes 1 argument");
    }
    if (lua_type(L, 1) != LUA_TSTRING) {
        return luaL_error(L, "Arc.fromJSON(string) argument must be a string");
    }

    const char *json_str = lua_tostring(L, 1);
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        log_error("ARC", "Arc.fromJSON: failed to parse JSON string: %s", json_str ? json_str : "NULL");
        return luaL_error(L, "Arc.fromJSON: invalid JSON string");
    }

    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (!engine) {
        cJSON_Delete(json);
        return luaL_error(L, "Arc.fromJSON: no engine available");
    }

    EseArc *arc = ese_arc_deserialize(engine, json);
    cJSON_Delete(json);
    if (!arc) {
        return luaL_error(L, "Arc.fromJSON: failed to deserialize arc");
    }

    ese_arc_lua_push(arc);
    return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Internal Lua initialization function for EseArc
 * 
 * @details This function is called by the public ese_arc_lua_init function
 *          to set up all the private Lua metamethods and methods for EseArc.
 * 
 * @param engine EseLuaEngine pointer where the EseArc type will be registered
 */
void _ese_arc_lua_init(EseLuaEngine *engine) {
    // Create metatable
    lua_engine_new_object_meta(engine, ARC_META, 
        _ese_arc_lua_index, 
        _ese_arc_lua_newindex, 
        _ese_arc_lua_gc, 
        _ese_arc_lua_tostring);
    
    // Create global Arc table with functions
    const char *keys[] = {"new", "zero", "fromJSON"};
    lua_CFunction functions[] = {_ese_arc_lua_new, _ese_arc_lua_zero, _ese_arc_lua_from_json};
    lua_engine_new_object(engine, "Arc", 3, keys, functions);
}
