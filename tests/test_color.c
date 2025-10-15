/*
* test_ese_color.c - Unity-based tests for color functionality
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

#include "../src/types/color.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"
#include "../src/scripting/lua_engine.h"
#include "../src/vendor/json/cJSON.h"

/**
* C API Test Functions Declarations
*/
static void test_ese_color_sizeof(void);
static void test_ese_color_create_requires_engine(void);
static void test_ese_color_create(void);
static void test_ese_color_r(void);
static void test_ese_color_g(void);
static void test_ese_color_b(void);
static void test_ese_color_a(void);
static void test_ese_color_ref(void);
static void test_ese_color_copy_requires_engine(void);
static void test_ese_color_copy(void);
static void test_ese_color_hex_conversion(void);
static void test_ese_color_byte_conversion(void);
static void test_ese_color_watcher_system(void);
static void test_ese_color_lua_integration(void);
static void test_ese_color_lua_init(void);
static void test_ese_color_lua_push(void);
static void test_ese_color_lua_get(void);
static void test_ese_color_serialization(void);
static void test_ese_color_lua_to_json(void);
static void test_ese_color_lua_from_json(void);
static void test_ese_color_json_round_trip(void);

/**
* Lua API Test Functions Declarations
*/
static void test_ese_color_lua_new(void);
static void test_ese_color_lua_white(void);
static void test_ese_color_lua_black(void);
static void test_ese_color_lua_red(void);
static void test_ese_color_lua_green(void);
static void test_ese_color_lua_blue(void);
static void test_ese_color_lua_set_hex(void);
static void test_ese_color_lua_set_byte(void);
static void test_ese_color_lua_r(void);
static void test_ese_color_lua_g(void);
static void test_ese_color_lua_b(void);
static void test_ese_color_lua_a(void);
static void test_ese_color_lua_tostring(void);
static void test_ese_color_lua_gc(void);

/**
* Mock watcher callback for testing
*/
static bool watcher_called = false;
static EseColor *last_watched_color = NULL;
static void *last_watcher_userdata = NULL;

static void test_watcher_callback(EseColor *color, void *userdata) {
    watcher_called = true;
    last_watched_color = color;
    last_watcher_userdata = userdata;
}

static void mock_reset(void) {
    watcher_called = false;
    last_watched_color = NULL;
    last_watcher_userdata = NULL;
}

/**
* Test suite setup and teardown
*/
static EseLuaEngine *g_engine = NULL;

void setUp(void) {
    g_engine = create_test_engine();
    ese_color_lua_init(g_engine);
}

void tearDown(void) {
    lua_engine_destroy(g_engine);
}

