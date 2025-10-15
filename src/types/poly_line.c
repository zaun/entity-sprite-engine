#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stddef.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/poly_line.h"
#include "types/point.h"
#include "types/color.h"
#include "vendor/json/cJSON.h"

// The actual EsePolyLine struct definition (private to this file)
typedef struct EsePolyLine {
    EsePolyLineType type;           /** The type of polyline (OPEN, CLOSED, FILLED) */
    float stroke_width;              /** The stroke width */
    EseColor *stroke_color;          /** The stroke color */
    EseColor *fill_color;            /** The fill color */
    
    float *points;                   /** Array of point coordinates (x1, y1, x2, y2, ...) */
    size_t point_count;              /** Number of points */
    size_t point_capacity;           /** Capacity of the points array (in number of points, not floats) */

    lua_State *state;                /** Lua State this EsePolyLine belongs to */
    int lua_ref;                     /** Lua registry reference to its own proxy table */
    int lua_ref_count;               /** Number of times this polyline has been referenced in C */
    
    // Watcher system
    EsePolyLineWatcherCallback *watchers;     /** Array of watcher callbacks */
    void **watcher_userdata;                  /** Array of userdata for each watcher */
    size_t watcher_count;                     /** Number of registered watchers */
    size_t watcher_capacity;                  /** Capacity of the watcher arrays */
} EsePolyLine;

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EsePolyLine *_ese_poly_line_make(void);

// Watcher system
static void _ese_poly_line_notify_watchers(EsePolyLine *poly_line);

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

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EsePolyLine instance with default values
 * 
 * Allocates memory for a new EsePolyLine and initializes all fields to safe defaults.
 * The polyline starts as OPEN type with no points, default stroke width, and default colors.
 * 
 * @return Pointer to the newly created EsePolyLine, or NULL on allocation failure
 */
static EsePolyLine *_ese_poly_line_make() {
    EsePolyLine *poly_line = (EsePolyLine *)memory_manager.malloc(sizeof(EsePolyLine), MMTAG_POLY_LINE);
    poly_line->type = POLY_LINE_OPEN;
    poly_line->stroke_width = 1.0f;
    poly_line->stroke_color = NULL;
    poly_line->fill_color = NULL;
    poly_line->points = NULL;
    poly_line->point_count = 0;
    poly_line->point_capacity = 0;
    poly_line->state = NULL;
    poly_line->lua_ref = LUA_NOREF;
    poly_line->lua_ref_count = 0;
    poly_line->watchers = NULL;
    poly_line->watcher_userdata = NULL;
    poly_line->watcher_count = 0;
    poly_line->watcher_capacity = 0;
    return poly_line;
}

// Watcher system
/**
 * @brief Notifies all registered watchers of a polyline change
 * 
 * Iterates through all registered watcher callbacks and invokes them with the
 * updated polyline and their associated userdata. This is called whenever any
 * property of the polyline is modified.
 * 
 * @param poly_line Pointer to the EsePolyLine that has changed
 */
