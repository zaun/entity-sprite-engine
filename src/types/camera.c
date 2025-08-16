#include <string.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "types/types.h"

/**
 * @brief Pushes a EseCamera pointer as a Lua userdata object onto the stack.
 * 
 * @details Creates a new Lua table that acts as a proxy for the EseCamera object,
 *          storing the C pointer as light userdata in the "__ptr" field and
 *          setting the CameraStateProxyMeta metatable for property access.
 * 
 * @param camera_state Pointer to the EseCamera object to wrap for Lua access
 * @param is_lua_owned True if LUA will handle freeing
 * 
 * @warning The EseCamera object must remain valid for the lifetime of the Lua object
 */
void _camera_state_lua_register(EseCamera *camera_state, bool is_lua_owned) {
    log_assert("CAMERA", camera_state, "_camera_state_lua_register called with NULL camera_state");
    log_assert("CAMERA", camera_state->lua_ref == LUA_NOREF, "_camera_state_lua_register camera_state is already registered");

    lua_newtable(camera_state->state);
    lua_pushlightuserdata(camera_state->state, camera_state);
    lua_setfield(camera_state->state, -2, "__ptr");

    // Store the ownership flag
    lua_pushboolean(camera_state->state, is_lua_owned);
    lua_setfield(camera_state->state, -2, "__is_lua_owned");

    luaL_getmetatable(camera_state->state, "CameraStateProxyMeta");
    lua_setmetatable(camera_state->state, -2);

    // Store a reference to this proxy table in the Lua registry
    camera_state->lua_ref = luaL_ref(camera_state->state, LUA_REGISTRYINDEX);
}

void camera_state_lua_push(EseCamera *camera_state) {
    log_assert("VECTOR", camera_state, "camera_state_lua_push called with NULL camera_state");
    log_assert("VECTOR", camera_state->lua_ref != LUA_NOREF, "camera_state_lua_push camera_state not registered with lua");

    // Push the proxy table back onto the stack for Lua to receive
    lua_rawgeti(camera_state->state, LUA_REGISTRYINDEX, camera_state->lua_ref);
}

/**
 * @brief Lua __index metamethod for EseCamera objects (getter).
 * 
 * @details Handles property access for EseCamera objects from Lua. Supports
 *          'position', 'rotation', and 'scale' properties. The position property
 *          returns the EsePoint object directly.
 * 
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack (1 for valid properties, 0 otherwise)
 */
static int _camera_state_lua_index(lua_State *L) {
    EseCamera *camera_state = camera_state_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!camera_state || !key) return 0;

    if (strcmp(key, "position") == 0) {
        // Push the EsePoint object's Lua proxy table onto the stack
        if (camera_state->position->lua_ref != LUA_NOREF) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, camera_state->position->lua_ref);
        } else {
            lua_pushnil(L);
        }
        return 1;
    } else if (strcmp(key, "rotation") == 0) {
        lua_pushnumber(L, camera_state->rotation);
        return 1;
    } else if (strcmp(key, "scale") == 0) {
        lua_pushnumber(L, camera_state->scale);
        return 1;
    }
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseCamera objects (setter).
 * 
 * @details Handles property assignment for EseCamera objects from Lua. Supports
 *          'rotation' and 'scale' properties. Position must be set through
 *          the EsePoint object (camera.position.x = value).
 * 
 * @param L Lua state pointer
 * @return Always returns 0 (no return values) or throws Lua error for invalid operations
 */
static int _camera_state_lua_newindex(lua_State *L) {
    EseCamera *camera_state = camera_state_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!camera_state || !key) return 0;

    if (strcmp(key, "rotation") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "rotation must be a number");
        }
        camera_state->rotation = (float)lua_tonumber(L, 3);
        return 0;
    } else if (strcmp(key, "scale") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "scale must be a number");
        }
        camera_state->scale = (float)lua_tonumber(L, 3);
        return 0;
    } else if (strcmp(key, "position") == 0) {
        EsePoint *new_position_point = point_lua_get(L, 3);
        if (!new_position_point) {
            return luaL_error(L, "position must be a EsePoint object");
        }
        // Copy values, don't copy reference (ownership safety)
        camera_state->position->x = new_position_point->x;
        camera_state->position->y = new_position_point->y;
    }
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __gc metamethod for EseCamera objects.
 *
 * @details Checks the '__is_lua_owned' flag in the proxy table. If true,
 * it means this EseCamera's memory was allocated by Lua and should be freed.
 * If false, the EseCamera's memory is managed externally (by C) and is not freed here.
 *
 * @param L Lua state pointer
 * @return Always 0 (no return values)
 */
