/*
* test_ese_poly_line.c - Unity-based tests for poly_line functionality
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

#include "../src/types/poly_line.h"
#include "../src/types/point.h"
#include "../src/types/color.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

/**
* C API Test Functions Declarations
*/
static void test_ese_poly_line_sizeof(void);
static void test_ese_poly_line_create_requires_engine(void);
static void test_ese_poly_line_create(void);
static void test_ese_poly_line_type(void);
static void test_ese_poly_line_stroke_width(void);
static void test_ese_poly_line_stroke_color(void);
static void test_ese_poly_line_fill_color(void);
static void test_ese_poly_line_ref(void);
static void test_ese_poly_line_copy_requires_engine(void);
static void test_ese_poly_line_copy(void);
static void test_ese_poly_line_add_point(void);
static void test_ese_poly_line_remove_point(void);
static void test_ese_poly_line_get_point(void);
static void test_ese_poly_line_get_point_count(void);
static void test_ese_poly_line_clear_points(void);
static void test_ese_poly_line_get_points(void);
static void test_ese_poly_line_watcher_system(void);
static void test_ese_poly_line_lua_integration(void);
static void test_ese_poly_line_lua_init(void);
static void test_ese_poly_line_lua_push(void);
static void test_ese_poly_line_lua_get(void);

/**
* Lua API Test Functions Declarations
*/
static void test_ese_poly_line_lua_new(void);
static void test_ese_poly_line_lua_type(void);
static void test_ese_poly_line_lua_stroke_width(void);
static void test_ese_poly_line_lua_stroke_color(void);
static void test_ese_poly_line_lua_fill_color(void);
static void test_ese_poly_line_lua_add_point(void);
static void test_ese_poly_line_lua_remove_point(void);
static void test_ese_poly_line_lua_get_point(void);
static void test_ese_poly_line_lua_get_point_count(void);
static void test_ese_poly_line_lua_clear_points(void);
static void test_ese_poly_line_lua_tostring(void);
static void test_ese_poly_line_lua_gc(void);

/**
* Mock watcher callback for testing
*/
static bool watcher_called = false;
static EsePolyLine *last_watched_poly_line = NULL;
static void *last_watcher_userdata = NULL;

static void test_watcher_callback(EsePolyLine *poly_line, void *userdata) {
    watcher_called = true;
    last_watched_poly_line = poly_line;
    last_watcher_userdata = userdata;
}

