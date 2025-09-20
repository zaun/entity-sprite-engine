#include <string.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "scripting/lua_value.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/types.h"
#include "types/point.h"

// The actual EseCamera struct definition (private to this file)
typedef struct EseCamera {
    EsePoint *position; /**< The position of the camera as a EsePoint object */
    float rotation; /**< The rotation of the camera in radians */
    float scale; /**< The scale/zoom level of the camera */

    int lua_ref;       /**< Lua registry reference to its own proxy table */
    int lua_ref_count; /**< Number of times this camera has been referenced in C */
} EseCamera;

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseCamera *_ese_camera_make(void);

// Lua metamethods
static EseLuaValue* _ese_camera_lua_gc(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_camera_lua_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_camera_lua_newindex(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_camera_lua_tostring(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

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
static EseCamera *_ese_camera_make() {
    EseCamera *camera_state = (EseCamera *)memory_manager.malloc(sizeof(EseCamera), MMTAG_CAMERA);
    camera_state->position = NULL;
    camera_state->rotation = 0.0f;
    camera_state->scale = 1.0f;
    camera_state->lua_ref = ESE_LUA_NOREF;
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
static EseLuaValue* _ese_camera_lua_gc(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return NULL;
    }

    // Get the camera from the first argument
    if (!lua_value_is_camera(argv[0])) {
        return NULL;
    }

    EseCamera *camera_state = lua_value_get_camera(argv[0]);
    if (camera_state) {
        // If lua_ref == ESE_LUA_NOREF, there are no more references to this camera, 
        // so we can free it.
        // If lua_ref != ESE_LUA_NOREF, this camera was referenced from C and should not be freed.
        if (camera_state->lua_ref == ESE_LUA_NOREF) {
            ese_camera_destroy(enginem, camera_state);
        }
    }

    return NULL;
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
static EseLuaValue* _ese_camera_lua_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_CAMERA_INDEX);
    
    if (argc != 2) {
        profile_cancel(PROFILE_LUA_CAMERA_INDEX);
        return lua_value_create_nil("result");
    }
    
    // Get the camera from the first argument (should be camera)
    if (!lua_value_is_camera(argv[0])) {
        profile_cancel(PROFILE_LUA_CAMERA_INDEX);
        return lua_value_create_nil("result");
    }
    
    EseCamera *camera_state = lua_value_get_camera(argv[0]);
    if (!camera_state) {
        profile_cancel(PROFILE_LUA_CAMERA_INDEX);
        return lua_value_create_nil("result");
    }
    
    // Get the key from the second argument (should be string)
    if (!lua_value_is_string(argv[1])) {
        profile_cancel(PROFILE_LUA_CAMERA_INDEX);
        return lua_value_create_nil("result");
    }
    
    const char *key = lua_value_get_string(argv[1]);
    if (!key) {
        profile_cancel(PROFILE_LUA_CAMERA_INDEX);
        return lua_value_create_nil("result");
    }

    if (strcmp(key, "position") == 0) {
        if (camera_state->position == NULL) {
            profile_stop(PROFILE_LUA_CAMERA_INDEX, "ese_camera_lua_index (position_nil)");
            return lua_value_create_nil("result");
        } else {
            profile_stop(PROFILE_LUA_CAMERA_INDEX, "ese_camera_lua_index (position)");
            return lua_value_create_point("result", camera_state->position);
        }
    } else if (strcmp(key, "rotation") == 0) {
        profile_stop(PROFILE_LUA_CAMERA_INDEX, "ese_camera_lua_index (rotation)");
        return lua_value_create_number("result", camera_state->rotation);
    } else if (strcmp(key, "scale") == 0) {
        profile_stop(PROFILE_LUA_CAMERA_INDEX, "ese_camera_lua_index (scale)");
        return lua_value_create_number("result", camera_state->scale);
    }
    
    profile_stop(PROFILE_LUA_CAMERA_INDEX, "ese_camera_lua_index (invalid)");
    return lua_value_create_nil("result");
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
static EseLuaValue* _ese_camera_lua_newindex(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_CAMERA_NEWINDEX);
    
    if (argc != 3) {
        profile_cancel(PROFILE_LUA_CAMERA_NEWINDEX);
        return lua_value_create_error("result", "camera.rotation, camera.scale, camera.position assignment requires exactly 3 arguments");
    }

    // Get the camera from the first argument (self)
    if (!lua_value_is_camera(argv[0])) {
        profile_cancel(PROFILE_LUA_CAMERA_NEWINDEX);
        return lua_value_create_error("result", "camera property assignment: first argument must be a Camera");
    }
    
    EseCamera *camera_state = lua_value_get_camera(argv[0]);
    if (!camera_state) {
        profile_cancel(PROFILE_LUA_CAMERA_NEWINDEX);
        return lua_value_create_error("result", "camera property assignment: camera is NULL");
    }
    
    // Get the key
    if (!lua_value_is_string(argv[1])) {
        profile_cancel(PROFILE_LUA_CAMERA_NEWINDEX);
        return lua_value_create_error("result", "camera property assignment: property name (rotation, scale, position) must be a string");
    }
    
    const char *key = lua_value_get_string(argv[1]);

    if (strcmp(key, "rotation") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_CAMERA_NEWINDEX);
            return lua_value_create_error("result", "camera.rotation must be a number");
        }
        camera_state->rotation = (float)lua_value_get_number(argv[2]);
        profile_stop(PROFILE_LUA_CAMERA_NEWINDEX, "ese_camera_lua_newindex (rotation)");
        return NULL;
    } else if (strcmp(key, "scale") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_CAMERA_NEWINDEX);
            return lua_value_create_error("result", "camera.scale must be a number");
        }
        camera_state->scale = (float)lua_value_get_number(argv[2]);
        profile_stop(PROFILE_LUA_CAMERA_NEWINDEX, "ese_camera_lua_newindex (scale)");
        return NULL;
    } else if (strcmp(key, "position") == 0) {
        if (!lua_value_is_point(argv[2])) {
            profile_cancel(PROFILE_LUA_CAMERA_NEWINDEX);
            return lua_value_create_error("result", "camera.position must be a Point object");
        }
        EsePoint *new_position_point = lua_value_get_point(argv[2]);
        if (!new_position_point) {
            profile_cancel(PROFILE_LUA_CAMERA_NEWINDEX);
            return lua_value_create_error("result", "point is NULL");
        }
        // Copy values, don't copy reference (ownership safety)
        if (camera_state->position) {
            ese_point_set_x(camera_state->position, ese_point_get_x(new_position_point));
            ese_point_set_y(camera_state->position, ese_point_get_y(new_position_point));
        }
        profile_stop(PROFILE_LUA_CAMERA_NEWINDEX, "ese_camera_lua_newindex (position)");
        return NULL;
    }
    
    profile_stop(PROFILE_LUA_CAMERA_NEWINDEX, "ese_camera_lua_newindex (invalid)");
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "unknown or unassignable property '%s'", key);
    return lua_value_create_error("result", error_msg);
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
static EseLuaValue* _ese_camera_lua_tostring(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return lua_value_create_string("result", "Camera: (invalid)");
    }

    if (!lua_value_is_camera(argv[0])) {
        return lua_value_create_string("result", "Camera: (invalid)");
    }

    EseCamera *camera_state = lua_value_get_camera(argv[0]);
    if (!camera_state) {
        return lua_value_create_string("result", "Camera: (invalid)");
    }

    char buf[128];
    if (camera_state->position) {
        snprintf(buf, sizeof(buf), "Camera: %p (pos=(%.2f, %.2f), rot=%.2f, scale=%.2f)", 
                 (void*)camera_state, ese_point_get_x(camera_state->position), ese_point_get_y(camera_state->position), 
                 camera_state->rotation, camera_state->scale);
    } else {
        snprintf(buf, sizeof(buf), "Camera: %p (pos=(nil), rot=%.2f, scale=%.2f)", 
                 (void*)camera_state, camera_state->rotation, camera_state->scale);
    }
    
    return lua_value_create_string("result", buf);
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseCamera *ese_camera_create(EseLuaEngine *engine) {
    log_debug("CAMERA", "Creating camera state");
    EseCamera *camera_state = _ese_camera_make();

    camera_state->position = ese_point_create(engine);
    ese_point_ref(engine, camera_state->position);

    return camera_state;
}

