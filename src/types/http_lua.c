#include "types/http_lua.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/http.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations for helper functions from http.c
extern EseHttpRequest *_ese_http_request_make(void);
extern void _ese_http_request_set_lua_ref(EseHttpRequest *request, int ref);
extern void _ese_http_request_set_lua_ref_count(EseHttpRequest *request, int count);

// Forward declarations for Lua methods
static int _ese_http_request_lua_gc(lua_State *L);
static int _ese_http_request_lua_index(lua_State *L);
static int _ese_http_request_lua_newindex(lua_State *L);
static int _ese_http_request_lua_tostring(lua_State *L);
static int _ese_http_request_lua_start(lua_State *L);
static int _ese_http_request_lua_set_timeout(lua_State *L);

// ========================================
// PRIVATE LUA FUNCTIONS
// ========================================

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseHttpRequest
 *
 * Handles cleanup when a Lua proxy table for an EseHttpRequest is garbage
 * collected. Only frees the underlying EseHttpRequest if it has no C-side
 * references.
 *
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static int _ese_http_request_lua_gc(lua_State *L) {
    // Get from userdata
    EseHttpRequest **ud = (EseHttpRequest **)luaL_testudata(L, 1, HTTP_PROXY_META);
    if (!ud || !*ud) {
        return 0; // Not our userdata or null pointer
    }

    EseHttpRequest *request = *ud;
    if (request) {
        // If lua_ref == LUA_NOREF, there are no more references to this
        // request, so we can free it. If lua_ref != LUA_NOREF, this request was
        // referenced from C and should not be freed.
        if (ese_http_request_get_lua_ref(request) == LUA_NOREF) {
            ese_http_request_destroy(request);
        }
        // Clear the pointer to prevent double-free
        *ud = NULL;
    }

    return 0;
}

/**
 * @brief Lua __index metamethod for EseHttpRequest property access
 *
 * Provides read access to request properties (url, status, body, headers, done)
 * from Lua. When a Lua script accesses request.url or request.status, this
 * function is called to retrieve the values. Also provides access to methods
 * like start and set_timeout.
 *
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid
 * properties/methods, 0 for invalid)
 */
