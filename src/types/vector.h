/**
 * @file vector.h
 * @brief 2D vector type with floating-point components
 * @details Provides a 2D vector structure with x,y components and mathematical
 * operations
 *
 * @copyright Copyright (c) 2024 ESE Project
 * @license See LICENSE.md for license information
 */

#ifndef ESE_VECTOR_H
#define ESE_VECTOR_H

#include "vendor/json/cJSON.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ========================================
// DEFINES AND STRUCTS
// ========================================

#define VECTOR_PROXY_META "VectorProxyMeta"
#define VECTOR_META "VectorMeta"

/**
 * @brief Represents a 2D vector with floating-point components.
 *
 * @details This structure stores the x and y components of a vector in 2D
 * space.
 */
typedef struct EseVector EseVector;

// ========================================
// FORWARD DECLARATIONS
// ========================================

typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
/**
 * @brief Creates a new EseVector object.
 *
 * @details Allocates memory for a new EseVector and initializes to (0, 0).
 *          The vector is created without Lua references and must be explicitly
 *          referenced with ese_vector_ref() if Lua access is desired.
 *
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseVector object
 *
 * @warning The returned EseVector must be freed with ese_vector_destroy() to
 * prevent memory leaks
 */
EseVector *ese_vector_create(EseLuaEngine *engine);

/**
 * @brief Copies a source EseVector into a new EseVector object.
 *
 * @details This function creates a deep copy of an EseVector object. It
 * allocates a new EseVector struct and copies all numeric members. The copy is
 * created without Lua references and must be explicitly referenced with
 * ese_vector_ref() if Lua access is desired.
 *
 * @param source Pointer to the source EseVector to copy.
 * @return A new, distinct EseVector object that is a copy of the source.
 *
 * @warning The returned EseVector must be freed with ese_vector_destroy() to
 * prevent memory leaks.
 */
EseVector *ese_vector_copy(const EseVector *source);

/**
 * @brief Destroys a EseVector object, managing memory based on Lua references.
 *
 * @details If the vector has no Lua references (lua_ref == LUA_NOREF), frees
 * memory immediately. If the vector has Lua references, decrements the
 * reference counter. When the counter reaches 0, removes the Lua reference and
 * lets Lua's garbage collector handle final cleanup.
 *
 * @note If the vector is Lua-owned, memory may not be freed immediately.
 *       Lua's garbage collector will finalize it once no references remain.
 *
 * @param vector Pointer to the EseVector object to destroy
 */
void ese_vector_destroy(EseVector *vector);

/**
 * @brief Gets the size of the EseVector structure in bytes.
 *
 * @return The size of the EseVector structure in bytes
 */
size_t ese_vector_sizeof(void);

// Property access
/**
 * @brief Sets the x-component of the vector.
 *
 * @param vector Pointer to the EseVector object
 * @param x The x-component value
 */
void ese_vector_set_x(EseVector *vector, float x);

/**
 * @brief Gets the x-component of the vector.
 *
 * @param vector Pointer to the EseVector object
 * @return The x-component value
 */
float ese_vector_get_x(const EseVector *vector);

/**
 * @brief Sets the y-component of the vector.
 *
 * @param vector Pointer to the EseVector object
 * @param y The y-component value
 */
void ese_vector_set_y(EseVector *vector, float y);

/**
 * @brief Gets the y-component of the vector.
 *
 * @param vector Pointer to the EseVector object
 * @return The y-component value
 */
float ese_vector_get_y(const EseVector *vector);

// Lua-related access
/**
 * @brief Gets the Lua state associated with this vector.
 *
 * @param vector Pointer to the EseVector object
 * @return Pointer to the Lua state, or NULL if none
 */
lua_State *ese_vector_get_state(const EseVector *vector);

/**
 * @brief Gets the Lua registry reference for this vector.
 *
 * @param vector Pointer to the EseVector object
 * @return The Lua registry reference value
 */
int ese_vector_get_lua_ref(const EseVector *vector);

/**
 * @brief Gets the Lua reference count for this vector.
 *
 * @param vector Pointer to the EseVector object
 * @return The current reference count
 */
int ese_vector_get_lua_ref_count(const EseVector *vector);

/**
 * @brief Sets the Lua state associated with this vector.
 *
 * @param vector Pointer to the EseVector object
 * @param state Pointer to the Lua state
 */
void ese_vector_set_state(EseVector *vector, lua_State *state);

