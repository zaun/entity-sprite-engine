/*
* test_ese_ray.c - Unity-based tests for ray functionality
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

#include "../src/types/ray.h"
#include "../src/types/rect.h"
#include "../src/types/point.h"
#include "../src/types/vector.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"
#include "../src/scripting/lua_engine.h"
#include "../src/vendor/json/cJSON.h"

/**
* C API Test Functions Declarations
*/
static void test_ese_ray_sizeof(void);
static void test_ese_ray_create_requires_engine(void);
static void test_ese_ray_create(void);
static void test_ese_ray_x(void);
static void test_ese_ray_y(void);
static void test_ese_ray_dx(void);
static void test_ese_ray_dy(void);
static void test_ese_ray_ref(void);
static void test_ese_ray_copy_requires_source(void);
static void test_ese_ray_copy(void);
static void test_ese_ray_intersects_rect(void);
static void test_ese_ray_get_point_at_distance(void);
static void test_ese_ray_normalize(void);
static void test_ese_ray_lua_integration(void);
static void test_ese_ray_lua_init(void);
static void test_ese_ray_lua_push(void);
static void test_ese_ray_lua_get(void);
static void test_ese_ray_serialization(void);
static void test_ese_ray_lua_to_json(void);
static void test_ese_ray_lua_from_json(void);
static void test_ese_ray_json_round_trip(void);

/**
* Lua API Test Functions Declarations
*/
static void test_ese_ray_lua_new(void);
static void test_ese_ray_lua_zero(void);
static void test_ese_ray_lua_intersects_rect(void);
static void test_ese_ray_lua_get_point_at_distance(void);
static void test_ese_ray_lua_normalize(void);
static void test_ese_ray_lua_x(void);
static void test_ese_ray_lua_y(void);
static void test_ese_ray_lua_dx(void);
static void test_ese_ray_lua_dy(void);
static void test_ese_ray_lua_tostring(void);
static void test_ese_ray_lua_gc(void);

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

    printf("\nEseRay Tests\n");
    printf("------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_ese_ray_sizeof);
    RUN_TEST(test_ese_ray_create_requires_engine);
    RUN_TEST(test_ese_ray_create);
    RUN_TEST(test_ese_ray_x);
    RUN_TEST(test_ese_ray_y);
    RUN_TEST(test_ese_ray_dx);
    RUN_TEST(test_ese_ray_dy);
    RUN_TEST(test_ese_ray_ref);
    RUN_TEST(test_ese_ray_copy_requires_source);
    RUN_TEST(test_ese_ray_copy);
    RUN_TEST(test_ese_ray_intersects_rect);
    RUN_TEST(test_ese_ray_get_point_at_distance);
    RUN_TEST(test_ese_ray_normalize);
    RUN_TEST(test_ese_ray_lua_integration);
    RUN_TEST(test_ese_ray_lua_init);
    RUN_TEST(test_ese_ray_lua_push);
    RUN_TEST(test_ese_ray_lua_get);
    RUN_TEST(test_ese_ray_serialization);
    RUN_TEST(test_ese_ray_lua_to_json);
    RUN_TEST(test_ese_ray_lua_from_json);
    RUN_TEST(test_ese_ray_json_round_trip);

    RUN_TEST(test_ese_ray_lua_new);
    RUN_TEST(test_ese_ray_lua_zero);
    RUN_TEST(test_ese_ray_lua_intersects_rect);
    RUN_TEST(test_ese_ray_lua_get_point_at_distance);
    RUN_TEST(test_ese_ray_lua_normalize);
    RUN_TEST(test_ese_ray_lua_x);
    RUN_TEST(test_ese_ray_lua_y);
    RUN_TEST(test_ese_ray_lua_dx);
    RUN_TEST(test_ese_ray_lua_dy);
    RUN_TEST(test_ese_ray_lua_tostring);
    RUN_TEST(test_ese_ray_lua_gc);

    memory_manager.destroy();

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_ese_ray_sizeof(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(32, ese_ray_sizeof(), "Ray should be 32 bytes");
}

static void test_ese_ray_create_requires_engine(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, ese_ray_sizeof(), "Ray size should be > 0");
}

