/*
* test_ese_point.c - Unity-based tests for point functionality
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

#include "../src/types/point.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"
#include "../src/scripting/lua_engine.h"
#include "../src/scripting/lua_engine_private.h"
#include "../src/types/types.h"
#include "../src/vendor/lua/src/lua.h"
#include "../src/vendor/lua/src/lauxlib.h"

/**
* C API Test Functions Declarations
*/
static void test_ese_point_sizeof(void);
static void test_ese_point_create_requires_engine(void);
static void test_ese_point_create(void);
static void test_ese_point_x(void);
static void test_ese_point_y(void);
static void test_ese_point_ref(void);
static void test_ese_point_copy_requires_engine(void);
static void test_ese_point_copy(void);
static void test_ese_point_distance(void);
static void test_ese_point_watcher_system(void);
static void test_ese_point_lua_integration(void);
static void test_ese_point_lua_init(void);
static void test_ese_point_lua_push(void);
static void test_ese_point_lua_get(void);

/**
* Lua API Test Functions Declarations
*/
static void test_ese_point_lua_new(void);
static void test_ese_point_lua_zero(void);
static void test_ese_point_lua_distance(void);
static void test_ese_point_lua_x(void);
static void test_ese_point_lua_y(void);
static void test_ese_point_lua_tostring(void);
static void test_ese_point_lua_gc(void);

/**
* Mock watcher callback for testing
*/
static bool watcher_called = false;
static EsePoint *last_watched_point = NULL;
static void *last_watcher_userdata = NULL;

static void test_watcher_callback(EsePoint *point, void *userdata) {
    watcher_called = true;
    last_watched_point = point;
    last_watcher_userdata = userdata;
}

static void mock_reset(void) {
    watcher_called = false;
    last_watched_point = NULL;
    last_watcher_userdata = NULL;
}

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

    printf("\nEsePoint Tests\n");
    printf("--------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_ese_point_sizeof);
    RUN_TEST(test_ese_point_create_requires_engine);
    RUN_TEST(test_ese_point_create);
    RUN_TEST(test_ese_point_x);
    RUN_TEST(test_ese_point_y);
    RUN_TEST(test_ese_point_ref);
    RUN_TEST(test_ese_point_copy_requires_engine);
    RUN_TEST(test_ese_point_copy);
    RUN_TEST(test_ese_point_distance);
    RUN_TEST(test_ese_point_watcher_system);
    RUN_TEST(test_ese_point_lua_integration);
    RUN_TEST(test_ese_point_lua_init);
    RUN_TEST(test_ese_point_lua_push);
    RUN_TEST(test_ese_point_lua_get);

    RUN_TEST(test_ese_point_lua_new);
    RUN_TEST(test_ese_point_lua_zero);
    RUN_TEST(test_ese_point_lua_distance);
    RUN_TEST(test_ese_point_lua_x);
    RUN_TEST(test_ese_point_lua_y);
    RUN_TEST(test_ese_point_lua_tostring);
    RUN_TEST(test_ese_point_lua_gc);

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_ese_point_sizeof(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(56, ese_point_sizeof(), "Point should be 56 bytes");
}

static void test_ese_point_create_requires_engine(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, ese_point_sizeof(), "Point size should be > 0");
}

static void test_ese_point_create(void) {
    EsePoint *point = ese_point_create(g_engine);

    TEST_ASSERT_NOT_NULL_MESSAGE(point, "Point should be created");
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_point_get_x(point));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_point_get_y(point));
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_point_get_lua_ref_count(point), "New point should have ref count 0");

    ese_point_destroy(point);
}

static void test_ese_point_x(void) {
    EsePoint *point = ese_point_create(g_engine);

    ese_point_set_x(point, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_point_get_x(point));

    ese_point_set_x(point, -10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, ese_point_get_x(point));

    ese_point_set_x(point, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_point_get_x(point));

    ese_point_destroy(point);
}

static void test_ese_point_y(void) {
    EsePoint *point = ese_point_create(g_engine);

    ese_point_set_y(point, 20.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, ese_point_get_y(point));

    ese_point_set_y(point, -10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, ese_point_get_y(point));

    ese_point_set_y(point, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_point_get_y(point));

    ese_point_destroy(point);
}

