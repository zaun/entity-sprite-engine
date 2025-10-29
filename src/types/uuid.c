/**
 * @file uuid.c
 * @brief Implementation of UUID type with string representation
 * @details Implements UUID generation, hashing, Lua integration, and JSON
 * serialization
 *
 * @copyright Copyright (c) 2024 ESE Project
 * @license See LICENSE.md for license information
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/uuid.h"
#include "types/uuid_lua.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"

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
    char value[37]; /** The string EseUUID */

    lua_State *state;  /** Lua State this EseUUID belongs to */
    int lua_ref;       /** Lua registry reference to its own proxy table */
    int lua_ref_count; /** Number of times this uuid has been referenced in C */
};

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers

// Private setters for Lua state management
static void _ese_uuid_set_lua_ref(EseUUID *uuid, int lua_ref);
static void _ese_uuid_set_lua_ref_count(EseUUID *uuid, int lua_ref_count);
static void _ese_uuid_set_state(EseUUID *uuid, lua_State *state);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseUUID instance with default values
 *
 * Allocates memory for a new EseUUID and initializes all fields to safe
 * defaults. The UUID starts with no Lua state or references, and a new random
 * UUID is generated.
 *
 * @return Pointer to the newly created EseUUID, or NULL on allocation failure
 */
EseUUID *_ese_uuid_make() {
    EseUUID *uuid = (EseUUID *)memory_manager.malloc(sizeof(EseUUID), MMTAG_UUID);
    uuid->state = NULL;
    uuid->lua_ref = LUA_NOREF;
    uuid->lua_ref_count = 0;
    ese_uuid_generate_new(uuid);
    return uuid;
}

// Private setters for Lua state management
static void _ese_uuid_set_lua_ref(EseUUID *uuid, int lua_ref) {
    if (uuid) {
        uuid->lua_ref = lua_ref;
    }
}

static void _ese_uuid_set_lua_ref_count(EseUUID *uuid, int lua_ref_count) {
    if (uuid) {
        uuid->lua_ref_count = lua_ref_count;
    }
}

