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
#include "../src/scripting/lua_value.h"
#include "../src/scripting/lua_engine.h"

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

// Additional comprehensive test functions
static void test_ese_arc_edge_cases(void);
static void test_ese_arc_error_conditions(void);
static void test_ese_arc_lua_metamethods(void);
static void test_ese_arc_lua_constructor_errors(void);

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
    ese_arc_lua_init(g_engine);
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

    // Additional comprehensive tests
    RUN_TEST(test_ese_arc_edge_cases);
    RUN_TEST(test_ese_arc_error_conditions);
    RUN_TEST(test_ese_arc_lua_metamethods);
    RUN_TEST(test_ese_arc_lua_constructor_errors);

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_ese_arc_sizeof(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, sizeof(EseArc), "Arc size should be > 0");
}

static void test_ese_arc_create_requires_engine(void) {
    ASSERT_DEATH(ese_arc_create(NULL), "ese_arc_create should abort with NULL engine");
}

static void test_ese_arc_create(void) {
    EseArc *arc = ese_arc_create(g_engine);

    TEST_ASSERT_NOT_NULL_MESSAGE(arc, "Arc should be created");
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, arc->x);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, arc->y);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, arc->radius);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, arc->start_angle);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.0f * M_PI, arc->end_angle);
    // Note: Arc no longer stores Lua state directly
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, arc->lua_ref_count, "New arc should have ref count 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, arc->lua_ref, "New arc should have LUA_NOREF value");

    ese_arc_destroy(arc);
}

static void test_ese_arc_x(void) {
    EseArc *arc = ese_arc_create(g_engine);

    arc->x = 10.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, arc->x);

    arc->x = -10.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, arc->x);

    arc->x = 0.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, arc->x);

    ese_arc_destroy(arc);
}

static void test_ese_arc_y(void) {
    EseArc *arc = ese_arc_create(g_engine);

    arc->y = 20.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, arc->y);

    arc->y = -10.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, arc->y);

    arc->y = 0.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, arc->y);

    ese_arc_destroy(arc);
}

static void test_ese_arc_radius(void) {
    EseArc *arc = ese_arc_create(g_engine);

    arc->radius = 5.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, arc->radius);

    arc->radius = 0.5f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, arc->radius);

    arc->radius = 1.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, arc->radius);

    ese_arc_destroy(arc);
}

static void test_ese_arc_start_angle(void) {
    EseArc *arc = ese_arc_create(g_engine);

    arc->start_angle = M_PI / 4.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, M_PI / 4.0f, arc->start_angle);

    arc->start_angle = -M_PI / 2.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -M_PI / 2.0f, arc->start_angle);

    arc->start_angle = 0.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, arc->start_angle);

    ese_arc_destroy(arc);
}

static void test_ese_arc_end_angle(void) {
    EseArc *arc = ese_arc_create(g_engine);

    arc->end_angle = 3.0f * M_PI / 4.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f * M_PI / 4.0f, arc->end_angle);

    arc->end_angle = -M_PI / 2.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -M_PI / 2.0f, arc->end_angle);

    arc->end_angle = 2.0f * M_PI;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f * M_PI, arc->end_angle);

    ese_arc_destroy(arc);
}

static void test_ese_arc_ref(void) {
    EseArc *arc = ese_arc_create(g_engine);

    ese_arc_ref(g_engine, arc);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, arc->lua_ref_count, "Ref count should be 1");

    ese_arc_unref(g_engine, arc);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, arc->lua_ref_count, "Ref count should be 0");

    ese_arc_destroy(arc);
}

static void test_ese_arc_copy_requires_engine(void) {
    // ese_arc_copy should handle NULL gracefully (not abort)
    EseArc *result = ese_arc_copy(NULL);
    TEST_ASSERT_NULL_MESSAGE(result, "ese_arc_copy should return NULL for NULL input");
}

static void test_ese_arc_copy(void) {
    EseArc *arc = ese_arc_create(g_engine);
    ese_arc_ref(g_engine, arc);
    arc->x = 10.0f;
    arc->y = 20.0f;
    arc->radius = 5.0f;
    arc->start_angle = M_PI / 4.0f;
    arc->end_angle = 3.0f * M_PI / 4.0f;
    EseArc *copy = ese_arc_copy(arc);

    TEST_ASSERT_NOT_NULL_MESSAGE(copy, "Copy should be created");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, copy->lua_ref_count, "Copy should have ref count 0");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, copy->x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, copy->y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, copy->radius);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, M_PI / 4.0f, copy->start_angle);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f * M_PI / 4.0f, copy->end_angle);

    ese_arc_unref(g_engine, arc);
    ese_arc_destroy(arc);
    ese_arc_destroy(copy);
}

