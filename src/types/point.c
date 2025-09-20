#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stddef.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "scripting/lua_engine_private.h"
#include "scripting/lua_value.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/types.h"
#include "types/point.h"

// The actual EsePoint struct definition (private to this file)
typedef struct EsePoint {
    float x;            /**< The x-coordinate of the point */
    float y;            /**< The y-coordinate of the point */

    int lua_ref;        /**< Lua registry reference to its own proxy table */
    int lua_ref_count;  /**< Number of times this point has been referenced in C */
    
    // Watcher system
    EsePointWatcherCallback *watchers;     /**< Array of watcher callbacks */
    void **watcher_userdata;               /**< Array of userdata for each watcher */
    size_t watcher_count;                  /**< Number of registered watchers */
    size_t watcher_capacity;               /**< Capacity of the watcher arrays */
} EsePoint;

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EsePoint *_ese_point_make(void);

// Watcher system
static void _ese_point_make_point_notify_watchers(EsePoint *point);

// Lua metamethods
static EseLuaValue* _ese_point_lua_gc(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_point_lua_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_point_lua_newindex(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_point_lua_tostring(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

// Lua constructors
static EseLuaValue* _ese_point_lua_new(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_point_lua_zero(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_point_lua_distance(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EsePoint instance with default values
 * 
 * Allocates memory for a new EsePoint and initializes all fields to safe defaults.
 * The point starts at origin (0,0) with no Lua state or watchers.
 * 
 * @return Pointer to the newly created EsePoint, or NULL on allocation failure
 */
static EsePoint *_ese_point_make() {
    EsePoint *point = (EsePoint *)memory_manager.malloc(sizeof(EsePoint), MMTAG_POINT);
    point->x = 0.0f;
    point->y = 0.0f;
    point->lua_ref = ESE_LUA_NOREF;
    point->lua_ref_count = 0;
    point->watchers = NULL;
    point->watcher_userdata = NULL;
    point->watcher_count = 0;
    point->watcher_capacity = 0;
    return point;
}

// Watcher system
/**
 * @brief Notifies all registered watchers of a point change
 * 
 * Iterates through all registered watcher callbacks and invokes them with the
 * updated point and their associated userdata. This is called whenever the
 * point's x or y coordinates are modified.
 * 
 * @param point Pointer to the EsePoint that has changed
 */
static void _ese_point_make_point_notify_watchers(EsePoint *point) {
    if (!point || point->watcher_count == 0) return;
    
    for (size_t i = 0; i < point->watcher_count; i++) {
        if (point->watchers[i]) {
            point->watchers[i](point, point->watcher_userdata[i]);
        }
    }
}

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EsePoint
 * 
 * Handles cleanup when a Lua proxy table for an EsePoint is garbage collected.
 * Only frees the underlying EsePoint if it has no C-side references.
 * 
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static EseLuaValue* _ese_point_lua_gc(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return NULL;
    }

    // Get the point from the first argument
    if (!lua_value_is_point(argv[0])) {
        return NULL;
    }

    EsePoint *point = lua_value_get_point(argv[0]);
    if (point) {
        // If lua_ref == ESE_LUA_NOREF, there are no more references to this point, 
        // so we can free it.
        // If lua_ref != ESE_LUA_NOREF, this point was referenced from C and should not be freed.
        if (point->lua_ref == ESE_LUA_NOREF) {
            ese_point_destroy(point);
        }
    }

    return NULL;
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
static EseLuaValue* _ese_point_lua_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_POINT_INDEX);
    
    if (argc != 2) {
        profile_cancel(PROFILE_LUA_POINT_INDEX);
        return lua_value_create_nil("result");
    }
    
    // Get the point from the first argument (should be point)
    if (!lua_value_is_point(argv[0])) {
        profile_cancel(PROFILE_LUA_POINT_INDEX);
        return lua_value_create_nil("result");
    }
    
    EsePoint *point = lua_value_get_point(argv[0]);
    if (!point) {
        profile_cancel(PROFILE_LUA_POINT_INDEX);
        return lua_value_create_nil("result");
    }
    
    // Get the key from the second argument (should be string)
    if (!lua_value_is_string(argv[1])) {
        profile_cancel(PROFILE_LUA_POINT_INDEX);
        return lua_value_create_nil("result");
    }
    
    const char *key = lua_value_get_string(argv[1]);
    if (!key) {
        profile_cancel(PROFILE_LUA_POINT_INDEX);
        return lua_value_create_nil("result");
    }

    if (strcmp(key, "x") == 0) {
        profile_stop(PROFILE_LUA_POINT_INDEX, "ese_point_lua_index (getter)");
        return lua_value_create_number("result", point->x);
    } else if (strcmp(key, "y") == 0) {
        profile_stop(PROFILE_LUA_POINT_INDEX, "ese_point_lua_index (getter)");
        return lua_value_create_number("result", point->y);
    }
    
    profile_stop(PROFILE_LUA_POINT_INDEX, "ese_point_lua_index (invalid)");
    return lua_value_create_nil("result");
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
static EseLuaValue* _ese_point_lua_newindex(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_POINT_NEWINDEX);
    
    if (argc != 3) {
        profile_cancel(PROFILE_LUA_POINT_NEWINDEX);
        return lua_value_create_error("result", "point.x, point.y assignment requires exactly 3 arguments");
    }

    // Get the point from the first argument (self)
    if (!lua_value_is_point(argv[0])) {
        profile_cancel(PROFILE_LUA_POINT_NEWINDEX);
        return lua_value_create_error("result", "point property assignment: first argument must be a Point");
    }
    
    EsePoint *point = lua_value_get_point(argv[0]);
    if (!point) {
        profile_cancel(PROFILE_LUA_POINT_NEWINDEX);
        return lua_value_create_error("result", "point property assignment: point is NULL");
    }
    
    // Get the key
    if (!lua_value_is_string(argv[1])) {
        profile_cancel(PROFILE_LUA_POINT_NEWINDEX);
        return lua_value_create_error("result", "point property assignment: property name (x, y) must be a string");
    }
    
    const char *key = lua_value_get_string(argv[1]);
    
    if (strcmp(key, "x") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_POINT_NEWINDEX);
            return lua_value_create_error("result", "point.x must be a number");
        }
        point->x = (float)lua_value_get_number(argv[2]);
        _ese_point_make_point_notify_watchers(point);
        profile_stop(PROFILE_LUA_POINT_NEWINDEX, "ese_point_lua_newindex (setter)");
        return NULL;
    } else if (strcmp(key, "y") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_POINT_NEWINDEX);
            return lua_value_create_error("result", "point.y must be a number");
        }
        point->y = (float)lua_value_get_number(argv[2]);
        _ese_point_make_point_notify_watchers(point);
        profile_stop(PROFILE_LUA_POINT_NEWINDEX, "ese_point_lua_newindex (setter)");
        return NULL;
    }
    
    profile_stop(PROFILE_LUA_POINT_NEWINDEX, "ese_point_lua_newindex (invalid)");
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "unknown or unassignable property '%s'", key);
    return lua_value_create_error("result", error_msg);
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
static EseLuaValue* _ese_point_lua_tostring(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return lua_value_create_string("result", "Point: (invalid)");
    }

    if (!lua_value_is_point(argv[0])) {
        return lua_value_create_string("result", "Point: (invalid)");
    }

    EsePoint *point = lua_value_get_point(argv[0]);
    if (!point) {
        return lua_value_create_string("result", "Point: (invalid)");
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "Point: %p (x=%.2f, y=%.2f)", (void*)point, point->x, point->y);
    
    return lua_value_create_string("result", buf);
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
static EseLuaValue* _ese_point_lua_new(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_POINT_NEW);
    float x = 0.0f, y = 0.0f;

    if (argc == 2) {
        if (!lua_value_is_number(argv[0]) || !lua_value_is_number(argv[1])) {
            profile_cancel(PROFILE_LUA_POINT_NEW);
            return lua_value_create_error("result", "all arguments must be numbers");
        }
        x = (float)lua_value_get_number(argv[0]);
        y = (float)lua_value_get_number(argv[1]);
    } else if (argc != 0) {
        profile_cancel(PROFILE_LUA_POINT_NEW);
        return lua_value_create_error("result", "new() takes 0 or 2 arguments (x, y)");
    }

    // Create the point using the standard creation function
    EsePoint *point = _ese_point_make();
    point->x = x;
    point->y = y;
    
    // Create userdata using the engine API
    EsePoint **ud = (EsePoint **)lua_engine_create_userdata(engine, "PointMeta", sizeof(EsePoint *));
    *ud = point;

    profile_stop(PROFILE_LUA_POINT_NEW, "ese_point_lua_new");
    return lua_value_create_point("result", point);
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
static EseLuaValue* _ese_point_lua_zero(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_POINT_ZERO);
    
    if (argc != 0) {
        profile_cancel(PROFILE_LUA_POINT_ZERO);
        return lua_value_create_error("result", "zero() takes no arguments");
    }
    
    // Create the point using the standard creation function
    EsePoint *point = _ese_point_make();  // We'll set the state manually
    
    // Create userdata using the engine API
    EsePoint **ud = (EsePoint **)lua_engine_create_userdata(engine, "PointMeta", sizeof(EsePoint *));
    *ud = point;

    profile_stop(PROFILE_LUA_POINT_ZERO, "ese_point_lua_zero");
    return lua_value_create_point("result", point);
}

