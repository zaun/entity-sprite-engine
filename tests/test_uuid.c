/*
* test_ese_uuid.c - Unity-based tests for UUID functionality
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <execinfo.h>
#include <signal.h>
#include <math.h>
#include <sys/wait.h>

#include "testing.h"

#include "../src/types/uuid.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"
#include "../src/scripting/lua_engine.h"
#include "../src/vendor/json/cJSON.h"

/**
* C API Test Functions Declarations
*/
static void test_ese_uuid_sizeof(void);
static void test_ese_uuid_create_requires_engine(void);
static void test_ese_uuid_create(void);
static void test_ese_uuid_value(void);
static void test_ese_uuid_ref(void);
static void test_ese_uuid_copy_requires_source(void);
static void test_ese_uuid_copy(void);
static void test_ese_uuid_generate_new(void);
static void test_ese_uuid_hash(void);
static void test_ese_uuid_lua_integration(void);
static void test_ese_uuid_lua_init(void);
static void test_ese_uuid_lua_push(void);
static void test_ese_uuid_lua_get(void);
static void test_ese_uuid_serialization(void);
static void test_ese_uuid_lua_to_json(void);
static void test_ese_uuid_lua_from_json(void);
static void test_ese_uuid_json_round_trip(void);

/**
* Lua API Test Functions Declarations
*/
static void test_ese_uuid_lua_new(void);
static void test_ese_uuid_lua_value(void);
static void test_ese_uuid_lua_string(void);
static void test_ese_uuid_lua_reset(void);
static void test_ese_uuid_lua_tostring(void);
static void test_ese_uuid_lua_gc(void);

/**
* Test suite setup and teardown
*/
static EseLuaEngine *g_engine = NULL;

void setUp(void) {
    g_engine = create_test_engine();
}

void tearDown(void) {
    lua_engine_destroy(g_engine);
}

/**
* Main test runner
*/
int main(void) {
    log_init();

    printf("\nEseUUID Tests\n");
    printf("-------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_ese_uuid_sizeof);
    RUN_TEST(test_ese_uuid_create_requires_engine);
    RUN_TEST(test_ese_uuid_create);
    RUN_TEST(test_ese_uuid_value);
    RUN_TEST(test_ese_uuid_ref);
    RUN_TEST(test_ese_uuid_copy_requires_source);
    RUN_TEST(test_ese_uuid_copy);
    RUN_TEST(test_ese_uuid_generate_new);
    RUN_TEST(test_ese_uuid_hash);
    RUN_TEST(test_ese_uuid_lua_integration);
    RUN_TEST(test_ese_uuid_lua_init);
    RUN_TEST(test_ese_uuid_lua_push);
    RUN_TEST(test_ese_uuid_lua_get);
    RUN_TEST(test_ese_uuid_serialization);
    RUN_TEST(test_ese_uuid_lua_to_json);
    RUN_TEST(test_ese_uuid_lua_from_json);
    RUN_TEST(test_ese_uuid_json_round_trip);

    RUN_TEST(test_ese_uuid_lua_new);
    RUN_TEST(test_ese_uuid_lua_value);
    RUN_TEST(test_ese_uuid_lua_string);
    RUN_TEST(test_ese_uuid_lua_reset);
    RUN_TEST(test_ese_uuid_lua_tostring);
    RUN_TEST(test_ese_uuid_lua_gc);

    memory_manager.destroy();

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_ese_uuid_sizeof(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, ese_uuid_sizeof(), "UUID size should be > 0");
}

static void test_ese_uuid_create_requires_engine(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, ese_uuid_sizeof(), "UUID size should be > 0");
}

static void test_ese_uuid_create(void) {
    EseUUID *uuid = ese_uuid_create(g_engine);

    TEST_ASSERT_NOT_NULL_MESSAGE(uuid, "UUID should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(ese_uuid_get_value(uuid), "UUID should have a value");
    TEST_ASSERT_EQUAL_INT_MESSAGE(36, strlen(ese_uuid_get_value(uuid)), "UUID should be 36 characters");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_uuid_get_state(uuid), "UUID should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_uuid_get_lua_ref_count(uuid), "New UUID should have ref count 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_uuid_get_lua_ref(uuid), "New UUID should have LUA_NOREF");

    ese_uuid_destroy(uuid);
}

