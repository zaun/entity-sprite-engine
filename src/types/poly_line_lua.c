#include "types/poly_line_lua.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/color.h"
#include "types/point.h"
#include "types/poly_line.h"
#include "types/types.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Lua metamethods
static int _ese_poly_line_lua_gc(lua_State *L);
static int _ese_poly_line_lua_index(lua_State *L);
static int _ese_poly_line_lua_newindex(lua_State *L);
static int _ese_poly_line_lua_tostring(lua_State *L);
static int _ese_poly_line_lua_to_json(lua_State *L);

// Lua constructors
static int _ese_poly_line_lua_new(lua_State *L);

// Lua utility methods
static int _ese_poly_line_lua_add_point(lua_State *L);
static int _ese_poly_line_lua_remove_point(lua_State *L);
static int _ese_poly_line_lua_get_point(lua_State *L);
static int _ese_poly_line_lua_get_point_count(lua_State *L);
static int _ese_poly_line_lua_clear_points(lua_State *L);
static int _ese_poly_line_lua_from_json(lua_State *L);

// Forward declarations for private functions
extern EsePolyLine *_ese_poly_line_make(void);
extern void _ese_poly_line_notify_watchers(EsePolyLine *poly_line);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EsePolyLine
 *
 * Handles cleanup when a Lua proxy table for an EsePolyLine is garbage
 * collected. Only frees the underlying EsePolyLine if it has no C-side
 * references.
 *
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static int _ese_poly_line_lua_gc(lua_State *L) {
    // Get from userdata
    EsePolyLine **ud = (EsePolyLine **)luaL_testudata(L, 1, POLY_LINE_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }

    EsePolyLine *poly_line = *ud;
    if (poly_line) {
        // If lua_ref == LUA_NOREF, there are no more references to this
        // polyline, so we can free it. If lua_ref != LUA_NOREF, this polyline
        // was referenced from C and should not be freed.
        if (ese_poly_line_get_lua_ref(poly_line) == LUA_NOREF) {
            ese_poly_line_destroy(poly_line);
        }
    }

    return 0;
}

/**
 * @brief Lua __index metamethod for EsePolyLine property access
 *
 * Provides read access to polyline properties from Lua. When a Lua script
 * accesses polyline properties, this function is called to retrieve the values.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid properties, 0 for
 * invalid)
 */