static void mock_reset(void) {
    watcher_called = false;
    last_watched_poly_line = NULL;
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

    printf("\nEsePolyLine Tests\n");
    printf("------------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_ese_poly_line_sizeof);
    RUN_TEST(test_ese_poly_line_create_requires_engine);
    RUN_TEST(test_ese_poly_line_create);
    RUN_TEST(test_ese_poly_line_type);
    RUN_TEST(test_ese_poly_line_stroke_width);
    RUN_TEST(test_ese_poly_line_stroke_color);
    RUN_TEST(test_ese_poly_line_fill_color);
    RUN_TEST(test_ese_poly_line_ref);
    RUN_TEST(test_ese_poly_line_copy_requires_engine);
    RUN_TEST(test_ese_poly_line_copy);
    RUN_TEST(test_ese_poly_line_add_point);
    RUN_TEST(test_ese_poly_line_remove_point);
    RUN_TEST(test_ese_poly_line_get_point);
    RUN_TEST(test_ese_poly_line_get_point_count);
    RUN_TEST(test_ese_poly_line_clear_points);
    RUN_TEST(test_ese_poly_line_get_points);
    RUN_TEST(test_ese_poly_line_watcher_system);
    RUN_TEST(test_ese_poly_line_lua_integration);
    RUN_TEST(test_ese_poly_line_lua_init);
    RUN_TEST(test_ese_poly_line_lua_push);
    RUN_TEST(test_ese_poly_line_lua_get);

    RUN_TEST(test_ese_poly_line_lua_new);
    RUN_TEST(test_ese_poly_line_lua_type);
    RUN_TEST(test_ese_poly_line_lua_stroke_width);
    RUN_TEST(test_ese_poly_line_lua_stroke_color);
    RUN_TEST(test_ese_poly_line_lua_fill_color);
    RUN_TEST(test_ese_poly_line_lua_add_point);
    RUN_TEST(test_ese_poly_line_lua_remove_point);
    RUN_TEST(test_ese_poly_line_lua_get_point);
    RUN_TEST(test_ese_poly_line_lua_get_point_count);
    RUN_TEST(test_ese_poly_line_lua_clear_points);
    RUN_TEST(test_ese_poly_line_lua_tostring);
    RUN_TEST(test_ese_poly_line_lua_gc);

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_ese_poly_line_sizeof(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, ese_poly_line_sizeof(), "PolyLine size should be > 0");
}

static void test_ese_poly_line_create_requires_engine(void) {
    ASSERT_DEATH(ese_poly_line_create(NULL), "ese_poly_line_create should abort with NULL engine");
}

static void test_ese_poly_line_create(void) {
    EsePolyLine *poly_line = ese_poly_line_create(g_engine);

    TEST_ASSERT_NOT_NULL_MESSAGE(poly_line, "PolyLine should be created");
    TEST_ASSERT_EQUAL_INT_MESSAGE(POLY_LINE_OPEN, ese_poly_line_get_type(poly_line), "Default type should be OPEN");
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, ese_poly_line_get_stroke_width(poly_line));
    TEST_ASSERT_NULL_MESSAGE(ese_poly_line_get_stroke_color(poly_line), "Default stroke color should be NULL");
    TEST_ASSERT_NULL_MESSAGE(ese_poly_line_get_fill_color(poly_line), "Default fill color should be NULL");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_poly_line_get_point_count(poly_line), "Default point count should be 0");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_poly_line_get_state(poly_line), "PolyLine should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_poly_line_get_lua_ref_count(poly_line), "New polyline should have ref count 0");

    ese_poly_line_destroy(poly_line);
}

static void test_ese_poly_line_type(void) {
    EsePolyLine *poly_line = ese_poly_line_create(g_engine);

    ese_poly_line_set_type(poly_line, POLY_LINE_CLOSED);
    TEST_ASSERT_EQUAL_INT_MESSAGE(POLY_LINE_CLOSED, ese_poly_line_get_type(poly_line), "Type should be CLOSED");

    ese_poly_line_set_type(poly_line, POLY_LINE_FILLED);
    TEST_ASSERT_EQUAL_INT_MESSAGE(POLY_LINE_FILLED, ese_poly_line_get_type(poly_line), "Type should be FILLED");

    ese_poly_line_set_type(poly_line, POLY_LINE_OPEN);
    TEST_ASSERT_EQUAL_INT_MESSAGE(POLY_LINE_OPEN, ese_poly_line_get_type(poly_line), "Type should be OPEN");

    ese_poly_line_destroy(poly_line);
}

static void test_ese_poly_line_stroke_width(void) {
    EsePolyLine *poly_line = ese_poly_line_create(g_engine);

    ese_poly_line_set_stroke_width(poly_line, 2.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, ese_poly_line_get_stroke_width(poly_line));

    ese_poly_line_set_stroke_width(poly_line, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_poly_line_get_stroke_width(poly_line));

    ese_poly_line_set_stroke_width(poly_line, -1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, ese_poly_line_get_stroke_width(poly_line));

    ese_poly_line_destroy(poly_line);
}

static void test_ese_poly_line_stroke_color(void) {
    EsePolyLine *poly_line = ese_poly_line_create(g_engine);
    EseColor *color = ese_color_create(g_engine);

    ese_poly_line_set_stroke_color(poly_line, color);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(color, ese_poly_line_get_stroke_color(poly_line), "Stroke color should be set");

    ese_poly_line_set_stroke_color(poly_line, NULL);
    TEST_ASSERT_NULL_MESSAGE(ese_poly_line_get_stroke_color(poly_line), "Stroke color should be NULL");

    ese_color_destroy(color);
    ese_poly_line_destroy(poly_line);
}

