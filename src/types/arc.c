/**
 * @file arc.c
 * @brief Implementation of arc type with center, radius, and angle range
 * @details Implements arc operations, collision detection, Lua integration, and JSON serialization
 * 
 * @copyright Copyright (c) 2024 ESE Project
 * @license See LICENSE.md for license information
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/arc.h"
#include "types/arc_lua.h"
#include "types/types.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ========================================
// STRUCT DEFINITION
// ========================================

/**
 * @brief Represents an arc with floating-point center, radius, and angle range.
 *
 * @details This structure stores an arc defined by center point, radius, and start/end angles.
 */
struct EseArc {
    float x;           /** The x-coordinate of the arc's center */
    float y;           /** The y-coordinate of the arc's center */
    float radius;      /** The radius of the arc */
    float start_angle; /** The start angle of the arc in radians */
    float end_angle;   /** The end angle of the arc in radians */

    lua_State *state;  /** Lua State this EseArc belongs to */
    int lua_ref;       /** Lua registry reference to its own proxy table */
    int lua_ref_count; /** Number of times this arc has been referenced in C */
};

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseArc *_ese_arc_make(void);

// Private setters
static void _ese_arc_set_lua_ref(EseArc *arc, int lua_ref);
static void _ese_arc_set_lua_ref_count(EseArc *arc, int lua_ref_count);
static void _ese_arc_set_state(EseArc *arc, lua_State *state);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseArc instance with default values
 * 
 * Allocates memory for a new EseArc and initializes all fields to safe defaults.
 * The arc starts at origin (0,0) with unit radius, full circle angles, and no Lua state or references.
 * 
 * @return Pointer to the newly created EseArc, or NULL on allocation failure
 */
static EseArc *_ese_arc_make() {
    EseArc *arc = (EseArc *)memory_manager.malloc(sizeof(EseArc), MMTAG_ARC);
    arc->x = 0.0f;
    arc->y = 0.0f;
    arc->radius = 1.0f;
    arc->start_angle = 0.0f;
    arc->end_angle = 2.0f * M_PI;
    arc->state = NULL;
    arc->lua_ref = LUA_NOREF;
    arc->lua_ref_count = 0;
    return arc;
}




// ========================================
// ACCESSOR FUNCTIONS
// ========================================

/**
 * @brief Gets the x-coordinate of the arc's center
 *
 * @param arc Pointer to the EseArc object
 * @return The x-coordinate of the arc's center
 */
float ese_arc_get_x(const EseArc *arc) {
    return arc->x;
}

/**
 * @brief Sets the x-coordinate of the arc's center
 *
 * @param arc Pointer to the EseArc object
 * @param x The new x-coordinate value
 */
void ese_arc_set_x(EseArc *arc, float x) {
    log_assert("ARC", arc != NULL, "ese_arc_set_x: arc cannot be NULL");
    arc->x = x;
}

/**
 * @brief Gets the y-coordinate of the arc's center
 *
 * @param arc Pointer to the EseArc object
 * @return The y-coordinate of the arc's center
 */
float ese_arc_get_y(const EseArc *arc) {
    return arc->y;
}

/**
 * @brief Sets the y-coordinate of the arc's center
 *
 * @param arc Pointer to the EseArc object
 * @param y The new y-coordinate value
 */
void ese_arc_set_y(EseArc *arc, float y) {
    log_assert("ARC", arc != NULL, "ese_arc_set_y: arc cannot be NULL");
    arc->y = y;
}

/**
 * @brief Gets the radius of the arc
 *
 * @param arc Pointer to the EseArc object
 * @return The radius of the arc
 */
float ese_arc_get_radius(const EseArc *arc) {
    return arc->radius;
}

/**
 * @brief Sets the radius of the arc
 *
 * @param arc Pointer to the EseArc object
 * @param radius The new radius value
 */
void ese_arc_set_radius(EseArc *arc, float radius) {
    log_assert("ARC", arc != NULL, "ese_arc_set_radius: arc cannot be NULL");
    arc->radius = radius;
}