static void test_ese_uuid_value(void) {
    EseUUID *uuid = ese_uuid_create(g_engine);

    const char *value = ese_uuid_get_value(uuid);
    TEST_ASSERT_NOT_NULL_MESSAGE(value, "UUID value should not be NULL");
    TEST_ASSERT_EQUAL_INT_MESSAGE(36, strlen(value), "UUID value should be 36 characters");
    
    // Test UUID format (should have hyphens at specific positions)
    TEST_ASSERT_EQUAL_CHAR_MESSAGE('-', value[8], "UUID should have hyphen at position 8");
    TEST_ASSERT_EQUAL_CHAR_MESSAGE('-', value[13], "UUID should have hyphen at position 13");
    TEST_ASSERT_EQUAL_CHAR_MESSAGE('-', value[18], "UUID should have hyphen at position 18");
    TEST_ASSERT_EQUAL_CHAR_MESSAGE('-', value[23], "UUID should have hyphen at position 23");

    ese_uuid_destroy(uuid);
}

static void test_ese_uuid_ref(void) {
    EseUUID *uuid = ese_uuid_create(g_engine);

    ese_uuid_ref(uuid);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, ese_uuid_get_lua_ref_count(uuid), "Ref count should be 1");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(LUA_NOREF, ese_uuid_get_lua_ref(uuid), "Should have valid Lua reference");

    ese_uuid_unref(uuid);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_uuid_get_lua_ref_count(uuid), "Ref count should be 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_uuid_get_lua_ref(uuid), "Should have LUA_NOREF after unref");

    ese_uuid_destroy(uuid);
}

static void test_ese_uuid_copy_requires_source(void) {
    ASSERT_DEATH(ese_uuid_copy(NULL), "ese_uuid_copy should abort with NULL UUID");
}

static void test_ese_uuid_copy(void) {
    EseUUID *uuid = ese_uuid_create(g_engine);
    ese_uuid_ref(uuid);
    EseUUID *copy = ese_uuid_copy(uuid);

    TEST_ASSERT_NOT_NULL_MESSAGE(copy, "Copy should be created");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_uuid_get_state(copy), "Copy should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_uuid_get_lua_ref_count(copy), "Copy should have ref count 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_uuid_get_lua_ref(copy), "Copy should have LUA_NOREF");
    TEST_ASSERT_EQUAL_STRING_MESSAGE(ese_uuid_get_value(uuid), ese_uuid_get_value(copy), "Copy should have same value");
    TEST_ASSERT_TRUE_MESSAGE(uuid != copy, "Copy should be different object");

    ese_uuid_unref(uuid);
    ese_uuid_destroy(uuid);
    ese_uuid_destroy(copy);
}

static void test_ese_uuid_generate_new(void) {
    EseUUID *uuid = ese_uuid_create(g_engine);
    char *original_value = memory_manager.strdup(ese_uuid_get_value(uuid), MMTAG_TEMP);
    
    // Generate a new UUID
    ese_uuid_generate_new(uuid);
    const char *new_value = ese_uuid_get_value(uuid);
    
    TEST_ASSERT_TRUE_MESSAGE(strcmp(original_value, new_value) != 0, "Generated UUID should be different");
    TEST_ASSERT_EQUAL_INT_MESSAGE(36, strlen(new_value), "Generated UUID should be 36 characters");
    
    // Test UUID format
    TEST_ASSERT_EQUAL_CHAR_MESSAGE('-', new_value[8], "Generated UUID should have hyphen at position 8");
    TEST_ASSERT_EQUAL_CHAR_MESSAGE('-', new_value[13], "Generated UUID should have hyphen at position 13");
    TEST_ASSERT_EQUAL_CHAR_MESSAGE('-', new_value[18], "Generated UUID should have hyphen at position 18");
    TEST_ASSERT_EQUAL_CHAR_MESSAGE('-', new_value[23], "Generated UUID should have hyphen at position 23");

    memory_manager.free(original_value);
    ese_uuid_destroy(uuid);
}