static void test_ese_poly_line_fill_color(void) {
    EsePolyLine *poly_line = ese_poly_line_create(g_engine);
    EseColor *color = ese_color_create(g_engine);

    ese_poly_line_set_fill_color(poly_line, color);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(color, ese_poly_line_get_fill_color(poly_line), "Fill color should be set");

    ese_poly_line_set_fill_color(poly_line, NULL);
    TEST_ASSERT_NULL_MESSAGE(ese_poly_line_get_fill_color(poly_line), "Fill color should be NULL");

    ese_color_destroy(color);
    ese_poly_line_destroy(poly_line);
}

static void test_ese_poly_line_ref(void) {
    EsePolyLine *poly_line = ese_poly_line_create(g_engine);

    ese_poly_line_ref(poly_line);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, ese_poly_line_get_lua_ref_count(poly_line), "Ref count should be 1");

    ese_poly_line_unref(poly_line);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_poly_line_get_lua_ref_count(poly_line), "Ref count should be 0");

    ese_poly_line_destroy(poly_line);
}

static void test_ese_poly_line_copy_requires_engine(void) {
    ASSERT_DEATH(ese_poly_line_copy(NULL), "ese_poly_line_copy should abort with NULL polyline");
}

static void test_ese_poly_line_copy(void) {
    EsePolyLine *poly_line = ese_poly_line_create(g_engine);
    ese_poly_line_ref(poly_line);
    ese_poly_line_set_type(poly_line, POLY_LINE_CLOSED);
    ese_poly_line_set_stroke_width(poly_line, 2.5f);
    
    // Add some points
    EsePoint *point1 = ese_point_create(g_engine);
    ese_point_set_x(point1, 10.0f);
    ese_point_set_y(point1, 20.0f);
    ese_poly_line_add_point(poly_line, point1);
    
    EsePoint *point2 = ese_point_create(g_engine);
    ese_point_set_x(point2, 30.0f);
    ese_point_set_y(point2, 40.0f);
    ese_poly_line_add_point(poly_line, point2);
    
    EsePolyLine *copy = ese_poly_line_copy(poly_line);
    TEST_ASSERT_NOT_NULL_MESSAGE(copy, "Copy should be created");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_poly_line_get_state(copy), "Copy should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_poly_line_get_lua_ref_count(copy), "Copy should have ref count 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(POLY_LINE_CLOSED, ese_poly_line_get_type(copy), "Copied type should match original");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, ese_poly_line_get_stroke_width(copy));
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, ese_poly_line_get_point_count(copy), "Copied point count should match original");
    
    // Test that points are deep copied
    EsePoint *copy_point1 = ese_poly_line_get_point(copy, 0);
    EsePoint *copy_point2 = ese_poly_line_get_point(copy, 1);
    TEST_ASSERT_NOT_NULL_MESSAGE(copy_point1, "First copied point should exist");
    TEST_ASSERT_NOT_NULL_MESSAGE(copy_point2, "Second copied point should exist");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_point_get_x(copy_point1));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, ese_point_get_y(copy_point1));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.0f, ese_point_get_x(copy_point2));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 40.0f, ese_point_get_y(copy_point2));

    ese_point_destroy(point1);
    ese_point_destroy(point2);
    ese_poly_line_unref(poly_line);
    ese_poly_line_destroy(poly_line);
    ese_poly_line_destroy(copy);
}

