#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stddef.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/poly_line.h"
#include "types/point.h"
#include "types/color.h"

// The actual EsePolyLine struct definition (private to this file)
typedef struct EsePolyLine {
    EsePolyLineType type;           /**< The type of polyline (OPEN, CLOSED, FILLED) */
    float stroke_width;              /**< The stroke width */
    EseColor *stroke_color;          /**< The stroke color */
    EseColor *fill_color;            /**< The fill color */
    
    EsePoint **points;               /**< Array of points in the polyline */
    size_t point_count;              /**< Number of points */
    size_t point_capacity;           /**< Capacity of the points array */

    lua_State *state;                /**< Lua State this EsePolyLine belongs to */
    int lua_ref;                     /**< Lua registry reference to its own proxy table */
    int lua_ref_count;               /**< Number of times this polyline has been referenced in C */
    
    // Watcher system
    EsePolyLineWatcherCallback *watchers;     /**< Array of watcher callbacks */
    void **watcher_userdata;                  /**< Array of userdata for each watcher */
    size_t watcher_count;                     /**< Number of registered watchers */
    size_t watcher_capacity;                  /**< Capacity of the watcher arrays */
} EsePolyLine;

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EsePolyLine *_poly_line_make(void);

// Watcher system
static void _poly_line_notify_watchers(EsePolyLine *poly_line);

// Lua metamethods
static int _ese_poly_line_lua_gc(lua_State *L);
static int _ese_poly_line_lua_index(lua_State *L);
static int _ese_poly_line_lua_newindex(lua_State *L);
static int _ese_poly_line_lua_tostring(lua_State *L);

// Lua constructors
static int _ese_poly_line_lua_new(lua_State *L);

