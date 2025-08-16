#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "scripting/lua_engine.h"
#include "core/memory_manager.h"
#include "utility/log.h"
#include "types/uuid.h"

/**
 * @brief Pushes a EseUUID pointer as a Lua userdata object onto the stack.
 * 
 * @details Creates a new Lua table that acts as a proxy for the EseUUID object,
 *          storing the C pointer as light userdata in the "__ptr" field and
 *          setting the UUIDProxyMeta metatable for property access.
 * 
 * @param uuid Pointer to the EseUUID object to wrap for Lua access
 * @param is_lua_owned True if LUA will handle freeing
 * 
 * @warning The EseUUID object must remain valid for the lifetime of the Lua object
 */
void _uuid_lua_register(EseUUID *uuid, bool is_lua_owned) {
    log_assert("UUID", uuid, "_uuid_lua_register called with NULL uuid");
    log_assert("UUID", uuid->lua_ref == LUA_NOREF, "_uuid_lua_register uuid is already registered");

    lua_newtable(uuid->state);
    lua_pushlightuserdata(uuid->state, uuid);
    lua_setfield(uuid->state, -2, "__ptr");

    // Store the ownership flag
    lua_pushboolean(uuid->state, is_lua_owned);
    lua_setfield(uuid->state, -2, "__is_lua_owned");

    luaL_getmetatable(uuid->state, "UUIDProxyMeta");
    lua_setmetatable(uuid->state, -2);

    // Store a reference to this proxy table in the Lua registry
    uuid->lua_ref = luaL_ref(uuid->state, LUA_REGISTRYINDEX);
}

void uuid_lua_push(EseUUID *uuid) {
    log_assert("UUID", uuid, "uuid_lua_push called with NULL uuid");
    log_assert("UUID", uuid->lua_ref != LUA_NOREF, "uuid_lua_push uuid not registered with lua");

    // Push the proxy table back onto the stack for Lua to receive
    lua_rawgeti(uuid->state, LUA_REGISTRYINDEX, uuid->lua_ref);
}

/**
 * @brief Lua function to create a new EseUUID object.
 * 
 * @details Callable from Lua as EseUUID.new(). Creates a new EseUUID with a
 *          randomly generated value and pushes it onto the Lua stack.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new EseUUID object)
 * 
 * @warning Items created in Lua are owned by Lua
 */
static int _uuid_lua_new(lua_State *L) {
    EseUUID *uuid = (EseUUID *)memory_manager.malloc(sizeof(EseUUID), MMTAG_GENERAL);
    uuid->state = L;
    uuid->lua_ref = LUA_NOREF;
    uuid_generate(uuid);

    _uuid_lua_register(uuid, true);
    uuid_lua_push(uuid);
    return 1;
}

/**
 * @brief Lua method to reset a EseUUID instance.
 * 
 * @details Callable as uuid:reset(). Generates a new EseUUID value for
 *          the existing EseUUID object.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 0)
 */
static int _uuid_lua_reset_method(lua_State *L) {
    // Get the EseUUID from the closure's upvalue
    EseUUID *uuid = (EseUUID *)lua_touserdata(L, lua_upvalueindex(1));
    if (!uuid) {
        return luaL_error(L, "Invalid EseUUID object in reset method");
    }
    
    uuid_generate(uuid);
    return 0;
}

/**
 * @brief Lua __index metamethod for EseUUID objects (getter).
 * 
 * @details Handles property access for EseUUID objects from Lua. Currently
 *          supports reading the EseUUID as a string value.
 * 
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack
 */
static int _uuid_lua_index(lua_State *L) {
    EseUUID *uuid = uuid_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!uuid || !key) return 0;

    if (strcmp(key, "value") == 0 || strcmp(key, "string") == 0) {
        lua_pushstring(L, uuid->value);
        return 1;
    } else if (strcmp(key, "reset") == 0) {
        // Return a reset function for this EseUUID instance
        lua_pushlightuserdata(L, uuid);
        lua_pushcclosure(L, _uuid_lua_reset_method, 1);
        return 1;
    }
    
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseUUID objects (setter).
 * 
 * @details Handles property assignment for EseUUID objects from Lua. UUIDs are
 *          typically immutable, so this function prevents modification.
 * 
 * @param L Lua state pointer
 * @return Always returns 0 or throws Lua error for modification attempts
 */
static int _uuid_lua_newindex(lua_State *L) {
    EseUUID *uuid = uuid_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!uuid || !key) return 0;

    return luaL_error(L, "UUID objects are immutable - cannot set property '%s'", key);
}

/**
 * @brief Lua __gc metamethod for EseUUID objects.
 *
 * @details Checks the '__is_lua_owned' flag in the proxy table. If true,
 * it means this EseUUID's memory was allocated by Lua and should be freed.
 * If false, the EseUUID's memory is managed externally (by C) and is not freed here.
 *
 * @param L Lua state pointer
 * @return Always 0 (no return values)
 */
