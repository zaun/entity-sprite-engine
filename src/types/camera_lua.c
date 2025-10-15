#include <string.h>
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/camera.h"
#include "types/point.h"

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Lua metamethods
static int _ese_camera_lua_gc(lua_State *L);
static int _ese_camera_lua_index(lua_State *L);
static int _ese_camera_lua_newindex(lua_State *L);
static int _ese_camera_lua_tostring(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

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
    EseCamera **ud = (EseCamera **)luaL_testudata(L, 1, CAMERA_META);
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseCamera *camera_state = *ud;
    if (camera_state) {
        // If lua_ref == LUA_NOREF, there are no more references to this camera, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this camera was referenced from C and should not be freed.
        if (ese_camera_get_lua_ref(camera_state) == LUA_NOREF) {
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
        ese_point_lua_push(ese_camera_get_position(camera_state));
        profile_stop(PROFILE_LUA_CAMERA_INDEX, "ese_camera_lua_index (position)");
        return 1;
    } else if (strcmp(key, "rotation") == 0) {
        lua_pushnumber(L, ese_camera_get_rotation(camera_state));
        profile_stop(PROFILE_LUA_CAMERA_INDEX, "ese_camera_lua_index (rotation)");
        return 1;
    } else if (strcmp(key, "scale") == 0) {
        lua_pushnumber(L, ese_camera_get_scale(camera_state));
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
        ese_camera_set_rotation(camera_state, (float)lua_tonumber(L, 3));
        profile_stop(PROFILE_LUA_CAMERA_NEWINDEX, "ese_camera_lua_newindex (rotation)");
        return 0;
    } else if (strcmp(key, "scale") == 0) {
        if (!lua_isnumber(L, 3)) {
            profile_cancel(PROFILE_LUA_CAMERA_NEWINDEX);
            return luaL_error(L, "scale must be a number");
        }
        ese_camera_set_scale(camera_state, (float)lua_tonumber(L, 3));
        profile_stop(PROFILE_LUA_CAMERA_NEWINDEX, "ese_camera_lua_newindex (scale)");
        return 0;
    } else if (strcmp(key, "position") == 0) {
        EsePoint *new_position_point = ese_point_lua_get(L, 3);
        if (!new_position_point) {
            profile_cancel(PROFILE_LUA_CAMERA_NEWINDEX);
            return luaL_error(L, "position must be a EsePoint object");
        }
        // Copy values, don't copy reference (ownership safety)
        ese_point_set_x(ese_camera_get_position(camera_state), ese_point_get_x(new_position_point));
        ese_point_set_y(ese_camera_get_position(camera_state), ese_point_get_y(new_position_point));
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
             (void*)camera_state, ese_point_get_x(ese_camera_get_position(camera_state)), ese_point_get_y(ese_camera_get_position(camera_state)), 
             ese_camera_get_rotation(camera_state), ese_camera_get_scale(camera_state));
    lua_pushstring(L, buf);

    return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Internal Lua initialization function for EseCamera
 * 
 * @details This function is called by the public ese_camera_lua_init function
 *          to set up all the private Lua metamethods and methods for EseCamera.
 * 
 * @param engine EseLuaEngine pointer where the EseCamera type will be registered
 */
void _ese_camera_lua_init(EseLuaEngine *engine) {
    // Create metatable
    lua_engine_new_object_meta(engine, CAMERA_META, 
        _ese_camera_lua_index, 
        _ese_camera_lua_newindex, 
        _ese_camera_lua_gc, 
        _ese_camera_lua_tostring);
}