static void test_ese_arc_contains_point(void) {
    EseArc *arc = ese_arc_create(g_engine);

    arc->x = 0.0f;
    arc->y = 0.0f;
    arc->radius = 2.0f;
    arc->start_angle = 0.0f;
    arc->end_angle = 2.0f * M_PI;

    TEST_ASSERT_TRUE_MESSAGE(ese_arc_contains_point(arc, 2.0f, 0.0f, 0.1f), "Point on arc should be contained");
    TEST_ASSERT_TRUE_MESSAGE(ese_arc_contains_point(arc, 0.0f, 2.0f, 0.1f), "Point on arc should be contained");
    TEST_ASSERT_FALSE_MESSAGE(ese_arc_contains_point(arc, 3.0f, 0.0f, 0.1f), "Point outside arc should not be contained");
    TEST_ASSERT_FALSE_MESSAGE(ese_arc_contains_point(arc, 1.0f, 1.0f, 0.1f), "Point inside circle but not on arc should not be contained");

    // Test partial arc
    arc->start_angle = 0.0f;
    arc->end_angle = M_PI / 2.0f; // 90 degrees
    TEST_ASSERT_TRUE_MESSAGE(ese_arc_contains_point(arc, 2.0f, 0.0f, 0.1f), "Point on start of arc should be contained");
    TEST_ASSERT_TRUE_MESSAGE(ese_arc_contains_point(arc, 0.0f, 2.0f, 0.1f), "Point on end of arc should be contained");
    TEST_ASSERT_FALSE_MESSAGE(ese_arc_contains_point(arc, -2.0f, 0.0f, 0.1f), "Point on opposite side should not be contained");

    ese_arc_destroy(arc);
}

static void test_ese_arc_get_length(void) {
    EseArc *arc = ese_arc_create(g_engine);

    arc->radius = 2.0f;
    arc->start_angle = 0.0f;
    arc->end_angle = 2.0f * M_PI;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f * M_PI * 2.0f, ese_arc_get_length(arc));

    arc->start_angle = 0.0f;
    arc->end_angle = M_PI; // 180 degrees
    TEST_ASSERT_FLOAT_WITHIN(0.001f, M_PI * 2.0f, ese_arc_get_length(arc));

    arc->start_angle = 0.0f;
    arc->end_angle = M_PI / 2.0f; // 90 degrees
    TEST_ASSERT_FLOAT_WITHIN(0.001f, M_PI * 2.0f / 2.0f, ese_arc_get_length(arc));

    ese_arc_destroy(arc);
}

static void test_ese_arc_get_point_at_angle(void) {
    EseArc *arc = ese_arc_create(g_engine);

    arc->x = 0.0f;
    arc->y = 0.0f;
    arc->radius = 2.0f;
    arc->start_angle = 0.0f;
    arc->end_angle = 2.0f * M_PI;
    
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
    arc->start_angle = 0.0f;
    arc->end_angle = M_PI / 2.0f;
    result = ese_arc_get_point_at_angle(arc, M_PI, &point_x, &point_y);
    TEST_ASSERT_FALSE_MESSAGE(result, "Should return false for angle outside arc range");

    ese_arc_destroy(arc);
}

static void test_ese_arc_intersects_rect(void) {
    EseArc *arc = ese_arc_create(g_engine);
    EseRect *rect = ese_rect_create(g_engine);

    arc->x = 0.0f;
    arc->y = 0.0f;
    arc->radius = 2.0f;
    arc->start_angle = 0.0f;
    arc->end_angle = 2.0f * M_PI;
    
    ese_rect_set_x(rect, 1.0f);
    ese_rect_set_y(rect, 1.0f);
    ese_rect_set_width(rect, 2.0f);
    ese_rect_set_height(rect, 2.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_arc_intersects_rect(arc, rect), "Arc should intersect with rectangle");
    
    arc->x = 10.0f; // Move arc away from rectangle
    TEST_ASSERT_FALSE_MESSAGE(ese_arc_intersects_rect(arc, rect), "Arc should not intersect with rectangle when far away");
    
    ese_arc_destroy(arc);
    ese_rect_destroy(rect);
}

static void test_ese_arc_watcher_system(void) {
    EseArc *arc = ese_arc_create(g_engine);

    mock_reset();
    arc->x = 25.0f;
    TEST_ASSERT_FALSE_MESSAGE(watcher_called, "Watcher should not be called before adding");

    void *test_userdata = (void*)0x12345678;
    // Note: Arc doesn't have watcher system like Point/Rect, so this test is placeholder
    // bool add_result = ese_arc_add_watcher(arc, test_watcher_callback, test_userdata);
    // TEST_ASSERT_TRUE_MESSAGE(add_result, "Should successfully add watcher");

    mock_reset();
    arc->x = 50.0f;
    // TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when x changes");

    ese_arc_destroy(arc);
}

static void test_ese_arc_lua_integration(void) {
    EseLuaEngine *engine = create_test_engine();
    EseArc *arc = ese_arc_create(engine);
    
    TEST_ASSERT_EQUAL_INT_MESSAGE(ESE_LUA_NOREF, arc->lua_ref, "Arc should have no Lua reference initially");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, arc->lua_ref_count, "Arc should have ref count 0 initially");

    ese_arc_ref(engine, arc);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(ESE_LUA_NOREF, arc->lua_ref, "Arc should have a valid Lua reference after ref");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, arc->lua_ref_count, "Arc should have ref count 1 after ref");

    ese_arc_unref(engine, arc);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ESE_LUA_NOREF, arc->lua_ref, "Arc should have no Lua reference after unref");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, arc->lua_ref_count, "Arc should have ref count 0 after unref");

    ese_arc_destroy(arc);
    lua_engine_destroy(engine);
}