/**
* Main test runner
*/
int main(void) {
    log_init();

    printf("\nEseColor Tests\n");
    printf("--------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_ese_color_sizeof);
    RUN_TEST(test_ese_color_create_requires_engine);
    RUN_TEST(test_ese_color_create);
    RUN_TEST(test_ese_color_r);
    RUN_TEST(test_ese_color_g);
    RUN_TEST(test_ese_color_b);
    RUN_TEST(test_ese_color_a);
    RUN_TEST(test_ese_color_ref);
    RUN_TEST(test_ese_color_copy_requires_engine);
    RUN_TEST(test_ese_color_copy);
    RUN_TEST(test_ese_color_hex_conversion);
    RUN_TEST(test_ese_color_byte_conversion);
    RUN_TEST(test_ese_color_watcher_system);
    RUN_TEST(test_ese_color_lua_integration);
    RUN_TEST(test_ese_color_lua_init);
    RUN_TEST(test_ese_color_lua_push);
    RUN_TEST(test_ese_color_lua_get);
    RUN_TEST(test_ese_color_serialization);
    RUN_TEST(test_ese_color_lua_to_json);
    RUN_TEST(test_ese_color_lua_from_json);
    RUN_TEST(test_ese_color_json_round_trip);

    RUN_TEST(test_ese_color_lua_new);
    RUN_TEST(test_ese_color_lua_white);
    RUN_TEST(test_ese_color_lua_black);
    RUN_TEST(test_ese_color_lua_red);
    RUN_TEST(test_ese_color_lua_green);
    RUN_TEST(test_ese_color_lua_blue);
    RUN_TEST(test_ese_color_lua_set_hex);
    RUN_TEST(test_ese_color_lua_set_byte);
    RUN_TEST(test_ese_color_lua_r);
    RUN_TEST(test_ese_color_lua_g);
    RUN_TEST(test_ese_color_lua_b);
    RUN_TEST(test_ese_color_lua_a);
    RUN_TEST(test_ese_color_lua_tostring);
    RUN_TEST(test_ese_color_lua_gc);

    memory_manager.destroy();

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_ese_color_sizeof(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, ese_color_sizeof(), "Color size should be > 0");
}

static void test_ese_color_create_requires_engine(void) {
    ASSERT_DEATH(ese_color_create(NULL), "ese_color_create should abort with NULL engine");
}

static void test_ese_color_create(void) {
    EseColor *color = ese_color_create(g_engine);

    TEST_ASSERT_NOT_NULL_MESSAGE(color, "Color should be created");
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_color_get_r(color));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_color_get_g(color));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_color_get_b(color));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, ese_color_get_a(color));
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_color_get_state(color), "Color should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_color_get_lua_ref_count(color), "New color should have ref count 0");

    ese_color_destroy(color);
}

static void test_ese_color_r(void) {
    EseColor *color = ese_color_create(g_engine);

    ese_color_set_r(color, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, ese_color_get_r(color));

    ese_color_set_r(color, -0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.5f, ese_color_get_r(color));

    ese_color_set_r(color, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_r(color));

    ese_color_destroy(color);
}

static void test_ese_color_g(void) {
    EseColor *color = ese_color_create(g_engine);

    ese_color_set_g(color, 0.3f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.3f, ese_color_get_g(color));

    ese_color_set_g(color, -0.3f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.3f, ese_color_get_g(color));

    ese_color_set_g(color, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_g(color));

    ese_color_destroy(color);
}

static void test_ese_color_b(void) {
    EseColor *color = ese_color_create(g_engine);

    ese_color_set_b(color, 0.7f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.7f, ese_color_get_b(color));

    ese_color_set_b(color, -0.7f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.7f, ese_color_get_b(color));

    ese_color_set_b(color, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_b(color));

    ese_color_destroy(color);
}

static void test_ese_color_a(void) {
    EseColor *color = ese_color_create(g_engine);

    ese_color_set_a(color, 0.8f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, ese_color_get_a(color));

    ese_color_set_a(color, -0.8f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.8f, ese_color_get_a(color));

    ese_color_set_a(color, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_a(color));

    ese_color_destroy(color);
}

static void test_ese_color_ref(void) {
    EseColor *color = ese_color_create(g_engine);

    ese_color_ref(color);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, ese_color_get_lua_ref_count(color), "Ref count should be 1");

    ese_color_unref(color);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_color_get_lua_ref_count(color), "Ref count should be 0");

    ese_color_destroy(color);
}

static void test_ese_color_copy_requires_engine(void) {
    ASSERT_DEATH(ese_color_copy(NULL), "ese_color_copy should abort with NULL color");
}

static void test_ese_color_copy(void) {
    EseColor *color = ese_color_create(g_engine);
    ese_color_ref(color);
    ese_color_set_r(color, 0.5f);
    ese_color_set_g(color, 0.25f);
    ese_color_set_b(color, 0.75f);
    ese_color_set_a(color, 0.8f);
    EseColor *copy = ese_color_copy(color);

    TEST_ASSERT_NOT_NULL_MESSAGE(copy, "Copy should be created");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_color_get_state(copy), "Copy should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_color_get_lua_ref_count(copy), "Copy should have ref count 0");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, ese_color_get_r(copy));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.25f, ese_color_get_g(copy));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.75f, ese_color_get_b(copy));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, ese_color_get_a(copy));

    ese_color_unref(color);
    ese_color_destroy(color);
    ese_color_destroy(copy);
}

