/**
 * @file arc.h
 * @brief Arc type with center, radius, and angle range
 * @details Provides arc operations, collision detection, and JSON serialization
 *
 * @copyright Copyright (c) 2024 ESE Project
 * @license See LICENSE.md for license information
 */

#ifndef ESE_ARC_H
#define ESE_ARC_H

#include "vendor/json/cJSON.h"
#include <stdbool.h>
#include <stddef.h>

// ========================================
// DEFINES AND STRUCTS
// ========================================

#define ARC_META "ArcMeta"

/**
 * @brief Represents an arc with floating-point center, radius, and angle range.
 *
 * @details This structure stores an arc defined by center point, radius, and
 * start/end angles.
 */
typedef struct EseArc EseArc;

// ========================================
// FORWARD DECLARATIONS
// ========================================

typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseRect EseRect;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Property accessors
/**
 * @brief Gets the x-coordinate of the arc's center
 *
 * @param arc Pointer to the EseArc object
 * @return The x-coordinate of the arc's center
 */
float ese_arc_get_x(const EseArc *arc);

/**
 * @brief Sets the x-coordinate of the arc's center
 *
 * @param arc Pointer to the EseArc object
 * @param x The new x-coordinate value
 */
void ese_arc_set_x(EseArc *arc, float x);

/**
 * @brief Gets the y-coordinate of the arc's center
 *
 * @param arc Pointer to the EseArc object
 * @return The y-coordinate of the arc's center
 */
float ese_arc_get_y(const EseArc *arc);

/**
 * @brief Sets the y-coordinate of the arc's center
 *
 * @param arc Pointer to the EseArc object
 * @param y The new y-coordinate value
 */
void ese_arc_set_y(EseArc *arc, float y);

/**
 * @brief Gets the radius of the arc
 *
 * @param arc Pointer to the EseArc object
 * @return The radius of the arc
 */
float ese_arc_get_radius(const EseArc *arc);

/**
 * @brief Sets the radius of the arc
 *
 * @param arc Pointer to the EseArc object
 * @param radius The new radius value
 */
void ese_arc_set_radius(EseArc *arc, float radius);

/**
 * @brief Gets the start angle of the arc in radians
 *
 * @param arc Pointer to the EseArc object
 * @return The start angle of the arc in radians
 */
float ese_arc_get_start_angle(const EseArc *arc);

/**
 * @brief Sets the start angle of the arc in radians
 *
 * @param arc Pointer to the EseArc object
 * @param start_angle The new start angle value in radians
 */
void ese_arc_set_start_angle(EseArc *arc, float start_angle);

/**
 * @brief Gets the end angle of the arc in radians
 *
 * @param arc Pointer to the EseArc object
 * @return The end angle of the arc in radians
 */
float ese_arc_get_end_angle(const EseArc *arc);

/**
 * @brief Sets the end angle of the arc in radians
 *
 * @param arc Pointer to the EseArc object
 * @param end_angle The new end angle value in radians
 */
void ese_arc_set_end_angle(EseArc *arc, float end_angle);

/**
 * @brief Gets the Lua state associated with the arc
 *
 * @param arc Pointer to the EseArc object
 * @return The Lua state associated with the arc
 */
lua_State *ese_arc_get_state(const EseArc *arc);

/**
 * @brief Gets the Lua registry reference for the arc
 *
 * @param arc Pointer to the EseArc object
 * @return The Lua registry reference for the arc
 */
int ese_arc_get_lua_ref(const EseArc *arc);

/**
 * @brief Gets the Lua reference count for the arc
 *
 * @param arc Pointer to the EseArc object
 * @return The Lua reference count for the arc
 */
int ese_arc_get_lua_ref_count(const EseArc *arc);

// Core lifecycle
/**
 * @brief Creates a new EseArc object.
 *
 * @details Allocates memory for a new EseArc and initializes to center (0,0),
 * radius 1, full circle. The arc is created without Lua references and must be
 * explicitly referenced with ese_arc_ref() if Lua access is desired.
 *
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseArc object
 *
 * @warning The returned EseArc must be freed with ese_arc_destroy() to prevent
 * memory leaks
 */
EseArc *ese_arc_create(EseLuaEngine *engine);

/**
 * @brief Copies a source EseArc into a new EseArc object.
 *
 * @details This function creates a deep copy of an EseArc object. It allocates
 * a new EseArc struct and copies all numeric members. The copy is created
 * without Lua references and must be explicitly referenced with ese_arc_ref()
 * if Lua access is desired.
 *
 * @param source Pointer to the source EseArc to copy.
 * @return A new, distinct EseArc object that is a copy of the source.
 *
 * @warning The returned EseArc must be freed with ese_arc_destroy() to prevent
 * memory leaks.
 */
EseArc *ese_arc_copy(const EseArc *source);

/**
 * @brief Destroys a EseArc object, managing memory based on Lua references.
 *
 * @details If the arc has no Lua references (lua_ref == LUA_NOREF), frees
 * memory immediately. If the arc has Lua references, decrements the reference
 * counter. When the counter reaches 0, removes the Lua reference and lets Lua's
 * garbage collector handle final cleanup.
 *
 * @note If the arc is Lua-owned, memory may not be freed immediately.
 *       Lua's garbage collector will finalize it once no references remain.
 *
 * @param arc Pointer to the EseArc object to destroy
 */