static void test_ese_arc_lua_init(void) {
    lua_State *L = g_engine->L;
    
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

    lua_State *L = g_engine->L;
    EseArc *arc = ese_arc_create(g_engine);
    
    ese_arc_lua_push(g_engine, arc);
    
    EseArc **ud = (EseArc **)lua_touserdata(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(arc, *ud, "The pushed item should be the actual arc");
    
    lua_pop(L, 1); 
    
    ese_arc_destroy(arc);
}

static void test_ese_arc_lua_get(void) {
    ese_arc_lua_init(g_engine);

    lua_State *L = g_engine->L;
    EseArc *arc = ese_arc_create(g_engine);
    
    ese_arc_lua_push(g_engine, arc);
    
    EseArc *extracted_arc = ese_arc_lua_get(g_engine, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(arc, extracted_arc, "Extracted arc should match original");
    
    lua_pop(L, 1);
    ese_arc_destroy(arc);
}

/**
* Lua API Test Functions
*/

static void test_ese_arc_lua_new(void) {
    // Test Arc.new() with no arguments
    EseLuaValue *result = lua_engine_call_function(g_engine, "Arc", "new", 0, NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "Arc.new() should return a result");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_arc(result), "Result should be an arc");
    
    EseArc *arc = lua_value_get_arc(result);
    TEST_ASSERT_NOT_NULL_MESSAGE(arc, "Extracted arc should not be NULL");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, arc->x, "Extracted arc should have x=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, arc->y, "Extracted arc should have y=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, arc->radius, "Extracted arc should have radius=1");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, arc->start_angle, "Extracted arc should have start_angle=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2.0f * M_PI, arc->end_angle, "Extracted arc should have end_angle=2π");
    lua_value_destroy(result);

    // Test Arc.new() with 5 arguments
    EseLuaValue *args[5];
    args[0] = lua_value_create_number("x", 10.0);
    args[1] = lua_value_create_number("y", 10.0);
    args[2] = lua_value_create_number("radius", 5.0);
    args[3] = lua_value_create_number("start_angle", 0.0);
    args[4] = lua_value_create_number("end_angle", 3.14159);
    
    result = lua_engine_call_function(g_engine, "Arc", "new", 5, args);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "Arc.new(10,10,5,0,3.14159) should return a result");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_arc(result), "Result should be an arc");
    
    arc = lua_value_get_arc(result);
    TEST_ASSERT_NOT_NULL_MESSAGE(arc, "Extracted arc should not be NULL");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, arc->x, "Extracted arc should have x=10");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, arc->y, "Extracted arc should have y=10");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(5.0f, arc->radius, "Extracted arc should have radius=5");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, arc->start_angle, "Extracted arc should have start_angle=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3.14159f, arc->end_angle, "Extracted arc should have end_angle=3.14159");
    
    // Clean up
    for (int i = 0; i < 5; i++) {
        lua_value_destroy(args[i]);
    }
    lua_value_destroy(result);

    // Test Arc.new() with wrong number of arguments
    EseLuaValue *wrong_args[1];
    wrong_args[0] = lua_value_create_number("x", 10.0);
    
    result = lua_engine_call_function(g_engine, "Arc", "new", 1, wrong_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_error(result), "Arc.new(10) should return an error");
    lua_value_destroy(result);
    lua_value_destroy(wrong_args[0]);
}

static void test_ese_arc_lua_zero(void) {
    // Test Arc.zero() with no arguments
    EseLuaValue *result = lua_engine_call_function(g_engine, "Arc", "zero", 0, NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "Arc.zero() should return a result");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_arc(result), "Result should be an arc");
    
    EseArc *arc = lua_value_get_arc(result);
    TEST_ASSERT_NOT_NULL_MESSAGE(arc, "Extracted arc should not be NULL");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, arc->x, "Extracted arc should have x=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, arc->y, "Extracted arc should have y=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, arc->radius, "Extracted arc should have radius=1");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, arc->start_angle, "Extracted arc should have start_angle=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2.0f * M_PI, arc->end_angle, "Extracted arc should have end_angle=2π");
    lua_value_destroy(result);

    // Test Arc.zero() with wrong number of arguments
    EseLuaValue *wrong_args[1];
    wrong_args[0] = lua_value_create_number("x", 10.0);
    
    result = lua_engine_call_function(g_engine, "Arc", "zero", 1, wrong_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_error(result), "Arc.zero(10) should return an error");
    lua_value_destroy(result);
    lua_value_destroy(wrong_args[0]);
}