static void test_ese_poly_line_add_point(void) {
    EsePolyLine *poly_line = ese_poly_line_create(g_engine);
    EsePoint *point = ese_point_create(g_engine);
    
    ese_point_set_x(point, 10.0f);
    ese_point_set_y(point, 20.0f);
    
    bool success = ese_poly_line_add_point(poly_line, point);
    TEST_ASSERT_TRUE_MESSAGE(success, "Should successfully add point");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, ese_poly_line_get_point_count(poly_line), "Point count should be 1");
    
    EsePoint *retrieved_point = ese_poly_line_get_point(poly_line, 0);
    TEST_ASSERT_NOT_NULL_MESSAGE(retrieved_point, "Retrieved point should not be NULL");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_point_get_x(retrieved_point));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, ese_point_get_y(retrieved_point));
    
    ese_point_destroy(point);
    ese_poly_line_destroy(poly_line);
}

static void test_ese_poly_line_remove_point(void) {
    EsePolyLine *poly_line = ese_poly_line_create(g_engine);
    EsePoint *point1 = ese_point_create(g_engine);
    EsePoint *point2 = ese_point_create(g_engine);
    
    ese_point_set_x(point1, 10.0f);
    ese_point_set_y(point1, 20.0f);
    ese_point_set_x(point2, 30.0f);
    ese_point_set_y(point2, 40.0f);
    
    ese_poly_line_add_point(poly_line, point1);
    ese_poly_line_add_point(poly_line, point2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, ese_poly_line_get_point_count(poly_line), "Point count should be 2");
    
    bool success = ese_poly_line_remove_point(poly_line, 0);
    TEST_ASSERT_TRUE_MESSAGE(success, "Should successfully remove point");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, ese_poly_line_get_point_count(poly_line), "Point count should be 1");
    
    EsePoint *remaining_point = ese_poly_line_get_point(poly_line, 0);
    TEST_ASSERT_NOT_NULL_MESSAGE(remaining_point, "Remaining point should not be NULL");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.0f, ese_point_get_x(remaining_point));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 40.0f, ese_point_get_y(remaining_point));
    
    success = ese_poly_line_remove_point(poly_line, 5);
    TEST_ASSERT_FALSE_MESSAGE(success, "Should fail to remove point at invalid index");
    
    ese_point_destroy(point1);
    ese_point_destroy(point2);
    ese_poly_line_destroy(poly_line);
}

static void test_ese_poly_line_get_point(void) {
    EsePolyLine *poly_line = ese_poly_line_create(g_engine);
    EsePoint *point = ese_point_create(g_engine);
    
    ese_point_set_x(point, 15.0f);
    ese_point_set_y(point, 25.0f);
    ese_poly_line_add_point(poly_line, point);
    
    EsePoint *retrieved_point = ese_poly_line_get_point(poly_line, 0);
    TEST_ASSERT_NOT_NULL_MESSAGE(retrieved_point, "Retrieved point should not be NULL");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, ese_point_get_x(retrieved_point));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, ese_point_get_y(retrieved_point));
    
    EsePoint *invalid_point = ese_poly_line_get_point(poly_line, 5);
    TEST_ASSERT_NULL_MESSAGE(invalid_point, "Getting point at invalid index should return NULL");
    
    ese_point_destroy(point);
    ese_poly_line_destroy(poly_line);
}

static void test_ese_poly_line_get_point_count(void) {
    EsePolyLine *poly_line = ese_poly_line_create(g_engine);
    
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_poly_line_get_point_count(poly_line), "Initial point count should be 0");
    
    EsePoint *point1 = ese_point_create(g_engine);
    EsePoint *point2 = ese_point_create(g_engine);
    
    ese_poly_line_add_point(poly_line, point1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, ese_poly_line_get_point_count(poly_line), "Point count should be 1");
    
    ese_poly_line_add_point(poly_line, point2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, ese_poly_line_get_point_count(poly_line), "Point count should be 2");
    
    ese_point_destroy(point1);
    ese_point_destroy(point2);
    ese_poly_line_destroy(poly_line);
}

