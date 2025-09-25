/*
* test_ese_vector.c - Unity-based tests for vector functionality
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

#include "../src/types/vector.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

/**
* C API Test Functions Declarations
*/
static void test_ese_vector_sizeof(void);
static void test_ese_vector_create_requires_engine(void);
static void test_ese_vector_create(void);
static void test_ese_vector_x(void);
static void test_ese_vector_y(void);
static void test_ese_vector_ref(void);
static void test_ese_vector_copy_requires_engine(void);
static void test_ese_vector_copy(void);
static void test_ese_vector_magnitude(void);
static void test_ese_vector_normalize(void);
static void test_ese_vector_set_direction(void);
static void test_ese_vector_lua_integration(void);
static void test_ese_vector_lua_init(void);
static void test_ese_vector_lua_push(void);
static void test_ese_vector_lua_get(void);

/**
* Lua API Test Functions Declarations
*/
static void test_ese_vector_lua_new(void);
static void test_ese_vector_lua_zero(void);
static void test_ese_vector_lua_magnitude(void);
static void test_ese_vector_lua_normalize(void);
static void test_ese_vector_lua_set_direction(void);
static void test_ese_vector_lua_x(void);
static void test_ese_vector_lua_y(void);
static void test_ese_vector_lua_tostring(void);
static void test_ese_vector_lua_gc(void);

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

    printf("\nEseVector Tests\n");
    printf("---------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_ese_vector_sizeof);
    RUN_TEST(test_ese_vector_create_requires_engine);
    RUN_TEST(test_ese_vector_create);
    RUN_TEST(test_ese_vector_x);
    RUN_TEST(test_ese_vector_y);
    RUN_TEST(test_ese_vector_ref);
    RUN_TEST(test_ese_vector_copy_requires_engine);
    RUN_TEST(test_ese_vector_copy);
    RUN_TEST(test_ese_vector_magnitude);
    RUN_TEST(test_ese_vector_normalize);
    RUN_TEST(test_ese_vector_set_direction);
    RUN_TEST(test_ese_vector_lua_integration);
    RUN_TEST(test_ese_vector_lua_init);
    RUN_TEST(test_ese_vector_lua_push);
    RUN_TEST(test_ese_vector_lua_get);

    RUN_TEST(test_ese_vector_lua_new);
    RUN_TEST(test_ese_vector_lua_zero);
    RUN_TEST(test_ese_vector_lua_magnitude);
    RUN_TEST(test_ese_vector_lua_normalize);
    RUN_TEST(test_ese_vector_lua_set_direction);
    RUN_TEST(test_ese_vector_lua_x);
    RUN_TEST(test_ese_vector_lua_y);
    RUN_TEST(test_ese_vector_lua_tostring);
    RUN_TEST(test_ese_vector_lua_gc);

    memory_manager.destroy();

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_ese_vector_sizeof(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, ese_vector_sizeof(), "Vector size should be > 0");
}

static void test_ese_vector_create_requires_engine(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, ese_vector_sizeof(), "Vector size should be > 0");
}

static void test_ese_vector_create(void) {
    EseVector *vector = ese_vector_create(g_engine);

    TEST_ASSERT_NOT_NULL_MESSAGE(vector, "Vector should be created");
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_vector_get_y(vector));
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_vector_get_state(vector), "Vector should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_vector_get_lua_ref_count(vector), "New vector should have ref count 0");

    ese_vector_destroy(vector);
}

static void test_ese_vector_x(void) {
    EseVector *vector = ese_vector_create(g_engine);

    ese_vector_set_x(vector, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_vector_get_x(vector));

    ese_vector_set_x(vector, -10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, ese_vector_get_x(vector));

    ese_vector_set_x(vector, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_vector_get_x(vector));

    ese_vector_destroy(vector);
}

static void test_ese_vector_y(void) {
    EseVector *vector = ese_vector_create(g_engine);

    ese_vector_set_y(vector, 20.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, ese_vector_get_y(vector));

    ese_vector_set_y(vector, -10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, ese_vector_get_y(vector));

    ese_vector_set_y(vector, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_vector_get_y(vector));

    ese_vector_destroy(vector);
}

static void test_ese_vector_ref(void) {
    EseVector *vector = ese_vector_create(g_engine);

    ese_vector_ref(vector);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, ese_vector_get_lua_ref_count(vector), "Ref count should be 1");

    vector_unref(vector);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_vector_get_lua_ref_count(vector), "Ref count should be 0");

    ese_vector_destroy(vector);
}

