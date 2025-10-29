/**
 * @file ray.c
 * @brief Implementation of ray type with origin and direction
 * @details Implements ray operations, collision detection, Lua integration, and
 * JSON serialization
 *
 * @copyright Copyright (c) 2024 ESE Project
 * @license See LICENSE.md for license information
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/ray.h"
#include "types/ray_lua.h"
#include "types/rect.h"
#include "types/types.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"

// ========================================
// PRIVATE STRUCT DEFINITION
// ========================================

/**
 * @brief Internal structure for EseRay
 *
 * @details This structure is only visible within this implementation file.
 *          External code must use the provided getter/setter functions.
 */
struct EseRay {
    float x;  /** X coordinate of the ray origin */
    float y;  /** Y coordinate of the ray origin */
    float dx; /** X component of the ray direction */
    float dy; /** Y component of the ray direction */

    lua_State *state;  /** Lua State this EseRay belongs to */
    int lua_ref;       /** Lua registry reference to its own proxy table */
    int lua_ref_count; /** Number of times this ray has been referenced in C */
};

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseRay *_ese_ray_make(void);

// Private static setters for Lua state management
static void _ese_ray_set_lua_ref(EseRay *ray, int lua_ref);
static void _ese_ray_set_lua_ref_count(EseRay *ray, int lua_ref_count);
static void _ese_ray_set_state(EseRay *ray, lua_State *state);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseRay instance with default values
 *
 * Allocates memory for a new EseRay and initializes all fields to safe
 * defaults. The ray starts at origin (0,0) with direction (1,0) and no Lua
 * state or references.
 *
 * @return Pointer to the newly created EseRay, or NULL on allocation failure
 */
static EseRay *_ese_ray_make() {
    EseRay *ray = (EseRay *)memory_manager.malloc(sizeof(EseRay), MMTAG_RAY);
    ray->x = 0.0f;
    ray->y = 0.0f;
    ray->dx = 1.0f;
    ray->dy = 0.0f;
    ray->state = NULL;
    ray->lua_ref = LUA_NOREF;
    ray->lua_ref_count = 0;
    return ray;
}

// Private static setters for Lua state management
static void _ese_ray_set_lua_ref(EseRay *ray, int lua_ref) { ray->lua_ref = lua_ref; }

static void _ese_ray_set_lua_ref_count(EseRay *ray, int lua_ref_count) {
    ray->lua_ref_count = lua_ref_count;
}

static void _ese_ray_set_state(EseRay *ray, lua_State *state) { ray->state = state; }

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseRay *ese_ray_create(EseLuaEngine *engine) {
    log_assert("RAY", engine, "ese_ray_create called with NULL engine");

    EseRay *ray = _ese_ray_make();
    _ese_ray_set_state(ray, engine->runtime);
    return ray;
}

EseRay *ese_ray_copy(const EseRay *source) {
    log_assert("RAY", source, "ese_ray_copy called with NULL source");

    EseRay *copy = (EseRay *)memory_manager.malloc(sizeof(EseRay), MMTAG_RAY);
    copy->x = source->x;
    copy->y = source->y;
    copy->dx = source->dx;
    copy->dy = source->dy;
    _ese_ray_set_state(copy, ese_ray_get_state(source));
    _ese_ray_set_lua_ref(copy, LUA_NOREF);
    _ese_ray_set_lua_ref_count(copy, 0);
    return copy;
}