// Lua utility methods
static int _ese_poly_line_lua_add_point(lua_State *L);
static int _ese_poly_line_lua_remove_point(lua_State *L);
static int _ese_poly_line_lua_get_point(lua_State *L);
static int _ese_poly_line_lua_get_point_count(lua_State *L);
static int _ese_poly_line_lua_clear_points(lua_State *L);

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
static EsePolyLine *_poly_line_make() {
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
static void _poly_line_notify_watchers(EsePolyLine *poly_line) {
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
            poly_line_destroy(poly_line);
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
    EsePolyLine *poly_line = poly_line_lua_get(L, 1);
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
    EsePolyLine *poly_line = poly_line_lua_get(L, 1);
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
        _poly_line_notify_watchers(poly_line);
        profile_stop(PROFILE_LUA_POLY_LINE_NEWINDEX, "poly_line_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "stroke_width") == 0) {
        if (!lua_isnumber(L, 3)) {
            profile_cancel(PROFILE_LUA_POLY_LINE_NEWINDEX);
            return luaL_error(L, "stroke_width must be a number");
        }
        poly_line->stroke_width = (float)lua_tonumber(L, 3);
        _poly_line_notify_watchers(poly_line);
        profile_stop(PROFILE_LUA_POLY_LINE_NEWINDEX, "poly_line_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "stroke_color") == 0) {
        if (lua_isnil(L, 3)) {
            poly_line->stroke_color = NULL;
        } else {
            EseColor *color = ese_color_lua_get(L, 3);
            if (!color) {
                profile_cancel(PROFILE_LUA_POLY_LINE_NEWINDEX);
                return luaL_error(L, "stroke_color must be a Color object or nil");
            }
            poly_line->stroke_color = color;
        }
        _poly_line_notify_watchers(poly_line);
        profile_stop(PROFILE_LUA_POLY_LINE_NEWINDEX, "poly_line_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "fill_color") == 0) {
        if (lua_isnil(L, 3)) {
            poly_line->fill_color = NULL;
        } else {
            EseColor *color = ese_color_lua_get(L, 3);
            if (!color) {
                profile_cancel(PROFILE_LUA_POLY_LINE_NEWINDEX);
                return luaL_error(L, "fill_color must be a Color object or nil");
            }
            poly_line->fill_color = color;
        }
        _poly_line_notify_watchers(poly_line);
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
    EsePolyLine *poly_line = poly_line_lua_get(L, 1);

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
    EsePolyLine *poly_line = _poly_line_make();
    poly_line->state = L;

    // Create userdata directly
    EsePolyLine **ud = (EsePolyLine **)lua_newuserdata(L, sizeof(EsePolyLine *));
    *ud = poly_line;

    // Attach metatable
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
    
    EsePolyLine *poly_line = poly_line_lua_get(L, 1);
    EsePoint *point = ese_point_lua_get(L, 2);
    
    if (!poly_line || !point) {
        profile_cancel(PROFILE_LUA_POLY_LINE_ADD_POINT);
        return luaL_error(L, "add_point requires a polyline and a point");
    }
    
    bool success = poly_line_add_point(poly_line, point);
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
    
    EsePolyLine *poly_line = poly_line_lua_get(L, 1);
    if (!poly_line) {
        profile_cancel(PROFILE_LUA_POLY_LINE_REMOVE_POINT);
        return luaL_error(L, "remove_point requires a polyline");
    }
    
    if (!lua_isnumber(L, 2)) {
        profile_cancel(PROFILE_LUA_POLY_LINE_REMOVE_POINT);
        return luaL_error(L, "Index must be a number");
    }
    
    size_t index = (size_t)lua_tonumber(L, 2);
    bool success = poly_line_remove_point(poly_line, index);
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
    
    EsePolyLine *poly_line = poly_line_lua_get(L, 1);
    if (!poly_line) {
        profile_cancel(PROFILE_LUA_POLY_LINE_GET_POINT);
        return luaL_error(L, "get_point requires a polyline");
    }
    
    if (!lua_isnumber(L, 2)) {
        profile_cancel(PROFILE_LUA_POLY_LINE_GET_POINT);
        return luaL_error(L, "Index must be a number");
    }
    
    size_t index = (size_t)lua_tonumber(L, 2);
    EsePoint *point = poly_line_get_point(poly_line, index);
    if (!point) {
        profile_cancel(PROFILE_LUA_POLY_LINE_GET_POINT);
        return luaL_error(L, "Invalid point index");
    }
    
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
    
    EsePolyLine *poly_line = poly_line_lua_get(L, 1);
    if (!poly_line) {
        profile_cancel(PROFILE_LUA_POLY_LINE_GET_POINT_COUNT);
        return luaL_error(L, "get_point_count requires a polyline");
    }
    
    lua_pushinteger(L, (int)poly_line_get_point_count(poly_line));
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
    
    EsePolyLine *poly_line = poly_line_lua_get(L, 1);
    if (!poly_line) {
        profile_cancel(PROFILE_LUA_POLY_LINE_CLEAR_POINTS);
        return luaL_error(L, "clear_points requires a polyline");
    }
    
    poly_line_clear_points(poly_line);
    profile_stop(PROFILE_LUA_POLY_LINE_CLEAR_POINTS, "poly_line_lua_clear_points");
    return 0;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EsePolyLine *poly_line_create(EseLuaEngine *engine) {
    log_assert("POLY_LINE", engine, "poly_line_create called with NULL engine");
    EsePolyLine *poly_line = _poly_line_make();
    poly_line->state = engine->runtime;
    return poly_line;
}

EsePolyLine *poly_line_copy(const EsePolyLine *source) {
    log_assert("POLY_LINE", source, "poly_line_copy called with NULL source");
    
    EsePolyLine *copy = (EsePolyLine *)memory_manager.malloc(sizeof(EsePolyLine), MMTAG_POLY_LINE);
    copy->type = source->type;
    copy->stroke_width = source->stroke_width;
    copy->stroke_color = source->stroke_color; // Shallow copy - shared reference
    copy->fill_color = source->fill_color;     // Shallow copy - shared reference
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
        copy->points = memory_manager.malloc(sizeof(EsePoint*) * source->point_capacity, MMTAG_POLY_LINE);
        for (size_t i = 0; i < source->point_count; i++) {
            copy->points[i] = ese_point_copy(source->points[i]);
        }
    } else {
        copy->points = NULL;
    }
    
    return copy;
}

void poly_line_destroy(EsePolyLine *poly_line) {
    if (!poly_line) return;
    
    // Free points array
    if (poly_line->points) {
        for (size_t i = 0; i < poly_line->point_count; i++) {
            ese_point_destroy(poly_line->points[i]);
        }
        memory_manager.free(poly_line->points);
        poly_line->points = NULL;
    }
    poly_line->point_count = 0;
    poly_line->point_capacity = 0;
    
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
    
    if (poly_line->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(poly_line);
    } else {
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
        poly_line_unref(poly_line);
    }
}

// Property access
void poly_line_set_type(EsePolyLine *poly_line, EsePolyLineType type) {
    log_assert("POLY_LINE", poly_line, "poly_line_set_type called with NULL poly_line");
    poly_line->type = type;
    _poly_line_notify_watchers(poly_line);
}

EsePolyLineType poly_line_get_type(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_type called with NULL poly_line");
    return poly_line->type;
}

void poly_line_set_stroke_width(EsePolyLine *poly_line, float width) {
    log_assert("POLY_LINE", poly_line, "poly_line_set_stroke_width called with NULL poly_line");
    poly_line->stroke_width = width;
    _poly_line_notify_watchers(poly_line);
}

float poly_line_get_stroke_width(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_stroke_width called with NULL poly_line");
    return poly_line->stroke_width;
}

void poly_line_set_stroke_color(EsePolyLine *poly_line, EseColor *color) {
    log_assert("POLY_LINE", poly_line, "poly_line_set_stroke_color called with NULL poly_line");
    poly_line->stroke_color = color;
    _poly_line_notify_watchers(poly_line);
}

EseColor *poly_line_get_stroke_color(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_stroke_color called with NULL poly_line");
    return poly_line->stroke_color;
}

void poly_line_set_fill_color(EsePolyLine *poly_line, EseColor *color) {
    log_assert("POLY_LINE", poly_line, "poly_line_set_fill_color called with NULL poly_line");
    poly_line->fill_color = color;
    _poly_line_notify_watchers(poly_line);
}

EseColor *poly_line_get_fill_color(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_fill_color called with NULL poly_line");
    return poly_line->fill_color;
}

// Points collection management
bool poly_line_add_point(EsePolyLine *poly_line, EsePoint *point) {
    log_assert("POLY_LINE", poly_line, "poly_line_add_point called with NULL poly_line");
    log_assert("POLY_LINE", point, "poly_line_add_point called with NULL point");
    
    // Initialize points array if this is the first point
    if (poly_line->point_count == 0) {
        poly_line->point_capacity = 4; // Start with capacity for 4 points
        poly_line->points = memory_manager.malloc(sizeof(EsePoint*) * poly_line->point_capacity, MMTAG_POLY_LINE);
        poly_line->point_count = 0;
    }
    
    // Expand array if needed
    if (poly_line->point_count >= poly_line->point_capacity) {
        size_t new_capacity = poly_line->point_capacity * 2;
        EsePoint **new_points = memory_manager.realloc(
            poly_line->points, 
            sizeof(EsePoint*) * new_capacity, 
            MMTAG_POLY_LINE
        );
        
        if (!new_points) return false;
        
        poly_line->points = new_points;
        poly_line->point_capacity = new_capacity;
    }
    
    // Add the new point
    poly_line->points[poly_line->point_count] = ese_point_copy(point);
    poly_line->point_count++;
    
    _poly_line_notify_watchers(poly_line);
    return true;
}

bool poly_line_remove_point(EsePolyLine *poly_line, size_t index) {
    log_assert("POLY_LINE", poly_line, "poly_line_remove_point called with NULL poly_line");
    
    if (index >= poly_line->point_count) {
        return false;
    }
    
    // Destroy the point at the index
    ese_point_destroy(poly_line->points[index]);
    
    // Shift remaining points
    for (size_t i = index; i < poly_line->point_count - 1; i++) {
        poly_line->points[i] = poly_line->points[i + 1];
    }
    
    poly_line->point_count--;
    _poly_line_notify_watchers(poly_line);
    return true;
}

EsePoint *poly_line_get_point(const EsePolyLine *poly_line, size_t index) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_point called with NULL poly_line");
    
    if (index >= poly_line->point_count) {
        return NULL;
    }
    
    return poly_line->points[index];
}

size_t poly_line_get_point_count(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_point_count called with NULL poly_line");
    return poly_line->point_count;
}

void poly_line_clear_points(EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_clear_points called with NULL poly_line");
    
    // Destroy all points
    for (size_t i = 0; i < poly_line->point_count; i++) {
        ese_point_destroy(poly_line->points[i]);
    }
    
    poly_line->point_count = 0;
    _poly_line_notify_watchers(poly_line);
}

// Lua-related access
lua_State *poly_line_get_state(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_state called with NULL poly_line");
    return poly_line->state;
}

int poly_line_get_lua_ref(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_lua_ref called with NULL poly_line");
    return poly_line->lua_ref;
}

int poly_line_get_lua_ref_count(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_lua_ref_count called with NULL poly_line");
    return poly_line->lua_ref_count;
}

// Watcher system
bool poly_line_add_watcher(EsePolyLine *poly_line, EsePolyLineWatcherCallback callback, void *userdata) {
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

bool poly_line_remove_watcher(EsePolyLine *poly_line, EsePolyLineWatcherCallback callback, void *userdata) {
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
void poly_line_lua_init(EseLuaEngine *engine) {
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
        lua_setglobal(engine->runtime, "PolyLine");
    } else {
        lua_pop(engine->runtime, 1); // Pop the existing polyline table
    }
}

void poly_line_lua_push(EsePolyLine *poly_line) {
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

EsePolyLine *poly_line_lua_get(lua_State *L, int idx) {
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

void poly_line_ref(EsePolyLine *poly_line) {
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

void poly_line_unref(EsePolyLine *poly_line) {
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

size_t poly_line_sizeof(void) {
    return sizeof(EsePolyLine);
}