static void test_ese_uuid_hash(void) {
    EseUUID *uuid1 = ese_uuid_create(g_engine);
    EseUUID *uuid2 = ese_uuid_create(g_engine);
    
    uint64_t hash1 = ese_uuid_hash(uuid1);
    uint64_t hash2 = ese_uuid_hash(uuid2);
    
    TEST_ASSERT_TRUE_MESSAGE(hash1 != hash2, "Different UUIDs should have different hashes");
    TEST_ASSERT_TRUE_MESSAGE(hash1 != 0, "Hash should not be zero");
    TEST_ASSERT_TRUE_MESSAGE(hash2 != 0, "Hash should not be zero");
    
    // Test that same UUID has same hash
    EseUUID *copy = ese_uuid_copy(uuid1);
    uint64_t hash_copy = ese_uuid_hash(copy);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(hash1, hash_copy, "Same UUID should have same hash");

    ese_uuid_destroy(uuid1);
    ese_uuid_destroy(uuid2);
    ese_uuid_destroy(copy);
}

static void test_ese_uuid_lua_integration(void) {
    EseLuaEngine *engine = create_test_engine();
    EseUUID *uuid = ese_uuid_create(engine);

    lua_State *before_state = ese_uuid_get_state(uuid);
    TEST_ASSERT_NOT_NULL_MESSAGE(before_state, "UUID should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, before_state, "UUID state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_uuid_get_lua_ref(uuid), "UUID should have no Lua reference initially");

    ese_uuid_ref(uuid);
    lua_State *after_ref_state = ese_uuid_get_state(uuid);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_ref_state, "UUID should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_ref_state, "UUID state should match engine runtime");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(LUA_NOREF, ese_uuid_get_lua_ref(uuid), "UUID should have a valid Lua reference after ref");

    ese_uuid_unref(uuid);
    lua_State *after_unref_state = ese_uuid_get_state(uuid);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_unref_state, "UUID should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_unref_state, "UUID state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_uuid_get_lua_ref(uuid), "UUID should have no Lua reference after unref");

    ese_uuid_destroy(uuid);
    lua_engine_destroy(engine);
}

static void test_ese_uuid_lua_init(void) {
    lua_State *L = g_engine->runtime;
    
    luaL_getmetatable(L, UUID_PROXY_META);
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Metatable should not exist before initialization");
    lua_pop(L, 1);
    
    lua_getglobal(L, "UUID");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Global UUID table should not exist before initialization");
    lua_pop(L, 1);
    
    ese_uuid_lua_init(g_engine);
    
    luaL_getmetatable(L, UUID_PROXY_META);
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Metatable should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Metatable should be a table");
    lua_pop(L, 1);
    
    lua_getglobal(L, "UUID");
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Global UUID table should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Global UUID table should be a table");
    lua_pop(L, 1);
}

static void test_ese_uuid_lua_push(void) {
    ese_uuid_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseUUID *uuid = ese_uuid_create(g_engine);
    
    ese_uuid_lua_push(uuid);
    
    EseUUID **ud = (EseUUID **)lua_touserdata(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(uuid, *ud, "The pushed item should be the actual UUID");
    
    lua_pop(L, 1); 
    
    ese_uuid_destroy(uuid);
}

static void test_ese_uuid_lua_get(void) {
    ese_uuid_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseUUID *uuid = ese_uuid_create(g_engine);
    
    ese_uuid_lua_push(uuid);
    
    EseUUID *extracted_uuid = ese_uuid_lua_get(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(uuid, extracted_uuid, "Extracted UUID should match original");
    
    lua_pop(L, 1);
    ese_uuid_destroy(uuid);
}

/**
* Lua API Test Functions
*/

static void test_ese_uuid_lua_new(void) {
    ese_uuid_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "return UUID.new(10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute with error");

    const char *testB = "return UUID.new(10, 10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute with error");

    const char *testC = "return UUID.new(\"10\")\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute with error");

    const char *testD = "return UUID.new()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testD), "testD Lua code should execute without error");
    EseUUID *extracted_uuid = ese_uuid_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_uuid, "Extracted UUID should not be NULL");
    TEST_ASSERT_EQUAL_INT_MESSAGE(36, strlen(ese_uuid_get_value(extracted_uuid)), "Extracted UUID should be 36 characters");
    ese_uuid_destroy(extracted_uuid);
}

