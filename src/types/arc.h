#ifndef ESE_ARC_H
#define ESE_ARC_H

#include <stdbool.h>

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseRect EseRect;

/**
 * @brief Represents an arc with floating-point center, radius, and angle range.
 * 
 * @details This structure stores an arc defined by center point, radius, and start/end angles.
 */
typedef struct EseArc {
    float x;                /**< The x-coordinate of the arc's center */
    float y;                /**< The y-coordinate of the arc's center */
    float radius;           /**< The radius of the arc */
    float start_angle;      /**< The start angle of the arc in radians */
    float end_angle;        /**< The end angle of the arc in radians */

    EseLuaEngine *engine;   /**< The engine that owns this arc */
    int lua_ref;            /**< Lua registry reference to its own proxy table */
    int lua_ref_count;      /**< Number of times this arc has been referenced in C */
} EseArc;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
/**
 * @brief Creates a new EseArc object.
 * 
 * @details Allocates memory for a new EseArc and initializes to center (0,0), radius 1, full circle.
 *          The arc is created without Lua references and must be explicitly
 *          referenced with ese_arc_ref() if Lua access is desired.
 * 
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseArc object
 * 
 * @warning The returned EseArc must be freed with ese_arc_destroy() to prevent memory leaks
 */
EseArc *ese_arc_create(EseLuaEngine *engine);

/**
 * @brief Copies a source EseArc into a new EseArc object.
 * 
 * @details This function creates a deep copy of an EseArc object. It allocates a new EseArc
 *          struct and copies all numeric members. The copy is created without Lua references
 *          and must be explicitly referenced with ese_arc_ref() if Lua access is desired.
 * 
 * @param source Pointer to the source EseArc to copy.
 * @return A new, distinct EseArc object that is a copy of the source.
 * 
 * @warning The returned EseArc must be freed with ese_arc_destroy() to prevent memory leaks.
 */
EseArc *ese_arc_copy(const EseArc *source);

/**
 * @brief Destroys a EseArc object, managing memory based on Lua references.
 * 
 * @details If the arc has no Lua references (lua_ref == LUA_NOREF), frees memory immediately.
 *          If the arc has Lua references, decrements the reference counter.
 *          When the counter reaches 0, removes the Lua reference and lets
 *          Lua's garbage collector handle final cleanup.
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
 * @details Creates and registers the "ArcProxyMeta" metatable with __index, __newindex,
 *          __gc, __tostring metamethods for property access and garbage collection.
 *          This allows EseArc objects to be used naturally from Lua with dot notation.
 *          Also creates the global "Arc" table with "new" and "zero" constructors.
 * 
 * @param engine EseLuaEngine pointer where the EseArc type will be registered
 */
void ese_arc_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EseArc object to the Lua stack.
 * 
 * @details If the arc has no Lua references (lua_ref == LUA_NOREF), creates a new
 *          proxy table. If the arc has Lua references, retrieves the existing
 *          proxy table from the registry.
 * 
 * @param arc Pointer to the EseArc object to push to Lua
 */
void ese_arc_lua_push(EseLuaEngine *engine, EseArc *arc);

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
 * @return Pointer to the EseArc object, or NULL if extraction fails or type check fails
 * 
 * @warning Returns NULL for invalid objects - always check return value before use
 */
EseArc *ese_arc_lua_get(EseLuaEngine *engine, int idx);

/**
 * @brief References a EseArc object for Lua access with reference counting.
 * 
 * @details If arc->lua_ref is LUA_NOREF, pushes the arc to Lua and references it,
 *          setting lua_ref_count to 1. If arc->lua_ref is already set, increments
 *          the reference count by 1. This prevents the arc from being garbage
 *          collected while C code holds references to it.
 * 
 * @param arc Pointer to the EseArc object to reference
 */
void ese_arc_ref(EseLuaEngine *engine, EseArc *arc);

/**
 * @brief Unreferences a EseArc object, decrementing the reference count.
 * 
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua reference
 *          is removed from the registry. This function does NOT free memory.
 * 
 * @param arc Pointer to the EseArc object to unreference
 */
void ese_arc_unref(EseLuaEngine *engine, EseArc *arc);

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

#endif // ESE_ARC_H
