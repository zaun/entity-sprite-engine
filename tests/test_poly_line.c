/*
 * Test file for poly_line functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <execinfo.h>
#include <signal.h>
#include "test_utils.h"
#include "../src/types/poly_line.h"
#include "../src/types/point.h"
#include "../src/types/color.h"
#include "../src/scripting/lua_engine.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

// Test function declarations
static void test_poly_line_creation();
static void test_poly_line_copy();
static void test_poly_line_properties();
static void test_poly_line_points_management();
static void test_poly_line_watchers();
static void test_poly_line_lua_integration();
static void test_poly_line_null_pointer_aborts();

// Helper function to create and initialize engine
static EseLuaEngine* create_test_engine() {
    EseLuaEngine *engine = lua_engine_create();
    if (engine) {
        // Set up registry keys that poly_line system needs
        lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);
        
        // Initialize required systems
        ese_point_lua_init(engine);
        color_lua_init(engine);
        poly_line_lua_init(engine);
    }
    return engine;
}

// Mock watcher callback
static bool mock_watcher_called = false;
static EsePolyLine *mock_watcher_poly_line = NULL;
static void *mock_watcher_userdata = NULL;

static void mock_watcher_callback(EsePolyLine *poly_line, void *userdata) {
    mock_watcher_called = true;
    mock_watcher_poly_line = poly_line;
    mock_watcher_userdata = userdata;
}

static void mock_reset() {
    mock_watcher_called = false;
    mock_watcher_poly_line = NULL;
    mock_watcher_userdata = NULL;
}

// Signal handler for segfaults
static void segfault_handler(int sig, siginfo_t *info, void *context) {
    printf("---- BACKTRACE START ----\n");
    void *array[10];
    size_t size = backtrace(array, 10);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    printf("---- BACKTRACE  END  ----\n");
    exit(1);
}

int main() {
    // Register signal handler for segfaults
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = segfault_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);

    test_suite_begin("PolyLine Tests");

    // Initialize required systems
    log_init();

    // Run all test suites
    test_poly_line_creation();
    test_poly_line_copy();
    test_poly_line_properties();
    test_poly_line_points_management();
    test_poly_line_watchers();
    test_poly_line_lua_integration();
    test_poly_line_null_pointer_aborts();

    test_suite_end("PolyLine Tests");

    return 0;
}

// Test basic poly_line creation
static void test_poly_line_creation() {
    test_begin("PolyLine Creation");
    
    EseLuaEngine *engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");
    
    EsePolyLine *poly_line = poly_line_create(engine);
    TEST_ASSERT_NOT_NULL(poly_line, "PolyLine should be created");
    
    // Test default values
    TEST_ASSERT_EQUAL(POLY_LINE_OPEN, poly_line_get_type(poly_line), "Default type should be OPEN");
    TEST_ASSERT_FLOAT_EQUAL(1.0f, poly_line_get_stroke_width(poly_line), 0.001f, "Default stroke width should be 1.0");
    TEST_ASSERT_NULL(poly_line_get_stroke_color(poly_line), "Default stroke color should be NULL");
    TEST_ASSERT_NULL(poly_line_get_fill_color(poly_line), "Default fill color should be NULL");
    TEST_ASSERT_EQUAL(0, poly_line_get_point_count(poly_line), "Default point count should be 0");
    
    // Clean up
    poly_line_destroy(poly_line);
    lua_engine_destroy(engine);
    
    test_end("PolyLine Creation");
}

// Test poly_line copying
static void test_poly_line_copy() {
    test_begin("PolyLine Copy");
    
    EseLuaEngine *engine = create_test_engine();
    EsePolyLine *original = poly_line_create(engine);
    
    // Set some values
    poly_line_set_type(original, POLY_LINE_CLOSED);
    poly_line_set_stroke_width(original, 2.5f);
    
    // Add some points
    EsePoint *point1 = ese_point_create(engine);
    ese_point_set_x(point1, 10.0f);
    ese_point_set_y(point1, 20.0f);
    poly_line_add_point(original, point1);
    
    EsePoint *point2 = ese_point_create(engine);
    ese_point_set_x(point2, 30.0f);
    ese_point_set_y(point2, 40.0f);
    poly_line_add_point(original, point2);
    
    EsePolyLine *copy = poly_line_copy(original);
    TEST_ASSERT_NOT_NULL(copy, "Copy should be created");
    TEST_ASSERT(original != copy, "Copy should be a different pointer");
    
    // Test that values are copied
    TEST_ASSERT_EQUAL(POLY_LINE_CLOSED, poly_line_get_type(copy), "Copied type should match original");
    TEST_ASSERT_FLOAT_EQUAL(2.5f, poly_line_get_stroke_width(copy), 0.001f, "Copied stroke width should match original");
    TEST_ASSERT_EQUAL(2, poly_line_get_point_count(copy), "Copied point count should match original");
    
    // Test that points are deep copied
    EsePoint *copy_point1 = poly_line_get_point(copy, 0);
    EsePoint *copy_point2 = poly_line_get_point(copy, 1);
    TEST_ASSERT_NOT_NULL(copy_point1, "First copied point should exist");
    TEST_ASSERT_NOT_NULL(copy_point2, "Second copied point should exist");
    TEST_ASSERT(point1 != copy_point1, "First copied point should be different from original");
    TEST_ASSERT(point2 != copy_point2, "Second copied point should be different from original");
    TEST_ASSERT_FLOAT_EQUAL(10.0f, ese_point_get_x(copy_point1), 0.001f, "First copied point x should match");
    TEST_ASSERT_FLOAT_EQUAL(20.0f, ese_point_get_y(copy_point1), 0.001f, "First copied point y should match");
    TEST_ASSERT_FLOAT_EQUAL(30.0f, ese_point_get_x(copy_point2), 0.001f, "Second copied point x should match");
    TEST_ASSERT_FLOAT_EQUAL(40.0f, ese_point_get_y(copy_point2), 0.001f, "Second copied point y should match");
    
    // Test that modifications to copy don't affect original
    poly_line_set_type(copy, POLY_LINE_FILLED);
    TEST_ASSERT_EQUAL(POLY_LINE_CLOSED, poly_line_get_type(original), "Original should not be affected by copy modification");
    
    // Clean up
    ese_point_destroy(point1);
    ese_point_destroy(point2);
    poly_line_destroy(copy);
    poly_line_destroy(original);
    lua_engine_destroy(engine);
    
    test_end("PolyLine Copy");
}

// Test poly_line property access
static void test_poly_line_properties() {
    test_begin("PolyLine Properties");
    
    EseLuaEngine *engine = create_test_engine();
    EsePolyLine *poly_line = poly_line_create(engine);
    EseColor *stroke_color = color_create(engine);
    EseColor *fill_color = color_create(engine);
    
    // Test setting and getting type
    poly_line_set_type(poly_line, POLY_LINE_CLOSED);
    TEST_ASSERT_EQUAL(POLY_LINE_CLOSED, poly_line_get_type(poly_line), "Type should be set and retrieved correctly");
    
    poly_line_set_type(poly_line, POLY_LINE_FILLED);
    TEST_ASSERT_EQUAL(POLY_LINE_FILLED, poly_line_get_type(poly_line), "Type should be set and retrieved correctly");
    
    // Test setting and getting stroke width
    poly_line_set_stroke_width(poly_line, 3.5f);
    TEST_ASSERT_FLOAT_EQUAL(3.5f, poly_line_get_stroke_width(poly_line), 0.001f, "Stroke width should be set and retrieved correctly");
    
    // Test setting and getting stroke color
    color_set_r(stroke_color, 1.0f);
    color_set_g(stroke_color, 0.0f);
    color_set_b(stroke_color, 0.0f);
    poly_line_set_stroke_color(poly_line, stroke_color);
    EseColor *retrieved_stroke = poly_line_get_stroke_color(poly_line);
    TEST_ASSERT(stroke_color == retrieved_stroke, "Stroke color should be set and retrieved correctly");
    
    // Test setting and getting fill color
    color_set_r(fill_color, 0.0f);
    color_set_g(fill_color, 1.0f);
    color_set_b(fill_color, 0.0f);
    poly_line_set_fill_color(poly_line, fill_color);
    EseColor *retrieved_fill = poly_line_get_fill_color(poly_line);
    TEST_ASSERT(fill_color == retrieved_fill, "Fill color should be set and retrieved correctly");
    
    // Test setting NULL colors
    poly_line_set_stroke_color(poly_line, NULL);
    TEST_ASSERT_NULL(poly_line_get_stroke_color(poly_line), "Stroke color should be NULL after setting NULL");
    
    poly_line_set_fill_color(poly_line, NULL);
    TEST_ASSERT_NULL(poly_line_get_fill_color(poly_line), "Fill color should be NULL after setting NULL");
    
    // Clean up
    color_destroy(stroke_color);
    color_destroy(fill_color);
    poly_line_destroy(poly_line);
    lua_engine_destroy(engine);
    
    test_end("PolyLine Properties");
}

// Test points management
static void test_poly_line_points_management() {
    test_begin("PolyLine Points Management");
    
    EseLuaEngine *engine = create_test_engine();
    EsePolyLine *poly_line = poly_line_create(engine);
    
    // Test initial state
    TEST_ASSERT_EQUAL(0, poly_line_get_point_count(poly_line), "Initial point count should be 0");
    TEST_ASSERT_NULL(poly_line_get_point(poly_line, 0), "Getting point at index 0 should return NULL");
    
    // Test adding points
    EsePoint *point1 = ese_point_create(engine);
    ese_point_set_x(point1, 10.0f);
    ese_point_set_y(point1, 20.0f);
    bool success = poly_line_add_point(poly_line, point1);
    TEST_ASSERT(success, "Should successfully add first point");
    TEST_ASSERT_EQUAL(1, poly_line_get_point_count(poly_line), "Point count should be 1 after adding first point");
    
    EsePoint *point2 = ese_point_create(engine);
    ese_point_set_x(point2, 30.0f);
    ese_point_set_y(point2, 40.0f);
    success = poly_line_add_point(poly_line, point2);
    TEST_ASSERT(success, "Should successfully add second point");
    TEST_ASSERT_EQUAL(2, poly_line_get_point_count(poly_line), "Point count should be 2 after adding second point");
    
    // Test getting points
    EsePoint *retrieved_point1 = poly_line_get_point(poly_line, 0);
    EsePoint *retrieved_point2 = poly_line_get_point(poly_line, 1);
    TEST_ASSERT_NOT_NULL(retrieved_point1, "First point should be retrievable");
    TEST_ASSERT_NOT_NULL(retrieved_point2, "Second point should be retrievable");
    TEST_ASSERT(point1 != retrieved_point1, "Retrieved point should be a copy, not the original");
    TEST_ASSERT(point2 != retrieved_point2, "Retrieved point should be a copy, not the original");
    TEST_ASSERT_FLOAT_EQUAL(10.0f, ese_point_get_x(retrieved_point1), 0.001f, "First point x should match");
    TEST_ASSERT_FLOAT_EQUAL(20.0f, ese_point_get_y(retrieved_point1), 0.001f, "First point y should match");
    TEST_ASSERT_FLOAT_EQUAL(30.0f, ese_point_get_x(retrieved_point2), 0.001f, "Second point x should match");
    TEST_ASSERT_FLOAT_EQUAL(40.0f, ese_point_get_y(retrieved_point2), 0.001f, "Second point y should match");
    
    // Test getting invalid index
    EsePoint *invalid_point = poly_line_get_point(poly_line, 5);
    TEST_ASSERT_NULL(invalid_point, "Getting point at invalid index should return NULL");
    
    // Test removing points
    success = poly_line_remove_point(poly_line, 0);
    TEST_ASSERT(success, "Should successfully remove first point");
    TEST_ASSERT_EQUAL(1, poly_line_get_point_count(poly_line), "Point count should be 1 after removing first point");
    
    // Verify remaining point is now at index 0
    EsePoint *remaining_point = poly_line_get_point(poly_line, 0);
    TEST_ASSERT_NOT_NULL(remaining_point, "Remaining point should be retrievable");
    TEST_ASSERT_FLOAT_EQUAL(30.0f, ese_point_get_x(remaining_point), 0.001f, "Remaining point x should match second point");
    TEST_ASSERT_FLOAT_EQUAL(40.0f, ese_point_get_y(remaining_point), 0.001f, "Remaining point y should match second point");
    
    // Test removing invalid index
    success = poly_line_remove_point(poly_line, 5);
    TEST_ASSERT(!success, "Should fail to remove point at invalid index");
    
    // Test clearing points
    poly_line_clear_points(poly_line);
    TEST_ASSERT_EQUAL(0, poly_line_get_point_count(poly_line), "Point count should be 0 after clearing");
    TEST_ASSERT_NULL(poly_line_get_point(poly_line, 0), "Getting point after clearing should return NULL");
    
    // Clean up
    ese_point_destroy(point1);
    ese_point_destroy(point2);
    poly_line_destroy(poly_line);
    lua_engine_destroy(engine);
    
    test_end("PolyLine Points Management");
}

// Test watcher system
static void test_poly_line_watchers() {
    test_begin("PolyLine Watchers");
    
    EseLuaEngine *engine = create_test_engine();
    EsePolyLine *poly_line = poly_line_create(engine);
    
    // Test adding watcher
    bool success = poly_line_add_watcher(poly_line, mock_watcher_callback, (void*)0x1234);
    TEST_ASSERT(success, "Should successfully add watcher");
    
    // Test that watcher is called on property change
    mock_reset();
    poly_line_set_type(poly_line, POLY_LINE_CLOSED);
    TEST_ASSERT(mock_watcher_called, "Watcher should be called on type change");
    TEST_ASSERT(mock_watcher_poly_line == poly_line, "Watcher should receive correct poly_line pointer");
    TEST_ASSERT(mock_watcher_userdata == (void*)0x1234, "Watcher should receive correct userdata");
    
    // Test that watcher is called on other property changes
    mock_reset();
    poly_line_set_stroke_width(poly_line, 2.0f);
    TEST_ASSERT(mock_watcher_called, "Watcher should be called on stroke width change");
    
    mock_reset();
    EseColor *color = color_create(engine);
    poly_line_set_stroke_color(poly_line, color);
    TEST_ASSERT(mock_watcher_called, "Watcher should be called on stroke color change");
    
    mock_reset();
    poly_line_set_fill_color(poly_line, color);
    TEST_ASSERT(mock_watcher_called, "Watcher should be called on fill color change");
    
    // Test that watcher is called on points changes
    mock_reset();
    EsePoint *point = ese_point_create(engine);
    poly_line_add_point(poly_line, point);
    TEST_ASSERT(mock_watcher_called, "Watcher should be called on point addition");
    
    mock_reset();
    poly_line_clear_points(poly_line);
    TEST_ASSERT(mock_watcher_called, "Watcher should be called on points clearing");
    
    // Test removing watcher
    success = poly_line_remove_watcher(poly_line, mock_watcher_callback, (void*)0x1234);
    TEST_ASSERT(success, "Should successfully remove watcher");
    
    // Test that watcher is not called after removal
    mock_reset();
    poly_line_set_type(poly_line, POLY_LINE_FILLED);
    TEST_ASSERT(!mock_watcher_called, "Watcher should not be called after removal");
    
    // Test removing non-existent watcher
    success = poly_line_remove_watcher(poly_line, mock_watcher_callback, (void*)0x1234);
    TEST_ASSERT(!success, "Should fail to remove non-existent watcher");
    
    // Clean up
    color_destroy(color);
    ese_point_destroy(point);
    poly_line_destroy(poly_line);
    lua_engine_destroy(engine);
    
    test_end("PolyLine Watchers");
}

// Test Lua integration
static void test_poly_line_lua_integration() {
    test_begin("PolyLine Lua Integration");
    
    EseLuaEngine *engine = create_test_engine();
    EsePolyLine *poly_line = poly_line_create(engine);
    
    // Test getting Lua reference
    int lua_ref = poly_line_get_lua_ref(poly_line);
    TEST_ASSERT(lua_ref == LUA_NOREF, "PolyLine should have no Lua reference initially");
    
    // Test referencing
    poly_line_ref(poly_line);
    lua_ref = poly_line_get_lua_ref(poly_line);
    TEST_ASSERT(lua_ref != LUA_NOREF, "PolyLine should have a valid Lua reference after ref");
    TEST_ASSERT_EQUAL(1, poly_line_get_lua_ref_count(poly_line), "PolyLine should have ref count of 1");
    
    // Test unreferencing
    poly_line_unref(poly_line);
    TEST_ASSERT_EQUAL(0, poly_line_get_lua_ref_count(poly_line), "PolyLine should have ref count of 0 after unref");
    
    // Test Lua state
    lua_State *state = poly_line_get_state(poly_line);
    TEST_ASSERT_NOT_NULL(state, "PolyLine should have a valid Lua state");
    TEST_ASSERT(state == engine->runtime, "PolyLine state should match engine runtime");
    
    // Clean up
    poly_line_destroy(poly_line);
    lua_engine_destroy(engine);
    
    test_end("PolyLine Lua Integration");
}

// Test NULL pointer aborts
static void test_poly_line_null_pointer_aborts() {
    test_begin("PolyLine NULL Pointer Abort Tests");
    
    EseLuaEngine *engine = create_test_engine();
    EsePolyLine *poly_line = poly_line_create(engine);
    EsePoint *point = ese_point_create(engine);
    EseColor *color = color_create(engine);
    
    // Test that creation functions abort with NULL pointers
    TEST_ASSERT_ABORT(poly_line_create(NULL), "poly_line_create should abort with NULL engine");
    TEST_ASSERT_ABORT(poly_line_copy(NULL), "poly_line_copy should abort with NULL poly_line");
    // poly_line_destroy should ignore NULL poly_line (not abort)
    poly_line_destroy(NULL);
    
    // Test that property functions abort with NULL pointers
    TEST_ASSERT_ABORT(poly_line_set_type(NULL, POLY_LINE_CLOSED), "poly_line_set_type should abort with NULL poly_line");
    TEST_ASSERT_ABORT(poly_line_get_type(NULL), "poly_line_get_type should abort with NULL poly_line");
    TEST_ASSERT_ABORT(poly_line_set_stroke_width(NULL, 2.0f), "poly_line_set_stroke_width should abort with NULL poly_line");
    TEST_ASSERT_ABORT(poly_line_get_stroke_width(NULL), "poly_line_get_stroke_width should abort with NULL poly_line");
    TEST_ASSERT_ABORT(poly_line_set_stroke_color(NULL, color), "poly_line_set_stroke_color should abort with NULL poly_line");
    TEST_ASSERT_ABORT(poly_line_get_stroke_color(NULL), "poly_line_get_stroke_color should abort with NULL poly_line");
    TEST_ASSERT_ABORT(poly_line_set_fill_color(NULL, color), "poly_line_set_fill_color should abort with NULL poly_line");
    TEST_ASSERT_ABORT(poly_line_get_fill_color(NULL), "poly_line_get_fill_color should abort with NULL poly_line");
    
    // Test that points functions abort with NULL pointers
    TEST_ASSERT_ABORT(poly_line_add_point(NULL, point), "poly_line_add_point should abort with NULL poly_line");
    TEST_ASSERT_ABORT(poly_line_add_point(poly_line, NULL), "poly_line_add_point should abort with NULL point");
    TEST_ASSERT_ABORT(poly_line_remove_point(NULL, 0), "poly_line_remove_point should abort with NULL poly_line");
    TEST_ASSERT_ABORT(poly_line_get_point(NULL, 0), "poly_line_get_point should abort with NULL poly_line");
    TEST_ASSERT_ABORT(poly_line_get_point_count(NULL), "poly_line_get_point_count should abort with NULL poly_line");
    TEST_ASSERT_ABORT(poly_line_clear_points(NULL), "poly_line_clear_points should abort with NULL poly_line");
    
    // Test that watcher functions abort with NULL pointers
    TEST_ASSERT_ABORT(poly_line_add_watcher(NULL, mock_watcher_callback, NULL), "poly_line_add_watcher should abort with NULL poly_line");
    TEST_ASSERT_ABORT(poly_line_add_watcher(poly_line, NULL, NULL), "poly_line_add_watcher should abort with NULL callback");
    TEST_ASSERT_ABORT(poly_line_remove_watcher(NULL, mock_watcher_callback, NULL), "poly_line_remove_watcher should abort with NULL poly_line");
    TEST_ASSERT_ABORT(poly_line_remove_watcher(poly_line, NULL, NULL), "poly_line_remove_watcher should abort with NULL callback");
    
    // Test that Lua functions abort with NULL pointers
    TEST_ASSERT_ABORT(poly_line_lua_init(NULL), "poly_line_lua_init should abort with NULL engine");
    TEST_ASSERT_ABORT(poly_line_lua_push(NULL), "poly_line_lua_push should abort with NULL poly_line");
    TEST_ASSERT_ABORT(poly_line_lua_get(NULL, 1), "poly_line_lua_get should abort with NULL Lua state");
    TEST_ASSERT_ABORT(poly_line_ref(NULL), "poly_line_ref should abort with NULL poly_line");
    // poly_line_unref should ignore NULL poly_line (not abort)
    poly_line_unref(NULL);
    TEST_ASSERT_ABORT(poly_line_get_state(NULL), "poly_line_get_state should abort with NULL poly_line");
    TEST_ASSERT_ABORT(poly_line_get_lua_ref(NULL), "poly_line_get_lua_ref should abort with NULL poly_line");
    TEST_ASSERT_ABORT(poly_line_get_lua_ref_count(NULL), "poly_line_get_lua_ref_count should abort with NULL poly_line");
    
    // Clean up
    color_destroy(color);
    ese_point_destroy(point);
    poly_line_destroy(poly_line);
    lua_engine_destroy(engine);
    
    test_end("PolyLine NULL Pointer Abort Tests");
}