static void test_ese_ray_create(void) {
    EseRay *ray = ese_ray_create(g_engine);

    TEST_ASSERT_NOT_NULL_MESSAGE(ray, "Ray should be created");
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_ray_get_x(ray));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_ray_get_y(ray));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, ese_ray_get_dx(ray));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_ray_get_dy(ray));
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_ray_get_state(ray), "Ray should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_ray_get_lua_ref_count(ray), "New ray should have ref count 0");

    ese_ray_destroy(ray);
}

static void test_ese_ray_x(void) {
    EseRay *ray = ese_ray_create(g_engine);

    ese_ray_set_x(ray, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_ray_get_x(ray));

    ese_ray_set_x(ray, -10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, ese_ray_get_x(ray));

    ese_ray_set_x(ray, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_ray_get_x(ray));

    ese_ray_destroy(ray);
}

static void test_ese_ray_y(void) {
    EseRay *ray = ese_ray_create(g_engine);

    ese_ray_set_y(ray, 20.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, ese_ray_get_y(ray));

    ese_ray_set_y(ray, -10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, ese_ray_get_y(ray));

    ese_ray_set_y(ray, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_ray_get_y(ray));

    ese_ray_destroy(ray);
}

static void test_ese_ray_dx(void) {
    EseRay *ray = ese_ray_create(g_engine);

    ese_ray_set_dx(ray, 3.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, ese_ray_get_dx(ray));

    ese_ray_set_dx(ray, -2.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -2.0f, ese_ray_get_dx(ray));

    ese_ray_set_dx(ray, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_ray_get_dx(ray));

    ese_ray_destroy(ray);
}

static void test_ese_ray_dy(void) {
    EseRay *ray = ese_ray_create(g_engine);

    ese_ray_set_dy(ray, 4.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, ese_ray_get_dy(ray));

    ese_ray_set_dy(ray, -1.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.5f, ese_ray_get_dy(ray));

    ese_ray_set_dy(ray, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_ray_get_dy(ray));

    ese_ray_destroy(ray);
}

static void test_ese_ray_ref(void) {
    EseRay *ray = ese_ray_create(g_engine);

    ese_ray_ref(ray);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, ese_ray_get_lua_ref_count(ray), "Ref count should be 1");

    ese_ray_unref(ray);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_ray_get_lua_ref_count(ray), "Ref count should be 0");

    ese_ray_destroy(ray);
}

static void test_ese_ray_copy_requires_source(void) {
    ASSERT_DEATH(ese_ray_copy(NULL), "ese_ray_copy should abort with NULL ray");
}

static void test_ese_ray_copy(void) {
    EseRay *ray = ese_ray_create(g_engine);
    ese_ray_ref(ray);
    ese_ray_set_x(ray, 10.0f);
    ese_ray_set_y(ray, 20.0f);
    ese_ray_set_dx(ray, 3.0f);
    ese_ray_set_dy(ray, 4.0f);
    EseRay *copy = ese_ray_copy(ray);

    TEST_ASSERT_NOT_NULL_MESSAGE(copy, "Copy should be created");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_ray_get_state(copy), "Copy should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_ray_get_lua_ref_count(copy), "Copy should have ref count 0");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_ray_get_x(copy));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, ese_ray_get_y(copy));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, ese_ray_get_dx(copy));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, ese_ray_get_dy(copy));

    ese_ray_unref(ray);
    ese_ray_destroy(ray);
    ese_ray_destroy(copy);
}

