#ifndef ESE_POINT_H
#define ESE_POINT_H

#include <stdint.h>

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Represents a 2D point with floating-point coordinates.
 * 
 * @details This structure stores the x and y coordinates of a point in 2D space.
 */
typedef struct EsePoint {
    float x;            /**< The x-coordinate of the point */
    float y;            /**< The y-coordinate of the point */
    lua_State *state;   /**< Lua State this EsePoint belongs to */
    int lua_ref;        /**< Lua registry reference */
} EsePoint;

/**
 * @brief Initializes the EsePoint userdata type in the Lua state.
 * 
 * @details Creates and registers the "PointProxyMeta" metatable with __index and 
 *          __newindex metamethods for property access. This allows EsePoint objects
 *          to be used naturally from Lua with dot notation (point.x, point.y).
 * 
 * @param engine EseLuaEngine pointer where the EsePoint type will be registered
 */
void point_lua_init(EseLuaEngine *engine);

/**
 * @brief Creates a new EsePoint object.
 * 
 * @details Allocates memory for a new EsePoint and initializes to (0, 0).
 * 
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EsePoint object
 * 
 * @warning The returned EsePoint must be freed with point_destroy() to prevent memory leaks
 */
EsePoint *point_create(EseLuaEngine *engine);

/**
 * @brief Destroys a EsePoint object and frees its memory.
 * 
 * @details Frees the memory allocated by point_create().
 * 
 * @param point Pointer to the EsePoint object to destroy
 */
void point_destroy(EsePoint *point);

/**
 * @brief Extracts a EsePoint pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EsePoint pointer from the "__ptr" field of a Lua
 *          table that was created by _point_lua_push(). Performs
 *          type checking to ensure the object is a valid EsePoint proxy table
 *          with the correct metatable and userdata pointer.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EsePoint object
 * @return Pointer to the EsePoint object, or NULL if extraction fails or type check fails
 * 
 * @warning Returns NULL for invalid objects - always check return value before use
 */
EsePoint *point_lua_get(lua_State *L, int idx);

#endif // ESE_POINT_H