static void test_ese_point_ref(void) {
    EsePoint *point = ese_point_create(g_engine);

    ese_point_ref(g_engine, point);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, ese_point_get_lua_ref_count(point), "Ref count should be 1");

    ese_point_unref(g_engine, point);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_point_get_lua_ref_count(point), "Ref count should be 0");

    ese_point_destroy(point);
}

static void test_ese_point_copy_requires_engine(void) {
    ASSERT_DEATH(ese_point_copy(NULL), "ese_point_copy should abort with NULL point");
}

static void test_ese_point_copy(void) {
    EsePoint *point = ese_point_create(g_engine);
    ese_point_ref(g_engine, point);
    ese_point_set_x(point, 10.0f);
    ese_point_set_y(point, 20.0f);
    EsePoint *copy = ese_point_copy(point);

    TEST_ASSERT_NOT_NULL_MESSAGE(copy, "Copy should be created");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_point_get_lua_ref_count(copy), "Copy should have ref count 0");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_point_get_x(copy));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, ese_point_get_y(copy));

    ese_point_unref(g_engine, point);
    ese_point_destroy(point);
    ese_point_destroy(copy);
}

static void test_ese_point_distance(void) {
    EsePoint *pointA = ese_point_create(g_engine);
    EsePoint *pointB = ese_point_create(g_engine);

    ese_point_set_x(pointA, 0.0f);
    ese_point_set_y(pointA, 0.0f);

    ese_point_set_x(pointB, 10.0f);
    ese_point_set_y(pointB, 0.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_point_distance(pointA, pointB));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, ese_point_distance_squared(pointA, pointB));

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_point_distance(pointB, pointA));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, ese_point_distance_squared(pointB, pointA));

    ese_point_set_x(pointA, 0.0f);
    ese_point_set_y(pointA, 0.0f);

    ese_point_set_x(pointB, 0.0f);
    ese_point_set_y(pointB, 10.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_point_distance(pointA, pointB));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, ese_point_distance_squared(pointA, pointB));

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_point_distance(pointB, pointA));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, ese_point_distance_squared(pointB, pointA));

    ese_point_set_x(pointA, 0.0f);
    ese_point_set_y(pointA, 0.0f);

    ese_point_set_x(pointB, -10.0f);
    ese_point_set_y(pointB, 0.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_point_distance(pointA, pointB));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, ese_point_distance_squared(pointA, pointB));

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_point_distance(pointB, pointA));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, ese_point_distance_squared(pointB, pointA));

    ese_point_set_x(pointA, 0.0f);
    ese_point_set_y(pointA, 0.0f);

    ese_point_set_x(pointB, 0.0f);
    ese_point_set_y(pointB, -10.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_point_distance(pointA, pointB));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, ese_point_distance_squared(pointA, pointB));

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_point_distance(pointB, pointA));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, ese_point_distance_squared(pointB, pointA));

    ese_point_set_x(pointA, 10.0f);
    ese_point_set_y(pointA, 0.0f);

    ese_point_set_x(pointB, 0.0f);
    ese_point_set_y(pointB, 10.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 14.14214f, ese_point_distance(pointA, pointB));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 200.0f, ese_point_distance_squared(pointA, pointB));

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 14.14214f, ese_point_distance(pointB, pointA));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 200.0f, ese_point_distance_squared(pointB, pointA));

    ese_point_set_x(pointA, 0.0f);
    ese_point_set_y(pointA, 0.0f);
    
    ese_point_set_x(pointB, 0.0f);
    ese_point_set_y(pointB, 0.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_point_distance(pointA, pointB));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_point_distance_squared(pointA, pointB));

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_point_distance(pointB, pointA));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_point_distance_squared(pointB, pointA));

    ese_point_destroy(pointA);
    ese_point_destroy(pointB);
}

static void test_ese_point_watcher_system(void) {
    EsePoint *point = ese_point_create(g_engine);

    mock_reset();
    ese_point_set_x(point, 25.0f);
    TEST_ASSERT_FALSE_MESSAGE(watcher_called, "Watcher should not be called before adding");

    void *test_userdata = (void*)0x12345678;
    bool add_result = ese_point_add_watcher(point, test_watcher_callback, test_userdata);
    TEST_ASSERT_TRUE_MESSAGE(add_result, "Should successfully add watcher");

    mock_reset();
    ese_point_set_x(point, 50.0f);
    TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when x changes");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(point, last_watched_point, "Watcher should receive correct point pointer");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(test_userdata, last_watcher_userdata, "Watcher should receive correct userdata");

    mock_reset();
    ese_point_set_y(point, 75.0f);
    TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when y changes");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(point, last_watched_point, "Watcher should receive correct point pointer");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(test_userdata, last_watcher_userdata, "Watcher should receive correct userdata");

    bool remove_result = ese_point_remove_watcher(point, test_watcher_callback, test_userdata);
    TEST_ASSERT_TRUE_MESSAGE(remove_result, "Should successfully remove watcher");

    mock_reset();
    ese_point_set_x(point, 100.0f);
    TEST_ASSERT_FALSE_MESSAGE(watcher_called, "Watcher should not be called after removal");

    ese_point_destroy(point);
}

