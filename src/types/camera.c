#include <string.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/types.h"

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseCamera *_ese_camera_make(EseLuaEngine *engine);

// Lua metamethods
static int _ese_camera_lua_gc(lua_State *L);
static int _ese_camera_lua_index(lua_State *L);
static int _ese_camera_lua_newindex(lua_State *L);
static int _ese_camera_lua_tostring(lua_State *L);


// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseCamera instance with default values
 * 
 * Allocates memory for a new EseCamera and initializes all fields to safe defaults.
 * The camera starts with no position, zero rotation, unit scale, and no Lua state or references.
 * 
 * @return Pointer to the newly created EseCamera, or NULL on allocation failure
 */
static EseCamera *_ese_camera_make(EseLuaEngine *engine) {
    EseCamera *camera_state = (EseCamera *)memory_manager.malloc(sizeof(EseCamera), MMTAG_CAMERA);
    camera_state->position = ese_point_create(engine);
    ese_point_ref(camera_state->position);
    camera_state->rotation = 0.0f;
    camera_state->scale = 1.0f;
    camera_state->state = NULL;
    camera_state->lua_ref = LUA_NOREF;
    camera_state->lua_ref_count = 0;
    return camera_state;
}

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseCamera
 * 
 * Handles cleanup when a Lua proxy table for an EseCamera is garbage collected.
 * Only frees the underlying EseCamera if it has no C-side references.
 * Note: Does NOT destroy the position point - that has its own lifecycle.
 * 
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static int _ese_camera_lua_gc(lua_State *L) {
    // Get from userdata
    EseCamera **ud = (EseCamera **)luaL_testudata(L, 1, "CameraMeta");
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseCamera *camera_state = *ud;
    if (camera_state) {
        // If lua_ref == LUA_NOREF, there are no more references to this camera, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this camera was referenced from C and should not be freed.
        if (camera_state->lua_ref == LUA_NOREF) {
            ese_camera_destroy(camera_state);            
        }
    }

    return 0;
}

/**
 * @brief Lua __index metamethod for EseCamera property access
 * 
 * Provides read access to camera properties from Lua. When a Lua script
 * accesses camera.position, camera.rotation, or camera.scale, this function is called
 * to retrieve the values. Position returns an EsePoint object, rotation and scale return numbers.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid properties, 0 for invalid)
 */
static int _ese_camera_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_CAMERA_INDEX);
    EseCamera *camera_state = ese_camera_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!camera_state || !key) {
        profile_cancel(PROFILE_LUA_CAMERA_INDEX);
        return 0;
    }

    if (strcmp(key, "position") == 0) {
        ese_point_lua_push(camera_state->position);
        profile_stop(PROFILE_LUA_CAMERA_INDEX, "ese_camera_lua_index (position)");
        return 1;
    } else if (strcmp(key, "rotation") == 0) {
        lua_pushnumber(L, camera_state->rotation);
        profile_stop(PROFILE_LUA_CAMERA_INDEX, "ese_camera_lua_index (rotation)");
        return 1;
    } else if (strcmp(key, "scale") == 0) {
        lua_pushnumber(L, camera_state->scale);
        profile_stop(PROFILE_LUA_CAMERA_INDEX, "ese_camera_lua_index (scale)");
        return 1;
    }
    profile_stop(PROFILE_LUA_CAMERA_INDEX, "ese_camera_lua_index (invalid)");
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseCamera property assignment
 * 
 * Provides write access to camera properties from Lua. When a Lua script
 * assigns to camera.rotation, camera.scale, or camera.position, this function is called
 * to update the values. Position must be set through an EsePoint object.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_camera_lua_newindex(lua_State *L) {
    profile_start(PROFILE_LUA_CAMERA_NEWINDEX);
    EseCamera *camera_state = ese_camera_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!camera_state || !key) {
        profile_cancel(PROFILE_LUA_CAMERA_NEWINDEX);
        return 0;
    }

    if (strcmp(key, "rotation") == 0) {
        if (!lua_isnumber(L, 3)) {
            profile_cancel(PROFILE_LUA_CAMERA_NEWINDEX);
            return luaL_error(L, "rotation must be a number");
        }
        camera_state->rotation = (float)lua_tonumber(L, 3);
        profile_stop(PROFILE_LUA_CAMERA_NEWINDEX, "ese_camera_lua_newindex (rotation)");
        return 0;
    } else if (strcmp(key, "scale") == 0) {
        if (!lua_isnumber(L, 3)) {
            profile_cancel(PROFILE_LUA_CAMERA_NEWINDEX);
            return luaL_error(L, "scale must be a number");
        }
        camera_state->scale = (float)lua_tonumber(L, 3);
        profile_stop(PROFILE_LUA_CAMERA_NEWINDEX, "ese_camera_lua_newindex (scale)");
        return 0;
    } else if (strcmp(key, "position") == 0) {
        EsePoint *new_position_point = ese_point_lua_get(L, 3);
        if (!new_position_point) {
            profile_cancel(PROFILE_LUA_CAMERA_NEWINDEX);
            return luaL_error(L, "position must be a EsePoint object");
        }
        // Copy values, don't copy reference (ownership safety)
        ese_point_set_x(camera_state->position, ese_point_get_x(new_position_point));
        ese_point_set_y(camera_state->position, ese_point_get_y(new_position_point));
        profile_stop(PROFILE_LUA_CAMERA_NEWINDEX, "ese_camera_lua_newindex (position)");
        return 0;
    }
    profile_stop(PROFILE_LUA_CAMERA_NEWINDEX, "ese_camera_lua_newindex (invalid)");
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __tostring metamethod for EseCamera string representation
 * 
 * Converts an EseCamera to a human-readable string for debugging and display.
 * The format includes the memory address, position coordinates, rotation, and scale values.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
