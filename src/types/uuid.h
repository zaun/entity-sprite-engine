#ifndef ESE_UUID_H
#define ESE_UUID_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define UUID_PROXY_META "UUIDProxyMeta"

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
typedef struct EseUUID EseUUID;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
/**
 * @brief Creates a new EseUUID object with a generated EseUUID value.
 * 
 * @details Allocates memory for a new EseUUID and initializes it with a
 *          randomly generated EseUUID v4 string. The uuid is created without
 *          Lua references and must be explicitly referenced with ese_uuid_ref()
 *          if Lua access is desired.
 * 
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseUUID object
 * 
 * @warning The returned EseUUID must be freed with ese_uuid_destroy() to prevent memory leaks
 */
EseUUID *ese_uuid_create(EseLuaEngine *engine);

/**
 * @brief Copies a source EseUUID into a new EseUUID object.
 * 
 * @details This function creates a deep copy of an EseUUID object. It allocates a new EseUUID
 *          struct and copies all string members. The copy is created without Lua references
 *          and must be explicitly referenced with ese_uuid_ref() if Lua access is desired.
 * 
 * @param source Pointer to the source EseUUID to copy.
 * @return A new, distinct EseUUID object that is a copy of the source.
 * 
 * @warning The returned EseUUID must be freed with ese_uuid_destroy() to prevent memory leaks.
 */
EseUUID *ese_uuid_copy(const EseUUID *source);

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
void ese_uuid_destroy(EseUUID *uuid);

/**
 * @brief Gets the size of the EseUUID structure in bytes.
 * 
 * @return The size of the EseUUID structure in bytes
 */
size_t ese_uuid_sizeof(void);

// Property access
/**
 * @brief Gets the UUID string value.
 * 
 * @param uuid Pointer to the EseUUID object
 * @return The UUID string value (36 characters)
 */
const char *ese_uuid_get_value(const EseUUID *uuid);

// Lua-related access
/**
 * @brief Gets the Lua state associated with this UUID.
 * 
 * @param uuid Pointer to the EseUUID object
 * @return Pointer to the Lua state, or NULL if none
 */
lua_State *ese_uuid_get_state(const EseUUID *uuid);

/**
 * @brief Gets the Lua registry reference for this UUID.
 * 
 * @param uuid Pointer to the EseUUID object
 * @return The Lua registry reference value
 */
int ese_uuid_get_lua_ref(const EseUUID *uuid);

/**
 * @brief Gets the Lua reference count for this UUID.
 * 
 * @param uuid Pointer to the EseUUID object
 * @return The current reference count
 */
int ese_uuid_get_lua_ref_count(const EseUUID *uuid);

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
void ese_uuid_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EseUUID object to the Lua stack.
 * 
 * @details If the uuid has no Lua references (lua_ref == LUA_NOREF), creates a new
 *          proxy table. If the uuid has Lua references, retrieves the existing
 *          proxy table from the registry.
 * 
 * @param uuid Pointer to the EseUUID object to push to Lua
 */
void ese_uuid_lua_push(EseUUID *uuid);

/**
 * @brief Extracts a EseUUID pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EseUUID pointer from the "__ptr" field of a Lua
 *          table that was created by ese_uuid_lua_push(). Performs type checking
 *          to ensure the object is a valid EseUUID proxy table.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseUUID object
 * @return Pointer to the EseUUID object, or NULL if extraction fails or type check fails
 * 
 * @warning Returns NULL for invalid objects - always check return value before use
 */
EseUUID *ese_uuid_lua_get(lua_State *L, int idx);

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
void ese_uuid_ref(EseUUID *uuid);

/**
 * @brief Unreferences a EseUUID object, decrementing the reference count.
 * 
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua reference
 *          is removed from the registry. This function does NOT free memory.
 * 
 * @param uuid Pointer to the EseUUID object to unreference
 */
void ese_uuid_unref(EseUUID *uuid);

// Utility functions
/**
 * @brief Generates a random EseUUID v4 string.
 * 
 * @details Creates a version 4 (random) EseUUID string in the standard format.
 *          Uses arc4random_buf for cryptographically secure random numbers.
 * 
 * @param uuid Pointer to EseUUID object to generate value for
 */
void ese_uuid_generate_new(EseUUID *uuid);

/**
 * @brief Computes a hash value for the EseUUID.
 * 
 * @details Uses a simple hash function to generate a uint64_t hash value
 *          from the EseUUID string representation.
 * 
 * @param uuid Pointer to the EseUUID object
 * @return Hash value as uint64_t
 */
uint64_t ese_uuid_hash(const EseUUID* uuid);

#endif // ESE_UUID_H