static void test_ese_poly_line_clear_points(void) {
    EsePolyLine *poly_line = ese_poly_line_create(g_engine);
    EsePoint *point1 = ese_point_create(g_engine);
    EsePoint *point2 = ese_point_create(g_engine);
    
    ese_poly_line_add_point(poly_line, point1);
    ese_poly_line_add_point(poly_line, point2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, ese_poly_line_get_point_count(poly_line), "Point count should be 2");
    
    ese_poly_line_clear_points(poly_line);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_poly_line_get_point_count(poly_line), "Point count should be 0 after clearing");
    
    ese_point_destroy(point1);
    ese_point_destroy(point2);
    ese_poly_line_destroy(poly_line);
}

static void test_ese_poly_line_get_points(void) {
    EsePolyLine *poly_line = ese_poly_line_create(g_engine);
    EsePoint *point1 = ese_point_create(g_engine);
    EsePoint *point2 = ese_point_create(g_engine);
    
    // Test with no points
    const float *points = ese_poly_line_get_points(poly_line);
    TEST_ASSERT_NULL_MESSAGE(points, "Points should be NULL when no points added");
    
    // Add points
    ese_point_set_x(point1, 10.0f);
    ese_point_set_y(point1, 20.0f);
    ese_point_set_x(point2, 30.0f);
    ese_point_set_y(point2, 40.0f);
    
    ese_poly_line_add_point(poly_line, point1);
    ese_poly_line_add_point(poly_line, point2);
    
    // Test with points
    points = ese_poly_line_get_points(poly_line);
    TEST_ASSERT_NOT_NULL_MESSAGE(points, "Points should not be NULL when points are added");
    
    // Verify the coordinates are stored correctly (x1, y1, x2, y2, ...)
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, 10.0f, points[0], "First point x should be 10.0");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, 20.0f, points[1], "First point y should be 20.0");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, 30.0f, points[2], "Second point x should be 30.0");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, 40.0f, points[3], "Second point y should be 40.0");
    
    ese_point_destroy(point1);
    ese_point_destroy(point2);
    ese_poly_line_destroy(poly_line);
}

static void test_ese_poly_line_watcher_system(void) {
    EsePolyLine *poly_line = ese_poly_line_create(g_engine);

    mock_reset();
    ese_poly_line_set_type(poly_line, POLY_LINE_CLOSED);
    TEST_ASSERT_FALSE_MESSAGE(watcher_called, "Watcher should not be called before adding");

    void *test_userdata = (void*)0x12345678;
    bool add_result = ese_poly_line_add_watcher(poly_line, test_watcher_callback, test_userdata);
    TEST_ASSERT_TRUE_MESSAGE(add_result, "Should successfully add watcher");

    mock_reset();
    ese_poly_line_set_type(poly_line, POLY_LINE_FILLED);
    TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when type changes");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(poly_line, last_watched_poly_line, "Watcher should receive correct polyline pointer");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(test_userdata, last_watcher_userdata, "Watcher should receive correct userdata");

    mock_reset();
    ese_poly_line_set_stroke_width(poly_line, 2.0f);
    TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when stroke width changes");

    mock_reset();
    EseColor *color = ese_color_create(g_engine);
    ese_poly_line_set_stroke_color(poly_line, color);
    TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when stroke color changes");

    mock_reset();
    ese_poly_line_set_fill_color(poly_line, color);
    TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when fill color changes");

    mock_reset();
    EsePoint *point = ese_point_create(g_engine);
    ese_poly_line_add_point(poly_line, point);
    TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when point is added");

    mock_reset();
    ese_poly_line_clear_points(poly_line);
    TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when points are cleared");

    bool remove_result = ese_poly_line_remove_watcher(poly_line, test_watcher_callback, test_userdata);
    TEST_ASSERT_TRUE_MESSAGE(remove_result, "Should successfully remove watcher");

    mock_reset();
    ese_poly_line_set_type(poly_line, POLY_LINE_OPEN);
    TEST_ASSERT_FALSE_MESSAGE(watcher_called, "Watcher should not be called after removal");

    ese_color_destroy(color);
    ese_point_destroy(point);
    ese_poly_line_destroy(poly_line);
}

