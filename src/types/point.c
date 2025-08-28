#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stddef.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
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
static EsePoint *_point_make(void);

// Watcher system
static void _point_notify_watchers(EsePoint *point);

// Lua metamethods
static int _point_lua_gc(lua_State *L);
static int _point_lua_index(lua_State *L);
static int _point_lua_newindex(lua_State *L);
static int _point_lua_tostring(lua_State *L);

// Lua constructors
static int _point_lua_new(lua_State *L);
static int _point_lua_zero(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
static EsePoint *_point_make() {
    EsePoint *point = (EsePoint *)memory_manager.malloc(sizeof(EsePoint), MMTAG_GENERAL);
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
static void _point_notify_watchers(EsePoint *point) {
    if (!point || point->watcher_count == 0) return;
    
    for (size_t i = 0; i < point->watcher_count; i++) {
        if (point->watchers[i]) {
            point->watchers[i](point, point->watcher_userdata[i]);
        }
    }
}

// Lua metamethods
static int _point_lua_gc(lua_State *L) {
    EsePoint *point = point_lua_get(L, 1);

    if (point) {
        // If lua_ref == LUA_NOREF, there are no more references to this point, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this point was referenced from C and should not be freed.
        if (point->lua_ref == LUA_NOREF) {
            point_destroy(point);
        }
    }

    return 0;
}

static int _point_lua_index(lua_State *L) {
    EsePoint *point = point_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!point || !key) return 0;

    if (strcmp(key, "x") == 0) {
        lua_pushnumber(L, point->x);
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(L, point->y);
        return 1;
    }
    return 0;
}

static int _point_lua_newindex(lua_State *L) {
    EsePoint *point = point_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!point || !key) return 0;

    if (strcmp(key, "x") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "point.x must be a number");
        }
        point->x = (float)lua_tonumber(L, 3);
        _point_notify_watchers(point);
        return 0;
    } else if (strcmp(key, "y") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "point.y must be a number");
        }
        point->y = (float)lua_tonumber(L, 3);
        _point_notify_watchers(point);
        return 0;
    }
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

