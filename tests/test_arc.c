/*
* test_ese_arc.c - Unity-based tests for arc functionality
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

#include "../src/types/arc.h"
#include "../src/types/rect.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"
#include "../src/scripting/lua_engine.h"
#include "../src/vendor/json/cJSON.h"

/**
* C API Test Functions Declarations
*/
static void test_ese_arc_sizeof(void);
static void test_ese_arc_create_requires_engine(void);
static void test_ese_arc_create(void);
static void test_ese_arc_x(void);
static void test_ese_arc_y(void);
static void test_ese_arc_radius(void);
static void test_ese_arc_start_angle(void);
static void test_ese_arc_end_angle(void);
static void test_ese_arc_ref(void);
static void test_ese_arc_copy_requires_engine(void);
static void test_ese_arc_copy(void);
static void test_ese_arc_contains_point(void);
static void test_ese_arc_get_length(void);
static void test_ese_arc_get_point_at_angle(void);
static void test_ese_arc_intersects_rect(void);
static void test_ese_arc_watcher_system(void);
static void test_ese_arc_lua_integration(void);
static void test_ese_arc_lua_init(void);
static void test_ese_arc_lua_push(void);
static void test_ese_arc_lua_get(void);
static void test_ese_arc_serialization(void);
static void test_ese_arc_lua_to_json(void);
static void test_ese_arc_lua_from_json(void);
static void test_ese_arc_json_round_trip(void);

/**
* Lua API Test Functions Declarations
*/
static void test_ese_arc_lua_new(void);
static void test_ese_arc_lua_zero(void);
static void test_ese_arc_lua_contains_point(void);
static void test_ese_arc_lua_get_length(void);
static void test_ese_arc_lua_get_point_at_angle(void);
static void test_ese_arc_lua_intersects_rect(void);
static void test_ese_arc_lua_x(void);
static void test_ese_arc_lua_y(void);
static void test_ese_arc_lua_radius(void);
static void test_ese_arc_lua_start_angle(void);
static void test_ese_arc_lua_end_angle(void);
static void test_ese_arc_lua_tostring(void);
static void test_ese_arc_lua_gc(void);

/**
* Mock watcher callback for testing
*/
static bool watcher_called = false;
static EseArc *last_watched_arc = NULL;
static void *last_watcher_userdata = NULL;

static void test_watcher_callback(EseArc *arc, void *userdata) {
    watcher_called = true;
    last_watched_arc = arc;
    last_watcher_userdata = userdata;
}

static void mock_reset(void) {
    watcher_called = false;
    last_watched_arc = NULL;
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

    printf("\nEseArc Tests\n");
    printf("------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_ese_arc_sizeof);
    RUN_TEST(test_ese_arc_create_requires_engine);
    RUN_TEST(test_ese_arc_create);
    RUN_TEST(test_ese_arc_x);
    RUN_TEST(test_ese_arc_y);
    RUN_TEST(test_ese_arc_radius);
    RUN_TEST(test_ese_arc_start_angle);
    RUN_TEST(test_ese_arc_end_angle);
    RUN_TEST(test_ese_arc_ref);
    RUN_TEST(test_ese_arc_copy_requires_engine);
    RUN_TEST(test_ese_arc_copy);
    RUN_TEST(test_ese_arc_contains_point);
    RUN_TEST(test_ese_arc_get_length);
    RUN_TEST(test_ese_arc_get_point_at_angle);
    RUN_TEST(test_ese_arc_intersects_rect);
    RUN_TEST(test_ese_arc_watcher_system);
    RUN_TEST(test_ese_arc_lua_integration);
    RUN_TEST(test_ese_arc_lua_init);
    RUN_TEST(test_ese_arc_lua_push);
    RUN_TEST(test_ese_arc_lua_get);
    RUN_TEST(test_ese_arc_serialization);
    RUN_TEST(test_ese_arc_lua_to_json);
    RUN_TEST(test_ese_arc_lua_from_json);
    RUN_TEST(test_ese_arc_json_round_trip);

    RUN_TEST(test_ese_arc_lua_new);
    RUN_TEST(test_ese_arc_lua_zero);
    RUN_TEST(test_ese_arc_lua_contains_point);
    RUN_TEST(test_ese_arc_lua_get_length);
    RUN_TEST(test_ese_arc_lua_get_point_at_angle);
    RUN_TEST(test_ese_arc_lua_intersects_rect);
    RUN_TEST(test_ese_arc_lua_x);
    RUN_TEST(test_ese_arc_lua_y);
    RUN_TEST(test_ese_arc_lua_radius);
    RUN_TEST(test_ese_arc_lua_start_angle);
    RUN_TEST(test_ese_arc_lua_end_angle);
    RUN_TEST(test_ese_arc_lua_tostring);
    RUN_TEST(test_ese_arc_lua_gc);

    memory_manager.destroy(true);

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_ese_arc_sizeof(void) {
    // Sizeof test removed since EseArc is now opaque
    TEST_PASS();
}

static void test_ese_arc_create_requires_engine(void) {
    TEST_ASSERT_DEATH(ese_arc_create(NULL), "ese_arc_create should abort with NULL engine");
}

static void test_ese_arc_create(void) {
    EseArc *arc = ese_arc_create(g_engine);

    TEST_ASSERT_NOT_NULL_MESSAGE(arc, "Arc should be created");
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_arc_get_x(arc));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_arc_get_y(arc));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, ese_arc_get_radius(arc));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_arc_get_start_angle(arc));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.0f * M_PI, ese_arc_get_end_angle(arc));
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_arc_get_state(arc), "Arc should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_arc_get_lua_ref_count(arc), "New arc should have ref count 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_arc_get_lua_ref(arc), "New arc should have LUA_NOREF value");

    ese_arc_destroy(arc);
}

