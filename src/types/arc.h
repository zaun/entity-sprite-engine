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
    float x;           /**< The x-coordinate of the arc's center */
    float y;           /**< The y-coordinate of the arc's center */
    float radius;      /**< The radius of the arc */
    float start_angle; /**< The start angle of the arc in radians */
    float end_angle;   /**< The end angle of the arc in radians */
    lua_State *state; /**< Lua State this EseArc belongs to */
    int lua_ref; /**< Lua registry reference to its own proxy table */
} EseArc;

/**
 * @brief Initializes the EseArc userdata type in the Lua state.
 * 
 * @details Creates and registers the "ArcProxyMeta" metatable with __index and 
 *          __newindex metamethods for property access. This allows EseArc objects
 *          to be used naturally from Lua with dot notation.
 * 
 * @param engine EseLuaEngine pointer where the EseArc type will be registered
 */
void arc_lua_init(EseLuaEngine *engine);

/**
 * @brief Creates a new EseArc object.
 * 
 * @details Allocates memory for a new EseArc and initializes to center (0,0), radius 1, full circle.
 * 
 * @param engine Pointer to a EseLuaEngine
 * @param c_only True if this object wont be accessable in LUA.
 * @return Pointer to newly created EseArc object
 * 
 * @warning The returned EseArc must be freed with arc_destroy() to prevent memory leaks
 */
EseArc *arc_create(EseLuaEngine *engine, bool c_only);

/**
 * @brief Copies a source EseArc into a new EseArc object.
 * 
 * @details This function creates a deep copy of an EseArc object. It allocates a new EseArc
 * struct and copies the numeric members.
 * 
 * @param source Pointer to the source EseArc to copy.
 * @param c_only True if the copied object wont be accessable in LUA.
 * @return A new, distinct EseArc object that is a copy of the source.
 * 
 * @warning The returned EseArc must be freed with arc_destroy() to prevent memory leaks.
 */
EseArc *arc_copy(const EseArc *source, bool c_only);

/**
 * @brief Destroys a EseArc object and frees its memory.
 * 
 * @details Frees the memory allocated by arc_create().
 * 
 * @param arc Pointer to the EseArc object to destroy
 */
void arc_destroy(EseArc *arc);

/**
 * @brief Extracts a EseArc pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EseArc pointer from the "__ptr" field of a Lua
 *          table that was created by _arc_lua_push(). Performs
 *          type checking to ensure the object is a valid EseArc proxy table
 *          with the correct metatable and userdata pointer.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseArc object
 * @return Pointer to the EseArc object, or NULL if extraction fails or type check fails
 * 
 * @warning Returns NULL for invalid objects - always check return value before use
 */
EseArc *arc_lua_get(lua_State *L, int idx);

/**
 * @brief Checks if a point is on the arc.
 * 
 * @param arc Pointer to the EseArc object
 * @param x X coordinate of the point
 * @param y Y coordinate of the point
 * @param tolerance Distance tolerance for point-on-arc check
 * @return true if point is on the arc within tolerance, false otherwise
 */
bool arc_contains_point(const EseArc *arc, float x, float y, float tolerance);

/**
 * @brief Gets the length of the arc.
 * 
 * @param arc Pointer to the EseArc object
 * @return The arc length
 */
float arc_get_length(const EseArc *arc);

/**
 * @brief Gets a point on the arc at a specific angle.
 * 
 * @param arc Pointer to the EseArc object
 * @param angle The angle in radians
 * @param out_x Pointer to store the resulting x coordinate
 * @param out_y Pointer to store the resulting y coordinate
 * @return true if angle is within arc range, false otherwise
 */
bool arc_get_point_at_angle(const EseArc *arc, float angle, float *out_x, float *out_y);

/**
 * @brief Checks if the arc intersects with a rectangle.
 * 
 * @param arc Pointer to the EseArc object
 * @param rect Pointer to the EseRect object
 * @return true if arc intersects rectangle, false otherwise
 */
bool arc_intersects_rect(const EseArc *arc, const EseRect *rect);

#endif // ESE_ARC_H