static void test_ese_arc_lua_contains_point(void) {
    // Create an arc
    EseLuaValue *arc_args[5];
    arc_args[0] = lua_value_create_number("x", 0.0);
    arc_args[1] = lua_value_create_number("y", 0.0);
    arc_args[2] = lua_value_create_number("radius", 2.0);
    arc_args[3] = lua_value_create_number("start_angle", 0.0);
    arc_args[4] = lua_value_create_number("end_angle", 6.28);
    
    EseLuaValue *arc_result = lua_engine_call_function(g_engine, "Arc", "new", 5, arc_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_arc(arc_result), "Arc creation should succeed");
    
    // Test contains_point with valid arguments
    EseLuaValue *method_args[3];
    method_args[0] = arc_result; // arc as self
    method_args[1] = lua_value_create_number("x", 2.0);
    method_args[2] = lua_value_create_number("y", 0.0);
    method_args[3] = lua_value_create_number("tolerance", 0.1);
    
    EseLuaValue *result = lua_engine_call_method(g_engine, "contains_point", 4, method_args);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "contains_point should return a result");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_boolean(result), "Result should be boolean");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_get_boolean(result), "Point should be contained");
    lua_value_destroy(result);
    
    // Test contains_point with point outside arc
    method_args[1] = lua_value_create_number("x", 3.0);
    result = lua_engine_call_method(g_engine, "contains_point", 4, method_args);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "contains_point should return a result");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_boolean(result), "Result should be boolean");
    TEST_ASSERT_FALSE_MESSAGE(lua_value_get_boolean(result), "Point should not be contained");
    lua_value_destroy(result);
    
    // Test contains_point with wrong number of arguments
    EseLuaValue *wrong_args[1];
    wrong_args[0] = arc_result;
    
    result = lua_engine_call_method(g_engine, "contains_point", 1, wrong_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_error(result), "contains_point() should return an error");
    lua_value_destroy(result);
    
    // Clean up
    for (int i = 0; i < 5; i++) {
        lua_value_destroy(arc_args[i]);
    }
    lua_value_destroy(arc_result);
    lua_value_destroy(method_args[1]);
    lua_value_destroy(method_args[2]);
    lua_value_destroy(method_args[3]);
}

static void test_ese_arc_lua_get_length(void) {
    // Create an arc
    EseLuaValue *arc_args[5];
    arc_args[0] = lua_value_create_number("x", 0.0);
    arc_args[1] = lua_value_create_number("y", 0.0);
    arc_args[2] = lua_value_create_number("radius", 2.0);
    arc_args[3] = lua_value_create_number("start_angle", 0.0);
    arc_args[4] = lua_value_create_number("end_angle", 6.28);
    
    EseLuaValue *arc_result = lua_engine_call_function(g_engine, "Arc", "new", 5, arc_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_arc(arc_result), "Arc creation should succeed");
    
    // Test get_length with valid arguments
    EseLuaValue *method_args[1];
    method_args[0] = arc_result; // arc as self
    
    EseLuaValue *result = lua_engine_call_method(g_engine, "get_length", 1, method_args);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "get_length should return a result");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_number(result), "Result should be a number");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f * M_PI * 2.0f, lua_value_get_number(result));
    lua_value_destroy(result);
    
    // Test get_length with wrong number of arguments
    EseLuaValue *wrong_args[2];
    wrong_args[0] = arc_result;
    wrong_args[1] = lua_value_create_number("extra", 10.0);
    
    result = lua_engine_call_method(g_engine, "get_length", 2, wrong_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_error(result), "get_length(10) should return an error");
    lua_value_destroy(result);
    
    // Clean up
    for (int i = 0; i < 5; i++) {
        lua_value_destroy(arc_args[i]);
    }
    lua_value_destroy(arc_result);
    lua_value_destroy(wrong_args[1]);
}

static void test_ese_arc_lua_get_point_at_angle(void) {
    // Create an arc
    EseLuaValue *arc_args[5];
    arc_args[0] = lua_value_create_number("x", 0.0);
    arc_args[1] = lua_value_create_number("y", 0.0);
    arc_args[2] = lua_value_create_number("radius", 2.0);
    arc_args[3] = lua_value_create_number("start_angle", 0.0);
    arc_args[4] = lua_value_create_number("end_angle", 6.28);
    
    EseLuaValue *arc_result = lua_engine_call_function(g_engine, "Arc", "new", 5, arc_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_arc(arc_result), "Arc creation should succeed");
    
    // Test get_point_at_angle with valid arguments
    EseLuaValue *method_args[2];
    method_args[0] = arc_result; // arc as self
    method_args[1] = lua_value_create_number("angle", M_PI / 2.0);
    
    EseLuaValue *result = lua_engine_call_method(g_engine, "get_point_at_angle", 2, method_args);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "get_point_at_angle should return a result");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_table(result), "Result should be a table");
    
    // Extract x and y from result table
    EseLuaValue *x_val = lua_value_get_table_prop(result, "x");
    EseLuaValue *y_val = lua_value_get_table_prop(result, "y");
    TEST_ASSERT_NOT_NULL_MESSAGE(x_val, "Result should have x property");
    TEST_ASSERT_NOT_NULL_MESSAGE(y_val, "Result should have y property");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lua_value_get_number(x_val));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, lua_value_get_number(y_val));
    
    lua_value_destroy(result);
    lua_value_destroy(x_val);
    lua_value_destroy(y_val);
    
    // Test get_point_at_angle with wrong number of arguments
    EseLuaValue *wrong_args[1];
    wrong_args[0] = arc_result;
    
    result = lua_engine_call_method(g_engine, "get_point_at_angle", 1, wrong_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_error(result), "get_point_at_angle() should return an error");
    lua_value_destroy(result);
    
    // Clean up
    for (int i = 0; i < 5; i++) {
        lua_value_destroy(arc_args[i]);
    }
    lua_value_destroy(arc_result);
    lua_value_destroy(method_args[1]);
}