void ese_arc_destroy(EseArc *arc);

// Lua integration
/**
 * @brief Initializes the EseArc userdata type in the Lua state.
 *
 * @details Creates and registers the "ArcProxyMeta" metatable with __index,
 * __newindex,
 *          __gc, __tostring metamethods for property access and garbage
 * collection. This allows EseArc objects to be used naturally from Lua with dot
 * notation. Also creates the global "Arc" table with "new" and "zero"
 * constructors.
 *
 * @param engine EseLuaEngine pointer where the EseArc type will be registered
 */
void ese_arc_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EseArc object to the Lua stack.
 *
 * @details If the arc has no Lua references (lua_ref == LUA_NOREF), creates a
 * new proxy table. If the arc has Lua references, retrieves the existing proxy
 * table from the registry.
 *
 * @param arc Pointer to the EseArc object to push to Lua
 */
void ese_arc_lua_push(EseArc *arc);

/**
 * @brief Extracts a EseArc pointer from a Lua userdata object with type safety.
 *
 * @details Retrieves the C EseArc pointer from the "__ptr" field of a Lua
 *          table that was created by ese_arc_lua_push(). Performs
 *          type checking to ensure the object is a valid EseArc proxy table
 *          with the correct metatable and userdata pointer.
 *
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseArc object
 * @return Pointer to the EseArc object, or NULL if extraction fails or type
 * check fails
 *
 * @warning Returns NULL for invalid objects - always check return value before
 * use
 */
EseArc *ese_arc_lua_get(lua_State *L, int idx);

/**
 * @brief References a EseArc object for Lua access with reference counting.
 *
 * @details If arc->lua_ref is LUA_NOREF, pushes the arc to Lua and references
 * it, setting lua_ref_count to 1. If arc->lua_ref is already set, increments
 *          the reference count by 1. This prevents the arc from being garbage
 *          collected while C code holds references to it.
 *
 * @param arc Pointer to the EseArc object to reference
 */
void ese_arc_ref(EseArc *arc);

/**
 * @brief Unreferences a EseArc object, decrementing the reference count.
 *
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua
 * reference is removed from the registry. This function does NOT free memory.
 *
 * @param arc Pointer to the EseArc object to unreference
 */
void ese_arc_unref(EseArc *arc);

// Mathematical operations
/**
 * @brief Checks if a point is on the arc.
 *
 * @param arc Pointer to the EseArc object
 * @param x X coordinate of the point
 * @param y Y coordinate of the point
 * @param tolerance Distance tolerance for point-on-arc check
 * @return true if point is on the arc within tolerance, false otherwise
 */
bool ese_arc_contains_point(const EseArc *arc, float x, float y, float tolerance);

/**
 * @brief Gets the length of the arc.
 *
 * @param arc Pointer to the EseArc object
 * @return The arc length
 */
float ese_arc_get_length(const EseArc *arc);

/**
 * @brief Gets a point on the arc at a specific angle.
 *
 * @param arc Pointer to the EseArc object
 * @param angle The angle in radians
 * @param out_x Pointer to store the resulting x coordinate
 * @param out_y Pointer to store the resulting y coordinate
 * @return true if angle is within arc range, false otherwise
 */
bool ese_arc_get_point_at_angle(const EseArc *arc, float angle, float *out_x, float *out_y);

/**
 * @brief Checks if the arc intersects with a rectangle.
 *
 * @param arc Pointer to the EseArc object
 * @param rect Pointer to the EseRect object
 * @return true if arc intersects rectangle, false otherwise
 */
bool ese_arc_intersects_rect(const EseArc *arc, const EseRect *rect);

/**
 * @brief Serializes an EseArc to a cJSON object.
 *
 * @details Creates a cJSON object representing the arc with type "ARC"
 *          and x, y, radius, start_angle, end_angle coordinates. Only
 * serializes the geometric data, not Lua-related fields.
 *
 * @param arc Pointer to the EseArc object to serialize
 * @return cJSON object representing the arc, or NULL on failure
 *
 * @warning The caller is responsible for calling cJSON_Delete() on the returned
 * object
 */
cJSON *ese_arc_serialize(const EseArc *arc);

/**
 * @brief Deserializes an EseArc from a cJSON object.
 *
 * @details Creates a new EseArc from a cJSON object with type "ARC"
 *          and x, y, radius, start_angle, end_angle coordinates. The arc is
 * created with the specified engine and must be explicitly referenced with
 *          ese_arc_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for arc creation
 * @param data cJSON object containing arc data
 * @return Pointer to newly created EseArc object, or NULL on failure
 *
 * @warning The returned EseArc must be freed with ese_arc_destroy() to prevent
 * memory leaks
 */
EseArc *ese_arc_deserialize(EseLuaEngine *engine, const cJSON *data);

#endif // ESE_ARC_H
