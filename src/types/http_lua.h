/**
 * @file http_lua.h
 * @brief Lua integration for HTTP client type
 * @details Provides Lua bindings for EseHttpRequest objects
 * 
 * @copyright Copyright (c) 2025-2026 Entity Sprite Engine
 * @license See LICENSE.md for details.
 */

#ifndef ESE_HTTP_LUA_H
#define ESE_HTTP_LUA_H

#include "types/http.h"

// ========================================
// FORWARD DECLARATIONS
// ========================================

typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Initializes the EseHttpRequest userdata type in the Lua state.
 * 
 * @details Creates and registers the "HttpProxyMeta" metatable with __index, __newindex,
 *          __gc, __tostring metamethods for property access and garbage collection.
 *          This allows EseHttpRequest objects to be used naturally from Lua with dot notation.
 *          Also creates the global "HTTP" table with "new" constructor and status constants.
 * 
 * @param engine EseLuaEngine pointer where the EseHttpRequest type will be registered
 */
void ese_http_request_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EseHttpRequest object to the Lua stack.
 * 
 * @details If the request has no Lua references (lua_ref == LUA_NOREF), creates a new
 *          proxy table. If the request has Lua references, retrieves the existing
 *          proxy table from the registry.
 * 
 * @param request Pointer to the EseHttpRequest object to push to Lua
 */
void ese_http_request_lua_push(EseHttpRequest *request);

/**
 * @brief Extracts a EseHttpRequest pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EseHttpRequest pointer from the userdata
 *          that was created by ese_http_request_lua_push(). Performs
 *          type checking to ensure the object is a valid EseHttpRequest userdata
 *          with the correct metatable.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseHttpRequest object
 * @return Pointer to the EseHttpRequest object, or NULL if extraction fails or type check fails
 * 
 * @warning Returns NULL for invalid objects - always check return value before use
 */
EseHttpRequest *ese_http_request_lua_get(lua_State *L, int idx);

/**
 * @brief References a EseHttpRequest object for Lua access with reference counting.
 * 
 * @details If request->lua_ref is LUA_NOREF, pushes the request to Lua and references it,
 *          setting lua_ref_count to 1. If request->lua_ref is already set, increments
 *          the reference count by 1. This prevents the request from being garbage
 *          collected while C code holds references to it.
 * 
 * @param request Pointer to the EseHttpRequest object to reference
 */
void ese_http_request_ref(EseHttpRequest *request);

/**
 * @brief Unreferences a EseHttpRequest object, decrementing the reference count.
 * 
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua reference
 *          is removed from the registry. This function does NOT free memory.
 * 
 * @param request Pointer to the EseHttpRequest object to unreference
 */
void ese_http_request_unref(EseHttpRequest *request);

#endif // ESE_HTTP_LUA_H
