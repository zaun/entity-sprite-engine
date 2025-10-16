/**
 * @file point.c
 * @brief Implementation of 2D point type with floating-point coordinates
 * @details Implements point operations, watcher system, Lua integration, and JSON serialization
 * 
 * @copyright Copyright (c) 2024 ESE Project
 * @license See LICENSE.md for license information
 */

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/point.h"
#include "types/point_lua.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"

// The actual EsePoint struct definition (private to this file)
typedef struct EsePoint {
    float x;            /** The x-coordinate of the point */
    float y;            /** The y-coordinate of the point */

    lua_State *state;   /** Lua State this EsePoint belongs to */
    int lua_ref;        /** Lua registry reference to its own proxy table */
    int lua_ref_count;  /** Number of times this point has been referenced in C */
    
    // Watcher system
    EsePointWatcherCallback *watchers;     /** Array of watcher callbacks */
    void **watcher_userdata;               /** Array of userdata for each watcher */
    size_t watcher_count;                  /** Number of registered watchers */
    size_t watcher_capacity;               /** Capacity of the watcher arrays */
} EsePoint;

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers

// Watcher system

// Private static setters for Lua state management
static void _ese_point_set_lua_ref(EsePoint *point, int lua_ref);
static void _ese_point_set_lua_ref_count(EsePoint *point, int lua_ref_count);
static void _ese_point_set_state(EsePoint *point, lua_State *state);


// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EsePoint instance with default values
 * 
 * Allocates memory for a new EsePoint and initializes all fields to safe defaults.
 * The point starts at origin (0,0) with no Lua state or watchers.
 * 
 * @return Pointer to the newly created EsePoint, or NULL on allocation failure
 */
EsePoint *_ese_point_make() {
    EsePoint *point = (EsePoint *)memory_manager.malloc(sizeof(EsePoint), MMTAG_POINT);
    point->x = 0.0f;
    point->y = 0.0f;
    point->state = NULL;
    _ese_point_set_lua_ref(point, LUA_NOREF);
    _ese_point_set_lua_ref_count(point, 0);
    point->watchers = NULL;
    point->watcher_userdata = NULL;
    point->watcher_count = 0;
    point->watcher_capacity = 0;
    return point;
}

// Watcher system
/**
 * @brief Notifies all registered watchers of a point change
 * 
 * Iterates through all registered watcher callbacks and invokes them with the
 * updated point and their associated userdata. This is called whenever the
 * point's x or y coordinates are modified.
 * 
 * @param point Pointer to the EsePoint that has changed
 */
void _ese_point_make_point_notify_watchers(EsePoint *point) {
    if (!point || point->watcher_count == 0) return;
    
    for (size_t i = 0; i < point->watcher_count; i++) {
        if (point->watchers[i]) {
            point->watchers[i](point, point->watcher_userdata[i]);
        }
    }
}

// Private static setters for Lua state management
static void _ese_point_set_lua_ref(EsePoint *point, int lua_ref) {
    point->lua_ref = lua_ref;
}

static void _ese_point_set_lua_ref_count(EsePoint *point, int lua_ref_count) {
    point->lua_ref_count = lua_ref_count;
}

static void _ese_point_set_state(EsePoint *point, lua_State *state) {
    point->state = state;
}