static void test_ese_arc_x(void) {
    EseArc *arc = ese_arc_create(g_engine);

    ese_arc_set_x(arc, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_arc_get_x(arc));

    ese_arc_set_x(arc, -10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, ese_arc_get_x(arc));

    ese_arc_set_x(arc, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_arc_get_x(arc));

    ese_arc_destroy(arc);
}

static void test_ese_arc_y(void) {
    EseArc *arc = ese_arc_create(g_engine);

    ese_arc_set_y(arc, 20.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, ese_arc_get_y(arc));

    ese_arc_set_y(arc, -10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, ese_arc_get_y(arc));

    ese_arc_set_y(arc, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_arc_get_y(arc));

    ese_arc_destroy(arc);
}

static void test_ese_arc_radius(void) {
    EseArc *arc = ese_arc_create(g_engine);

    ese_arc_set_radius(arc, 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, ese_arc_get_radius(arc));

    ese_arc_set_radius(arc, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, ese_arc_get_radius(arc));

    ese_arc_set_radius(arc, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_arc_get_radius(arc));

    ese_arc_destroy(arc);
}

static void test_ese_arc_start_angle(void) {
    EseArc *arc = ese_arc_create(g_engine);

    ese_arc_set_start_angle(arc, M_PI / 4.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, M_PI / 4.0f, ese_arc_get_start_angle(arc));

    ese_arc_set_start_angle(arc, -M_PI / 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -M_PI / 2.0f, ese_arc_get_start_angle(arc));

    ese_arc_set_start_angle(arc, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_arc_get_start_angle(arc));

    ese_arc_destroy(arc);
}

static void test_ese_arc_end_angle(void) {
    EseArc *arc = ese_arc_create(g_engine);

    ese_arc_set_end_angle(arc, 3.0f * M_PI / 4.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f * M_PI / 4.0f, ese_arc_get_end_angle(arc));

    ese_arc_set_end_angle(arc, -M_PI / 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -M_PI / 2.0f, ese_arc_get_end_angle(arc));

    ese_arc_set_end_angle(arc, 2.0f * M_PI);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f * M_PI, ese_arc_get_end_angle(arc));

    ese_arc_destroy(arc);
}

static void test_ese_arc_ref(void) {
    EseArc *arc = ese_arc_create(g_engine);

    ese_arc_ref(arc);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, ese_arc_get_lua_ref_count(arc), "Ref count should be 1");

    ese_arc_unref(arc);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_arc_get_lua_ref_count(arc), "Ref count should be 0");

    ese_arc_destroy(arc);
}

static void test_ese_arc_copy_requires_engine(void) {
    // ese_arc_copy should handle NULL gracefully (not abort)
    EseArc *result = ese_arc_copy(NULL);
    TEST_ASSERT_NULL_MESSAGE(result, "ese_arc_copy should return NULL for NULL input");
}

