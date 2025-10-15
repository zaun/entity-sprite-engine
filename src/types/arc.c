#include <string.h>
#include <stdio.h>
#include <math.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/types.h"
#include "vendor/json/cJSON.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ========================================
// STRUCT DEFINITION
// ========================================

/**
 * @brief Represents an arc with floating-point center, radius, and angle range.
 *
 * @details This structure stores an arc defined by center point, radius, and start/end angles.
 */
struct EseArc {
    float x;           /** The x-coordinate of the arc's center */
    float y;           /** The y-coordinate of the arc's center */
    float radius;      /** The radius of the arc */
    float start_angle; /** The start angle of the arc in radians */
    float end_angle;   /** The end angle of the arc in radians */

    lua_State *state;  /** Lua State this EseArc belongs to */
    int lua_ref;       /** Lua registry reference to its own proxy table */
    int lua_ref_count; /** Number of times this arc has been referenced in C */
};

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseArc *_ese_arc_make(void);

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

// Core helpers
/**
 * @brief Creates a new EseArc instance with default values
 * 
 * Allocates memory for a new EseArc and initializes all fields to safe defaults.
 * The arc starts at origin (0,0) with unit radius, full circle angles, and no Lua state or references.
 * 
 * @return Pointer to the newly created EseArc, or NULL on allocation failure
 */
static EseArc *_ese_arc_make() {
    EseArc *arc = (EseArc *)memory_manager.malloc(sizeof(EseArc), MMTAG_ARC);
    arc->x = 0.0f;
    arc->y = 0.0f;
    arc->radius = 1.0f;
    arc->start_angle = 0.0f;
    arc->end_angle = 2.0f * M_PI;
    arc->state = NULL;
    arc->lua_ref = LUA_NOREF;
    arc->lua_ref_count = 0;
    return arc;
}

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
    EseArc **ud = (EseArc **)luaL_testudata(L, 1, "ArcMeta");
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseArc *arc = *ud;
    if (arc) {
        // If lua_ref == LUA_NOREF, there are no more references to this arc, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this arc was referenced from C and should not be freed.
        if (arc->lua_ref == LUA_NOREF) {
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
        lua_pushnumber(L, arc->x);
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(L, arc->y);
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "radius") == 0) {
        lua_pushnumber(L, arc->radius);
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "start_angle") == 0) {
        lua_pushnumber(L, arc->start_angle);
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "end_angle") == 0) {
        lua_pushnumber(L, arc->end_angle);
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
        arc->x = (float)lua_tonumber(L, 3);
        profile_stop(PROFILE_LUA_ARC_NEWINDEX, "ese_arc_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "y") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
            return luaL_error(L, "arc.y must be a number");
        }
        arc->y = (float)lua_tonumber(L, 3);
        profile_stop(PROFILE_LUA_ARC_NEWINDEX, "ese_arc_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "radius") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
            return luaL_error(L, "arc.radius must be a number");
        }
        arc->radius = (float)lua_tonumber(L, 3);
        profile_stop(PROFILE_LUA_ARC_NEWINDEX, "ese_arc_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "start_angle") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
            return luaL_error(L, "arc.start_angle must be a number");
        }
        arc->start_angle = (float)lua_tonumber(L, 3);
        profile_stop(PROFILE_LUA_ARC_NEWINDEX, "ese_arc_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "end_angle") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
            return luaL_error(L, "arc.end_angle must be a number");
        }
        arc->end_angle = (float)lua_tonumber(L, 3);
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
             (void*)arc, arc->x, arc->y, arc->radius, arc->start_angle, arc->end_angle);
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

    // Create the arc using the standard creation function
    EseArc *arc = _ese_arc_make();
    arc->x = x;
    arc->y = y;
    arc->radius = radius;
    arc->start_angle = start_angle;
    arc->end_angle = end_angle;
    arc->state = L;
    
    // Create userdata directly
    EseArc **ud = (EseArc **)lua_newuserdata(L, sizeof(EseArc *));
    *ud = arc;

    // Attach metatable
    luaL_getmetatable(L, "ArcMeta");
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
    
    // Create the arc using the standard creation function
    EseArc *arc = _ese_arc_make();  // We'll set the state manually
    arc->state = L;
    
    // Create userdata directly
    EseArc **ud = (EseArc **)lua_newuserdata(L, sizeof(EseArc *));
    *ud = arc;

    // Attach metatable
    luaL_getmetatable(L, "ArcMeta");
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
// ACCESSOR FUNCTIONS
// ========================================