void ese_point_set_state(EsePoint *point, lua_State *state) {
    log_assert("POINT", point, "ese_point_set_state called with NULL point");
    point->state = state;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EsePoint *ese_point_create(EseLuaEngine *engine) {
    log_assert("POINT", engine, "ese_point_create called with NULL engine");
    EsePoint *point = _ese_point_make();
    _ese_point_set_state(point, engine->runtime);
    return point;
}

EsePoint *ese_point_copy(const EsePoint *source) {
    log_assert("POINT", source, "ese_point_copy called with NULL source");
    
    EsePoint *copy = (EsePoint *)memory_manager.malloc(sizeof(EsePoint), MMTAG_POINT);
    copy->x = source->x;
    copy->y = source->y;
    copy->state = source->state;
    _ese_point_set_lua_ref(copy, LUA_NOREF);
    _ese_point_set_lua_ref_count(copy, 0);
    copy->watchers = NULL;
    copy->watcher_userdata = NULL;
    copy->watcher_count = 0;
    copy->watcher_capacity = 0;
    return copy;
}

void ese_point_destroy(EsePoint *point) {
    if (!point) return;
    
    if (ese_point_get_lua_ref(point) == LUA_NOREF) {
        // No Lua references, safe to free immediately
    
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

        memory_manager.free(point);
    } else {
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
        ese_point_unref(point);
    }
}

// Lua integration
void ese_point_lua_init(EseLuaEngine *engine) {
    log_assert("POINT", engine, "ese_point_lua_init called with NULL engine");
    
    _ese_point_lua_init(engine);
}

void ese_point_lua_push(EsePoint *point) {
    log_assert("POINT", point, "ese_point_lua_push called with NULL point");

    if (ese_point_get_lua_ref(point) == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EsePoint **ud = (EsePoint **)lua_newuserdata(ese_point_get_state(point), sizeof(EsePoint *));
        *ud = point;

        // Attach metatable
        luaL_getmetatable(ese_point_get_state(point), POINT_PROXY_META);
        lua_setmetatable(ese_point_get_state(point), -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(ese_point_get_state(point), LUA_REGISTRYINDEX, ese_point_get_lua_ref(point));
    }
}

EsePoint *ese_point_lua_get(lua_State *L, int idx) {
    log_assert("POINT", L, "ese_point_lua_get called with NULL Lua state");
    
    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EsePoint **ud = (EsePoint **)luaL_testudata(L, idx, POINT_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_point_ref(EsePoint *point) {
    log_assert("POINT", point, "ese_point_ref called with NULL point");
    
    if (ese_point_get_lua_ref(point) == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EsePoint **ud = (EsePoint **)lua_newuserdata(ese_point_get_state(point), sizeof(EsePoint *));
        *ud = point;

        // Attach metatable
        luaL_getmetatable(ese_point_get_state(point), POINT_PROXY_META);
        lua_setmetatable(ese_point_get_state(point), -2);

        // Store hard reference to prevent garbage collection
        _ese_point_set_lua_ref(point, luaL_ref(ese_point_get_state(point), LUA_REGISTRYINDEX));
        _ese_point_set_lua_ref_count(point, 1);
    } else {
        // Already referenced - just increment count
        _ese_point_set_lua_ref_count(point, ese_point_get_lua_ref_count(point) + 1);
    }

    profile_count_add("ese_point_ref_count");
}

void ese_point_unref(EsePoint *point) {
    if (!point) return;
    
    if (ese_point_get_lua_ref(point) != LUA_NOREF && ese_point_get_lua_ref_count(point) > 0) {
        _ese_point_set_lua_ref_count(point, ese_point_get_lua_ref_count(point) - 1);
        
        if (ese_point_get_lua_ref_count(point) == 0) {
            // No more references - remove from registry
            luaL_unref(ese_point_get_state(point), LUA_REGISTRYINDEX, ese_point_get_lua_ref(point));
            _ese_point_set_lua_ref(point, LUA_NOREF);
        }
    }

    profile_count_add("ese_point_unref_count");
}

// Mathematical operations
float ese_point_distance(const EsePoint *point1, const EsePoint *point2) {
    log_assert("POINT", point1, "ese_point_distance called with NULL first point");
    log_assert("POINT", point2, "ese_point_distance called with NULL second point");
    
    float dx = point2->x - point1->x;
    float dy = point2->y - point1->y;
    return sqrtf(dx * dx + dy * dy);
}

float ese_point_distance_squared(const EsePoint *point1, const EsePoint *point2) {
    log_assert("POINT", point1, "ese_point_distance_squared called with NULL first point");
    log_assert("POINT", point2, "ese_point_distance_squared called with NULL second point");
    
    float dx = point2->x - point1->x;
    float dy = point2->y - point1->y;
    return dx * dx + dy * dy;
}

// Property access
void ese_point_set_x(EsePoint *point, float x) {
    log_assert("POINT", point, "ese_point_set_x called with NULL point");
    point->x = x;
    _ese_point_make_point_notify_watchers(point);
}

float ese_point_get_x(const EsePoint *point) {
    log_assert("POINT", point, "ese_point_get_x called with NULL point");
    return point->x;
}

void ese_point_set_y(EsePoint *point, float y) {
    log_assert("POINT", point, "ese_point_set_y called with NULL point");
    point->y = y;
    _ese_point_make_point_notify_watchers(point);
}

float ese_point_get_y(const EsePoint *point) {
    log_assert("POINT", point, "ese_point_get_y called with NULL point");
    return point->y;
}

// Lua-related access
lua_State *ese_point_get_state(const EsePoint *point) {
    log_assert("POINT", point, "ese_point_get_state called with NULL point");
    return point->state;
}

int ese_point_get_lua_ref(const EsePoint *point) {
    log_assert("POINT", point, "ese_point_get_lua_ref called with NULL point");
    return point->lua_ref;
}

int ese_point_get_lua_ref_count(const EsePoint *point) {
    log_assert("POINT", point, "ese_point_get_lua_ref_count called with NULL point");
    return point->lua_ref_count;
}

// Watcher system
bool ese_point_add_watcher(EsePoint *point, EsePointWatcherCallback callback, void *userdata) {
    log_assert("POINT", point, "ese_point_add_watcher called with NULL point");
    log_assert("POINT", callback, "ese_point_add_watcher called with NULL callback");
    
    // Initialize watcher arrays if this is the first watcher
    if (point->watcher_count == 0) {
        point->watcher_capacity = 4; // Start with capacity for 4 watchers
        point->watchers = memory_manager.malloc(sizeof(EsePointWatcherCallback) * point->watcher_capacity, MMTAG_POINT);
        point->watcher_userdata = memory_manager.malloc(sizeof(void*) * point->watcher_capacity, MMTAG_POINT);
        point->watcher_count = 0;
    }
    
    // Expand arrays if needed
    if (point->watcher_count >= point->watcher_capacity) {
        size_t new_capacity = point->watcher_capacity * 2;
        EsePointWatcherCallback *new_watchers = memory_manager.realloc(
            point->watchers, 
            sizeof(EsePointWatcherCallback) * new_capacity, 
            MMTAG_POINT
        );
        void **new_userdata = memory_manager.realloc(
            point->watcher_userdata, 
            sizeof(void*) * new_capacity, 
            MMTAG_POINT
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

bool ese_point_remove_watcher(EsePoint *point, EsePointWatcherCallback callback, void *userdata) {
    log_assert("POINT", point, "ese_point_remove_watcher called with NULL point");
    log_assert("POINT", callback, "ese_point_remove_watcher called with NULL callback");
    
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

size_t ese_point_sizeof(void) {
    return sizeof(EsePoint);
}

/**
 * @brief Serializes an EsePoint to a cJSON object.
 *
 * Creates a cJSON object representing the point with type "POINT"
 * and x, y coordinates. Only serializes the coordinate data, not
 * Lua-related fields.
 *
 * @param point Pointer to the EsePoint object to serialize
 * @return cJSON object representing the point, or NULL on failure
 */
cJSON *ese_point_serialize(const EsePoint *point) {
    log_assert("POINT", point, "ese_point_serialize called with NULL point");

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        log_error("POINT", "Failed to create cJSON object for point serialization");
        return NULL;
    }

    // Add type field
    cJSON *type = cJSON_CreateString("POINT");
    if (!type || !cJSON_AddItemToObject(json, "type", type)) {
        log_error("POINT", "Failed to add type field to point serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add x coordinate
    cJSON *x = cJSON_CreateNumber((double)point->x);
    if (!x || !cJSON_AddItemToObject(json, "x", x)) {
        log_error("POINT", "Failed to add x field to point serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add y coordinate
    cJSON *y = cJSON_CreateNumber((double)point->y);
    if (!y || !cJSON_AddItemToObject(json, "y", y)) {
        log_error("POINT", "Failed to add y field to point serialization");
        cJSON_Delete(json);
        return NULL;
    }

    return json;
}

/**
 * @brief Deserializes an EsePoint from a cJSON object.
 *
 * Creates a new EsePoint from a cJSON object with type "POINT"
 * and x, y coordinates. The point is created with the specified engine
 * and must be explicitly referenced with ese_point_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for point creation
 * @param data cJSON object containing point data
 * @return Pointer to newly created EsePoint object, or NULL on failure
 */
EsePoint *ese_point_deserialize(EseLuaEngine *engine, const cJSON *data) {
    log_assert("POINT", data, "ese_point_deserialize called with NULL data");

    if (!cJSON_IsObject(data)) {
        log_error("POINT", "Point deserialization failed: data is not a JSON object");
        return NULL;
    }

    // Check type field
    cJSON *type_item = cJSON_GetObjectItem(data, "type");
    if (!type_item || !cJSON_IsString(type_item) || strcmp(type_item->valuestring, "POINT") != 0) {
        log_error("POINT", "Point deserialization failed: invalid or missing type field");
        return NULL;
    }

    // Get x coordinate
    cJSON *x_item = cJSON_GetObjectItem(data, "x");
    if (!x_item || !cJSON_IsNumber(x_item)) {
        log_error("POINT", "Point deserialization failed: invalid or missing x field");
        return NULL;
    }

    // Get y coordinate
    cJSON *y_item = cJSON_GetObjectItem(data, "y");
    if (!y_item || !cJSON_IsNumber(y_item)) {
        log_error("POINT", "Point deserialization failed: invalid or missing y field");
        return NULL;
    }

    // Create new point
    EsePoint *point = ese_point_create(engine);
    ese_point_set_x(point, (float)x_item->valuedouble);
    ese_point_set_y(point, (float)y_item->valuedouble);

    return point;
}
