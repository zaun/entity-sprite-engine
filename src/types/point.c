#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "vendor/lua/src/lauxlib.h"
#include "types/point.h"

/**
 * @brief Pushes a EsePoint pointer as a Lua userdata object onto the stack.
 * 
 * @details Creates a new Lua table that acts as a proxy for the EsePoint object,
 *          storing the C pointer as light userdata in the "__ptr" field and
 *          setting the PointProxyMeta metatable for property access.
 * 
 * @param point Pointer to the EsePoint object to wrap for Lua access
 * @param is_lua_owned True if LUA will handle freeing
 * 
 * @warning The EsePoint object must remain valid for the lifetime of the Lua object
 */
void _point_lua_register(EsePoint *point, bool is_lua_owned) {
    log_assert("POINT", point, "_point_lua_register called with NULL point");
    log_assert("POINT", point->lua_ref == LUA_NOREF, "_point_lua_register point is already registered");

    lua_newtable(point->state);
    lua_pushlightuserdata(point->state, point);
    lua_setfield(point->state, -2, "__ptr");

    // Store the ownership flag
    lua_pushboolean(point->state, is_lua_owned);
    lua_setfield(point->state, -2, "__is_lua_owned");

    luaL_getmetatable(point->state, "PointProxyMeta");
    lua_setmetatable(point->state, -2);

    // Store a reference to this proxy table in the Lua registry
    point->lua_ref = luaL_ref(point->state, LUA_REGISTRYINDEX);
}

void point_lua_push(EsePoint *point) {
    log_assert("POINT", point, "point_lua_push called with NULL point");
    log_assert("POINT", point->lua_ref != LUA_NOREF, "point_lua_push point not registered with lua");

    // Push the proxy table back onto the stack for Lua to receive
    lua_rawgeti(point->state, LUA_REGISTRYINDEX, point->lua_ref);
}

/**
 * @brief Lua function to create a new EsePoint object.
 * 
 * @details Callable from Lua as EsePoint.new(). Creates a new EsePoint.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new point object)
 * 
 * @warning Items created in Lua are owned by Lua
 */
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

    EsePoint *point = (EsePoint *)memory_manager.malloc(sizeof(EsePoint), MMTAG_GENERAL);
    point->x = x;
    point->y = y;
    point->state = L;
    point->lua_ref = LUA_NOREF;
    _point_lua_register(point, true);

    point_lua_push(point);
    return 1;
}

/**
 * @brief Lua function to create a new EsePoint object.
 * 
 * @details Callable from Lua as point.zero(). Creates a new EsePoint.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new point object)
 */
static int _point_lua_zero(lua_State *L) {
    EsePoint *point = (EsePoint *)memory_manager.malloc(sizeof(EsePoint), MMTAG_GENERAL);
    point->x = 0.0f;
    point->y = 0.0f;
    point->state = L;
    point->lua_ref = LUA_NOREF;
    _point_lua_register(point, true);

    point_lua_push(point);
    return 1;
}

/**
 * @brief Lua __index metamethod for EsePoint objects (getter).
 * 
 * @details Handles property access for EsePoint objects from Lua. Currently supports
 *          'x' and 'y' properties, returning their float values as Lua numbers.
 * 
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack (1 for valid properties, 0 otherwise)
 */
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

/**
 * @brief Lua __newindex metamethod for EsePoint objects (setter).
 * 
 * @details Handles property assignment for EsePoint objects from Lua. Currently supports
 *          'x' and 'y' properties, validating that assigned values are numbers.
 * 
 * @param L Lua state pointer
 * @return Always returns 0 (no return values) or throws Lua error for invalid operations
 */
static int _point_lua_newindex(lua_State *L) {
    EsePoint *point = point_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!point || !key) return 0;

    if (strcmp(key, "x") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "point.x must be a number");
        }
        point->x = (float)lua_tonumber(L, 3);
        return 0;
    } else if (strcmp(key, "y") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "point.y must be a number");
        }
        point->y = (float)lua_tonumber(L, 3);
        return 0;
    }
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __gc metamethod for EsePoint objects.
 *
 * @details Checks the '__is_lua_owned' flag in the proxy table. If true,
 * it means this EsePoint's memory was allocated by Lua and should be freed.
 * If false, the EsePoint's memory is managed externally (by C) and is not freed here.
 *
 * @param L Lua state pointer
 * @return Always 0 (no return values)
 */
static int _point_lua_gc(lua_State *L) {
    // The proxy table is at index 1
    // Get the EsePoint pointer
    EsePoint *point = point_lua_get(L, 1);

    if (point) {
        // Get the __is_lua_owned flag from the proxy table itself
        lua_getfield(L, 1, "__is_lua_owned"); // Proxy table at index 1, get field "__is_lua_owned"
        bool is_lua_owned = lua_toboolean(L, -1);
        lua_pop(L, 1); // Pop the boolean value

        if (is_lua_owned) {
            point_destroy(point); // Free the C memory allocated for this Lua-owned point
            log_debug("LUA_GC", "Point object (Lua-owned) garbage collected and C memory freed.");
        } else {
            log_debug("LUA_GC", "Point object (C-owned) garbage collected, C memory *not* freed.");
        }
    }

    return 0;
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

void point_lua_init(EseLuaEngine *engine) {
    if (luaL_newmetatable(engine->runtime, "PointProxyMeta")) {
        log_debug("LUA", "Adding entity PointProxyMeta to engine");
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

EsePoint *point_create(EseLuaEngine *engine) {
    EsePoint *point = (EsePoint *)memory_manager.malloc(sizeof(EsePoint), MMTAG_GENERAL);
    point->x = 0.0f;
    point->y = 0.0f;
    point->state = engine->runtime;
    point->lua_ref = LUA_NOREF;
    _point_lua_register(point, false);
    return point;
}

void point_destroy(EsePoint *point) {
    if (point) {
        if (point->lua_ref != LUA_NOREF) {
            luaL_unref(point->state, LUA_REGISTRYINDEX, point->lua_ref);
        }
        memory_manager.free(point);
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