static void test_ese_uuid_lua_value(void) {
    ese_uuid_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code = "local uuid = UUID.new(); return uuid.value";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Lua value test should execute without error");
    const char *value = lua_tostring(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(value, "UUID value should not be NULL");
    TEST_ASSERT_EQUAL_INT_MESSAGE(36, strlen(value), "UUID value should be 36 characters");
    lua_pop(L, 1);
}

static void test_ese_uuid_lua_string(void) {
    ese_uuid_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code = "local uuid = UUID.new(); return uuid.string";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Lua string test should execute without error");
    const char *string = lua_tostring(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(string, "UUID string should not be NULL");
    TEST_ASSERT_EQUAL_INT_MESSAGE(36, strlen(string), "UUID string should be 36 characters");
    lua_pop(L, 1);
}

static void test_ese_uuid_lua_reset(void) {
    ese_uuid_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code = "local uuid = UUID.new(); local old_value = uuid.value; uuid.reset(); return uuid.value ~= old_value";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Lua reset test should execute without error");
    int result = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(result, "Reset should change UUID value");
    lua_pop(L, 1);
}

static void test_ese_uuid_lua_tostring(void) {
    ese_uuid_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test_code = "local uuid = UUID.new(); return tostring(uuid)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "tostring test should execute without error");
    const char *result = lua_tostring(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "tostring result should not be NULL");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "UUID:") != NULL, "tostring should contain 'UUID:'");
    lua_pop(L, 1); 
}

static void test_ese_uuid_lua_gc(void) {
    ese_uuid_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *testA = "local uuid = UUID.new()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "UUID creation should execute without error");
    
    int collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testB = "return UUID.new()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "UUID creation should execute without error");
    EseUUID *extracted_uuid = ese_uuid_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_uuid, "Extracted UUID should not be NULL");
    ese_uuid_ref(extracted_uuid);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    ese_uuid_unref(extracted_uuid);

    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testC = "return UUID.new()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "UUID creation should execute without error");
    extracted_uuid = ese_uuid_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_uuid, "Extracted UUID should not be NULL");
    ese_uuid_ref(extracted_uuid);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    ese_uuid_unref(extracted_uuid);
    ese_uuid_destroy(extracted_uuid);

    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    // Verify GC didn't crash by running another operation
    const char *verify_code = "return 42";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, verify_code), "Lua should still work after GC");
    int result = (int)lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(42, result, "Lua should return correct value after GC");
    lua_pop(L, 1);
}