static int _ese_poly_line_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_POLY_LINE_INDEX);
    EsePolyLine *poly_line = ese_poly_line_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!poly_line || !key) {
        profile_cancel(PROFILE_LUA_POLY_LINE_INDEX);
        return 0;
    }

    if (strcmp(key, "type") == 0) {
        lua_pushinteger(L, (int)ese_poly_line_get_type(poly_line));
        profile_stop(PROFILE_LUA_POLY_LINE_INDEX, "poly_line_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "stroke_width") == 0) {
        lua_pushnumber(L, ese_poly_line_get_stroke_width(poly_line));
        profile_stop(PROFILE_LUA_POLY_LINE_INDEX, "poly_line_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "stroke_color") == 0) {
        EseColor *stroke_color = ese_poly_line_get_stroke_color(poly_line);
        if (stroke_color) {
            ese_color_lua_push(stroke_color);
        } else {
            lua_pushnil(L);
        }
        profile_stop(PROFILE_LUA_POLY_LINE_INDEX, "poly_line_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "fill_color") == 0) {
        EseColor *fill_color = ese_poly_line_get_fill_color(poly_line);
        if (fill_color) {
            ese_color_lua_push(fill_color);
        } else {
            lua_pushnil(L);
        }
        profile_stop(PROFILE_LUA_POLY_LINE_INDEX, "poly_line_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "add_point") == 0) {
        lua_pushcfunction(L, _ese_poly_line_lua_add_point);
        profile_stop(PROFILE_LUA_POLY_LINE_INDEX, "poly_line_lua_index (method)");
        return 1;
    } else if (strcmp(key, "remove_point") == 0) {
        lua_pushcfunction(L, _ese_poly_line_lua_remove_point);
        profile_stop(PROFILE_LUA_POLY_LINE_INDEX, "poly_line_lua_index (method)");
        return 1;
    } else if (strcmp(key, "get_point") == 0) {
        lua_pushcfunction(L, _ese_poly_line_lua_get_point);
        profile_stop(PROFILE_LUA_POLY_LINE_INDEX, "poly_line_lua_index (method)");
        return 1;
    } else if (strcmp(key, "get_point_count") == 0) {
        lua_pushcfunction(L, _ese_poly_line_lua_get_point_count);
        profile_stop(PROFILE_LUA_POLY_LINE_INDEX, "poly_line_lua_index (method)");
        return 1;
    } else if (strcmp(key, "clear_points") == 0) {
        lua_pushcfunction(L, _ese_poly_line_lua_clear_points);
        profile_stop(PROFILE_LUA_POLY_LINE_INDEX, "poly_line_lua_index (method)");
        return 1;
    } else if (strcmp(key, "toJSON") == 0) {
        lua_pushlightuserdata(L, poly_line);
        lua_pushcclosure(L, _ese_poly_line_lua_to_json, 1);
        profile_stop(PROFILE_LUA_POLY_LINE_INDEX, "poly_line_lua_index (method)");
        return 1;
    }
    profile_stop(PROFILE_LUA_POLY_LINE_INDEX, "poly_line_lua_index (invalid)");
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EsePolyLine property assignment
 *
 * Provides write access to polyline properties from Lua. When a Lua script
 * assigns to polyline properties, this function is called to update the values
 * and notify any registered watchers of the change.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_poly_line_lua_newindex(lua_State *L) {
    profile_start(PROFILE_LUA_POLY_LINE_NEWINDEX);
    EsePolyLine *poly_line = ese_poly_line_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!poly_line || !key) {
        profile_cancel(PROFILE_LUA_POLY_LINE_NEWINDEX);
        return 0;
    }

    if (strcmp(key, "type") == 0) {
        if (!lua_isnumber(L, 3)) {
            profile_cancel(PROFILE_LUA_POLY_LINE_NEWINDEX);
            return luaL_error(L, "type must be a number");
        }
        int type_val = (int)lua_tonumber(L, 3);
        if (type_val < 0 || type_val > 2) {
            profile_cancel(PROFILE_LUA_POLY_LINE_NEWINDEX);
            return luaL_error(L, "type must be 0 (OPEN), 1 (CLOSED), or 2 (FILLED)");
        }
        ese_poly_line_set_type(poly_line, (EsePolyLineType)type_val);
        _ese_poly_line_notify_watchers(poly_line);
        profile_stop(PROFILE_LUA_POLY_LINE_NEWINDEX, "poly_line_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "stroke_width") == 0) {
        if (!lua_isnumber(L, 3)) {
            profile_cancel(PROFILE_LUA_POLY_LINE_NEWINDEX);
            return luaL_error(L, "stroke_width must be a number");
        }
        ese_poly_line_set_stroke_width(poly_line, (float)lua_tonumber(L, 3));
        _ese_poly_line_notify_watchers(poly_line);
        profile_stop(PROFILE_LUA_POLY_LINE_NEWINDEX, "poly_line_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "stroke_color") == 0) {
        int vtype = lua_type(L, 3);
        if (vtype == LUA_TNIL || vtype == LUA_TNONE) {
            EseColor *current_stroke_color = ese_poly_line_get_stroke_color(poly_line);
            if (current_stroke_color) {
                ese_color_unref(current_stroke_color);
                ese_poly_line_set_stroke_color(poly_line, NULL);
            }
            _ese_poly_line_notify_watchers(poly_line);
            profile_stop(PROFILE_LUA_POLY_LINE_NEWINDEX, "poly_line_lua_newindex (setter)");
            return 0;
        }
        EseColor *color = ese_color_lua_get(L, 3);
        if (!color) {
            profile_cancel(PROFILE_LUA_POLY_LINE_NEWINDEX);
            return luaL_error(L, "stroke_color must be a Color object or nil");
        }
        EseColor *current_stroke_color = ese_poly_line_get_stroke_color(poly_line);
        if (current_stroke_color) {
            ese_color_unref(current_stroke_color);
        }
        ese_poly_line_set_stroke_color(poly_line, color);
        ese_color_ref(color);
        _ese_poly_line_notify_watchers(poly_line);
        profile_stop(PROFILE_LUA_POLY_LINE_NEWINDEX, "poly_line_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "fill_color") == 0) {
        int vtype = lua_type(L, 3);
        if (vtype == LUA_TNIL || vtype == LUA_TNONE) {
            EseColor *current_fill_color = ese_poly_line_get_fill_color(poly_line);
            if (current_fill_color) {
                ese_color_unref(current_fill_color);
                ese_poly_line_set_fill_color(poly_line, NULL);
            }
            _ese_poly_line_notify_watchers(poly_line);
            profile_stop(PROFILE_LUA_POLY_LINE_NEWINDEX, "poly_line_lua_newindex (setter)");
            return 0;
        }
        EseColor *color = ese_color_lua_get(L, 3);
        if (!color) {
            profile_cancel(PROFILE_LUA_POLY_LINE_NEWINDEX);
            return luaL_error(L, "fill_color must be a Color object or nil");
        }
        EseColor *current_fill_color = ese_poly_line_get_fill_color(poly_line);
        if (current_fill_color) {
            ese_color_unref(current_fill_color);
        }
        ese_poly_line_set_fill_color(poly_line, color);
        ese_color_ref(color);
        _ese_poly_line_notify_watchers(poly_line);
        profile_stop(PROFILE_LUA_POLY_LINE_NEWINDEX, "poly_line_lua_newindex (setter)");
        return 0;
    }
    profile_stop(PROFILE_LUA_POLY_LINE_NEWINDEX, "poly_line_lua_newindex (invalid)");
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __tostring metamethod for EsePolyLine string representation
 *
 * Converts an EsePolyLine to a human-readable string for debugging and display.
 * The format includes the memory address and current properties.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
static int _ese_poly_line_lua_tostring(lua_State *L) {
    EsePolyLine *poly_line = ese_poly_line_lua_get(L, 1);

    if (!poly_line) {
        lua_pushstring(L, "PolyLine: (invalid)");
        return 1;
    }

    const char *type_str = "UNKNOWN";
    switch (ese_poly_line_get_type(poly_line)) {
    case POLY_LINE_OPEN:
        type_str = "OPEN";
        break;
    case POLY_LINE_CLOSED:
        type_str = "CLOSED";
        break;
    case POLY_LINE_FILLED:
        type_str = "FILLED";
        break;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "PolyLine: %p (type=%s, points=%zu, stroke_width=%.2f)",
             (void *)poly_line, type_str, ese_poly_line_get_point_count(poly_line),
             ese_poly_line_get_stroke_width(poly_line));
    lua_pushstring(L, buf);

    return 1;
}

/**
 * @brief Lua instance method for converting EsePolyLine to JSON string
 */
static int _ese_poly_line_lua_to_json(lua_State *L) {
    EsePolyLine *poly_line = ese_poly_line_lua_get(L, 1);
    if (!poly_line) {
        return luaL_error(L, "PolyLine:toJSON() called on invalid polyline");
    }

    cJSON *json = ese_poly_line_serialize(poly_line);
    if (!json) {
        return luaL_error(L, "PolyLine:toJSON() failed to serialize polyline");
    }

    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!json_str) {
        return luaL_error(L, "PolyLine:toJSON() failed to convert to string");
    }

    lua_pushstring(L, json_str);
    free(json_str);
    return 1;
}

// Lua constructors
/**
 * @brief Lua constructor function for creating new EsePolyLine instances
 *
 * Creates a new EsePolyLine from Lua. This function is called when Lua code
 * executes `PolyLine.new()`.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_poly_line_lua_new(lua_State *L) {
    profile_start(PROFILE_LUA_POLY_LINE_NEW);

    // Create the polyline
    EsePolyLine *poly_line = _ese_poly_line_make();
    ese_poly_line_set_state(poly_line, L);

    // Create userdata directly; Lua owns it (no registry ref)
    EsePolyLine **ud = (EsePolyLine **)lua_newuserdata(L, sizeof(EsePolyLine *));
    *ud = poly_line;
    luaL_getmetatable(L, POLY_LINE_PROXY_META);
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_POLY_LINE_NEW, "poly_line_lua_new");
    return 1;
}

// Lua utility methods
/**
 * @brief Lua method for adding a point to the polyline
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_poly_line_lua_add_point(lua_State *L) {
    profile_start(PROFILE_LUA_POLY_LINE_ADD_POINT);

    EsePolyLine *poly_line = ese_poly_line_lua_get(L, 1);
    EsePoint *point = ese_point_lua_get(L, 2);

    if (!poly_line || !point) {
        profile_cancel(PROFILE_LUA_POLY_LINE_ADD_POINT);
        return luaL_error(L, "add_point requires a polyline and a point");
    }

    bool success = ese_poly_line_add_point(poly_line, point);
    if (!success) {
        profile_cancel(PROFILE_LUA_POLY_LINE_ADD_POINT);
        return luaL_error(L, "Failed to add point to polyline");
    }

    profile_stop(PROFILE_LUA_POLY_LINE_ADD_POINT, "poly_line_lua_add_point");
    return 0;
}

/**
 * @brief Lua method for removing a point from the polyline
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_poly_line_lua_remove_point(lua_State *L) {
    profile_start(PROFILE_LUA_POLY_LINE_REMOVE_POINT);

    EsePolyLine *poly_line = ese_poly_line_lua_get(L, 1);
    if (!poly_line) {
        profile_cancel(PROFILE_LUA_POLY_LINE_REMOVE_POINT);
        return luaL_error(L, "remove_point requires a polyline");
    }

    if (!lua_isnumber(L, 2)) {
        profile_cancel(PROFILE_LUA_POLY_LINE_REMOVE_POINT);
        return luaL_error(L, "Index must be a number");
    }

    size_t index = (size_t)lua_tonumber(L, 2);
    bool success = ese_poly_line_remove_point(poly_line, index);
    if (!success) {
        profile_cancel(PROFILE_LUA_POLY_LINE_REMOVE_POINT);
        return luaL_error(L, "Invalid point index");
    }

    profile_stop(PROFILE_LUA_POLY_LINE_REMOVE_POINT, "poly_line_lua_remove_point");
    return 0;
}

/**
 * @brief Lua method for getting a point from the polyline
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid point, 0 for
 * invalid)
 */
static int _ese_poly_line_lua_get_point(lua_State *L) {
    profile_start(PROFILE_LUA_POLY_LINE_GET_POINT);

    EsePolyLine *poly_line = ese_poly_line_lua_get(L, 1);
    if (!poly_line) {
        profile_cancel(PROFILE_LUA_POLY_LINE_GET_POINT);
        return luaL_error(L, "get_point requires a polyline");
    }

    if (!lua_isnumber(L, 2)) {
        profile_cancel(PROFILE_LUA_POLY_LINE_GET_POINT);
        return luaL_error(L, "Index must be a number");
    }

    size_t index = (size_t)lua_tonumber(L, 2);
    if (index >= ese_poly_line_get_point_count(poly_line)) {
        profile_cancel(PROFILE_LUA_POLY_LINE_GET_POINT);
        return luaL_error(L, "Invalid point index");
    }

    size_t coord_index = index * 2;
    float x = ese_poly_line_get_point_x(poly_line, index);
    float y = ese_poly_line_get_point_y(poly_line, index);
    log_error("POLY_LINE", "get_point index=%zu x=%.2f y=%.2f count=%zu", index, x, y,
              ese_poly_line_get_point_count(poly_line));

    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(
        ese_poly_line_get_state(poly_line), LUA_ENGINE_KEY);
    EsePoint *point = ese_point_create(engine);
    ese_point_set_x(point, x);
    ese_point_set_y(point, y);

    ese_point_lua_push(point);

    profile_stop(PROFILE_LUA_POLY_LINE_GET_POINT, "poly_line_lua_get_point");
    return 1;
}

/**
 * @brief Lua method for getting the point count
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
static int _ese_poly_line_lua_get_point_count(lua_State *L) {
    profile_start(PROFILE_LUA_POLY_LINE_GET_POINT_COUNT);

    EsePolyLine *poly_line = ese_poly_line_lua_get(L, 1);
    if (!poly_line) {
        profile_cancel(PROFILE_LUA_POLY_LINE_GET_POINT_COUNT);
        return luaL_error(L, "get_point_count requires a polyline");
    }

    lua_pushinteger(L, (int)ese_poly_line_get_point_count(poly_line));
    profile_stop(PROFILE_LUA_POLY_LINE_GET_POINT_COUNT, "poly_line_lua_get_point_count");
    return 1;
}

/**
 * @brief Lua method for clearing all points
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_poly_line_lua_clear_points(lua_State *L) {
    profile_start(PROFILE_LUA_POLY_LINE_CLEAR_POINTS);

    EsePolyLine *poly_line = ese_poly_line_lua_get(L, 1);
    if (!poly_line) {
        profile_cancel(PROFILE_LUA_POLY_LINE_CLEAR_POINTS);
        return luaL_error(L, "clear_points requires a polyline");
    }

    ese_poly_line_clear_points(poly_line);
    profile_stop(PROFILE_LUA_POLY_LINE_CLEAR_POINTS, "poly_line_lua_clear_points");
    return 0;
}

/**
 * @brief Lua static method for creating EsePolyLine from JSON string
 */
static int _ese_poly_line_lua_from_json(lua_State *L) {
    int argc = lua_gettop(L);
    if (argc != 1) {
        return luaL_error(L, "PolyLine.fromJSON(string) takes 1 argument");
    }
    if (lua_type(L, 1) != LUA_TSTRING) {
        return luaL_error(L, "PolyLine.fromJSON(string) argument must be a string");
    }

    const char *json_str = lua_tostring(L, 1);
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        log_error("POLY_LINE", "PolyLine.fromJSON: failed to parse JSON string: %s",
                  json_str ? json_str : "NULL");
        return luaL_error(L, "PolyLine.fromJSON: invalid JSON string");
    }

    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (!engine) {
        cJSON_Delete(json);
        return luaL_error(L, "PolyLine.fromJSON: no engine available");
    }

    EsePolyLine *poly_line = ese_poly_line_deserialize(engine, json);
    cJSON_Delete(json);
    if (!poly_line) {
        return luaL_error(L, "PolyLine.fromJSON: failed to deserialize polyline");
    }

    ese_poly_line_lua_push(poly_line);
    return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Internal Lua initialization function for EsePolyLine
 *
 * Sets up the Lua metatable and global PolyLine table with constructors and
 * methods. This function is called by the public ese_poly_line_lua_init
 * function.
 *
 * @param engine EseLuaEngine pointer where the EsePolyLine type will be
 * registered
 */
void _ese_poly_line_lua_init(EseLuaEngine *engine) {
    // Create metatable
    lua_engine_new_object_meta(engine, POLY_LINE_PROXY_META, _ese_poly_line_lua_index,
                               _ese_poly_line_lua_newindex, _ese_poly_line_lua_gc,
                               _ese_poly_line_lua_tostring);

    // Create global PolyLine table with functions
    const char *keys[] = {"new", "fromJSON"};
    lua_CFunction functions[] = {_ese_poly_line_lua_new, _ese_poly_line_lua_from_json};
    lua_engine_new_object(engine, "PolyLine", 2, keys, functions);
}