static void test_ese_color_hex_conversion(void) {
    EseColor *color = ese_color_create(g_engine);

    // Test #RGB format
    bool success = ese_color_set_hex(color, "#F0A");
    TEST_ASSERT_TRUE_MESSAGE(success, "Should successfully parse #F0A");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_r(color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_g(color));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.67f, ese_color_get_b(color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_a(color));

    // Test #RRGGBB format
    success = ese_color_set_hex(color, "#FF0000");
    TEST_ASSERT_TRUE_MESSAGE(success, "Should successfully parse #FF0000");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_r(color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_g(color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_b(color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_a(color));

    // Test #RRGGBBAA format
    success = ese_color_set_hex(color, "#FF000080");
    TEST_ASSERT_TRUE_MESSAGE(success, "Should successfully parse #FF000080");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_r(color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_g(color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_b(color));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, ese_color_get_a(color));

    // Test invalid formats
    success = ese_color_set_hex(color, "invalid");
    TEST_ASSERT_FALSE_MESSAGE(success, "Should fail to parse invalid format");

    success = ese_color_set_hex(color, "#GG");
    TEST_ASSERT_FALSE_MESSAGE(success, "Should fail to parse invalid hex characters");

    success = ese_color_set_hex(color, "#");
    TEST_ASSERT_FALSE_MESSAGE(success, "Should fail to parse incomplete hex string");

    ese_color_destroy(color);
}

static void test_ese_color_byte_conversion(void) {
    EseColor *color = ese_color_create(g_engine);

    // Test setting from byte values
    ese_color_set_byte(color, 255, 128, 64, 192);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_r(color));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.502f, ese_color_get_g(color));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.251f, ese_color_get_b(color));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.753f, ese_color_get_a(color));

    // Test getting byte values
    unsigned char r, g, b, a;
    ese_color_get_byte(color, &r, &g, &b, &a);
    TEST_ASSERT_EQUAL(255, r);
    TEST_ASSERT_EQUAL(128, g);
    TEST_ASSERT_EQUAL(64, b);
    TEST_ASSERT_EQUAL(192, a);

    // Test edge cases
    ese_color_set_byte(color, 0, 0, 0, 0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_r(color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_g(color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_b(color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_a(color));

    ese_color_destroy(color);
}

static void test_ese_color_watcher_system(void) {
    EseColor *color = ese_color_create(g_engine);

    mock_reset();
    ese_color_set_r(color, 0.5f);
    TEST_ASSERT_FALSE_MESSAGE(watcher_called, "Watcher should not be called before adding");

    void *test_userdata = (void*)0x12345678;
    bool add_result = ese_color_add_watcher(color, test_watcher_callback, test_userdata);
    TEST_ASSERT_TRUE_MESSAGE(add_result, "Should successfully add watcher");

    mock_reset();
    ese_color_set_r(color, 0.7f);
    TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when r changes");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(color, last_watched_color, "Watcher should receive correct color pointer");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(test_userdata, last_watcher_userdata, "Watcher should receive correct userdata");

    mock_reset();
    ese_color_set_g(color, 0.3f);
    TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when g changes");

    mock_reset();
    ese_color_set_b(color, 0.8f);
    TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when b changes");

    mock_reset();
    ese_color_set_a(color, 0.9f);
    TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when a changes");

    bool remove_result = ese_color_remove_watcher(color, test_watcher_callback, test_userdata);
    TEST_ASSERT_TRUE_MESSAGE(remove_result, "Should successfully remove watcher");

    mock_reset();
    ese_color_set_r(color, 1.0f);
    TEST_ASSERT_FALSE_MESSAGE(watcher_called, "Watcher should not be called after removal");

    ese_color_destroy(color);
}

static void test_ese_color_lua_integration(void) {
    EseLuaEngine *engine = create_test_engine();
    EseColor *color = ese_color_create(engine);

    lua_State *before_state = ese_color_get_state(color);
    TEST_ASSERT_NOT_NULL_MESSAGE(before_state, "Color should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, before_state, "Color state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_color_get_lua_ref(color), "Color should have no Lua reference initially");

    ese_color_ref(color);
    lua_State *after_ref_state = ese_color_get_state(color);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_ref_state, "Color should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_ref_state, "Color state should match engine runtime");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(LUA_NOREF, ese_color_get_lua_ref(color), "Color should have a valid Lua reference after ref");

    ese_color_unref(color);
    lua_State *after_unref_state = ese_color_get_state(color);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_unref_state, "Color should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_unref_state, "Color state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_color_get_lua_ref(color), "Color should have no Lua reference after unref");

    ese_color_destroy(color);
    lua_engine_destroy(engine);
}

static void test_ese_color_lua_init(void) {
    lua_State *L = g_engine->runtime;
    
    // Since setUp already calls ese_color_lua_init, we just verify the metatable exists
    luaL_getmetatable(L, "ColorMeta");
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Metatable should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Metatable should be a table");
    lua_pop(L, 1);
    
    lua_getglobal(L, "Color");
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Global Color table should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Global Color table should be a table");
    lua_pop(L, 1);
}

static void test_ese_color_lua_push(void) {
    ese_color_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseColor *color = ese_color_create(g_engine);
    
    ese_color_lua_push(color);
    
    EseColor **ud = (EseColor **)lua_touserdata(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(color, *ud, "The pushed item should be the actual color");
    
    lua_pop(L, 1); 
    
    ese_color_destroy(color);
}

static void test_ese_color_lua_get(void) {
    ese_color_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseColor *color = ese_color_create(g_engine);
    
    ese_color_lua_push(color);
    
    EseColor *extracted_color = ese_color_lua_get(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(color, extracted_color, "Extracted color should match original");
    
    lua_pop(L, 1);
    ese_color_destroy(color);
}

/**
* Lua API Test Functions
*/

static void test_ese_color_lua_new(void) {
    ese_color_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "return Color.new()\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "Color.new() should error (requires 3 or 4 numbers)");

    const char *testB = "return Color.new(0.5)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "Color.new(r) should error (requires 3 or 4 numbers)");

    const char *testC = "return Color.new(0.1, 0.2, 0.3, 0.4)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute without error");
    EseColor *extracted_color = ese_color_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_color, "Extracted color should not be NULL");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, ese_color_get_r(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.2f, ese_color_get_g(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.3f, ese_color_get_b(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.4f, ese_color_get_a(extracted_color));
    lua_pop(L, 1);

    const char *testD = "return Color.new(0.1, 0.2, 0.3)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testD), "testC Lua code should execute without error");
    extracted_color = ese_color_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_color, "Extracted color should not be NULL");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, ese_color_get_r(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.2f, ese_color_get_g(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.3f, ese_color_get_b(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_a(extracted_color));
    lua_pop(L, 1);

    const char *testE = "return Color.new(\"0.5\", \"0.6\")\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testE), "string args should error (numbers required)");
}

static void test_ese_color_lua_white(void) {
    ese_color_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code = "return Color.white()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "white() should execute without error");
    EseColor *extracted_color = ese_color_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_color, "Extracted color should not be NULL");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_r(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_g(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_b(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_a(extracted_color));
    lua_pop(L, 1);
}

static void test_ese_color_lua_black(void) {
    ese_color_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code = "return Color.black()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "black() should execute without error");
    EseColor *extracted_color = ese_color_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_color, "Extracted color should not be NULL");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_r(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_g(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_b(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_a(extracted_color));
    lua_pop(L, 1);
}

static void test_ese_color_lua_red(void) {
    ese_color_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code = "return Color.red()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "red() should execute without error");
    EseColor *extracted_color = ese_color_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_color, "Extracted color should not be NULL");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_r(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_g(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_b(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_a(extracted_color));
    lua_pop(L, 1);
}

static void test_ese_color_lua_green(void) {
    ese_color_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code = "return Color.green()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "green() should execute without error");
    EseColor *extracted_color = ese_color_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_color, "Extracted color should not be NULL");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_r(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_g(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_b(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_a(extracted_color));
    lua_pop(L, 1);
}

static void test_ese_color_lua_blue(void) {
    ese_color_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code = "return Color.blue()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "blue() should execute without error");
    EseColor *extracted_color = ese_color_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_color, "Extracted color should not be NULL");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_r(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_color_get_g(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_b(extracted_color));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_color_get_a(extracted_color));
    lua_pop(L, 1);
}

static void test_ese_color_lua_set_hex(void) {
    ese_color_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code2 = "local c = Color.new(0, 0, 0); c:set_hex(\"#FF0000\"); return c.r\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code2), "set_hex should execute without error");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, lua_tonumber(L, -1));
    lua_pop(L, 1);

    const char *test_error = "local c = Color.new(0, 0, 0); c:set_hex(\"invalid\"); return c\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_error), "set_hex with invalid string should error");
}

static void test_ese_color_lua_set_byte(void) {
    ese_color_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code = "local c = Color.new(0, 0, 0); c:set_byte(255, 128, 64, 192); return c.r\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "set_byte should execute without error");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, lua_tonumber(L, -1));
    lua_pop(L, 1);

    const char *test_error = "local c = Color.new(0, 0, 0); c:set_byte(255, 128); return c\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_error), "set_byte with wrong number of args should error");
}

static void test_ese_color_lua_r(void) {
    ese_color_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local c = Color.new(0, 0, 0, 0); c.r = 0.5; return c.r";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua r set/get test 1 should execute without error");
    double r = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, r);
    lua_pop(L, 1);

    const char *test2 = "local c = Color.new(0, 0, 0, 0); c.r = -0.5; return c.r";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua r set/get test 2 should execute without error");
    r = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.5f, r);
    lua_pop(L, 1);

    const char *test3 = "local c = Color.new(0, 0, 0, 0); c.r = \"0.5\"; return c.r";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua r set with string should error");
}

static void test_ese_color_lua_g(void) {
    ese_color_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local c = Color.new(0, 0, 0, 0); c.g = 0.3; return c.g";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua g set/get test 1 should execute without error");
    double g = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.3f, g);
    lua_pop(L, 1);

    const char *test2 = "local c = Color.new(0, 0, 0, 0); c.g = -0.3; return c.g";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua g set/get test 2 should execute without error");
    g = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.3f, g);
    lua_pop(L, 1);

    const char *test3 = "local c = Color.new(0, 0, 0, 0); c.g = \"0.3\"; return c.g";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua g set with string should error");
}

static void test_ese_color_lua_b(void) {
    ese_color_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local c = Color.new(0, 0, 0, 0); c.b = 0.7; return c.b";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua b set/get test 1 should execute without error");
    double b = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.7f, b);
    lua_pop(L, 1);

    const char *test2 = "local c = Color.new(0, 0, 0, 0); c.b = -0.7; return c.b";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua b set/get test 2 should execute without error");
    b = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.7f, b);
    lua_pop(L, 1);

    const char *test3 = "local c = Color.new(0, 0, 0, 0); c.b = \"0.7\"; return c.b";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua b set with string should error");
}