/**
 * @brief Gets the start angle of the arc in radians
 *
 * @param arc Pointer to the EseArc object
 * @return The start angle of the arc in radians
 */
float ese_arc_get_start_angle(const EseArc *arc) {
    return arc->start_angle;
}

/**
 * @brief Sets the start angle of the arc in radians
 *
 * @param arc Pointer to the EseArc object
 * @param start_angle The new start angle value in radians
 */
void ese_arc_set_start_angle(EseArc *arc, float start_angle) {
    log_assert("ARC", arc != NULL, "ese_arc_set_start_angle: arc cannot be NULL");
    arc->start_angle = start_angle;
}

/**
 * @brief Gets the end angle of the arc in radians
 *
 * @param arc Pointer to the EseArc object
 * @return The end angle of the arc in radians
 */
float ese_arc_get_end_angle(const EseArc *arc) {
    return arc->end_angle;
}

/**
 * @brief Sets the end angle of the arc in radians
 *
 * @param arc Pointer to the EseArc object
 * @param end_angle The new end angle value in radians
 */
void ese_arc_set_end_angle(EseArc *arc, float end_angle) {
    log_assert("ARC", arc != NULL, "ese_arc_set_end_angle: arc cannot be NULL");
    arc->end_angle = end_angle;
}

/**
 * @brief Gets the Lua state associated with the arc
 *
 * @param arc Pointer to the EseArc object
 * @return The Lua state associated with the arc
 */
lua_State *ese_arc_get_state(const EseArc *arc) {
    return arc->state;
}

/**
 * @brief Gets the Lua registry reference for the arc
 *
 * @param arc Pointer to the EseArc object
 * @return The Lua registry reference for the arc
 */
int ese_arc_get_lua_ref(const EseArc *arc) {
    return arc->lua_ref;
}

/**
 * @brief Gets the Lua reference count for the arc
 *
 * @param arc Pointer to the EseArc object
 * @return The Lua reference count for the arc
 */
int ese_arc_get_lua_ref_count(const EseArc *arc) {
    return arc->lua_ref_count;
}

/**
 * @brief Sets the Lua registry reference for the arc (private)
 *
 * @param arc Pointer to the EseArc object
 * @param lua_ref The new Lua registry reference value
 */
static void _ese_arc_set_lua_ref(EseArc *arc, int lua_ref) {
    log_assert("ARC", arc != NULL, "_ese_arc_set_lua_ref: arc cannot be NULL");
    arc->lua_ref = lua_ref;
}

/**
 * @brief Sets the Lua reference count for the arc (private)
 *
 * @param arc Pointer to the EseArc object
 * @param lua_ref_count The new Lua reference count value
 */
static void _ese_arc_set_lua_ref_count(EseArc *arc, int lua_ref_count) {
    log_assert("ARC", arc != NULL, "_ese_arc_set_lua_ref_count: arc cannot be NULL");
    arc->lua_ref_count = lua_ref_count;
}

/**
 * @brief Sets the Lua state associated with the arc (private)
 *
 * @param arc Pointer to the EseArc object
 * @param state The new Lua state value
 */
