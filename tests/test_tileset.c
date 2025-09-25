/*
* test_ese_tileset.c - Unity-based tests for tileset functionality
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

#include "../src/types/tileset.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

/**
* C API Test Functions Declarations
*/
static void test_ese_tileset_sizeof(void);
static void test_ese_tileset_create_requires_engine(void);
static void test_ese_tileset_create(void);
static void test_ese_tileset_ref(void);
static void test_ese_tileset_copy_requires_engine(void);
static void test_ese_tileset_copy(void);
static void test_ese_tileset_add_sprite(void);
static void test_ese_tileset_remove_sprite(void);
static void test_ese_tileset_get_sprite(void);
static void test_ese_tileset_clear_mapping(void);
static void test_ese_tileset_get_sprite_count(void);
static void test_ese_tileset_update_sprite_weight(void);
static void test_ese_tileset_set_seed(void);
static void test_ese_tileset_get_sprite_random(void);
static void test_ese_tileset_lua_integration(void);
static void test_ese_tileset_lua_init(void);
static void test_ese_tileset_lua_push(void);
static void test_ese_tileset_lua_get(void);

/**
* Lua API Test Functions Declarations
*/
static void test_ese_tileset_lua_new(void);
static void test_ese_tileset_lua_add_sprite(void);
static void test_ese_tileset_lua_remove_sprite(void);
static void test_ese_tileset_lua_get_sprite(void);
static void test_ese_tileset_lua_clear_mapping(void);
static void test_ese_tileset_lua_get_sprite_count(void);
static void test_ese_tileset_lua_update_sprite_weight(void);
static void test_ese_tileset_lua_get_sprite_random(void);
static void test_ese_tileset_lua_tostring(void);
static void test_ese_tileset_lua_gc(void);

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

    printf("\nEseTileSet Tests\n");
    printf("----------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_ese_tileset_sizeof);
    RUN_TEST(test_ese_tileset_create_requires_engine);
    RUN_TEST(test_ese_tileset_create);
    RUN_TEST(test_ese_tileset_copy_requires_engine);
    RUN_TEST(test_ese_tileset_copy);
    RUN_TEST(test_ese_tileset_add_sprite);
    RUN_TEST(test_ese_tileset_remove_sprite);
    RUN_TEST(test_ese_tileset_get_sprite);
    RUN_TEST(test_ese_tileset_clear_mapping);
    RUN_TEST(test_ese_tileset_get_sprite_count);
    RUN_TEST(test_ese_tileset_update_sprite_weight);
    RUN_TEST(test_ese_tileset_set_seed);
    RUN_TEST(test_ese_tileset_get_sprite_random);

    memory_manager.destroy();

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_ese_tileset_sizeof(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, ese_tileset_sizeof(), "Tileset size should be > 0");
}

static void test_ese_tileset_create_requires_engine(void) {
    ASSERT_DEATH(ese_tileset_create(NULL), "ese_tileset_create should abort with NULL engine");
}

static void test_ese_tileset_create(void) {
    EseTileSet *tileset = ese_tileset_create(g_engine);

    TEST_ASSERT_NOT_NULL_MESSAGE(tileset, "TileSet should be created");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_tileset_get_state(tileset), "TileSet should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_tileset_get_lua_ref(tileset), "New tileset should have LUA_NOREF");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_tileset_get_lua_ref_count(tileset), "New tileset should have ref count 0");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, ese_tileset_get_rng_seed(tileset), "Initial seed should be 0");

    // Test that all mappings are empty
    for (int i = 0; i < 256; i++) {
        TEST_ASSERT_EQUAL_UINT_MESSAGE(0, ese_tileset_get_sprite_count(tileset, i), "All mappings should be empty initially");
    }

    ese_tileset_destroy(tileset);
}


static void test_ese_tileset_copy_requires_engine(void) {
    ASSERT_DEATH(ese_tileset_copy(NULL), "ese_tileset_copy should abort with NULL tileset");
}

