#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "scripting/lua_engine.h"
#include "core/memory_manager.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/uuid.h"

// ========================================
// PRIVATE STRUCT DEFINITION
// ========================================

/**
 * @brief Internal structure for EseUUID
 * 
 * @details This structure is only visible within this implementation file.
 *          External code must use the provided getter/setter functions.
 */
struct EseUUID {
    char value[37];     /**< The string EseUUID */

    lua_State *state;   /**< Lua State this EseUUID belongs to */
    int lua_ref;        /**< Lua registry reference to its own proxy table */
    int lua_ref_count;  /**< Number of times this uuid has been referenced in C */
};

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseUUID *_ese_uuid_make(void);

// Lua metamethods
static int _ese_uuid_lua_gc(lua_State *L);
static int _ese_uuid_lua_index(lua_State *L);
static int _ese_uuid_lua_newindex(lua_State *L);
static int _ese_uuid_lua_tostring(lua_State *L);

// Lua constructors
static int _ese_uuid_lua_new(lua_State *L);

// Lua methods
static int _ese_uuid_lua_reset_method(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseUUID instance with default values
 * 
 * Allocates memory for a new EseUUID and initializes all fields to safe defaults.
 * The UUID starts with no Lua state or references, and a new random UUID is generated.
 * 
 * @return Pointer to the newly created EseUUID, or NULL on allocation failure
 */
static EseUUID *_ese_uuid_make() {
    EseUUID *uuid = (EseUUID *)memory_manager.malloc(sizeof(EseUUID), MMTAG_UUID);
    uuid->state = NULL;
    uuid->lua_ref = LUA_NOREF;
    uuid->lua_ref_count = 0;
    ese_uuid_generate_new(uuid);
    return uuid;
}

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
        if (uuid->lua_ref == LUA_NOREF) {
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
        lua_pushstring(L, uuid->value);
        profile_stop(PROFILE_LUA_UUID_INDEX, "uuid_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "reset") == 0) {
        // Return a reset function for this EseUUID instance
        lua_pushlightuserdata(L, uuid);
        lua_pushcclosure(L, _ese_uuid_lua_reset_method, 1);
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
    snprintf(buf, sizeof(buf), "UUID: %p (%s)", (void*)uuid, uuid->value);
    lua_pushstring(L, buf);

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
    uuid->state = L;
    
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

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseUUID *ese_uuid_create(EseLuaEngine *engine) {
    log_assert("UUID", engine, "ese_uuid_create called with NULL engine");

    EseUUID *uuid = _ese_uuid_make();
    uuid->state = engine->runtime;
    return uuid;
}

EseUUID *ese_uuid_copy(const EseUUID *source) {
    log_assert("UUID", source, "ese_uuid_copy called with NULL source");

    EseUUID *copy = (EseUUID *)memory_manager.malloc(sizeof(EseUUID), MMTAG_UUID);
    strcpy(copy->value, source->value);
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
    copy->lua_ref_count = 0;
    return copy;
}

void ese_uuid_destroy(EseUUID *uuid) {
    if (!uuid) return;
    
    if (uuid->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(uuid);
    } else {
        ese_uuid_unref(uuid);
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

// Lua integration
void ese_uuid_lua_init(EseLuaEngine *engine) {
    log_assert("UUID", engine, "ese_uuid_lua_init called with NULL engine");

    if (luaL_newmetatable(engine->runtime, UUID_PROXY_META)) {
        log_debug("LUA", "Adding entity UUIDProxyMeta to engine");
        lua_pushstring(engine->runtime, UUID_PROXY_META);
        lua_setfield(engine->runtime, -2, "__name");
        lua_pushcfunction(engine->runtime, _ese_uuid_lua_index);
        lua_setfield(engine->runtime, -2, "__index");               // For property getters
        lua_pushcfunction(engine->runtime, _ese_uuid_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");            // For property setters
        lua_pushcfunction(engine->runtime, _ese_uuid_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");                  // For garbage collection
        lua_pushcfunction(engine->runtime, _ese_uuid_lua_tostring);
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
        lua_pushcfunction(engine->runtime, _ese_uuid_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_setglobal(engine->runtime, "UUID");
    } else {
        lua_pop(engine->runtime, 1); // Pop the existing EseUUID table
    }
}

void ese_uuid_lua_push(EseUUID *uuid) {
    log_assert("UUID", uuid, "ese_uuid_lua_push called with NULL uuid");

    if (uuid->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseUUID **ud = (EseUUID **)lua_newuserdata(uuid->state, sizeof(EseUUID *));
        *ud = uuid;

        // Attach metatable
        luaL_getmetatable(uuid->state, UUID_PROXY_META);
        lua_setmetatable(uuid->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(uuid->state, LUA_REGISTRYINDEX, uuid->lua_ref);
    }
}

EseUUID *ese_uuid_lua_get(lua_State *L, int idx) {
    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseUUID **ud = (EseUUID **)luaL_testudata(L, idx, UUID_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_uuid_ref(EseUUID *uuid) {
    log_assert("UUID", uuid, "ese_uuid_ref called with NULL uuid");
    
    if (uuid->lua_ref == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseUUID **ud = (EseUUID **)lua_newuserdata(uuid->state, sizeof(EseUUID *));
        *ud = uuid;

        // Attach metatable
        luaL_getmetatable(uuid->state, UUID_PROXY_META);
        lua_setmetatable(uuid->state, -2);

        // Store hard reference to prevent garbage collection
        uuid->lua_ref = luaL_ref(uuid->state, LUA_REGISTRYINDEX);
        uuid->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        uuid->lua_ref_count++;
    }

    profile_count_add("ese_uuid_ref_count");
}

void ese_uuid_unref(EseUUID *uuid) {
    if (!uuid) return;
    
    if (uuid->lua_ref != LUA_NOREF && uuid->lua_ref_count > 0) {
        uuid->lua_ref_count--;
        
        if (uuid->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(uuid->state, LUA_REGISTRYINDEX, uuid->lua_ref);
            uuid->lua_ref = LUA_NOREF;
        }
    }

    profile_count_add("ese_uuid_unref_count");
}

// Utility functions
void ese_uuid_generate_new(EseUUID *uuid) {
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

uint64_t ese_uuid_hash(const EseUUID* uuid) {
    uint64_t hash = 5381;
    const char* str = uuid->value;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

// ========================================
// OPQUE ACCESSOR FUNCTIONS
// ========================================

size_t ese_uuid_sizeof(void) {
    return sizeof(struct EseUUID);
}

const char *ese_uuid_get_value(const EseUUID *uuid) {
    log_assert("UUID", uuid, "ese_uuid_get_value called with NULL uuid");
    return uuid->value;
}

lua_State *ese_uuid_get_state(const EseUUID *uuid) {
    log_assert("UUID", uuid, "ese_uuid_get_state called with NULL uuid");
    return uuid->state;
}

int ese_uuid_get_lua_ref(const EseUUID *uuid) {
    log_assert("UUID", uuid, "ese_uuid_get_lua_ref called with NULL uuid");
    return uuid->lua_ref;
}

int ese_uuid_get_lua_ref_count(const EseUUID *uuid) {
    log_assert("UUID", uuid, "ese_uuid_get_lua_ref_count called with NULL uuid");
    return uuid->lua_ref_count;
}