/**
 * @brief Gets the x-coordinate of the arc's center
 *
 * @param arc Pointer to the EseArc object
 * @return The x-coordinate of the arc's center
 */
float ese_arc_get_x(const EseArc *arc) {
    return arc->x;
}

/**
 * @brief Sets the x-coordinate of the arc's center
 *
 * @param arc Pointer to the EseArc object
 * @param x The new x-coordinate value
 */
void ese_arc_set_x(EseArc *arc, float x) {
    log_assert("ARC", arc != NULL, "ese_arc_set_x: arc cannot be NULL");
    arc->x = x;
}

/**
 * @brief Gets the y-coordinate of the arc's center
 *
 * @param arc Pointer to the EseArc object
 * @return The y-coordinate of the arc's center
 */
float ese_arc_get_y(const EseArc *arc) {
    return arc->y;
}

/**
 * @brief Sets the y-coordinate of the arc's center
 *
 * @param arc Pointer to the EseArc object
 * @param y The new y-coordinate value
 */
void ese_arc_set_y(EseArc *arc, float y) {
    log_assert("ARC", arc != NULL, "ese_arc_set_y: arc cannot be NULL");
    arc->y = y;
}

/**
 * @brief Gets the radius of the arc
 *
 * @param arc Pointer to the EseArc object
 * @return The radius of the arc
 */
float ese_arc_get_radius(const EseArc *arc) {
    return arc->radius;
}

/**
 * @brief Sets the radius of the arc
 *
 * @param arc Pointer to the EseArc object
 * @param radius The new radius value
 */
void ese_arc_set_radius(EseArc *arc, float radius) {
    log_assert("ARC", arc != NULL, "ese_arc_set_radius: arc cannot be NULL");
    arc->radius = radius;
}

/**
 * @brief Gets the start angle of the arc in radians
 *
 * @param arc Pointer to the EseArc object
 * @return The start angle of the arc in radians
 */
float ese_arc_get_start_angle(const EseArc *arc) {
    return arc->start_angle;
}

/**
 * @brief Sets the start angle of the arc in radians
 *
 * @param arc Pointer to the EseArc object
 * @param start_angle The new start angle value in radians
 */
void ese_arc_set_start_angle(EseArc *arc, float start_angle) {
    log_assert("ARC", arc != NULL, "ese_arc_set_start_angle: arc cannot be NULL");
    arc->start_angle = start_angle;
}

/**
 * @brief Gets the end angle of the arc in radians
 *
 * @param arc Pointer to the EseArc object
 * @return The end angle of the arc in radians
 */
float ese_arc_get_end_angle(const EseArc *arc) {
    return arc->end_angle;
}

/**
 * @brief Sets the end angle of the arc in radians
 *
 * @param arc Pointer to the EseArc object
 * @param end_angle The new end angle value in radians
 */
void ese_arc_set_end_angle(EseArc *arc, float end_angle) {
    log_assert("ARC", arc != NULL, "ese_arc_set_end_angle: arc cannot be NULL");
    arc->end_angle = end_angle;
}

/**
 * @brief Gets the Lua state associated with the arc
 *
 * @param arc Pointer to the EseArc object
 * @return The Lua state associated with the arc
 */
lua_State *ese_arc_get_state(const EseArc *arc) {
    return arc->state;
}

/**
 * @brief Gets the Lua registry reference for the arc
 *
 * @param arc Pointer to the EseArc object
 * @return The Lua registry reference for the arc
 */