static void test_ese_arc_copy(void) {
    EseArc *arc = ese_arc_create(g_engine);
    ese_arc_ref(arc);
    ese_arc_set_x(arc, 10.0f);
    ese_arc_set_y(arc, 20.0f);
    ese_arc_set_radius(arc, 5.0f);
    ese_arc_set_start_angle(arc, M_PI / 4.0f);
    ese_arc_set_end_angle(arc, 3.0f * M_PI / 4.0f);
    EseArc *copy = ese_arc_copy(arc);

    TEST_ASSERT_NOT_NULL_MESSAGE(copy, "Copy should be created");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_arc_get_state(copy), "Copy should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_arc_get_lua_ref_count(copy), "Copy should have ref count 0");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_arc_get_x(copy));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, ese_arc_get_y(copy));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, ese_arc_get_radius(copy));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, M_PI / 4.0f, ese_arc_get_start_angle(copy));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f * M_PI / 4.0f, ese_arc_get_end_angle(copy));

    ese_arc_unref(arc);
    ese_arc_destroy(arc);
    ese_arc_destroy(copy);
}

static void test_ese_arc_contains_point(void) {
    EseArc *arc = ese_arc_create(g_engine);

    ese_arc_set_x(arc, 0.0f);
    ese_arc_set_y(arc, 0.0f);
    ese_arc_set_radius(arc, 2.0f);
    ese_arc_set_start_angle(arc, 0.0f);
    ese_arc_set_end_angle(arc, 2.0f * M_PI);

    TEST_ASSERT_TRUE_MESSAGE(ese_arc_contains_point(arc, 2.0f, 0.0f, 0.1f), "Point on arc should be contained");
    TEST_ASSERT_TRUE_MESSAGE(ese_arc_contains_point(arc, 0.0f, 2.0f, 0.1f), "Point on arc should be contained");
    TEST_ASSERT_FALSE_MESSAGE(ese_arc_contains_point(arc, 3.0f, 0.0f, 0.1f), "Point outside arc should not be contained");
    TEST_ASSERT_FALSE_MESSAGE(ese_arc_contains_point(arc, 1.0f, 1.0f, 0.1f), "Point inside circle but not on arc should not be contained");

    // Test partial arc
    ese_arc_set_start_angle(arc, 0.0f);
    ese_arc_set_end_angle(arc, M_PI / 2.0f); // 90 degrees
    TEST_ASSERT_TRUE_MESSAGE(ese_arc_contains_point(arc, 2.0f, 0.0f, 0.1f), "Point on start of arc should be contained");
    TEST_ASSERT_TRUE_MESSAGE(ese_arc_contains_point(arc, 0.0f, 2.0f, 0.1f), "Point on end of arc should be contained");
    TEST_ASSERT_FALSE_MESSAGE(ese_arc_contains_point(arc, -2.0f, 0.0f, 0.1f), "Point on opposite side should not be contained");

    ese_arc_destroy(arc);
}

static void test_ese_arc_get_length(void) {
    EseArc *arc = ese_arc_create(g_engine);

    ese_arc_set_radius(arc, 2.0f);
    ese_arc_set_start_angle(arc, 0.0f);
    ese_arc_set_end_angle(arc, 2.0f * M_PI);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f * M_PI * 2.0f, ese_arc_get_length(arc));

    ese_arc_set_start_angle(arc, 0.0f);
    ese_arc_set_end_angle(arc, M_PI); // 180 degrees
    TEST_ASSERT_FLOAT_WITHIN(0.001f, M_PI * 2.0f, ese_arc_get_length(arc));

    ese_arc_set_start_angle(arc, 0.0f);
    ese_arc_set_end_angle(arc, M_PI / 2.0f); // 90 degrees
    TEST_ASSERT_FLOAT_WITHIN(0.001f, M_PI * 2.0f / 2.0f, ese_arc_get_length(arc));

    ese_arc_destroy(arc);
}

static void test_ese_arc_get_point_at_angle(void) {
    EseArc *arc = ese_arc_create(g_engine);

    ese_arc_set_x(arc, 0.0f);
    ese_arc_set_y(arc, 0.0f);
    ese_arc_set_radius(arc, 2.0f);
    ese_arc_set_start_angle(arc, 0.0f);
    ese_arc_set_end_angle(arc, 2.0f * M_PI);
    
    float point_x, point_y;
    bool result = ese_arc_get_point_at_angle(arc, M_PI / 2.0f, &point_x, &point_y);
    TEST_ASSERT_TRUE_MESSAGE(result, "Should return true for valid angle");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, point_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, point_y);

    result = ese_arc_get_point_at_angle(arc, 0.0f, &point_x, &point_y);
    TEST_ASSERT_TRUE_MESSAGE(result, "Should return true for valid angle");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, point_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, point_y);

    result = ese_arc_get_point_at_angle(arc, M_PI, &point_x, &point_y);
    TEST_ASSERT_TRUE_MESSAGE(result, "Should return true for valid angle");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -2.0f, point_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, point_y);

    // Test partial arc
    ese_arc_set_start_angle(arc, 0.0f);
    ese_arc_set_end_angle(arc, M_PI / 2.0f);
    result = ese_arc_get_point_at_angle(arc, M_PI, &point_x, &point_y);
    TEST_ASSERT_FALSE_MESSAGE(result, "Should return false for angle outside arc range");

    ese_arc_destroy(arc);
}

