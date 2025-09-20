#include <math.h>
#include <stdio.h>
#include <string.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "scripting/lua_value.h"
#include "types/rect.h"
#include "types/point.h"
#include "types/types.h"
#include "utility/log.h"
#include "utility/profile.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// The actual EseRect struct definition (private to this file)
typedef struct EseRect {
    float x;            /**< The x-coordinate of the rectangle's top-left corner */
    float y;            /**< The y-coordinate of the rectangle's top-left corner */
    float width;        /**< The width of the rectangle */
    float height;       /**< The height of the rectangle */
    float rotation;     /**< The rotation of the rect around the center point in radians */

    int lua_ref;        /**< Lua registry reference to its own proxy table */
    int lua_ref_count;  /**< Number of times this rect has been referenced in C */
    
    // Watcher system
    EseRectWatcherCallback *watchers;     /**< Array of watcher callbacks */
    void **watcher_userdata;              /**< Array of userdata for each watcher */
    size_t watcher_count;                 /**< Number of registered watchers */
    size_t watcher_capacity;              /**< Capacity of the watcher arrays */
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
    Vec2 c;                         /**< Center point of the bounding box */
    Vec2 axis[2];                   /**< Normalized local axes in world space */
    float ext[2];                   /**< Half-widths along each axis */
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
static EseLuaValue* _ese_rect_lua_gc(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_rect_lua_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_rect_lua_newindex(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_rect_lua_tostring(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

// Lua constructors
static EseLuaValue* _ese_rect_lua_new(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_rect_lua_zero(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

// Lua methods
static EseLuaValue* _ese_rect_lua_area(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_rect_lua_contains_point(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_rect_lua_intersects(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

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
    rect->lua_ref = ESE_LUA_NOREF;
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
static EseLuaValue* _ese_rect_lua_gc(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return NULL;
    }

    // Get the rect from the first argument
    if (!lua_value_is_rect(argv[0])) {
        return NULL;
    }

    EseRect *rect = lua_value_get_rect(argv[0]);
    if (rect) {
        // If lua_ref == ESE_LUA_NOREF, there are no more references to this rect, 
        // so we can free it.
        // If lua_ref != ESE_LUA_NOREF, this rect was referenced from C and should not be freed.
        if (rect->lua_ref == ESE_LUA_NOREF) {
            ese_rect_destroy(rect);
        }
    }

    return NULL;
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
static EseLuaValue* _ese_rect_lua_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_RECT_INDEX);
    
    if (argc != 2) {
        profile_cancel(PROFILE_LUA_RECT_INDEX);
        return lua_value_create_nil("result");
    }
    
    // Get the rect from the first argument (should be rect)
    if (!lua_value_is_rect(argv[0])) {
        profile_cancel(PROFILE_LUA_RECT_INDEX);
        return lua_value_create_nil("result");
    }
    
    EseRect *rect = lua_value_get_rect(argv[0]);
    if (!rect) {
        profile_cancel(PROFILE_LUA_RECT_INDEX);
        return lua_value_create_nil("result");
    }
    
    // Get the key from the second argument (should be string)
    if (!lua_value_is_string(argv[1])) {
        profile_cancel(PROFILE_LUA_RECT_INDEX);
        return lua_value_create_nil("result");
    }
    
    const char *key = lua_value_get_string(argv[1]);
    if (!key) {
        profile_cancel(PROFILE_LUA_RECT_INDEX);
        return lua_value_create_nil("result");
    }

    if (strcmp(key, "x") == 0) {
        profile_stop(PROFILE_LUA_RECT_INDEX, "ese_rect_lua_index (getter)");
        return lua_value_create_number("result", rect->x);
    } else if (strcmp(key, "y") == 0) {
        profile_stop(PROFILE_LUA_RECT_INDEX, "ese_rect_lua_index (getter)");
        return lua_value_create_number("result", rect->y);
    } else if (strcmp(key, "width") == 0) {
        profile_stop(PROFILE_LUA_RECT_INDEX, "ese_rect_lua_index (getter)");
        return lua_value_create_number("result", rect->width);
    } else if (strcmp(key, "height") == 0) {
        profile_stop(PROFILE_LUA_RECT_INDEX, "ese_rect_lua_index (getter)");
        return lua_value_create_number("result", rect->height);
    } else if (strcmp(key, "rotation") == 0) {
        profile_stop(PROFILE_LUA_RECT_INDEX, "ese_rect_lua_index (getter)");
        return lua_value_create_number("result", (double)rad_to_deg(rect->rotation));
    } else if (strcmp(key, "contains_point") == 0) {
        EseLuaValue *rect_value = lua_value_create_rect("rect", rect);
        profile_stop(PROFILE_LUA_RECT_INDEX, "ese_rect_lua_index (method)");
        return lua_value_create_cfunc("contains_point", _ese_rect_lua_contains_point, rect_value);
    } else if (strcmp(key, "intersects") == 0) {
        EseLuaValue *rect_value = lua_value_create_rect("rect", rect);
        profile_stop(PROFILE_LUA_RECT_INDEX, "ese_rect_lua_index (method)");
        return lua_value_create_cfunc("intersects", _ese_rect_lua_intersects, rect_value);
    } else if (strcmp(key, "area") == 0) {
        EseLuaValue *rect_value = lua_value_create_rect("rect", rect);
        profile_stop(PROFILE_LUA_RECT_INDEX, "ese_rect_lua_index (method)");
        return lua_value_create_cfunc("area", _ese_rect_lua_area, rect_value);
    }
    
    profile_stop(PROFILE_LUA_RECT_INDEX, "ese_rect_lua_index (invalid)");
    return lua_value_create_nil("result");
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
static EseLuaValue* _ese_rect_lua_newindex(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_RECT_NEWINDEX);
    
    if (argc != 3) {
        profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
        return lua_value_create_error("result", "rect.x, rect.y, rect.width, rect.height, rect.rotation assignment requires exactly 3 arguments");
    }

    // Get the rect from the first argument (self)
    if (!lua_value_is_rect(argv[0])) {
        profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
        return lua_value_create_error("result", "rect property assignment: first argument must be a Rect");
    }
    
    EseRect *rect = lua_value_get_rect(argv[0]);
    if (!rect) {
        profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
        return lua_value_create_error("result", "rect property assignment: rect is NULL");
    }
    
    // Get the key
    if (!lua_value_is_string(argv[1])) {
        profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
        return lua_value_create_error("result", "rect property assignment: property name (x, y, width, height, rotation) must be a string");
    }
    
    const char *key = lua_value_get_string(argv[1]);
    
    if (strcmp(key, "x") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
            return lua_value_create_error("result", "rect.x must be a number");
        }
        rect->x = (float)lua_value_get_number(argv[2]);
        _ese_rect_notify_watchers(rect);
        profile_stop(PROFILE_LUA_RECT_NEWINDEX, "ese_rect_lua_newindex (setter)");
        return NULL;
    } else if (strcmp(key, "y") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
            return lua_value_create_error("result", "rect.y must be a number");
        }
        rect->y = (float)lua_value_get_number(argv[2]);
        _ese_rect_notify_watchers(rect);
        profile_stop(PROFILE_LUA_RECT_NEWINDEX, "ese_rect_lua_newindex (setter)");
        return NULL;
    } else if (strcmp(key, "width") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
            return lua_value_create_error("result", "rect.width must be a number");
        }
        rect->width = (float)lua_value_get_number(argv[2]);
        _ese_rect_notify_watchers(rect);
        profile_stop(PROFILE_LUA_RECT_NEWINDEX, "ese_rect_lua_newindex (setter)");
        return NULL;
    } else if (strcmp(key, "height") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
            return lua_value_create_error("result", "rect.height must be a number");
        }
        rect->height = (float)lua_value_get_number(argv[2]);
        _ese_rect_notify_watchers(rect);
        profile_stop(PROFILE_LUA_RECT_NEWINDEX, "ese_rect_lua_newindex (setter)");
        return NULL;
    } else if (strcmp(key, "rotation") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_RECT_NEWINDEX);
            return lua_value_create_error("result", "rect.rotation must be a number (degrees)");
        }
        float deg = (float)lua_value_get_number(argv[2]);
        rect->rotation = deg_to_rad(deg);
        _ese_rect_notify_watchers(rect);
        profile_stop(PROFILE_LUA_RECT_NEWINDEX, "ese_rect_lua_newindex (setter)");
        return NULL;
    }
    
    profile_stop(PROFILE_LUA_RECT_NEWINDEX, "ese_rect_lua_newindex (invalid)");
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "unknown or unassignable property '%s'", key);
    return lua_value_create_error("result", error_msg);
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
static EseLuaValue* _ese_rect_lua_tostring(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return lua_value_create_string("result", "Rect: (invalid)");
    }

    if (!lua_value_is_rect(argv[0])) {
        return lua_value_create_string("result", "Rect: (invalid)");
    }

    EseRect *rect = lua_value_get_rect(argv[0]);
    if (!rect) {
        return lua_value_create_string("result", "Rect: (invalid)");
    }

    char buf[160];
    snprintf(
        buf, sizeof(buf),
        "Rect: %p (x=%.2f, y=%.2f, w=%.2f, h=%.2f, rot=%.2fdeg)",
        (void*)rect, rect->x, rect->y, rect->width, rect->height,
        (double)rad_to_deg(rect->rotation)
    );
    
    return lua_value_create_string("result", buf);
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
static EseLuaValue* _ese_rect_lua_new(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_RECT_NEW);
    float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;

    if (argc == 4) {
        if (!lua_value_is_number(argv[0]) || !lua_value_is_number(argv[1]) || 
            !lua_value_is_number(argv[2]) || !lua_value_is_number(argv[3])) {
            profile_cancel(PROFILE_LUA_RECT_NEW);
            return lua_value_create_error("result", "all arguments must be numbers");
        }
        x = (float)lua_value_get_number(argv[0]);
        y = (float)lua_value_get_number(argv[1]);
        width = (float)lua_value_get_number(argv[2]);
        height = (float)lua_value_get_number(argv[3]);
    } else if (argc != 0) {
        profile_cancel(PROFILE_LUA_RECT_NEW);
        return lua_value_create_error("result", "new() takes 0 or 4 arguments (x, y, width, height)");
    }

    // Create the rect using the standard creation function
    EseRect *rect = _ese_rect_make();
    rect->x = x;
    rect->y = y;
    rect->width = width;
    rect->height = height;
    
    // Create userdata using the engine API
    EseRect **ud = (EseRect **)lua_engine_create_userdata(engine, "RectMeta", sizeof(EseRect *));
    *ud = rect;

    profile_stop(PROFILE_LUA_RECT_NEW, "ese_rect_lua_new");
    return lua_value_create_rect("result", rect);
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
static EseLuaValue* _ese_rect_lua_zero(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_RECT_ZERO);
    
    if (argc != 0) {
        profile_cancel(PROFILE_LUA_RECT_ZERO);
        return lua_value_create_error("result", "zero() takes no arguments");
    }
    
    // Create the rect using the standard creation function
    EseRect *rect = _ese_rect_make();  // We'll set the state manually
    
    // Create userdata using the engine API
    EseRect **ud = (EseRect **)lua_engine_create_userdata(engine, "RectMeta", sizeof(EseRect *));
    *ud = rect;

    profile_stop(PROFILE_LUA_RECT_ZERO, "ese_rect_lua_zero");
    return lua_value_create_rect("result", rect);
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
static EseLuaValue* _ese_rect_lua_area(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return lua_value_create_error("result", "rect:area() takes no arguments");
    }
    
    // First argument is the rect instance (from upvalue)
    if (!lua_value_is_rect(argv[0])) {
        return lua_value_create_error("result", "first argument must be a Rect");
    }
    
    EseRect *rect = lua_value_get_rect(argv[0]);
    if (!rect) {
        return lua_value_create_error("result", "rect is NULL");
    }
    
    float area = ese_rect_area(rect);
    return lua_value_create_number("result", area);
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
static EseLuaValue* _ese_rect_lua_contains_point(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc < 2 || argc > 3) {
        return lua_value_create_error("result", "rect:contains_point(x, y) or rect:contains_point(point) requires 1 or 2 arguments");
    }
    
    // First argument is the rect instance (from upvalue)
    if (!lua_value_is_rect(argv[0])) {
        return lua_value_create_error("result", "first argument must be a Rect");
    }
    
    EseRect *rect = lua_value_get_rect(argv[0]);
    if (!rect) {
        return lua_value_create_error("result", "rect is NULL");
    }
    
    float x, y;
    
    if (argc == 3) {
        if (!lua_value_is_number(argv[1]) || !lua_value_is_number(argv[2])) {
            return lua_value_create_error("result", "rect:contains_point(x, y) arguments must be numbers");
        }
        x = (float)lua_value_get_number(argv[1]);
        y = (float)lua_value_get_number(argv[2]);
    } else if (argc == 2) {
        if (!lua_value_is_point(argv[1])) {
            return lua_value_create_error("result", "rect:contains_point(point) requires a Point object");
        }
        EsePoint *point = lua_value_get_point(argv[1]);
        if (!point) {
            return lua_value_create_error("result", "point is NULL");
        }
        x = ese_point_get_x(point);
        y = ese_point_get_y(point);
    }
    
    bool result = ese_rect_contains_point(rect, x, y);
    return lua_value_create_bool("result", result);
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
static EseLuaValue* _ese_rect_lua_intersects(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 2) {
        return lua_value_create_error("result", "rect:intersects(rect) requires exactly 1 argument");
    }

    // First argument is the rect instance (from upvalue)
    if (!lua_value_is_rect(argv[0])) {
        return lua_value_create_error("result", "first argument must be a Rect");
    }
    
    EseRect *rect = lua_value_get_rect(argv[0]);
    if (!rect) {
        return lua_value_create_error("result", "rect is NULL");
    }
    
    if (!lua_value_is_rect(argv[1])) {
        return lua_value_create_error("result", "rect:intersects(rect) argument must be a Rect object");
    }
    
    EseRect *other = lua_value_get_rect(argv[1]);
    if (!other) {
        return lua_value_create_error("result", "other rect is NULL");
    }
    
    bool result = ese_rect_intersects(rect, other);
    return lua_value_create_bool("result", result);
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseRect *ese_rect_create(EseLuaEngine *engine) {
    EseRect *rect = _ese_rect_make();
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
    copy->lua_ref = ESE_LUA_NOREF;
    copy->lua_ref_count = 0;
    copy->watchers = NULL;
    copy->watcher_userdata = NULL;
    copy->watcher_count = 0;
    copy->watcher_capacity = 0;
    return copy;
}

void ese_rect_destroy(EseRect *rect) {
    if (!rect) return;
    
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
    
    if (rect->lua_ref == ESE_LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(rect);
    } else {
        // Don't call ese_rect_unref here - it needs an engine parameter
        // Just let Lua GC handle it
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

size_t ese_rect_sizeof(void) {
    return sizeof(EseRect);
}

// Lua integration
void ese_rect_lua_init(EseLuaEngine *engine) {
    // Add the metatable using the new API
    lua_engine_add_metatable(engine, "RectMeta", 
                            _ese_rect_lua_index, 
                            _ese_rect_lua_newindex, 
                            _ese_rect_lua_gc, 
                            _ese_rect_lua_tostring);
    
    // Create global Rect table with constructors
    const char *function_names[] = {"new", "zero"};
    EseLuaCFunction functions[] = {_ese_rect_lua_new, _ese_rect_lua_zero};
    lua_engine_add_globaltable(engine, "Rect", 2, function_names, functions);
}

void ese_rect_lua_push(EseLuaEngine *engine, EseRect *rect) {
    log_assert("RECT", engine, "ese_rect_lua_push called with NULL engine");
    log_assert("RECT", rect, "ese_rect_lua_push called with NULL rect");

    if (rect->lua_ref == ESE_LUA_NOREF) {
        // Lua-owned: create a new userdata using the engine API
        EseRect **ud = (EseRect **)lua_engine_create_userdata(engine, "RectMeta", sizeof(EseRect *));
        *ud = rect;
    } else {
        // C-owned: get from registry using the engine API
        lua_engine_get_registry_value(engine, rect->lua_ref);
    }
}

EseRect *ese_rect_lua_get(EseLuaEngine *engine, int idx) {
    log_assert("RECT", engine, "ese_rect_lua_get called with NULL engine");
    
    // Check if the value at idx is userdata
    if (!lua_engine_is_userdata(engine, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseRect **ud = (EseRect **)lua_engine_test_userdata(engine, idx, "RectMeta");
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_rect_ref(EseLuaEngine *engine, EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_ref called with NULL rect");
    
    if (rect->lua_ref == ESE_LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseRect **ud = (EseRect **)lua_engine_create_userdata(engine, "RectMeta", sizeof(EseRect *));
        *ud = rect;

        // Get the reference from the engine's Lua state
        rect->lua_ref = lua_engine_get_reference(engine);
        rect->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        rect->lua_ref_count++;
    }

    profile_count_add("ese_rect_ref_count");
}

void ese_rect_unref(EseLuaEngine *engine, EseRect *rect) {
    if (!rect) return;
    
    if (rect->lua_ref != ESE_LUA_NOREF && rect->lua_ref_count > 0) {
        rect->lua_ref_count--;
        
        if (rect->lua_ref_count == 0) {
            // No more references - remove from registry
            // Note: We need to use the engine API to unref
            // For now, just reset the ref
            rect->lua_ref = ESE_LUA_NOREF;
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