static void test_ese_arc_lua_intersects_rect(void) {
    // Create an arc
    EseLuaValue *arc_args[5];
    arc_args[0] = lua_value_create_number("x", 0.0);
    arc_args[1] = lua_value_create_number("y", 0.0);
    arc_args[2] = lua_value_create_number("radius", 2.0);
    arc_args[3] = lua_value_create_number("start_angle", 0.0);
    arc_args[4] = lua_value_create_number("end_angle", 6.28);
    
    EseLuaValue *arc_result = lua_engine_call_function(g_engine, "Arc", "new", 5, arc_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_arc(arc_result), "Arc creation should succeed");
    
    // Create a rectangle
    EseLuaValue *rect_args[4];
    rect_args[0] = lua_value_create_number("x", 1.0);
    rect_args[1] = lua_value_create_number("y", 1.0);
    rect_args[2] = lua_value_create_number("width", 2.0);
    rect_args[3] = lua_value_create_number("height", 2.0);
    
    EseLuaValue *rect_result = lua_engine_call_function(g_engine, "Rect", "new", 4, rect_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_rect(rect_result), "Rect creation should succeed");
    
    // Test intersects_rect with valid arguments
    EseLuaValue *method_args[2];
    method_args[0] = arc_result; // arc as self
    method_args[1] = rect_result; // rect as argument
    
    EseLuaValue *result = lua_engine_call_method(g_engine, "intersects_rect", 2, method_args);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "intersects_rect should return a result");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_boolean(result), "Result should be boolean");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_get_boolean(result), "Arc should intersect with rectangle");
    lua_value_destroy(result);
    
    // Test intersects_rect with wrong number of arguments
    EseLuaValue *wrong_args[1];
    wrong_args[0] = arc_result;
    
    result = lua_engine_call_method(g_engine, "intersects_rect", 1, wrong_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_error(result), "intersects_rect() should return an error");
    lua_value_destroy(result);
    
    // Clean up
    for (int i = 0; i < 5; i++) {
        lua_value_destroy(arc_args[i]);
    }
    for (int i = 0; i < 4; i++) {
        lua_value_destroy(rect_args[i]);
    }
    lua_value_destroy(arc_result);
    lua_value_destroy(rect_result);
}

static void test_ese_arc_lua_x(void) {
    // Create an arc
    EseLuaValue *arc_args[5];
    arc_args[0] = lua_value_create_number("x", 0.0);
    arc_args[1] = lua_value_create_number("y", 0.0);
    arc_args[2] = lua_value_create_number("radius", 1.0);
    arc_args[3] = lua_value_create_number("start_angle", 0.0);
    arc_args[4] = lua_value_create_number("end_angle", 6.28);
    
    EseLuaValue *arc_result = lua_engine_call_function(g_engine, "Arc", "new", 5, arc_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_arc(arc_result), "Arc creation should succeed");
    
    // Test setting and getting x property
    EseLuaValue *set_args[2];
    set_args[0] = arc_result;
    set_args[1] = lua_value_create_number("value", 10.0);
    
    EseLuaValue *result = lua_engine_call_method(g_engine, "x", 2, set_args);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "Setting x should return a result");
    lua_value_destroy(result);
    
    // Get x property
    EseLuaValue *get_args[1];
    get_args[0] = arc_result;
    
    result = lua_engine_call_method(g_engine, "x", 1, get_args);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "Getting x should return a result");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_number(result), "Result should be a number");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, lua_value_get_number(result));
    lua_value_destroy(result);
    
    // Test negative value
    set_args[1] = lua_value_create_number("value", -10.0);
    result = lua_engine_call_method(g_engine, "x", 2, set_args);
    lua_value_destroy(result);
    
    result = lua_engine_call_method(g_engine, "x", 1, get_args);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, lua_value_get_number(result));
    lua_value_destroy(result);
    
    // Clean up
    for (int i = 0; i < 5; i++) {
        lua_value_destroy(arc_args[i]);
    }
    lua_value_destroy(arc_result);
    lua_value_destroy(set_args[1]);
}

static void test_ese_arc_lua_y(void) {
    // Create an arc
    EseLuaValue *arc_args[5];
    arc_args[0] = lua_value_create_number("x", 0.0);
    arc_args[1] = lua_value_create_number("y", 0.0);
    arc_args[2] = lua_value_create_number("radius", 1.0);
    arc_args[3] = lua_value_create_number("start_angle", 0.0);
    arc_args[4] = lua_value_create_number("end_angle", 6.28);
    
    EseLuaValue *arc_result = lua_engine_call_function(g_engine, "Arc", "new", 5, arc_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_arc(arc_result), "Arc creation should succeed");
    
    // Test setting and getting y property
    EseLuaValue *set_args[2];
    set_args[0] = arc_result;
    set_args[1] = lua_value_create_number("value", 20.0);
    
    EseLuaValue *result = lua_engine_call_method(g_engine, "y", 2, set_args);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "Setting y should return a result");
    lua_value_destroy(result);
    
    // Get y property
    EseLuaValue *get_args[1];
    get_args[0] = arc_result;
    
    result = lua_engine_call_method(g_engine, "y", 1, get_args);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "Getting y should return a result");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_number(result), "Result should be a number");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, lua_value_get_number(result));
    lua_value_destroy(result);
    
    // Test negative value
    set_args[1] = lua_value_create_number("value", -10.0);
    result = lua_engine_call_method(g_engine, "y", 2, set_args);
    lua_value_destroy(result);
    
    result = lua_engine_call_method(g_engine, "y", 1, get_args);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, lua_value_get_number(result));
    lua_value_destroy(result);
    
    // Clean up
    for (int i = 0; i < 5; i++) {
        lua_value_destroy(arc_args[i]);
    }
    lua_value_destroy(arc_result);
    lua_value_destroy(set_args[1]);
}