EseCamera *ese_camera_copy(const EseCamera *source) {
    if (source == NULL) {
        return NULL;
    }

    EseCamera *copy = (EseCamera *)memory_manager.malloc(sizeof(EseCamera), MMTAG_CAMERA);
    copy->position = ese_point_copy(source->position);
    copy->rotation = source->rotation;
    copy->scale = source->scale;
    copy->lua_ref = ESE_LUA_NOREF;
    copy->lua_ref_count = 0;
    return copy;
}

void ese_camera_destroy(EseLuaEngine *engine, EseCamera *camera_state) {
    if (!camera_state) return;
    
    if (camera_state->lua_ref == ESE_LUA_NOREF) {
        // No Lua references, safe to free immediately
        if (camera_state->position) {
            ese_point_unref(enginecamera_state->position);
        }
        memory_manager.free(camera_state);
    } else {
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
        ese_camera_unref(engine, camera_state);
    }
}

// Lua integration
void ese_camera_lua_init(EseLuaEngine *engine) {
    // Add the metatable using the new API
    lua_engine_add_metatable(engine, "CameraMeta", 
                            _ese_camera_lua_index, 
                            _ese_camera_lua_newindex, 
                            _ese_camera_lua_gc, 
                            _ese_camera_lua_tostring);
    
    // No global table needed - camera is not directly constructible from Lua
}

