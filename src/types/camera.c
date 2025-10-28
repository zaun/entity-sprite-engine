#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/camera_lua.h"
#include "types/types.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <string.h>

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseCamera *_ese_camera_make(EseLuaEngine *engine);

// Private setters
static void _ese_camera_set_lua_ref(EseCamera *camera_state, int lua_ref);
static void _ese_camera_set_lua_ref_count(EseCamera *camera_state,
                                          int lua_ref_count);
static void _ese_camera_set_state(EseCamera *camera_state, lua_State *state);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseCamera instance with default values
 *
 * Allocates memory for a new EseCamera and initializes all fields to safe
 * defaults. The camera starts with no position, zero rotation, unit scale, and
 * no Lua state or references.
 *
 * @return Pointer to the newly created EseCamera, or NULL on allocation failure
 */
static EseCamera *_ese_camera_make(EseLuaEngine *engine) {
  EseCamera *camera_state =
      (EseCamera *)memory_manager.malloc(sizeof(EseCamera), MMTAG_CAMERA);
  ese_camera_set_position(camera_state, ese_point_create(engine));
  ese_point_ref(ese_camera_get_position(camera_state));
  ese_camera_set_rotation(camera_state, 0.0f);
  ese_camera_set_scale(camera_state, 1.0f);
  _ese_camera_set_state(camera_state, NULL);
  _ese_camera_set_lua_ref(camera_state, LUA_NOREF);
  _ese_camera_set_lua_ref_count(camera_state, 0);
  return camera_state;
}

// ========================================
// ACCESSOR FUNCTIONS
// ========================================

/**
 * @brief Gets the position of the camera
 *
 * @param camera_state Pointer to the EseCamera object
 * @return The position EsePoint of the camera
 */
EsePoint *ese_camera_get_position(const EseCamera *camera_state) {
  return camera_state->position;
}

/**
 * @brief Sets the position of the camera
 *
 * @param camera_state Pointer to the EseCamera object
 * @param position The new position EsePoint
 */
void ese_camera_set_position(EseCamera *camera_state, EsePoint *position) {
  log_assert("CAMERA", camera_state != NULL,
             "ese_camera_set_position: camera_state cannot be NULL");
  log_assert("CAMERA", position != NULL,
             "ese_camera_set_position: position cannot be NULL");
  camera_state->position = position;
}

/**
 * @brief Gets the rotation of the camera in radians
 *
 * @param camera_state Pointer to the EseCamera object
 * @return The rotation of the camera in radians
 */
float ese_camera_get_rotation(const EseCamera *camera_state) {
  return camera_state->rotation;
}

/**
 * @brief Sets the rotation of the camera in radians
 *
 * @param camera_state Pointer to the EseCamera object
 * @param rotation The new rotation value in radians
 */
void ese_camera_set_rotation(EseCamera *camera_state, float rotation) {
  log_assert("CAMERA", camera_state != NULL,
             "ese_camera_set_rotation: camera_state cannot be NULL");
  camera_state->rotation = rotation;
}

/**
 * @brief Gets the scale of the camera
 *
 * @param camera_state Pointer to the EseCamera object
 * @return The scale of the camera
 */
float ese_camera_get_scale(const EseCamera *camera_state) {
  return camera_state->scale;
}

/**
 * @brief Sets the scale of the camera
 *
 * @param camera_state Pointer to the EseCamera object
 * @param scale The new scale value
 */
void ese_camera_set_scale(EseCamera *camera_state, float scale) {
  log_assert("CAMERA", camera_state != NULL,
             "ese_camera_set_scale: camera_state cannot be NULL");
  camera_state->scale = scale;
}

/**
 * @brief Gets the Lua state associated with the camera
 *
 * @param camera_state Pointer to the EseCamera object
 * @return The Lua state associated with the camera
 */
lua_State *ese_camera_get_state(const EseCamera *camera_state) {
  return camera_state->state;
}

/**
 * @brief Gets the Lua registry reference for the camera
 *
 * @param camera_state Pointer to the EseCamera object
 * @return The Lua registry reference for the camera
 */
int ese_camera_get_lua_ref(const EseCamera *camera_state) {
  return camera_state->lua_ref;
}

/**
 * @brief Gets the Lua reference count for the camera
 *
 * @param camera_state Pointer to the EseCamera object
 * @return The Lua reference count for the camera
 */
int ese_camera_get_lua_ref_count(const EseCamera *camera_state) {
  return camera_state->lua_ref_count;
}

/**
 * @brief Sets the Lua registry reference for the camera (private)
 *
 * @param camera_state Pointer to the EseCamera object
 * @param lua_ref The new Lua registry reference value
 */
static void _ese_camera_set_lua_ref(EseCamera *camera_state, int lua_ref) {
  log_assert("CAMERA", camera_state != NULL,
             "_ese_camera_set_lua_ref: camera_state cannot be NULL");
  camera_state->lua_ref = lua_ref;
}

/**
 * @brief Sets the Lua reference count for the camera (private)
 *
 * @param camera_state Pointer to the EseCamera object
 * @param lua_ref_count The new Lua reference count value
 */
static void _ese_camera_set_lua_ref_count(EseCamera *camera_state,
                                          int lua_ref_count) {
  log_assert("CAMERA", camera_state != NULL,
             "_ese_camera_set_lua_ref_count: camera_state cannot be NULL");
  camera_state->lua_ref_count = lua_ref_count;
}

/**
 * @brief Sets the Lua state associated with the camera (private)
 *
 * @param camera_state Pointer to the EseCamera object
 * @param state The new Lua state value
 */