static void test_ese_arc_lua_radius(void) {
    // Create an arc
    EseLuaValue *arc_args[5];
    arc_args[0] = lua_value_create_number("x", 0.0);
    arc_args[1] = lua_value_create_number("y", 0.0);
    arc_args[2] = lua_value_create_number("radius", 1.0);
    arc_args[3] = lua_value_create_number("start_angle", 0.0);
    arc_args[4] = lua_value_create_number("end_angle", 6.28);
    
    EseLuaValue *arc_result = lua_engine_call_function(g_engine, "Arc", "new", 5, arc_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_arc(arc_result), "Arc creation should succeed");
    
    // Test setting and getting radius property
    EseLuaValue *set_args[2];
    set_args[0] = arc_result;
    set_args[1] = lua_value_create_number("value", 5.0);
    
    EseLuaValue *result = lua_engine_call_method(g_engine, "radius", 2, set_args);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "Setting radius should return a result");
    lua_value_destroy(result);
    
    // Get radius property
    EseLuaValue *get_args[1];
    get_args[0] = arc_result;
    
    result = lua_engine_call_method(g_engine, "radius", 1, get_args);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "Getting radius should return a result");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_number(result), "Result should be a number");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, lua_value_get_number(result));
    lua_value_destroy(result);
    
    // Test decimal value
    set_args[1] = lua_value_create_number("value", 0.5);
    result = lua_engine_call_method(g_engine, "radius", 2, set_args);
    lua_value_destroy(result);
    
    result = lua_engine_call_method(g_engine, "radius", 1, get_args);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, lua_value_get_number(result));
    lua_value_destroy(result);
    
    // Clean up
    for (int i = 0; i < 5; i++) {
        lua_value_destroy(arc_args[i]);
    }
    lua_value_destroy(arc_result);
    lua_value_destroy(set_args[1]);
}

static void test_ese_arc_lua_start_angle(void) {
    // Create an arc
    EseLuaValue *arc_args[5];
    arc_args[0] = lua_value_create_number("x", 0.0);
    arc_args[1] = lua_value_create_number("y", 0.0);
    arc_args[2] = lua_value_create_number("radius", 1.0);
    arc_args[3] = lua_value_create_number("start_angle", 0.0);
    arc_args[4] = lua_value_create_number("end_angle", 6.28);
    
    EseLuaValue *arc_result = lua_engine_call_function(g_engine, "Arc", "new", 5, arc_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_arc(arc_result), "Arc creation should succeed");
    
    // Test setting and getting start_angle property
    EseLuaValue *set_args[2];
    set_args[0] = arc_result;
    set_args[1] = lua_value_create_number("value", 1.57);
    
    EseLuaValue *result = lua_engine_call_method(g_engine, "start_angle", 2, set_args);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "Setting start_angle should return a result");
    lua_value_destroy(result);
    
    // Get start_angle property
    EseLuaValue *get_args[1];
    get_args[0] = arc_result;
    
    result = lua_engine_call_method(g_engine, "start_angle", 1, get_args);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "Getting start_angle should return a result");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_number(result), "Result should be a number");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.57f, lua_value_get_number(result));
    lua_value_destroy(result);
    
    // Test negative value
    set_args[1] = lua_value_create_number("value", -1.57);
    result = lua_engine_call_method(g_engine, "start_angle", 2, set_args);
    lua_value_destroy(result);
    
    result = lua_engine_call_method(g_engine, "start_angle", 1, get_args);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.57f, lua_value_get_number(result));
    lua_value_destroy(result);
    
    // Clean up
    for (int i = 0; i < 5; i++) {
        lua_value_destroy(arc_args[i]);
    }
    lua_value_destroy(arc_result);
    lua_value_destroy(set_args[1]);
}

static void test_ese_arc_lua_end_angle(void) {
    // Create an arc
    EseLuaValue *arc_args[5];
    arc_args[0] = lua_value_create_number("x", 0.0);
    arc_args[1] = lua_value_create_number("y", 0.0);
    arc_args[2] = lua_value_create_number("radius", 1.0);
    arc_args[3] = lua_value_create_number("start_angle", 0.0);
    arc_args[4] = lua_value_create_number("end_angle", 6.28);
    
    EseLuaValue *arc_result = lua_engine_call_function(g_engine, "Arc", "new", 5, arc_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_arc(arc_result), "Arc creation should succeed");
    
    // Test setting and getting end_angle property
    EseLuaValue *set_args[2];
    set_args[0] = arc_result;
    set_args[1] = lua_value_create_number("value", 3.14);
    
    EseLuaValue *result = lua_engine_call_method(g_engine, "end_angle", 2, set_args);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "Setting end_angle should return a result");
    lua_value_destroy(result);
    
    // Get end_angle property
    EseLuaValue *get_args[1];
    get_args[0] = arc_result;
    
    result = lua_engine_call_method(g_engine, "end_angle", 1, get_args);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "Getting end_angle should return a result");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_number(result), "Result should be a number");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.14f, lua_value_get_number(result));
    lua_value_destroy(result);
    
    // Test negative value
    set_args[1] = lua_value_create_number("value", -1.57);
    result = lua_engine_call_method(g_engine, "end_angle", 2, set_args);
    lua_value_destroy(result);
    
    result = lua_engine_call_method(g_engine, "end_angle", 1, get_args);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.57f, lua_value_get_number(result));
    lua_value_destroy(result);
    
    // Clean up
    for (int i = 0; i < 5; i++) {
        lua_value_destroy(arc_args[i]);
    }
    lua_value_destroy(arc_result);
    lua_value_destroy(set_args[1]);
}

