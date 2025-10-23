/*
* test_ese_mapcell.c - Unity-based tests for map cell functionality
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

#include "../src/types/map.h"
#include "../src/types/map_cell.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

/**
* C API Test Functions Declarations
*/
static void test_ese_map_cell_sizeof(void);
static void test_ese_map_cell_create_requires_engine(void);
static void test_ese_map_cell_create(void);
static void test_ese_map_cell_copy_requires_source(void);
static void test_ese_map_cell_copy(void);
static void test_ese_map_cell_layers(void);
static void test_ese_map_cell_flags(void);
static void test_ese_map_cell_properties(void);
static void test_ese_map_cell_ref(void);
static void test_ese_map_cell_lua_integration(void);
static void test_ese_map_cell_lua_init(void);
static void test_ese_map_cell_lua_push(void);
static void test_ese_map_cell_lua_get(void);

/**
* Lua API Test Functions Declarations
*/
static void test_ese_map_cell_lua_properties(void);
static void test_ese_map_cell_lua_methods(void);
static void test_ese_map_cell_lua_tostring(void);
static void test_ese_map_cell_lua_gc(void);

/**
* Test suite setup and teardown
*/
static EseLuaEngine *g_engine = NULL;
static EseMap *g_map = NULL;

void setUp(void) {
    g_engine = create_test_engine();
    g_map = ese_map_create(g_engine, 10, 10, MAP_TYPE_GRID, false);
}

void tearDown(void) {
    lua_engine_destroy(g_engine);
    ese_map_destroy(g_map);
}

/**
* Main test runner
*/
int main(void) {
    log_init();

    printf("\nEseMapCell Tests\n");
    printf("----------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_ese_map_cell_sizeof);
    RUN_TEST(test_ese_map_cell_create_requires_engine);
    RUN_TEST(test_ese_map_cell_create);
    RUN_TEST(test_ese_map_cell_copy_requires_source);
    RUN_TEST(test_ese_map_cell_copy);
    RUN_TEST(test_ese_map_cell_layers);
    RUN_TEST(test_ese_map_cell_flags);
    RUN_TEST(test_ese_map_cell_properties);
    RUN_TEST(test_ese_map_cell_ref);
    RUN_TEST(test_ese_map_cell_lua_integration);
    RUN_TEST(test_ese_map_cell_lua_init);
    RUN_TEST(test_ese_map_cell_lua_push);
    RUN_TEST(test_ese_map_cell_lua_get);

    RUN_TEST(test_ese_map_cell_lua_properties);
    RUN_TEST(test_ese_map_cell_lua_methods);
    RUN_TEST(test_ese_map_cell_lua_tostring);
    RUN_TEST(test_ese_map_cell_lua_gc);

    memory_manager.destroy();

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_ese_map_cell_sizeof(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, ese_map_cell_sizeof(), "MapCell size should be > 0");
}

static void test_ese_map_cell_create_requires_engine(void) {
    TEST_ASSERT_DEATH(ese_map_cell_create(NULL, NULL), "ese_map_cell_create should abort with NULL engine");
}

static void test_ese_map_cell_create(void) {
    EseMapCell *cell = ese_map_cell_create(g_engine, g_map);

    TEST_ASSERT_NOT_NULL_MESSAGE(cell, "MapCell should be created");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_map_cell_get_state(cell), "MapCell should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_map_cell_get_lua_ref_count(cell), "New map cell should have ref count 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_map_cell_get_lua_ref(cell), "New map cell should have LUA_NOREF");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, ese_map_cell_get_layer_count(cell), "New map cell should have 0 layers");
    TEST_ASSERT_FALSE_MESSAGE(ese_map_cell_get_is_dynamic(cell), "New map cell should not be dynamic");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, ese_map_cell_get_flags(cell), "New map cell should have 0 flags");
    TEST_ASSERT_FALSE_MESSAGE(ese_map_cell_has_layers(cell), "New map cell should not have layers");

    ese_map_cell_destroy(cell);
}

static void test_ese_map_cell_copy_requires_source(void) {
    TEST_ASSERT_DEATH(ese_map_cell_copy(NULL), "ese_map_cell_copy should abort with NULL source");
}

