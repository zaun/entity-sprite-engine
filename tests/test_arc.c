/*
 * Test file for arc functionality
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
#include "test_utils.h"
#include "../src/types/arc.h"
#include "../src/types/rect.h"
#include "../src/scripting/lua_engine.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

// Test function declarations
static void test_arc_creation();
static void test_arc_properties();
static void test_arc_copy();
static void test_arc_mathematical_operations();
static void test_arc_intersection_tests();
static void test_arc_lua_integration();
static void test_arc_null_pointer_aborts();

// Helper function to create and initialize engine
static EseLuaEngine* create_test_engine() {
    EseLuaEngine *engine = lua_engine_create();
    if (engine) {
        // Set up registry keys that arc system needs
        lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);
        
        // Initialize arc system
        arc_lua_init(engine);
    }
    return engine;
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

    test_suite_begin("Arc Tests");

    // Initialize required systems
    log_init();

    // Run all test suites
    test_arc_creation();
    test_arc_properties();
    test_arc_copy();
    test_arc_mathematical_operations();
    test_arc_intersection_tests();
    test_arc_lua_integration();
    test_arc_null_pointer_aborts();

    test_suite_end("Arc Tests");

    return 0;
}

static void test_arc_creation() {
    test_begin("Arc Creation");
    
    EseLuaEngine *engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");
    
    EseArc *arc = arc_create(engine);
    TEST_ASSERT_NOT_NULL(arc, "Arc should be created");
    TEST_ASSERT_EQUAL(0.0f, arc->x, "Default x should be 0.0");
    TEST_ASSERT_EQUAL(0.0f, arc->y, "Default y should be 0.0");
    TEST_ASSERT_EQUAL(1.0f, arc->radius, "Default radius should be 1.0");
    TEST_ASSERT_EQUAL(0.0f, arc->start_angle, "Default start_angle should be 0.0");
    TEST_ASSERT_FLOAT_EQUAL(2.0f * M_PI, arc->end_angle, 0.001f, "Default end_angle should be 2π");
    TEST_ASSERT_POINTER_EQUAL(engine->runtime, arc->state, "Arc should have correct Lua state");
    TEST_ASSERT_EQUAL(0, arc->lua_ref_count, "New arc should have ref count 0");
    TEST_ASSERT(arc->lua_ref == LUA_NOREF, "New arc should have LUA_NOREF value");
    
    // Clean up
    arc_destroy(arc);
    lua_engine_destroy(engine);
    
    test_end("Arc Creation");
}

static void test_arc_properties() {
    test_begin("Arc Properties");
    
    EseLuaEngine *engine = create_test_engine();
    EseArc *arc = arc_create(engine);
    
    // Test setting and getting properties
    arc->x = 10.5f;
    arc->y = -5.25f;
    arc->radius = 2.0f;
    arc->start_angle = 0.785398f; // π/4 radians (45 degrees)
    arc->end_angle = 2.35619f;    // 3π/4 radians (135 degrees)
    
    TEST_ASSERT_EQUAL(10.5f, arc->x, "X should be set and retrieved correctly");
    TEST_ASSERT_EQUAL(-5.25f, arc->y, "Y should be set and retrieved correctly");
    TEST_ASSERT_EQUAL(2.0f, arc->radius, "Radius should be set and retrieved correctly");
    TEST_ASSERT_FLOAT_EQUAL(0.785398f, arc->start_angle, 0.001f, "Start angle should be set and retrieved correctly");
    TEST_ASSERT_FLOAT_EQUAL(2.35619f, arc->end_angle, 0.001f, "End angle should be set and retrieved correctly");
    
    // Test negative values
    arc->x = -100.0f;
    arc->y = 200.0f;
    arc->radius = 5.0f;
    arc->start_angle = -1.5708f; // -π/2 radians (-90 degrees)
    arc->end_angle = 1.5708f;    // π/2 radians (90 degrees)
    
    TEST_ASSERT_EQUAL(-100.0f, arc->x, "X should handle negative values");
    TEST_ASSERT_EQUAL(200.0f, arc->y, "Y should handle negative values");
    TEST_ASSERT_EQUAL(5.0f, arc->radius, "Radius should handle positive values");
    TEST_ASSERT_FLOAT_EQUAL(-1.5708f, arc->start_angle, 0.001f, "Start angle should handle negative values");
    TEST_ASSERT_FLOAT_EQUAL(1.5708f, arc->end_angle, 0.001f, "End angle should handle positive values");
    
    // Clean up
    arc_destroy(arc);
    lua_engine_destroy(engine);
    
    test_end("Arc Properties");
}

static void test_arc_copy() {
    test_begin("Arc Copy");
    
    EseLuaEngine *engine = create_test_engine();
    EseArc *original = arc_create(engine);
    
    // Set some values
    original->x = 42.0f;
    original->y = -17.5f;
    original->radius = 3.0f;
    original->start_angle = 0.523599f; // π/6 radians (30 degrees)
    original->end_angle = 1.0472f;     // π/3 radians (60 degrees)
    
    EseArc *copy = arc_copy(original);
    TEST_ASSERT_NOT_NULL(copy, "Copy should be created");
    TEST_ASSERT(original != copy, "Copy should be a different pointer");
    
    // Test that values are copied
    TEST_ASSERT_EQUAL(42.0f, copy->x, "Copied x should match original");
    TEST_ASSERT_EQUAL(-17.5f, copy->y, "Copied y should match original");
    TEST_ASSERT_EQUAL(3.0f, copy->radius, "Copied radius should match original");
    TEST_ASSERT_FLOAT_EQUAL(0.523599f, copy->start_angle, 0.001f, "Copied start_angle should match original");
    TEST_ASSERT_FLOAT_EQUAL(1.0472f, copy->end_angle, 0.001f, "Copied end_angle should match original");
    
    // Test that modifications to copy don't affect original
    copy->x = 100.0f;
    TEST_ASSERT_EQUAL(42.0f, original->x, "Original should not be affected by copy modification");
    
    // Clean up
    arc_destroy(copy);
    arc_destroy(original);
    lua_engine_destroy(engine);
    
    test_end("Arc Copy");
}

static void test_arc_mathematical_operations() {
    test_begin("Arc Mathematical Operations");
    
    EseLuaEngine *engine = create_test_engine();
    EseArc *arc = arc_create(engine);
    
    // Test point at angle
    arc->x = 0.0f;
    arc->y = 0.0f;
    arc->radius = 2.0f;
    arc->start_angle = 0.0f;
    arc->end_angle = 2.0f * M_PI;
    
    float point_x, point_y;
    arc_get_point_at_angle(arc, M_PI / 2.0f, &point_x, &point_y); // 90 degrees
    TEST_ASSERT_FLOAT_EQUAL(0.0f, point_x, 0.001f, "Point at 90° should have x = 0.0");
    TEST_ASSERT_FLOAT_EQUAL(2.0f, point_y, 0.001f, "Point at 90° should have y = 2.0");
    
    arc_get_point_at_angle(arc, 0.0f, &point_x, &point_y); // 0 degrees
    TEST_ASSERT_FLOAT_EQUAL(2.0f, point_x, 0.001f, "Point at 0° should have x = 2.0");
    TEST_ASSERT_FLOAT_EQUAL(0.0f, point_y, 0.001f, "Point at 0° should have y = 0.0");
    
    arc_get_point_at_angle(arc, M_PI, &point_x, &point_y); // 180 degrees
    TEST_ASSERT_FLOAT_EQUAL(-2.0f, point_x, 0.001f, "Point at 180° should have x = -2.0");
    TEST_ASSERT_FLOAT_EQUAL(0.0f, point_y, 0.001f, "Point at 180° should have y = 0.0");
    
    // Test arc length
    float arc_length = arc_get_length(arc);
    TEST_ASSERT_FLOAT_EQUAL(2.0f * M_PI * 2.0f, arc_length, 0.001f, "Full circle arc length should be 2πr");
    
    // Clean up
    arc_destroy(arc);
    lua_engine_destroy(engine);
    
    test_end("Arc Mathematical Operations");
}

static void test_arc_intersection_tests() {
    test_begin("Arc Intersection");
    
    EseLuaEngine *engine = create_test_engine();
    EseArc *arc = arc_create(engine);
    EseRect *rect = ese_rect_create(engine);
    
    // Set up arc at (0,0) with radius 2, full circle
    arc->x = 0.0f;
    arc->y = 0.0f;
    arc->radius = 2.0f;
    arc->start_angle = 0.0f;
    arc->end_angle = 2.0f * M_PI;
    
    // Set up rectangle that intersects with arc
    ese_rect_set_x(rect, 1.0f);
    ese_rect_set_y(rect, 1.0f);
    ese_rect_set_width(rect, 2.0f);
    ese_rect_set_height(rect, 2.0f);
    
    // Test intersection
    bool intersects = arc_intersects_rect(arc, rect);
    TEST_ASSERT(intersects, "Arc should intersect with rectangle");
    
    // Test arc that doesn't intersect
    arc->x = 10.0f; // Move arc away from rectangle
    intersects = arc_intersects_rect(arc, rect);
    TEST_ASSERT(!intersects, "Arc should not intersect with rectangle when far away");
    
    // Clean up
    arc_destroy(arc);
    ese_rect_destroy(rect);
    lua_engine_destroy(engine);
    
    test_end("Arc Intersection");
}

static void test_arc_lua_integration() {
    test_begin("Arc Lua Integration");
    
    EseLuaEngine *engine = create_test_engine();
    EseArc *arc = arc_create(engine);
    
    // Test initial Lua reference state
    TEST_ASSERT(arc->lua_ref == LUA_NOREF, "Arc should have no Lua reference initially");
    TEST_ASSERT_EQUAL(0, arc->lua_ref_count, "Arc should have ref count of 0 initially");
    
    // Test referencing
    arc_ref(arc);
    TEST_ASSERT(arc->lua_ref != LUA_NOREF, "Arc should have a valid Lua reference after ref");
    TEST_ASSERT_EQUAL(1, arc->lua_ref_count, "Arc should have ref count of 1");
    
    // Test unreferencing
    arc_unref(arc);
    TEST_ASSERT_EQUAL(0, arc->lua_ref_count, "Arc should have ref count of 0 after unref");
    
    // Test Lua state
    TEST_ASSERT_NOT_NULL(arc->state, "Arc should have a valid Lua state");
    TEST_ASSERT(arc->state == engine->runtime, "Arc state should match engine runtime");
    
    // Clean up
    arc_destroy(arc);
    lua_engine_destroy(engine);
    
    test_end("Arc Lua Integration");
}

static void test_arc_null_pointer_aborts() {
    test_begin("Arc NULL Pointer Abort Tests");
    
    EseLuaEngine *engine = create_test_engine();
    EseArc *arc = arc_create(engine);
    EseRect *rect = ese_rect_create(engine);
    
    // Test that creation functions abort with NULL pointers
    TEST_ASSERT_ABORT(arc_create(NULL), "arc_create should abort with NULL engine");
    TEST_ASSERT_ABORT(arc_copy(NULL), "arc_copy should abort with NULL arc");
    // arc_destroy should ignore NULL arc (not abort)
    arc_destroy(NULL);
    
    // Test that mathematical functions abort with NULL pointers
    float x, y;
    TEST_ASSERT_ABORT(arc_get_point_at_angle(NULL, 1.0f, &x, &y), "arc_get_point_at_angle should abort with NULL arc");
    TEST_ASSERT_ABORT(arc_get_point_at_angle(arc, 1.0f, NULL, &y), "arc_get_point_at_angle should abort with NULL x pointer");
    TEST_ASSERT_ABORT(arc_get_point_at_angle(arc, 1.0f, &x, NULL), "arc_get_point_at_angle should abort with NULL y pointer");
    TEST_ASSERT_ABORT(arc_get_length(NULL), "arc_get_length should abort with NULL arc");
    TEST_ASSERT_ABORT(arc_intersects_rect(NULL, rect), "arc_intersects_rect should abort with NULL arc");
    TEST_ASSERT_ABORT(arc_intersects_rect(arc, NULL), "arc_intersects_rect should abort with NULL rect");
    
    // Test that Lua functions abort with NULL pointers
    TEST_ASSERT_ABORT(arc_lua_init(NULL), "arc_lua_init should abort with NULL engine");
    TEST_ASSERT_ABORT(arc_lua_push(NULL), "arc_lua_push should abort with NULL arc");
    TEST_ASSERT_ABORT(arc_lua_get(NULL, 1), "arc_lua_get should abort with NULL Lua state");
    TEST_ASSERT_ABORT(arc_ref(NULL), "arc_ref should abort with NULL arc");
    // arc_unref should ignore NULL arc (not abort)
    arc_unref(NULL);
    
    // Clean up
    ese_rect_destroy(rect);
    arc_destroy(arc);
    lua_engine_destroy(engine);
    
    test_end("Arc NULL Pointer Abort Tests");
}