static void test_ese_arc_lua_tostring(void) {
    // Create an arc
    EseLuaValue *arc_args[5];
    arc_args[0] = lua_value_create_number("x", 10.5);
    arc_args[1] = lua_value_create_number("y", 20.25);
    arc_args[2] = lua_value_create_number("radius", 5.0);
    arc_args[3] = lua_value_create_number("start_angle", 1.57);
    arc_args[4] = lua_value_create_number("end_angle", 4.71);
    
    EseLuaValue *arc_result = lua_engine_call_function(g_engine, "Arc", "new", 5, arc_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_arc(arc_result), "Arc creation should succeed");
    
    // Test tostring method
    EseLuaValue *method_args[1];
    method_args[0] = arc_result;
    
    EseLuaValue *result = lua_engine_call_method(g_engine, "tostring", 1, method_args);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "tostring should return a result");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_string(result), "Result should be a string");
    
    const char *str_result = lua_value_get_string(result);
    TEST_ASSERT_NOT_NULL_MESSAGE(str_result, "tostring result should not be NULL");
    TEST_ASSERT_TRUE_MESSAGE(strstr(str_result, "Arc:") != NULL, "tostring should contain 'Arc:'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(str_result, "x=10.50") != NULL, "tostring should contain 'x=10.50'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(str_result, "y=20.25") != NULL, "tostring should contain 'y=20.25'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(str_result, "r=5.00") != NULL, "tostring should contain 'r=5.00'");
    
    lua_value_destroy(result);
    
    // Clean up
    for (int i = 0; i < 5; i++) {
        lua_value_destroy(arc_args[i]);
    }
    lua_value_destroy(arc_result);
}

static void test_ese_arc_lua_gc(void) {
    // Create an arc that will be garbage collected
    EseLuaValue *arc_args[5];
    arc_args[0] = lua_value_create_number("x", 5.0);
    arc_args[1] = lua_value_create_number("y", 10.0);
    arc_args[2] = lua_value_create_number("radius", 3.0);
    arc_args[3] = lua_value_create_number("start_angle", 0.0);
    arc_args[4] = lua_value_create_number("end_angle", 6.28);
    
    EseLuaValue *arc_result = lua_engine_call_function(g_engine, "Arc", "new", 5, arc_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_arc(arc_result), "Arc creation should succeed");
    
    // Get the arc and reference it
    EseArc *arc = lua_value_get_arc(arc_result);
    TEST_ASSERT_NOT_NULL_MESSAGE(arc, "Extracted arc should not be NULL");
    ese_arc_ref(g_engine, arc);
    
    // Test that referenced arc is not garbage collected
    lua_State *L = g_engine->L;
    int collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should run");
    
    // Unreference the arc
    ese_arc_unref(g_engine, arc);
    
    // Test that unreferenced arc can be garbage collected
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should run");
    
    // Clean up
    for (int i = 0; i < 5; i++) {
        lua_value_destroy(arc_args[i]);
    }
    lua_value_destroy(arc_result);
    
    // Verify GC didn't crash by running another operation
    EseLuaValue *verify_result = lua_engine_call_function(g_engine, "math", "abs", 1, (EseLuaValue*[]){lua_value_create_number("x", -42)});
    TEST_ASSERT_NOT_NULL_MESSAGE(verify_result, "Lua should still work after GC");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_number(verify_result), "Result should be a number");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(42.0, lua_value_get_number(verify_result), "Lua should return correct value after GC");
    lua_value_destroy(verify_result);
}

/**
* Additional Comprehensive Test Functions
*/

static void test_ese_arc_edge_cases(void) {
    // Test with very small radius
    EseArc *arc = ese_arc_create(g_engine);
    arc->radius = 0.001f;
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.001f, arc->radius);
    
    // Test with very large radius
    arc->radius = 1000000.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1000000.0f, arc->radius);
    
    // Test with negative radius (should be handled gracefully)
    arc->radius = -5.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -5.0f, arc->radius);
    
    // Test with very large angles
    arc->start_angle = 100.0f * M_PI;
    arc->end_angle = 200.0f * M_PI;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f * M_PI, arc->start_angle);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 200.0f * M_PI, arc->end_angle);
    
    // Test contains_point with edge case tolerance
    arc->x = 0.0f;
    arc->y = 0.0f;
    arc->radius = 1.0f;
    arc->start_angle = 0.0f;
    arc->end_angle = 2.0f * M_PI;
    
    TEST_ASSERT_TRUE_MESSAGE(ese_arc_contains_point(arc, 1.0f, 0.0f, 0.0f), "Point exactly on arc should be contained with zero tolerance");
    TEST_ASSERT_FALSE_MESSAGE(ese_arc_contains_point(arc, 1.1f, 0.0f, 0.0f), "Point just outside arc should not be contained with zero tolerance");
    TEST_ASSERT_TRUE_MESSAGE(ese_arc_contains_point(arc, 1.1f, 0.0f, 0.2f), "Point just outside arc should be contained with larger tolerance");
    
    ese_arc_destroy(arc);
}