static void _ese_camera_set_state(EseCamera *camera_state, lua_State *state) {
  log_assert("CAMERA", camera_state != NULL,
             "_ese_camera_set_state: camera_state cannot be NULL");
  camera_state->state = state;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseCamera *ese_camera_create(EseLuaEngine *engine) {
  log_debug("CAMERA", "Creating camera state");
  EseCamera *camera_state = _ese_camera_make(engine);
  _ese_camera_set_state(camera_state, engine->runtime);

  return camera_state;
}

EseCamera *ese_camera_copy(const EseCamera *source) {
  if (source == NULL) {
    return NULL;
  }

  EseCamera *copy =
      (EseCamera *)memory_manager.malloc(sizeof(EseCamera), MMTAG_CAMERA);
  ese_camera_set_position(copy,
                          ese_point_copy(ese_camera_get_position(source)));
  ese_point_ref(ese_camera_get_position(copy));
  ese_camera_set_rotation(copy, ese_camera_get_rotation(source));
  ese_camera_set_scale(copy, ese_camera_get_scale(source));
  _ese_camera_set_state(copy, ese_camera_get_state(source));
  _ese_camera_set_lua_ref(copy, LUA_NOREF);
  _ese_camera_set_lua_ref_count(copy, 0);
  return copy;
}

void ese_camera_destroy(EseCamera *camera_state) {
  if (!camera_state)
    return;

  if (ese_camera_get_lua_ref(camera_state) == LUA_NOREF) {
    // No Lua references, safe to free immediately
    ese_point_unref(ese_camera_get_position(camera_state));
    ese_point_destroy(ese_camera_get_position(camera_state));
    memory_manager.free(camera_state);
  } else {
    // Don't free memory here - let Lua GC handle it
    // As the script may still have a reference to it.
    ese_camera_unref(camera_state);
  }
}

// Lua integration
void ese_camera_lua_init(EseLuaEngine *engine) {
  log_assert("CAMERA_STATE", engine,
             "ese_camera_lua_init called with NULL engine");
  log_assert("CAMERA_STATE", engine->runtime,
             "ese_camera_lua_init called with NULL engine->runtime");

  _ese_camera_lua_init(engine);
}

void ese_camera_lua_push(EseCamera *camera_state) {
  log_assert("CAMERA", camera_state,
             "ese_camera_lua_push called with NULL camera_state");

  if (ese_camera_get_lua_ref(camera_state) == LUA_NOREF) {
    // Lua-owned: create a new userdata
    EseCamera **ud = (EseCamera **)lua_newuserdata(
        ese_camera_get_state(camera_state), sizeof(EseCamera *));
    *ud = camera_state;

    // Attach metatable
    luaL_getmetatable(ese_camera_get_state(camera_state), CAMERA_META);
    lua_setmetatable(ese_camera_get_state(camera_state), -2);
  } else {
    // C-owned: get from registry
    lua_rawgeti(ese_camera_get_state(camera_state), LUA_REGISTRYINDEX,
                ese_camera_get_lua_ref(camera_state));
  }
}

EseCamera *ese_camera_lua_get(lua_State *L, int idx) {
  log_assert("CAMERA", L, "ese_camera_lua_get called with NULL Lua state");

  // Check if the value at idx is userdata
  if (!lua_isuserdata(L, idx)) {
    return NULL;
  }

  // Get the userdata and check metatable
  EseCamera **ud = (EseCamera **)luaL_testudata(L, idx, CAMERA_META);
  if (!ud) {
    return NULL; // Wrong metatable or not userdata
  }

  return *ud;
}

void ese_camera_ref(EseCamera *camera_state) {
  log_assert("CAMERA", camera_state,
             "ese_camera_ref called with NULL camera_state");

  if (ese_camera_get_lua_ref(camera_state) == LUA_NOREF) {
    // First time referencing - create userdata and store reference
    EseCamera **ud = (EseCamera **)lua_newuserdata(
        ese_camera_get_state(camera_state), sizeof(EseCamera *));
    *ud = camera_state;

    // Attach metatable
    luaL_getmetatable(ese_camera_get_state(camera_state), CAMERA_META);
    lua_setmetatable(ese_camera_get_state(camera_state), -2);

    // Store hard reference to prevent garbage collection
    int ref = luaL_ref(ese_camera_get_state(camera_state), LUA_REGISTRYINDEX);
    _ese_camera_set_lua_ref(camera_state, ref);
    _ese_camera_set_lua_ref_count(camera_state, 1);
  } else {
    // Already referenced - just increment count
    _ese_camera_set_lua_ref_count(
        camera_state, ese_camera_get_lua_ref_count(camera_state) + 1);
  }

  profile_count_add("ese_camera_ref_count");
}

void ese_camera_unref(EseCamera *camera_state) {
  if (!camera_state)
    return;

  if (ese_camera_get_lua_ref(camera_state) != LUA_NOREF &&
      ese_camera_get_lua_ref_count(camera_state) > 0) {
    _ese_camera_set_lua_ref_count(
        camera_state, ese_camera_get_lua_ref_count(camera_state) - 1);

    if (ese_camera_get_lua_ref_count(camera_state) == 0) {
      // No more references - remove from registry
      luaL_unref(ese_camera_get_state(camera_state), LUA_REGISTRYINDEX,
                 ese_camera_get_lua_ref(camera_state));
      _ese_camera_set_lua_ref(camera_state, LUA_NOREF);
    }
  }

  profile_count_add("ese_camera_unref_count");
}
