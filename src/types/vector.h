#ifndef ESE_VECTOR_H
#define ESE_VECTOR_H

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Represents a 2D vector with floating-point components.
 * 
 * @details This structure stores the x and y components of a vector in 2D space.
 */
typedef struct EseVector {
    float x; /**< The x-component of the vector */
    float y; /**< The y-component of the vector */
    lua_State *state; /**< Lua State this EseVector belongs to */
    int lua_ref; /**< Lua registry reference to its own proxy table */
} EseVector;

/**
 * @brief Initializes the EseVector userdata type in the Lua state.
 * 
 * @details Creates and registers the "VectorProxyMeta" metatable with __index and 
 *          __newindex metamethods for property access. This allows EseVector objects
 *          to be used naturally from Lua with dot notation (vector.x, vector.y).
 * 
 * @param engine EseLuaEngine pointer where the EseVector type will be registered
 */
void vector_lua_init(EseLuaEngine *engine);

/**
 * @brief Creates a new EseVector object.
 * 
 * @details Allocates memory for a new EseVector and initializes to (0, 0).
 * 
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseVector object
 * 
 * @warning The returned EseVector must be freed with vector_destroy() to prevent memory leaks
 */
EseVector *vector_create(EseLuaEngine *engine);

/**
 * @brief Destroys a EseVector object and frees its memory.
 * 
 * @details Frees the memory allocated by vector_create().
 * 
 * @param vector Pointer to the EseVector object to destroy
 */
void vector_destroy(EseVector *vector);

/**
 * @brief Extracts a EseVector pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EseVector pointer from the "__ptr" field of a Lua
 *          table that was created by _vector_lua_push(). Performs
 *          type checking to ensure the object is a valid EseVector proxy table
 *          with the correct metatable and userdata pointer.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseVector object
 * @return Pointer to the EseVector object, or NULL if extraction fails or type check fails
 * 
 * @warning Returns NULL for invalid objects - always check return value before use
 */
EseVector *vector_lua_get(lua_State *L, int idx);

/**
 * @brief Sets vector to point in a cardinal or ordinal direction with given magnitude.
 * 
 * @param vector Pointer to the EseVector object
 * @param direction Direction character: 'n', 's', 'e', 'w', or combinations like "nw"
 * @param magnitude Length of the resulting vector
 */
void vector_set_direction(EseVector *vector, const char *direction, float magnitude);

/**
 * @brief Calculates the magnitude (length) of the vector.
 * 
 * @param vector Pointer to the EseVector object
 * @return The magnitude of the vector
 */
float vector_magnitude(const EseVector *vector);

/**
 * @brief Normalizes the vector to unit length.
 * 
 * @param vector Pointer to the EseVector object to normalize
 */
void vector_normalize(EseVector *vector);

#endif // ESE_VECTOR_H