static void test_ese_arc_intersects_rect(void) {
    EseArc *arc = ese_arc_create(g_engine);
    EseRect *rect = ese_rect_create(g_engine);

    ese_arc_set_x(arc, 0.0f);
    ese_arc_set_y(arc, 0.0f);
    ese_arc_set_radius(arc, 2.0f);
    ese_arc_set_start_angle(arc, 0.0f);
    ese_arc_set_end_angle(arc, 2.0f * M_PI);
    
    ese_rect_set_x(rect, 1.0f);
    ese_rect_set_y(rect, 1.0f);
    ese_rect_set_width(rect, 2.0f);
    ese_rect_set_height(rect, 2.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_arc_intersects_rect(arc, rect), "Arc should intersect with rectangle");

    ese_arc_set_x(arc, 10.0f); // Move arc away from rectangle
    TEST_ASSERT_FALSE_MESSAGE(ese_arc_intersects_rect(arc, rect), "Arc should not intersect with rectangle when far away");
    
    ese_arc_destroy(arc);
    ese_rect_destroy(rect);
}

static void test_ese_arc_watcher_system(void) {
    EseArc *arc = ese_arc_create(g_engine);

    mock_reset();
    ese_arc_set_x(arc, 25.0f);
    TEST_ASSERT_FALSE_MESSAGE(watcher_called, "Watcher should not be called before adding");

    void *test_userdata = (void*)0x12345678;
    // Note: Arc doesn't have watcher system like Point/Rect, so this test is placeholder
    // bool add_result = ese_arc_add_watcher(arc, test_watcher_callback, test_userdata);
    // TEST_ASSERT_TRUE_MESSAGE(add_result, "Should successfully add watcher");

    mock_reset();
    ese_arc_set_x(arc, 50.0f);
    // TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when x changes");

    ese_arc_destroy(arc);
}

static void test_ese_arc_lua_integration(void) {
    EseLuaEngine *engine = create_test_engine();
    EseArc *arc = ese_arc_create(engine);
    
    lua_State *before_state = ese_arc_get_state(arc);
    TEST_ASSERT_NOT_NULL_MESSAGE(before_state, "Arc should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, before_state, "Arc state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_arc_get_lua_ref(arc), "Arc should have no Lua reference initially");

    ese_arc_ref(arc);
    lua_State *after_ref_state = ese_arc_get_state(arc);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_ref_state, "Arc should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_ref_state, "Arc state should match engine runtime");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(LUA_NOREF, ese_arc_get_lua_ref(arc), "Arc should have a valid Lua reference after ref");

    ese_arc_unref(arc);
    lua_State *after_unref_state = ese_arc_get_state(arc);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_unref_state, "Arc should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_unref_state, "Arc state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_arc_get_lua_ref(arc), "Arc should have no Lua reference after unref");

    ese_arc_destroy(arc);
    lua_engine_destroy(engine);
}

static void test_ese_arc_lua_init(void) {
    lua_State *L = g_engine->runtime;
    
    luaL_getmetatable(L, "ArcMeta");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Metatable should not exist before initialization");
    lua_pop(L, 1);
    
    lua_getglobal(L, "Arc");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Global Arc table should not exist before initialization");
    lua_pop(L, 1);
    
    ese_arc_lua_init(g_engine);
    
    luaL_getmetatable(L, "ArcMeta");
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Metatable should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Metatable should be a table");
    lua_pop(L, 1);
    
    lua_getglobal(L, "Arc");
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Global Arc table should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Global Arc table should be a table");
    lua_pop(L, 1);
}