static void test_ese_color_lua_a(void) {
    ese_color_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local c = Color.new(0, 0, 0, 0); c.a = 0.8; return c.a";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua a set/get test 1 should execute without error");
    double a = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, a);
    lua_pop(L, 1);

    const char *test2 = "local c = Color.new(0, 0, 0, 0); c.a = -0.8; return c.a";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua a set/get test 2 should execute without error");
    a = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.8f, a);
    lua_pop(L, 1);

    const char *test3 = "local c = Color.new(0, 0, 0, 0); c.a = \"0.8\"; return c.a";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua a set with string should error");
}

static void test_ese_color_lua_tostring(void) {
    ese_color_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test_code = "local c = Color.new(0.5, 0.25, 0.75, 0.8); return tostring(c)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "tostring test should execute without error");
    const char *result = lua_tostring(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "tostring result should not be NULL");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "Color:") != NULL, "tostring should contain 'Color:'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "r=0.50") != NULL, "tostring should contain 'r=0.50'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "g=0.25") != NULL, "tostring should contain 'g=0.25'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "b=0.75") != NULL, "tostring should contain 'b=0.75'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "a=0.80") != NULL, "tostring should contain 'a=0.80'");
    lua_pop(L, 1); 
}

static void test_ese_color_lua_gc(void) {
    ese_color_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *testA = "local c = Color.new(0.5, 0.25, 0.75, 0.8)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "Color creation should execute without error");
    
    int collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testB = "return Color.new(0.5, 0.25, 0.75, 0.8)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "Color creation should execute without error");
    EseColor *extracted_color = ese_color_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_color, "Extracted color should not be NULL");
    ese_color_ref(extracted_color);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    ese_color_unref(extracted_color);

    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testC = "return Color.new(0.5, 0.25, 0.75, 0.8)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "Color creation should execute without error");
    extracted_color = ese_color_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_color, "Extracted color should not be NULL");
    ese_color_ref(extracted_color);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    ese_color_unref(extracted_color);
    ese_color_destroy(extracted_color);

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
* Tests for color serialization/deserialization functionality
*/
static void test_ese_color_serialization(void) {
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    // Create a test color
    EseColor *original = ese_color_create(engine);
    TEST_ASSERT_NOT_NULL(original);

    ese_color_set_r(original, 0.5f);
    ese_color_set_g(original, 0.25f);
    ese_color_set_b(original, 0.75f);
    ese_color_set_a(original, 0.8f);

    // Test serialization
    cJSON *json = ese_color_serialize(original);
    TEST_ASSERT_NOT_NULL(json);

    // Verify JSON structure
    cJSON *type_item = cJSON_GetObjectItem(json, "type");
    TEST_ASSERT_NOT_NULL(type_item);
    TEST_ASSERT_TRUE(cJSON_IsString(type_item));
    TEST_ASSERT_EQUAL_STRING("COLOR", type_item->valuestring);

    cJSON *r_item = cJSON_GetObjectItem(json, "r");
    TEST_ASSERT_NOT_NULL(r_item);
    TEST_ASSERT_TRUE(cJSON_IsNumber(r_item));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.5, r_item->valuedouble);

    cJSON *g_item = cJSON_GetObjectItem(json, "g");
    TEST_ASSERT_NOT_NULL(g_item);
    TEST_ASSERT_TRUE(cJSON_IsNumber(g_item));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.25, g_item->valuedouble);

    cJSON *b_item = cJSON_GetObjectItem(json, "b");
    TEST_ASSERT_NOT_NULL(b_item);
    TEST_ASSERT_TRUE(cJSON_IsNumber(b_item));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.75, b_item->valuedouble);

    cJSON *a_item = cJSON_GetObjectItem(json, "a");
    TEST_ASSERT_NOT_NULL(a_item);
    TEST_ASSERT_TRUE(cJSON_IsNumber(a_item));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.8, a_item->valuedouble);

    // Test deserialization
    EseColor *deserialized = ese_color_deserialize(engine, json);
    TEST_ASSERT_NOT_NULL(deserialized);

    // Verify all components match
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, ese_color_get_r(deserialized));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.25f, ese_color_get_g(deserialized));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.75f, ese_color_get_b(deserialized));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, ese_color_get_a(deserialized));

    // Clean up
    cJSON_Delete(json);
    ese_color_destroy(original);
    ese_color_destroy(deserialized);
    lua_engine_destroy(engine);
}

