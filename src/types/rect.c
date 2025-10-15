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

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// The actual EseRect struct definition (private to this file)
typedef struct EseRect {
    float x;            /** The x-coordinate of the rectangle's top-left corner */
    float y;            /** The y-coordinate of the rectangle's top-left corner */
    float width;        /** The width of the rectangle */
    float height;       /** The height of the rectangle */
    float rotation;     /** The rotation of the rect around the center point in radians */

    lua_State *state;   /** Lua State this EseRect belongs to */
    int lua_ref;        /** Lua registry reference to its own proxy table */
    int lua_ref_count;  /** Number of times this rect has been referenced in C */
    
    // Watcher system
    EseRectWatcherCallback *watchers;     /** Array of watcher callbacks */
    void **watcher_userdata;              /** Array of userdata for each watcher */
    size_t watcher_count;                 /** Number of registered watchers */
    size_t watcher_capacity;              /** Capacity of the watcher arrays */
} EseRect;

/**
 * @brief Simple 2D vector structure for mathematical operations.
 * 
 * @details This structure represents a 2D vector with x and y components
 *          for use in collision detection and geometric calculations.
 */
typedef struct { float x, y; } Vec2;

/**
 * @brief Oriented bounding box structure for collision detection.
 * 
 * @details This structure represents an oriented bounding box using
 *          center point, normalized axes, and half-widths for efficient
 *          collision detection using the Separating Axis Theorem (SAT).
 */
typedef struct {
    Vec2 c;                         /** Center point of the bounding box */
    Vec2 axis[2];                   /** Normalized local axes in world space */
    float ext[2];                   /** Half-widths along each axis */
} OBB;


/* mini helpers */
static inline float deg_to_rad(float d) { return d * (M_PI / 180.0f); }
static inline float rad_to_deg(float r) { return r * (180.0f / M_PI); }
static inline float dotf(Vec2 a, Vec2 b) { return a.x*b.x + a.y*b.y; }


// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseRect *_ese_rect_make(void);
static void _ese_rect_to_obb(const EseRect *r, OBB *out);

// Collision detection
static bool _ese_obb_overlap(const OBB *A, const OBB *B);

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

// Watcher system
static void _ese_rect_notify_watchers(EseRect *rect);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseRect instance with default values
 * 
 * Allocates memory for a new EseRect and initializes all fields to safe defaults.
 * The rect starts at origin (0,0) with zero dimensions and no rotation.
 * 
 * @return Pointer to the newly created EseRect, or NULL on allocation failure
 */
static EseRect *_ese_rect_make() {
    EseRect *rect = (EseRect *)memory_manager.malloc(sizeof(EseRect), MMTAG_RECT);
    rect->x = 0.0f;
    rect->y = 0.0f;
    rect->width = 0.0f;
    rect->height = 0.0f;
    rect->rotation = 0.0f;
    rect->state = NULL;
    rect->lua_ref = LUA_NOREF;
    rect->lua_ref_count = 0;
    rect->watchers = NULL;
    rect->watcher_userdata = NULL;
    rect->watcher_count = 0;
    rect->watcher_capacity = 0;
    return rect;
}

/**
 * @brief Converts an EseRect to an oriented bounding box (OBB) for collision detection
 * 
 * Transforms the axis-aligned rectangle into an oriented bounding box representation
 * using the Separating Axis Theorem (SAT) for efficient collision detection.
 * 
 * @param r Pointer to the source EseRect
 * @param out Pointer to the output OBB structure to be filled
 */
static void _ese_rect_to_obb(const EseRect *r, OBB *out) {
    float cx = r->x + r->width * 0.5f;
    float cy = r->y + r->height * 0.5f;
    float a = r->rotation;
    float ca = cosf(a);
    float sa = sinf(a);

    out->c.x = cx;
    out->c.y = cy;
    out->axis[0].x = ca; out->axis[0].y = sa;
    out->axis[1].x = -sa; out->axis[1].y = ca;
    out->ext[0] = r->width * 0.5f;
    out->ext[1] = r->height * 0.5f;
}