static void _ese_arc_set_state(EseArc *arc, lua_State *state) {
    log_assert("ARC", arc != NULL, "_ese_arc_set_state: arc cannot be NULL");
    arc->state = state;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseArc *ese_arc_create(EseLuaEngine *engine) {
    EseArc *arc = _ese_arc_make();
    _ese_arc_set_state(arc, engine->runtime);
    return arc;
}

EseArc *ese_arc_copy(const EseArc *source) {
    if (source == NULL) {
        return NULL;
    }

    EseArc *copy = (EseArc *)memory_manager.malloc(sizeof(EseArc), MMTAG_ARC);
    ese_arc_set_x(copy, ese_arc_get_x(source));
    ese_arc_set_y(copy, ese_arc_get_y(source));
    ese_arc_set_radius(copy, ese_arc_get_radius(source));
    ese_arc_set_start_angle(copy, ese_arc_get_start_angle(source));
    ese_arc_set_end_angle(copy, ese_arc_get_end_angle(source));
    _ese_arc_set_state(copy, ese_arc_get_state(source));
    _ese_arc_set_lua_ref(copy, LUA_NOREF);
    _ese_arc_set_lua_ref_count(copy, 0);
    return copy;
}

void ese_arc_destroy(EseArc *arc) {
    if (!arc) return;
    
    if (ese_arc_get_lua_ref(arc) == LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(arc);
    } else {
        ese_arc_unref(arc);
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

// Lua integration
void ese_arc_lua_init(EseLuaEngine *engine) {
    _ese_arc_lua_init(engine);
}

void ese_arc_lua_push(EseArc *arc) {
    log_assert("ARC", arc, "ese_arc_lua_push called with NULL arc");

    if (ese_arc_get_lua_ref(arc) == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseArc **ud = (EseArc **)lua_newuserdata(ese_arc_get_state(arc), sizeof(EseArc *));
        *ud = arc;

        // Attach metatable
        luaL_getmetatable(ese_arc_get_state(arc), ARC_META);
        lua_setmetatable(ese_arc_get_state(arc), -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(ese_arc_get_state(arc), LUA_REGISTRYINDEX, ese_arc_get_lua_ref(arc));
    }
}

EseArc *ese_arc_lua_get(lua_State *L, int idx) {
    log_assert("ARC", L, "ese_arc_lua_get called with NULL Lua state");
    
    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseArc **ud = (EseArc **)luaL_testudata(L, idx, ARC_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_arc_ref(EseArc *arc) {
    log_assert("ARC", arc, "ese_arc_ref called with NULL arc");
    
    if (ese_arc_get_lua_ref(arc) == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseArc **ud = (EseArc **)lua_newuserdata(ese_arc_get_state(arc), sizeof(EseArc *));
        *ud = arc;

        // Attach metatable
        luaL_getmetatable(ese_arc_get_state(arc), ARC_META);
        lua_setmetatable(ese_arc_get_state(arc), -2);

        // Store hard reference to prevent garbage collection
        int ref = luaL_ref(ese_arc_get_state(arc), LUA_REGISTRYINDEX);
        _ese_arc_set_lua_ref(arc, ref);
        _ese_arc_set_lua_ref_count(arc, 1);
    } else {
        // Already referenced - just increment count
        _ese_arc_set_lua_ref_count(arc, ese_arc_get_lua_ref_count(arc) + 1);
    }

    profile_count_add("ese_arc_ref_count");
}

void ese_arc_unref(EseArc *arc) {
    if (!arc) return;
    
    if (ese_arc_get_lua_ref(arc) != LUA_NOREF && ese_arc_get_lua_ref_count(arc) > 0) {
        _ese_arc_set_lua_ref_count(arc, ese_arc_get_lua_ref_count(arc) - 1);
        
        if (ese_arc_get_lua_ref_count(arc) == 0) {
            // No more references - remove from registry
            luaL_unref(ese_arc_get_state(arc), LUA_REGISTRYINDEX, ese_arc_get_lua_ref(arc));
            _ese_arc_set_lua_ref(arc, LUA_NOREF);
        }
    }

    profile_count_add("ese_arc_unref_count");
}

// Mathematical operations
bool ese_arc_contains_point(const EseArc *arc, float x, float y, float tolerance) {
    if (!arc) return false;
    
    // Calculate distance from point to arc center
    float dx = x - ese_arc_get_x(arc);
    float dy = y - ese_arc_get_y(arc);
    float distance = sqrtf(dx * dx + dy * dy);
    
    // Check if point is within radius tolerance
    if (fabsf(distance - ese_arc_get_radius(arc)) > tolerance) {
        return false;
    }
    
    // Check if point is within angle range
    float angle = atan2f(dy, dx);
    if (angle < 0) {
        angle += 2.0f * M_PI;
    }
    
    // Normalize start and end angles
    float start = ese_arc_get_start_angle(arc);
    float end = ese_arc_get_end_angle(arc);
    
    if (end < start) {
        end += 2.0f * M_PI;
    }
    
    return (angle >= start && angle <= end);
}

float ese_arc_get_length(const EseArc *arc) {
    if (!arc) return 0.0f;
    
    float angle_diff = ese_arc_get_end_angle(arc) - ese_arc_get_start_angle(arc);
    if (angle_diff < 0) {
        angle_diff += 2.0f * M_PI;
    }
    
    return ese_arc_get_radius(arc) * angle_diff;
}

bool ese_arc_get_point_at_angle(const EseArc *arc, float angle, float *out_x, float *out_y) {
    if (!arc || !out_x || !out_y) return false;
    
    // Normalize start and end angles
    float start = ese_arc_get_start_angle(arc);
    float end = ese_arc_get_end_angle(arc);
    
    if (end < start) {
        end += 2.0f * M_PI;
    }
    
    // Normalize input angle
    while (angle < start) {
        angle += 2.0f * M_PI;
    }
    
    if (angle > end) {
        return false;
    }
    
    *out_x = ese_arc_get_x(arc) + ese_arc_get_radius(arc) * cosf(angle);
    *out_y = ese_arc_get_y(arc) + ese_arc_get_radius(arc) * sinf(angle);
    
    return true;
}

bool ese_arc_intersects_rect(const EseArc *arc, const EseRect *rect) {
    if (!arc || !rect) return false;
    
    // Simple bounding box check for now
    // This could be enhanced with more sophisticated arc-rectangle intersection
    float ese_arc_left = ese_arc_get_x(arc) - ese_arc_get_radius(arc);
    float ese_arc_right = ese_arc_get_x(arc) + ese_arc_get_radius(arc);
    float ese_arc_top = ese_arc_get_y(arc) - ese_arc_get_radius(arc);
    float ese_arc_bottom = ese_arc_get_y(arc) + ese_arc_get_radius(arc);
    
    float rect_left = ese_rect_get_x(rect);
    float rect_right = ese_rect_get_x(rect) + ese_rect_get_width(rect);
    float rect_top = ese_rect_get_y(rect);
    float rect_bottom = ese_rect_get_y(rect) + ese_rect_get_height(rect);
    
    return !(ese_arc_right < rect_left || ese_arc_left > rect_right ||
             ese_arc_bottom < rect_top || ese_arc_top > rect_bottom);
}

/**
 * @brief Serializes an EseArc to a cJSON object.
 *
 * Creates a cJSON object representing the arc with type "ARC"
 * and x, y, radius, start_angle, end_angle coordinates. Only serializes the
 * geometric data, not Lua-related fields.
 *
 * @param arc Pointer to the EseArc object to serialize
 * @return cJSON object representing the arc, or NULL on failure
 */
cJSON *ese_arc_serialize(const EseArc *arc) {
    log_assert("ARC", arc, "ese_arc_serialize called with NULL arc");

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        log_error("ARC", "Failed to create cJSON object for arc serialization");
        return NULL;
    }

    // Add type field
    cJSON *type = cJSON_CreateString("ARC");
    if (!type || !cJSON_AddItemToObject(json, "type", type)) {
        log_error("ARC", "Failed to add type field to arc serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add x coordinate (center)
    cJSON *x = cJSON_CreateNumber((double)ese_arc_get_x(arc));
    if (!x || !cJSON_AddItemToObject(json, "x", x)) {
        log_error("ARC", "Failed to add x field to arc serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add y coordinate (center)
    cJSON *y = cJSON_CreateNumber((double)ese_arc_get_y(arc));
    if (!y || !cJSON_AddItemToObject(json, "y", y)) {
        log_error("ARC", "Failed to add y field to arc serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add radius
    cJSON *radius = cJSON_CreateNumber((double)ese_arc_get_radius(arc));
    if (!radius || !cJSON_AddItemToObject(json, "radius", radius)) {
        log_error("ARC", "Failed to add radius field to arc serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add start_angle
    cJSON *start_angle = cJSON_CreateNumber((double)ese_arc_get_start_angle(arc));
    if (!start_angle || !cJSON_AddItemToObject(json, "start_angle", start_angle)) {
        log_error("ARC", "Failed to add start_angle field to arc serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add end_angle
    cJSON *end_angle = cJSON_CreateNumber((double)ese_arc_get_end_angle(arc));
    if (!end_angle || !cJSON_AddItemToObject(json, "end_angle", end_angle)) {
        log_error("ARC", "Failed to add end_angle field to arc serialization");
        cJSON_Delete(json);
        return NULL;
    }

    return json;
}

/**
 * @brief Deserializes an EseArc from a cJSON object.
 *
 * Creates a new EseArc from a cJSON object with type "ARC"
 * and x, y, radius, start_angle, end_angle coordinates. The arc is created
 * with the specified engine and must be explicitly referenced with
 * ese_arc_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for arc creation
 * @param data cJSON object containing arc data
 * @return Pointer to newly created EseArc object, or NULL on failure
 */
EseArc *ese_arc_deserialize(EseLuaEngine *engine, const cJSON *data) {
    log_assert("ARC", data, "ese_arc_deserialize called with NULL data");

    if (!cJSON_IsObject(data)) {
        log_error("ARC", "Arc deserialization failed: data is not a JSON object");
        return NULL;
    }

    // Check type field
    cJSON *type_item = cJSON_GetObjectItem(data, "type");
    if (!type_item || !cJSON_IsString(type_item) || strcmp(type_item->valuestring, "ARC") != 0) {
        log_error("ARC", "Arc deserialization failed: invalid or missing type field");
        return NULL;
    }

    // Get x coordinate (center)
    cJSON *x_item = cJSON_GetObjectItem(data, "x");
    if (!x_item || !cJSON_IsNumber(x_item)) {
        log_error("ARC", "Arc deserialization failed: invalid or missing x field");
        return NULL;
    }

    // Get y coordinate (center)
    cJSON *y_item = cJSON_GetObjectItem(data, "y");
    if (!y_item || !cJSON_IsNumber(y_item)) {
        log_error("ARC", "Arc deserialization failed: invalid or missing y field");
        return NULL;
    }

    // Get radius
    cJSON *radius_item = cJSON_GetObjectItem(data, "radius");
    if (!radius_item || !cJSON_IsNumber(radius_item)) {
        log_error("ARC", "Arc deserialization failed: invalid or missing radius field");
        return NULL;
    }

    // Get start_angle
    cJSON *start_angle_item = cJSON_GetObjectItem(data, "start_angle");
    if (!start_angle_item || !cJSON_IsNumber(start_angle_item)) {
        log_error("ARC", "Arc deserialization failed: invalid or missing start_angle field");
        return NULL;
    }

    // Get end_angle
    cJSON *end_angle_item = cJSON_GetObjectItem(data, "end_angle");
    if (!end_angle_item || !cJSON_IsNumber(end_angle_item)) {
        log_error("ARC", "Arc deserialization failed: invalid or missing end_angle field");
        return NULL;
    }

    // Create new arc
    EseArc *arc = ese_arc_create(engine);
    ese_arc_set_x(arc, (float)x_item->valuedouble);
    ese_arc_set_y(arc, (float)y_item->valuedouble);
    ese_arc_set_radius(arc, (float)radius_item->valuedouble);
    ese_arc_set_start_angle(arc, (float)start_angle_item->valuedouble);
    ese_arc_set_end_angle(arc, (float)end_angle_item->valuedouble);

    return arc;
}