static void test_ese_arc_lua_push(void) {
    ese_arc_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseArc *arc = ese_arc_create(g_engine);
    
    ese_arc_lua_push(arc);
    
    EseArc **ud = (EseArc **)lua_touserdata(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(arc, *ud, "The pushed item should be the actual arc");
    
    lua_pop(L, 1); 
    
    ese_arc_destroy(arc);
}

static void test_ese_arc_lua_get(void) {
    ese_arc_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseArc *arc = ese_arc_create(g_engine);
    
    ese_arc_lua_push(arc);
    
    EseArc *extracted_arc = ese_arc_lua_get(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(arc, extracted_arc, "Extracted arc should match original");
    
    lua_pop(L, 1);
    ese_arc_destroy(arc);
}

/**
* Lua API Test Functions
*/

static void test_ese_arc_lua_new(void) {
    ese_arc_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "return Arc.new()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute without error");
    EseArc *extracted_arc = ese_arc_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_arc, "Extracted arc should not be NULL");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_arc_get_x(extracted_arc), "Extracted arc should have x=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_arc_get_y(extracted_arc), "Extracted arc should have y=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, ese_arc_get_radius(extracted_arc), "Extracted arc should have radius=1");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_arc_get_start_angle(extracted_arc), "Extracted arc should have start_angle=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2.0f * M_PI, ese_arc_get_end_angle(extracted_arc), "Extracted arc should have end_angle=2π");
    ese_arc_destroy(extracted_arc);

    const char *testB = "return Arc.new(10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute with error");

    const char *testC = "return Arc.new(10, 10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute with error");

    const char *testD = "return Arc.new(10, 10, 10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testD), "testD Lua code should execute with error");

    const char *testE = "return Arc.new(\"10\", \"10\", \"10\", \"10\", \"10\")\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testE), "testE Lua code should execute with error");

    const char *testF = "return Arc.new(10, 10, 5, 0, 3.14159)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testF), "testF Lua code should execute without error");
    extracted_arc = ese_arc_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_arc, "Extracted arc should not be NULL");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, ese_arc_get_x(extracted_arc), "Extracted arc should have x=10");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, ese_arc_get_y(extracted_arc), "Extracted arc should have y=10");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(5.0f, ese_arc_get_radius(extracted_arc), "Extracted arc should have radius=5");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_arc_get_start_angle(extracted_arc), "Extracted arc should have start_angle=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3.14159f, ese_arc_get_end_angle(extracted_arc), "Extracted arc should have end_angle=3.14159");
    ese_arc_destroy(extracted_arc);
}

static void test_ese_arc_lua_zero(void) {
    ese_arc_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "return Arc.zero(10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute with error");

    const char *testB = "return Arc.zero(10, 10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute with error");

    const char *testC = "return Arc.zero()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute without error");
    EseArc *extracted_arc = ese_arc_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_arc, "Extracted arc should not be NULL");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_arc_get_x(extracted_arc), "Extracted arc should have x=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_arc_get_y(extracted_arc), "Extracted arc should have y=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, ese_arc_get_radius(extracted_arc), "Extracted arc should have radius=1");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_arc_get_start_angle(extracted_arc), "Extracted arc should have start_angle=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2.0f * M_PI, ese_arc_get_end_angle(extracted_arc), "Extracted arc should have end_angle=2π");
    ese_arc_destroy(extracted_arc);
}

static void test_ese_arc_lua_contains_point(void) {
    ese_arc_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code = "local a = Arc.new(0, 0, 2, 0, 6.28); return a:contains_point(2, 0, 0.1)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "contains_point test should execute without error");
    bool contains = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(contains, "Point should be contained");
    lua_pop(L, 1);

    const char *test_code2 = "local a = Arc.new(0, 0, 2, 0, 6.28); return a:contains_point(3, 0, 0.1)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code2), "contains_point test 2 should execute without error");
    bool contains2 = lua_toboolean(L, -1);
    TEST_ASSERT_FALSE_MESSAGE(contains2, "Point should not be contained");
    lua_pop(L, 1);

    const char *test_code3 = "local a = Arc.new(0, 0, 2, 0, 6.28); return a:contains_point()";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code3), "test_code3 Lua code should execute with error");

    const char *test_code4 = "local a = Arc.new(0, 0, 2, 0, 6.28); return a:contains_point(2, 0, 0.1, 0.1)";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code4), "test_code4 Lua code should execute with error");
}

static void test_ese_arc_lua_get_length(void) {
    ese_arc_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code1 = "local a = Arc.new(0, 0, 2, 0, 6.28); return a:get_length()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code1), "get_length test should execute without error");
    double length = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f * M_PI * 2.0f, length);
    lua_pop(L, 1);

    const char *test_code2 = "local a = Arc.new(0, 0, 2, 0, 6.28); return a:get_length(10)";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code2), "get_length test 2 should execute with error");
}

static void test_ese_arc_lua_get_point_at_angle(void) {
    ese_arc_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code = "local a = Arc.new(0, 0, 2, 0, 6.28); local success, x, y = a:get_point_at_angle(math.pi/2); return success, x, y";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "get_point_at_angle test should execute without error");
    bool success = lua_toboolean(L, -3);
    double x = lua_tonumber(L, -2);
    double y = lua_tonumber(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(success, "get_point_at_angle should return success=true");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, y);
    lua_pop(L, 3);

    const char *test_code2 = "local a = Arc.new(0, 0, 2, 0, 6.28); return a:get_point_at_angle()";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code2), "test_code2 Lua code should execute with error");
}

