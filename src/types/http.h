/**
 * @file http.h
 * @brief HTTP client type for making asynchronous HTTP GET requests
 * @details Provides a simple API for making HTTP requests with callbacks and
 * timeouts
 *
 * @copyright Copyright (c) 2025-2026 Entity Sprite Engine
 * @license See LICENSE.md for details.
 */

#ifndef ESE_HTTP_H
#define ESE_HTTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// mbedTLS includes - only include if building the main library
#ifdef ESE_HTTP_IMPLEMENTATION
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#endif

// ========================================
// DEFINES AND STRUCTS
// ========================================

#define HTTP_PROXY_META "HttpProxyMeta"
#define HTTP_META "HttpMeta"

/**
 * @brief HTTP request callback function type.
 *
 * @details This callback is invoked when an HTTP request completes, either
 * successfully or with an error. The callback receives the status code,
 * headers, raw response data, body text, and user data pointer.
 *
 * @param status_code HTTP status code, or -1 for connection/parsing errors
 * @param headers NUL-terminated string containing response headers
 * @param raw Raw response data (may be NULL if error)
 * @param raw_len Length of raw response data
 * @param body NUL-terminated string containing response body
 * @param user User data pointer passed to the request
 */
typedef void (*http_callback_t)(int status_code, const char *headers, const uint8_t *raw,
                                size_t raw_len, const char *body, void *user);

/**
 * @brief HTTP request structure.
 *
 * @details Contains all information needed to make an HTTP GET request
 * including URL, timeout settings, callback, and user data.
 */