static void test_ese_tileset_copy(void) {
    EseTileSet *tileset = ese_tileset_create(g_engine);
    ese_tileset_add_sprite(tileset, 1, "grass", 10);
    ese_tileset_add_sprite(tileset, 1, "stone", 20);
    ese_tileset_set_seed(tileset, 12345);
    EseTileSet *copy = ese_tileset_copy(tileset);

    TEST_ASSERT_NOT_NULL_MESSAGE(copy, "Copy should be created");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_tileset_get_state(copy), "Copy should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_tileset_get_lua_ref_count(copy), "Copy should have ref count 0");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(12345, ese_tileset_get_rng_seed(copy), "Copy should have same seed");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(2, ese_tileset_get_sprite_count(copy, 1), "Copy should have same sprite count");

    ese_tileset_destroy(tileset);
    ese_tileset_destroy(copy);
}

static void test_ese_tileset_add_sprite(void) {
    EseTileSet *tileset = ese_tileset_create(g_engine);

    // Test adding first sprite
    TEST_ASSERT_TRUE_MESSAGE(ese_tileset_add_sprite(tileset, 1, "grass", 10), "Should add first sprite");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1, ese_tileset_get_sprite_count(tileset, 1), "Should have 1 sprite");

    // Test adding second sprite
    TEST_ASSERT_TRUE_MESSAGE(ese_tileset_add_sprite(tileset, 1, "stone", 20), "Should add second sprite");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(2, ese_tileset_get_sprite_count(tileset, 1), "Should have 2 sprites");

    // Test adding to different tile
    TEST_ASSERT_TRUE_MESSAGE(ese_tileset_add_sprite(tileset, 2, "water", 5), "Should add sprite to different tile");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1, ese_tileset_get_sprite_count(tileset, 2), "Different tile should have 1 sprite");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(2, ese_tileset_get_sprite_count(tileset, 1), "Original tile should still have 2 sprites");

    // Test adding duplicate sprite (should update weight)
    TEST_ASSERT_TRUE_MESSAGE(ese_tileset_add_sprite(tileset, 1, "grass", 15), "Should update duplicate sprite weight");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(2, ese_tileset_get_sprite_count(tileset, 1), "Should still have 2 sprites after update");

    // Test adding with zero weight (should fail)
    TEST_ASSERT_FALSE_MESSAGE(ese_tileset_add_sprite(tileset, 1, "dirt", 0), "Should not add sprite with zero weight");

    // Test adding with NULL sprite_id (should fail)
    TEST_ASSERT_FALSE_MESSAGE(ese_tileset_add_sprite(tileset, 1, NULL, 10), "Should not add sprite with NULL sprite_id");

    // Test adding with NULL tileset (should fail)
    TEST_ASSERT_FALSE_MESSAGE(ese_tileset_add_sprite(NULL, 1, "test", 10), "Should not add sprite with NULL tileset");

    ese_tileset_destroy(tileset);
}

static void test_ese_tileset_remove_sprite(void) {
    EseTileSet *tileset = ese_tileset_create(g_engine);

    // Add some sprites first
    ese_tileset_add_sprite(tileset, 1, "grass", 10);
    ese_tileset_add_sprite(tileset, 1, "stone", 20);
    ese_tileset_add_sprite(tileset, 1, "dirt", 5);

    // Test removing existing sprite
    TEST_ASSERT_TRUE_MESSAGE(ese_tileset_remove_sprite(tileset, 1, "stone"), "Should remove existing sprite");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(2, ese_tileset_get_sprite_count(tileset, 1), "Should have 2 sprites after removal");

    // Test removing non-existent sprite
    TEST_ASSERT_FALSE_MESSAGE(ese_tileset_remove_sprite(tileset, 1, "nonexistent"), "Should not remove non-existent sprite");

    // Test removing from empty tile
    TEST_ASSERT_FALSE_MESSAGE(ese_tileset_remove_sprite(tileset, 2, "grass"), "Should not remove from empty tile");

    // Test removing with NULL sprite_id (should fail)
    TEST_ASSERT_FALSE_MESSAGE(ese_tileset_remove_sprite(tileset, 1, NULL), "Should not remove sprite with NULL sprite_id");

    // Test removing with NULL tileset (should fail)
    TEST_ASSERT_FALSE_MESSAGE(ese_tileset_remove_sprite(NULL, 1, "grass"), "Should not remove sprite with NULL tileset");

    ese_tileset_destroy(tileset);
}

