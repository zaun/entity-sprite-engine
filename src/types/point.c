#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stddef.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/point.h"

// The actual EsePoint struct definition (private to this file)
typedef struct EsePoint {
    float x;            /**< The x-coordinate of the point */
    float y;            /**< The y-coordinate of the point */

    lua_State *state;   /**< Lua State this EsePoint belongs to */
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
static int _ese_point_lua_gc(lua_State *L);
static int _ese_point_lua_index(lua_State *L);
static int _ese_point_lua_newindex(lua_State *L);
static int _ese_point_lua_tostring(lua_State *L);

// Lua constructors
static int _ese_point_lua_new(lua_State *L);
static int _ese_point_lua_zero(lua_State *L);
static int _ese_point_lua_distance(lua_State *L);

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
    point->state = NULL;
    point->lua_ref = LUA_NOREF;
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
        if (point->lua_ref == LUA_NOREF) {
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
        lua_pushnumber(L, point->x);
        profile_stop(PROFILE_LUA_POINT_INDEX, "point_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(L, point->y);
        profile_stop(PROFILE_LUA_POINT_INDEX, "point_lua_index (getter)");
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
        point->x = (float)lua_tonumber(L, 3);
        _ese_point_make_point_notify_watchers(point);
        profile_stop(PROFILE_LUA_POINT_NEWINDEX, "point_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "y") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_POINT_NEWINDEX);
            return luaL_error(L, "point.y must be a number");
        }
        point->y = (float)lua_tonumber(L, 3);
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
    snprintf(buf, sizeof(buf), "Point: %p (x=%.2f, y=%.2f)", (void*)point, point->x, point->y);
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
    point->x = x;
    point->y = y;
    point->state = L;

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
    EsePoint *point = _ese_point_make();  // We'll set the state manually
    point->state = L;

    // Create userdata directly
    EsePoint **ud = (EsePoint **)lua_newuserdata(L, sizeof(EsePoint *));
    *ud = point;

    // Attach metatable
    luaL_getmetatable(L, POINT_PROXY_META);
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_POINT_ZERO, "point_lua_zero");
    return 1;
}

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

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EsePoint *ese_point_create(EseLuaEngine *engine) {
    log_assert("POINT", engine, "ese_point_create called with NULL engine");
    EsePoint *point = _ese_point_make();
    point->state = engine->runtime;
    return point;
}

EsePoint *ese_point_copy(const EsePoint *source) {
    log_assert("POINT", source, "ese_point_copy called with NULL source");
    
    EsePoint *copy = (EsePoint *)memory_manager.malloc(sizeof(EsePoint), MMTAG_POINT);
    copy->x = source->x;
    copy->y = source->y;
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
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
    
    if (point->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(point);
    } else {
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
        ese_point_unref(point);
    }
}

// Lua integration
void ese_point_lua_init(EseLuaEngine *engine) {
    log_assert("POINT", engine, "ese_point_lua_init called with NULL engine");
    if (luaL_newmetatable(engine->runtime, POINT_PROXY_META)) {
        log_debug("LUA", "Adding entity PointMeta to engine");
        lua_pushstring(engine->runtime, POINT_PROXY_META);
        lua_setfield(engine->runtime, -2, "__name");
        lua_pushcfunction(engine->runtime, _ese_point_lua_index);
        lua_setfield(engine->runtime, -2, "__index");               // For property getters
        lua_pushcfunction(engine->runtime, _ese_point_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");            // For property setters
        lua_pushcfunction(engine->runtime, _ese_point_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");                  // For garbage collection
        lua_pushcfunction(engine->runtime, _ese_point_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");            // For printing/debugging
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
    
    // Create global EsePoint table with constructor
    lua_getglobal(engine->runtime, "Point");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1); // Pop the nil value
        log_debug("LUA", "Creating global point table");
        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _ese_point_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_pushcfunction(engine->runtime, _ese_point_lua_zero);
        lua_setfield(engine->runtime, -2, "zero");
        lua_pushcfunction(engine->runtime, _ese_point_lua_distance);
        lua_setfield(engine->runtime, -2, "distance");
        lua_setglobal(engine->runtime, "Point");
    } else {
        lua_pop(engine->runtime, 1); // Pop the existing point table
    }
}

void ese_point_lua_push(EsePoint *point) {
    log_assert("POINT", point, "ese_point_lua_push called with NULL point");

    if (point->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EsePoint **ud = (EsePoint **)lua_newuserdata(point->state, sizeof(EsePoint *));
        *ud = point;

        // Attach metatable
        luaL_getmetatable(point->state, POINT_PROXY_META);
        lua_setmetatable(point->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(point->state, LUA_REGISTRYINDEX, point->lua_ref);
    }
}

EsePoint *ese_point_lua_get(lua_State *L, int idx) {
    log_assert("POINT", L, "ese_point_lua_get called with NULL Lua state");
    
    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EsePoint **ud = (EsePoint **)luaL_testudata(L, idx, POINT_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_point_ref(EsePoint *point) {
    log_assert("POINT", point, "ese_point_ref called with NULL point");
    
    if (point->lua_ref == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EsePoint **ud = (EsePoint **)lua_newuserdata(point->state, sizeof(EsePoint *));
        *ud = point;

        // Attach metatable
        luaL_getmetatable(point->state, POINT_PROXY_META);
        lua_setmetatable(point->state, -2);

        // Store hard reference to prevent garbage collection
        point->lua_ref = luaL_ref(point->state, LUA_REGISTRYINDEX);
        point->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        point->lua_ref_count++;
    }

    profile_count_add("ese_point_ref_count");
}

void ese_point_unref(EsePoint *point) {
    if (!point) return;
    
    if (point->lua_ref != LUA_NOREF && point->lua_ref_count > 0) {
        point->lua_ref_count--;
        
        if (point->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(point->state, LUA_REGISTRYINDEX, point->lua_ref);
            point->lua_ref = LUA_NOREF;
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
lua_State *ese_point_get_state(const EsePoint *point) {
    log_assert("POINT", point, "ese_point_get_state called with NULL point");
    return point->state;
}

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
