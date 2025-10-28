/**
 * @file vector.c
 * @brief Implementation of 2D vector type with floating-point components
 * @details Implements vector operations, Lua integration, and JSON
 * serialization
 *
 * @copyright Copyright (c) 2024 ESE Project
 * @license See LICENSE.md for license information
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/vector.h"
#include "types/vector_lua.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"

// ========================================
// PRIVATE STRUCT DEFINITION
// ========================================

/**
 * @brief Internal structure for EseVector
 *
 * @details This structure stores the x and y components of a vector in 2D
 * space.
 */
struct EseVector {
  float x; /** The x-component of the vector */
  float y; /** The y-component of the vector */

  lua_State *state;  /** Lua State this EseVector belongs to */
  int lua_ref;       /** Lua registry reference to its own proxy table */
  int lua_ref_count; /** Number of times this vector has been referenced in C */
};

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers

// Private setters for Lua state management
static void _ese_vector_set_lua_ref(EseVector *vector, int lua_ref);
static void _ese_vector_set_lua_ref_count(EseVector *vector, int lua_ref_count);
static void _ese_vector_set_state(EseVector *vector, lua_State *state);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseVector instance with default values
 *
 * Allocates memory for a new EseVector and initializes all fields to safe
 * defaults. The vector starts at origin (0,0) with no Lua state or references.
 *
 * @return Pointer to the newly created EseVector, or NULL on allocation failure
 */
EseVector *_ese_vector_make() {
  EseVector *vector =
      (EseVector *)memory_manager.malloc(sizeof(EseVector), MMTAG_VECTOR);
  vector->x = 0.0f;
  vector->y = 0.0f;
  vector->state = NULL;
  vector->lua_ref = LUA_NOREF;
  vector->lua_ref_count = 0;
  return vector;
}

// Private setters for Lua state management
static void _ese_vector_set_lua_ref(EseVector *vector, int lua_ref) {
  if (vector) {
    vector->lua_ref = lua_ref;
  }
}

static void _ese_vector_set_lua_ref_count(EseVector *vector,
                                          int lua_ref_count) {
  if (vector) {
    vector->lua_ref_count = lua_ref_count;
  }
}

