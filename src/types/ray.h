#ifndef ESE_RAY_H
#define ESE_RAY_H

#include <stdbool.h>

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseRect EseRect;

/**
 * @brief Represents a ray with floating-point origin and direction.
 * 
 * @details This structure stores a ray defined by an origin point and direction vector.
 */
typedef struct EseRay {
    float x;  /**< The x-coordinate of the ray's origin */
    float y;  /**< The y-coordinate of the ray's origin */
    float dx; /**< The x-component of the ray's direction vector */
    float dy; /**< The y-component of the ray's direction vector */
    lua_State *state; /**< Lua State this EseRay belongs to */
    int lua_ref; /**< Lua registry reference to its own proxy table */
} EseRay;

/**
 * @brief Initializes the EseRay userdata type in the Lua state.
 * 
 * @details Creates and registers the "RayProxyMeta" metatable with __index and 
 *          __newindex metamethods for property access. This allows EseRay objects
 *          to be used naturally from Lua with dot notation.
 * 
 * @param engine EseLuaEngine pointer where the EseRay type will be registered
 */
void ray_lua_init(EseLuaEngine *engine);

/**
 * @brief Creates a new EseRay object.
 * 
 * @details Allocates memory for a new EseRay and initializes to origin (0,0) with direction (1,0).
 * 
 * @param engine Pointer to a EseLuaEngine
 * @param c_only True if this object wont be accessable in LUA.
 * @return Pointer to newly created EseRay object
 * 
 * @warning The returned EseRay must be freed with ray_destroy() to prevent memory leaks
 */
EseRay *ray_create(EseLuaEngine *engine, bool c_only);

/**
 * @brief Copies a source EseRay into a new EseRay object.
 * 
 * @details This function creates a deep copy of an EseRay object. It allocates a new EseRay
 * struct and copies the numeric members.
 * 
 * @param source Pointer to the source EseRay to copy.
 * @param c_only True if the copied object wont be accessable in LUA.
 * @return A new, distinct EseRay object that is a copy of the source.
 * 
 * @warning The returned EseRay must be freed with ray_destroy() to prevent memory leaks.
 */
EseRay *ray_copy(const EseRay *source, bool c_only);

/**
 * @brief Destroys a EseRay object and frees its memory.
 * 
 * @details Frees the memory allocated by ray_create().
 * 
 * @param ray Pointer to the EseRay object to destroy
 */
void ray_destroy(EseRay *ray);

/**
 * @brief Extracts a EseRay pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EseRay pointer from the "__ptr" field of a Lua
 *          table that was created by _ray_lua_push(). Performs
 *          type checking to ensure the object is a valid EseRay proxy table
 *          with the correct metatable and userdata pointer.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseRay object
 * @return Pointer to the EseRay object, or NULL if extraction fails or type check fails
 * 
 * @warning Returns NULL for invalid objects - always check return value before use
 */
EseRay *ray_lua_get(lua_State *L, int idx);

/**
 * @brief Checks if the ray intersects with a rectangle.
 * 
 * @param ray Pointer to the EseRay object
 * @param rect Pointer to the EseRect object
 * @return true if ray intersects rectangle, false otherwise
 */
bool ray_intersects_rect(const EseRay *ray, const EseRect *rect);

/**
 * @brief Gets a point along the ray at a given distance from the origin.
 * 
 * @param ray Pointer to the EseRay object
 * @param distance Distance along the ray from origin
 * @param out_x Pointer to store the resulting x coordinate
 * @param out_y Pointer to store the resulting y coordinate
 */
void ray_get_point_at_distance(const EseRay *ray, float distance, float *out_x, float *out_y);

/**
 * @brief Normalizes the ray's direction vector.
 * 
 * @param ray Pointer to the EseRay object to normalize
 */
void ray_normalize(EseRay *ray);

#endif // ESE_RAY_H
