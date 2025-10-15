#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "scripting/lua_engine.h"
#include "core/memory_manager.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/uuid.h"
#include "types/uuid_lua.h"
#include "vendor/json/cJSON.h"

// Forward declarations for helper functions from uuid.c
extern EseUUID *_ese_uuid_make(void);

// Forward declarations for Lua methods
static int _ese_uuid_lua_reset_method(lua_State *L);
static int _ese_uuid_lua_to_json(lua_State *L);

// ========================================
// PRIVATE LUA FUNCTIONS
// ========================================

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseUUID
 * 
 * Handles cleanup when a Lua proxy table for an EseUUID is garbage collected.
 * Only frees the underlying EseUUID if it has no C-side references.
 * 
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static int _ese_uuid_lua_gc(lua_State *L) {
    // Get from userdata
    EseUUID **ud = (EseUUID **)luaL_testudata(L, 1, UUID_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseUUID *uuid = *ud;
    if (uuid) {
        // If lua_ref == LUA_NOREF, there are no more references to this uuid, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this uuid was referenced from C and should not be freed.
        if (ese_uuid_get_lua_ref(uuid) == LUA_NOREF) {
            ese_uuid_destroy(uuid);
        }
    }

    return 0;
}

/**
 * @brief Lua __index metamethod for EseUUID property access
 * 
 * Provides read access to UUID properties (value, string) from Lua. When a Lua script
 * accesses uuid.value or uuid.string, this function is called to retrieve the values.
 * Also provides access to methods like reset.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid properties/methods, 0 for invalid)
 */
static int _ese_uuid_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_UUID_INDEX);
    EseUUID *uuid = ese_uuid_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!uuid || !key) {
        profile_cancel(PROFILE_LUA_UUID_INDEX);
        return 0;
    }

    if (strcmp(key, "value") == 0 || strcmp(key, "string") == 0) {
        lua_pushstring(L, ese_uuid_get_value(uuid));
        profile_stop(PROFILE_LUA_UUID_INDEX, "uuid_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "reset") == 0) {
        // Return a reset function for this EseUUID instance
        lua_pushlightuserdata(L, uuid);
        lua_pushcclosure(L, _ese_uuid_lua_reset_method, 1);
        profile_stop(PROFILE_LUA_UUID_INDEX, "uuid_lua_index (method)");
        return 1;
    } else if (strcmp(key, "toJSON") == 0) {
        lua_pushlightuserdata(L, uuid);
        lua_pushcclosure(L, _ese_uuid_lua_to_json, 1);
        profile_stop(PROFILE_LUA_UUID_INDEX, "uuid_lua_index (method)");
        return 1;
    }
    
    profile_stop(PROFILE_LUA_UUID_INDEX, "uuid_lua_index (invalid)");
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseUUID property assignment
 * 
 * Provides write access to UUID properties from Lua. Since UUIDs are immutable,
 * this function always returns an error for any property assignment attempts.
 * 
 * @param L Lua state
 * @return Never returns (always calls luaL_error)
 */
static int _ese_uuid_lua_newindex(lua_State *L) {
    profile_start(PROFILE_LUA_UUID_NEWINDEX);
    EseUUID *uuid = ese_uuid_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!uuid || !key) {
        profile_cancel(PROFILE_LUA_UUID_NEWINDEX);
        return 0;
    }

    profile_stop(PROFILE_LUA_UUID_NEWINDEX, "uuid_lua_newindex (error)");
    return luaL_error(L, "UUID objects are immutable - cannot set property '%s'", key);
}

/**
 * @brief Lua __tostring metamethod for EseUUID string representation
 * 
 * Converts an EseUUID to a human-readable string for debugging and display.
 * The format includes the memory address and current UUID value.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
static int _ese_uuid_lua_tostring(lua_State *L) {
    EseUUID *uuid = ese_uuid_lua_get(L, 1);

    if (!uuid) {
        lua_pushstring(L, "UUID: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "UUID: %p (%s)", (void*)uuid, ese_uuid_get_value(uuid));
    lua_pushstring(L, buf);

    return 1;
}

/**
 * @brief Lua instance method for converting EseUUID to JSON string
 */