/**
* Test Color:toJSON Lua instance method
*/
static void test_ese_color_lua_to_json(void) {
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    ese_color_lua_init(engine);
    lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);

    const char *testA = "local c = Color.new(0.5, 0.25, 0.75, 0.8) "
                       "local json = c:toJSON() "
                       "if json == nil or json == '' then error('toJSON should return non-empty string') end "
                       "if not string.find(json, '\"type\":\"COLOR\"') then error('toJSON should return valid JSON') end ";

    int result = luaL_dostring(engine->runtime, testA);
    if (result != LUA_OK) {
        const char *error_msg = lua_tostring(engine->runtime, -1);
        printf("ERROR in color toJSON test: %s\n", error_msg ? error_msg : "unknown error");
        lua_pop(engine->runtime, 1);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, result, "Color:toJSON should create valid JSON");

    lua_engine_destroy(engine);
}

/**
* Test Color.fromJSON Lua static method
*/
static void test_ese_color_lua_from_json(void) {
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    ese_color_lua_init(engine);
    lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);

    const char *testA = "local json_str = '{\\\"type\\\":\\\"COLOR\\\",\\\"r\\\":0.5,\\\"g\\\":0.25,\\\"b\\\":0.75,\\\"a\\\":0.8}' "
                       "local c = Color.fromJSON(json_str) "
                       "if c == nil then error('Color.fromJSON should return a color') end "
                       "if math.abs(c.r - 0.5) > 0.001 then error('Color fromJSON should set correct r') end "
                       "if math.abs(c.g - 0.25) > 0.001 then error('Color fromJSON should set correct g') end "
                       "if math.abs(c.b - 0.75) > 0.001 then error('Color fromJSON should set correct b') end "
                       "if math.abs(c.a - 0.8) > 0.001 then error('Color fromJSON should set correct a') end ";

    int resultA = luaL_dostring(engine->runtime, testA);
    if (resultA != LUA_OK) {
        const char *error_msg = lua_tostring(engine->runtime, -1);
        printf("ERROR in color fromJSON test: %s\n", error_msg ? error_msg : "unknown error");
        lua_pop(engine->runtime, 1);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, resultA, "Color.fromJSON should work with valid JSON");

    const char *testB = "local c = Color.fromJSON('invalid json')";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(engine->runtime, testB), "Color.fromJSON should fail with invalid JSON");

    lua_engine_destroy(engine);
}

/**
* Test Color JSON round-trip (toJSON -> fromJSON)
*/
static void test_ese_color_json_round_trip(void) {
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    ese_color_lua_init(engine);
    lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);

    const char *testA = "local original = Color.new(0.5, 0.25, 0.75, 0.8) "
                       "local json = original:toJSON() "
                       "local restored = Color.fromJSON(json) "
                       "if not restored then error('Color.fromJSON should return a color') end "
                       "if math.abs(restored.r - original.r) > 0.001 then error('Round-trip should preserve r') end "
                       "if math.abs(restored.g - original.g) > 0.001 then error('Round-trip should preserve g') end "
                       "if math.abs(restored.b - original.b) > 0.001 then error('Round-trip should preserve b') end "
                       "if math.abs(restored.a - original.a) > 0.001 then error('Round-trip should preserve a') end ";

    int result = luaL_dostring(engine->runtime, testA);
    if (result != LUA_OK) {
        const char *error_msg = lua_tostring(engine->runtime, -1);
        printf("ERROR in color round-trip test: %s\n", error_msg ? error_msg : "unknown error");
        lua_pop(engine->runtime, 1);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, result, "Color JSON round-trip should work correctly");

    lua_engine_destroy(engine);
}