typedef struct EseHttpRequest EseHttpRequest;

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
 * @brief Creates a new HTTP request for the specified URL.
 *
 * @details Parses the URL and prepares a request structure. The request must be
 * configured with a callback and optionally a timeout before starting.
 *
 * @param engine Pointer to a EseLuaEngine
 * @param url The HTTP URL to request (must start with http://)
 * @return Pointer to the new HTTP request, or NULL on error
 */
EseHttpRequest *ese_http_request_create(EseLuaEngine *engine, const char *url);

/**
 * @brief Destroys an HTTP request and frees its resources.
 *
 * @details This function should only be called on requests that have not been
 * started. Started requests free themselves automatically after the callback
 * completes.
 *
 * @param request The HTTP request to destroy
 */
void ese_http_request_destroy(EseHttpRequest *request);

/**
 * @brief Gets the size of the EseHttpRequest structure in bytes.
 *
 * @return The size of the EseHttpRequest structure in bytes
 */
size_t ese_http_request_sizeof(void);

// Property access
/**
 * @brief Gets the URL of the HTTP request.
 *
 * @param request Pointer to the EseHttpRequest object
 * @return The URL string
 */
const char *ese_http_request_get_url(const EseHttpRequest *request);

/**
 * @brief Gets the status code of the HTTP request.
 *
 * @param request Pointer to the EseHttpRequest object
 * @return The HTTP status code, or -1 if not completed
 */
int ese_http_request_get_status(const EseHttpRequest *request);

/**
 * @brief Gets the response body of the HTTP request.
 *
 * @param request Pointer to the EseHttpRequest object
 * @return The response body string, or NULL if not completed
 */
const char *ese_http_request_get_body(const EseHttpRequest *request);

/**
 * @brief Gets the response headers of the HTTP request.
 *
 * @param request Pointer to the EseHttpRequest object
 * @return The response headers string, or NULL if not completed
 */
const char *ese_http_request_get_headers(const EseHttpRequest *request);

/**
 * @brief Checks if the HTTP request is done (completed or failed).
 *
 * @param request Pointer to the EseHttpRequest object
 * @return true if the request is done, false otherwise
 */
bool ese_http_request_is_done(const EseHttpRequest *request);

/**
 * @brief Sets the timeout for the HTTP request.
 *
 * @details Sets both connection and receive timeouts. A timeout of 0 uses
 * system defaults.
 *
 * @param request The HTTP request to configure
 * @param timeout_ms Timeout in milliseconds, or 0 for system default
 */
void ese_http_request_set_timeout(EseHttpRequest *request, long timeout_ms);

/**
 * @brief Sets the callback function and user data for the request.
 *
 * @details The callback will be invoked when the request completes. The request
 * takes ownership of the user data pointer and passes it to the callback.
 *
 * @param request The HTTP request to configure
 * @param callback Function to call when request completes
 * @param user_data User data to pass to the callback
 */
void ese_http_request_set_callback(EseHttpRequest *request, http_callback_t callback,
                                   void *user_data);

/**
 * @brief Starts the HTTP request on a background thread.
 *
 * @details The request runs asynchronously on a detached pthread. The request
 * will free itself after the callback completes. Returns immediately.
 *
 * @param request The HTTP request to start
 * @return 0 on success, negative value on error
 */
int ese_http_request_start(EseHttpRequest *request);

// Lua-related access
/**
 * @brief Gets the Lua state associated with this HTTP request.
 *
 * @param request Pointer to the EseHttpRequest object
 * @return Pointer to the Lua state, or NULL if none
 */
lua_State *ese_http_request_get_state(const EseHttpRequest *request);

/**
 * @brief Gets the Lua registry reference for this HTTP request.
 *
 * @param request Pointer to the EseHttpRequest object
 * @return The Lua registry reference value
 */
int ese_http_request_get_lua_ref(const EseHttpRequest *request);

/**
 * @brief Gets the Lua reference count for this HTTP request.
 *
 * @param request Pointer to the EseHttpRequest object
 * @return The current reference count
 */
int ese_http_request_get_lua_ref_count(const EseHttpRequest *request);

/**
 * @brief Sets the Lua state associated with this HTTP request.
 *
 * @param request Pointer to the EseHttpRequest object
 * @param state Pointer to the Lua state
 */
void ese_http_request_set_state(EseHttpRequest *request, lua_State *state);

/**
 * @brief Creates a new EseHttpRequest instance with default values
 *
 * @details Internal function used by Lua constructors and other internal
 * functions. Allocates memory for a new EseHttpRequest and initializes all
 * fields to safe defaults. The request starts with no URL, no Lua state or
 * references.
 *
 * @return Pointer to the newly created EseHttpRequest, or NULL on allocation
 * failure
 */
EseHttpRequest *_ese_http_request_make(void);

// Lua integration
/**
 * @brief Initializes the EseHttpRequest userdata type in the Lua state.
 *
 * @details Creates and registers the "HttpProxyMeta" metatable with __index,
 * __newindex,
 *          __gc, __tostring metamethods for property access and garbage
 * collection. This allows EseHttpRequest objects to be used naturally from Lua
 * with dot notation. Also creates the global "HTTP" table with "new"
 * constructor and status constants.
 *
 * @param engine EseLuaEngine pointer where the EseHttpRequest type will be
 * registered
 */
void ese_http_request_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EseHttpRequest object to the Lua stack.
 *
 * @details If the request has no Lua references (lua_ref == LUA_NOREF), creates
 * a new proxy table. If the request has Lua references, retrieves the existing
 *          proxy table from the registry.
 *
 * @param request Pointer to the EseHttpRequest object to push to Lua
 */
void ese_http_request_lua_push(EseHttpRequest *request);

/**
 * @brief Extracts a EseHttpRequest pointer from a Lua userdata object with type
 * safety.
 *
 * @details Retrieves the C EseHttpRequest pointer from the userdata
 *          that was created by ese_http_request_lua_push(). Performs
 *          type checking to ensure the object is a valid EseHttpRequest
 * userdata with the correct metatable.
 *
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseHttpRequest object
 * @return Pointer to the EseHttpRequest object, or NULL if extraction fails or
 * type check fails
 *
 * @warning Returns NULL for invalid objects - always check return value before
 * use
 */
EseHttpRequest *ese_http_request_lua_get(lua_State *L, int idx);

/**
 * @brief References a EseHttpRequest object for Lua access with reference
 * counting.
 *
 * @details If request->lua_ref is LUA_NOREF, pushes the request to Lua and
 * references it, setting lua_ref_count to 1. If request->lua_ref is already
 * set, increments the reference count by 1. This prevents the request from
 * being garbage collected while C code holds references to it.
 *
 * @param request Pointer to the EseHttpRequest object to reference
 */
void ese_http_request_ref(EseHttpRequest *request);

/**
 * @brief Unreferences a EseHttpRequest object, decrementing the reference
 * count.
 *
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua
 * reference is removed from the registry. This function does NOT free memory.
 *
 * @param request Pointer to the EseHttpRequest object to unreference
 */
void ese_http_request_unref(EseHttpRequest *request);

#endif // ESE_HTTP_H