static void test_ese_tileset_get_sprite(void) {
    EseTileSet *tileset = ese_tileset_create(g_engine);

    // Test getting sprite from empty tile
    TEST_ASSERT_NULL_MESSAGE(ese_tileset_get_sprite(tileset, 1), "Should return NULL for empty tile");

    // Add a sprite
    ese_tileset_add_sprite(tileset, 1, "grass", 10);
    const char *sprite = ese_tileset_get_sprite(tileset, 1);
    TEST_ASSERT_NOT_NULL_MESSAGE(sprite, "Should return sprite for tile with sprites");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("grass", sprite, "Should return correct sprite");

    // Test getting sprite with NULL tileset (should fail)
    TEST_ASSERT_NULL_MESSAGE(ese_tileset_get_sprite(NULL, 1), "Should return NULL for NULL tileset");

    ese_tileset_destroy(tileset);
}

static void test_ese_tileset_clear_mapping(void) {
    EseTileSet *tileset = ese_tileset_create(g_engine);

    // Add some sprites
    ese_tileset_add_sprite(tileset, 1, "grass", 10);
    ese_tileset_add_sprite(tileset, 1, "stone", 20);
    ese_tileset_add_sprite(tileset, 2, "water", 5);

    // Clear mapping for tile 1
    ese_tileset_clear_mapping(tileset, 1);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, ese_tileset_get_sprite_count(tileset, 1), "Should have 0 sprites after clear");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1, ese_tileset_get_sprite_count(tileset, 2), "Other tiles should be unaffected");

    // Clear mapping for empty tile (should not crash)
    ese_tileset_clear_mapping(tileset, 3);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, ese_tileset_get_sprite_count(tileset, 3), "Should still have 0 sprites");

    // Test clearing with NULL tileset (should not crash)
    ese_tileset_clear_mapping(NULL, 1);

    ese_tileset_destroy(tileset);
}

static void test_ese_tileset_get_sprite_count(void) {
    EseTileSet *tileset = ese_tileset_create(g_engine);

    // Test empty tile
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, ese_tileset_get_sprite_count(tileset, 1), "Empty tile should have 0 sprites");

    // Add sprites and test count
    ese_tileset_add_sprite(tileset, 1, "grass", 10);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1, ese_tileset_get_sprite_count(tileset, 1), "Should have 1 sprite");

    ese_tileset_add_sprite(tileset, 1, "stone", 20);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(2, ese_tileset_get_sprite_count(tileset, 1), "Should have 2 sprites");

    // Test with NULL tileset (should return 0)
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, ese_tileset_get_sprite_count(NULL, 1), "NULL tileset should return 0");

    ese_tileset_destroy(tileset);
}

static void test_ese_tileset_update_sprite_weight(void) {
    EseTileSet *tileset = ese_tileset_create(g_engine);

    // Add a sprite
    ese_tileset_add_sprite(tileset, 1, "grass", 10);

    // Test updating existing sprite weight
    TEST_ASSERT_TRUE_MESSAGE(ese_tileset_update_sprite_weight(tileset, 1, "grass", 15), "Should update existing sprite weight");

    // Test updating non-existent sprite (should fail)
    TEST_ASSERT_FALSE_MESSAGE(ese_tileset_update_sprite_weight(tileset, 1, "stone", 20), "Should not update non-existent sprite");

    // Test updating with zero weight (should fail)
    TEST_ASSERT_FALSE_MESSAGE(ese_tileset_update_sprite_weight(tileset, 1, "grass", 0), "Should not update to zero weight");

    // Test updating with NULL sprite_id (should fail)
    TEST_ASSERT_FALSE_MESSAGE(ese_tileset_update_sprite_weight(tileset, 1, NULL, 20), "Should not update with NULL sprite_id");

    // Test updating with NULL tileset (should fail)
    TEST_ASSERT_FALSE_MESSAGE(ese_tileset_update_sprite_weight(NULL, 1, "grass", 20), "Should not update with NULL tileset");

    ese_tileset_destroy(tileset);
}