void ese_ray_destroy(EseRay *ray) {
    if (!ray)
        return;

    if (ese_ray_get_lua_ref(ray) == LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(ray);
    } else {
        ese_ray_unref(ray);
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

// Lua integration
void ese_ray_lua_init(EseLuaEngine *engine) { _ese_ray_lua_init(engine); }

void ese_ray_lua_push(EseRay *ray) {
    log_assert("RAY", ray, "ese_ray_lua_push called with NULL ray");

    if (ese_ray_get_lua_ref(ray) == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseRay **ud = (EseRay **)lua_newuserdata(ese_ray_get_state(ray), sizeof(EseRay *));
        *ud = ray;

        // Attach metatable
        luaL_getmetatable(ese_ray_get_state(ray), RAY_PROXY_META);
        lua_setmetatable(ese_ray_get_state(ray), -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(ese_ray_get_state(ray), LUA_REGISTRYINDEX, ese_ray_get_lua_ref(ray));
    }
}

EseRay *ese_ray_lua_get(lua_State *L, int idx) {
    log_assert("RAY", L, "ese_ray_lua_get called with NULL Lua state");

    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }

    // Get the userdata and check metatable
    EseRay **ud = (EseRay **)luaL_testudata(L, idx, RAY_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }

    return *ud;
}

void ese_ray_ref(EseRay *ray) {
    log_assert("RAY", ray, "ese_ray_ref called with NULL ray");

    if (ese_ray_get_lua_ref(ray) == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseRay **ud = (EseRay **)lua_newuserdata(ese_ray_get_state(ray), sizeof(EseRay *));
        *ud = ray;

        // Attach metatable
        luaL_getmetatable(ese_ray_get_state(ray), RAY_PROXY_META);
        lua_setmetatable(ese_ray_get_state(ray), -2);

        // Store hard reference to prevent garbage collection
        int ref = luaL_ref(ese_ray_get_state(ray), LUA_REGISTRYINDEX);
        _ese_ray_set_lua_ref(ray, ref);
        _ese_ray_set_lua_ref_count(ray, 1);
    } else {
        // Already referenced - just increment count
        _ese_ray_set_lua_ref_count(ray, ese_ray_get_lua_ref_count(ray) + 1);
    }

    profile_count_add("ese_ray_ref_count");
}

void ese_ray_unref(EseRay *ray) {
    if (!ray)
        return;

    if (ese_ray_get_lua_ref(ray) != LUA_NOREF && ese_ray_get_lua_ref_count(ray) > 0) {
        int new_count = ese_ray_get_lua_ref_count(ray) - 1;
        _ese_ray_set_lua_ref_count(ray, new_count);

        if (new_count == 0) {
            // No more references - remove from registry
            luaL_unref(ese_ray_get_state(ray), LUA_REGISTRYINDEX, ese_ray_get_lua_ref(ray));
            _ese_ray_set_lua_ref(ray, LUA_NOREF);
        }
    }

    profile_count_add("ese_ray_unref_count");
}

// ========================================
// OPQUE ACCESSOR FUNCTIONS
// ========================================

size_t ese_ray_sizeof(void) { return sizeof(struct EseRay); }

/**
 * @brief Serializes an EseRay to a cJSON object.
 *
 * Creates a cJSON object representing the ray with type "RAY"
 * and x, y, dx, dy coordinates. Only serializes the
 * coordinate and direction data, not Lua-related fields.
 *
 * @param ray Pointer to the EseRay object to serialize
 * @return cJSON object representing the ray, or NULL on failure
 */
cJSON *ese_ray_serialize(const EseRay *ray) {
    log_assert("RAY", ray, "ese_ray_serialize called with NULL ray");

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        log_error("RAY", "Failed to create cJSON object for ray serialization");
        return NULL;
    }

    // Add type field
    cJSON *type = cJSON_CreateString("RAY");
    if (!type || !cJSON_AddItemToObject(json, "type", type)) {
        log_error("RAY", "Failed to add type field to ray serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add x coordinate (origin)
    cJSON *x = cJSON_CreateNumber((double)ese_ray_get_x(ray));
    if (!x || !cJSON_AddItemToObject(json, "x", x)) {
        log_error("RAY", "Failed to add x field to ray serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add y coordinate (origin)
    cJSON *y = cJSON_CreateNumber((double)ese_ray_get_y(ray));
    if (!y || !cJSON_AddItemToObject(json, "y", y)) {
        log_error("RAY", "Failed to add y field to ray serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add dx component (direction)
    cJSON *dx = cJSON_CreateNumber((double)ese_ray_get_dx(ray));
    if (!dx || !cJSON_AddItemToObject(json, "dx", dx)) {
        log_error("RAY", "Failed to add dx field to ray serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add dy component (direction)
    cJSON *dy = cJSON_CreateNumber((double)ese_ray_get_dy(ray));
    if (!dy || !cJSON_AddItemToObject(json, "dy", dy)) {
        log_error("RAY", "Failed to add dy field to ray serialization");
        cJSON_Delete(json);
        return NULL;
    }

    return json;
}

/**
 * @brief Deserializes an EseRay from a cJSON object.
 *
 * Creates a new EseRay from a cJSON object with type "RAY"
 * and x, y, dx, dy coordinates. The ray is created
 * with the specified engine and must be explicitly referenced with
 * ese_ray_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for ray creation
 * @param data cJSON object containing ray data
 * @return Pointer to newly created EseRay object, or NULL on failure
 */
EseRay *ese_ray_deserialize(EseLuaEngine *engine, const cJSON *data) {
    log_assert("RAY", data, "ese_ray_deserialize called with NULL data");

    if (!cJSON_IsObject(data)) {
        log_error("RAY", "Ray deserialization failed: data is not a JSON object");
        return NULL;
    }

    // Check type field
    cJSON *type_item = cJSON_GetObjectItem(data, "type");
    if (!type_item || !cJSON_IsString(type_item) || strcmp(type_item->valuestring, "RAY") != 0) {
        log_error("RAY", "Ray deserialization failed: invalid or missing type field");
        return NULL;
    }

    // Get x coordinate (origin)
    cJSON *x_item = cJSON_GetObjectItem(data, "x");
    if (!x_item || !cJSON_IsNumber(x_item)) {
        log_error("RAY", "Ray deserialization failed: invalid or missing x field");
        return NULL;
    }

    // Get y coordinate (origin)
    cJSON *y_item = cJSON_GetObjectItem(data, "y");
    if (!y_item || !cJSON_IsNumber(y_item)) {
        log_error("RAY", "Ray deserialization failed: invalid or missing y field");
        return NULL;
    }

    // Get dx component (direction)
    cJSON *dx_item = cJSON_GetObjectItem(data, "dx");
    if (!dx_item || !cJSON_IsNumber(dx_item)) {
        log_error("RAY", "Ray deserialization failed: invalid or missing dx field");
        return NULL;
    }

    // Get dy component (direction)
    cJSON *dy_item = cJSON_GetObjectItem(data, "dy");
    if (!dy_item || !cJSON_IsNumber(dy_item)) {
        log_error("RAY", "Ray deserialization failed: invalid or missing dy field");
        return NULL;
    }

    // Create new ray
    EseRay *ray = ese_ray_create(engine);
    ese_ray_set_x(ray, (float)x_item->valuedouble);
    ese_ray_set_y(ray, (float)y_item->valuedouble);
    ese_ray_set_dx(ray, (float)dx_item->valuedouble);
    ese_ray_set_dy(ray, (float)dy_item->valuedouble);

    return ray;
}

float ese_ray_get_x(const EseRay *ray) {
    log_assert("RAY", ray, "ese_ray_get_x called with NULL ray");
    return ray->x;
}

void ese_ray_set_x(EseRay *ray, float x) {
    log_assert("RAY", ray, "ese_ray_set_x called with NULL ray");
    ray->x = x;
}

float ese_ray_get_y(const EseRay *ray) {
    log_assert("RAY", ray, "ese_ray_get_y called with NULL ray");
    return ray->y;
}

void ese_ray_set_y(EseRay *ray, float y) {
    log_assert("RAY", ray, "ese_ray_set_y called with NULL ray");
    ray->y = y;
}

float ese_ray_get_dx(const EseRay *ray) {
    log_assert("RAY", ray, "ese_ray_get_dx called with NULL ray");
    return ray->dx;
}

void ese_ray_set_dx(EseRay *ray, float dx) {
    log_assert("RAY", ray, "ese_ray_set_dx called with NULL ray");
    ray->dx = dx;
}

float ese_ray_get_dy(const EseRay *ray) {
    log_assert("RAY", ray, "ese_ray_get_dy called with NULL ray");
    return ray->dy;
}

void ese_ray_set_dy(EseRay *ray, float dy) {
    log_assert("RAY", ray, "ese_ray_set_dy called with NULL ray");
    ray->dy = dy;
}

lua_State *ese_ray_get_state(const EseRay *ray) {
    log_assert("RAY", ray, "ese_ray_get_state called with NULL ray");
    return ray->state;
}

int ese_ray_get_lua_ref(const EseRay *ray) {
    log_assert("RAY", ray, "ray_get_lua_ref called with NULL ray");
    return ray->lua_ref;
}

int ese_ray_get_lua_ref_count(const EseRay *ray) {
    log_assert("RAY", ray, "ese_ray_get_lua_ref_count called with NULL ray");
    return ray->lua_ref_count;
}

// Mathematical operations
bool ese_ray_intersects_rect(const EseRay *ray, const EseRect *rect) {
    log_assert("RAY", ray, "ese_ray_intersects_rect called with NULL ray");
    log_assert("RAY", rect, "ese_ray_intersects_rect called with NULL rect");

    // Simple AABB intersection test for now
    // This could be enhanced with more sophisticated ray-rectangle intersection
    float t_near = -INFINITY;
    float t_far = INFINITY;

    float dx = ese_ray_get_dx(ray);
    float dy = ese_ray_get_dy(ray);
    float x = ese_ray_get_x(ray);
    float y = ese_ray_get_y(ray);

    if (dx != 0.0f) {
        float t1 = (ese_rect_get_x(rect) - x) / dx;
        float t2 = (ese_rect_get_x(rect) + ese_rect_get_width(rect) - x) / dx;
        if (t1 > t2) {
            float temp = t1;
            t1 = t2;
            t2 = temp;
        }
        if (t1 > t_near)
            t_near = t1;
        if (t2 < t_far)
            t_far = t2;
    } else if (x < ese_rect_get_x(rect) || x > ese_rect_get_x(rect) + ese_rect_get_width(rect)) {
        return false;
    }

    if (dy != 0.0f) {
        float t1 = (ese_rect_get_y(rect) - y) / dy;
        float t2 = (ese_rect_get_y(rect) + ese_rect_get_height(rect) - y) / dy;
        if (t1 > t2) {
            float temp = t1;
            t1 = t2;
            t2 = temp;
        }
        if (t1 > t_near)
            t_near = t1;
        if (t2 < t_far)
            t_far = t2;
    } else if (y < ese_rect_get_y(rect) || y > ese_rect_get_y(rect) + ese_rect_get_height(rect)) {
        return false;
    }

    if (t_near > t_far || t_far < 0.0f) {
        return false;
    }

    return true;
}

void ese_ray_get_point_at_distance(const EseRay *ray, float distance, float *out_x, float *out_y) {
    if (!ray || !out_x || !out_y)
        return;

    *out_x = ese_ray_get_x(ray) + ese_ray_get_dx(ray) * distance;
    *out_y = ese_ray_get_y(ray) + ese_ray_get_dy(ray) * distance;
}

void ese_ray_normalize(EseRay *ray) {
    if (!ray)
        return;

    float dx = ese_ray_get_dx(ray);
    float dy = ese_ray_get_dy(ray);
    float length = sqrtf(dx * dx + dy * dy);
    if (length > 0.0f) {
        ese_ray_set_dx(ray, dx / length);
        ese_ray_set_dy(ray, dy / length);
    }
}