static void test_ese_point_lua_integration(void) {
    EseLuaEngine *engine = create_test_engine();
    EsePoint *point = ese_point_create(engine);

    TEST_ASSERT_EQUAL_INT_MESSAGE(ESE_LUA_NOREF, ese_point_get_lua_ref(point), "Point should have no Lua reference initially");

    ese_point_ref(engine, point);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(ESE_LUA_NOREF, ese_point_get_lua_ref(point), "Point should have a valid Lua reference after ref");

    ese_point_unref(engine, point);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ESE_LUA_NOREF, ese_point_get_lua_ref(point), "Point should have no Lua reference after unref");

    ese_point_destroy(point);
    lua_engine_destroy(engine);
}

static void test_ese_point_lua_init(void) {
    lua_State *L = g_engine->L;
    
    luaL_getmetatable(L, "PointMeta");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Metatable should not exist before initialization");
    lua_pop(L, 1);
    
    lua_getglobal(L, "Point");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Global Point table should not exist before initialization");
    lua_pop(L, 1);
    
    ese_point_lua_init(g_engine);
    
    luaL_getmetatable(L, "PointMeta");
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Metatable should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Metatable should be a table");
    lua_pop(L, 1);
    
    lua_getglobal(L, "Point");
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Global Point table should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Global Point table should be a table");
    lua_pop(L, 1);
}

static void test_ese_point_lua_push(void) {
    ese_point_lua_init(g_engine);

    lua_State *L = g_engine->L;
    EsePoint *point = ese_point_create(g_engine);
    
    ese_point_lua_push(g_engine, point);
    
    EsePoint **ud = (EsePoint **)lua_touserdata(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(point, *ud, "The pushed item should be the actual point");
    
    lua_pop(L, 1); 
    
    ese_point_destroy(point);
}

static void test_ese_point_lua_get(void) {
    ese_point_lua_init(g_engine);

    lua_State *L = g_engine->L;
    EsePoint *point = ese_point_create(g_engine);
    
    ese_point_lua_push(g_engine, point);
    
    EsePoint *extracted_point = ese_point_lua_get(g_engine, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(point, extracted_point, "Extracted point should match original");
    
    lua_pop(L, 1);
    ese_point_destroy(point);
}

/**
* Lua API Test Functions
*/

static void test_ese_point_lua_new(void) {
    ese_point_lua_init(g_engine);
    lua_State *L = g_engine->L;
    
    const char *testA = "return Point.new()\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute with error");

    const char *testB = "return Point.new(10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute with error");

    const char *testC = "return Point.new(10, 10, 10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute with error");

    const char *testD = "return Point.new(\"10\", \"10\")\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testD), "testD Lua code should execute with error");

    const char *testE = "return Point.new(10, 10)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testE), "testE Lua code should execute without error");
    EsePoint *extracted_point = ese_point_lua_get(g_engine, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_point, "Extracted point should not be NULL");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, ese_point_get_x(extracted_point), "Extracted point should have x=10");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, ese_point_get_y(extracted_point), "Extracted point should have y=10");
    ese_point_destroy(extracted_point);
}

static void test_ese_point_lua_zero(void) {
    ese_point_lua_init(g_engine);
    lua_State *L = g_engine->L;
    
    const char *testA = "return Point.zero(10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute with error");

    const char *testB = "return Point.zero(10, 10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute with error");

    const char *testC = "return Point.zero()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute without error");
    EsePoint *extracted_point = ese_point_lua_get(g_engine, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_point, "Extracted point should not be NULL");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_point_get_x(extracted_point), "Extracted point should have x=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_point_get_y(extracted_point), "Extracted point should have y=0");
    ese_point_destroy(extracted_point);
}