static void test_ese_vector_copy_requires_engine(void) {
    ASSERT_DEATH(ese_vector_copy(NULL), "ese_vector_copy should abort with NULL vector");
}

static void test_ese_vector_copy(void) {
    EseVector *vector = ese_vector_create(g_engine);
    ese_vector_ref(vector);
    ese_vector_set_x(vector, 10.0f);
    ese_vector_set_y(vector, 20.0f);
    EseVector *copy = ese_vector_copy(vector);

    TEST_ASSERT_NOT_NULL_MESSAGE(copy, "Copy should be created");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_vector_get_state(copy), "Copy should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_vector_get_lua_ref_count(copy), "Copy should have ref count 0");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_vector_get_x(copy));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, ese_vector_get_y(copy));

    vector_unref(vector);
    ese_vector_destroy(vector);
    ese_vector_destroy(copy);
}

static void test_ese_vector_magnitude(void) {
    EseVector *vector = ese_vector_create(g_engine);

    // Test zero vector
    ese_vector_set_x(vector, 0.0f);
    ese_vector_set_y(vector, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_vector_magnitude(vector));

    // Test unit vectors
    ese_vector_set_x(vector, 1.0f);
    ese_vector_set_y(vector, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_vector_magnitude(vector));

    ese_vector_set_x(vector, 0.0f);
    ese_vector_set_y(vector, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_vector_magnitude(vector));

    // Test 3-4-5 triangle
    ese_vector_set_x(vector, 3.0f);
    ese_vector_set_y(vector, 4.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, ese_vector_magnitude(vector));

    // Test negative components
    ese_vector_set_x(vector, -3.0f);
    ese_vector_set_y(vector, -4.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, ese_vector_magnitude(vector));

    // Test mixed signs
    ese_vector_set_x(vector, 3.0f);
    ese_vector_set_y(vector, -4.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, ese_vector_magnitude(vector));

    // Test decimal values
    ese_vector_set_x(vector, 1.5f);
    ese_vector_set_y(vector, 2.0f);
    float expected = sqrtf(1.5f * 1.5f + 2.0f * 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, ese_vector_magnitude(vector));

    ese_vector_destroy(vector);
}

static void test_ese_vector_normalize(void) {
    EseVector *vector = ese_vector_create(g_engine);

    // Test zero vector (should not change)
    ese_vector_set_x(vector, 0.0f);
    ese_vector_set_y(vector, 0.0f);
    ese_vector_normalize(vector);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_vector_get_y(vector));

    // Test 3-4-5 triangle
    ese_vector_set_x(vector, 3.0f);
    ese_vector_set_y(vector, 4.0f);
    ese_vector_normalize(vector);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.6f, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, ese_vector_get_y(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_vector_magnitude(vector));

    // Test negative components
    ese_vector_set_x(vector, -3.0f);
    ese_vector_set_y(vector, -4.0f);
    ese_vector_normalize(vector);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.6f, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.8f, ese_vector_get_y(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_vector_magnitude(vector));

    // Test mixed signs
    ese_vector_set_x(vector, 3.0f);
    ese_vector_set_y(vector, -4.0f);
    ese_vector_normalize(vector);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.6f, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.8f, ese_vector_get_y(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_vector_magnitude(vector));

    // Test already normalized vector
    ese_vector_set_x(vector, 1.0f);
    ese_vector_set_y(vector, 0.0f);
    ese_vector_normalize(vector);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_vector_get_y(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_vector_magnitude(vector));

    ese_vector_destroy(vector);
}

static void test_ese_vector_set_direction(void) {
    EseVector *vector = ese_vector_create(g_engine);

    // Test cardinal directions
    ese_vector_set_direction(vector, "n", 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, ese_vector_get_y(vector));

    ese_vector_set_direction(vector, "s", 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -5.0f, ese_vector_get_y(vector));

    ese_vector_set_direction(vector, "e", 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_vector_get_y(vector));

    ese_vector_set_direction(vector, "w", 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -5.0f, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_vector_get_y(vector));

    // Test diagonal directions
    ese_vector_set_direction(vector, "ne", 5.0f);
    float expected = 5.0f / sqrtf(2.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, ese_vector_get_y(vector));

    ese_vector_set_direction(vector, "nw", 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -expected, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, ese_vector_get_y(vector));

    ese_vector_set_direction(vector, "se", 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -expected, ese_vector_get_y(vector));

    ese_vector_set_direction(vector, "sw", 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -expected, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -expected, ese_vector_get_y(vector));

    // Test case insensitive
    ese_vector_set_direction(vector, "N", 3.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, ese_vector_get_y(vector));

    ese_vector_set_direction(vector, "E", 3.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_vector_get_y(vector));

    // Test zero magnitude
    ese_vector_set_direction(vector, "n", 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_vector_get_y(vector));

    // Test invalid direction do nothing
    ese_vector_set_x(vector, 1.0f);
    ese_vector_set_y(vector, 2.0f);
    ese_vector_set_direction(vector, "invalid", 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_vector_get_x(vector));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, ese_vector_get_y(vector));

    ese_vector_destroy(vector);
}