static void test_ese_tileset_set_seed(void) {
    EseTileSet *tileset = ese_tileset_create(g_engine);

    // Test initial seed
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, ese_tileset_get_rng_seed(tileset), "Initial seed should be 0");

    // Test setting seed
    ese_tileset_set_seed(tileset, 12345);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(12345, ese_tileset_get_rng_seed(tileset), "Seed should be set correctly");

    // Test setting seed to 0
    ese_tileset_set_seed(tileset, 0);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, ese_tileset_get_rng_seed(tileset), "Seed should be set to 0");

    // Test setting with NULL tileset (should not crash)
    ese_tileset_set_seed(NULL, 12345);

    ese_tileset_destroy(tileset);
}

static void test_ese_tileset_get_sprite_random(void) {
    EseTileSet *tileset = ese_tileset_create(g_engine);

    // Test getting sprite from empty tile
    TEST_ASSERT_NULL_MESSAGE(ese_tileset_get_sprite(tileset, 1), "Should return NULL for empty tile");

    // Add sprites with different weights
    ese_tileset_add_sprite(tileset, 1, "grass", 10);
    ese_tileset_add_sprite(tileset, 1, "stone", 20);
    ese_tileset_add_sprite(tileset, 1, "dirt", 5);

    // Test getting random sprite (should return one of the added sprites)
    const char *sprite = ese_tileset_get_sprite(tileset, 1);
    TEST_ASSERT_NOT_NULL_MESSAGE(sprite, "Should return a sprite");
    bool is_valid = (strcmp(sprite, "grass") == 0 || strcmp(sprite, "stone") == 0 || strcmp(sprite, "dirt") == 0);
    TEST_ASSERT_TRUE_MESSAGE(is_valid, "Should return one of the added sprites");

    // Test with NULL tileset (should return NULL)
    TEST_ASSERT_NULL_MESSAGE(ese_tileset_get_sprite(NULL, 1), "Should return NULL for NULL tileset");

    ese_tileset_destroy(tileset);
}


static void test_ese_tileset_lua_init(void) {
    lua_State *L = g_engine->runtime;
    
    luaL_getmetatable(L, TILESET_PROXY_META);
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Metatable should not exist before initialization");
    lua_pop(L, 1);
    
    lua_getglobal(L, "Tileset");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Global Tileset table should not exist before initialization");
    lua_pop(L, 1);
    
    ese_tileset_lua_init(g_engine);
    
    luaL_getmetatable(L, TILESET_PROXY_META);
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Metatable should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Metatable should be a table");
    lua_pop(L, 1);
    
    lua_getglobal(L, "Tileset");
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Global Tileset table should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Global Tileset table should be a table");
    lua_pop(L, 1);
}

static void test_ese_tileset_lua_push(void) {
    ese_tileset_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseTileSet *tileset = ese_tileset_create(g_engine);
    
    ese_tileset_lua_push(tileset);
    
    EseTileSet **ud = (EseTileSet **)lua_touserdata(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(tileset, *ud, "The pushed item should be the actual tileset");
    
    lua_pop(L, 1); 
    
    ese_tileset_destroy(tileset);
}

static void test_ese_tileset_lua_get(void) {
    ese_tileset_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseTileSet *tileset = ese_tileset_create(g_engine);
    
    ese_tileset_lua_push(tileset);
    
    EseTileSet *extracted_tileset = ese_tileset_lua_get(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(tileset, extracted_tileset, "Extracted tileset should match original");
    
    lua_pop(L, 1);
    ese_tileset_destroy(tileset);
}

/**
* Lua API Test Functions
*/

static void test_ese_tileset_lua_new(void) {
    ese_tileset_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "return Tileset.new(10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute with error");

    const char *testB = "return Tileset.new(10, 10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute with error");

    const char *testC = "return Tileset.new(\"10\")\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute with error");

    const char *testD = "return Tileset.new()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testD), "testD Lua code should execute without error");
    EseTileSet *extracted_tileset = ese_tileset_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_tileset, "Extracted tileset should not be NULL");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, ese_tileset_get_sprite_count(extracted_tileset, 0), "New tileset should have 0 sprites");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, ese_tileset_get_rng_seed(extracted_tileset), "New tileset should have seed 0");
    lua_pop(L, 1); // Pop the tileset from Lua stack
    // Don't destroy Lua-owned tilesets - let Lua GC handle them
}