static int _uuid_lua_gc(lua_State *L) {
    EseUUID *uuid = uuid_lua_get(L, 1);

    if (uuid) {
        // Get the __is_lua_owned flag from the proxy table itself
        lua_getfield(L, 1, "__is_lua_owned");
        bool is_lua_owned = lua_toboolean(L, -1);
        lua_pop(L, 1);

        if (is_lua_owned) {
            uuid_destroy(uuid);
            log_debug("LUA_GC", "UUID object (Lua-owned) garbage collected and C memory freed.");
        } else {
            log_debug("LUA_GC", "UUID object (C-owned) garbage collected, C memory *not* freed.");
        }
    }

    return 0;
}

static int _uuid_lua_tostring(lua_State *L) {
    EseUUID *uuid = uuid_lua_get(L, 1);

    if (!uuid) {
        lua_pushstring(L, "UUID: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "UUID: %p (%s)", (void*)uuid, uuid->value);
    lua_pushstring(L, buf);

    return 1;
}

void uuid_lua_init(EseLuaEngine *engine) {
    if (luaL_newmetatable(engine->runtime, "UUIDProxyMeta")) {
        log_debug("LUA", "Adding entity UUIDProxyMeta to engine");
        lua_pushcfunction(engine->runtime, _uuid_lua_index);
        lua_setfield(engine->runtime, -2, "__index");               // For property getters
        lua_pushcfunction(engine->runtime, _uuid_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");            // For property setters
        lua_pushcfunction(engine->runtime, _uuid_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");                  // For garbage collection
        lua_pushcfunction(engine->runtime, _uuid_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");            // For printing/debugging
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
    
    // Create global EseUUID table with constructor
    lua_getglobal(engine->runtime, "UUID");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1); // Pop the nil value
        log_debug("LUA", "Creating global EseUUID table");
        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _uuid_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_setglobal(engine->runtime, "UUID");
    } else {
        lua_pop(engine->runtime, 1); // Pop the existing EseUUID table
    }
}

EseUUID *uuid_lua_get(lua_State *L, int idx) {
    // Check if the value at idx is a table
    if (!lua_istable(L, idx)) {
        return NULL;
    }
    
    // Check if it has the correct metatable
    if (!lua_getmetatable(L, idx)) {
        return NULL; // No metatable
    }
    
    // Get the expected metatable for comparison
    luaL_getmetatable(L, "UUIDProxyMeta");
    
    // Compare metatables
    if (!lua_rawequal(L, -1, -2)) {
        lua_pop(L, 2); // Pop both metatables
        return NULL; // Wrong metatable
    }
    
    lua_pop(L, 2); // Pop both metatables
    
    // Get the __ptr field
    lua_getfield(L, idx, "__ptr");
    
    // Check if __ptr exists and is light userdata
    if (!lua_islightuserdata(L, -1)) {
        lua_pop(L, 1); // Pop the __ptr value (or nil)
        return NULL;
    }
    
    // Extract the pointer
    void *uuid = lua_touserdata(L, -1);
    lua_pop(L, 1); // Pop the __ptr value
    
    return (EseUUID *)uuid;
}

EseUUID *uuid_create(EseLuaEngine *engine) {
    EseUUID *uuid = (EseUUID *)memory_manager.malloc(sizeof(EseUUID), MMTAG_GENERAL);
    uuid->state = engine->runtime;
    uuid->lua_ref = LUA_NOREF;
    uuid_generate(uuid);
    _uuid_lua_register(uuid, false);
    return uuid;
}

void uuid_destroy(EseUUID *uuid) {
    if (uuid) {
        if (uuid->lua_ref != LUA_NOREF) {
            luaL_unref(uuid->state, LUA_REGISTRYINDEX, uuid->lua_ref);
        }
        memory_manager.free(uuid);
    }
}

void uuid_generate(EseUUID *uuid) {
    log_assert("ENGINE", uuid, "uuid_generate called with NULL uuid");

    unsigned char bytes[16]; // A EseUUID is 128 bits, which is 16 bytes

    // Fill the 16-byte buffer with cryptographically strong random data
    arc4random_buf(bytes, sizeof(bytes));

    // Set the EseUUID version (nibble 4) to 0100 (binary) = 4 (hex)
    bytes[6] = (bytes[6] & 0x0F) | 0x40; // Clear the top 4 bits and set them to 0100 (4)

    // Set the EseUUID variant (first two bits of nibble 13) to 10xx (binary)
    bytes[8] = (bytes[8] & 0x3F) | 0x80; // Clear the top 2 bits and set them to 10 (8, 9, A, B)

    // Format the bytes into the standard EseUUID string representation
    snprintf(uuid->value, sizeof(uuid->value),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             bytes[0], bytes[1], bytes[2], bytes[3],      // 4 bytes = 8 hex digits
             bytes[4], bytes[5],                          // 2 bytes = 4 hex digits
             bytes[6], bytes[7],                          // 2 bytes = 4 hex digits (version byte set here)
             bytes[8], bytes[9],                          // 2 bytes = 4 hex digits (variant byte set here)
             bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]); // 6 bytes = 12 hex digits
}

uint64_t uuid_hash(const EseUUID* uuid) {
    uint64_t hash = 5381;
    const char* str = uuid->value;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}