static void _ese_poly_line_notify_watchers(EsePolyLine *poly_line) {
    if (!poly_line || poly_line->watcher_count == 0) return;
    
    for (size_t i = 0; i < poly_line->watcher_count; i++) {
        if (poly_line->watchers[i]) {
            poly_line->watchers[i](poly_line, poly_line->watcher_userdata[i]);
        }
    }
}

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EsePolyLine
 * 
 * Handles cleanup when a Lua proxy table for an EsePolyLine is garbage collected.
 * Only frees the underlying EsePolyLine if it has no C-side references.
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
        // If lua_ref == LUA_NOREF, there are no more references to this polyline, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this polyline was referenced from C and should not be freed.
        if (poly_line->lua_ref == LUA_NOREF) {
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
 * @return Number of values pushed onto the stack (1 for valid properties, 0 for invalid)
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
        lua_pushinteger(L, (int)poly_line->type);
        profile_stop(PROFILE_LUA_POLY_LINE_INDEX, "poly_line_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "stroke_width") == 0) {
        lua_pushnumber(L, poly_line->stroke_width);
        profile_stop(PROFILE_LUA_POLY_LINE_INDEX, "poly_line_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "stroke_color") == 0) {
        if (poly_line->stroke_color) {
            ese_color_lua_push(poly_line->stroke_color);
        } else {
            lua_pushnil(L);
        }
        profile_stop(PROFILE_LUA_POLY_LINE_INDEX, "poly_line_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "fill_color") == 0) {
        if (poly_line->fill_color) {
            ese_color_lua_push(poly_line->fill_color);
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
        poly_line->type = (EsePolyLineType)type_val;
        _ese_poly_line_notify_watchers(poly_line);
        profile_stop(PROFILE_LUA_POLY_LINE_NEWINDEX, "poly_line_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "stroke_width") == 0) {
        if (!lua_isnumber(L, 3)) {
            profile_cancel(PROFILE_LUA_POLY_LINE_NEWINDEX);
            return luaL_error(L, "stroke_width must be a number");
        }
        poly_line->stroke_width = (float)lua_tonumber(L, 3);
        _ese_poly_line_notify_watchers(poly_line);
        profile_stop(PROFILE_LUA_POLY_LINE_NEWINDEX, "poly_line_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "stroke_color") == 0) {
        int vtype = lua_type(L, 3);
        if (vtype == LUA_TNIL || vtype == LUA_TNONE) {
            if (poly_line->stroke_color) {
                ese_color_unref(poly_line->stroke_color);
                poly_line->stroke_color = NULL;
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
        if (poly_line->stroke_color) {
            ese_color_unref(poly_line->stroke_color);
        }
        poly_line->stroke_color = color;
        ese_color_ref(poly_line->stroke_color);
        _ese_poly_line_notify_watchers(poly_line);
        profile_stop(PROFILE_LUA_POLY_LINE_NEWINDEX, "poly_line_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "fill_color") == 0) {
        int vtype = lua_type(L, 3);
        if (vtype == LUA_TNIL || vtype == LUA_TNONE) {
            if (poly_line->fill_color) {
                ese_color_unref(poly_line->fill_color);
                poly_line->fill_color = NULL;
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
        if (poly_line->fill_color) {
            ese_color_unref(poly_line->fill_color);
        }
        poly_line->fill_color = color;
        ese_color_ref(poly_line->fill_color);
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
    switch (poly_line->type) {
        case POLY_LINE_OPEN: type_str = "OPEN"; break;
        case POLY_LINE_CLOSED: type_str = "CLOSED"; break;
        case POLY_LINE_FILLED: type_str = "FILLED"; break;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "PolyLine: %p (type=%s, points=%zu, stroke_width=%.2f)", 
             (void*)poly_line, type_str, poly_line->point_count, poly_line->stroke_width);
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
 * Creates a new EsePolyLine from Lua. This function is called when Lua code executes `PolyLine.new()`.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_poly_line_lua_new(lua_State *L) {
    profile_start(PROFILE_LUA_POLY_LINE_NEW);

    // Create the polyline
    EsePolyLine *poly_line = _ese_poly_line_make();
    poly_line->state = L;

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
 * @return Number of values pushed onto the stack (1 for valid point, 0 for invalid)
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
    if (index >= poly_line->point_count) {
        profile_cancel(PROFILE_LUA_POLY_LINE_GET_POINT);
        return luaL_error(L, "Invalid point index");
    }

    size_t coord_index = index * 2;
    float x = poly_line->points ? poly_line->points[coord_index] : 0.0f;
    float y = poly_line->points ? poly_line->points[coord_index + 1] : 0.0f;
    log_error("POLY_LINE", "get_point index=%zu x=%.2f y=%.2f count=%zu", index, x, y, poly_line->point_count);

    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(poly_line->state, LUA_ENGINE_KEY);
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

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EsePolyLine *ese_poly_line_create(EseLuaEngine *engine) {
    log_assert("POLY_LINE", engine, "poly_line_create called with NULL engine");
    EsePolyLine *poly_line = _ese_poly_line_make();
    poly_line->state = engine->runtime;
    return poly_line;
}

EsePolyLine *ese_poly_line_copy(const EsePolyLine *source) {
    log_assert("POLY_LINE", source, "poly_line_copy called with NULL source");
    
    EsePolyLine *copy = (EsePolyLine *)memory_manager.malloc(sizeof(EsePolyLine), MMTAG_POLY_LINE);
    copy->type = source->type;
    copy->stroke_width = source->stroke_width;
    copy->stroke_color = NULL;
    copy->fill_color = NULL;
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
    copy->lua_ref_count = 0;
    copy->watchers = NULL;
    copy->watcher_userdata = NULL;
    copy->watcher_count = 0;
    copy->watcher_capacity = 0;
    
    // Copy points array
    copy->point_count = source->point_count;
    copy->point_capacity = source->point_capacity;
    if (source->point_count > 0) {
        copy->points = memory_manager.malloc(sizeof(float) * source->point_count * 2, MMTAG_POLY_LINE);
        memcpy(copy->points, source->points, sizeof(float) * source->point_count * 2);
    } else {
        copy->points = NULL;
    }
    
    // Colors: increase ref counts if present
    if (source->stroke_color) {
        copy->stroke_color = source->stroke_color;
        ese_color_ref(copy->stroke_color);
    }
    if (source->fill_color) {
        copy->fill_color = source->fill_color;
        ese_color_ref(copy->fill_color);
    }

    return copy;
}

void ese_poly_line_destroy(EsePolyLine *poly_line) {
    if (!poly_line) return;

    
    if (poly_line->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately

        // Free points array
        if (poly_line->points) {
            memory_manager.free(poly_line->points);
            poly_line->points = NULL;
        }
        poly_line->point_count = 0;
        poly_line->point_capacity = 0;

        if (poly_line->stroke_color) {
            ese_color_unref(poly_line->stroke_color);
            ese_color_destroy(poly_line->stroke_color);
            poly_line->stroke_color = NULL;
        }
        if (poly_line->fill_color) {
            ese_color_unref(poly_line->fill_color);
            ese_color_destroy(poly_line->fill_color);
            poly_line->fill_color = NULL;
        }
        
        // Free watcher arrays if they exist
        if (poly_line->watchers) {
            memory_manager.free(poly_line->watchers);
            poly_line->watchers = NULL;
        }
        if (poly_line->watcher_userdata) {
            memory_manager.free(poly_line->watcher_userdata);
            poly_line->watcher_userdata = NULL;
        }
        poly_line->watcher_count = 0;
        poly_line->watcher_capacity = 0;

        memory_manager.free(poly_line);
    } else {
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
        ese_poly_line_unref(poly_line);
    }
}

// Property access
void ese_poly_line_set_type(EsePolyLine *poly_line, EsePolyLineType type) {
    log_assert("POLY_LINE", poly_line, "poly_line_set_type called with NULL poly_line");
    poly_line->type = type;
    _ese_poly_line_notify_watchers(poly_line);
}

EsePolyLineType ese_poly_line_get_type(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_type called with NULL poly_line");
    return poly_line->type;
}

void ese_poly_line_set_stroke_width(EsePolyLine *poly_line, float width) {
    log_assert("POLY_LINE", poly_line, "poly_line_set_stroke_width called with NULL poly_line");
    poly_line->stroke_width = width;
    _ese_poly_line_notify_watchers(poly_line);
}

float ese_poly_line_get_stroke_width(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_stroke_width called with NULL poly_line");
    return poly_line->stroke_width;
}

void ese_poly_line_set_stroke_color(EsePolyLine *poly_line, EseColor *color) {
    log_assert("POLY_LINE", poly_line, "poly_line_set_stroke_color called with NULL poly_line");
    poly_line->stroke_color = color;
    _ese_poly_line_notify_watchers(poly_line);
}

EseColor *ese_poly_line_get_stroke_color(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_stroke_color called with NULL poly_line");
    return poly_line->stroke_color;
}

void ese_poly_line_set_fill_color(EsePolyLine *poly_line, EseColor *color) {
    log_assert("POLY_LINE", poly_line, "poly_line_set_fill_color called with NULL poly_line");
    poly_line->fill_color = color;
    _ese_poly_line_notify_watchers(poly_line);
}

EseColor *ese_poly_line_get_fill_color(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_fill_color called with NULL poly_line");
    return poly_line->fill_color;
}

// Points collection management
bool ese_poly_line_add_point(EsePolyLine *poly_line, EsePoint *point) {
    log_assert("POLY_LINE", poly_line, "poly_line_add_point called with NULL poly_line");
    log_assert("POLY_LINE", point, "poly_line_add_point called with NULL point");
    
    // Initialize points array if this is the first point
    if (poly_line->point_count == 0) {
        poly_line->point_capacity = 4; // Start with capacity for 4 points
        poly_line->points = memory_manager.malloc(sizeof(float) * poly_line->point_capacity * 2, MMTAG_POLY_LINE);
        poly_line->point_count = 0;
    }
    
    // Expand array if needed
    if (poly_line->point_count >= poly_line->point_capacity) {
        size_t new_capacity = poly_line->point_capacity * 2;
        float *new_points = memory_manager.realloc(
            poly_line->points, 
            sizeof(float) * new_capacity * 2, 
            MMTAG_POLY_LINE
        );
        
        if (!new_points) return false;
        
        poly_line->points = new_points;
        poly_line->point_capacity = new_capacity;
    }
    
    // Add the new point coordinates
    size_t index = poly_line->point_count * 2;
    poly_line->points[index] = ese_point_get_x(point);
    poly_line->points[index + 1] = ese_point_get_y(point);
    poly_line->point_count++;
    
    _ese_poly_line_notify_watchers(poly_line);
    return true;
}

bool ese_poly_line_remove_point(EsePolyLine *poly_line, size_t index) {
    log_assert("POLY_LINE", poly_line, "poly_line_remove_point called with NULL poly_line");
    
    if (index >= poly_line->point_count) {
        return false;
    }
    
    // Shift remaining points (each point is 2 floats)
    for (size_t i = index; i < poly_line->point_count - 1; i++) {
        size_t src_index = (i + 1) * 2;
        size_t dst_index = i * 2;
        poly_line->points[dst_index] = poly_line->points[src_index];
        poly_line->points[dst_index + 1] = poly_line->points[src_index + 1];
    }
    
    poly_line->point_count--;
    _ese_poly_line_notify_watchers(poly_line);
    return true;
}

EsePoint *ese_poly_line_get_point(const EsePolyLine *poly_line, size_t index) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_point called with NULL poly_line");
    
    if (index >= poly_line->point_count) {
        return NULL;
    }
    
    // Create a temporary EsePoint from the stored coordinates
    size_t coord_index = index * 2;
    float x = poly_line->points[coord_index];
    float y = poly_line->points[coord_index + 1];
    
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(poly_line->state, LUA_ENGINE_KEY);
    EsePoint *point = ese_point_create(engine);
    ese_point_set_x(point, x);
    ese_point_set_y(point, y);
    
    return point;
}

size_t ese_poly_line_get_point_count(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_point_count called with NULL poly_line");
    return poly_line->point_count;
}

void ese_poly_line_clear_points(EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_clear_points called with NULL poly_line");
    
    // No need to destroy individual points since we're storing floats directly
    poly_line->point_count = 0;
    _ese_poly_line_notify_watchers(poly_line);
}

const float *ese_poly_line_get_points(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_points called with NULL poly_line");
    return poly_line->points;
}

// Lua-related access
lua_State *ese_poly_line_get_state(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_state called with NULL poly_line");
    return poly_line->state;
}

int ese_poly_line_get_lua_ref(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_lua_ref called with NULL poly_line");
    return poly_line->lua_ref;
}

int ese_poly_line_get_lua_ref_count(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_lua_ref_count called with NULL poly_line");
    return poly_line->lua_ref_count;
}

// Watcher system
bool ese_poly_line_add_watcher(EsePolyLine *poly_line, EsePolyLineWatcherCallback callback, void *userdata) {
    log_assert("POLY_LINE", poly_line, "poly_line_add_watcher called with NULL poly_line");
    log_assert("POLY_LINE", callback, "poly_line_add_watcher called with NULL callback");
    
    // Initialize watcher arrays if this is the first watcher
    if (poly_line->watcher_count == 0) {
        poly_line->watcher_capacity = 4; // Start with capacity for 4 watchers
        poly_line->watchers = memory_manager.malloc(sizeof(EsePolyLineWatcherCallback) * poly_line->watcher_capacity, MMTAG_POLY_LINE);
        poly_line->watcher_userdata = memory_manager.malloc(sizeof(void*) * poly_line->watcher_capacity, MMTAG_POLY_LINE);
        poly_line->watcher_count = 0;
    }
    
    // Expand arrays if needed
    if (poly_line->watcher_count >= poly_line->watcher_capacity) {
        size_t new_capacity = poly_line->watcher_capacity * 2;
        EsePolyLineWatcherCallback *new_watchers = memory_manager.realloc(
            poly_line->watchers, 
            sizeof(EsePolyLineWatcherCallback) * new_capacity, 
            MMTAG_POLY_LINE
        );
        void **new_userdata = memory_manager.realloc(
            poly_line->watcher_userdata, 
            sizeof(void*) * new_capacity, 
            MMTAG_POLY_LINE
        );
        
        if (!new_watchers || !new_userdata) return false;
        
        poly_line->watchers = new_watchers;
        poly_line->watcher_userdata = new_userdata;
        poly_line->watcher_capacity = new_capacity;
    }
    
    // Add the new watcher
    poly_line->watchers[poly_line->watcher_count] = callback;
    poly_line->watcher_userdata[poly_line->watcher_count] = userdata;
    poly_line->watcher_count++;
    
    return true;
}

bool ese_poly_line_remove_watcher(EsePolyLine *poly_line, EsePolyLineWatcherCallback callback, void *userdata) {
    log_assert("POLY_LINE", poly_line, "poly_line_remove_watcher called with NULL poly_line");
    log_assert("POLY_LINE", callback, "poly_line_remove_watcher called with NULL callback");
    
    for (size_t i = 0; i < poly_line->watcher_count; i++) {
        if (poly_line->watchers[i] == callback && poly_line->watcher_userdata[i] == userdata) {
            // Remove this watcher by shifting remaining ones
            for (size_t j = i; j < poly_line->watcher_count - 1; j++) {
                poly_line->watchers[j] = poly_line->watchers[j + 1];
                poly_line->watcher_userdata[j] = poly_line->watcher_userdata[j + 1];
            }
            poly_line->watcher_count--;
            return true;
        }
    }
    
    return false;
}

// Lua integration
void ese_poly_line_lua_init(EseLuaEngine *engine) {
    log_assert("POLY_LINE", engine, "poly_line_lua_init called with NULL engine");
    if (luaL_newmetatable(engine->runtime, POLY_LINE_PROXY_META)) {
        log_debug("LUA", "Adding entity PolyLineProxyMeta to engine");
        lua_pushstring(engine->runtime, POLY_LINE_PROXY_META);
        lua_setfield(engine->runtime, -2, "__name");
        lua_pushcfunction(engine->runtime, _ese_poly_line_lua_index);
        lua_setfield(engine->runtime, -2, "__index");               // For property getters
        lua_pushcfunction(engine->runtime, _ese_poly_line_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");            // For property setters
        lua_pushcfunction(engine->runtime, _ese_poly_line_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");                  // For garbage collection
        lua_pushcfunction(engine->runtime, _ese_poly_line_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");            // For printing/debugging
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
    
    // Create global EsePolyLine table with constructor
    lua_getglobal(engine->runtime, "PolyLine");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1); // Pop the nil value
        log_debug("LUA", "Creating global polyline table");
        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _ese_poly_line_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_pushcfunction(engine->runtime, _ese_poly_line_lua_from_json);
        lua_setfield(engine->runtime, -2, "fromJSON");
        lua_setglobal(engine->runtime, "PolyLine");
    } else {
        lua_pop(engine->runtime, 1); // Pop the existing polyline table
    }
}

void ese_poly_line_lua_push(EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_lua_push called with NULL poly_line");

    if (poly_line->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EsePolyLine **ud = (EsePolyLine **)lua_newuserdata(poly_line->state, sizeof(EsePolyLine *));
        *ud = poly_line;

        // Attach metatable
        luaL_getmetatable(poly_line->state, POLY_LINE_PROXY_META);
        lua_setmetatable(poly_line->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(poly_line->state, LUA_REGISTRYINDEX, poly_line->lua_ref);
    }
}

EsePolyLine *ese_poly_line_lua_get(lua_State *L, int idx) {
    log_assert("POLY_LINE", L, "poly_line_lua_get called with NULL Lua state");
    
    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EsePolyLine **ud = (EsePolyLine **)luaL_testudata(L, idx, POLY_LINE_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
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
        log_error("POLY_LINE", "PolyLine.fromJSON: failed to parse JSON string: %s", json_str ? json_str : "NULL");
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

void ese_poly_line_ref(EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_ref called with NULL poly_line");
    
    if (poly_line->lua_ref == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EsePolyLine **ud = (EsePolyLine **)lua_newuserdata(poly_line->state, sizeof(EsePolyLine *));
        *ud = poly_line;

        // Attach metatable
        luaL_getmetatable(poly_line->state, POLY_LINE_PROXY_META);
        lua_setmetatable(poly_line->state, -2);

        // Store hard reference to prevent garbage collection
        poly_line->lua_ref = luaL_ref(poly_line->state, LUA_REGISTRYINDEX);
        poly_line->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        poly_line->lua_ref_count++;
    }

    profile_count_add("poly_line_ref_count");
}

void ese_poly_line_unref(EsePolyLine *poly_line) {
    if (!poly_line) return;
    
    if (poly_line->lua_ref != LUA_NOREF && poly_line->lua_ref_count > 0) {
        poly_line->lua_ref_count--;
        
        if (poly_line->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(poly_line->state, LUA_REGISTRYINDEX, poly_line->lua_ref);
            poly_line->lua_ref = LUA_NOREF;
        }
    }

    profile_count_add("poly_line_unref_count");
}

size_t ese_poly_line_sizeof(void) {
    return sizeof(EsePolyLine);
}

/**
 * @brief Serializes an EsePolyLine to a cJSON object.
 *
 * Creates a cJSON object representing the polyline with type "POLY_LINE"
 * and all properties including points, colors, and styling. Only serializes the
 * geometric and styling data, not Lua-related fields.
 *
 * @param poly_line Pointer to the EsePolyLine object to serialize
 * @return cJSON object representing the polyline, or NULL on failure
 */
cJSON *ese_poly_line_serialize(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "ese_poly_line_serialize called with NULL poly_line");

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        log_error("POLY_LINE", "Failed to create cJSON object for poly_line serialization");
        return NULL;
    }

    // Add type field
    cJSON *type = cJSON_CreateString("POLY_LINE");
    if (!type || !cJSON_AddItemToObject(json, "type", type)) {
        log_error("POLY_LINE", "Failed to add type field to poly_line serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add polyline type
    const char *type_str;
    switch (poly_line->type) {
        case POLY_LINE_OPEN: type_str = "OPEN"; break;
        case POLY_LINE_CLOSED: type_str = "CLOSED"; break;
        case POLY_LINE_FILLED: type_str = "FILLED"; break;
        default: type_str = "UNKNOWN"; break;
    }
    cJSON *poly_type = cJSON_CreateString(type_str);
    if (!poly_type || !cJSON_AddItemToObject(json, "poly_type", poly_type)) {
        log_error("POLY_LINE", "Failed to add poly_type field to poly_line serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add stroke_width
    cJSON *stroke_width = cJSON_CreateNumber((double)poly_line->stroke_width);
    if (!stroke_width || !cJSON_AddItemToObject(json, "stroke_width", stroke_width)) {
        log_error("POLY_LINE", "Failed to add stroke_width field to poly_line serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Serialize stroke_color if present
    if (poly_line->stroke_color) {
        cJSON *stroke_color_json = ese_color_serialize(poly_line->stroke_color);
        if (!stroke_color_json || !cJSON_AddItemToObject(json, "stroke_color", stroke_color_json)) {
            log_error("POLY_LINE", "Failed to add stroke_color field to poly_line serialization");
            cJSON_Delete(json);
            return NULL;
        }
    }

    // Serialize fill_color if present
    if (poly_line->fill_color) {
        cJSON *fill_color_json = ese_color_serialize(poly_line->fill_color);
        if (!fill_color_json || !cJSON_AddItemToObject(json, "fill_color", fill_color_json)) {
            log_error("POLY_LINE", "Failed to add fill_color field to poly_line serialization");
            cJSON_Delete(json);
            return NULL;
        }
    }

    // Add points array
    cJSON *points_array = cJSON_CreateArray();
    if (!points_array || !cJSON_AddItemToObject(json, "points", points_array)) {
        log_error("POLY_LINE", "Failed to add points array to poly_line serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add each point as an array of [x, y] coordinates
    for (size_t i = 0; i < poly_line->point_count; i++) {
        cJSON *point_array = cJSON_CreateArray();
        if (!point_array) {
            log_error("POLY_LINE", "Failed to create point array for poly_line serialization");
            cJSON_Delete(json);
            return NULL;
        }

        cJSON *x = cJSON_CreateNumber((double)poly_line->points[i * 2]);
        cJSON *y = cJSON_CreateNumber((double)poly_line->points[i * 2 + 1]);

        if (!x || !y || !cJSON_AddItemToArray(point_array, x) || !cJSON_AddItemToArray(point_array, y)) {
            log_error("POLY_LINE", "Failed to add point coordinates to poly_line serialization");
            cJSON_Delete(point_array);
            cJSON_Delete(json);
            return NULL;
        }

        if (!cJSON_AddItemToArray(points_array, point_array)) {
            log_error("POLY_LINE", "Failed to add point to points array in poly_line serialization");
            cJSON_Delete(point_array);
            cJSON_Delete(json);
            return NULL;
        }
    }

    return json;
}

/**
 * @brief Deserializes an EsePolyLine from a cJSON object.
 *
 * Creates a new EsePolyLine from a cJSON object with type "POLY_LINE"
 * and all properties including points, colors, and styling. The polyline is created
 * with the specified engine and must be explicitly referenced with
 * ese_poly_line_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for polyline creation
 * @param data cJSON object containing polyline data
 * @return Pointer to newly created EsePolyLine object, or NULL on failure
 */
EsePolyLine *ese_poly_line_deserialize(EseLuaEngine *engine, const cJSON *data) {
    log_assert("POLY_LINE", data, "ese_poly_line_deserialize called with NULL data");

    if (!cJSON_IsObject(data)) {
        log_error("POLY_LINE", "PolyLine deserialization failed: data is not a JSON object");
        return NULL;
    }

    // Check type field
    cJSON *type_item = cJSON_GetObjectItem(data, "type");
    if (!type_item || !cJSON_IsString(type_item) || strcmp(type_item->valuestring, "POLY_LINE") != 0) {
        log_error("POLY_LINE", "PolyLine deserialization failed: invalid or missing type field");
        return NULL;
    }

    // Get poly_type field
    cJSON *poly_type_item = cJSON_GetObjectItem(data, "poly_type");
    if (!poly_type_item || !cJSON_IsString(poly_type_item)) {
        log_error("POLY_LINE", "PolyLine deserialization failed: invalid or missing poly_type field");
        return NULL;
    }

    EsePolyLineType poly_type;
    if (strcmp(poly_type_item->valuestring, "OPEN") == 0) {
        poly_type = POLY_LINE_OPEN;
    } else if (strcmp(poly_type_item->valuestring, "CLOSED") == 0) {
        poly_type = POLY_LINE_CLOSED;
    } else if (strcmp(poly_type_item->valuestring, "FILLED") == 0) {
        poly_type = POLY_LINE_FILLED;
    } else {
        log_error("POLY_LINE", "PolyLine deserialization failed: invalid poly_type value");
        return NULL;
    }

    // Get stroke_width field
    cJSON *stroke_width_item = cJSON_GetObjectItem(data, "stroke_width");
    if (!stroke_width_item || !cJSON_IsNumber(stroke_width_item)) {
        log_error("POLY_LINE", "PolyLine deserialization failed: invalid or missing stroke_width field");
        return NULL;
    }

    // Get points array
    cJSON *points_item = cJSON_GetObjectItem(data, "points");
    if (!points_item || !cJSON_IsArray(points_item)) {
        log_error("POLY_LINE", "PolyLine deserialization failed: invalid or missing points array");
        return NULL;
    }

    size_t point_count = cJSON_GetArraySize(points_item);
    if (point_count == 0) {
        log_error("POLY_LINE", "PolyLine deserialization failed: empty points array");
        return NULL;
    }

    // Create new polyline
    EsePolyLine *poly_line = ese_poly_line_create(engine);

    // Set polyline type and stroke width
    poly_line->type = poly_type;
    poly_line->stroke_width = (float)stroke_width_item->valuedouble;

    // Initialize points array
    poly_line->point_capacity = point_count;
    poly_line->point_count = 0;
    poly_line->points = memory_manager.malloc(sizeof(float) * poly_line->point_capacity * 2, MMTAG_POLY_LINE);

    // Add points
    for (size_t i = 0; i < point_count; i++) {
        cJSON *point_item = cJSON_GetArrayItem(points_item, i);
        if (!point_item || !cJSON_IsArray(point_item) || cJSON_GetArraySize(point_item) != 2) {
            log_error("POLY_LINE", "PolyLine deserialization failed: invalid point format");
            ese_poly_line_destroy(poly_line);
            return NULL;
        }

        cJSON *x_item = cJSON_GetArrayItem(point_item, 0);
        cJSON *y_item = cJSON_GetArrayItem(point_item, 1);

        if (!x_item || !cJSON_IsNumber(x_item) || !y_item || !cJSON_IsNumber(y_item)) {
            log_error("POLY_LINE", "PolyLine deserialization failed: invalid point coordinates");
            ese_poly_line_destroy(poly_line);
            return NULL;
        }

        // Add point coordinates directly (ese_poly_line_add_point just extracts x,y)
        size_t index = poly_line->point_count * 2;
        poly_line->points[index] = (float)x_item->valuedouble;
        poly_line->points[index + 1] = (float)y_item->valuedouble;
        poly_line->point_count++;
    }

    // Deserialize stroke_color if present
    cJSON *stroke_color_item = cJSON_GetObjectItem(data, "stroke_color");
    if (stroke_color_item && cJSON_IsObject(stroke_color_item)) {
        EseColor *stroke_color = ese_color_deserialize(engine, stroke_color_item);
        if (stroke_color) {
            ese_poly_line_set_stroke_color(poly_line, stroke_color);
            // Don't destroy - polyline takes ownership
        }
    }

    // Deserialize fill_color if present
    cJSON *fill_color_item = cJSON_GetObjectItem(data, "fill_color");
    if (fill_color_item && cJSON_IsObject(fill_color_item)) {
        EseColor *fill_color = ese_color_deserialize(engine, fill_color_item);
        if (fill_color) {
            ese_poly_line_set_fill_color(poly_line, fill_color);
            // Don't destroy - polyline takes ownership
        }
    }

    return poly_line;
}