static void _ese_uuid_set_state(EseUUID *uuid, lua_State *state) {
    if (uuid) {
        uuid->state = state;
    }
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseUUID *ese_uuid_create(EseLuaEngine *engine) {
    log_assert("UUID", engine, "ese_uuid_create called with NULL engine");

    EseUUID *uuid = _ese_uuid_make();
    _ese_uuid_set_state(uuid, engine->runtime);
    return uuid;
}

EseUUID *ese_uuid_copy(const EseUUID *source) {
    log_assert("UUID", source, "ese_uuid_copy called with NULL source");

    EseUUID *copy = (EseUUID *)memory_manager.malloc(sizeof(EseUUID), MMTAG_UUID);
    strcpy(copy->value, ese_uuid_get_value(source));
    _ese_uuid_set_state(copy, ese_uuid_get_state(source));
    _ese_uuid_set_lua_ref(copy, LUA_NOREF);
    _ese_uuid_set_lua_ref_count(copy, 0);
    return copy;
}

void ese_uuid_destroy(EseUUID *uuid) {
    if (!uuid)
        return;

    if (ese_uuid_get_lua_ref(uuid) == LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(uuid);
    } else {
        ese_uuid_unref(uuid);
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

// Lua integration
void ese_uuid_lua_init(EseLuaEngine *engine) { _ese_uuid_lua_init(engine); }

void ese_uuid_lua_push(EseUUID *uuid) {
    log_assert("UUID", uuid, "ese_uuid_lua_push called with NULL uuid");

    if (ese_uuid_get_lua_ref(uuid) == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseUUID **ud = (EseUUID **)lua_newuserdata(ese_uuid_get_state(uuid), sizeof(EseUUID *));
        *ud = uuid;

        // Attach metatable
        luaL_getmetatable(ese_uuid_get_state(uuid), UUID_PROXY_META);
        lua_setmetatable(ese_uuid_get_state(uuid), -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(ese_uuid_get_state(uuid), LUA_REGISTRYINDEX, ese_uuid_get_lua_ref(uuid));
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
        log_error("UUID", "UUID.fromJSON: failed to parse JSON string: %s",
                  json_str ? json_str : "NULL");
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

void ese_uuid_ref(EseUUID *uuid) {
    log_assert("UUID", uuid, "ese_uuid_ref called with NULL uuid");

    if (ese_uuid_get_lua_ref(uuid) == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseUUID **ud = (EseUUID **)lua_newuserdata(ese_uuid_get_state(uuid), sizeof(EseUUID *));
        *ud = uuid;

        // Attach metatable
        luaL_getmetatable(ese_uuid_get_state(uuid), UUID_PROXY_META);
        lua_setmetatable(ese_uuid_get_state(uuid), -2);

        // Store hard reference to prevent garbage collection
        int lua_ref = luaL_ref(ese_uuid_get_state(uuid), LUA_REGISTRYINDEX);
        _ese_uuid_set_lua_ref(uuid, lua_ref);
        _ese_uuid_set_lua_ref_count(uuid, 1);
    } else {
        // Already referenced - just increment count
        _ese_uuid_set_lua_ref_count(uuid, ese_uuid_get_lua_ref_count(uuid) + 1);
    }

    profile_count_add("ese_uuid_ref_count");
}

void ese_uuid_unref(EseUUID *uuid) {
    if (!uuid)
        return;

    if (ese_uuid_get_lua_ref(uuid) != LUA_NOREF && ese_uuid_get_lua_ref_count(uuid) > 0) {
        int new_count = ese_uuid_get_lua_ref_count(uuid) - 1;
        _ese_uuid_set_lua_ref_count(uuid, new_count);

        if (new_count == 0) {
            // No more references - remove from registry
            luaL_unref(ese_uuid_get_state(uuid), LUA_REGISTRYINDEX, ese_uuid_get_lua_ref(uuid));
            _ese_uuid_set_lua_ref(uuid, LUA_NOREF);
        }
    }

    profile_count_add("ese_uuid_unref_count");
}

// Utility functions
void ese_uuid_generate_new(EseUUID *uuid) {
    log_assert("UUID", uuid, "uuid_generate called with NULL uuid");

    unsigned char bytes[16]; // A EseUUID is 128 bits, which is 16 bytes

    // Fill the 16-byte buffer with cryptographically strong random data
    arc4random_buf(bytes, sizeof(bytes));

    // Set the EseUUID version (nibble 4) to 0100 (binary) = 4 (hex)
    bytes[6] = (bytes[6] & 0x0F) | 0x40; // Clear the top 4 bits and set them to 0100 (4)

    // Set the EseUUID variant (first two bits of nibble 13) to 10xx (binary)
    bytes[8] = (bytes[8] & 0x3F) | 0x80; // Clear the top 2 bits and set them to 10 (8, 9, A, B)

    // Format the bytes into the standard EseUUID string representation
    snprintf(uuid->value, sizeof(uuid->value),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", bytes[0],
             bytes[1], bytes[2], bytes[3], // 4 bytes = 8 hex digits
             bytes[4], bytes[5],           // 2 bytes = 4 hex digits
             bytes[6], bytes[7],           // 2 bytes = 4 hex digits (version byte set here)
             bytes[8], bytes[9],           // 2 bytes = 4 hex digits (variant byte set here)
             bytes[10], bytes[11], bytes[12], bytes[13], bytes[14],
             bytes[15]); // 6 bytes = 12 hex digits
}

uint64_t ese_uuid_hash(const EseUUID *uuid) {
    uint64_t hash = 5381;
    const char *str = uuid->value;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

// ========================================
// OPQUE ACCESSOR FUNCTIONS
// ========================================

size_t ese_uuid_sizeof(void) { return sizeof(struct EseUUID); }

/**
 * @brief Serializes an EseUUID to a cJSON object.
 *
 * Creates a cJSON object representing the uuid with type "UUID"
 * and value string. Only serializes the UUID string data,
 * not Lua-related fields.
 *
 * @param uuid Pointer to the EseUUID object to serialize
 * @return cJSON object representing the uuid, or NULL on failure
 */
cJSON *ese_uuid_serialize(const EseUUID *uuid) {
    log_assert("UUID", uuid, "ese_uuid_serialize called with NULL uuid");

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        log_error("UUID", "Failed to create cJSON object for uuid serialization");
        return NULL;
    }

    // Add type field
    cJSON *type = cJSON_CreateString("UUID");
    if (!type || !cJSON_AddItemToObject(json, "type", type)) {
        log_error("UUID", "Failed to add type field to uuid serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add value field
    cJSON *value = cJSON_CreateString(uuid->value);
    if (!value || !cJSON_AddItemToObject(json, "value", value)) {
        log_error("UUID", "Failed to add value field to uuid serialization");
        cJSON_Delete(json);
        return NULL;
    }

    return json;
}

/**
 * @brief Deserializes an EseUUID from a cJSON object.
 *
 * Creates a new EseUUID from a cJSON object with type "UUID"
 * and value string. The uuid is created with the specified engine
 * and must be explicitly referenced with ese_uuid_ref() if Lua access is
 * desired.
 *
 * @param engine EseLuaEngine pointer for uuid creation
 * @param data cJSON object containing uuid data
 * @return Pointer to newly created EseUUID object, or NULL on failure
 */
EseUUID *ese_uuid_deserialize(EseLuaEngine *engine, const cJSON *data) {
    log_assert("UUID", data, "ese_uuid_deserialize called with NULL data");

    if (!cJSON_IsObject(data)) {
        log_error("UUID", "UUID deserialization failed: data is not a JSON object");
        return NULL;
    }

    // Check type field
    cJSON *type_item = cJSON_GetObjectItem(data, "type");
    if (!type_item || !cJSON_IsString(type_item) || strcmp(type_item->valuestring, "UUID") != 0) {
        log_error("UUID", "UUID deserialization failed: invalid or missing type field");
        return NULL;
    }

    // Get value field
    cJSON *value_item = cJSON_GetObjectItem(data, "value");
    if (!value_item || !cJSON_IsString(value_item)) {
        log_error("UUID", "UUID deserialization failed: invalid or missing value field");
        return NULL;
    }

    // Validate UUID format (basic check for length)
    const char *uuid_str = value_item->valuestring;
    if (!uuid_str || strlen(uuid_str) != 36) {
        log_error("UUID", "UUID deserialization failed: invalid UUID format");
        return NULL;
    }

    // Create new uuid
    EseUUID *uuid = ese_uuid_create(engine);
    strcpy(uuid->value, uuid_str);

    return uuid;
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

void ese_uuid_set_state(EseUUID *uuid, lua_State *state) {
    if (uuid) {
        uuid->state = state;
    }
}