/**
 * @brief Creates a new EseVector instance with default values
 *
 * @details Internal function used by Lua constructors and other internal
 * functions. Allocates memory for a new EseVector and initializes all fields to
 * safe defaults. The vector starts at origin (0,0) with no Lua state or
 * references.
 *
 * @return Pointer to the newly created EseVector, or NULL on allocation failure
 */
EseVector *_ese_vector_make(void);

// Lua integration
/**
 * @brief Initializes the EseVector userdata type in the Lua state.
 *
 * @details Creates and registers the "VectorProxyMeta" metatable with __index,
 * __newindex,
 *          __gc, __tostring metamethods for property access and garbage
 * collection. This allows EseVector objects to be used naturally from Lua with
 * dot notation. Also creates the global "Vector" table with "new" and "zero"
 * constructors.
 *
 * @param engine EseLuaEngine pointer where the EseVector type will be
 * registered
 */
void ese_vector_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EseVector object to the Lua stack.
 *
 * @details If the vector has no Lua references (lua_ref == LUA_NOREF), creates
 * a new proxy table. If the vector has Lua references, retrieves the existing
 *          proxy table from the registry.
 *
 * @param vector Pointer to the EseVector object to push to Lua
 */
void ese_vector_lua_push(EseVector *vector);

/**
 * @brief Extracts a EseVector pointer from a Lua userdata object with type
 * safety.
 *
 * @details Retrieves the C EseVector pointer from the userdata
 *          that was created by ese_vector_lua_push(). Performs
 *          type checking to ensure the object is a valid EseVector userdata
 *          with the correct metatable.
 *
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseVector object
 * @return Pointer to the EseVector object, or NULL if extraction fails or type
 * check fails
 *
 * @warning Returns NULL for invalid objects - always check return value before
 * use
 */
EseVector *ese_vector_lua_get(lua_State *L, int idx);

/**
 * @brief References a EseVector object for Lua access with reference counting.
 *
 * @details If vector->lua_ref is LUA_NOREF, pushes the vector to Lua and
 * references it, setting lua_ref_count to 1. If vector->lua_ref is already set,
 * increments the reference count by 1. This prevents the vector from being
 * garbage collected while C code holds references to it.
 *
 * @param vector Pointer to the EseVector object to reference
 */
void ese_vector_ref(EseVector *vector);

/**
 * @brief Unreferences a EseVector object, decrementing the reference count.
 *
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua
 * reference is removed from the registry. This function does NOT free memory.
 *
 * @param vector Pointer to the EseVector object to unreference
 */
void ese_vector_unref(EseVector *vector);

// Mathematical operations
/**
 * @brief Sets vector to point in a cardinal or ordinal direction with given
 * magnitude.
 *
 * @param vector Pointer to the EseVector object
 * @param direction Direction character: 'n', 's', 'e', 'w', or combinations
 * like "nw"
 * @param magnitude Length of the resulting vector
 */
void ese_vector_set_direction(EseVector *vector, const char *direction, float magnitude);

/**
 * @brief Calculates the magnitude (length) of the vector.
 *
 * @param vector Pointer to the EseVector object
 * @return The magnitude of the vector
 */
float ese_vector_magnitude(const EseVector *vector);

/**
 * @brief Normalizes the vector to unit length.
 *
 * @param vector Pointer to the EseVector object to normalize
 */
void ese_vector_normalize(EseVector *vector);

/**
 * @brief Serializes an EseVector to a cJSON object.
 *
 * @details Creates a cJSON object representing the vector with type "VECTOR"
 *          and x, y components. Only serializes the
 *          vector data, not Lua-related fields.
 *
 * @param vector Pointer to the EseVector object to serialize
 * @return cJSON object representing the vector, or NULL on failure
 *
 * @warning The caller is responsible for calling cJSON_Delete() on the returned
 * object
 */
cJSON *ese_vector_serialize(const EseVector *vector);

/**
 * @brief Deserializes an EseVector from a cJSON object.
 *
 * @details Creates a new EseVector from a cJSON object with type "VECTOR"
 *          and x, y components. The vector is created
 *          with the specified engine and must be explicitly referenced with
 *          ese_vector_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for vector creation
 * @param data cJSON object containing vector data
 * @return Pointer to newly created EseVector object, or NULL on failure
 *
 * @warning The returned EseVector must be freed with ese_vector_destroy() to
 * prevent memory leaks
 */
EseVector *ese_vector_deserialize(EseLuaEngine *engine, const cJSON *data);

#endif // ESE_VECTOR_H