static EseLuaValue* _ese_point_lua_distance(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_POINT_DISTANCE);

    if (argc != 2) {
        profile_cancel(PROFILE_LUA_POINT_DISTANCE);
        return lua_value_create_error("result", "Point.distance(point, point) takes 2 arguments");
    }

    if (!lua_value_is_point(argv[0]) || !lua_value_is_point(argv[1])) {
        profile_cancel(PROFILE_LUA_POINT_DISTANCE);
        return lua_value_create_error("result", "Point.distance(point, point) arguments must be points");
    }

    EsePoint *point1 = lua_value_get_point(argv[0]);
    EsePoint *point2 = lua_value_get_point(argv[1]);

    if (!point1 || !point2) {
        profile_cancel(PROFILE_LUA_POINT_DISTANCE);
        return lua_value_create_error("result", "points are NULL");
    }

    float distance = ese_point_distance(point1, point2);
    profile_stop(PROFILE_LUA_POINT_DISTANCE, "ese_point_lua_distance");
    return lua_value_create_number("result", distance);
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EsePoint *ese_point_create(EseLuaEngine *engine) {
    EsePoint *point = _ese_point_make();
    return point;
}

EsePoint *ese_point_copy(const EsePoint *source) {
    log_assert("POINT", source, "ese_point_copy called with NULL source");
    
    EsePoint *copy = (EsePoint *)memory_manager.malloc(sizeof(EsePoint), MMTAG_POINT);
    copy->x = source->x;
    copy->y = source->y;
    copy->lua_ref = ESE_LUA_NOREF;
    copy->lua_ref_count = 0;
    copy->watchers = NULL;
    copy->watcher_userdata = NULL;
    copy->watcher_count = 0;
    copy->watcher_capacity = 0;
    return copy;
}