static void test_ese_point_lua_distance(void) {
    ese_point_lua_init(g_engine);
    lua_State *L = g_engine->L;
    
    const char *testA = "return Point.distance(Point.new(0, 0), Point.new(10, 0))\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute without error");
    double distance = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, distance, "Distance should be 10");

    const char *testB = "return Point.distance(Point.new(0, 0), Point.new(0, 10))\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute without error");
    distance = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, distance, "Distance should be 10");

    const char *testC = "return Point.distance(Point.new(0, 0), Point.new(-10, 0))\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute without error");
    distance = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, distance, "Distance should be 10");

    const char *testD = "return Point.distance(Point.new(0, 0), Point.new(0, -10))\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testD), "testD Lua code should execute without error");
    distance = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, distance, "Distance should be 10");

    const char *testE = "return Point.distance(Point.new(10, 0), Point.new(0, 10))\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testE), "testE Lua code should execute without error");
    distance = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(14.14214f, distance, "Distance should be 14.14214");

    const char *testF = "return Point.distance(Point.new(0, 0), Point.new(0, 0))\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testF), "testF Lua code should execute without error");
    distance = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, distance, "Distance should be 0");
}

static void test_ese_point_lua_x(void) {
    ese_point_lua_init(g_engine);
    lua_State *L = g_engine->L;

    const char *test1 = "local p = Point.new(0, 0); p.x = \"20\"; return p.y";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "testD Lua code should execute with error");

    const char *test2 = "local p = Point.new(0, 0); p.x = 10; return p.x";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua x set/get test 1 should execute without error");
    double x = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, x);
    lua_pop(L, 1);

    const char *test3 = "local p = Point.new(0, 0); p.x = -10; return p.x";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua x set/get test 2 should execute without error");
    x = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, x);
    lua_pop(L, 1);

    const char *test4 = "local p = Point.new(0, 0); p.x = 0; return p.x";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test4), "Lua x set/get test 3 should execute without error");
    x = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, x);
    lua_pop(L, 1);
}

static void test_ese_point_lua_y(void) {
    ese_point_lua_init(g_engine);
    lua_State *L = g_engine->L;

    const char *test1 = "local p = Point.new(0, 0); p.y = \"20\"; return p.y";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "testD Lua code should execute with error");

    const char *test2 = "local p = Point.new(0, 0); p.y = 20; return p.y";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua y set/get test 1 should execute without error");
    double y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, y);
    lua_pop(L, 1);

    const char *test3 = "local p = Point.new(0, 0); p.y = -10; return p.y";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua y set/get test 2 should execute without error");
    y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, y);
    lua_pop(L, 1);

    const char *test4 = "local p = Point.new(0, 0); p.y = 0; return p.y";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test4), "Lua y set/get test 3 should execute without error");
    y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, y);
    lua_pop(L, 1);
}

static void test_ese_point_lua_tostring(void) {
    ese_point_lua_init(g_engine);
    lua_State *L = g_engine->L;

    const char *test_code = "local p = Point.new(10.5, 20.25); return tostring(p)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "tostring test should execute without error");
    const char *result = lua_tostring(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "tostring result should not be NULL");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "Point:") != NULL, "tostring should contain 'Point:'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "x=10.50") != NULL, "tostring should contain 'x=10.50'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "y=20.25") != NULL, "tostring should contain 'y=20.25'");    
    lua_pop(L, 1); 
}

static void test_ese_point_lua_gc(void) {
    ese_point_lua_init(g_engine);
    lua_State *L = g_engine->L;

    const char *testA = "local p = Point.new(5, 10)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "Point creation should execute without error");
    
    int collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testB = "return Point.new(5, 10)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "Point creation should execute without error");
    EsePoint *extracted_point = ese_point_lua_get(g_engine, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_point, "Extracted point should not be NULL");
    ese_point_ref(g_engine, extracted_point);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    ese_point_unref(g_engine, extracted_point);

    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testC = "return Point.new(5, 10)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "Point creation should execute without error");
    extracted_point = ese_point_lua_get(g_engine, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_point, "Extracted point should not be NULL");
    ese_point_ref(g_engine, extracted_point);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    ese_point_unref(g_engine, extracted_point);
    ese_point_destroy(extracted_point);

    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    // Verify GC didn't crash by running another operation
    const char *verify_code = "return 42";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, verify_code), "Lua should still work after GC");
    int result = (int)lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(42, result, "Lua should return correct value after GC");
    lua_pop(L, 1);
}