static int _ese_http_request_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_HTTP_INDEX);
    EseHttpRequest *request = ese_http_request_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!request || !key) {
        profile_cancel(PROFILE_LUA_HTTP_INDEX);
        return 0;
    }

    if (strcmp(key, "url") == 0) {
        const char *url = ese_http_request_get_url(request);
        lua_pushstring(L, url ? url : "");
        profile_stop(PROFILE_LUA_HTTP_INDEX, "http_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "status") == 0) {
        int status = ese_http_request_get_status(request);
        lua_pushinteger(L, status);
        profile_stop(PROFILE_LUA_HTTP_INDEX, "http_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "body") == 0) {
        const char *body = ese_http_request_get_body(request);
        lua_pushstring(L, body ? body : "");
        profile_stop(PROFILE_LUA_HTTP_INDEX, "http_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "headers") == 0) {
        const char *headers = ese_http_request_get_headers(request);
        lua_pushstring(L, headers ? headers : "");
        profile_stop(PROFILE_LUA_HTTP_INDEX, "http_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "done") == 0) {
        bool done = ese_http_request_is_done(request);
        lua_pushboolean(L, done);
        profile_stop(PROFILE_LUA_HTTP_INDEX, "http_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "start") == 0) {
        lua_pushlightuserdata(L, request);
        lua_pushcclosure(L, _ese_http_request_lua_start, 1);
        profile_stop(PROFILE_LUA_HTTP_INDEX, "http_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "set_timeout") == 0) {
        lua_pushlightuserdata(L, request);
        lua_pushcclosure(L, _ese_http_request_lua_set_timeout, 1);
        profile_stop(PROFILE_LUA_HTTP_INDEX, "http_lua_index (getter)");
        return 1;
    }

    profile_cancel(PROFILE_LUA_HTTP_INDEX);
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseHttpRequest property assignment
 *
 * Provides write access to request properties from Lua. When a Lua script
 * assigns to request.property, this function is called to set the values.
 *
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static int _ese_http_request_lua_newindex(lua_State *L) {
    EseHttpRequest *request = ese_http_request_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!request || !key) {
        return 0;
    }

    // HTTP requests are mostly read-only after creation
    // Only timeout can be set
    if (strcmp(key, "timeout") == 0) {
        if (lua_isnumber(L, 3)) {
            long timeout = (long)lua_tonumber(L, 3);
            ese_http_request_set_timeout(request, timeout);
        }
    }

    return 0;
}

/**
 * @brief Lua __tostring metamethod for EseHttpRequest
 *
 * Provides string representation of the HTTP request for debugging.
 *
 * @param L Lua state
 * @return Always returns 1 (one value pushed)
 */
static int _ese_http_request_lua_tostring(lua_State *L) {
    EseHttpRequest *request = ese_http_request_lua_get(L, 1);
    if (!request) {
        lua_pushstring(L, "HTTP Request: <invalid>");
        return 1;
    }

    const char *url = ese_http_request_get_url(request);
    bool done = ese_http_request_is_done(request);
    int status = ese_http_request_get_status(request);

    char buffer[256];
    if (done) {
        snprintf(buffer, sizeof(buffer), "HTTP Request: %s (status: %d)", url ? url : "unknown",
                 status);
    } else {
        snprintf(buffer, sizeof(buffer), "HTTP Request: %s (pending)", url ? url : "unknown");
    }

    lua_pushstring(L, buffer);
    return 1;
}

// Lua methods
/**
 * @brief Lua method to start the HTTP request
 *
 * @param L Lua state
 * @return Number of values pushed (0 on success, 1 on error)
 */
static int _ese_http_request_lua_start(lua_State *L) {
    EseHttpRequest *request = (EseHttpRequest *)lua_touserdata(L, lua_upvalueindex(1));
    if (!request) {
        log_debug("HTTP", "Lua start() called on invalid HTTP request");
        lua_pushstring(L, "Invalid HTTP request");
        return 1;
    }

    log_debug("HTTP", "Lua start() called for URL: %s", ese_http_request_get_url(request));
    int result = ese_http_request_start(request);
    if (result != 0) {
        log_debug("HTTP", "Failed to start HTTP request from Lua (error: %d)", result);
        lua_pushstring(L, "Failed to start HTTP request");
        return 1;
    }

    log_verbose("HTTP", "HTTP request started successfully from Lua");
    return 0;
}

/**
 * @brief Lua method to set the timeout for the HTTP request
 *
 * @param L Lua state
 * @return Number of values pushed (0 on success, 1 on error)
 */
static int _ese_http_request_lua_set_timeout(lua_State *L) {
    EseHttpRequest *request = (EseHttpRequest *)lua_touserdata(L, lua_upvalueindex(1));
    if (!request) {
        lua_pushstring(L, "Invalid HTTP request");
        return 1;
    }

    if (!lua_isnumber(L, 1)) {
        lua_pushstring(L, "Timeout must be a number");
        return 1;
    }

    long timeout = (long)lua_tonumber(L, 1);
    ese_http_request_set_timeout(request, timeout);

    return 0;
}

// ========================================
// PRIVATE FUNCTIONS
// ========================================

static int _ese_http_request_lua_new(lua_State *L) {
    int args = lua_gettop(L);
    if (args != 1) {
        luaL_error(L, "HTTP.new(string) requires a URL string");
        return 0;
    }

    if (lua_type(L, 1) != LUA_TSTRING) {
        luaL_error(L, "HTTP.new(string) requires a URL string");
        return 0;
    }

    const char *url = lua_tostring(L, 1);
    if (!url) {
        luaL_error(L, "HTTP.new(string) requires a URL string");
        return 0;
    }

    // Get the engine from the Lua state
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    EseHttpRequest *request = ese_http_request_create(engine, url);
    if (!request) {
        luaL_error(L, "Failed to create HTTP request");
        return 0;
    }

    // Create table to hold the HTTP request
    lua_newtable(L);

    // Store the HTTP request as userdata in the table
    EseHttpRequest **ud = (EseHttpRequest **)lua_newuserdata(L, sizeof(EseHttpRequest *));
    *ud = request;
    // Set userdata metatable so __gc runs on userdata (do not change __gc
    // implementation)
    luaL_setmetatable(L, HTTP_PROXY_META);
    // Place userdata inside the proxy table field
    lua_setfield(L, -2, "__http_request");

    // Set metatable
    luaL_getmetatable(L, HTTP_PROXY_META);
    lua_setmetatable(L, -2);

    // Set up Lua integration
    _ese_http_request_set_lua_ref(request, LUA_NOREF);
    _ese_http_request_set_lua_ref_count(request, 0);
    ese_http_request_set_state(request, L);

    // Store a registry reference to the proxy table so C can manage lifetime
    lua_pushvalue(L, -1); // duplicate the proxy table
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    _ese_http_request_set_lua_ref(request, ref);
    _ese_http_request_set_lua_ref_count(request, 1);

    return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

void ese_http_request_lua_init(EseLuaEngine *engine) {
    log_debug("HTTP", "Initializing HTTP Lua integration");

    lua_State *L = engine->runtime;

    // Create metatable for HTTP request objects
    luaL_newmetatable(L, HTTP_PROXY_META);

    // Set __index metamethod
    lua_pushstring(L, "__index");
    lua_pushcfunction(L, _ese_http_request_lua_index);
    lua_settable(L, -3);

    // Set __newindex metamethod
    lua_pushstring(L, "__newindex");
    lua_pushcfunction(L, _ese_http_request_lua_newindex);
    lua_settable(L, -3);

    // Set __gc metamethod
    lua_pushstring(L, "__gc");
    lua_pushcfunction(L, _ese_http_request_lua_gc);
    lua_settable(L, -3);

    // Set __tostring metamethod
    lua_pushstring(L, "__tostring");
    lua_pushcfunction(L, _ese_http_request_lua_tostring);
    lua_settable(L, -3);

    // Pop metatable from stack
    lua_pop(L, 1);

    // Create HTTP table
    lua_newtable(L);

    // Add new function
    lua_pushstring(L, "new");
    lua_pushcfunction(L, _ese_http_request_lua_new);
    lua_settable(L, -3);

    // Add STATUS constants
    lua_pushstring(L, "STATUS");
    lua_newtable(L);

    lua_pushstring(L, "OKAY");
    lua_pushinteger(L, 200);
    lua_settable(L, -3);

    lua_pushstring(L, "NOT_FOUND");
    lua_pushinteger(L, 404);
    lua_settable(L, -3);

    lua_pushstring(L, "BAD_REQUEST");
    lua_pushinteger(L, 400);
    lua_settable(L, -3);

    lua_pushstring(L, "INTERNAL_SERVER_ERROR");
    lua_pushinteger(L, 500);
    lua_settable(L, -3);

    lua_pushstring(L, "UNKNOWN");
    lua_pushinteger(L, -1);
    lua_settable(L, -3);

    lua_pushstring(L, "IN_PROGRESS");
    lua_pushinteger(L, 0);
    lua_settable(L, -3);

    lua_settable(L, -3); // Set STATUS table in HTTP table

    // Set as global HTTP table
    lua_setglobal(L, "HTTP");

    log_debug("HTTP", "HTTP Lua integration initialized successfully");
}

void ese_http_request_lua_push(EseHttpRequest *request) {
    if (!request) {
        return;
    }

    lua_State *L = ese_http_request_get_state(request);
    if (!L) {
        return;
    }

    // Check if we already have a proxy for this request
    int ref = ese_http_request_get_lua_ref(request);
    if (ref != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        return;
    }

    // Create new proxy table
    lua_newtable(L);

    // Create userdata with pointer to request
    EseHttpRequest **ud = (EseHttpRequest **)lua_newuserdata(L, sizeof(EseHttpRequest *));
    if (!ud) {
        lua_pop(L, 1); // Remove the table we created
        return;
    }
    *ud = request;
    luaL_setmetatable(L, HTTP_PROXY_META);

    // Set as metatable for the proxy table
    lua_setmetatable(L, -2);

    // Reference it directly without calling ese_http_request_ref
    ref = luaL_ref(L, LUA_REGISTRYINDEX);
    // Store the reference in the request
    _ese_http_request_set_lua_ref(request, ref);
    _ese_http_request_set_lua_ref_count(request, 1);
}

EseHttpRequest *ese_http_request_lua_get(lua_State *L, int idx) {
    if (lua_type(L, idx) != LUA_TTABLE) {
        return NULL;
    }

    // Get the metatable
    if (!lua_getmetatable(L, idx)) {
        return NULL;
    }

    // Check if it's our metatable
    luaL_getmetatable(L, HTTP_PROXY_META);
    bool is_our_meta = lua_rawequal(L, -1, -2);
    lua_pop(L, 2); // Remove both metatables

    if (!is_our_meta) {
        return NULL;
    }

    // Get the userdata from the table
    lua_pushstring(L, "__http_request");
    lua_rawget(L, idx);

    if (lua_type(L, -1) != LUA_TUSERDATA) {
        lua_pop(L, 1);
        return NULL;
    }

    EseHttpRequest **ud = (EseHttpRequest **)lua_touserdata(L, -1);
    lua_pop(L, 1);

    return *ud;
}

void ese_http_request_ref(EseHttpRequest *request) {
    if (!request) {
        return;
    }

    lua_State *L = ese_http_request_get_state(request);
    if (!L) {
        return;
    }

    int ref = ese_http_request_get_lua_ref(request);
    if (ref == LUA_NOREF) {
        // First reference - create proxy and reference it
        lua_newtable(L);

        // Create userdata with pointer to request
        EseHttpRequest **ud = (EseHttpRequest **)lua_newuserdata(L, sizeof(EseHttpRequest *));
        *ud = request;
        luaL_setmetatable(L, HTTP_PROXY_META);

        // Set as metatable for the proxy table
        lua_setmetatable(L, -2);

        // Reference it
        ref = luaL_ref(L, LUA_REGISTRYINDEX);
        // Note: We need to store this ref in the request, but we don't have
        // access to the struct This is a limitation of the current design -
        // we'd need to modify the struct
    } else {
        // Increment reference count
        // Note: We'd need to track this in the request struct
    }
}

void ese_http_request_unref(EseHttpRequest *request) {
    if (!request) {
        return;
    }

    lua_State *L = ese_http_request_get_state(request);
    if (!L) {
        return;
    }

    int ref = ese_http_request_get_lua_ref(request);
    if (ref != LUA_NOREF) {
        // Decrement reference count
        // Note: We'd need to track this in the request struct
        // For now, just remove the reference
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
    }
}