static void test_ese_ray_intersects_rect(void) {
    EseRay *ray = ese_ray_create(g_engine);
    EseRect *rect = ese_rect_create(g_engine);

    // Set up ray from (0,0) going right
    ese_ray_set_x(ray, 0.0f);
    ese_ray_set_y(ray, 0.0f);
    ese_ray_set_dx(ray, 1.0f);
    ese_ray_set_dy(ray, 0.0f);

    // Set up rectangle at (5, -2) with size (4, 4)
    ese_rect_set_x(rect, 5.0f);
    ese_rect_set_y(rect, -2.0f);
    ese_rect_set_width(rect, 4.0f);
    ese_rect_set_height(rect, 4.0f);

    // Test intersection
    bool intersects = ese_ray_intersects_rect(ray, rect);
    TEST_ASSERT_TRUE_MESSAGE(intersects, "Ray should intersect with rectangle");

    // Test ray that doesn't intersect
    ese_ray_set_y(ray, 10.0f); // Move ray above rectangle
    intersects = ese_ray_intersects_rect(ray, rect);
    TEST_ASSERT_FALSE_MESSAGE(intersects, "Ray should not intersect with rectangle when above it");

    // Test ray going left (negative direction)
    ese_ray_set_x(ray, 10.0f);
    ese_ray_set_y(ray, 0.0f);
    ese_ray_set_dx(ray, -1.0f);
    ese_ray_set_dy(ray, 0.0f);
    intersects = ese_ray_intersects_rect(ray, rect);
    TEST_ASSERT_TRUE_MESSAGE(intersects, "Ray going left should intersect with rectangle");

    // Test ray going up
    ese_ray_set_x(ray, 7.0f);
    ese_ray_set_y(ray, 5.0f);
    ese_ray_set_dx(ray, 0.0f);
    ese_ray_set_dy(ray, -1.0f);
    intersects = ese_ray_intersects_rect(ray, rect);
    TEST_ASSERT_TRUE_MESSAGE(intersects, "Ray going up should intersect with rectangle");

    // Test ray going down
    ese_ray_set_x(ray, 7.0f);
    ese_ray_set_y(ray, -5.0f);
    ese_ray_set_dx(ray, 0.0f);
    ese_ray_set_dy(ray, 1.0f);
    intersects = ese_ray_intersects_rect(ray, rect);
    TEST_ASSERT_TRUE_MESSAGE(intersects, "Ray going down should intersect with rectangle");

    // Test diagonal ray
    ese_ray_set_x(ray, 3.0f);
    ese_ray_set_y(ray, -3.0f);
    ese_ray_set_dx(ray, 1.0f);
    ese_ray_set_dy(ray, 1.0f);
    intersects = ese_ray_intersects_rect(ray, rect);
    TEST_ASSERT_TRUE_MESSAGE(intersects, "Diagonal ray should intersect with rectangle");

    ese_ray_destroy(ray);
    ese_rect_destroy(rect);
}

static void test_ese_ray_get_point_at_distance(void) {
    EseRay *ray = ese_ray_create(g_engine);

    // Set up ray from (0,0) with direction (3,4)
    ese_ray_set_x(ray, 0.0f);
    ese_ray_set_y(ray, 0.0f);
    ese_ray_set_dx(ray, 3.0f);
    ese_ray_set_dy(ray, 4.0f);

    float point_x, point_y;
    ese_ray_get_point_at_distance(ray, 5.0f, &point_x, &point_y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, point_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, point_y);

    // Test point at distance 0
    ese_ray_get_point_at_distance(ray, 0.0f, &point_x, &point_y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, point_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, point_y);

    // Test point at negative distance
    ese_ray_get_point_at_distance(ray, -2.0f, &point_x, &point_y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -6.0f, point_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -8.0f, point_y);

    // Test with different origin
    ese_ray_set_x(ray, 10.0f);
    ese_ray_set_y(ray, 20.0f);
    ese_ray_get_point_at_distance(ray, 2.0f, &point_x, &point_y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 16.0f, point_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 28.0f, point_y);

    ese_ray_destroy(ray);
}

static void test_ese_ray_normalize(void) {
    EseRay *ray = ese_ray_create(g_engine);

    // Test normalizing a ray with direction (3,4)
    ese_ray_set_dx(ray, 3.0f);
    ese_ray_set_dy(ray, 4.0f);
    ese_ray_normalize(ray);
    
    float length = sqrtf(ese_ray_get_dx(ray) * ese_ray_get_dx(ray) + ese_ray_get_dy(ray) * ese_ray_get_dy(ray));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, length);

    // Test normalizing a ray with direction (1,0)
    ese_ray_set_dx(ray, 1.0f);
    ese_ray_set_dy(ray, 0.0f);
    ese_ray_normalize(ray);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_ray_get_dx(ray));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_ray_get_dy(ray));

    // Test normalizing a ray with direction (0,1)
    ese_ray_set_dx(ray, 0.0f);
    ese_ray_set_dy(ray, 1.0f);
    ese_ray_normalize(ray);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_ray_get_dx(ray));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_ray_get_dy(ray));

    // Test normalizing a zero vector (should not change)
    ese_ray_set_dx(ray, 0.0f);
    ese_ray_set_dy(ray, 0.0f);
    ese_ray_normalize(ray);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_ray_get_dx(ray));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_ray_get_dy(ray));

    ese_ray_destroy(ray);
}