static void test_ese_vector_lua_integration(void) {
    EseLuaEngine *engine = create_test_engine();
    EseVector *vector = ese_vector_create(engine);

    lua_State *before_state = ese_vector_get_state(vector);
    TEST_ASSERT_NOT_NULL_MESSAGE(before_state, "Vector should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, before_state, "Vector state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_vector_get_lua_ref(vector), "Vector should have no Lua reference initially");

    ese_vector_ref(vector);
    lua_State *after_ref_state = ese_vector_get_state(vector);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_ref_state, "Vector should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_ref_state, "Vector state should match engine runtime");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(LUA_NOREF, ese_vector_get_lua_ref(vector), "Vector should have a valid Lua reference after ref");

    vector_unref(vector);
    lua_State *after_unref_state = ese_vector_get_state(vector);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_unref_state, "Vector should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_unref_state, "Vector state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_vector_get_lua_ref(vector), "Vector should have no Lua reference after unref");

    ese_vector_destroy(vector);
    lua_engine_destroy(engine);
}

static void test_ese_vector_lua_init(void) {
    lua_State *L = g_engine->runtime;
    
    luaL_getmetatable(L, VECTOR_PROXY_META);
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Metatable should not exist before initialization");
    lua_pop(L, 1);
    
    lua_getglobal(L, "Vector");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Global Vector table should not exist before initialization");
    lua_pop(L, 1);
    
    ese_vector_lua_init(g_engine);
    
    luaL_getmetatable(L, VECTOR_PROXY_META);
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Metatable should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Metatable should be a table");
    lua_pop(L, 1);
    
    lua_getglobal(L, "Vector");
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Global Vector table should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Global Vector table should be a table");
    lua_pop(L, 1);
}

static void test_ese_vector_lua_push(void) {
    ese_vector_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseVector *vector = ese_vector_create(g_engine);
    
    ese_vector_lua_push(vector);
    
    EseVector **ud = (EseVector **)lua_touserdata(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(vector, *ud, "The pushed item should be the actual vector");
    
    lua_pop(L, 1); 
    
    ese_vector_destroy(vector);
}

static void test_ese_vector_lua_get(void) {
    ese_vector_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseVector *vector = ese_vector_create(g_engine);
    
    ese_vector_lua_push(vector);
    
    EseVector *extracted_vector = ese_vector_lua_get(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(vector, extracted_vector, "Extracted vector should match original");
    
    lua_pop(L, 1);
    ese_vector_destroy(vector);
}

/**
* Lua API Test Functions
*/

static void test_ese_vector_lua_new(void) {
    ese_vector_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "return Vector.new()\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute with error");

    const char *testB = "return Vector.new(10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute with error");

    const char *testC = "return Vector.new(10, 10, 10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute with error");

    const char *testD = "return Vector.new(\"10\", \"10\")\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testD), "testD Lua code should execute with error");

    const char *testE = "return Vector.new(10, 10)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testE), "testE Lua code should execute without error");
    EseVector *extracted_vector = ese_vector_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_vector, "Extracted vector should not be NULL");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, ese_vector_get_x(extracted_vector), "Extracted vector should have x=10");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, ese_vector_get_y(extracted_vector), "Extracted vector should have y=10");
    ese_vector_destroy(extracted_vector);
}

static void test_ese_vector_lua_zero(void) {
    ese_vector_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "return Vector.zero(10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute with error");

    const char *testB = "return Vector.zero(10, 10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute with error");

    const char *testC = "return Vector.zero()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute without error");
    EseVector *extracted_vector = ese_vector_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_vector, "Extracted vector should not be NULL");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_vector_get_x(extracted_vector), "Extracted vector should have x=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_vector_get_y(extracted_vector), "Extracted vector should have y=0");
    ese_vector_destroy(extracted_vector);
}

static void test_ese_vector_lua_magnitude(void) {
    ese_vector_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "return Vector.new(3, 4):magnitude()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute without error");
    double magnitude = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(5.0f, magnitude, "Magnitude should be 5");

    const char *testB = "return Vector.new(0, 0):magnitude()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute without error");
    magnitude = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, magnitude, "Magnitude should be 0");

    const char *testC = "return Vector.new(-3, -4):magnitude()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute without error");
    magnitude = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(5.0f, magnitude, "Magnitude should be 5");
}