static void test_ese_map_cell_copy(void) {
    EseMapCell *original = ese_map_cell_create(g_engine, g_map);
    ese_map_cell_ref(original);
    ese_map_cell_add_layer(original, 5);
    ese_map_cell_add_layer(original, 10);
    ese_map_cell_set_is_dynamic(original, true);
    ese_map_cell_set_flags(original, 0x42);

    EseMapCell *copy = ese_map_cell_copy(original);

    TEST_ASSERT_NOT_NULL_MESSAGE(copy, "Copy should be created");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_map_cell_get_state(copy), "Copy should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_map_cell_get_lua_ref_count(copy), "Copy should have ref count 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_map_cell_get_lua_ref(copy), "Copy should have LUA_NOREF");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(2, ese_map_cell_get_layer_count(copy), "Copy should have same layer count");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(5, ese_map_cell_get_layer(copy, 0), "Copy should have same first layer");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(10, ese_map_cell_get_layer(copy, 1), "Copy should have same second layer");
    TEST_ASSERT_TRUE_MESSAGE(ese_map_cell_get_is_dynamic(copy), "Copy should have same isDynamic value");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0x42, ese_map_cell_get_flags(copy), "Copy should have same flags value");

    ese_map_cell_unref(original);
    ese_map_cell_destroy(original);
    ese_map_cell_destroy(copy);
}

static void test_ese_map_cell_layers(void) {
    EseMapCell *cell = ese_map_cell_create(g_engine, g_map);

    // Test adding layers
    TEST_ASSERT_TRUE_MESSAGE(ese_map_cell_add_layer(cell, 1), "Should add first layer");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1, ese_map_cell_get_layer_count(cell), "Should have 1 layer");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1, ese_map_cell_get_layer(cell, 0), "First layer should be 1");
    TEST_ASSERT_TRUE_MESSAGE(ese_map_cell_has_layers(cell), "Should have layers");

    TEST_ASSERT_TRUE_MESSAGE(ese_map_cell_add_layer(cell, 2), "Should add second layer");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(2, ese_map_cell_get_layer_count(cell), "Should have 2 layers");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(2, ese_map_cell_get_layer(cell, 1), "Second layer should be 2");

    // Test setting layers
    TEST_ASSERT_TRUE_MESSAGE(ese_map_cell_set_layer(cell, 0, 10), "Should set first layer");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(10, ese_map_cell_get_layer(cell, 0), "First layer should be 10");

    TEST_ASSERT_TRUE_MESSAGE(ese_map_cell_set_layer(cell, 1, 20), "Should set second layer");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(20, ese_map_cell_get_layer(cell, 1), "Second layer should be 20");

    // Test out of bounds access
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, ese_map_cell_get_layer(cell, 2), "Out of bounds should return 0");
    TEST_ASSERT_FALSE_MESSAGE(ese_map_cell_set_layer(cell, 2, 30), "Out of bounds set should fail");

    // Test removing layers
    TEST_ASSERT_TRUE_MESSAGE(ese_map_cell_remove_layer(cell, 0), "Should remove first layer");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1, ese_map_cell_get_layer_count(cell), "Should have 1 layer after removal");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(20, ese_map_cell_get_layer(cell, 0), "Remaining layer should be 20");

    TEST_ASSERT_FALSE_MESSAGE(ese_map_cell_remove_layer(cell, 1), "Out of bounds removal should fail");

    // Test clearing layers
    ese_map_cell_clear_layers(cell);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, ese_map_cell_get_layer_count(cell), "Should have 0 layers after clear");
    TEST_ASSERT_FALSE_MESSAGE(ese_map_cell_has_layers(cell), "Should not have layers after clear");

    ese_map_cell_destroy(cell);
}