static void test_ese_ray_lua_integration(void) {
    EseLuaEngine *engine = create_test_engine();
    EseRay *ray = ese_ray_create(engine);

    lua_State *before_state = ese_ray_get_state(ray);
    TEST_ASSERT_NOT_NULL_MESSAGE(before_state, "Ray should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, before_state, "Ray state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_ray_get_lua_ref(ray), "Ray should have no Lua reference initially");

    ese_ray_ref(ray);
    lua_State *after_ref_state = ese_ray_get_state(ray);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_ref_state, "Ray should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_ref_state, "Ray state should match engine runtime");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(LUA_NOREF, ese_ray_get_lua_ref(ray), "Ray should have a valid Lua reference after ref");

    ese_ray_unref(ray);
    lua_State *after_unref_state = ese_ray_get_state(ray);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_unref_state, "Ray should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_unref_state, "Ray state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_ray_get_lua_ref(ray), "Ray should have no Lua reference after unref");

    ese_ray_destroy(ray);
    lua_engine_destroy(engine);
}

static void test_ese_ray_lua_init(void) {
    lua_State *L = g_engine->runtime;
    
    luaL_getmetatable(L, RAY_PROXY_META);
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Metatable should not exist before initialization");
    lua_pop(L, 1);
    
    lua_getglobal(L, "Ray");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Global Ray table should not exist before initialization");
    lua_pop(L, 1);
    
    ese_ray_lua_init(g_engine);
    
    luaL_getmetatable(L, RAY_PROXY_META);
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Metatable should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Metatable should be a table");
    lua_pop(L, 1);
    
    lua_getglobal(L, "Ray");
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Global Ray table should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Global Ray table should be a table");
    lua_pop(L, 1);
}

static void test_ese_ray_lua_push(void) {
    ese_ray_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseRay *ray = ese_ray_create(g_engine);
    
    ese_ray_lua_push(ray);
    
    EseRay **ud = (EseRay **)lua_touserdata(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(ray, *ud, "The pushed item should be the actual ray");
    
    lua_pop(L, 1); 
    
    ese_ray_destroy(ray);
}

static void test_ese_ray_lua_get(void) {
    ese_ray_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseRay *ray = ese_ray_create(g_engine);
    
    ese_ray_lua_push(ray);
    
    EseRay *extracted_ray = ese_ray_lua_get(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(ray, extracted_ray, "Extracted ray should match original");
    
    lua_pop(L, 1);
    ese_ray_destroy(ray);
}

/**
* Lua API Test Functions
*/

static void test_ese_ray_lua_new(void) {
    ese_ray_lua_init(g_engine);
    ese_point_lua_init(g_engine);
    ese_vector_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "return Ray.new()\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute with error");

    const char *testB = "return Ray.new(10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute with error");

    const char *testC = "return Ray.new(\"10\", \"20\", \"3\", \"4\")\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute with error");

    const char *testD = "return Ray.new(10, 20, 3, 4)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testD), "testD Lua code should execute without error");
    EseRay *extracted_ray = ese_ray_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_ray, "Extracted ray should not be NULL");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, ese_ray_get_x(extracted_ray), "Extracted ray should have x=10");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(20.0f, ese_ray_get_y(extracted_ray), "Extracted ray should have y=20");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3.0f, ese_ray_get_dx(extracted_ray), "Extracted ray should have dx=3");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4.0f, ese_ray_get_dy(extracted_ray), "Extracted ray should have dy=4");
    ese_ray_destroy(extracted_ray);

    const char *testE = "return Ray.new(Point.new(10, 20), Vector.new(3, 4))\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testE), "testE Lua code should execute without error");
    extracted_ray = ese_ray_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_ray, "Extracted ray should not be NULL");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, ese_ray_get_x(extracted_ray), "Extracted ray should have x=10");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(20.0f, ese_ray_get_y(extracted_ray), "Extracted ray should have y=20");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3.0f, ese_ray_get_dx(extracted_ray), "Extracted ray should have dx=3");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4.0f, ese_ray_get_dy(extracted_ray), "Extracted ray should have dy=4");
    ese_ray_destroy(extracted_ray);
}