// Collision detection
/**
 * @brief Tests for overlap between two oriented bounding boxes
 * 
 * Implements the Separating Axis Theorem (SAT) to determine if two OBBs overlap.
 * Tests all possible separating axes to find a separation or confirm overlap.
 * 
 * @param A Pointer to the first OBB
 * @param B Pointer to the second OBB
 * @return true if the OBBs overlap, false otherwise
 */
static bool _ese_obb_overlap(const OBB *A, const OBB *B) {
    const float EPS = 1e-6f;
    Vec2 d = { B->c.x - A->c.x, B->c.y - A->c.y };
    /* test A's axes */
    for (int i = 0; i < 2; ++i) {
        Vec2 axis = A->axis[i];
        float ra = A->ext[i];
        float rb = B->ext[0] * fabsf(dotf(B->axis[0], axis))
                 + B->ext[1] * fabsf(dotf(B->axis[1], axis));
        float dist = fabsf(dotf(d, axis));
        if (dist > ra + rb + EPS) return false;
    }

    /* test B's axes */
    for (int i = 0; i < 2; ++i) {
        Vec2 axis = B->axis[i];
        float ra = A->ext[0] * fabsf(dotf(A->axis[0], axis))
                 + A->ext[1] * fabsf(dotf(A->axis[1], axis));
        float rb = B->ext[i];
        float dist = fabsf(dotf(d, axis));
        if (dist > ra + rb + EPS) return false;
    }

    return true;
}

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
        if (rect->lua_ref == LUA_NOREF) {
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
        lua_pushnumber(L, rect->x);
        profile_stop(PROFILE_LUA_RECT_INDEX, "rect_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(L, rect->y);
        profile_stop(PROFILE_LUA_RECT_INDEX, "rect_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "width") == 0) {
        lua_pushnumber(L, rect->width);
        profile_stop(PROFILE_LUA_RECT_INDEX, "rect_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "height") == 0) {
        lua_pushnumber(L, rect->height);
        profile_stop(PROFILE_LUA_RECT_INDEX, "rect_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "rotation") == 0) {
        lua_pushnumber(L, (double)rad_to_deg(rect->rotation));
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
        rect->x = (float)lua_tonumber(L, 3);
        _ese_rect_notify_watchers(rect);
        profile_stop(PROFILE_LUA_RECT_NEWINDEX, "rect_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "y") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
            return luaL_error(L, "rect.y must be a number");
        }
        rect->y = (float)lua_tonumber(L, 3);
        _ese_rect_notify_watchers(rect);
        profile_stop(PROFILE_LUA_RECT_NEWINDEX, "rect_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "width") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
            return luaL_error(L, "rect.width must be a number");
        }
        rect->width = (float)lua_tonumber(L, 3);
        _ese_rect_notify_watchers(rect);
        profile_stop(PROFILE_LUA_RECT_NEWINDEX, "rect_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "height") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
            return luaL_error(L, "rect.height must be a number");
        }
        rect->height = (float)lua_tonumber(L, 3);
        _ese_rect_notify_watchers(rect);
        profile_stop(PROFILE_LUA_RECT_NEWINDEX, "rect_lua_newindex (setter)");
        return 0;
    }  else if (strcmp(key, "rotation") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
            return luaL_error(L, "rect.rotation must be a number (degrees)");
        }
        float deg = (float)lua_tonumber(L, 3);
        rect->rotation = deg_to_rad(deg);
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
        rect->x, rect->y, rect->width, rect->height,
        (double)rad_to_deg(rect->rotation)
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

    // Create the rect using the standard creation function
    EseRect *rect = _ese_rect_make();
    rect->x = x;
    rect->y = y;
    rect->width = width;
    rect->height = height;
    rect->state = L;
    
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

    // Create the rect using the standard creation function
    EseRect *rect = _ese_rect_make();  // We'll set the state manually
    rect->state = L;
    
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

// Core lifecycle
EseRect *ese_rect_create(EseLuaEngine *engine) {
    log_assert("RECT", engine, "ese_rect_create called with NULL engine");
    EseRect *rect = _ese_rect_make();
    rect->state = engine->runtime;
    return rect;
}

EseRect *ese_rect_copy(const EseRect *source) {
    log_assert("RECT", source, "ese_rect_copy called with NULL source");
    
    EseRect *copy = (EseRect *)memory_manager.malloc(sizeof(EseRect), MMTAG_RECT);
    copy->x = source->x;
    copy->y = source->y;
    copy->width = source->width;
    copy->height = source->height;
    copy->rotation = source->rotation;
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
    copy->lua_ref_count = 0;
    copy->watchers = NULL;
    copy->watcher_userdata = NULL;
    copy->watcher_count = 0;
    copy->watcher_capacity = 0;
    return copy;
}

void ese_rect_destroy(EseRect *rect) {
    if (!rect) return;
    
    if (rect->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
    
        // Free watcher arrays if they exist
        if (rect->watchers) {
            memory_manager.free(rect->watchers);
            rect->watchers = NULL;
        }
        if (rect->watcher_userdata) {
            memory_manager.free(rect->watcher_userdata);
            rect->watcher_userdata = NULL;
        }
        rect->watcher_count = 0;
        rect->watcher_capacity = 0;

        memory_manager.free(rect);
    } else {
        ese_rect_unref(rect);
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

size_t ese_rect_sizeof(void) {
    return sizeof(EseRect);
}

/**
 * @brief Serializes an EseRect to a cJSON object.
 *
 * Creates a cJSON object representing the rect with type "RECT"
 * and x, y, width, height, rotation coordinates. Only serializes the
 * coordinate and dimension data, not Lua-related fields.
 *
 * @param rect Pointer to the EseRect object to serialize
 * @return cJSON object representing the rect, or NULL on failure
 */
cJSON *ese_rect_serialize(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_serialize called with NULL rect");

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        log_error("RECT", "Failed to create cJSON object for rect serialization");
        return NULL;
    }

    // Add type field
    cJSON *type = cJSON_CreateString("RECT");
    if (!type || !cJSON_AddItemToObject(json, "type", type)) {
        log_error("RECT", "Failed to add type field to rect serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add x coordinate
    cJSON *x = cJSON_CreateNumber((double)rect->x);
    if (!x || !cJSON_AddItemToObject(json, "x", x)) {
        log_error("RECT", "Failed to add x field to rect serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add y coordinate
    cJSON *y = cJSON_CreateNumber((double)rect->y);
    if (!y || !cJSON_AddItemToObject(json, "y", y)) {
        log_error("RECT", "Failed to add y field to rect serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add width
    cJSON *width = cJSON_CreateNumber((double)rect->width);
    if (!width || !cJSON_AddItemToObject(json, "width", width)) {
        log_error("RECT", "Failed to add width field to rect serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add height
    cJSON *height = cJSON_CreateNumber((double)rect->height);
    if (!height || !cJSON_AddItemToObject(json, "height", height)) {
        log_error("RECT", "Failed to add height field to rect serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add rotation
    cJSON *rotation = cJSON_CreateNumber((double)rad_to_deg(rect->rotation));
    if (!rotation || !cJSON_AddItemToObject(json, "rotation", rotation)) {
        log_error("RECT", "Failed to add rotation field to rect serialization");
        cJSON_Delete(json);
        return NULL;
    }

    return json;
}

/**
 * @brief Deserializes an EseRect from a cJSON object.
 *
 * Creates a new EseRect from a cJSON object with type "RECT"
 * and x, y, width, height, rotation coordinates. The rect is created
 * with the specified engine and must be explicitly referenced with
 * ese_rect_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for rect creation
 * @param data cJSON object containing rect data
 * @return Pointer to newly created EseRect object, or NULL on failure
 */
EseRect *ese_rect_deserialize(EseLuaEngine *engine, const cJSON *data) {
    log_assert("RECT", data, "ese_rect_deserialize called with NULL data");

    if (!cJSON_IsObject(data)) {
        log_error("RECT", "Rect deserialization failed: data is not a JSON object");
        return NULL;
    }

    // Check type field
    cJSON *type_item = cJSON_GetObjectItem(data, "type");
    if (!type_item || !cJSON_IsString(type_item) || strcmp(type_item->valuestring, "RECT") != 0) {
        log_error("RECT", "Rect deserialization failed: invalid or missing type field");
        return NULL;
    }

    // Get x coordinate
    cJSON *x_item = cJSON_GetObjectItem(data, "x");
    if (!x_item || !cJSON_IsNumber(x_item)) {
        log_error("RECT", "Rect deserialization failed: invalid or missing x field");
        return NULL;
    }

    // Get y coordinate
    cJSON *y_item = cJSON_GetObjectItem(data, "y");
    if (!y_item || !cJSON_IsNumber(y_item)) {
        log_error("RECT", "Rect deserialization failed: invalid or missing y field");
        return NULL;
    }

    // Get width
    cJSON *width_item = cJSON_GetObjectItem(data, "width");
    if (!width_item || !cJSON_IsNumber(width_item)) {
        log_error("RECT", "Rect deserialization failed: invalid or missing width field");
        return NULL;
    }

    // Get height
    cJSON *height_item = cJSON_GetObjectItem(data, "height");
    if (!height_item || !cJSON_IsNumber(height_item)) {
        log_error("RECT", "Rect deserialization failed: invalid or missing height field");
        return NULL;
    }

    // Get rotation (optional field)
    cJSON *rotation_item = cJSON_GetObjectItem(data, "rotation");
    float rotation = 0.0f;
    if (rotation_item && cJSON_IsNumber(rotation_item)) {
        rotation = deg_to_rad((float)rotation_item->valuedouble);
    }

    // Create new rect
    EseRect *rect = ese_rect_create(engine);
    ese_rect_set_x(rect, (float)x_item->valuedouble);
    ese_rect_set_y(rect, (float)y_item->valuedouble);
    ese_rect_set_width(rect, (float)width_item->valuedouble);
    ese_rect_set_height(rect, (float)height_item->valuedouble);
    ese_rect_set_rotation(rect, rotation);

    return rect;
}

// Lua integration
void ese_rect_lua_init(EseLuaEngine *engine) {
    log_assert("RECT", engine, "ese_rect_lua_init called with NULL engine");
    if (luaL_newmetatable(engine->runtime, RECT_PROXY_META)) {
        log_debug("LUA", "Adding entity RectMeta to engine");
        lua_pushstring(engine->runtime, RECT_PROXY_META);
        lua_setfield(engine->runtime, -2, "__name");
        lua_pushcfunction(engine->runtime, _ese_rect_lua_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _ese_rect_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushcfunction(engine->runtime, _ese_rect_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");
        lua_pushcfunction(engine->runtime, _ese_rect_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
    
    // Create global EseRect table with constructor
    lua_getglobal(engine->runtime, "Rect");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);
        log_debug("LUA", "Creating global EseRect table");
        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _ese_rect_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_pushcfunction(engine->runtime, _ese_rect_lua_zero);
        lua_setfield(engine->runtime, -2, "zero");
        lua_pushcfunction(engine->runtime, _ese_rect_lua_from_json);
        lua_setfield(engine->runtime, -2, "fromJSON");
        lua_setglobal(engine->runtime, "Rect");
    } else {
        lua_pop(engine->runtime, 1);
    }
}

void ese_rect_lua_push(EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_lua_push called with NULL rect");

    if (rect->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseRect **ud = (EseRect **)lua_newuserdata(rect->state, sizeof(EseRect *));
        *ud = rect;

        // Attach metatable
        luaL_getmetatable(rect->state, RECT_PROXY_META);
        lua_setmetatable(rect->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(rect->state, LUA_REGISTRYINDEX, rect->lua_ref);
    }
}

EseRect *ese_rect_lua_get(lua_State *L, int idx) {
    log_assert("RECT", L, "ese_rect_lua_get called with NULL Lua state");
    
    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseRect **ud = (EseRect **)luaL_testudata(L, idx, RECT_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_rect_ref(EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_ref called with NULL rect");
    
    if (rect->lua_ref == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseRect **ud = (EseRect **)lua_newuserdata(rect->state, sizeof(EseRect *));
        *ud = rect;

        // Attach metatable
        luaL_getmetatable(rect->state, RECT_PROXY_META);
        lua_setmetatable(rect->state, -2);

        // Store hard reference to prevent garbage collection
        rect->lua_ref = luaL_ref(rect->state, LUA_REGISTRYINDEX);
        rect->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        rect->lua_ref_count++;
    }

    profile_count_add("ese_rect_ref_count");
}

void ese_rect_unref(EseRect *rect) {
    if (!rect) return;
    
    if (rect->lua_ref != LUA_NOREF && rect->lua_ref_count > 0) {
        rect->lua_ref_count--;
        
        if (rect->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(rect->state, LUA_REGISTRYINDEX, rect->lua_ref);
            rect->lua_ref = LUA_NOREF;
        }
    }

    profile_count_add("ese_rect_unref_count");
}

// Mathematical operations
bool ese_rect_contains_point(const EseRect *rect, float x, float y) {
    log_assert("RECT", rect, "ese_rect_contains_point called with NULL rect");

    /* fast AABB path */
    if (fabsf(rect->rotation) < 1e-6f) {
        return (x >= rect->x && x <= rect->x + rect->width &&
                y >= rect->y && y <= rect->y + rect->height);
    }

    /* transform point to rect-local coordinates by rotating around center by -rotation */
    float cx = rect->x + rect->width * 0.5f;
    float cy = rect->y + rect->height * 0.5f;
    float ca = cosf(rect->rotation);
    float sa = sinf(rect->rotation);

    float dx = x - cx;
    float dy = y - cy;

    float localX = ca * dx + sa * dy;
    float localY = -sa * dx + ca * dy;

    float halfW = rect->width * 0.5f;
    float halfH = rect->height * 0.5f;

    return (localX >= -halfW && localX <= halfW && localY >= -halfH && localY <= halfH);
}

bool ese_rect_intersects(const EseRect *rect1, const EseRect *rect2) {
    log_assert("RECT", rect1, "ese_rect_intersects called with NULL first rect");
    log_assert("RECT", rect2, "ese_rect_intersects called with NULL second rect");

    /* fast AABB path if both rotations are effectively zero */
    if (fabsf(rect1->rotation) < 1e-6f && fabsf(rect2->rotation) < 1e-6f) {
        return !(rect1->x > rect2->x + rect2->width ||
                 rect2->x > rect1->x + rect1->width ||
                 rect1->y > rect2->y + rect2->height ||
                 rect2->y > rect1->y + rect1->height);
    }

    OBB a, b;
    _ese_rect_to_obb(rect1, &a);
    _ese_rect_to_obb(rect2, &b);

    return _ese_obb_overlap(&a, &b);
}

float ese_rect_area(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_area called with NULL rect");
    return rect->width * rect->height;
}

// Property access
void ese_rect_set_rotation(EseRect *rect, float radians) {
    log_assert("RECT", rect, "ese_rect_set_rotation called with NULL rect");
    rect->rotation = radians;
    _ese_rect_notify_watchers(rect);
}

float ese_rect_get_rotation(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_get_rotation called with NULL rect");
    return rect->rotation;
}

void ese_rect_set_x(EseRect *rect, float x) {
    log_assert("RECT", rect, "ese_rect_set_x called with NULL rect");
    rect->x = x;
    _ese_rect_notify_watchers(rect);
}

float ese_rect_get_x(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_get_x called with NULL rect");
    return rect->x;
}

void ese_rect_set_y(EseRect *rect, float y) {
    log_assert("RECT", rect, "ese_rect_set_y called with NULL rect");
    rect->y = y;
    _ese_rect_notify_watchers(rect);
}

float ese_rect_get_y(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_get_y called with NULL rect");
    return rect->y;
}

void ese_rect_set_width(EseRect *rect, float width) {
    log_assert("RECT", rect, "ese_rect_set_width called with NULL rect");
    rect->width = width;
    _ese_rect_notify_watchers(rect);
}

float ese_rect_get_width(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_get_width called with NULL rect");
    return rect->width;
}

void ese_rect_set_height(EseRect *rect, float height) {
    log_assert("RECT", rect, "ese_rect_set_height called with NULL rect");
    rect->height = height;
    _ese_rect_notify_watchers(rect);
}

float ese_rect_get_height(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_get_height called with NULL rect");
    return rect->height;
}

// Lua-related access
lua_State *ese_rect_get_state(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_get_state called with NULL rect");
    return rect->state;
}

int ese_rect_get_lua_ref(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_get_lua_ref called with NULL rect");
    return rect->lua_ref;
}

int ese_rect_get_lua_ref_count(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_get_lua_ref_count called with NULL rect");
    return rect->lua_ref_count;
}

// Watcher system
bool ese_rect_add_watcher(EseRect *rect, EseRectWatcherCallback callback, void *userdata) {
    log_assert("RECT", rect, "ese_rect_add_watcher called with NULL rect");
    log_assert("RECT", callback, "ese_rect_add_watcher called with NULL callback");
    
    // Initialize watcher arrays if this is the first watcher
    if (rect->watcher_count == 0) {
        rect->watcher_capacity = 4; // Start with capacity for 4 watchers
        rect->watchers = memory_manager.malloc(sizeof(EseRectWatcherCallback) * rect->watcher_capacity, MMTAG_RECT);
        rect->watcher_userdata = memory_manager.malloc(sizeof(void*) * rect->watcher_capacity, MMTAG_RECT);
        rect->watcher_count = 0;
    }
    
    // Expand arrays if needed
    if (rect->watcher_count >= rect->watcher_capacity) {
        size_t new_capacity = rect->watcher_capacity * 2;
        EseRectWatcherCallback *new_watchers = memory_manager.realloc(
            rect->watchers, 
            sizeof(EseRectWatcherCallback) * new_capacity, 
            MMTAG_RECT
        );
        void **new_userdata = memory_manager.realloc(
            rect->watcher_userdata, 
            sizeof(void*) * new_capacity, 
            MMTAG_RECT
        );
        
        if (!new_watchers || !new_userdata) return false;
        
        rect->watchers = new_watchers;
        rect->watcher_userdata = new_userdata;
        rect->watcher_capacity = new_capacity;
    }
    
    // Add the new watcher
    rect->watchers[rect->watcher_count] = callback;
    rect->watcher_userdata[rect->watcher_count] = userdata;
    rect->watcher_count++;
    
    return true;
}

bool ese_rect_remove_watcher(EseRect *rect, EseRectWatcherCallback callback, void *userdata) {
    log_assert("RECT", rect, "ese_rect_remove_watcher called with NULL rect");
    log_assert("RECT", callback, "ese_rect_remove_watcher called with NULL callback");
    
    for (size_t i = 0; i < rect->watcher_count; i++) {
        if (rect->watchers[i] == callback && rect->watcher_userdata[i] == userdata) {
            // Remove this watcher by shifting remaining ones
            for (size_t j = i; j < rect->watcher_count - 1; j++) {
                rect->watchers[j] = rect->watchers[j + 1];
                rect->watcher_userdata[j] = rect->watcher_userdata[j + 1];
            }
            rect->watcher_count--;
            return true;
        }
    }
    
    return false;
}

// Helper function to notify all watchers
/**
 * @brief Notifies all registered watchers of a rect change
 * 
 * Iterates through all registered watcher callbacks and invokes them with the
 * updated rect and their associated userdata. This is called whenever any
 * rect property (x, y, width, height, rotation) is modified.
 * 
 * @param rect Pointer to the EseRect that has changed
 */
static void _ese_rect_notify_watchers(EseRect *rect) {
    if (!rect || rect->watcher_count == 0) return;
    
    for (size_t i = 0; i < rect->watcher_count; i++) {
        if (rect->watchers[i]) {
            rect->watchers[i](rect, rect->watcher_userdata[i]);
        }
    }
}

// Lua integration