static int _camera_state_lua_gc(lua_State *L) {
    // The proxy table is at index 1
    // Get the EseCamera pointer
    EseCamera *camera_state = camera_state_lua_get(L, 1);

    if (camera_state) {
        // Get the __is_lua_owned flag from the proxy table itself
        lua_getfield(L, 1, "__is_lua_owned"); // Proxy table at index 1, get field "__is_lua_owned"
        bool is_lua_owned = lua_toboolean(L, -1);
        lua_pop(L, 1); // Pop the boolean value

        if (is_lua_owned) {
            camera_state_destroy(camera_state); // Free the C memory allocated for this Lua-owned camera state
            log_debug("LUA_GC", "Camera object (Lua-owned) garbage collected and C memory freed.");
        } else {
            log_debug("LUA_GC", "Camera object (C-owned) garbage collected, C memory *not* freed.");
        }
    }

    return 0;
}

static int _camera_state_lua_tostring(lua_State *L) {
    EseCamera *camera_state = camera_state_lua_get(L, 1);

    if (!camera_state) {
        lua_pushstring(L, "Camera: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Camera: %p (pos=(%.2f, %.2f), rot=%.2f, scale=%.2f)", 
             (void*)camera_state, camera_state->position->x, camera_state->position->y, 
             camera_state->rotation, camera_state->scale);
    lua_pushstring(L, buf);

    return 1;
}

void camera_state_lua_init(EseLuaEngine *engine) {
    if (luaL_newmetatable(engine->runtime, "CameraStateProxyMeta")) {
        log_debug("LUA", "Adding entity CameraStateProxyMeta to engine");
        lua_pushcfunction(engine->runtime, _camera_state_lua_index);
        lua_setfield(engine->runtime, -2, "__index");               // For property getters
        lua_pushcfunction(engine->runtime, _camera_state_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");            // For property setters
        lua_pushcfunction(engine->runtime, _camera_state_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");                  // For garbage collection
        lua_pushcfunction(engine->runtime, _camera_state_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");            // For printing/debugging
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
}

EseCamera *camera_state_create(EseLuaEngine *engine) {
    EseCamera *camera_state = (EseCamera *)memory_manager.malloc(sizeof(EseCamera), MMTAG_GENERAL);
    
    // Create a C-owned EsePoint for the position
    camera_state->position = point_create(engine);
    
    camera_state->rotation = 0.0f;
    camera_state->scale = 1.0f;
    camera_state->state = engine->runtime;
    camera_state->lua_ref = LUA_NOREF;
    _camera_state_lua_register(camera_state, false);
    return camera_state;
}

void camera_state_destroy(EseCamera *camera_state) {
    if (camera_state) {
        if (camera_state->lua_ref != LUA_NOREF) {
            luaL_unref(camera_state->state, LUA_REGISTRYINDEX, camera_state->lua_ref);
        }
        
        // Destroy the position EsePoint
        if (camera_state->position) {
            point_destroy(camera_state->position);
        }
        
        memory_manager.free(camera_state);
    }
}

EseCamera *camera_state_lua_get(lua_State *L, int idx) {
    // Check if the value at idx is a table
    if (!lua_istable(L, idx)) {
        return NULL;
    }
    
    // Check if it has the correct metatable
    if (!lua_getmetatable(L, idx)) {
        return NULL; // No metatable
    }
    
    // Get the expected metatable for comparison
    luaL_getmetatable(L, "CameraStateProxyMeta");
    
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
    
    return (EseCamera *)pos;
}