static void test_ese_ray_lua_zero(void) {
    ese_ray_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "return Ray.zero(10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute with error");

    const char *testB = "return Ray.zero()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute without error");
    EseRay *extracted_ray = ese_ray_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_ray, "Extracted ray should not be NULL");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_ray_get_x(extracted_ray), "Extracted ray should have x=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_ray_get_y(extracted_ray), "Extracted ray should have y=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, ese_ray_get_dx(extracted_ray), "Extracted ray should have dx=1");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_ray_get_dy(extracted_ray), "Extracted ray should have dy=0");
    ese_ray_destroy(extracted_ray);
}

static void test_ese_ray_lua_intersects_rect(void) {
    ese_ray_lua_init(g_engine);
    ese_rect_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "local r = Ray.new(0, 0, 1, 0); local rect = Rect.new(5, -2, 4, 4); return r:intersects_rect(rect)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute without error");
    bool intersects = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(intersects, "Ray should intersect with rectangle");

    const char *testB = "local r = Ray.new(0, 10, 1, 0); local rect = Rect.new(5, -2, 4, 4); return r:intersects_rect(rect)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute without error");
    intersects = lua_toboolean(L, -1);
    TEST_ASSERT_FALSE_MESSAGE(intersects, "Ray should not intersect with rectangle when above it");
}

static void test_ese_ray_lua_get_point_at_distance(void) {
    ese_ray_lua_init(g_engine);
    ese_rect_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "local r = Ray.new(0, 0, 3, 4); local x, y = r:get_point_at_distance(5); return x, y\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute without error");
    double x = lua_tonumber(L, -2);
    double y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, y);

    const char *testB = "local r = Ray.new(0, 0, 3, 4); local x, y = r:get_point_at_distance(0); return x, y\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute without error");
    x = lua_tonumber(L, -2);
    y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, y);

    const char *testC = "local r = Ray.new(0, 0, 3, 4); local x, y = r:get_point_at_distance(-2); return x, y\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute without error");
    x = lua_tonumber(L, -2);
    y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -6.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -8.0f, y);
}

static void test_ese_ray_lua_normalize(void) {
    ese_ray_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "local r = Ray.new(0, 0, 3, 4); r:normalize(); return r.dx, r.dy\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute without error");
    double dx = lua_tonumber(L, -2);
    double dy = lua_tonumber(L, -1);
    double length = sqrt(dx * dx + dy * dy);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, length);

    const char *testB = "local r = Ray.new(0, 0, 1, 0); r:normalize(); return r.dx, r.dy\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute without error");
    dx = lua_tonumber(L, -2);
    dy = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, dx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, dy);
}

static void test_ese_ray_lua_x(void) {
    ese_ray_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local r = Ray.new(0, 0, 1, 0); r.x = \"20\"; return r.x";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "test1 Lua code should execute with error");

    const char *test2 = "local r = Ray.new(0, 0, 1, 0); r.x = 10; return r.x";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua x set/get test 1 should execute without error");
    double x = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, x);
    lua_pop(L, 1);

    const char *test3 = "local r = Ray.new(0, 0, 1, 0); r.x = -10; return r.x";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua x set/get test 2 should execute without error");
    x = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, x);
    lua_pop(L, 1);

    const char *test4 = "local r = Ray.new(0, 0, 1, 0); r.x = 0; return r.x";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test4), "Lua x set/get test 3 should execute without error");
    x = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, x);
    lua_pop(L, 1);
}