static int _ese_camera_lua_tostring(lua_State *L) {
    EseCamera *camera_state = ese_camera_lua_get(L, 1);

    if (!camera_state) {
        lua_pushstring(L, "Camera: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Camera: %p (pos=(%.2f, %.2f), rot=%.2f, scale=%.2f)", 
             (void*)camera_state, ese_point_get_x(camera_state->position), ese_point_get_y(camera_state->position), 
             camera_state->rotation, camera_state->scale);
    lua_pushstring(L, buf);

    return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseCamera *ese_camera_create(EseLuaEngine *engine) {
    log_debug("CAMERA", "Creating camera state");
    EseCamera *camera_state = _ese_camera_make(engine);
    camera_state->state = engine->runtime;

    return camera_state;
}

EseCamera *ese_camera_copy(const EseCamera *source) {
    if (source == NULL) {
        return NULL;
    }

    EseCamera *copy = (EseCamera *)memory_manager.malloc(sizeof(EseCamera), MMTAG_CAMERA);
    copy->position = ese_point_copy(source->position);
    ese_point_ref(copy->position);
    copy->rotation = source->rotation;
    copy->scale = source->scale;
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
    copy->lua_ref_count = 0;
    return copy;
}

void ese_camera_destroy(EseCamera *camera_state) {
    if (!camera_state) return;
    
    if (camera_state->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
        ese_point_unref(camera_state->position);
        ese_point_destroy(camera_state->position);
        memory_manager.free(camera_state);
    } else {
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
        ese_camera_unref(camera_state);
    }
}

// Lua integration
void ese_camera_lua_init(EseLuaEngine *engine) {
    log_assert("CAMERA_STATE", engine, "ese_camera_lua_init called with NULL engine");
    log_assert("CAMERA_STATE", engine->runtime, "ese_camera_lua_init called with NULL engine->runtime");

    // Create metatable
    lua_engine_new_object_meta(engine, "CameraMeta", 
        _ese_camera_lua_index, 
        _ese_camera_lua_newindex, 
        _ese_camera_lua_gc, 
        _ese_camera_lua_tostring);
}

void ese_camera_lua_push(EseCamera *camera_state) {
    log_assert("CAMERA", camera_state, "ese_camera_lua_push called with NULL camera_state");

    if (camera_state->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseCamera **ud = (EseCamera **)lua_newuserdata(camera_state->state, sizeof(EseCamera *));
        *ud = camera_state;

        // Attach metatable
        luaL_getmetatable(camera_state->state, "CameraMeta");
        lua_setmetatable(camera_state->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(camera_state->state, LUA_REGISTRYINDEX, camera_state->lua_ref);
    }
}

EseCamera *ese_camera_lua_get(lua_State *L, int idx) {
    log_assert("CAMERA", L, "ese_camera_lua_get called with NULL Lua state");
    
    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseCamera **ud = (EseCamera **)luaL_testudata(L, idx, "CameraMeta");
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_camera_ref(EseCamera *camera_state) {
    log_assert("CAMERA", camera_state, "ese_camera_ref called with NULL camera_state");
    
    if (camera_state->lua_ref == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseCamera **ud = (EseCamera **)lua_newuserdata(camera_state->state, sizeof(EseCamera *));
        *ud = camera_state;

        // Attach metatable
        luaL_getmetatable(camera_state->state, "CameraMeta");
        lua_setmetatable(camera_state->state, -2);

        // Store hard reference to prevent garbage collection
        camera_state->lua_ref = luaL_ref(camera_state->state, LUA_REGISTRYINDEX);
        camera_state->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        camera_state->lua_ref_count++;
    }

    profile_count_add("ese_camera_ref_count");
}

void ese_camera_unref(EseCamera *camera_state) {
    if (!camera_state) return;
    
    if (camera_state->lua_ref != LUA_NOREF && camera_state->lua_ref_count > 0) {
        camera_state->lua_ref_count--;
        
        if (camera_state->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(camera_state->state, LUA_REGISTRYINDEX, camera_state->lua_ref);
            camera_state->lua_ref = LUA_NOREF;
        }
    }

    profile_count_add("ese_camera_unref_count");
}