static void _ese_vector_set_state(EseVector *vector, lua_State *state) {
  if (vector) {
    vector->state = state;
  }
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseVector *ese_vector_create(EseLuaEngine *engine) {
  EseVector *vector = _ese_vector_make();
  _ese_vector_set_state(vector, engine->runtime);
  return vector;
}

EseVector *ese_vector_copy(const EseVector *source) {
  log_assert("VECTOR", source, "ese_vector_copy called with NULL source");

  EseVector *copy =
      (EseVector *)memory_manager.malloc(sizeof(EseVector), MMTAG_VECTOR);
  ese_vector_set_x(copy, ese_vector_get_x(source));
  ese_vector_set_y(copy, ese_vector_get_y(source));
  _ese_vector_set_state(copy, ese_vector_get_state(source));
  _ese_vector_set_lua_ref(copy, LUA_NOREF);
  _ese_vector_set_lua_ref_count(copy, 0);
  return copy;
}

void ese_vector_destroy(EseVector *vector) {
  if (!vector)
    return;

  if (ese_vector_get_lua_ref(vector) == LUA_NOREF) {
    // No Lua references, safe to free immediately
    memory_manager.free(vector);
  } else {
    ese_vector_unref(vector);
    // Don't free memory here - let Lua GC handle it
    // As the script may still have a reference to it.
  }
}

size_t ese_vector_sizeof(void) { return sizeof(EseVector); }

/**
 * @brief Serializes an EseVector to a cJSON object.
 *
 * Creates a cJSON object representing the vector with type "VECTOR"
 * and x, y components. Only serializes the
 * vector data, not Lua-related fields.
 *
 * @param vector Pointer to the EseVector object to serialize
 * @return cJSON object representing the vector, or NULL on failure
 */
cJSON *ese_vector_serialize(const EseVector *vector) {
  log_assert("VECTOR", vector, "ese_vector_serialize called with NULL vector");

  cJSON *json = cJSON_CreateObject();
  if (!json) {
    log_error("VECTOR",
              "Failed to create cJSON object for vector serialization");
    return NULL;
  }

  // Add type field
  cJSON *type = cJSON_CreateString("VECTOR");
  if (!type || !cJSON_AddItemToObject(json, "type", type)) {
    log_error("VECTOR", "Failed to add type field to vector serialization");
    cJSON_Delete(json);
    return NULL;
  }

  // Add x component
  cJSON *x = cJSON_CreateNumber((double)vector->x);
  if (!x || !cJSON_AddItemToObject(json, "x", x)) {
    log_error("VECTOR", "Failed to add x field to vector serialization");
    cJSON_Delete(json);
    return NULL;
  }

  // Add y component
  cJSON *y = cJSON_CreateNumber((double)vector->y);
  if (!y || !cJSON_AddItemToObject(json, "y", y)) {
    log_error("VECTOR", "Failed to add y field to vector serialization");
    cJSON_Delete(json);
    return NULL;
  }

  return json;
}

/**
 * @brief Deserializes an EseVector from a cJSON object.
 *
 * Creates a new EseVector from a cJSON object with type "VECTOR"
 * and x, y components. The vector is created
 * with the specified engine and must be explicitly referenced with
 * ese_vector_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for vector creation
 * @param data cJSON object containing vector data
 * @return Pointer to newly created EseVector object, or NULL on failure
 */
EseVector *ese_vector_deserialize(EseLuaEngine *engine, const cJSON *data) {
  log_assert("VECTOR", data, "ese_vector_deserialize called with NULL data");

  if (!cJSON_IsObject(data)) {
    log_error("VECTOR",
              "Vector deserialization failed: data is not a JSON object");
    return NULL;
  }

  // Check type field
  cJSON *type_item = cJSON_GetObjectItem(data, "type");
  if (!type_item || !cJSON_IsString(type_item) ||
      strcmp(type_item->valuestring, "VECTOR") != 0) {
    log_error("VECTOR",
              "Vector deserialization failed: invalid or missing type field");
    return NULL;
  }

  // Get x component
  cJSON *x_item = cJSON_GetObjectItem(data, "x");
  if (!x_item || !cJSON_IsNumber(x_item)) {
    log_error("VECTOR",
              "Vector deserialization failed: invalid or missing x field");
    return NULL;
  }

  // Get y component
  cJSON *y_item = cJSON_GetObjectItem(data, "y");
  if (!y_item || !cJSON_IsNumber(y_item)) {
    log_error("VECTOR",
              "Vector deserialization failed: invalid or missing y field");
    return NULL;
  }

  // Create new vector
  EseVector *vector = ese_vector_create(engine);
  ese_vector_set_x(vector, (float)x_item->valuedouble);
  ese_vector_set_y(vector, (float)y_item->valuedouble);

  return vector;
}

// Property access
void ese_vector_set_x(EseVector *vector, float x) {
  log_assert("VECTOR", vector, "ese_vector_set_x called with NULL vector");
  vector->x = x;
}

float ese_vector_get_x(const EseVector *vector) {
  log_assert("VECTOR", vector, "ese_vector_get_x called with NULL vector");
  return vector->x;
}

void ese_vector_set_y(EseVector *vector, float y) {
  log_assert("VECTOR", vector, "ese_vector_set_y called with NULL vector");
  vector->y = y;
}

float ese_vector_get_y(const EseVector *vector) {
  log_assert("VECTOR", vector, "ese_vector_get_y called with NULL vector");
  return vector->y;
}

// Lua-related access
lua_State *ese_vector_get_state(const EseVector *vector) {
  log_assert("VECTOR", vector, "ese_vector_get_state called with NULL vector");
  return vector->state;
}

int ese_vector_get_lua_ref(const EseVector *vector) {
  log_assert("VECTOR", vector,
             "ese_vector_get_lua_ref called with NULL vector");
  return vector->lua_ref;
}

int ese_vector_get_lua_ref_count(const EseVector *vector) {
  log_assert("VECTOR", vector,
             "ese_vector_get_lua_ref_count called with NULL vector");
  return vector->lua_ref_count;
}

void ese_vector_set_state(EseVector *vector, lua_State *state) {
  if (vector) {
    vector->state = state;
  }
}

// Lua integration
void ese_vector_lua_init(EseLuaEngine *engine) { _ese_vector_lua_init(engine); }

void ese_vector_lua_push(EseVector *vector) {
  log_assert("VECTOR", vector, "ese_vector_lua_push called with NULL vector");

  if (ese_vector_get_lua_ref(vector) == LUA_NOREF) {
    // Lua-owned: create a new userdata
    EseVector **ud = (EseVector **)lua_newuserdata(ese_vector_get_state(vector),
                                                   sizeof(EseVector *));
    *ud = vector;

    // Attach metatable
    luaL_getmetatable(ese_vector_get_state(vector), VECTOR_PROXY_META);
    lua_setmetatable(ese_vector_get_state(vector), -2);
  } else {
    // C-owned: get from registry
    lua_rawgeti(ese_vector_get_state(vector), LUA_REGISTRYINDEX,
                ese_vector_get_lua_ref(vector));
  }
}

EseVector *ese_vector_lua_get(lua_State *L, int idx) {
  log_assert("VECTOR", L, "ese_vector_lua_get called with NULL Lua state");

  // Check if the value at idx is userdata
  if (!lua_isuserdata(L, idx)) {
    return NULL;
  }

  // Get the userdata and check metatable
  EseVector **ud = (EseVector **)luaL_testudata(L, idx, VECTOR_PROXY_META);
  if (!ud) {
    return NULL; // Wrong metatable or not userdata
  }

  return *ud;
}

void ese_vector_ref(EseVector *vector) {
  log_assert("VECTOR", vector, "ese_vector_ref called with NULL vector");

  if (ese_vector_get_lua_ref(vector) == LUA_NOREF) {
    // First time referencing - create userdata and store reference
    EseVector **ud = (EseVector **)lua_newuserdata(ese_vector_get_state(vector),
                                                   sizeof(EseVector *));
    *ud = vector;

    // Attach metatable
    luaL_getmetatable(ese_vector_get_state(vector), VECTOR_PROXY_META);
    lua_setmetatable(ese_vector_get_state(vector), -2);

    // Store hard reference to prevent garbage collection
    int lua_ref = luaL_ref(ese_vector_get_state(vector), LUA_REGISTRYINDEX);
    _ese_vector_set_lua_ref(vector, lua_ref);
    _ese_vector_set_lua_ref_count(vector, 1);
  } else {
    // Already referenced - just increment count
    _ese_vector_set_lua_ref_count(vector,
                                  ese_vector_get_lua_ref_count(vector) + 1);
  }

  profile_count_add("ese_vector_ref_count");
}

void ese_vector_unref(EseVector *vector) {
  if (!vector)
    return;

  if (ese_vector_get_lua_ref(vector) != LUA_NOREF &&
      ese_vector_get_lua_ref_count(vector) > 0) {
    int new_count = ese_vector_get_lua_ref_count(vector) - 1;
    _ese_vector_set_lua_ref_count(vector, new_count);

    if (new_count == 0) {
      // No more references - remove from registry
      luaL_unref(ese_vector_get_state(vector), LUA_REGISTRYINDEX,
                 ese_vector_get_lua_ref(vector));
      _ese_vector_set_lua_ref(vector, LUA_NOREF);
    }
  }

  profile_count_add("vector_unref_count");
}

// Mathematical operations
void ese_vector_set_direction(EseVector *vector, const char *direction,
                              float magnitude) {
  if (!vector || !direction)
    return;

  float dx = 0.0f, dy = 0.0f;

  // Parse direction string
  size_t len = strlen(direction);
  for (size_t i = 0; i < len; i++) {
    switch (direction[i]) {
    case 'n':
    case 'N':
      dy += 1.0f;
      break;
    case 's':
    case 'S':
      dy -= 1.0f;
      break;
    case 'e':
    case 'E':
      dx += 1.0f;
      break;
    case 'w':
    case 'W':
      dx -= 1.0f;
      break;
    default:
      return;
    }
  }

  // Normalize for diagonal directions
  float length = sqrtf(dx * dx + dy * dy);
  if (length > 0.0f) {
    dx /= length;
    dy /= length;
    // Apply magnitude
    vector->x = dx * magnitude;
    vector->y = dy * magnitude;
  } else {
    // Invalid direction - set to zero
    vector->x = 0.0f;
    vector->y = 0.0f;
  }
}

float ese_vector_magnitude(const EseVector *vector) {
  if (!vector)
    return 0.0f;
  return sqrtf(vector->x * vector->x + vector->y * vector->y);
}

void ese_vector_normalize(EseVector *vector) {
  if (!vector)
    return;

  float magnitude = ese_vector_magnitude(vector);
  if (magnitude > 0.0f) {
    vector->x /= magnitude;
    vector->y /= magnitude;
  }
}