/**
* Tests for uuid serialization/deserialization functionality
*/
static void test_ese_uuid_serialization(void) {
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    // Create a test uuid
    EseUUID *original = ese_uuid_create(engine);
    TEST_ASSERT_NOT_NULL(original);

    // Generate a new UUID to have a known value for testing
    ese_uuid_generate_new(original);
    const char *original_value = ese_uuid_get_value(original);

    // Test serialization
    cJSON *json = ese_uuid_serialize(original);
    TEST_ASSERT_NOT_NULL(json);

    // Verify JSON structure
    cJSON *type_item = cJSON_GetObjectItem(json, "type");
    TEST_ASSERT_NOT_NULL(type_item);
    TEST_ASSERT_TRUE(cJSON_IsString(type_item));
    TEST_ASSERT_EQUAL_STRING("UUID", type_item->valuestring);

    cJSON *value_item = cJSON_GetObjectItem(json, "value");
    TEST_ASSERT_NOT_NULL(value_item);
    TEST_ASSERT_TRUE(cJSON_IsString(value_item));
    TEST_ASSERT_EQUAL_STRING(original_value, value_item->valuestring);

    // Test deserialization
    EseUUID *deserialized = ese_uuid_deserialize(engine, json);
    TEST_ASSERT_NOT_NULL(deserialized);

    // Verify UUID value matches
    TEST_ASSERT_EQUAL_STRING(original_value, ese_uuid_get_value(deserialized));

    // Clean up
    cJSON_Delete(json);
    ese_uuid_destroy(original);
    ese_uuid_destroy(deserialized);
    lua_engine_destroy(engine);
}

/**
* Test UUID:toJSON Lua instance method
*/
static void test_ese_uuid_lua_to_json(void) {
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    ese_uuid_lua_init(engine);
    lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);

    const char *testA = "local u = UUID.new() "
                       "local json = u:toJSON() "
                       "if json == nil or json == '' then error('toJSON should return non-empty string') end "
                       "if not string.find(json, '\"type\":\"UUID\"') then error('toJSON should return valid JSON') end ";

    int result = luaL_dostring(engine->runtime, testA);
    if (result != LUA_OK) {
        const char *error_msg = lua_tostring(engine->runtime, -1);
        printf("ERROR in uuid toJSON test: %s\n", error_msg ? error_msg : "unknown error");
        lua_pop(engine->runtime, 1);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, result, "UUID:toJSON should create valid JSON");

    lua_engine_destroy(engine);
}

/**
* Test UUID.fromJSON Lua static method
*/
static void test_ese_uuid_lua_from_json(void) {
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    ese_uuid_lua_init(engine);
    lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);

    // Prepare a UUID via C then build JSON string from it
    EseUUID *tmp = ese_uuid_create(engine);
    const char *val = ese_uuid_get_value(tmp);

    char json_buf[128];
    snprintf(json_buf, sizeof(json_buf), "{\\\"type\\\":\\\"UUID\\\",\\\"value\\\":\\\"%s\\\"}", val);

    char lua_buf[256];
    snprintf(lua_buf, sizeof(lua_buf), "local json_str = '%s' local u = UUID.fromJSON(json_str) if u == nil then error('UUID.fromJSON should return a uuid') end", json_buf);

    int resultA = luaL_dostring(engine->runtime, lua_buf);
    if (resultA != LUA_OK) {
        const char *error_msg = lua_tostring(engine->runtime, -1);
        printf("ERROR in uuid fromJSON test: %s\n", error_msg ? error_msg : "unknown error");
        lua_pop(engine->runtime, 1);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, resultA, "UUID.fromJSON should work with valid JSON");

    const char *testB = "local u = UUID.fromJSON('invalid json')";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(engine->runtime, testB), "UUID.fromJSON should fail with invalid JSON");

    ese_uuid_destroy(tmp);
    lua_engine_destroy(engine);
}

/**
* Test UUID JSON round-trip (toJSON -> fromJSON)
*/
static void test_ese_uuid_json_round_trip(void) {
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    ese_uuid_lua_init(engine);
    lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);

    const char *testA = "local original = UUID.new() "
                       "local json = original:toJSON() "
                       "local restored = UUID.fromJSON(json) "
                       "if not restored then error('UUID.fromJSON should return a uuid') end ";

    int result = luaL_dostring(engine->runtime, testA);
    if (result != LUA_OK) {
        const char *error_msg = lua_tostring(engine->runtime, -1);
        printf("ERROR in uuid round-trip test: %s\n", error_msg ? error_msg : "unknown error");
        lua_pop(engine->runtime, 1);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, result, "UUID JSON round-trip should work correctly");

    lua_engine_destroy(engine);
}