static void test_ese_poly_line_lua_integration(void) {
    EseLuaEngine *engine = create_test_engine();
    EsePolyLine *poly_line = ese_poly_line_create(engine);

    lua_State *before_state = ese_poly_line_get_state(poly_line);
    TEST_ASSERT_NOT_NULL_MESSAGE(before_state, "PolyLine should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, before_state, "PolyLine state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_poly_line_get_lua_ref(poly_line), "PolyLine should have no Lua reference initially");

    ese_poly_line_ref(poly_line);
    lua_State *after_ref_state = ese_poly_line_get_state(poly_line);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_ref_state, "PolyLine should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_ref_state, "PolyLine state should match engine runtime");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(LUA_NOREF, ese_poly_line_get_lua_ref(poly_line), "PolyLine should have a valid Lua reference after ref");

    ese_poly_line_unref(poly_line);
    lua_State *after_unref_state = ese_poly_line_get_state(poly_line);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_unref_state, "PolyLine should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_unref_state, "PolyLine state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_poly_line_get_lua_ref(poly_line), "PolyLine should have no Lua reference after unref");

    ese_poly_line_destroy(poly_line);
    lua_engine_destroy(engine);
}

static void test_ese_poly_line_lua_init(void) {
    lua_State *L = g_engine->runtime;
    
    luaL_getmetatable(L, POLY_LINE_PROXY_META);
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Metatable should not exist before initialization");
    lua_pop(L, 1);
    
    lua_getglobal(L, "PolyLine");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Global PolyLine table should not exist before initialization");
    lua_pop(L, 1);
    
    ese_poly_line_lua_init(g_engine);
    
    luaL_getmetatable(L, POLY_LINE_PROXY_META);
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Metatable should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Metatable should be a table");
    lua_pop(L, 1);
    
    lua_getglobal(L, "PolyLine");
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Global PolyLine table should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Global PolyLine table should be a table");
    lua_pop(L, 1);
}

static void test_ese_poly_line_lua_push(void) {
    ese_poly_line_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EsePolyLine *poly_line = ese_poly_line_create(g_engine);
    
    ese_poly_line_lua_push(poly_line);
    
    EsePolyLine **ud = (EsePolyLine **)lua_touserdata(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(poly_line, *ud, "The pushed item should be the actual polyline");
    
    lua_pop(L, 1); 
    
    ese_poly_line_destroy(poly_line);
}

static void test_ese_poly_line_lua_get(void) {
    ese_poly_line_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EsePolyLine *poly_line = ese_poly_line_create(g_engine);
    
    ese_poly_line_lua_push(poly_line);
    
    EsePolyLine *extracted_poly_line = ese_poly_line_lua_get(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(poly_line, extracted_poly_line, "Extracted polyline should match original");
    
    lua_pop(L, 1);
    ese_poly_line_destroy(poly_line);
}

/**
* Lua API Test Functions
*/

static void test_ese_poly_line_lua_new(void) {
    ese_poly_line_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code = "return PolyLine.new()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "PolyLine.new() should execute without error");
    EsePolyLine *extracted_poly_line = ese_poly_line_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_poly_line, "Extracted polyline should not be NULL");
    TEST_ASSERT_EQUAL_INT_MESSAGE(POLY_LINE_OPEN, ese_poly_line_get_type(extracted_poly_line), "New polyline should have OPEN type");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ese_poly_line_get_stroke_width(extracted_poly_line));
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_poly_line_get_point_count(extracted_poly_line), "New polyline should have 0 points");
    ese_poly_line_destroy(extracted_poly_line);
}