static void test_ese_tileset_lua_add_sprite(void) {
    ese_tileset_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "local t = Tileset.new(); return t:add_sprite(1, \"grass\", 10)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute without error");
    bool result = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(result, "Should successfully add sprite");
    lua_pop(L, 1);

    const char *testB = "local t = Tileset.new(); return t:add_sprite(1, \"grass\")\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute without error");
    result = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(result, "Should successfully add sprite with default weight");
    lua_pop(L, 1);

    const char *testC = "local t = Tileset.new(); return t:add_sprite(1, \"grass\", 0)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute without error");
    result = lua_toboolean(L, -1);
    TEST_ASSERT_FALSE_MESSAGE(result, "Should not add sprite with zero weight");
    lua_pop(L, 1);

    const char *testD = "local t = Tileset.new(); return t:add_sprite(1, \"grass\", 10, 20)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testD), "testD Lua code should execute with error");

    const char *testE = "local t = Tileset.new(); return t:add_sprite(1)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testE), "testE Lua code should execute with error");
}

static void test_ese_tileset_lua_remove_sprite(void) {
    ese_tileset_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "local t = Tileset.new(); t:add_sprite(1, \"grass\", 10); return t:remove_sprite(1, \"grass\")\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute without error");
    bool result = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(result, "Should successfully remove existing sprite");
    lua_pop(L, 1);

    const char *testB = "local t = Tileset.new(); return t:remove_sprite(1, \"grass\")\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute without error");
    result = lua_toboolean(L, -1);
    TEST_ASSERT_FALSE_MESSAGE(result, "Should not remove non-existent sprite");
    lua_pop(L, 1);

    const char *testC = "local t = Tileset.new(); return t:remove_sprite(1)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute with error");

    const char *testD = "local t = Tileset.new(); return t:remove_sprite(1, \"grass\", \"extra\")\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testD), "testD Lua code should execute with error");
}

static void test_ese_tileset_lua_get_sprite(void) {
    ese_tileset_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "local t = Tileset.new(); t:add_sprite(1, \"grass\", 10); return t:get_sprite(1)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute without error");
    const char *sprite = lua_tostring(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(sprite, "Should return sprite");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("grass", sprite, "Should return correct sprite");
    lua_pop(L, 1);

    const char *testB = "local t = Tileset.new(); return t:get_sprite(1)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute without error");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Should return nil for empty tile");
    lua_pop(L, 1);

    const char *testC = "local t = Tileset.new(); return t:get_sprite()\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute with error");

    const char *testD = "local t = Tileset.new(); return t:get_sprite(1, 2)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testD), "testD Lua code should execute with error");
}

static void test_ese_tileset_lua_clear_mapping(void) {
    ese_tileset_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "local t = Tileset.new(); t:add_sprite(1, \"grass\", 10); t:clear_mapping(1); return t:get_sprite_count(1)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute without error");
    double count = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0, count, "Should have 0 sprites after clear");
    lua_pop(L, 1);

    const char *testB = "local t = Tileset.new(); return t:clear_mapping()\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute with error");

    const char *testC = "local t = Tileset.new(); return t:clear_mapping(1, 2)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute with error");
}