static void test_ese_arc_lua_intersects_rect(void) {
    ese_arc_lua_init(g_engine);
    ese_rect_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code = "local a = Arc.new(0, 0, 2, 0, 6.28); local r = Rect.new(1, 1, 2, 2); return a:intersects_rect(r)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "intersects_rect test should execute without error");
    bool intersects = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(intersects, "Arc should intersect with rectangle");
    lua_pop(L, 1);

    const char *test_code2 = "local a = Arc.new(0, 0, 2, 0, 6.28); return a:intersects_rect()";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code2), "test_code2 Lua code should execute with error");
}

static void test_ese_arc_lua_x(void) {
    ese_arc_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local a = Arc.new(0, 0, 1, 0, 6.28); a.x = 10; return a.x";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua x set/get test 1 should execute without error");
    double x = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, x);
    lua_pop(L, 1);

    const char *test2 = "local a = Arc.new(0, 0, 1, 0, 6.28); a.x = -10; return a.x";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua x set/get test 2 should execute without error");
    x = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, x);
    lua_pop(L, 1);

    const char *test3 = "local a = Arc.new(0, 0, 1, 0, 6.28); a.x = 0; return a.x";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua x set/get test 3 should execute without error");
    x = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, x);
    lua_pop(L, 1);
}

static void test_ese_arc_lua_y(void) {
    ese_arc_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local a = Arc.new(0, 0, 1, 0, 6.28); a.y = 20; return a.y";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua y set/get test 1 should execute without error");
    double y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, y);
    lua_pop(L, 1);

    const char *test2 = "local a = Arc.new(0, 0, 1, 0, 6.28); a.y = -10; return a.y";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua y set/get test 2 should execute without error");
    y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, y);
    lua_pop(L, 1);

    const char *test3 = "local a = Arc.new(0, 0, 1, 0, 6.28); a.y = 0; return a.y";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua y set/get test 3 should execute without error");
    y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, y);
    lua_pop(L, 1);
}

static void test_ese_arc_lua_radius(void) {
    ese_arc_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local a = Arc.new(0, 0, 1, 0, 6.28); a.radius = 5; return a.radius";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua radius set/get test 1 should execute without error");
    double radius = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, radius);
    lua_pop(L, 1);

    const char *test2 = "local a = Arc.new(0, 0, 1, 0, 6.28); a.radius = 0.5; return a.radius";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua radius set/get test 2 should execute without error");
    radius = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, radius);
    lua_pop(L, 1);

    const char *test3 = "local a = Arc.new(0, 0, 1, 0, 6.28); a.radius = 1; return a.radius";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua radius set/get test 3 should execute without error");
    radius = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, radius);
    lua_pop(L, 1);
}

static void test_ese_arc_lua_start_angle(void) {
    ese_arc_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local a = Arc.new(0, 0, 1, 0, 6.28); a.start_angle = 1.57; return a.start_angle";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua start_angle set/get test 1 should execute without error");
    double start_angle = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.57f, start_angle);
    lua_pop(L, 1);

    const char *test2 = "local a = Arc.new(0, 0, 1, 0, 6.28); a.start_angle = -1.57; return a.start_angle";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua start_angle set/get test 2 should execute without error");
    start_angle = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.57f, start_angle);
    lua_pop(L, 1);

    const char *test3 = "local a = Arc.new(0, 0, 1, 0, 6.28); a.start_angle = 0; return a.start_angle";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua start_angle set/get test 3 should execute without error");
    start_angle = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, start_angle);
    lua_pop(L, 1);
}

static void test_ese_arc_lua_end_angle(void) {
    ese_arc_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local a = Arc.new(0, 0, 1, 0, 6.28); a.end_angle = 3.14; return a.end_angle";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua end_angle set/get test 1 should execute without error");
    double end_angle = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.14f, end_angle);
    lua_pop(L, 1);

    const char *test2 = "local a = Arc.new(0, 0, 1, 0, 6.28); a.end_angle = -1.57; return a.end_angle";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua end_angle set/get test 2 should execute without error");
    end_angle = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.57f, end_angle);
    lua_pop(L, 1);

    const char *test3 = "local a = Arc.new(0, 0, 1, 0, 6.28); a.end_angle = 6.28; return a.end_angle";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua end_angle set/get test 3 should execute without error");
    end_angle = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.28f, end_angle);
    lua_pop(L, 1);
}