static int _point_lua_tostring(lua_State *L) {
    EsePoint *point = point_lua_get(L, 1);

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
static int _point_lua_new(lua_State *L) {
    float x = 0.0f;
    float y = 0.0f;

    // Check for optional x and y arguments
    int n_args = lua_gettop(L);
    if (n_args != 2) {
        return luaL_error(L, "new(x, y) takes 2 arguments");
    }

    if (lua_isnumber(L, 1)) {
        x = (float)lua_tonumber(L, 1);
    } else {
        return luaL_error(L, "x must be a number");
    }
    if (lua_isnumber(L, 2)) {
        y = (float)lua_tonumber(L, 2);
    } else {
        return luaL_error(L, "y must be a number");
    }

    // Create the point using the standard creation function
    EsePoint *point = _point_make();
    point->x = x;
    point->y = y;
    point->state = L;
    
    // Create proxy table for Lua-owned point
    lua_newtable(L);
    lua_pushlightuserdata(L, point);
    lua_setfield(L, -2, "__ptr");

    luaL_getmetatable(L, "PointProxyMeta");
    lua_setmetatable(L, -2);

    return 1;
}

static int _point_lua_zero(lua_State *L) {
    // Create the point using the standard creation function
    EsePoint *point = _point_make();  // We'll set the state manually
    point->state = L;
    
    // Create proxy table for Lua-owned point
    lua_newtable(L);
    lua_pushlightuserdata(L, point);
    lua_setfield(L, -2, "__ptr");

    luaL_getmetatable(L, "PointProxyMeta");
    lua_setmetatable(L, -2);

    return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EsePoint *point_create(EseLuaEngine *engine) {
    EsePoint *point = _point_make();
    point->state = engine->runtime;
    return point;
}

EsePoint *point_copy(const EsePoint *source) {
    if (source == NULL) {
        return NULL;
    }

    EsePoint *copy = (EsePoint *)memory_manager.malloc(sizeof(EsePoint), MMTAG_GENERAL);
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

void point_destroy(EsePoint *point) {
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
        // Has Lua references, decrement counter
        if (point->lua_ref_count > 0) {
            point->lua_ref_count--;
            
            if (point->lua_ref_count == 0) {
                // No more C references, unref from Lua registry
                // Let Lua's GC handle the final cleanup
                luaL_unref(point->state, LUA_REGISTRYINDEX, point->lua_ref);
                point->lua_ref = LUA_NOREF;
            }
        }
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

// Lua integration
void point_lua_init(EseLuaEngine *engine) {
    if (luaL_newmetatable(engine->runtime, "PointProxyMeta")) {
        log_debug("LUA", "Adding entity PointProxyMeta to engine");
        lua_pushstring(engine->runtime, "PointProxyMeta");
        lua_setfield(engine->runtime, -2, "__name");
        lua_pushcfunction(engine->runtime, _point_lua_index);
        lua_setfield(engine->runtime, -2, "__index");               // For property getters
        lua_pushcfunction(engine->runtime, _point_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");            // For property setters
        lua_pushcfunction(engine->runtime, _point_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");                  // For garbage collection
        lua_pushcfunction(engine->runtime, _point_lua_tostring);
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
        lua_pushcfunction(engine->runtime, _point_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_pushcfunction(engine->runtime, _point_lua_zero);
        lua_setfield(engine->runtime, -2, "zero");
        lua_setglobal(engine->runtime, "Point");
    } else {
        lua_pop(engine->runtime, 1); // Pop the existing point table
    }
}

void point_lua_push(EsePoint *point) {
    log_assert("POINT", point, "point_lua_push called with NULL point");

    if (point->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new proxy table since we don't store them
        lua_newtable(point->state);
        lua_pushlightuserdata(point->state, point);
        lua_setfield(point->state, -2, "__ptr");
        
        luaL_getmetatable(point->state, "PointProxyMeta");
        lua_setmetatable(point->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(point->state, LUA_REGISTRYINDEX, point->lua_ref);
    }
}

EsePoint *point_lua_get(lua_State *L, int idx) {
    // Check if the value at idx is a table
    if (!lua_istable(L, idx)) {
        return NULL;
    }
    
    // Check if it has the correct metatable
    if (!lua_getmetatable(L, idx)) {
        return NULL; // No metatable
    }
    
    // Get the expected metatable for comparison
    luaL_getmetatable(L, "PointProxyMeta");
    
    // Compare metatables
    if (!lua_rawequal(L, -1, -2)) {
        lua_pop(L, 2); // Pop both metatables
        return NULL; // Wrong metatable
    }
    
    lua_pop(L, 2); // Pop both metatables
    
    // Get the __ptr field
    lua_getfield(L, idx, "__ptr");
    
    // Check if __ptr exists and is light userdata
    if (!lua_islightuserdata(L, -1)) {
        lua_pop(L, 1); // Pop the __ptr value (or nil)
        return NULL;
    }
    
    // Extract the pointer
    void *pos = lua_touserdata(L, -1);
    lua_pop(L, 1); // Pop the __ptr value
    
    return (EsePoint *)pos;
}

void point_ref(EsePoint *point) {
    log_assert("POINT", point, "point_ref called with NULL point");
    
    if (point->lua_ref == LUA_NOREF) {
        // First time referencing - create proxy table and store reference
        lua_newtable(point->state);
        lua_pushlightuserdata(point->state, point);
        lua_setfield(point->state, -2, "__ptr");

        luaL_getmetatable(point->state, "PointProxyMeta");
        lua_setmetatable(point->state, -2);

        // Store hard reference to prevent garbage collection
        point->lua_ref = luaL_ref(point->state, LUA_REGISTRYINDEX);
        point->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        point->lua_ref_count++;
    }
}

void point_unref(EsePoint *point) {
    if (!point) return;
    
    if (point->lua_ref != LUA_NOREF && point->lua_ref_count > 0) {
        point->lua_ref_count--;
        
        if (point->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(point->state, LUA_REGISTRYINDEX, point->lua_ref);
            point->lua_ref = LUA_NOREF;
        }
    }
}

// Mathematical operations
float point_distance(const EsePoint *point1, const EsePoint *point2) {
    if (!point1 || !point2) return 0.0f;
    
    float dx = point2->x - point1->x;
    float dy = point2->y - point1->y;
    return sqrtf(dx * dx + dy * dy);
}

float point_distance_squared(const EsePoint *point1, const EsePoint *point2) {
    if (!point1 || !point2) return 0.0f;
    
    float dx = point2->x - point1->x;
    float dy = point2->y - point1->y;
    return dx * dx + dy * dy;
}

// Property access
void point_set_x(EsePoint *point, float x) {
    if (!point) return;
    point->x = x;
    _point_notify_watchers(point);
}

float point_get_x(const EsePoint *point) {
    if (!point) return 0.0f;
    return point->x;
}

void point_set_y(EsePoint *point, float y) {
    if (!point) return;
    point->y = y;
    _point_notify_watchers(point);
}

float point_get_y(const EsePoint *point) {
    if (!point) return 0.0f;
    return point->y;
}

// Lua-related access
lua_State *point_get_state(const EsePoint *point) {
    if (!point) return NULL;
    return point->state;
}

int point_get_lua_ref(const EsePoint *point) {
    if (!point) return LUA_NOREF;
    return point->lua_ref;
}

int point_get_lua_ref_count(const EsePoint *point) {
    if (!point) return 0;
    return point->lua_ref_count;
}

// Watcher system
bool point_add_watcher(EsePoint *point, EsePointWatcherCallback callback, void *userdata) {
    if (!point || !callback) return false;
    
    // Initialize watcher arrays if this is the first watcher
    if (point->watcher_count == 0) {
        point->watcher_capacity = 4; // Start with capacity for 4 watchers
        point->watchers = memory_manager.malloc(sizeof(EsePointWatcherCallback) * point->watcher_capacity, MMTAG_GENERAL);
        point->watcher_userdata = memory_manager.malloc(sizeof(void*) * point->watcher_capacity, MMTAG_GENERAL);
        point->watcher_count = 0;
    }
    
    // Expand arrays if needed
    if (point->watcher_count >= point->watcher_capacity) {
        size_t new_capacity = point->watcher_capacity * 2;
        EsePointWatcherCallback *new_watchers = memory_manager.realloc(
            point->watchers, 
            sizeof(EsePointWatcherCallback) * new_capacity, 
            MMTAG_GENERAL
        );
        void **new_userdata = memory_manager.realloc(
            point->watcher_userdata, 
            sizeof(void*) * new_capacity, 
            MMTAG_GENERAL
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

bool point_remove_watcher(EsePoint *point, EsePointWatcherCallback callback, void *userdata) {
    if (!point || !callback) return false;
    
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

size_t point_sizeof(void) {
    return sizeof(EsePoint);
}