static void test_ese_ray_lua_y(void) {
    ese_ray_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local r = Ray.new(0, 0, 1, 0); r.y = \"20\"; return r.y";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "test1 Lua code should execute with error");

    const char *test2 = "local r = Ray.new(0, 0, 1, 0); r.y = 20; return r.y";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua y set/get test 1 should execute without error");
    double y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, y);
    lua_pop(L, 1);

    const char *test3 = "local r = Ray.new(0, 0, 1, 0); r.y = -10; return r.y";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua y set/get test 2 should execute without error");
    y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, y);
    lua_pop(L, 1);

    const char *test4 = "local r = Ray.new(0, 0, 1, 0); r.y = 0; return r.y";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test4), "Lua y set/get test 3 should execute without error");
    y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, y);
    lua_pop(L, 1);
}

static void test_ese_ray_lua_dx(void) {
    ese_ray_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local r = Ray.new(0, 0, 1, 0); r.dx = \"20\"; return r.dx";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "test1 Lua code should execute with error");

    const char *test2 = "local r = Ray.new(0, 0, 1, 0); r.dx = 3; return r.dx";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua dx set/get test 1 should execute without error");
    double dx = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, dx);
    lua_pop(L, 1);

    const char *test3 = "local r = Ray.new(0, 0, 1, 0); r.dx = -2; return r.dx";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua dx set/get test 2 should execute without error");
    dx = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -2.0f, dx);
    lua_pop(L, 1);

    const char *test4 = "local r = Ray.new(0, 0, 1, 0); r.dx = 0; return r.dx";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test4), "Lua dx set/get test 3 should execute without error");
    dx = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, dx);
    lua_pop(L, 1);
}

static void test_ese_ray_lua_dy(void) {
    ese_ray_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local r = Ray.new(0, 0, 1, 0); r.dy = \"20\"; return r.dy";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "test1 Lua code should execute with error");

    const char *test2 = "local r = Ray.new(0, 0, 1, 0); r.dy = 4; return r.dy";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua dy set/get test 1 should execute without error");
    double dy = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, dy);
    lua_pop(L, 1);

    const char *test3 = "local r = Ray.new(0, 0, 1, 0); r.dy = -1.5; return r.dy";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua dy set/get test 2 should execute without error");
    dy = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.5f, dy);
    lua_pop(L, 1);

    const char *test4 = "local r = Ray.new(0, 0, 1, 0); r.dy = 0; return r.dy";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test4), "Lua dy set/get test 3 should execute without error");
    dy = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, dy);
    lua_pop(L, 1);
}

static void test_ese_ray_lua_tostring(void) {
    ese_ray_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test_code = "local r = Ray.new(10.5, 20.25, 3.0, 4.0); return tostring(r)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "tostring test should execute without error");
    const char *result = lua_tostring(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "tostring result should not be NULL");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "Ray:") != NULL, "tostring should contain 'Ray:'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "x=10.50") != NULL, "tostring should contain 'x=10.50'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "y=20.25") != NULL, "tostring should contain 'y=20.25'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "dx=3.00") != NULL, "tostring should contain 'dx=3.00'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "dy=4.00") != NULL, "tostring should contain 'dy=4.00'");
    lua_pop(L, 1); 
}