static void test_ese_arc_lua_tostring(void) {
    ese_arc_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test_code = "local a = Arc.new(10.5, 20.25, 5.0, 1.57, 4.71); return tostring(a)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "tostring test should execute without error");
    const char *result = lua_tostring(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "tostring result should not be NULL");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "Arc:") != NULL, "tostring should contain 'Arc:'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "x=10.50") != NULL, "tostring should contain 'x=10.50'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "y=20.25") != NULL, "tostring should contain 'y=20.25'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "r=5.00") != NULL, "tostring should contain 'r=5.00'");
    lua_pop(L, 1); 
}

static void test_ese_arc_lua_gc(void) {
    ese_arc_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *testA = "local a = Arc.new(5, 10, 3, 0, 6.28)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "Arc creation should execute without error");
    
    int collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testB = "return Arc.new(5, 10, 3, 0, 6.28)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "Arc creation should execute without error");
    EseArc *extracted_arc = ese_arc_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_arc, "Extracted arc should not be NULL");
    ese_arc_ref(extracted_arc);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    ese_arc_unref(extracted_arc);

    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testC = "return Arc.new(5, 10, 3, 0, 6.28)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "Arc creation should execute without error");
    extracted_arc = ese_arc_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_arc, "Extracted arc should not be NULL");
    ese_arc_ref(extracted_arc);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    ese_arc_unref(extracted_arc);
    ese_arc_destroy(extracted_arc);

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
* Tests for arc serialization/deserialization functionality
*/
static void test_ese_arc_serialization(void) {
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    // Create a test arc
    EseArc *original = ese_arc_create(engine);
    TEST_ASSERT_NOT_NULL(original);

    ese_arc_set_x(original, 10.5f);
    ese_arc_set_y(original, 20.7f);
    ese_arc_set_radius(original, 5.0f);
    ese_arc_set_start_angle(original, 0.0f);
    ese_arc_set_end_angle(original, M_PI); // π radians (180 degrees)

    // Test serialization
    cJSON *json = ese_arc_serialize(original);
    TEST_ASSERT_NOT_NULL(json);

    // Verify JSON structure
    cJSON *type_item = cJSON_GetObjectItem(json, "type");
    TEST_ASSERT_NOT_NULL(type_item);
    TEST_ASSERT_TRUE(cJSON_IsString(type_item));
    TEST_ASSERT_EQUAL_STRING("ARC", type_item->valuestring);

    cJSON *x_item = cJSON_GetObjectItem(json, "x");
    TEST_ASSERT_NOT_NULL(x_item);
    TEST_ASSERT_TRUE(cJSON_IsNumber(x_item));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 10.5, x_item->valuedouble);

    cJSON *y_item = cJSON_GetObjectItem(json, "y");
    TEST_ASSERT_NOT_NULL(y_item);
    TEST_ASSERT_TRUE(cJSON_IsNumber(y_item));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 20.7, y_item->valuedouble);

    cJSON *radius_item = cJSON_GetObjectItem(json, "radius");
    TEST_ASSERT_NOT_NULL(radius_item);
    TEST_ASSERT_TRUE(cJSON_IsNumber(radius_item));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 5.0, radius_item->valuedouble);

    cJSON *start_angle_item = cJSON_GetObjectItem(json, "start_angle");
    TEST_ASSERT_NOT_NULL(start_angle_item);
    TEST_ASSERT_TRUE(cJSON_IsNumber(start_angle_item));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, start_angle_item->valuedouble);

    cJSON *end_angle_item = cJSON_GetObjectItem(json, "end_angle");
    TEST_ASSERT_NOT_NULL(end_angle_item);
    TEST_ASSERT_TRUE(cJSON_IsNumber(end_angle_item));
    TEST_ASSERT_FLOAT_WITHIN(0.001, M_PI, end_angle_item->valuedouble);

    // Test deserialization
    EseArc *deserialized = ese_arc_deserialize(engine, json);
    TEST_ASSERT_NOT_NULL(deserialized);

    // Verify all properties match
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.5f, ese_arc_get_x(deserialized));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.7f, ese_arc_get_y(deserialized));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, ese_arc_get_radius(deserialized));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_arc_get_start_angle(deserialized));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, M_PI, ese_arc_get_end_angle(deserialized));

    // Clean up
    cJSON_Delete(json);
    ese_arc_destroy(original);
    ese_arc_destroy(deserialized);
    lua_engine_destroy(engine);
}