int ese_arc_get_lua_ref(const EseArc *arc) {
    return arc->lua_ref;
}

/**
 * @brief Gets the Lua reference count for the arc
 *
 * @param arc Pointer to the EseArc object
 * @return The Lua reference count for the arc
 */
int ese_arc_get_lua_ref_count(const EseArc *arc) {
    return arc->lua_ref_count;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseArc *ese_arc_create(EseLuaEngine *engine) {
    EseArc *arc = _ese_arc_make();
    arc->state = engine->runtime;
    return arc;
}

EseArc *ese_arc_copy(const EseArc *source) {
    if (source == NULL) {
        return NULL;
    }

    EseArc *copy = (EseArc *)memory_manager.malloc(sizeof(EseArc), MMTAG_ARC);
    copy->x = source->x;
    copy->y = source->y;
    copy->radius = source->radius;
    copy->start_angle = source->start_angle;
    copy->end_angle = source->end_angle;
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
    copy->lua_ref_count = 0;
    return copy;
}

void ese_arc_destroy(EseArc *arc) {
    if (!arc) return;
    
    if (arc->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(arc);
    } else {
        ese_arc_unref(arc);
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

// Lua integration
void ese_arc_lua_init(EseLuaEngine *engine) {
    if (luaL_newmetatable(engine->runtime, "ArcMeta")) {
        log_debug("LUA", "Adding entity ArcMeta to engine");
        lua_pushstring(engine->runtime, "ArcMeta");
        lua_setfield(engine->runtime, -2, "__name");
        lua_pushcfunction(engine->runtime, _ese_arc_lua_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _ese_arc_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushcfunction(engine->runtime, _ese_arc_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");
        lua_pushcfunction(engine->runtime, _ese_arc_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
    
    // Create global EseArc table with constructor
    lua_getglobal(engine->runtime, "Arc");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);
        log_debug("LUA", "Creating global EseArc table");
        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _ese_arc_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_pushcfunction(engine->runtime, _ese_arc_lua_zero);
        lua_setfield(engine->runtime, -2, "zero");
        lua_pushcfunction(engine->runtime, _ese_arc_lua_from_json);
        lua_setfield(engine->runtime, -2, "fromJSON");
        lua_setglobal(engine->runtime, "Arc");
    } else {
        lua_pop(engine->runtime, 1);
    }
}

void ese_arc_lua_push(EseArc *arc) {
    log_assert("ARC", arc, "ese_arc_lua_push called with NULL arc");

    if (arc->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseArc **ud = (EseArc **)lua_newuserdata(arc->state, sizeof(EseArc *));
        *ud = arc;

        // Attach metatable
        luaL_getmetatable(arc->state, "ArcMeta");
        lua_setmetatable(arc->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(arc->state, LUA_REGISTRYINDEX, arc->lua_ref);
    }
}

EseArc *ese_arc_lua_get(lua_State *L, int idx) {
    log_assert("ARC", L, "ese_arc_lua_get called with NULL Lua state");
    
    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseArc **ud = (EseArc **)luaL_testudata(L, idx, "ArcMeta");
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_arc_ref(EseArc *arc) {
    log_assert("ARC", arc, "ese_arc_ref called with NULL arc");
    
    if (arc->lua_ref == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseArc **ud = (EseArc **)lua_newuserdata(arc->state, sizeof(EseArc *));
        *ud = arc;

        // Attach metatable
        luaL_getmetatable(arc->state, "ArcMeta");
        lua_setmetatable(arc->state, -2);

        // Store hard reference to prevent garbage collection
        arc->lua_ref = luaL_ref(arc->state, LUA_REGISTRYINDEX);
        arc->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        arc->lua_ref_count++;
    }

    profile_count_add("ese_arc_ref_count");
}

void ese_arc_unref(EseArc *arc) {
    if (!arc) return;
    
    if (arc->lua_ref != LUA_NOREF && arc->lua_ref_count > 0) {
        arc->lua_ref_count--;
        
        if (arc->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(arc->state, LUA_REGISTRYINDEX, arc->lua_ref);
            arc->lua_ref = LUA_NOREF;
        }
    }

    profile_count_add("ese_arc_unref_count");
}

// Mathematical operations
bool ese_arc_contains_point(const EseArc *arc, float x, float y, float tolerance) {
    if (!arc) return false;
    
    // Calculate distance from point to arc center
    float dx = x - arc->x;
    float dy = y - arc->y;
    float distance = sqrtf(dx * dx + dy * dy);
    
    // Check if point is within radius tolerance
    if (fabsf(distance - arc->radius) > tolerance) {
        return false;
    }
    
    // Check if point is within angle range
    float angle = atan2f(dy, dx);
    if (angle < 0) {
        angle += 2.0f * M_PI;
    }
    
    // Normalize start and end angles
    float start = arc->start_angle;
    float end = arc->end_angle;
    
    if (end < start) {
        end += 2.0f * M_PI;
    }
    
    return (angle >= start && angle <= end);
}

float ese_arc_get_length(const EseArc *arc) {
    if (!arc) return 0.0f;
    
    float angle_diff = arc->end_angle - arc->start_angle;
    if (angle_diff < 0) {
        angle_diff += 2.0f * M_PI;
    }
    
    return arc->radius * angle_diff;
}

bool ese_arc_get_point_at_angle(const EseArc *arc, float angle, float *out_x, float *out_y) {
    if (!arc || !out_x || !out_y) return false;
    
    // Normalize start and end angles
    float start = arc->start_angle;
    float end = arc->end_angle;
    
    if (end < start) {
        end += 2.0f * M_PI;
    }
    
    // Normalize input angle
    while (angle < start) {
        angle += 2.0f * M_PI;
    }
    
    if (angle > end) {
        return false;
    }
    
    *out_x = arc->x + arc->radius * cosf(angle);
    *out_y = arc->y + arc->radius * sinf(angle);
    
    return true;
}

bool ese_arc_intersects_rect(const EseArc *arc, const EseRect *rect) {
    if (!arc || !rect) return false;
    
    // Simple bounding box check for now
    // This could be enhanced with more sophisticated arc-rectangle intersection
    float ese_arc_left = arc->x - arc->radius;
    float ese_arc_right = arc->x + arc->radius;
    float ese_arc_top = arc->y - arc->radius;
    float ese_arc_bottom = arc->y + arc->radius;
    
    float rect_left = ese_rect_get_x(rect);
    float rect_right = ese_rect_get_x(rect) + ese_rect_get_width(rect);
    float rect_top = ese_rect_get_y(rect);
    float rect_bottom = ese_rect_get_y(rect) + ese_rect_get_height(rect);
    
    return !(ese_arc_right < rect_left || ese_arc_left > rect_right ||
             ese_arc_bottom < rect_top || ese_arc_top > rect_bottom);
}

/**
 * @brief Serializes an EseArc to a cJSON object.
 *
 * Creates a cJSON object representing the arc with type "ARC"
 * and x, y, radius, start_angle, end_angle coordinates. Only serializes the
 * geometric data, not Lua-related fields.
 *
 * @param arc Pointer to the EseArc object to serialize
 * @return cJSON object representing the arc, or NULL on failure
 */
cJSON *ese_arc_serialize(const EseArc *arc) {
    log_assert("ARC", arc, "ese_arc_serialize called with NULL arc");

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        log_error("ARC", "Failed to create cJSON object for arc serialization");
        return NULL;
    }

    // Add type field
    cJSON *type = cJSON_CreateString("ARC");
    if (!type || !cJSON_AddItemToObject(json, "type", type)) {
        log_error("ARC", "Failed to add type field to arc serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add x coordinate (center)
    cJSON *x = cJSON_CreateNumber((double)arc->x);
    if (!x || !cJSON_AddItemToObject(json, "x", x)) {
        log_error("ARC", "Failed to add x field to arc serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add y coordinate (center)
    cJSON *y = cJSON_CreateNumber((double)arc->y);
    if (!y || !cJSON_AddItemToObject(json, "y", y)) {
        log_error("ARC", "Failed to add y field to arc serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add radius
    cJSON *radius = cJSON_CreateNumber((double)arc->radius);
    if (!radius || !cJSON_AddItemToObject(json, "radius", radius)) {
        log_error("ARC", "Failed to add radius field to arc serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add start_angle
    cJSON *start_angle = cJSON_CreateNumber((double)arc->start_angle);
    if (!start_angle || !cJSON_AddItemToObject(json, "start_angle", start_angle)) {
        log_error("ARC", "Failed to add start_angle field to arc serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add end_angle
    cJSON *end_angle = cJSON_CreateNumber((double)arc->end_angle);
    if (!end_angle || !cJSON_AddItemToObject(json, "end_angle", end_angle)) {
        log_error("ARC", "Failed to add end_angle field to arc serialization");
        cJSON_Delete(json);
        return NULL;
    }

    return json;
}

/**
 * @brief Deserializes an EseArc from a cJSON object.
 *
 * Creates a new EseArc from a cJSON object with type "ARC"
 * and x, y, radius, start_angle, end_angle coordinates. The arc is created
 * with the specified engine and must be explicitly referenced with
 * ese_arc_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for arc creation
 * @param data cJSON object containing arc data
 * @return Pointer to newly created EseArc object, or NULL on failure
 */
EseArc *ese_arc_deserialize(EseLuaEngine *engine, const cJSON *data) {
    log_assert("ARC", data, "ese_arc_deserialize called with NULL data");

    if (!cJSON_IsObject(data)) {
        log_error("ARC", "Arc deserialization failed: data is not a JSON object");
        return NULL;
    }

    // Check type field
    cJSON *type_item = cJSON_GetObjectItem(data, "type");
    if (!type_item || !cJSON_IsString(type_item) || strcmp(type_item->valuestring, "ARC") != 0) {
        log_error("ARC", "Arc deserialization failed: invalid or missing type field");
        return NULL;
    }

    // Get x coordinate (center)
    cJSON *x_item = cJSON_GetObjectItem(data, "x");
    if (!x_item || !cJSON_IsNumber(x_item)) {
        log_error("ARC", "Arc deserialization failed: invalid or missing x field");
        return NULL;
    }

    // Get y coordinate (center)
    cJSON *y_item = cJSON_GetObjectItem(data, "y");
    if (!y_item || !cJSON_IsNumber(y_item)) {
        log_error("ARC", "Arc deserialization failed: invalid or missing y field");
        return NULL;
    }

    // Get radius
    cJSON *radius_item = cJSON_GetObjectItem(data, "radius");
    if (!radius_item || !cJSON_IsNumber(radius_item)) {
        log_error("ARC", "Arc deserialization failed: invalid or missing radius field");
        return NULL;
    }

    // Get start_angle
    cJSON *start_angle_item = cJSON_GetObjectItem(data, "start_angle");
    if (!start_angle_item || !cJSON_IsNumber(start_angle_item)) {
        log_error("ARC", "Arc deserialization failed: invalid or missing start_angle field");
        return NULL;
    }

    // Get end_angle
    cJSON *end_angle_item = cJSON_GetObjectItem(data, "end_angle");
    if (!end_angle_item || !cJSON_IsNumber(end_angle_item)) {
        log_error("ARC", "Arc deserialization failed: invalid or missing end_angle field");
        return NULL;
    }

    // Create new arc
    EseArc *arc = ese_arc_create(engine);
    ese_arc_set_x(arc, (float)x_item->valuedouble);
    ese_arc_set_y(arc, (float)y_item->valuedouble);
    ese_arc_set_radius(arc, (float)radius_item->valuedouble);
    ese_arc_set_start_angle(arc, (float)start_angle_item->valuedouble);
    ese_arc_set_end_angle(arc, (float)end_angle_item->valuedouble);

    return arc;
}
