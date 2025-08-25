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
    float x;            /**< The x-component of the vector */
    float y;            /**< The y-component of the vector */

    lua_State *state;   /**< Lua State this EseVector belongs to */
    int lua_ref;        /**< Lua registry reference to its own proxy table */
    int lua_ref_count;  /**< Number of times this vector has been referenced in C */
} EseVector;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
/**
 * @brief Creates a new EseVector object.
 * 
 * @details Allocates memory for a new EseVector and initializes to (0, 0).
 *          The vector is created without Lua references and must be explicitly
 *          referenced with vector_ref() if Lua access is desired.
 * 
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseVector object
 * 
 * @warning The returned EseVector must be freed with vector_destroy() to prevent memory leaks
 */
EseVector *vector_create(EseLuaEngine *engine);

/**
 * @brief Copies a source EseVector into a new EseVector object.
 * 
 * @details This function creates a deep copy of an EseVector object. It allocates a new EseVector
 *          struct and copies all numeric members. The copy is created without Lua references
 *          and must be explicitly referenced with vector_ref() if Lua access is desired.
 * 
 * @param source Pointer to the source EseVector to copy.
 * @return A new, distinct EseVector object that is a copy of the source.
 * 
 * @warning The returned EseVector must be freed with vector_destroy() to prevent memory leaks.
 */
EseVector *vector_copy(const EseVector *source);

/**
 * @brief Destroys a EseVector object, managing memory based on Lua references.
 * 
 * @details If the vector has no Lua references (lua_ref == LUA_NOREF), frees memory immediately.
 *          If the vector has Lua references, decrements the reference counter.
 *          When the counter reaches 0, removes the Lua reference and lets
 *          Lua's garbage collector handle final cleanup.
 * 
 * @note If the vector is Lua-owned, memory may not be freed immediately.
 *       Lua's garbage collector will finalize it once no references remain.
 * 
 * @param vector Pointer to the EseVector object to destroy
 */
void vector_destroy(EseVector *vector);

// Lua integration
/**
 * @brief Initializes the EseVector userdata type in the Lua state.
 * 
 * @details Creates and registers the "VectorProxyMeta" metatable with __index, __newindex,
 *          __gc, __tostring metamethods for property access and garbage collection.
 *          This allows EseVector objects to be used naturally from Lua with dot notation.
 *          Also creates the global "Vector" table with "new" and "zero" constructors.
 * 
 * @param engine EseLuaEngine pointer where the EseVector type will be registered
 */
void vector_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EseVector object to the Lua stack.
 * 
 * @details If the vector has no Lua references (lua_ref == LUA_NOREF), creates a new
 *          proxy table. If the vector has Lua references, retrieves the existing
 *          proxy table from the registry.
 * 
 * @param vector Pointer to the EseVector object to push to Lua
 */
void vector_lua_push(EseVector *vector);

/**
 * @brief Extracts a EseVector pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EseVector pointer from the "__ptr" field of a Lua
 *          table that was created by vector_lua_push(). Performs
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
 * @brief References a EseVector object for Lua access with reference counting.
 * 
 * @details If vector->lua_ref is LUA_NOREF, pushes the vector to Lua and references it,
 *          setting lua_ref_count to 1. If vector->lua_ref is already set, increments
 *          the reference count by 1. This prevents the vector from being garbage
 *          collected while C code holds references to it.
 * 
 * @param vector Pointer to the EseVector object to reference
 */
void vector_ref(EseVector *vector);

/**
 * @brief Unreferences a EseVector object, decrementing the reference count.
 * 
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua reference
 *          is removed from the registry. This function does NOT free memory.
 * 
 * @param vector Pointer to the EseVector object to unreference
 */
void vector_unref(EseVector *vector);

// Mathematical operations
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