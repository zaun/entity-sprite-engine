#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/rect.h"
#include "types/point.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"
#include "types/rect_lua.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
extern EseRect *_ese_rect_make(void);
extern void _ese_rect_notify_watchers(EseRect *rect);

// Lua metamethods
static int _ese_rect_lua_gc(lua_State *L);
static int _ese_rect_lua_index(lua_State *L);
static int _ese_rect_lua_newindex(lua_State *L);
static int _ese_rect_lua_tostring(lua_State *L);

// Lua constructors
static int _ese_rect_lua_new(lua_State *L);
static int _ese_rect_lua_zero(lua_State *L);

// Lua methods
static int _ese_rect_lua_area(lua_State *L);
static int _ese_rect_lua_contains_point(lua_State *L);
static int _ese_rect_lua_intersects(lua_State *L);

// Lua JSON methods
static int _ese_rect_lua_from_json(lua_State *L);
static int _ese_rect_lua_to_json(lua_State *L);

// Helper functions
static inline float deg_to_rad(float d) { return d * (M_PI / 180.0f); }
static inline float rad_to_deg(float r) { return r * (180.0f / M_PI); }

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseRect
 * 
 * Handles cleanup when a Lua proxy table for an EseRect is garbage collected.
 * Only frees the underlying EseRect if it has no C-side references.
 * 
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static int _ese_rect_lua_gc(lua_State *L) {
    // Get from userdata
    EseRect **ud = (EseRect **)luaL_testudata(L, 1, RECT_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseRect *rect = *ud;
    if (rect) {
        // If lua_ref == LUA_NOREF, there are no more references to this rect, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this rect was referenced from C and should not be freed.
        if (ese_rect_get_lua_ref(rect) == LUA_NOREF) {
            ese_rect_destroy(rect);
        }
    }

    return 0;
}

/**
 * @brief Lua __index metamethod for EseRect property access
 * 
 * Provides read access to rect properties (x, y, width, height, rotation) from Lua.
 * When a Lua script accesses rect.x, rect.y, etc., this function is called to retrieve the values.
 * Also provides access to methods like contains_point, intersects, and area.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid properties/methods, 0 for invalid)
 */
static int _ese_rect_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_RECT_INDEX);
    EseRect *rect = ese_rect_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!rect || !key) {
        profile_cancel(PROFILE_LUA_RECT_INDEX);
        return 0;
    }

    if (strcmp(key, "x") == 0) {
        lua_pushnumber(L, ese_rect_get_x(rect));
        profile_stop(PROFILE_LUA_RECT_INDEX, "rect_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(L, ese_rect_get_y(rect));
        profile_stop(PROFILE_LUA_RECT_INDEX, "rect_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "width") == 0) {
        lua_pushnumber(L, ese_rect_get_width(rect));
        profile_stop(PROFILE_LUA_RECT_INDEX, "rect_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "height") == 0) {
        lua_pushnumber(L, ese_rect_get_height(rect));
        profile_stop(PROFILE_LUA_RECT_INDEX, "rect_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "rotation") == 0) {
        lua_pushnumber(L, (double)rad_to_deg(ese_rect_get_rotation(rect)));
        profile_stop(PROFILE_LUA_RECT_INDEX, "rect_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "contains_point") == 0) {
        lua_pushlightuserdata(L, rect);
        lua_pushcclosure(L, _ese_rect_lua_contains_point, 1);
        profile_stop(PROFILE_LUA_RECT_INDEX, "rect_lua_index (method)");
        return 1;
    } else if (strcmp(key, "intersects") == 0) {
        lua_pushlightuserdata(L, rect);
        lua_pushcclosure(L, _ese_rect_lua_intersects, 1);
        profile_stop(PROFILE_LUA_RECT_INDEX, "rect_lua_index (method)");
        return 1;
    } else if (strcmp(key, "area") == 0) {
        lua_pushlightuserdata(L, rect);
        lua_pushcclosure(L, _ese_rect_lua_area, 1);
        profile_stop(PROFILE_LUA_RECT_INDEX, "rect_lua_index (method)");
        return 1;
    } else if (strcmp(key, "toJSON") == 0) {
        lua_pushlightuserdata(L, rect);
        lua_pushcclosure(L, _ese_rect_lua_to_json, 1);
        profile_stop(PROFILE_LUA_RECT_INDEX, "rect_lua_index (method)");
        return 1;
    }
    profile_stop(PROFILE_LUA_RECT_INDEX, "rect_lua_index (invalid)");
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseRect property assignment
 * 
 * Provides write access to rect properties (x, y, width, height, rotation) from Lua.
 * When a Lua script assigns to rect.x, rect.y, etc., this function is called to update
 * the values and notify any registered watchers of the change.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_rect_lua_newindex(lua_State *L) {
    profile_start(PROFILE_LUA_RECT_NEWINDEX);
    EseRect *rect = ese_rect_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!rect || !key) {
        profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
        return 0;
    }

    if (strcmp(key, "x") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
            return luaL_error(L, "rect.x must be a number");
        }
        ese_rect_set_x(rect, (float)lua_tonumber(L, 3));
        _ese_rect_notify_watchers(rect);
        profile_stop(PROFILE_LUA_RECT_NEWINDEX, "rect_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "y") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
            return luaL_error(L, "rect.y must be a number");
        }
        ese_rect_set_y(rect, (float)lua_tonumber(L, 3));
        _ese_rect_notify_watchers(rect);
        profile_stop(PROFILE_LUA_RECT_NEWINDEX, "rect_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "width") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
            return luaL_error(L, "rect.width must be a number");
        }
        ese_rect_set_width(rect, (float)lua_tonumber(L, 3));
        _ese_rect_notify_watchers(rect);
        profile_stop(PROFILE_LUA_RECT_NEWINDEX, "rect_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "height") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
            return luaL_error(L, "rect.height must be a number");
        }
        ese_rect_set_height(rect, (float)lua_tonumber(L, 3));
        _ese_rect_notify_watchers(rect);
        profile_stop(PROFILE_LUA_RECT_NEWINDEX, "rect_lua_newindex (setter)");
        return 0;
    }  else if (strcmp(key, "rotation") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
            return luaL_error(L, "rect.rotation must be a number (degrees)");
        }
        float deg = (float)lua_tonumber(L, 3);
        ese_rect_set_rotation(rect, deg_to_rad(deg));
        _ese_rect_notify_watchers(rect);
        profile_stop(PROFILE_LUA_RECT_NEWINDEX, "rect_lua_newindex (setter)");
        return 0;
    }
    profile_stop(PROFILE_LUA_RECT_NEWINDEX, "rect_lua_newindex (invalid)");
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __tostring metamethod for EseRect string representation
 * 
 * Converts an EseRect to a human-readable string for debugging and display.
 * The format includes the memory address and current x,y,width,height,rotation values.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
static int _ese_rect_lua_tostring(lua_State *L) {
    EseRect *rect = ese_rect_lua_get(L, 1);

    if (!rect) {
        lua_pushstring(L, "Rect: (invalid)");
        return 1;
    }

    char buf[160];
    snprintf(
        buf, sizeof(buf),
        "(x=%.3f, y=%.3f, w=%.3f, h=%.3f, rot=%.3fdeg)",
        ese_rect_get_x(rect), ese_rect_get_y(rect), ese_rect_get_width(rect), ese_rect_get_height(rect),
        (double)rad_to_deg(ese_rect_get_rotation(rect))
    );
    lua_pushstring(L, buf);

    return 1;
}

// Lua constructors
/**
 * @brief Lua constructor function for creating new EseRect instances
 * 
 * Creates a new EseRect from Lua with specified x,y,width,height coordinates.
 * This function is called when Lua code executes `Rect.new(x, y, width, height)`.
 * It validates the arguments, creates the underlying EseRect, and returns a proxy
 * table that provides access to the rect's properties and methods.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_rect_lua_new(lua_State *L) {
    profile_start(PROFILE_LUA_RECT_NEW);

    int n_args = lua_gettop(L);
    if (n_args == 4) {
        if (lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER || 
            lua_type(L, 3) != LUA_TNUMBER || lua_type(L, 4) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_RECT_NEW);
            return luaL_error(L, "Rect.new(number, number, number, number) arguments must be numbers");
        }
    } else {
        profile_cancel(PROFILE_LUA_RECT_NEW);
        return luaL_error(L, "Rect.new(number, number, number, number) takes 4 arguments");
    }

    float x = (float)lua_tonumber(L, 1);
    float y = (float)lua_tonumber(L, 2);
    float width = (float)lua_tonumber(L, 3);
    float height = (float)lua_tonumber(L, 4);

    // Get the current engine from Lua registry
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (!engine) {
        profile_cancel(PROFILE_LUA_RECT_NEW);
        return luaL_error(L, "Rect.new: no engine available");
    }

    // Create the rect using the standard creation function
    EseRect *rect = ese_rect_create(engine);
    ese_rect_set_x(rect, x);
    ese_rect_set_y(rect, y);
    ese_rect_set_width(rect, width);
    ese_rect_set_height(rect, height);
    
    // Create userdata directly
    EseRect **ud = (EseRect **)lua_newuserdata(L, sizeof(EseRect *));
    *ud = rect;

    // Attach metatable
    luaL_getmetatable(L, RECT_PROXY_META);
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_RECT_NEW, "rect_lua_new");
    return 1;
}

/**
 * @brief Lua constructor function for creating EseRect at origin
 * 
 * Creates a new EseRect at the origin (0,0) with zero dimensions from Lua.
 * This function is called when Lua code executes `Rect.zero()`. It's a convenience
 * constructor for creating rects at the default position and size.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_rect_lua_zero(lua_State *L) {
    profile_start(PROFILE_LUA_RECT_ZERO);

    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 0) {
        profile_cancel(PROFILE_LUA_RECT_ZERO);
        return luaL_error(L, "Rect.zero() takes 0 arguments");
    }

    // Get the current engine from Lua registry
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (!engine) {
        profile_cancel(PROFILE_LUA_RECT_ZERO);
        return luaL_error(L, "Rect.zero: no engine available");
    }

    // Create the rect using the standard creation function
    EseRect *rect = ese_rect_create(engine);
    
    // Create userdata directly
    EseRect **ud = (EseRect **)lua_newuserdata(L, sizeof(EseRect *));
    *ud = rect;

    // Attach metatable
    luaL_getmetatable(L, RECT_PROXY_META);
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_RECT_ZERO, "rect_lua_zero");
    return 1;
}

// Lua methods
/**
 * @brief Lua method for calculating rect area
 * 
 * Calculates and returns the area of the rectangle (width * height).
 * This is a Lua method that can be called on rect instances.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the area value)
 */
static int _ese_rect_lua_area(lua_State *L) {
    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 1) {
        return luaL_error(L, "rect:area() takes 0 argument");
    }
    
    EseRect *rect = ese_rect_lua_get(L, 1);
    if (!rect) {
        return luaL_error(L, "Invalid EseRect object in area method");
    }
    
    lua_pushnumber(L, ese_rect_area(rect));
    return 1;
}

/**
 * @brief Lua method for testing if a point is contained within the rect
 * 
 * Tests whether the specified (x,y) point lies within the rectangle bounds.
 * Handles both axis-aligned and rotated rectangles appropriately.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - boolean result)
 */
static int _ese_rect_lua_contains_point(lua_State *L) {
    EseRect *rect = ese_rect_lua_get(L, 1);
    if (!rect) {
        return luaL_error(L, "Invalid EseRect object in contains_point method");
    }

    float x, y;
    
    int n_args = lua_gettop(L);
    if (n_args == 3) {
        if (lua_type(L, 2) != LUA_TNUMBER || lua_type(L, 3) != LUA_TNUMBER) {
            return luaL_error(L, "rect:contains_point(number, number) arguments must be numbers");
        }
        x = (float)lua_tonumber(L, 2);
        y = (float)lua_tonumber(L, 3);
    } else if (n_args == 2) {
        EsePoint *point = ese_point_lua_get(L, 2);
        if (!point) {
            return luaL_error(L, "rect:contains_point(point) requires a point");
        }
        x = ese_point_get_x(point);
        y = ese_point_get_y(point);
    } else {
        return luaL_error(L, "nrect:contains_point(point) takes 1 argument\nrect:contains_point(number, number) takes 2 arguments");
    }
        
    
    lua_pushboolean(L, ese_rect_contains_point(rect, x, y));
    return 1;
}

/**
 * @brief Lua method for testing intersection with another rect
 * 
 * Tests whether this rectangle intersects with another rectangle.
 * Uses efficient collision detection algorithms for both AABB and OBB cases.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - boolean result)
 */
static int _ese_rect_lua_intersects(lua_State *L) {
    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 2) {
        return luaL_error(L, "Rect.intersects(rect) takes 1 arguments");
    }

    EseRect *rect = ese_rect_lua_get(L, 1);
    if (!rect) {
        return luaL_error(L, "Invalid EseRect object in intersects method");
    }
    
    EseRect *other = ese_rect_lua_get(L, 2);
    if (!other) {
        return luaL_error(L, "rect:intersects(rect) requires another EseRect object");
    }
    
    lua_pushboolean(L, ese_rect_intersects(rect, other));
    return 1;
}

/**
 * @brief Lua static method for creating EseRect from JSON string
 *
 * Creates a new EseRect from a JSON string. This function is called when Lua code
 * executes `Rect.fromJSON(json_string)`. It parses the JSON string, validates it
 * contains rect data, and creates a new rect with the specified coordinates and dimensions.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_rect_lua_from_json(lua_State *L) {
    profile_start(PROFILE_LUA_RECT_FROM_JSON);

    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 1) {
        profile_cancel(PROFILE_LUA_RECT_FROM_JSON);
        return luaL_error(L, "Rect.fromJSON(string) takes 1 argument");
    }

    if (lua_type(L, 1) != LUA_TSTRING) {
        profile_cancel(PROFILE_LUA_RECT_FROM_JSON);
        return luaL_error(L, "Rect.fromJSON(string) argument must be a string");
    }

    const char *json_str = lua_tostring(L, 1);

    // Parse JSON string
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        log_error("RECT", "Rect.fromJSON: failed to parse JSON string: %s", json_str ? json_str : "NULL");
        profile_cancel(PROFILE_LUA_RECT_FROM_JSON);
        return luaL_error(L, "Rect.fromJSON: invalid JSON string");
    }

    // Get the current engine from Lua registry
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (!engine) {
        cJSON_Delete(json);
        profile_cancel(PROFILE_LUA_RECT_FROM_JSON);
        return luaL_error(L, "Rect.fromJSON: no engine available");
    }

    // Use the existing deserialization function
    EseRect *rect = ese_rect_deserialize(engine, json);
    cJSON_Delete(json); // Clean up JSON object

    if (!rect) {
        profile_cancel(PROFILE_LUA_RECT_FROM_JSON);
        return luaL_error(L, "Rect.fromJSON: failed to deserialize rect");
    }

    ese_rect_lua_push(rect);

    profile_stop(PROFILE_LUA_RECT_FROM_JSON, "rect_lua_from_json");
    return 1;
}

/**
 * @brief Lua instance method for converting EseRect to JSON string
 *
 * Converts an EseRect to a JSON string representation. This function is called when
 * Lua code executes `rect:toJSON()`. It serializes the rect's coordinates, dimensions,
 * and rotation to JSON format and returns the string.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the JSON string)
 */
static int _ese_rect_lua_to_json(lua_State *L) {
    profile_start(PROFILE_LUA_RECT_TO_JSON);

    EseRect *rect = ese_rect_lua_get(L, 1);
    if (!rect) {
        profile_cancel(PROFILE_LUA_RECT_TO_JSON);
        return luaL_error(L, "Rect:toJSON() called on invalid rect");
    }

    // Serialize rect to JSON
    cJSON *json = ese_rect_serialize(rect);
    if (!json) {
        profile_cancel(PROFILE_LUA_RECT_TO_JSON);
        return luaL_error(L, "Rect:toJSON() failed to serialize rect");
    }

    // Convert JSON to string
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json); // Clean up JSON object

    if (!json_str) {
        profile_cancel(PROFILE_LUA_RECT_TO_JSON);
        return luaL_error(L, "Rect:toJSON() failed to convert to string");
    }

    // Push the JSON string onto the stack (lua_pushstring makes a copy)
    lua_pushstring(L, json_str);

    // Clean up the string (cJSON_Print uses malloc)
    // Note: We free here, but lua_pushstring should have made a copy
    free(json_str); // cJSON doesnt use the memory manager.

    profile_stop(PROFILE_LUA_RECT_TO_JSON, "rect_lua_to_json");
    return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

void _ese_rect_lua_init(EseLuaEngine *engine) {
    // Create metatable
    lua_engine_new_object_meta(engine, RECT_PROXY_META, 
        _ese_rect_lua_index, 
        _ese_rect_lua_newindex, 
        _ese_rect_lua_gc, 
        _ese_rect_lua_tostring);
    
    // Create global Rect table with functions
    const char *keys[] = {"new", "zero", "fromJSON"};
    lua_CFunction functions[] = {_ese_rect_lua_new, _ese_rect_lua_zero, _ese_rect_lua_from_json};
    lua_engine_new_object(engine, "Rect", 3, keys, functions);
}