static void test_ese_vector_lua_normalize(void) {
    ese_vector_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "local v = Vector.new(3, 4); v:normalize(); return v.x, v.y\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute without error");
    double x = lua_tonumber(L, -2);
    double y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.6f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, y);

    const char *testB = "local v = Vector.new(0, 0); v:normalize(); return v.x, v.y\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute without error");
    x = lua_tonumber(L, -2);
    y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, y);
}

static void test_ese_vector_lua_set_direction(void) {
    ese_vector_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "local v = Vector.new(0, 0); v:set_direction(\"n\", 5); return v.x, v.y\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute without error");
    double x = lua_tonumber(L, -2);
    double y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, y);

    const char *testB = "local v = Vector.new(0, 0); v:set_direction(\"e\", 3); return v.x, v.y\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute without error");
    x = lua_tonumber(L, -2);
    y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, y);

    const char *testC = "local v = Vector.new(0, 0); v:set_direction(\"ne\", 5); return v.x, v.y\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute without error");
    x = lua_tonumber(L, -2);
    y = lua_tonumber(L, -1);
    float expected = 5.0f / sqrtf(2.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, y);

    const char *testD = "local v = Vector.new(0, 1); v:set_direction(\"invalid\", 5); return v.x, v.y\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testD), "testD Lua code should execute without error");
    x = lua_tonumber(L, -2);
    y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, y);
}

static void test_ese_vector_lua_x(void) {
    ese_vector_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local v = Vector.new(0, 0); v.x = \"20\"; return v.x";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "test1 Lua code should execute with error");

    const char *test2 = "local v = Vector.new(0, 0); v.x = 10; return v.x";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua x set/get test 1 should execute without error");
    double x = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, x);
    lua_pop(L, 1);

    const char *test3 = "local v = Vector.new(0, 0); v.x = -10; return v.x";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua x set/get test 2 should execute without error");
    x = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, x);
    lua_pop(L, 1);

    const char *test4 = "local v = Vector.new(0, 0); v.x = 0; return v.x";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test4), "Lua x set/get test 3 should execute without error");
    x = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, x);
    lua_pop(L, 1);
}

static void test_ese_vector_lua_y(void) {
    ese_vector_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local v = Vector.new(0, 0); v.y = \"20\"; return v.y";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "test1 Lua code should execute with error");

    const char *test2 = "local v = Vector.new(0, 0); v.y = 20; return v.y";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua y set/get test 1 should execute without error");
    double y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, y);
    lua_pop(L, 1);

    const char *test3 = "local v = Vector.new(0, 0); v.y = -10; return v.y";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua y set/get test 2 should execute without error");
    y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, y);
    lua_pop(L, 1);

    const char *test4 = "local v = Vector.new(0, 0); v.y = 0; return v.y";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test4), "Lua y set/get test 3 should execute without error");
    y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, y);
    lua_pop(L, 1);
}

static void test_ese_vector_lua_tostring(void) {
    ese_vector_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test_code = "local v = Vector.new(10.5, 20.25); return tostring(v)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "tostring test should execute without error");
    const char *result = lua_tostring(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "tostring result should not be NULL");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "Vector:") != NULL, "tostring should contain 'Vector:'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "x=10.50") != NULL, "tostring should contain 'x=10.50'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "y=20.25") != NULL, "tostring should contain 'y=20.25'");    
    lua_pop(L, 1); 
}

static void test_ese_vector_lua_gc(void) {
    ese_vector_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *testA = "local v = Vector.new(5, 10)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "Vector creation should execute without error");
    
    int collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testB = "return Vector.new(5, 10)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "Vector creation should execute without error");
    EseVector *extracted_vector = ese_vector_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_vector, "Extracted vector should not be NULL");
    ese_vector_ref(extracted_vector);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    vector_unref(extracted_vector);

    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testC = "return Vector.new(5, 10)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "Vector creation should execute without error");
    extracted_vector = ese_vector_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_vector, "Extracted vector should not be NULL");
    ese_vector_ref(extracted_vector);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    vector_unref(extracted_vector);
    ese_vector_destroy(extracted_vector);

    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    // Verify GC didn't crash by running another operation
    const char *verify_code = "return 42";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, verify_code), "Lua should still work after GC");
    int result = (int)lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(42, result, "Lua should return correct value after GC");
    lua_pop(L, 1);
}