static void test_ese_map_cell_flags(void) {
    EseMapCell *cell = ese_map_cell_create(g_engine, g_map);

    // Test initial state
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, ese_map_cell_get_flags(cell), "Initial flags should be 0");
    TEST_ASSERT_FALSE_MESSAGE(ese_map_cell_has_flag(cell, 0x01), "Should not have flag 0x01");

    // Test setting flags
    ese_map_cell_set_flag(cell, 0x01);
    TEST_ASSERT_TRUE_MESSAGE(ese_map_cell_has_flag(cell, 0x01), "Should have flag 0x01");
    TEST_ASSERT_FALSE_MESSAGE(ese_map_cell_has_flag(cell, 0x02), "Should not have flag 0x02");

    ese_map_cell_set_flag(cell, 0x04);
    TEST_ASSERT_TRUE_MESSAGE(ese_map_cell_has_flag(cell, 0x01), "Should still have flag 0x01");
    TEST_ASSERT_TRUE_MESSAGE(ese_map_cell_has_flag(cell, 0x04), "Should have flag 0x04");
    TEST_ASSERT_FALSE_MESSAGE(ese_map_cell_has_flag(cell, 0x02), "Should not have flag 0x02");

    // Test clearing flags
    ese_map_cell_clear_flag(cell, 0x01);
    TEST_ASSERT_FALSE_MESSAGE(ese_map_cell_has_flag(cell, 0x01), "Should not have flag 0x01 after clear");
    TEST_ASSERT_TRUE_MESSAGE(ese_map_cell_has_flag(cell, 0x04), "Should still have flag 0x04");

    // Test setting all flags at once
    ese_map_cell_set_flags(cell, 0xFF);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0xFF, ese_map_cell_get_flags(cell), "Should have all flags set");
    TEST_ASSERT_TRUE_MESSAGE(ese_map_cell_has_flag(cell, 0x01), "Should have flag 0x01");
    TEST_ASSERT_TRUE_MESSAGE(ese_map_cell_has_flag(cell, 0x80), "Should have flag 0x80");

    ese_map_cell_destroy(cell);
}

static void test_ese_map_cell_properties(void) {
    EseMapCell *cell = ese_map_cell_create(g_engine, g_map);

    // Test isDynamic property
    TEST_ASSERT_FALSE_MESSAGE(ese_map_cell_get_is_dynamic(cell), "Initial isDynamic should be false");
    
    ese_map_cell_set_is_dynamic(cell, true);
    TEST_ASSERT_TRUE_MESSAGE(ese_map_cell_get_is_dynamic(cell), "isDynamic should be true after set");
    
    ese_map_cell_set_is_dynamic(cell, false);
    TEST_ASSERT_FALSE_MESSAGE(ese_map_cell_get_is_dynamic(cell), "isDynamic should be false after set");

    // Test flags property
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, ese_map_cell_get_flags(cell), "Initial flags should be 0");
    
    ese_map_cell_set_flags(cell, 0x12345678);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0x12345678, ese_map_cell_get_flags(cell), "Flags should be set correctly");

    ese_map_cell_destroy(cell);
}

static void test_ese_map_cell_ref(void) {
    EseMapCell *cell = ese_map_cell_create(g_engine, g_map);

    // Test initial state
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_map_cell_get_lua_ref_count(cell), "Initial ref count should be 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_map_cell_get_lua_ref(cell), "Initial lua_ref should be LUA_NOREF");

    // Test ref/unref
    ese_map_cell_ref(cell);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, ese_map_cell_get_lua_ref_count(cell), "Ref count should be 1 after ref");
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_map_cell_get_lua_ref(cell), "lua_ref should not be LUA_NOREF after ref");

    ese_map_cell_ref(cell);
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, ese_map_cell_get_lua_ref_count(cell), "Ref count should be 2 after second ref");

    ese_map_cell_unref(cell);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, ese_map_cell_get_lua_ref_count(cell), "Ref count should be 1 after unref");

    ese_map_cell_unref(cell);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_map_cell_get_lua_ref_count(cell), "Ref count should be 0 after second unref");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_map_cell_get_lua_ref(cell), "lua_ref should be LUA_NOREF after unref");

    ese_map_cell_destroy(cell);
}

static void test_ese_map_cell_lua_integration(void) {
    EseLuaEngine *engine = create_test_engine();
    EseMapCell *cell = ese_map_cell_create(engine, g_map);

    lua_State *before_state = ese_map_cell_get_state(cell);
    TEST_ASSERT_NOT_NULL_MESSAGE(before_state, "MapCell should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, before_state, "MapCell state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_map_cell_get_lua_ref(cell), "MapCell should have no Lua reference initially");

    ese_map_cell_ref(cell);
    lua_State *after_ref_state = ese_map_cell_get_state(cell);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_ref_state, "MapCell should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_ref_state, "MapCell state should match engine runtime");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(LUA_NOREF, ese_map_cell_get_lua_ref(cell), "MapCell should have a valid Lua reference after ref");

    ese_map_cell_unref(cell);
    lua_State *after_unref_state = ese_map_cell_get_state(cell);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_unref_state, "MapCell should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_unref_state, "MapCell state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_map_cell_get_lua_ref(cell), "MapCell should have no Lua reference after unref");

    ese_map_cell_destroy(cell);
    lua_engine_destroy(engine);
}

