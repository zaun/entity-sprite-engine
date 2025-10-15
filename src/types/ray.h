#ifndef ESE_RAY_H
#define ESE_RAY_H

#include <stdbool.h>
#include "vendor/json/cJSON.h"

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseRect EseRect;

#define RAY_PROXY_META "RayProxyMeta"

/**
 * @brief Opaque ray structure.
 * 
 * @details This structure stores a ray defined by an origin point and direction vector.
 *          The internal structure is hidden from the public API.
 */
typedef struct EseRay EseRay;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
/**
 * @brief Creates a new EseRay object.
 * 
 * @details Allocates memory for a new EseRay and initializes to origin (0,0) with direction (1,0).
 *          The ray is created without Lua references and must be explicitly
 *          referenced with ese_ray_ref() if Lua access is desired.
 * 
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseRay object
 * 
 * @warning The returned EseRay must be freed with ese_ray_destroy() to prevent memory leaks
 */
EseRay *ese_ray_create(EseLuaEngine *engine);

/**
 * @brief Copies a source EseRay into a new EseRay object.
 * 
 * @details This function creates a deep copy of an EseRay object. It allocates a new EseRay
 *          struct and copies all numeric members. The copy is created without Lua references
 *          and must be explicitly referenced with ese_ray_ref() if Lua access is desired.
 * 
 * @param source Pointer to the source EseRay to copy.
 * @return A new, distinct EseRay object that is a copy of the source.
 * 
 * @warning The returned EseRay must be freed with ese_ray_destroy() to prevent memory leaks.
 */
EseRay *ese_ray_copy(const EseRay *source);

/**
 * @brief Destroys a EseRay object, managing memory based on Lua references.
 * 
 * @details If the ray has no Lua references (lua_ref == LUA_NOREF), frees memory immediately.
 *          If the ray has Lua references, decrements the reference counter.
 *          When the counter reaches 0, removes the Lua reference and lets
 *          Lua's garbage collector handle final cleanup.
 * 
 * @note If the ray is Lua-owned, memory may not be freed immediately.
 *       Lua's garbage collector will finalize it once no references remain.
 * 
 * @param ray Pointer to the EseRay object to destroy
 */
void ese_ray_destroy(EseRay *ray);

// Property accessors
/**
 * @brief Gets the x-coordinate of the ray's origin.
 * 
 * @param ray Pointer to the EseRay object
 * @return The x-coordinate of the ray's origin
 */
float ese_ray_get_x(const EseRay *ray);

/**
 * @brief Sets the x-coordinate of the ray's origin.
 * 
 * @param ray Pointer to the EseRay object
 * @param x The new x-coordinate value
 */
void ese_ray_set_x(EseRay *ray, float x);

/**
 * @brief Gets the y-coordinate of the ray's origin.
 * 
 * @param ray Pointer to the EseRay object
 * @return The y-coordinate of the ray's origin
 */
float ese_ray_get_y(const EseRay *ray);

/**
 * @brief Sets the y-coordinate of the ray's origin.
 * 
 * @param ray Pointer to the EseRay object
 * @param y The new y-coordinate value
 */
void ese_ray_set_y(EseRay *ray, float y);

/**
 * @brief Gets the x-component of the ray's direction vector.
 * 
 * @param ray Pointer to the EseRay object
 * @return The x-component of the ray's direction vector
 */
float ese_ray_get_dx(const EseRay *ray);

/**
 * @brief Sets the x-component of the ray's direction vector.
 * 
 * @param ray Pointer to the EseRay object
 * @param dx The new x-component value
 */
void ese_ray_set_dx(EseRay *ray, float dx);

/**
 * @brief Gets the y-component of the ray's direction vector.
 * 
 * @param ray Pointer to the EseRay object
 * @return The y-component of the ray's direction vector
 */
float ese_ray_get_dy(const EseRay *ray);

/**
 * @brief Sets the y-component of the ray's direction vector.
 * 
 * @param ray Pointer to the EseRay object
 * @param dy The new y-component value
 */
void ese_ray_set_dy(EseRay *ray, float dy);

/**
 * @brief Gets the Lua state associated with the ray.
 * 
 * @param ray Pointer to the EseRay object
 * @return The Lua state pointer
 */
lua_State *ese_ray_get_state(const EseRay *ray);

/**
 * @brief Gets the Lua reference count for the ray.
 * 
 * @param ray Pointer to the EseRay object
 * @return The current reference count
 */
int ese_ray_get_lua_ref_count(const EseRay *ray);