static void test_ese_arc_error_conditions(void) {
    // Test NULL arc with contains_point
    TEST_ASSERT_FALSE_MESSAGE(ese_arc_contains_point(NULL, 0.0f, 0.0f, 0.1f), "contains_point should return false for NULL arc");
    
    // Test NULL arc with get_length
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_arc_get_length(NULL));
    
    // Test NULL arc with get_point_at_angle
    float x, y;
    TEST_ASSERT_FALSE_MESSAGE(ese_arc_get_point_at_angle(NULL, 0.0f, &x, &y), "get_point_at_angle should return false for NULL arc");
    
    // Test NULL output pointers with get_point_at_angle
    EseArc *arc = ese_arc_create(g_engine);
    TEST_ASSERT_FALSE_MESSAGE(ese_arc_get_point_at_angle(arc, 0.0f, NULL, &y), "get_point_at_angle should return false for NULL x pointer");
    TEST_ASSERT_FALSE_MESSAGE(ese_arc_get_point_at_angle(arc, 0.0f, &x, NULL), "get_point_at_angle should return false for NULL y pointer");
    TEST_ASSERT_FALSE_MESSAGE(ese_arc_get_point_at_angle(arc, 0.0f, NULL, NULL), "get_point_at_angle should return false for NULL pointers");
    
    // Test intersects_rect with NULL arc
    EseRect *rect = ese_rect_create(g_engine);
    TEST_ASSERT_FALSE_MESSAGE(ese_arc_intersects_rect(NULL, rect), "intersects_rect should return false for NULL arc");
    TEST_ASSERT_FALSE_MESSAGE(ese_arc_intersects_rect(arc, NULL), "intersects_rect should return false for NULL rect");
    TEST_ASSERT_FALSE_MESSAGE(ese_arc_intersects_rect(NULL, NULL), "intersects_rect should return false for NULL arc and rect");
    
    ese_arc_destroy(arc);
    ese_rect_destroy(rect);
}static void test_ese_arc_lua_metamethods(void) {
    // Test __index metamethod with invalid property
    EseLuaValue *arc_args[5];
    arc_args[0] = lua_value_create_number("x", 0.0);
    arc_args[1] = lua_value_create_number("y", 0.0);
    arc_args[2] = lua_value_create_number("radius", 1.0);
    arc_args[3] = lua_value_create_number("start_angle", 0.0);
    arc_args[4] = lua_value_create_number("end_angle", 6.28);
    
    EseLuaValue *arc_result = lua_engine_call_function(g_engine, "Arc", "new", 5, arc_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_arc(arc_result), "Arc creation should succeed");
    
    // Test accessing invalid property
    EseLuaValue *method_args[1];
    method_args[0] = arc_result;
    
    EseLuaValue *result = lua_engine_call_method(g_engine, "invalid_property", 1, method_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_error(result), "Accessing invalid property should return an error");
    lua_value_destroy(result);
    
    // Test __newindex metamethod with invalid property
    EseLuaValue *set_args[2];
    set_args[0] = arc_result;
    set_args[1] = lua_value_create_number("value", 10.0);
    
    result = lua_engine_call_method(g_engine, "invalid_property", 2, set_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_error(result), "Setting invalid property should return an error");
    lua_value_destroy(result);
    
    // Clean up
    for (int i = 0; i < 5; i++) {
        lua_value_destroy(arc_args[i]);
    }
    lua_value_destroy(arc_result);
    lua_value_destroy(set_args[1]);
}

static void test_ese_arc_lua_constructor_errors(void) {
    // Test Arc.new with wrong argument types
    EseLuaValue *wrong_args[5];
    wrong_args[0] = lua_value_create_string("x", "not_a_number");
    wrong_args[1] = lua_value_create_string("y", "not_a_number");
    wrong_args[2] = lua_value_create_string("radius", "not_a_number");
    wrong_args[3] = lua_value_create_string("start_angle", "not_a_number");
    wrong_args[4] = lua_value_create_string("end_angle", "not_a_number");
    
    EseLuaValue *result = lua_engine_call_function(g_engine, "Arc", "new", 5, wrong_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_error(result), "Arc.new with string arguments should return an error");
    lua_value_destroy(result);
    
    // Test Arc.new with boolean arguments
    wrong_args[0] = lua_value_create_boolean("x", true);
    wrong_args[1] = lua_value_create_boolean("y", false);
    wrong_args[2] = lua_value_create_boolean("radius", true);
    wrong_args[3] = lua_value_create_boolean("start_angle", false);
    wrong_args[4] = lua_value_create_boolean("end_angle", true);
    
    result = lua_engine_call_function(g_engine, "Arc", "new", 5, wrong_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_error(result), "Arc.new with boolean arguments should return an error");
    lua_value_destroy(result);
    
    // Test Arc.zero with wrong argument types
    EseLuaValue *zero_wrong_args[1];
    zero_wrong_args[0] = lua_value_create_string("x", "not_a_number");
    
    result = lua_engine_call_function(g_engine, "Arc", "zero", 1, zero_wrong_args);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_error(result), "Arc.zero with string argument should return an error");
    lua_value_destroy(result);
    
    // Clean up
    for (int i = 0; i < 5; i++) {
        lua_value_destroy(wrong_args[i]);
    }
    lua_value_destroy(zero_wrong_args[0]);
}