static void test_ese_poly_line_lua_type(void) {
    ese_poly_line_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local p = PolyLine.new(); p.type = 1; return p.type";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua type set/get test 1 should execute without error");
    int type = (int)lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, type, "Type should be 1 (CLOSED)");
    lua_pop(L, 1);

    const char *test2 = "local p = PolyLine.new(); p.type = 2; return p.type";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua type set/get test 2 should execute without error");
    type = (int)lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, type, "Type should be 2 (FILLED)");
    lua_pop(L, 1);

    const char *test3 = "local p = PolyLine.new(); p.type = 0; return p.type";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua type set/get test 3 should execute without error");
    type = (int)lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, type, "Type should be 0 (OPEN)");
    lua_pop(L, 1);

    const char *test4 = "local p = PolyLine.new(); p.type = 3; return p.type";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test4), "Invalid type should cause error");
}

static void test_ese_poly_line_lua_stroke_width(void) {
    ese_poly_line_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local p = PolyLine.new(); p.stroke_width = 2.5; return p.stroke_width";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua stroke_width set/get test 1 should execute without error");
    double width = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, width);
    lua_pop(L, 1);

    const char *test2 = "local p = PolyLine.new(); p.stroke_width = 0; return p.stroke_width";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua stroke_width set/get test 2 should execute without error");
    width = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, width);
    lua_pop(L, 1);

    const char *test3 = "local p = PolyLine.new(); p.stroke_width = \"invalid\"; return p.stroke_width";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Invalid stroke_width should cause error");
}

static void test_ese_poly_line_lua_stroke_color(void) {
    ese_poly_line_lua_init(g_engine);
    ese_color_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local p = PolyLine.new(); local c = Color.new(1, 0, 0); p.stroke_color = c; return p.stroke_color ~= nil";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua stroke_color set/get test 1 should execute without error");
    bool has_color = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(has_color, "Stroke color should be set");
    lua_pop(L, 1);

    const char *test2 = "local p = PolyLine.new(); p.stroke_color = nil; return p.stroke_color == nil";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua stroke_color set/get test 2 should execute without error");
    bool is_nil = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(is_nil, "Stroke color should be nil");
    lua_pop(L, 1);

    const char *test3 = "local p = PolyLine.new(); p.stroke_color = \"invalid\"; return p.stroke_color";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Invalid stroke_color should cause error");
}

static void test_ese_poly_line_lua_fill_color(void) {
    ese_poly_line_lua_init(g_engine);
    ese_color_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local p = PolyLine.new(); local c = Color.new(0, 1, 0); p.fill_color = c; return p.fill_color ~= nil";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua fill_color set/get test 1 should execute without error");
    bool has_color = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(has_color, "Fill color should be set");
    lua_pop(L, 1);

    const char *test2 = "local p = PolyLine.new(); p.fill_color = nil; return p.fill_color == nil";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua fill_color set/get test 2 should execute without error");
    bool is_nil = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(is_nil, "Fill color should be nil");
    lua_pop(L, 1);

    const char *test3 = "local p = PolyLine.new(); p.fill_color = \"invalid\"; return p.fill_color";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Invalid fill_color should cause error");
}

static void test_ese_poly_line_lua_add_point(void) {
    ese_poly_line_lua_init(g_engine);
    ese_point_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test_code = "local p = PolyLine.new(); local pt = Point.new(10, 20); p:add_point(pt); return p:get_point_count()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Lua add_point test should execute without error");
    int count = (int)lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, count, "Point count should be 1 after adding point");
    lua_pop(L, 1);

    const char *test_error = "local p = PolyLine.new(); p:add_point(\"invalid\"); return p:get_point_count()";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_error), "Invalid add_point should cause error");
}