void ese_camera_lua_push(EseLuaEngine *engine, EseCamera *camera_state) {
    log_assert("CAMERA", engine, "ese_camera_lua_push called with NULL engine");
    log_assert("CAMERA", camera_state, "ese_camera_lua_push called with NULL camera_state");

    if (camera_state->lua_ref == ESE_LUA_NOREF) {
        // Lua-owned: create a new userdata using the engine API
        EseCamera **ud = (EseCamera **)lua_engine_create_userdata(engine, "CameraMeta", sizeof(EseCamera *));
        *ud = camera_state;
    } else {
        // C-owned: get from registry using the engine API
        lua_engine_get_registry_value(engine, camera_state->lua_ref);
    }
}

EseCamera *ese_camera_lua_get(EseLuaEngine *engine, int idx) {
    log_assert("CAMERA", engine, "ese_camera_lua_get called with NULL engine");
    
    // Check if the value at idx is userdata
    if (!lua_engine_is_userdata(engine, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseCamera **ud = (EseCamera **)lua_engine_test_userdata(engine, idx, "CameraMeta");
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_camera_ref(EseLuaEngine *engine, EseCamera *camera_state) {
    log_assert("CAMERA", camera_state, "ese_camera_ref called with NULL camera_state");
    
    if (camera_state->lua_ref == ESE_LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseCamera **ud = (EseCamera **)lua_engine_create_userdata(engine, "CameraMeta", sizeof(EseCamera *));
        *ud = camera_state;

        // Get the reference from the engine's Lua state
        camera_state->lua_ref = lua_engine_get_reference(engine);
        camera_state->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        camera_state->lua_ref_count++;
    }

    profile_count_add("ese_camera_ref_count");
}

void ese_camera_unref(EseLuaEngine *engine, EseCamera *camera_state) {
    if (!camera_state) return;
    
    if (camera_state->lua_ref != ESE_LUA_NOREF && camera_state->lua_ref_count > 0) {
        camera_state->lua_ref_count--;
        
        if (camera_state->lua_ref_count == 0) {
            // No more references - remove from registry
            // Note: We need to use the engine API to unref
            // For now, just reset the ref
            camera_state->lua_ref = ESE_LUA_NOREF;
        }
    }

    profile_count_add("ese_camera_unref_count");
}

// Property access
void ese_camera_set_position(EseCamera *camera_state, EsePoint *position) {
    log_assert("CAMERA", camera_state, "ese_camera_set_position called with NULL camera_state");
    log_assert("CAMERA", position, "ese_camera_set_position called with NULL position");
    
    if (camera_state->position) {
        ese_point_destroy(camera_state->position);
    }
    camera_state->position = ese_point_copy(position);
}

EsePoint *ese_camera_get_position(const EseCamera *camera_state) {
    log_assert("CAMERA", camera_state, "ese_camera_get_position called with NULL camera_state");
    return camera_state->position;
}

void ese_camera_set_rotation(EseCamera *camera_state, float rotation) {
    log_assert("CAMERA", camera_state, "ese_camera_set_rotation called with NULL camera_state");
    camera_state->rotation = rotation;
}

float ese_camera_get_rotation(const EseCamera *camera_state) {
    log_assert("CAMERA", camera_state, "ese_camera_get_rotation called with NULL camera_state");
    return camera_state->rotation;
}

void ese_camera_set_scale(EseCamera *camera_state, float scale) {
    log_assert("CAMERA", camera_state, "ese_camera_set_scale called with NULL camera_state");
    camera_state->scale = scale;
}

float ese_camera_get_scale(const EseCamera *camera_state) {
    log_assert("CAMERA", camera_state, "ese_camera_get_scale called with NULL camera_state");
    return camera_state->scale;
}

// Lua-related access
int ese_camera_get_lua_ref(const EseCamera *camera_state) {
    log_assert("CAMERA", camera_state, "ese_camera_get_lua_ref called with NULL camera_state");
    return camera_state->lua_ref;
}

int ese_camera_get_lua_ref_count(const EseCamera *camera_state) {
    log_assert("CAMERA", camera_state, "ese_camera_get_lua_ref_count called with NULL camera_state");
    return camera_state->lua_ref_count;
}

size_t ese_camera_sizeof(void) {
    return sizeof(EseCamera);
}
