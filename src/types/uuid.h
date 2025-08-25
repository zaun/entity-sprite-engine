#ifndef ESE_UUID_H
#define ESE_UUID_H

#include <stdint.h>

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Represents a EseUUID as a null-terminated string.
 * 
 * @details Fixed-size character array that holds a EseUUID string in standard
 *          format (36 characters plus null terminator). Example format:
 *          "550e8400-e29b-41d4-a716-446655440000"
 */
typedef struct EseUUID {
    char value[37]; /**< The string EseUUID */

    lua_State *state;   /**< Lua State this EseUUID belongs to */
    int lua_ref;        /**< Lua registry reference to its own proxy table */
    int lua_ref_count;  /**< Number of times this uuid has been referenced in C */
} EseUUID;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
/**
 * @brief Creates a new EseUUID object with a generated EseUUID value.
 * 
 * @details Allocates memory for a new EseUUID and initializes it with a
 *          randomly generated EseUUID v4 string. The uuid is created without
 *          Lua references and must be explicitly referenced with uuid_ref()
 *          if Lua access is desired.
 * 
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseUUID object
 * 
 * @warning The returned EseUUID must be freed with uuid_destroy() to prevent memory leaks
 */
EseUUID *uuid_create(EseLuaEngine *engine);

/**
 * @brief Copies a source EseUUID into a new EseUUID object.
 * 
 * @details This function creates a deep copy of an EseUUID object. It allocates a new EseUUID
 *          struct and copies all string members. The copy is created without Lua references
 *          and must be explicitly referenced with uuid_ref() if Lua access is desired.
 * 
 * @param source Pointer to the source EseUUID to copy.
 * @return A new, distinct EseUUID object that is a copy of the source.
 * 
 * @warning The returned EseUUID must be freed with uuid_destroy() to prevent memory leaks.
 */
EseUUID *uuid_copy(const EseUUID *source);

/**
 * @brief Destroys a EseUUID object, managing memory based on Lua references.
 * 
 * @details If the uuid has no Lua references (lua_ref == LUA_NOREF), frees memory immediately.
 *          If the uuid has Lua references, decrements the reference counter.
 *          When the counter reaches 0, removes the Lua reference and lets
 *          Lua's garbage collector handle final cleanup.
 * 
 * @note If the uuid is Lua-owned, memory may not be freed immediately.
 *       Lua's garbage collector will finalize it once no references remain.
 * 
 * @param uuid Pointer to the EseUUID object to destroy
 */
void uuid_destroy(EseUUID *uuid);

// Lua integration
/**
 * @brief Initializes the EseUUID userdata type in the Lua state.
 * 
 * @details Creates and registers the "UUIDProxyMeta" metatable with __index, __newindex,
 *          __gc, __tostring metamethods for property access and garbage collection.
 *          This allows EseUUID objects to be used naturally from Lua with dot notation.
 *          Also creates the global "UUID" table with "new" constructor.
 * 
 * @param engine EseLuaEngine pointer where the EseUUID type will be registered
 */
void uuid_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EseUUID object to the Lua stack.
 * 
 * @details If the uuid has no Lua references (lua_ref == LUA_NOREF), creates a new
 *          proxy table. If the uuid has Lua references, retrieves the existing
 *          proxy table from the registry.
 * 
 * @param uuid Pointer to the EseUUID object to push to Lua
 */
void uuid_lua_push(EseUUID *uuid);

/**
 * @brief Extracts a EseUUID pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EseUUID pointer from the "__ptr" field of a Lua
 *          table that was created by uuid_lua_push(). Performs type checking
 *          to ensure the object is a valid EseUUID proxy table.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseUUID object
 * @return Pointer to the EseUUID object, or NULL if extraction fails or type check fails
 * 
 * @warning Returns NULL for invalid objects - always check return value before use
 */
EseUUID *uuid_lua_get(lua_State *L, int idx);

/**
 * @brief References a EseUUID object for Lua access with reference counting.
 * 
 * @details If uuid->lua_ref is LUA_NOREF, pushes the uuid to Lua and references it,
 *          setting lua_ref_count to 1. If uuid->lua_ref is already set, increments
 *          the reference count by 1. This prevents the uuid from being garbage
 *          collected while C code holds references to it.
 * 
 * @param uuid Pointer to the EseUUID object to reference
 */
void uuid_ref(EseUUID *uuid);

/**
 * @brief Unreferences a EseUUID object, decrementing the reference count.
 * 
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua reference
 *          is removed from the registry. This function does NOT free memory.
 * 
 * @param uuid Pointer to the EseUUID object to unreference
 */
void uuid_unref(EseUUID *uuid);

// Utility functions
/**
 * @brief Generates a random EseUUID v4 string.
 * 
 * @details Creates a version 4 (random) EseUUID string in the standard format.
 *          Uses arc4random_buf for cryptographically secure random numbers.
 * 
 * @param uuid Pointer to EseUUID object to generate value for
 */
void uuid_generate(EseUUID *uuid);

/**
 * @brief Computes a hash value for the EseUUID.
 * 
 * @details Uses a simple hash function to generate a uint64_t hash value
 *          from the EseUUID string representation.
 * 
 * @param uuid Pointer to the EseUUID object
 * @return Hash value as uint64_t
 */
uint64_t uuid_hash(const EseUUID* uuid);

#endif // ESE_UUID_H