static int _ese_uuid_lua_to_json(lua_State *L) {
    EseUUID *uuid = ese_uuid_lua_get(L, 1);
    if (!uuid) {
        return luaL_error(L, "UUID:toJSON() called on invalid uuid");
    }

    cJSON *json = ese_uuid_serialize(uuid);
    if (!json) {
        return luaL_error(L, "UUID:toJSON() failed to serialize uuid");
    }

    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!json_str) {
        return luaL_error(L, "UUID:toJSON() failed to convert to string");
    }

    lua_pushstring(L, json_str);
    free(json_str);
    return 1;
}

// Lua constructors
/**
 * @brief Lua constructor function for creating new EseUUID instances
 * 
 * Creates a new EseUUID from Lua with a randomly generated UUID value.
 * This function is called when Lua code executes `UUID.new()`.
 * It creates the underlying EseUUID and returns a proxy table that provides
 * access to the UUID's properties and methods.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_uuid_lua_new(lua_State *L) {
    profile_start(PROFILE_LUA_UUID_NEW);

    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 0) {
        profile_cancel(PROFILE_LUA_UUID_NEW);
        return luaL_error(L, "UUID.new() takes 0 argument");
    }

    // Create the uuid using the standard creation function
    EseUUID *uuid = _ese_uuid_make();
    
    // Set the Lua state
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (engine) {
        ese_uuid_set_state(uuid, L);
    }
    
    // Create userdata directly
    EseUUID **ud = (EseUUID **)lua_newuserdata(L, sizeof(EseUUID *));
    *ud = uuid;

    // Attach metatable
    luaL_getmetatable(L, UUID_PROXY_META);
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_UUID_NEW, "uuid_lua_new");
    return 1;
}

// Lua methods
/**
 * @brief Lua method for resetting UUID to a new random value
 * 
 * Generates a new random UUID value for the existing UUID instance.
 * This allows reusing UUID objects while maintaining their identity.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_uuid_lua_reset_method(lua_State *L) {
    // Get the EseUUID from the closure's upvalue
    EseUUID *uuid = (EseUUID *)lua_touserdata(L, lua_upvalueindex(1));
    if (!uuid) {
        return luaL_error(L, "Invalid EseUUID object in reset method");
    }
    
    ese_uuid_generate_new(uuid);
    return 0;
}

/**
 * @brief Lua static method for creating EseUUID from JSON string
 */
static int _ese_uuid_lua_from_json(lua_State *L) {
    int argc = lua_gettop(L);
    if (argc != 1) {
        return luaL_error(L, "UUID.fromJSON(string) takes 1 argument");
    }
    if (lua_type(L, 1) != LUA_TSTRING) {
        return luaL_error(L, "UUID.fromJSON(string) argument must be a string");
    }

    const char *json_str = lua_tostring(L, 1);
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        log_error("UUID", "UUID.fromJSON: failed to parse JSON string: %s", json_str ? json_str : "NULL");
        return luaL_error(L, "UUID.fromJSON: invalid JSON string");
    }

    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (!engine) {
        cJSON_Delete(json);
        return luaL_error(L, "UUID.fromJSON: no engine available");
    }

    EseUUID *uuid = ese_uuid_deserialize(engine, json);
    cJSON_Delete(json);
    if (!uuid) {
        return luaL_error(L, "UUID.fromJSON: failed to deserialize uuid");
    }

    ese_uuid_lua_push(uuid);
    return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

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
void _ese_uuid_lua_init(EseLuaEngine *engine) {
    log_assert("UUID", engine, "_ese_uuid_lua_init called with NULL engine");

    // Create metatable
    lua_engine_new_object_meta(engine, UUID_PROXY_META, 
        _ese_uuid_lua_index, 
        _ese_uuid_lua_newindex, 
        _ese_uuid_lua_gc, 
        _ese_uuid_lua_tostring);
    
    // Create global UUID table with functions
    const char *keys[] = {"new", "fromJSON"};
    lua_CFunction functions[] = {_ese_uuid_lua_new, _ese_uuid_lua_from_json};
    lua_engine_new_object(engine, "UUID", 2, keys, functions);
}