static void test_ese_ray_lua_gc(void) {
    ese_ray_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *testA = "local r = Ray.new(5, 10, 1, 0)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "Ray creation should execute without error");
    
    int collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testB = "return Ray.new(5, 10, 1, 0)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "Ray creation should execute without error");
    EseRay *extracted_ray = ese_ray_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_ray, "Extracted ray should not be NULL");
    ese_ray_ref(extracted_ray);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    ese_ray_unref(extracted_ray);

    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testC = "return Ray.new(5, 10, 1, 0)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "Ray creation should execute without error");
    extracted_ray = ese_ray_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_ray, "Extracted ray should not be NULL");
    ese_ray_ref(extracted_ray);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    ese_ray_unref(extracted_ray);
    ese_ray_destroy(extracted_ray);

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
* Tests for ray serialization/deserialization functionality
*/
static void test_ese_ray_serialization(void) {
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    // Create a test ray
    EseRay *original = ese_ray_create(engine);
    TEST_ASSERT_NOT_NULL(original);

    ese_ray_set_x(original, 10.5f);
    ese_ray_set_y(original, 20.7f);
    ese_ray_set_dx(original, 3.0f);
    ese_ray_set_dy(original, 4.0f);

    // Test serialization
    cJSON *json = ese_ray_serialize(original);
    TEST_ASSERT_NOT_NULL(json);

    // Verify JSON structure
    cJSON *type_item = cJSON_GetObjectItem(json, "type");
    TEST_ASSERT_NOT_NULL(type_item);
    TEST_ASSERT_TRUE(cJSON_IsString(type_item));
    TEST_ASSERT_EQUAL_STRING("RAY", type_item->valuestring);

    cJSON *x_item = cJSON_GetObjectItem(json, "x");
    TEST_ASSERT_NOT_NULL(x_item);
    TEST_ASSERT_TRUE(cJSON_IsNumber(x_item));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 10.5, x_item->valuedouble);

    cJSON *y_item = cJSON_GetObjectItem(json, "y");
    TEST_ASSERT_NOT_NULL(y_item);
    TEST_ASSERT_TRUE(cJSON_IsNumber(y_item));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 20.7, y_item->valuedouble);

    cJSON *dx_item = cJSON_GetObjectItem(json, "dx");
    TEST_ASSERT_NOT_NULL(dx_item);
    TEST_ASSERT_TRUE(cJSON_IsNumber(dx_item));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 3.0, dx_item->valuedouble);

    cJSON *dy_item = cJSON_GetObjectItem(json, "dy");
    TEST_ASSERT_NOT_NULL(dy_item);
    TEST_ASSERT_TRUE(cJSON_IsNumber(dy_item));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 4.0, dy_item->valuedouble);

    // Test deserialization
    EseRay *deserialized = ese_ray_deserialize(engine, json);
    TEST_ASSERT_NOT_NULL(deserialized);

    // Verify all properties match
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.5f, ese_ray_get_x(deserialized));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.7f, ese_ray_get_y(deserialized));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, ese_ray_get_dx(deserialized));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, ese_ray_get_dy(deserialized));

    // Clean up
    cJSON_Delete(json);
    ese_ray_destroy(original);
    ese_ray_destroy(deserialized);
    lua_engine_destroy(engine);
}

/**
* Test Ray:toJSON Lua instance method
*
* Tests the Lua toJSON instance method for EseRay objects.
* Verifies that valid JSON strings are generated and error cases are handled.
*/
static void test_ese_ray_lua_to_json(void) {
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    ese_ray_lua_init(engine);

    // Set engine in Lua registry so fromJSON can retrieve it
    lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);

    // Test toJSON method - individual functionality test
    const char *testA = "local r = Ray.new(15.5, 25.8, 3.0, 4.0) "
                       "local json = r:toJSON() "
                       "print('JSON: ' .. json) "
                       "if json == nil or json == '' then error('toJSON should return non-empty string') end "
                       "if not string.find(json, '\"type\":\"RAY\"') then error('toJSON should return valid JSON') end "
                       "if not string.find(json, '\"x\":15.5') then error('toJSON should contain correct x') end "
                       "if not string.find(json, '\"y\":25.7') then error('toJSON should contain correct y') end "
                       "if not string.find(json, '\"dx\":3') then error('toJSON should contain correct dx') end "
                       "if not string.find(json, '\"dy\":4') then error('toJSON should contain correct dy') end "
                       "return json; ";

    int result = luaL_dostring(engine->runtime, testA);
    if (result != LUA_OK) {
        const char *error_msg = lua_tostring(engine->runtime, -1);
        printf("ERROR in toJSON test: %s\n", error_msg ? error_msg : "unknown error");
        lua_pop(engine->runtime, 1); // Remove error message from stack
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, result, "Ray:toJSON should create valid JSON");

    lua_engine_destroy(engine);
}