static void test_ese_tileset_lua_get_sprite_count(void) {
    ese_tileset_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "local t = Tileset.new(); t:add_sprite(1, \"grass\", 10); t:add_sprite(1, \"stone\", 20); return t:get_sprite_count(1)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute without error");
    double count = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2.0, count, "Should have 2 sprites");
    lua_pop(L, 1);

    const char *testB = "local t = Tileset.new(); return t:get_sprite_count(1)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute without error");
    count = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0, count, "Should have 0 sprites for empty tile");
    lua_pop(L, 1);

    const char *testC = "local t = Tileset.new(); return t:get_sprite_count()\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute with error");

    const char *testD = "local t = Tileset.new(); return t:get_sprite_count(1, 2)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testD), "testD Lua code should execute with error");
}

static void test_ese_tileset_lua_update_sprite_weight(void) {
    ese_tileset_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "local t = Tileset.new(); t:add_sprite(1, \"grass\", 10); return t:update_sprite_weight(1, \"grass\", 20)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute without error");
    bool result = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(result, "Should successfully update sprite weight");
    lua_pop(L, 1);

    const char *testB = "local t = Tileset.new(); return t:update_sprite_weight(1, \"grass\", 20)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute without error");
    result = lua_toboolean(L, -1);
    TEST_ASSERT_FALSE_MESSAGE(result, "Should not update non-existent sprite");
    lua_pop(L, 1);

    const char *testC = "local t = Tileset.new(); return t:update_sprite_weight(1, \"grass\", 0)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute without error");
    result = lua_toboolean(L, -1);
    TEST_ASSERT_FALSE_MESSAGE(result, "Should not update to zero weight");
    lua_pop(L, 1);

    const char *testD = "local t = Tileset.new(); return t:update_sprite_weight(1)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testD), "testD Lua code should execute with error");

    const char *testE = "local t = Tileset.new(); return t:update_sprite_weight(1, \"grass\", 20, 30)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testE), "testE Lua code should execute with error");
}

static void test_ese_tileset_lua_get_sprite_random(void) {
    ese_tileset_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "local t = Tileset.new(); t:add_sprite(1, \"grass\", 10); t:add_sprite(1, \"stone\", 20); local sprite = t:get_sprite(1); return sprite == \"grass\" or sprite == \"stone\"\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute without error");
    bool result = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(result, "Should return one of the added sprites");
    lua_pop(L, 1);

    const char *testB = "local t = Tileset.new(); return t:get_sprite(1)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute without error");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Should return nil for empty tile");
    lua_pop(L, 1);

    const char *testC = "local t = Tileset.new(); return t:get_sprite()\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute with error");

    const char *testD = "local t = Tileset.new(); return t:get_sprite(1, 2)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testD), "testD Lua code should execute with error");
}

static void test_ese_tileset_lua_tostring(void) {
    ese_tileset_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test_code = "local t = Tileset.new(); t:add_sprite(1, \"grass\", 10); t:add_sprite(1, \"stone\", 20); return tostring(t)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "tostring test should execute without error");
    const char *result = lua_tostring(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "tostring result should not be NULL");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "Tileset:") != NULL, "tostring should contain 'Tileset:'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "total_sprites=") != NULL, "tostring should contain 'total_sprites='");
    lua_pop(L, 1); 
}

static void test_ese_tileset_lua_gc(void) {
    ese_tileset_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *testA = "local t = Tileset.new(); t:add_sprite(1, \"grass\", 10)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "Tileset creation should execute without error");
    
    int collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testB = "return Tileset.new()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "Tileset creation should execute without error");
    EseTileSet *extracted_tileset = ese_tileset_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_tileset, "Extracted tileset should not be NULL");
    ese_tileset_ref(extracted_tileset);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    ese_tileset_unref(extracted_tileset);

    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testC = "return Tileset.new()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "Tileset creation should execute without error");
    extracted_tileset = ese_tileset_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_tileset, "Extracted tileset should not be NULL");
    ese_tileset_ref(extracted_tileset);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    ese_tileset_unref(extracted_tileset);
    // Don't destroy Lua-owned tilesets - let Lua GC handle them

    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    // Verify GC didn't crash by running another operation
    const char *verify_code = "return 42";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, verify_code), "Lua should still work after GC");
    int result = (int)lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(42, result, "Lua should return correct value after GC");
    lua_pop(L, 1);
}