void ese_point_destroy(EsePoint *point) {
    if (!point) return;
    
    // Free watcher arrays if they exist
    if (point->watchers) {
        memory_manager.free(point->watchers);
        point->watchers = NULL;
    }
    if (point->watcher_userdata) {
        memory_manager.free(point->watcher_userdata);
        point->watcher_userdata = NULL;
    }
    point->watcher_count = 0;
    point->watcher_capacity = 0;
    
    if (point->lua_ref == ESE_LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(point);
    } else {
        // Don't call ese_point_unref here - it needs an engine parameter
        // Just let Lua GC handle it
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

// Lua integration
void ese_point_lua_init(EseLuaEngine *engine) {
    // Add the metatable using the new API
    lua_engine_add_metatable(engine, "PointMeta", 
                            _ese_point_lua_index, 
                            _ese_point_lua_newindex, 
                            _ese_point_lua_gc, 
                            _ese_point_lua_tostring);
    
    // Create global Point table with constructors
    const char *function_names[] = {"new", "zero", "distance"};
    EseLuaCFunction functions[] = {_ese_point_lua_new, _ese_point_lua_zero, _ese_point_lua_distance};
    lua_engine_add_globaltable(engine, "Point", 3, function_names, functions);
}

void ese_point_lua_push(EseLuaEngine *engine, EsePoint *point) {
    log_assert("POINT", engine, "ese_point_lua_push called with NULL engine");
    log_assert("POINT", point, "ese_point_lua_push called with NULL point");

    if (point->lua_ref == ESE_LUA_NOREF) {
        // Lua-owned: create a new userdata using the engine API
        EsePoint **ud = (EsePoint **)lua_engine_create_userdata(engine, "PointMeta", sizeof(EsePoint *));
        *ud = point;
    } else {
        // C-owned: get from registry using the engine API
        lua_engine_get_registry_value(engine, point->lua_ref);
    }
}

EsePoint *ese_point_lua_get(EseLuaEngine *engine, int idx) {
    log_assert("POINT", engine, "ese_point_lua_get called with NULL engine");
    
    // Check if the value at idx is userdata
    if (!lua_engine_is_userdata(engine, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EsePoint **ud = (EsePoint **)lua_engine_test_userdata(engine, idx, "PointMeta");
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_point_ref(EseLuaEngine *engine, EsePoint *point) {
    log_assert("POINT", point, "ese_point_ref called with NULL point");
    
    if (point->lua_ref == ESE_LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EsePoint **ud = (EsePoint **)lua_engine_create_userdata(engine, "PointMeta", sizeof(EsePoint *));
        *ud = point;

        // Get the reference from the engine's Lua state
        point->lua_ref = lua_engine_get_reference(engine);
        point->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        point->lua_ref_count++;
    }

    profile_count_add("ese_point_ref_count");
}

void ese_point_unref(EseLuaEngine *engine, EsePoint *point) {
    if (!point) return;
    
    if (point->lua_ref != ESE_LUA_NOREF && point->lua_ref_count > 0) {
        point->lua_ref_count--;
        
        if (point->lua_ref_count == 0) {
            // No more references - remove from registry
            // Note: We need to use the engine API to unref
            // For now, just reset the ref
            point->lua_ref = ESE_LUA_NOREF;
        }
    }

    profile_count_add("ese_point_unref_count");
}

// Mathematical operations
float ese_point_distance(const EsePoint *point1, const EsePoint *point2) {
    log_assert("POINT", point1, "ese_point_distance called with NULL first point");
    log_assert("POINT", point2, "ese_point_distance called with NULL second point");
    
    float dx = point2->x - point1->x;
    float dy = point2->y - point1->y;
    return sqrtf(dx * dx + dy * dy);
}

float ese_point_distance_squared(const EsePoint *point1, const EsePoint *point2) {
    log_assert("POINT", point1, "ese_point_distance_squared called with NULL first point");
    log_assert("POINT", point2, "ese_point_distance_squared called with NULL second point");
    
    float dx = point2->x - point1->x;
    float dy = point2->y - point1->y;
    return dx * dx + dy * dy;
}

// Property access
void ese_point_set_x(EsePoint *point, float x) {
    log_assert("POINT", point, "ese_point_set_x called with NULL point");
    point->x = x;
    _ese_point_make_point_notify_watchers(point);
}

float ese_point_get_x(const EsePoint *point) {
    log_assert("POINT", point, "ese_point_get_x called with NULL point");
    return point->x;
}

void ese_point_set_y(EsePoint *point, float y) {
    log_assert("POINT", point, "ese_point_set_y called with NULL point");
    point->y = y;
    _ese_point_make_point_notify_watchers(point);
}

float ese_point_get_y(const EsePoint *point) {
    log_assert("POINT", point, "ese_point_get_y called with NULL point");
    return point->y;
}

// Lua-related access
int ese_point_get_lua_ref(const EsePoint *point) {
    log_assert("POINT", point, "ese_point_get_lua_ref called with NULL point");
    return point->lua_ref;
}

int ese_point_get_lua_ref_count(const EsePoint *point) {
    log_assert("POINT", point, "ese_point_get_lua_ref_count called with NULL point");
    return point->lua_ref_count;
}

// Watcher system
bool ese_point_add_watcher(EsePoint *point, EsePointWatcherCallback callback, void *userdata) {
    log_assert("POINT", point, "ese_point_add_watcher called with NULL point");
    log_assert("POINT", callback, "ese_point_add_watcher called with NULL callback");
    
    // Initialize watcher arrays if this is the first watcher
    if (point->watcher_count == 0) {
        point->watcher_capacity = 4; // Start with capacity for 4 watchers
        point->watchers = memory_manager.malloc(sizeof(EsePointWatcherCallback) * point->watcher_capacity, MMTAG_POINT);
        point->watcher_userdata = memory_manager.malloc(sizeof(void*) * point->watcher_capacity, MMTAG_POINT);
        point->watcher_count = 0;
    }
    
    // Expand arrays if needed
    if (point->watcher_count >= point->watcher_capacity) {
        size_t new_capacity = point->watcher_capacity * 2;
        EsePointWatcherCallback *new_watchers = memory_manager.realloc(
            point->watchers, 
            sizeof(EsePointWatcherCallback) * new_capacity, 
            MMTAG_POINT
        );
        void **new_userdata = memory_manager.realloc(
            point->watcher_userdata, 
            sizeof(void*) * new_capacity, 
            MMTAG_POINT
        );
        
        if (!new_watchers || !new_userdata) return false;
        
        point->watchers = new_watchers;
        point->watcher_userdata = new_userdata;
        point->watcher_capacity = new_capacity;
    }
    
    // Add the new watcher
    point->watchers[point->watcher_count] = callback;
    point->watcher_userdata[point->watcher_count] = userdata;
    point->watcher_count++;
    
    return true;
}

bool ese_point_remove_watcher(EsePoint *point, EsePointWatcherCallback callback, void *userdata) {
    log_assert("POINT", point, "ese_point_remove_watcher called with NULL point");
    log_assert("POINT", callback, "ese_point_remove_watcher called with NULL callback");
    
    for (size_t i = 0; i < point->watcher_count; i++) {
        if (point->watchers[i] == callback && point->watcher_userdata[i] == userdata) {
            // Remove this watcher by shifting remaining ones
            for (size_t j = i; j < point->watcher_count - 1; j++) {
                point->watchers[j] = point->watchers[j + 1];
                point->watcher_userdata[j] = point->watcher_userdata[j + 1];
            }
            point->watcher_count--;
            return true;
        }
    }
    
    return false;
}

size_t ese_point_sizeof(void) {
    return sizeof(EsePoint);
}