/**
 * @brief Gets the Lua reference for the ray.
 * 
 * @param ray Pointer to the EseRay object
 * @return The Lua reference value
 */
int ese_ray_get_lua_ref(const EseRay *ray);

/**
 * @brief Gets the size of the EseRay structure.
 * 
 * @return The size in bytes of the EseRay structure
 */
size_t ese_ray_sizeof(void);

// Lua integration
/**
 * @brief Initializes the EseRay userdata type in the Lua state.
 * 
 * @details Creates and registers the "RayProxyMeta" metatable with __index, __newindex,
 *          __gc, __tostring metamethods for property access and garbage collection.
 *          This allows EseRay objects to be used naturally from Lua with dot notation.
 *          Also creates the global "Ray" table with "new" and "zero" constructors.
 * 
 * @param engine EseLuaEngine pointer where the EseRay type will be registered
 */
void ese_ray_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EseRay object to the Lua stack.
 * 
 * @details If the ray has no Lua references (lua_ref == LUA_NOREF), creates a new
 *          proxy table. If the ray has Lua references, retrieves the existing
 *          proxy table from the registry.
 * 
 * @param ray Pointer to the EseRay object to push to Lua
 */
void ese_ray_lua_push(EseRay *ray);

/**
 * @brief Extracts a EseRay pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EseRay pointer from the "__ptr" field of a Lua
 *          table that was created by ese_ray_lua_push(). Performs
 *          type checking to ensure the object is a valid EseRay proxy table
 *          with the correct metatable and userdata pointer.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseRay object
 * @return Pointer to the EseRay object, or NULL if extraction fails or type check fails
 * 
 * @warning Returns NULL for invalid objects - always check return value before use
 */
EseRay *ese_ray_lua_get(lua_State *L, int idx);

/**
 * @brief References a EseRay object for Lua access with reference counting.
 * 
 * @details If ray->lua_ref is LUA_NOREF, pushes the ray to Lua and references it,
 *          setting lua_ref_count to 1. If ray->lua_ref is already set, increments
 *          the reference count by 1. This prevents the ray from being garbage
 *          collected while C code holds references to it.
 * 
 * @param ray Pointer to the EseRay object to reference
 */
void ese_ray_ref(EseRay *ray);

/**
 * @brief Unreferences a EseRay object, decrementing the reference count.
 * 
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua reference
 *          is removed from the registry. This function does NOT free memory.
 * 
 * @param ray Pointer to the EseRay object to unreference
 */
void ese_ray_unref(EseRay *ray);

// Mathematical operations
/**
 * @brief Checks if the ray intersects with a rectangle.
 * 
 * @param ray Pointer to the EseRay object
 * @param rect Pointer to the EseRect object
 * @return true if ray intersects rectangle, false otherwise
 */
bool ese_ray_intersects_rect(const EseRay *ray, const EseRect *rect);

/**
 * @brief Gets a point along the ray at a given distance from the origin.
 * 
 * @param ray Pointer to the EseRay object
 * @param distance Distance along the ray from origin
 * @param out_x Pointer to store the resulting x coordinate
 * @param out_y Pointer to store the resulting y coordinate
 */
void ese_ray_get_point_at_distance(const EseRay *ray, float distance, float *out_x, float *out_y);

/**
 * @brief Normalizes the ray's direction vector.
 * 
 * @param ray Pointer to the EseRay object to normalize
 */
void ese_ray_normalize(EseRay *ray);

/**
 * @brief Serializes an EseRay to a cJSON object.
 *
 * @details Creates a cJSON object representing the ray with type "RAY"
 *          and x, y, dx, dy coordinates. Only serializes the
 *          coordinate and direction data, not Lua-related fields.
 *
 * @param ray Pointer to the EseRay object to serialize
 * @return cJSON object representing the ray, or NULL on failure
 *
 * @warning The caller is responsible for calling cJSON_Delete() on the returned object
 */
cJSON *ese_ray_serialize(const EseRay *ray);

/**
 * @brief Deserializes an EseRay from a cJSON object.
 *
 * @details Creates a new EseRay from a cJSON object with type "RAY"
 *          and x, y, dx, dy coordinates. The ray is created
 *          with the specified engine and must be explicitly referenced with
 *          ese_ray_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for ray creation
 * @param data cJSON object containing ray data
 * @return Pointer to newly created EseRay object, or NULL on failure
 *
 * @warning The returned EseRay must be freed with ese_ray_destroy() to prevent memory leaks
 */
EseRay *ese_ray_deserialize(EseLuaEngine *engine, const cJSON *data);

#endif // ESE_RAY_H