static void test_ese_poly_line_lua_remove_point(void) {
    ese_poly_line_lua_init(g_engine);
    ese_point_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test_code = "local p = PolyLine.new(); local pt1 = Point.new(10, 20); local pt2 = Point.new(30, 40); p:add_point(pt1); p:add_point(pt2); p:remove_point(0); return p:get_point_count()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Lua remove_point test should execute without error");
    int count = (int)lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, count, "Point count should be 1 after removing point");
    lua_pop(L, 1);

    const char *test_error = "local p = PolyLine.new(); p:remove_point(5); return p:get_point_count()";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_error), "Invalid remove_point should cause error");
}

static void test_ese_poly_line_lua_get_point(void) {
    ese_poly_line_lua_init(g_engine);
    ese_point_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test_code = "local p = PolyLine.new(); local pt = Point.new(15, 25); p:add_point(pt); local retrieved = p:get_point(0); return retrieved.x, retrieved.y";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Lua get_point test should execute without error");
    double x = lua_tonumber(L, -2);
    double y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, y);
    lua_pop(L, 2);

    const char *test_error = "local p = PolyLine.new(); local pt = p:get_point(5); return pt";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_error), "Invalid get_point should cause error");
}

static void test_ese_poly_line_lua_get_point_count(void) {
    ese_poly_line_lua_init(g_engine);
    ese_point_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test_code = "local p = PolyLine.new(); local count1 = p:get_point_count(); local pt1 = Point.new(10, 20); local pt2 = Point.new(30, 40); p:add_point(pt1); p:add_point(pt2); local count2 = p:get_point_count(); return count1, count2";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Lua get_point_count test should execute without error");
    int count1 = (int)lua_tonumber(L, -2);
    int count2 = (int)lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, count1, "Initial point count should be 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, count2, "Point count should be 2 after adding points");
    lua_pop(L, 2);
}

static void test_ese_poly_line_lua_clear_points(void) {
    ese_poly_line_lua_init(g_engine);
    ese_point_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test_code = "local p = PolyLine.new(); local pt1 = Point.new(10, 20); local pt2 = Point.new(30, 40); p:add_point(pt1); p:add_point(pt2); local count1 = p:get_point_count(); p:clear_points(); local count2 = p:get_point_count(); return count1, count2";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Lua clear_points test should execute without error");
    int count1 = (int)lua_tonumber(L, -2);
    int count2 = (int)lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, count1, "Point count should be 2 before clearing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, count2, "Point count should be 0 after clearing");
    lua_pop(L, 2);
}

static void test_ese_poly_line_lua_tostring(void) {
    ese_poly_line_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test_code = "local p = PolyLine.new(); p.type = 1; p.stroke_width = 2.5; return tostring(p)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "tostring test should execute without error");
    const char *result = lua_tostring(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "tostring result should not be NULL");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "PolyLine:") != NULL, "tostring should contain 'PolyLine:'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "type=CLOSED") != NULL, "tostring should contain 'type=CLOSED'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "stroke_width=2.50") != NULL, "tostring should contain 'stroke_width=2.50'");
    lua_pop(L, 1);
}

static void test_ese_poly_line_lua_gc(void) {
    ese_poly_line_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *testA = "local p = PolyLine.new()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "PolyLine creation should execute without error");
    
    int collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testB = "return PolyLine.new()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "PolyLine creation should execute without error");
    EsePolyLine *extracted_poly_line = ese_poly_line_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_poly_line, "Extracted polyline should not be NULL");
    ese_poly_line_ref(extracted_poly_line);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    ese_poly_line_unref(extracted_poly_line);

    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testC = "return PolyLine.new()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "PolyLine creation should execute without error");
    extracted_poly_line = ese_poly_line_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_poly_line, "Extracted polyline should not be NULL");
    ese_poly_line_ref(extracted_poly_line);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    ese_poly_line_unref(extracted_poly_line);
    ese_poly_line_destroy(extracted_poly_line);

    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    // Verify GC didn't crash by running another operation
    const char *verify_code = "return 42";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, verify_code), "Lua should still work after GC");
    int result = (int)lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(42, result, "Lua should return correct value after GC");
    lua_pop(L, 1);
}