/**
* Test Arc:toJSON Lua instance method
*/
static void test_ese_arc_lua_to_json(void) {
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    ese_arc_lua_init(engine);

    // Set engine in Lua registry so fromJSON can retrieve it
    lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);

    const char *testA = "local a = Arc.new(10.5, 20.7, 5.0, 0.0, 3.14159) "
                       "local json = a:toJSON() "
                       "if json == nil or json == '' then error('toJSON should return non-empty string') end "
                       "if not string.find(json, '\"type\":\"ARC\"') then error('toJSON should return valid JSON') end "
                       "if not string.find(json, '\"x\":10.5') then error('toJSON should contain correct x') end "
                       "if not string.find(json, '\"y\":20.7') then error('toJSON should contain correct y') end "
                       "if not string.find(json, '\"radius\":5') then error('toJSON should contain correct radius') end "
                       "if not string.find(json, '\"start_angle\":0') then error('toJSON should contain correct start_angle') end "
                       "if not string.find(json, '\"end_angle\":3.14') then error('toJSON should contain correct end_angle') end ";

    int result = luaL_dostring(engine->runtime, testA);
    if (result != LUA_OK) {
        const char *error_msg = lua_tostring(engine->runtime, -1);
        printf("ERROR in arc toJSON test: %s\n", error_msg ? error_msg : "unknown error");
        lua_pop(engine->runtime, 1);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, result, "Arc:toJSON should create valid JSON");

    lua_engine_destroy(engine);
}

/**
* Test Arc.fromJSON Lua static method
*/
static void test_ese_arc_lua_from_json(void) {
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    ese_arc_lua_init(engine);
    lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);

    const char *testA = "local json_str = '{\\\"type\\\":\\\"ARC\\\",\\\"x\\\":10.5,\\\"y\\\":20.7,\\\"radius\\\":5.0,\\\"start_angle\\\":0.0,\\\"end_angle\\\":3.14159}' "
                       "local a = Arc.fromJSON(json_str) "
                       "if a == nil then error('Arc.fromJSON should return an arc') end "
                       "if math.abs(a.x - 10.5) > 0.001 then error('Arc fromJSON should set correct x') end "
                       "if math.abs(a.y - 20.7) > 0.001 then error('Arc fromJSON should set correct y') end "
                       "if math.abs(a.radius - 5.0) > 0.001 then error('Arc fromJSON should set correct radius') end "
                       "if math.abs(a.start_angle - 0.0) > 0.001 then error('Arc fromJSON should set correct start_angle') end "
                       "if math.abs(a.end_angle - 3.14159) > 0.001 then error('Arc fromJSON should set correct end_angle') end ";

    int resultA = luaL_dostring(engine->runtime, testA);
    if (resultA != LUA_OK) {
        const char *error_msg = lua_tostring(engine->runtime, -1);
        printf("ERROR in arc fromJSON test: %s\n", error_msg ? error_msg : "unknown error");
        lua_pop(engine->runtime, 1);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, resultA, "Arc.fromJSON should work with valid JSON");

    // Invalid JSON should error
    const char *testB = "local a = Arc.fromJSON('invalid json')";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(engine->runtime, testB), "Arc.fromJSON should fail with invalid JSON");

    lua_engine_destroy(engine);
}

/**
* Test Arc JSON round-trip (toJSON -> fromJSON)
*/
static void test_ese_arc_json_round_trip(void) {
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    ese_arc_lua_init(engine);
    lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);

    const char *testA = "local original = Arc.new(10.5, 20.7, 5.0, 0.0, 3.14159) "
                       "local json = original:toJSON() "
                       "local restored = Arc.fromJSON(json) "
                       "if not restored then error('Arc.fromJSON should return an arc') end "
                       "if math.abs(restored.x - original.x) > 0.001 then error('Round-trip should preserve x') end "
                       "if math.abs(restored.y - original.y) > 0.001 then error('Round-trip should preserve y') end "
                       "if math.abs(restored.radius - original.radius) > 0.001 then error('Round-trip should preserve radius') end "
                       "if math.abs(restored.start_angle - original.start_angle) > 0.001 then error('Round-trip should preserve start_angle') end "
                       "if math.abs(restored.end_angle - original.end_angle) > 0.001 then error('Round-trip should preserve end_angle') end ";

    int result = luaL_dostring(engine->runtime, testA);
    if (result != LUA_OK) {
        const char *error_msg = lua_tostring(engine->runtime, -1);
        printf("ERROR in arc round-trip test: %s\n", error_msg ? error_msg : "unknown error");
        lua_pop(engine->runtime, 1);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, result, "Arc JSON round-trip should work correctly");

    lua_engine_destroy(engine);
}
