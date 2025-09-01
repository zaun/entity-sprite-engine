#include <string.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "types/types.h"

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseCamera *_camera_state_make(void);

// Lua metamethods
static int _camera_state_lua_gc(lua_State *L);
static int _camera_state_lua_index(lua_State *L);
static int _camera_state_lua_newindex(lua_State *L);
static int _camera_state_lua_tostring(lua_State *L);

// Lua constructors
// static int _camera_state_lua_new(lua_State *L); // REMOVED

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
static EseCamera *_camera_state_make() {
    EseCamera *camera_state = (EseCamera *)memory_manager.malloc(sizeof(EseCamera), MMTAG_GENERAL);
    camera_state->position = NULL;
    camera_state->rotation = 0.0f;
    camera_state->scale = 1.0f;
    camera_state->state = NULL;
    camera_state->lua_ref = LUA_NOREF;
    camera_state->lua_ref_count = 0;
    return camera_state;
}

// Lua metamethods
static int _camera_state_lua_gc(lua_State *L) {
    EseCamera *camera_state = camera_state_lua_get(L, 1);

    if (camera_state) {
        // If lua_ref == LUA_NOREF, there are no more references to this camera, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this camera was referenced from C and should not be freed.
        if (camera_state->lua_ref == LUA_NOREF) {
            camera_state_destroy(camera_state);
        }
    }

    return 0;
}

/**
 * @brief Lua __index metamethod for EseCamera objects (getter).
 * 
 * @details Handles property access for EseCamera objects from Lua. Supports
 *          'position', 'rotation', and 'scale' properties.
 * 
 * @param L Lua state pointer
 * @return Always returns 0 (no return values) or throws Lua error for invalid operations
 */
static int _camera_state_lua_index(lua_State *L) {
    EseCamera *camera_state = camera_state_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!camera_state || !key) return 0;

    if (strcmp(key, "position") == 0) {
        if (camera_state->position == NULL) {
            lua_pushnil(L);
            return 1;
        } else {
            point_lua_push(camera_state->position);
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
        point_set_x(camera_state->position, point_get_x(new_position_point));
        point_set_y(camera_state->position, point_get_y(new_position_point));
        return 0;
    }
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

static int _camera_state_lua_tostring(lua_State *L) {
    EseCamera *camera_state = camera_state_lua_get(L, 1);

    if (!camera_state) {
        lua_pushstring(L, "Camera: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Camera: %p (pos=(%.2f, %.2f), rot=%.2f, scale=%.2f)", 
             (void*)camera_state, point_get_x(camera_state->position), point_get_y(camera_state->position), 
             camera_state->rotation, camera_state->scale);
    lua_pushstring(L, buf);

    return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseCamera *camera_state_create(EseLuaEngine *engine) {
    log_debug("CAMERA", "Creating camera state");
    EseCamera *camera_state = _camera_state_make();
    camera_state->state = engine->runtime;

    camera_state->position = point_create(engine);
    point_ref(camera_state->position);

    return camera_state;
}

EseCamera *camera_state_copy(const EseCamera *source) {
    if (source == NULL) {
        return NULL;
    }

    EseCamera *copy = (EseCamera *)memory_manager.malloc(sizeof(EseCamera), MMTAG_GENERAL);
    copy->position = point_copy(source->position);
    point_ref(copy->position); 
    copy->rotation = source->rotation;
    copy->scale = source->scale;
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
    copy->lua_ref_count = 0;
    return copy;
}

void camera_state_destroy(EseCamera *camera_state) {
    if (!camera_state) return;
    
    if (camera_state->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
        if (camera_state->position) {
            point_destroy(camera_state->position);
        }
        memory_manager.free(camera_state);
    } else {
        if (camera_state->position) {
            point_destroy(camera_state->position);
        }
        camera_state_unref(camera_state);
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

// Lua integration
void camera_state_lua_init(EseLuaEngine *engine) {
    log_assert("CAMERA_STATE", engine, "camera_state_lua_init called with NULL engine");
    log_assert("CAMERA_STATE", engine->runtime, "camera_state_lua_init called with NULL engine->runtime");

    if (luaL_newmetatable(engine->runtime, "CameraProxyMeta")) {
        log_debug("LUA", "Adding CameraProxyMeta to engine");
        lua_pushstring(engine->runtime, "CameraProxyMeta");
        lua_setfield(engine->runtime, -2, "__name");
        lua_pushcfunction(engine->runtime, _camera_state_lua_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _camera_state_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushcfunction(engine->runtime, _camera_state_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");
        lua_pushcfunction(engine->runtime, _camera_state_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
}

void camera_state_lua_push(EseCamera *camera_state) {
    log_assert("CAMERA", camera_state, "camera_state_lua_push called with NULL camera_state");

    if (camera_state->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new proxy table since we don't store them
        lua_newtable(camera_state->state);
        lua_pushlightuserdata(camera_state->state, camera_state);
        lua_setfield(camera_state->state, -2, "__ptr");
        
        luaL_getmetatable(camera_state->state, "CameraProxyMeta");
        lua_setmetatable(camera_state->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(camera_state->state, LUA_REGISTRYINDEX, camera_state->lua_ref);
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
    luaL_getmetatable(L, "CameraProxyMeta");
    
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

void camera_state_ref(EseCamera *camera_state) {
    log_assert("CAMERA", camera_state, "camera_state_ref called with NULL camera_state");
    
    if (camera_state->lua_ref == LUA_NOREF) {
        // First time referencing - create proxy table and store reference
        lua_newtable(camera_state->state);
        lua_pushlightuserdata(camera_state->state, camera_state);
        lua_setfield(camera_state->state, -2, "__ptr");

        luaL_getmetatable(camera_state->state, "CameraProxyMeta");
        lua_setmetatable(camera_state->state, -2);

        // Store hard reference to prevent garbage collection
        camera_state->lua_ref = luaL_ref(camera_state->state, LUA_REGISTRYINDEX);
        camera_state->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        camera_state->lua_ref_count++;
    }
}

void camera_state_unref(EseCamera *camera_state) {
    if (!camera_state) return;
    
    if (camera_state->lua_ref != LUA_NOREF && camera_state->lua_ref_count > 0) {
        camera_state->lua_ref_count--;
        
        if (camera_state->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(camera_state->state, LUA_REGISTRYINDEX, camera_state->lua_ref);
            camera_state->lua_ref = LUA_NOREF;
        }
    }
}