static void test_ese_map_cell_lua_init(void) {
    lua_State *L = g_engine->runtime;
    
    luaL_getmetatable(L, MAP_CELL_PROXY_META);
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Metatable should not exist before initialization");
    lua_pop(L, 1);
        
    ese_map_cell_lua_init(g_engine);
    
    luaL_getmetatable(L, MAP_CELL_PROXY_META);
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Metatable should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Metatable should be a table");
    lua_pop(L, 1);
    
    lua_getglobal(L, "MapCell");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Global MapCell table should NOT exist after initialization");
    lua_pop(L, 1);
}

static void test_ese_map_cell_lua_push(void) {
    ese_map_cell_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseMapCell *cell = ese_map_cell_create(g_engine, g_map);
    
    ese_map_cell_lua_push(cell);
    
    EseMapCell **ud = (EseMapCell **)lua_touserdata(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(cell, *ud, "The pushed item should be the actual map cell");
    
    lua_pop(L, 1); 
    
    ese_map_cell_destroy(cell);
}

static void test_ese_map_cell_lua_get(void) {
    ese_map_cell_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseMapCell *cell = ese_map_cell_create(g_engine, g_map);
    
    ese_map_cell_lua_push(cell);
    
    EseMapCell *extracted_cell = ese_map_cell_lua_get(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(cell, extracted_cell, "Extracted map cell should match original");
    
    lua_pop(L, 1);
    ese_map_cell_destroy(cell);
}

/**
* Lua API Test Functions
*/

static void test_ese_map_cell_lua_properties(void) {
    ese_map_cell_lua_init(g_engine);
    ese_map_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    // Test isDynamic property
    const char *test1 = "local map = Map.new(1,1); local mc = map:get_cell(0,0); mc.isDynamic = \"true\"; return mc.isDynamic";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "isDynamic string assignment should fail");

    const char *test2 = "local map = Map.new(1,1); local mc = map:get_cell(0,0); mc.isDynamic = true; return mc.isDynamic";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "isDynamic boolean assignment should work");
    bool isDynamic = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(isDynamic, "isDynamic should be true");
    lua_pop(L, 1);

    const char *test3 = "local map = Map.new(1,1); local mc = map:get_cell(0,0); mc.isDynamic = false; return mc.isDynamic";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "isDynamic false assignment should work");
    isDynamic = lua_toboolean(L, -1);
    TEST_ASSERT_FALSE_MESSAGE(isDynamic, "isDynamic should be false");
    lua_pop(L, 1);

    // Test flags property
    const char *test4 = "local map = Map.new(1,1); local mc = map:get_cell(0,0); mc.flags = \"42\"; return mc.flags";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test4), "flags string assignment should fail");

    const char *test5 = "local map = Map.new(1,1); local mc = map:get_cell(0,0); mc.flags = 42; return mc.flags";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test5), "flags number assignment should work");
    double flags = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(42.0, flags, "flags should be 42");
    lua_pop(L, 1);

    // Test layer_count property (read-only)
    const char *test6 = "local map = Map.new(1,1); local mc = map:get_cell(0,0); return mc.layer_count";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test6), "layer_count read should work");
    double layer_count = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0, layer_count, "layer_count should be 0");
    lua_pop(L, 1);
}