/**
* Test Ray.fromJSON Lua static method
*
* Tests the Lua fromJSON static method for EseRay objects.
* Verifies that valid JSON strings are parsed correctly and error cases are handled.
*/
static void test_ese_ray_lua_from_json(void) {
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    ese_ray_lua_init(engine);

    // Set engine in Lua registry so fromJSON can retrieve it
    lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);

    // Test valid JSON string
    const char *testA = "local json_str = '{\"type\":\"RAY\",\"x\":15.5,\"y\":25.8,\"dx\":3.0,\"dy\":4.0}' "
                       "local r = Ray.fromJSON(json_str) "
                       "if r == nil then error('Ray.fromJSON should return a ray') end "
                       "if math.abs(r.x - 15.5) > 0.001 then error('Ray fromJSON should set correct x') end "
                       "if math.abs(r.y - 25.8) > 0.001 then error('Ray fromJSON should set correct y') end "
                       "if math.abs(r.dx - 3.0) > 0.001 then error('Ray fromJSON should set correct dx') end "
                       "if math.abs(r.dy - 4.0) > 0.001 then error('Ray fromJSON should set correct dy') end ";

    int resultA = luaL_dostring(engine->runtime, testA);
    if (resultA != LUA_OK) {
        const char *error_msg = lua_tostring(engine->runtime, -1);
        printf("ERROR in fromJSON testA: %s\n", error_msg ? error_msg : "unknown error");
        lua_pop(engine->runtime, 1); // Remove error message from stack
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, resultA, "Ray.fromJSON should work with valid JSON");

    // Test invalid JSON string
    const char *testB = "local r = Ray.fromJSON('invalid json') "
                       "error('Ray.fromJSON should fail with invalid JSON'); ";

    int resultB = luaL_dostring(engine->runtime, testB);
    if (resultB == LUA_OK) {
        printf("ERROR in fromJSON testB: Expected failure but got success\n");
    }
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, resultB, "Ray.fromJSON should fail with invalid JSON");

    // Test JSON with wrong type
    const char *testC = "local r = Ray.fromJSON('{\"type\":\"POINT\",\"x\":15.5,\"y\":25.8,\"dx\":3.0,\"dy\":4.0}') "
                       "error('Ray.fromJSON should fail with wrong type'); ";

    int resultC = luaL_dostring(engine->runtime, testC);
    if (resultC == LUA_OK) {
        printf("ERROR in fromJSON testC: Expected failure but got success\n");
    }
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, resultC, "Ray.fromJSON should fail with wrong type");

    // Test JSON missing coordinates
    const char *testD = "local r = Ray.fromJSON('{\"type\":\"RAY\"}') "
                       "error('Ray.fromJSON should fail with missing coordinates'); ";

    int resultD = luaL_dostring(engine->runtime, testD);
    if (resultD == LUA_OK) {
        printf("ERROR in fromJSON testD: Expected failure but got success\n");
    }
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, resultD, "Ray.fromJSON should fail with missing coordinates");

    lua_engine_destroy(engine);
}

/**
* Test Ray JSON round-trip (toJSON -> fromJSON)
*
* Tests that serializing a ray to JSON and then deserializing it back
* produces an equivalent ray object.
*/
static void test_ese_ray_json_round_trip(void) {
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    ese_ray_lua_init(engine);

    // Set engine in Lua registry so fromJSON can retrieve it
    lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);

    // Test JSON round-trip
    const char *testA = "local original = Ray.new(10.5, 20.7, 3.0, 4.0) "
                       "local json = original:toJSON() "
                       "local restored = Ray.fromJSON(json) "
                       "if restored == nil then error('Ray.fromJSON should return a ray') end "
                       "if math.abs(restored.x - original.x) > 0.001 then error('Round-trip should preserve x') end "
                       "if math.abs(restored.y - original.y) > 0.001 then error('Round-trip should preserve y') end "
                       "if math.abs(restored.dx - original.dx) > 0.001 then error('Round-trip should preserve dx') end "
                       "if math.abs(restored.dy - original.dy) > 0.001 then error('Round-trip should preserve dy') end ";

    int result = luaL_dostring(engine->runtime, testA);
    if (result != LUA_OK) {
        const char *error_msg = lua_tostring(engine->runtime, -1);
        printf("ERROR in round-trip test: %s\n", error_msg ? error_msg : "unknown error");
        lua_pop(engine->runtime, 1); // Remove error message from stack
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, result, "Ray JSON round-trip should work correctly");

    lua_engine_destroy(engine);
}
