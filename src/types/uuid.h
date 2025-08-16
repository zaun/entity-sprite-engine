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
    lua_State *state; /**< Lua State this EseUUID belongs to */
    int lua_ref; /**< Lua registry reference to its own proxy table */
} EseUUID;

/**
 * @brief Initializes the EseUUID userdata type in the Lua state.
 * 
 * @details Creates and registers the "UUIDProxyMeta" metatable with __index and 
 *          __newindex metamethods for property access. This allows EseUUID objects
 *          to be used from Lua while maintaining immutability.
 * 
 * @param engine EseLuaEngine pointer where the EseUUID type will be registered
 */
void uuid_lua_init(EseLuaEngine *engine);

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
 * @brief Creates a new EseUUID object with a generated EseUUID value.
 * 
 * @details Allocates memory for a new EseUUID and initializes it with a
 *          randomly generated EseUUID v4 string.
 * 
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseUUID object
 * 
 * @warning The returned EseUUID must be freed with uuid_destroy() to prevent memory leaks
 */
EseUUID *uuid_create(EseLuaEngine *engine);

/**
 * @brief Destroys a EseUUID object and frees its memory.
 * 
 * @details Frees the memory allocated by uuid_create().
 * 
 * @param uuid Pointer to the EseUUID object to destroy
 */
void uuid_destroy(EseUUID *uuid);

/**
 * @brief Generates a random EseUUID v4 string.
 * 
 * @details Creates a version 4 (random) EseUUID string in the standard format.
 *          Uses rand() for simplicity - consider using a cryptographically
 *          secure random number generator for production use.
 * 
 * @param uuid Pointer to EseUUID object to generate value for
 */
void uuid_generate(EseUUID *uuid);

uint64_t uuid_hash(const EseUUID* uuid);

#endif // ESE_UUID_H