static void test_ese_map_cell_lua_methods(void) {
    ese_map_cell_lua_init(g_engine);
    ese_map_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    // Test add_layer method
    const char *test1 = "local map = Map.new(1,1); local mc = map:get_cell(0,0); mc:add_layer(5); return mc.layer_count";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "add_layer should work");
    double layer_count = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0, layer_count, "layer_count should be 1 after add_layer");
    lua_pop(L, 1);

    // Test get_layer method
    const char *test2 = "local map = Map.new(1,1); local mc = map:get_cell(0,0); mc:add_layer(10); mc:add_layer(20); return mc:get_layer(0), mc:get_layer(1)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "get_layer should work");
    double layer0 = lua_tonumber(L, -2);
    double layer1 = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0, layer0, "first layer should be 10");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(20.0, layer1, "second layer should be 20");
    lua_pop(L, 2);

    // Test set_layer method
    const char *test3 = "local map = Map.new(1,1); local mc = map:get_cell(0,0); mc:add_layer(5); mc:set_layer(0, 15); return mc:get_layer(0)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "set_layer should work");
    double layer = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(15.0, layer, "layer should be 15 after set_layer");
    lua_pop(L, 1);

    // Test remove_layer method
    const char *test4 = "local map = Map.new(1,1); local mc = map:get_cell(0,0); mc:add_layer(10); mc:add_layer(20); mc:remove_layer(0); return mc.layer_count, mc:get_layer(0)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test4), "remove_layer should work");
    double count = lua_tonumber(L, -2);
    double remaining = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0, count, "layer_count should be 1 after remove_layer");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(20.0, remaining, "remaining layer should be 20");
    lua_pop(L, 2);

    // Test clear_layers method
    const char *test5 = "local map = Map.new(1,1); local mc = map:get_cell(0,0); mc:add_layer(10); mc:add_layer(20); mc:clear_layers(); return mc.layer_count";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test5), "clear_layers should work");
    double cleared_count = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0, cleared_count, "layer_count should be 0 after clear_layers");
    lua_pop(L, 1);

    // Test flag methods
    const char *test6 = "local map = Map.new(1,1); local mc = map:get_cell(0,0); mc:set_flag(1); return mc:has_flag(1), mc:has_flag(2)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test6), "flag methods should work");
    bool has_flag1 = lua_toboolean(L, -2);
    bool has_flag2 = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(has_flag1, "should have flag 1");
    TEST_ASSERT_FALSE_MESSAGE(has_flag2, "should not have flag 2");
    lua_pop(L, 2);

    // Test clear_flag method
    const char *test7 = "local map = Map.new(1,1); local mc = map:get_cell(0,0); mc:set_flag(1); mc:clear_flag(1); return mc:has_flag(1)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test7), "clear_flag should work");
    bool has_flag = lua_toboolean(L, -1);
    TEST_ASSERT_FALSE_MESSAGE(has_flag, "should not have flag 1 after clear");
    lua_pop(L, 1);
}

static void test_ese_map_cell_lua_tostring(void) {
    ese_map_cell_lua_init(g_engine);
    ese_map_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test_code = "local map = Map.new(1,1); local mc = map:get_cell(0,0); mc:add_layer(5); mc:set_flag(1); mc.isDynamic = true; return tostring(mc)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "tostring test should execute without error");
    const char *result = lua_tostring(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "tostring result should not be NULL");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "MapCell:") != NULL, "tostring should contain 'MapCell:'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "layers=1") != NULL, "tostring should contain 'layers=1'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "flags=1") != NULL, "tostring should contain 'flags=1'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "dynamic=1") != NULL, "tostring should contain 'dynamic=1'");
    lua_pop(L, 1);
}

static void test_ese_map_cell_lua_gc(void) {
    ese_map_cell_lua_init(g_engine);
    ese_map_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *testA = "local map = Map.new(1,1); local mc = map:get_cell(0,0)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "MapCell creation should execute without error");
    
    int collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testB = "local map = Map.new(1,1); return map:get_cell(0,0)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "MapCell creation should execute without error");
    EseMapCell *extracted_cell = ese_map_cell_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_cell, "Extracted map cell should not be NULL");
    ese_map_cell_ref(extracted_cell);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should not collect referenced cell");

    ese_map_cell_unref(extracted_cell);

    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testC = "local map = Map.new(1,1); return map:get_cell(0,0)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "MapCell creation should execute without error");
    extracted_cell = ese_map_cell_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_cell, "Extracted map cell should not be NULL");
    ese_map_cell_ref(extracted_cell);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should not collect referenced cell");

    ese_map_cell_unref(extracted_cell);
    ese_map_cell_destroy(extracted_cell);

    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should not collect referenced cell");

    // Verify GC didn't crash by running another operation
    const char *verify_code = "return 42";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, verify_code), "Lua should still work after GC");
    int result = (int)lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(42, result, "Lua should return correct value after GC");
    lua_pop(L, 1);
}