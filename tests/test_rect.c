/*
* test_ese_rect.c - Unity-based tests for rect functionality
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
#include "../src/types/rect.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

/**
* C API Test Functions Declarations
*/
static void test_ese_rect_sizeof(void);
static void test_ese_rect_create_requires_engine(void);
static void test_ese_rect_create(void);
static void test_ese_rect_x(void);
static void test_ese_rect_y(void);
static void test_ese_rect_width(void);
static void test_ese_rect_height(void);
static void test_ese_rect_rotation(void);
static void test_ese_rect_ref(void);
static void test_ese_rect_copy_requires_engine(void);
static void test_ese_rect_copy(void);
static void test_ese_rect_area(void);
static void test_ese_rect_contains_point(void);
static void test_ese_rect_contains_point_rotated(void);
static void test_ese_rect_intersects(void);
static void test_ese_rect_intersects_rotated(void);
static void test_ese_rect_watcher_system(void);
static void test_ese_rect_lua_integration(void);
static void test_ese_rect_lua_init(void);
static void test_ese_rect_lua_push(void);
static void test_ese_rect_lua_get(void);

/**
* Lua API Test Functions Declarations
*/
static void test_ese_rect_lua_new(void);
static void test_ese_rect_lua_zero(void);
static void test_ese_rect_lua_area(void);
static void test_ese_rect_lua_contains_point(void);
static void test_ese_rect_lua_intersects(void);
static void test_ese_rect_lua_x(void);
static void test_ese_rect_lua_y(void);
static void test_ese_rect_lua_width(void);
static void test_ese_rect_lua_height(void);
static void test_ese_rect_lua_rotation(void);
static void test_ese_rect_lua_tostring(void);
static void test_ese_rect_lua_gc(void);

/**
* Mock watcher callback for testing
*/
static bool watcher_called = false;
static EseRect *last_watched_rect = NULL;
static void *last_watcher_userdata = NULL;

static void test_watcher_callback(EseRect *rect, void *userdata) {
    watcher_called = true;
    last_watched_rect = rect;
    last_watcher_userdata = userdata;
}

static void mock_reset(void) {
    watcher_called = false;
    last_watched_rect = NULL;
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

    printf("\nEseRect Tests\n");
    printf("--------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_ese_rect_sizeof);
    RUN_TEST(test_ese_rect_create_requires_engine);
    RUN_TEST(test_ese_rect_create);
    RUN_TEST(test_ese_rect_x);
    RUN_TEST(test_ese_rect_y);
    RUN_TEST(test_ese_rect_width);
    RUN_TEST(test_ese_rect_height);
    RUN_TEST(test_ese_rect_rotation);
    RUN_TEST(test_ese_rect_ref);
    RUN_TEST(test_ese_rect_copy_requires_engine);
    RUN_TEST(test_ese_rect_copy);
    RUN_TEST(test_ese_rect_area);
    RUN_TEST(test_ese_rect_contains_point);
    RUN_TEST(test_ese_rect_contains_point_rotated);
    RUN_TEST(test_ese_rect_intersects);
    RUN_TEST(test_ese_rect_intersects_rotated);
    RUN_TEST(test_ese_rect_watcher_system);
    RUN_TEST(test_ese_rect_lua_integration);
    RUN_TEST(test_ese_rect_lua_init);
    RUN_TEST(test_ese_rect_lua_push);
    RUN_TEST(test_ese_rect_lua_get);

    RUN_TEST(test_ese_rect_lua_new);
    RUN_TEST(test_ese_rect_lua_zero);
    RUN_TEST(test_ese_rect_lua_area);
    RUN_TEST(test_ese_rect_lua_contains_point);
    RUN_TEST(test_ese_rect_lua_intersects);
    RUN_TEST(test_ese_rect_lua_x);
    RUN_TEST(test_ese_rect_lua_y);
    RUN_TEST(test_ese_rect_lua_width);
    RUN_TEST(test_ese_rect_lua_height);
    RUN_TEST(test_ese_rect_lua_rotation);
    RUN_TEST(test_ese_rect_lua_tostring);
    RUN_TEST(test_ese_rect_lua_gc);

    memory_manager.destroy();

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_ese_rect_sizeof(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, ese_rect_sizeof(), "Point size should be > 0");
}

static void test_ese_rect_create_requires_engine(void) {
    ASSERT_DEATH(ese_rect_create(NULL), "ese_rect_create should abort with NULL engine");
}

static void test_ese_rect_create(void) {
    EseRect *rect = ese_rect_create(g_engine);

    TEST_ASSERT_NOT_NULL_MESSAGE(rect, "Rect should be created");
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_rect_get_x(rect));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_rect_get_y(rect));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_rect_get_width(rect));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_rect_get_height(rect));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_rect_get_rotation(rect));
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_rect_get_state(rect), "Rect should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_rect_get_lua_ref_count(rect), "New rect should have ref count 0");

    ese_rect_destroy(rect);
}

static void test_ese_rect_x(void) {
    EseRect *rect = ese_rect_create(g_engine);

    ese_rect_set_x(rect, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_rect_get_x(rect));

    ese_rect_set_x(rect, -10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, ese_rect_get_x(rect));

    ese_rect_set_x(rect, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_rect_get_x(rect));

    ese_rect_destroy(rect);
}

static void test_ese_rect_y(void) {
    EseRect *rect = ese_rect_create(g_engine);

    ese_rect_set_y(rect, 20.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, ese_rect_get_y(rect));

    ese_rect_set_y(rect, -10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, ese_rect_get_y(rect));

    ese_rect_set_y(rect, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_rect_get_y(rect));

    ese_rect_destroy(rect);
}

static void test_ese_rect_width(void) {
    EseRect *rect = ese_rect_create(g_engine);

    ese_rect_set_width(rect, 100.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, ese_rect_get_width(rect));

    ese_rect_set_width(rect, -50.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -50.0f, ese_rect_get_width(rect));

    ese_rect_set_width(rect, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_rect_get_width(rect));

    ese_rect_destroy(rect);
}

static void test_ese_rect_height(void) {
    EseRect *rect = ese_rect_create(g_engine);

    ese_rect_set_height(rect, 75.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 75.0f, ese_rect_get_height(rect));

    ese_rect_set_height(rect, -25.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -25.0f, ese_rect_get_height(rect));

    ese_rect_set_height(rect, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_rect_get_height(rect));

    ese_rect_destroy(rect);
}

static void test_ese_rect_rotation(void) {
    EseRect *rect = ese_rect_create(g_engine);

    ese_rect_set_rotation(rect, M_PI / 4.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, M_PI / 4.0f, ese_rect_get_rotation(rect));

    ese_rect_set_rotation(rect, -M_PI / 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -M_PI / 2.0f, ese_rect_get_rotation(rect));

    ese_rect_set_rotation(rect, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_rect_get_rotation(rect));

    ese_rect_destroy(rect);
}

static void test_ese_rect_ref(void) {
    EseRect *rect = ese_rect_create(g_engine);

    ese_rect_ref(rect);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, ese_rect_get_lua_ref_count(rect), "Ref count should be 1");

    ese_rect_unref(rect);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_rect_get_lua_ref_count(rect), "Ref count should be 0");

    ese_rect_destroy(rect);
}

static void test_ese_rect_copy_requires_engine(void) {
    ASSERT_DEATH(ese_rect_copy(NULL), "ese_rect_copy should abort with NULL rect");
}

static void test_ese_rect_copy(void) {
    EseRect *rect = ese_rect_create(g_engine);
    ese_rect_ref(rect);
    ese_rect_set_x(rect, 10.0f);
    ese_rect_set_y(rect, 20.0f);
    ese_rect_set_width(rect, 100.0f);
    ese_rect_set_height(rect, 75.0f);
    ese_rect_set_rotation(rect, M_PI / 4.0f);
    EseRect *copy = ese_rect_copy(rect);

    TEST_ASSERT_NOT_NULL_MESSAGE(copy, "Copy should be created");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_rect_get_state(copy), "Copy should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_rect_get_lua_ref_count(copy), "Copy should have ref count 0");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_rect_get_x(copy));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, ese_rect_get_y(copy));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, ese_rect_get_width(copy));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 75.0f, ese_rect_get_height(copy));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, M_PI / 4.0f, ese_rect_get_rotation(copy));

    ese_rect_unref(rect);
    ese_rect_destroy(rect);
    ese_rect_destroy(copy);
}

static void test_ese_rect_area(void) {
    EseRect *rect = ese_rect_create(g_engine);

    ese_rect_set_width(rect, 10.0f);
    ese_rect_set_height(rect, 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, ese_rect_area(rect));

    ese_rect_set_width(rect, -10.0f);
    ese_rect_set_height(rect, -5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, ese_rect_area(rect));

    ese_rect_set_width(rect, 0.0f);
    ese_rect_set_height(rect, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_rect_area(rect));

    ese_rect_destroy(rect);
}

static void test_ese_rect_contains_point(void) {
    EseRect *rect = ese_rect_create(g_engine);

    ese_rect_set_x(rect, 0.0f);
    ese_rect_set_y(rect, 0.0f);
    ese_rect_set_width(rect, 10.0f);
    ese_rect_set_height(rect, 10.0f);
    ese_rect_set_rotation(rect, 0.0f);

    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 5.0f, 5.0f), "Point (5,5) should be inside rect");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 0.0f, 0.0f), "Point (0,0) on edge should be inside rect");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 10.0f, 10.0f), "Point (10,10) on edge should be inside rect");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, 15.0f, 15.0f), "Point (15,15) should not be inside rect");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, -1.0f, 5.0f), "Point (-1,5) should not be inside rect");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, 5.0f, -1.0f), "Point (5,-1) should not be inside rect");

    ese_rect_destroy(rect);
}

static void test_ese_rect_intersects(void) {
    EseRect *rect1 = ese_rect_create(g_engine);
    EseRect *rect2 = ese_rect_create(g_engine);
    EseRect *rect3 = ese_rect_create(g_engine);

    ese_rect_set_x(rect1, 0.0f);
    ese_rect_set_y(rect1, 0.0f);
    ese_rect_set_width(rect1, 10.0f);
    ese_rect_set_height(rect1, 10.0f);
    ese_rect_set_rotation(rect1, 0.0f);

    ese_rect_set_x(rect2, 5.0f);
    ese_rect_set_y(rect2, 5.0f);
    ese_rect_set_width(rect2, 10.0f);
    ese_rect_set_height(rect2, 10.0f);
    ese_rect_set_rotation(rect2, 0.0f);

    ese_rect_set_x(rect3, 20.0f);
    ese_rect_set_y(rect3, 20.0f);
    ese_rect_set_width(rect3, 5.0f);
    ese_rect_set_height(rect3, 5.0f);
    ese_rect_set_rotation(rect3, 0.0f);

    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect1, rect2), "Overlapping rectangles should intersect");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect2, rect1), "Intersection should be commutative");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect1, rect3), "Non-overlapping rectangles should not intersect");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect3, rect1), "Non-intersection should be commutative");

    ese_rect_destroy(rect1);
    ese_rect_destroy(rect2);
    ese_rect_destroy(rect3);
}

static void test_ese_rect_contains_point_rotated(void) {
    EseRect *rect = ese_rect_create(g_engine);
    
    // Test with rect at (0,0) and size 20x20
    ese_rect_set_x(rect, 0.0f);
    ese_rect_set_y(rect, 0.0f);
    ese_rect_set_width(rect, 20.0f);
    ese_rect_set_height(rect, 20.0f);
    
    // Test 45 degree rotation (π/4 radians)
    ese_rect_set_rotation(rect, M_PI / 4.0f);
    
    // For a 20x20 square at (0,0) rotated 45 degrees, the center is at (10,10)
    // The rotated square should contain points near the center
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 10.0f, 10.0f), "Center point (10,10) should be inside 45° rotated rect");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 15.0f, 10.0f), "Point (15,10) should be inside 45° rotated rect");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 10.0f, 15.0f), "Point (10,15) should be inside 45° rotated rect");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, 25.0f, 10.0f), "Point (25,10) should not be inside 45° rotated rect");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, 10.0f, 25.0f), "Point (10,25) should not be inside 45° rotated rect");
    
    // Test 135 degree rotation (3π/4 radians)
    ese_rect_set_rotation(rect, 3.0f * M_PI / 4.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 10.0f, 10.0f), "Center point (10,10) should be inside 135° rotated rect");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 5.0f, 10.0f), "Point (5,10) should be inside 135° rotated rect");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 10.0f, 15.0f), "Point (10,15) should be inside 135° rotated rect");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, -5.0f, 10.0f), "Point (-5,10) should not be inside 135° rotated rect");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, 10.0f, 25.0f), "Point (10,25) should not be inside 135° rotated rect");
    
    // Test 225 degree rotation (5π/4 radians)
    ese_rect_set_rotation(rect, 5.0f * M_PI / 4.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 10.0f, 10.0f), "Center point (10,10) should be inside 225° rotated rect");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 5.0f, 10.0f), "Point (5,10) should be inside 225° rotated rect");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 10.0f, 5.0f), "Point (10,5) should be inside 225° rotated rect");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, -5.0f, 10.0f), "Point (-5,10) should not be inside 225° rotated rect");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, 10.0f, -5.0f), "Point (10,-5) should not be inside 225° rotated rect");
    
    // Test 315 degree rotation (7π/4 radians)
    ese_rect_set_rotation(rect, 7.0f * M_PI / 4.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 10.0f, 10.0f), "Center point (10,10) should be inside 315° rotated rect");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 15.0f, 10.0f), "Point (15,10) should be inside 315° rotated rect");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 10.0f, 5.0f), "Point (10,5) should be inside 315° rotated rect");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, 25.0f, 10.0f), "Point (25,10) should not be inside 315° rotated rect");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, 10.0f, -5.0f), "Point (10,-5) should not be inside 315° rotated rect");
    
    // Now test with rect at (50,50) position
    ese_rect_set_x(rect, 50.0f);
    ese_rect_set_y(rect, 50.0f);
    
    // Test 45 degree rotation at (50,50)
    ese_rect_set_rotation(rect, M_PI / 4.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 60.0f, 60.0f), "Center point (60,60) should be inside 45° rotated rect at (50,50)");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 65.0f, 60.0f), "Point (65,60) should be inside 45° rotated rect at (50,50)");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 60.0f, 65.0f), "Point (60,65) should be inside 45° rotated rect at (50,50)");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, 75.0f, 60.0f), "Point (75,60) should not be inside 45° rotated rect at (50,50)");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, 60.0f, 75.0f), "Point (60,75) should not be inside 45° rotated rect at (50,50)");
    
    // Test 135 degree rotation at (50,50)
    ese_rect_set_rotation(rect, 3.0f * M_PI / 4.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 60.0f, 60.0f), "Center point (60,60) should be inside 135° rotated rect at (50,50)");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 55.0f, 60.0f), "Point (55,60) should be inside 135° rotated rect at (50,50)");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 60.0f, 65.0f), "Point (60,65) should be inside 135° rotated rect at (50,50)");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, 45.0f, 60.0f), "Point (45,60) should not be inside 135° rotated rect at (50,50)");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, 60.0f, 75.0f), "Point (60,75) should not be inside 135° rotated rect at (50,50)");
    
    // Test 225 degree rotation at (50,50)
    ese_rect_set_rotation(rect, 5.0f * M_PI / 4.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 60.0f, 60.0f), "Center point (60,60) should be inside 225° rotated rect at (50,50)");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 55.0f, 60.0f), "Point (55,60) should be inside 225° rotated rect at (50,50)");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 60.0f, 55.0f), "Point (60,55) should be inside 225° rotated rect at (50,50)");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, 45.0f, 60.0f), "Point (45,60) should not be inside 225° rotated rect at (50,50)");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, 60.0f, 45.0f), "Point (60,45) should not be inside 225° rotated rect at (50,50)");
    
    // Test 315 degree rotation at (50,50)
    ese_rect_set_rotation(rect, 7.0f * M_PI / 4.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 60.0f, 60.0f), "Center point (60,60) should be inside 315° rotated rect at (50,50)");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 65.0f, 60.0f), "Point (65,60) should be inside 315° rotated rect at (50,50)");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_contains_point(rect, 60.0f, 55.0f), "Point (60,55) should be inside 315° rotated rect at (50,50)");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, 75.0f, 60.0f), "Point (75,60) should not be inside 315° rotated rect at (50,50)");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_contains_point(rect, 60.0f, 45.0f), "Point (60,45) should not be inside 315° rotated rect at (50,50)");
    
    ese_rect_destroy(rect);
}

static void test_ese_rect_intersects_rotated(void) {
    EseRect *rect1 = ese_rect_create(g_engine);
    EseRect *rect2 = ese_rect_create(g_engine);
    EseRect *rect3 = ese_rect_create(g_engine);
    
    // Test with rects at (0,0) and size 20x20
    ese_rect_set_x(rect1, 0.0f);
    ese_rect_set_y(rect1, 0.0f);
    ese_rect_set_width(rect1, 20.0f);
    ese_rect_set_height(rect1, 20.0f);
    ese_rect_set_rotation(rect1, 0.0f); // No rotation for rect1
    
    ese_rect_set_x(rect2, 10.0f);
    ese_rect_set_y(rect2, 10.0f);
    ese_rect_set_width(rect2, 20.0f);
    ese_rect_set_height(rect2, 20.0f);
    
    ese_rect_set_x(rect3, 30.0f);
    ese_rect_set_y(rect3, 30.0f);
    ese_rect_set_width(rect3, 20.0f);
    ese_rect_set_height(rect3, 20.0f);
    
    // Test 45 degree rotation
    ese_rect_set_rotation(rect2, M_PI / 4.0f);
    ese_rect_set_rotation(rect3, M_PI / 4.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect1, rect2), "Rect1 should intersect with 45° rotated rect2");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect2, rect1), "45° rotated rect2 should intersect with rect1 (commutative)");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect1, rect3), "Rect1 should not intersect with 45° rotated rect3");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect3, rect1), "45° rotated rect3 should not intersect with rect1 (commutative)");
    
    // Test 135 degree rotation
    ese_rect_set_rotation(rect2, 3.0f * M_PI / 4.0f);
    ese_rect_set_rotation(rect3, 3.0f * M_PI / 4.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect1, rect2), "Rect1 should intersect with 135° rotated rect2");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect2, rect1), "135° rotated rect2 should intersect with rect1 (commutative)");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect1, rect3), "Rect1 should not intersect with 135° rotated rect3");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect3, rect1), "135° rotated rect3 should not intersect with rect1 (commutative)");
    
    // Test 225 degree rotation
    ese_rect_set_rotation(rect2, 5.0f * M_PI / 4.0f);
    ese_rect_set_rotation(rect3, 5.0f * M_PI / 4.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect1, rect2), "Rect1 should intersect with 225° rotated rect2");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect2, rect1), "225° rotated rect2 should intersect with rect1 (commutative)");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect1, rect3), "Rect1 should not intersect with 225° rotated rect3");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect3, rect1), "225° rotated rect3 should not intersect with rect1 (commutative)");
    
    // Test 315 degree rotation
    ese_rect_set_rotation(rect2, 7.0f * M_PI / 4.0f);
    ese_rect_set_rotation(rect3, 7.0f * M_PI / 4.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect1, rect2), "Rect1 should intersect with 315° rotated rect2");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect2, rect1), "315° rotated rect2 should intersect with rect1 (commutative)");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect1, rect3), "Rect1 should not intersect with 315° rotated rect3");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect3, rect1), "315° rotated rect3 should not intersect with rect1 (commutative)");
    
    // Now test with rects at (50,50) position
    ese_rect_set_x(rect1, 50.0f);
    ese_rect_set_y(rect1, 50.0f);
    
    ese_rect_set_x(rect2, 60.0f);
    ese_rect_set_y(rect2, 60.0f);
    
    ese_rect_set_x(rect3, 80.0f);
    ese_rect_set_y(rect3, 80.0f);
    
    // Test 45 degree rotation at (50,50)
    ese_rect_set_rotation(rect2, M_PI / 4.0f);
    ese_rect_set_rotation(rect3, M_PI / 4.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect1, rect2), "Rect1 at (50,50) should intersect with 45° rotated rect2");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect2, rect1), "45° rotated rect2 should intersect with rect1 at (50,50) (commutative)");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect1, rect3), "Rect1 at (50,50) should not intersect with 45° rotated rect3");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect3, rect1), "45° rotated rect3 should not intersect with rect1 at (50,50) (commutative)");
    
    // Test 135 degree rotation at (50,50)
    ese_rect_set_rotation(rect2, 3.0f * M_PI / 4.0f);
    ese_rect_set_rotation(rect3, 3.0f * M_PI / 4.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect1, rect2), "Rect1 at (50,50) should intersect with 135° rotated rect2");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect2, rect1), "135° rotated rect2 should intersect with rect1 at (50,50) (commutative)");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect1, rect3), "Rect1 at (50,50) should not intersect with 135° rotated rect3");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect3, rect1), "135° rotated rect3 should not intersect with rect1 at (50,50) (commutative)");
    
    // Test 225 degree rotation at (50,50)
    ese_rect_set_rotation(rect2, 5.0f * M_PI / 4.0f);
    ese_rect_set_rotation(rect3, 5.0f * M_PI / 4.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect1, rect2), "Rect1 at (50,50) should intersect with 225° rotated rect2");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect2, rect1), "225° rotated rect2 should intersect with rect1 at (50,50) (commutative)");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect1, rect3), "Rect1 at (50,50) should not intersect with 225° rotated rect3");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect3, rect1), "225° rotated rect3 should not intersect with rect1 at (50,50) (commutative)");
    
    // Test 315 degree rotation at (50,50)
    ese_rect_set_rotation(rect2, 7.0f * M_PI / 4.0f);
    ese_rect_set_rotation(rect3, 7.0f * M_PI / 4.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect1, rect2), "Rect1 at (50,50) should intersect with 315° rotated rect2");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect2, rect1), "315° rotated rect2 should intersect with rect1 at (50,50) (commutative)");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect1, rect3), "Rect1 at (50,50) should not intersect with 315° rotated rect3");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect3, rect1), "315° rotated rect3 should not intersect with rect1 at (50,50) (commutative)");
    
    // Test both rects rotated
    ese_rect_set_rotation(rect1, M_PI / 4.0f);
    ese_rect_set_rotation(rect2, M_PI / 4.0f);
    ese_rect_set_rotation(rect3, M_PI / 4.0f);
    
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect1, rect2), "Both 45° rotated rects should intersect");
    TEST_ASSERT_TRUE_MESSAGE(ese_rect_intersects(rect2, rect1), "Both 45° rotated rects should intersect (commutative)");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect1, rect3), "45° rotated rects should not intersect when far apart");
    TEST_ASSERT_FALSE_MESSAGE(ese_rect_intersects(rect3, rect1), "45° rotated rects should not intersect when far apart (commutative)");
    
    ese_rect_destroy(rect1);
    ese_rect_destroy(rect2);
    ese_rect_destroy(rect3);
}

static void test_ese_rect_watcher_system(void) {
    EseRect *rect = ese_rect_create(g_engine);

    mock_reset();
    ese_rect_set_x(rect, 25.0f);
    TEST_ASSERT_FALSE_MESSAGE(watcher_called, "Watcher should not be called before adding");

    void *test_userdata = (void*)0x12345678;
    bool add_result = ese_rect_add_watcher(rect, test_watcher_callback, test_userdata);
    TEST_ASSERT_TRUE_MESSAGE(add_result, "Should successfully add watcher");

    mock_reset();
    ese_rect_set_x(rect, 50.0f);
    TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when x changes");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(rect, last_watched_rect, "Watcher should receive correct rect pointer");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(test_userdata, last_watcher_userdata, "Watcher should receive correct userdata");

    mock_reset();
    ese_rect_set_y(rect, 75.0f);
    TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when y changes");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(rect, last_watched_rect, "Watcher should receive correct rect pointer");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(test_userdata, last_watcher_userdata, "Watcher should receive correct userdata");

    mock_reset();
    ese_rect_set_width(rect, 100.0f);
    TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when width changes");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(rect, last_watched_rect, "Watcher should receive correct rect pointer");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(test_userdata, last_watcher_userdata, "Watcher should receive correct userdata");

    mock_reset();
    ese_rect_set_height(rect, 150.0f);
    TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when height changes");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(rect, last_watched_rect, "Watcher should receive correct rect pointer");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(test_userdata, last_watcher_userdata, "Watcher should receive correct userdata");

    mock_reset();
    ese_rect_set_rotation(rect, M_PI / 2.0f);
    TEST_ASSERT_TRUE_MESSAGE(watcher_called, "Watcher should be called when rotation changes");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(rect, last_watched_rect, "Watcher should receive correct rect pointer");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(test_userdata, last_watcher_userdata, "Watcher should receive correct userdata");

    bool remove_result = ese_rect_remove_watcher(rect, test_watcher_callback, test_userdata);
    TEST_ASSERT_TRUE_MESSAGE(remove_result, "Should successfully remove watcher");

    mock_reset();
    ese_rect_set_x(rect, 100.0f);
    TEST_ASSERT_FALSE_MESSAGE(watcher_called, "Watcher should not be called after removal");

    ese_rect_destroy(rect);
}

static void test_ese_rect_lua_integration(void) {
    EseLuaEngine *engine = create_test_engine();
    EseRect *rect = ese_rect_create(engine);

    lua_State *before_state = ese_rect_get_state(rect);
    TEST_ASSERT_NOT_NULL_MESSAGE(before_state, "Rect should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, before_state, "Rect state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_rect_get_lua_ref(rect), "Rect should have no Lua reference initially");

    ese_rect_ref(rect);
    lua_State *after_ref_state = ese_rect_get_state(rect);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_ref_state, "Rect should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_ref_state, "Rect state should match engine runtime");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(LUA_NOREF, ese_rect_get_lua_ref(rect), "Rect should have a valid Lua reference after ref");

    ese_rect_unref(rect);
    lua_State *after_unref_state = ese_rect_get_state(rect);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_unref_state, "Rect should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_unref_state, "Rect state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_rect_get_lua_ref(rect), "Rect should have no Lua reference after unref");

    ese_rect_destroy(rect);
    lua_engine_destroy(engine);
}

static void test_ese_rect_lua_init(void) {
    lua_State *L = g_engine->runtime;
    
    luaL_getmetatable(L, RECT_PROXY_META);
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Metatable should not exist before initialization");
    lua_pop(L, 1);
    
    lua_getglobal(L, "Rect");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Global Rect table should not exist before initialization");
    lua_pop(L, 1);
    
    ese_rect_lua_init(g_engine);
    
    luaL_getmetatable(L, RECT_PROXY_META);
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Metatable should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Metatable should be a table");
    lua_pop(L, 1);
    
    lua_getglobal(L, "Rect");
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Global Rect table should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Global Rect table should be a table");
    lua_pop(L, 1);
}

static void test_ese_rect_lua_push(void) {
    ese_rect_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseRect *rect = ese_rect_create(g_engine);
    
    ese_rect_lua_push(rect);
    
    EseRect **ud = (EseRect **)lua_touserdata(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(rect, *ud, "The pushed item should be the actual rect");
    
    lua_pop(L, 1); 
    
    ese_rect_destroy(rect);
}

static void test_ese_rect_lua_get(void) {
    ese_rect_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseRect *rect = ese_rect_create(g_engine);
    
    ese_rect_lua_push(rect);
    
    EseRect *extracted_rect = ese_rect_lua_get(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(rect, extracted_rect, "Extracted rect should match original");
    
    lua_pop(L, 1);
    ese_rect_destroy(rect);
}

/**
* Lua API Test Functions
*/

static void test_ese_rect_lua_new(void) {
    ese_rect_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "return Rect.new()\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute with error");

    const char *testB = "return Rect.new(10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute with error");

    const char *testC = "return Rect.new(10, 10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute with error");

    const char *testD = "return Rect.new(10, 10, 10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testD), "testD Lua code should execute with error");

    const char *testE = "return Rect.new(\"10\", \"10\", \"10\", \"10\")\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testE), "testE Lua code should execute with error");

    const char *testF = "return Rect.new(10, 10, 100, 75)\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testF), "testF Lua code should execute without error");
    EseRect *extracted_rect = ese_rect_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_rect, "Extracted rect should not be NULL");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, ese_rect_get_x(extracted_rect), "Extracted rect should have x=10");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, ese_rect_get_y(extracted_rect), "Extracted rect should have y=10");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(100.0f, ese_rect_get_width(extracted_rect), "Extracted rect should have width=100");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(75.0f, ese_rect_get_height(extracted_rect), "Extracted rect should have height=75");
    ese_rect_destroy(extracted_rect);
}

static void test_ese_rect_lua_zero(void) {
    ese_rect_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "return Rect.zero(10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "testA Lua code should execute with error");

    const char *testB = "return Rect.zero(10, 10)\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "testB Lua code should execute with error");

    const char *testC = "return Rect.zero()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute without error");
    EseRect *extracted_rect = ese_rect_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_rect, "Extracted rect should not be NULL");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_rect_get_x(extracted_rect), "Extracted rect should have x=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_rect_get_y(extracted_rect), "Extracted rect should have y=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_rect_get_width(extracted_rect), "Extracted rect should have width=0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, ese_rect_get_height(extracted_rect), "Extracted rect should have height=0");
    ese_rect_destroy(extracted_rect);
}

static void test_ese_rect_lua_area(void) {
    ese_rect_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code1 = "local r = Rect.new(0, 0, 10, 5); return r:area()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code1), "area test should execute without error");
    double area = lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(50.0f, area, "Area should be 50");
    lua_pop(L, 1);

    const char *test_code2 = "local r = Rect.new(0, 0, 10, 5); return r:area(10)";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code2), "area test 2 should execute with error");
}

static void test_ese_rect_lua_contains_point(void) {
    ese_rect_lua_init(g_engine);
    ese_point_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code = "local r = Rect.new(0, 0, 10, 10); return r:contains_point(5, 5)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "contains_point test should execute without error");
    bool contains = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(contains, "Point should be contained");
    lua_pop(L, 1);

    const char *test_code2 = "local r = Rect.new(0, 0, 10, 10); return r:contains_point(15, 15)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code2), "contains_point test 2 should execute without error");
    bool contains2 = lua_toboolean(L, -1);
    TEST_ASSERT_FALSE_MESSAGE(contains2, "Point should not be contained");
    lua_pop(L, 1);

    const char *test_code3 = "local r = Rect.new(0, 0, 10, 10); return r:contains_point(Point.new(5, 5))";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code3), "contains_point test 3 should execute without error");
    bool contains3 = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(contains3, "Point should be contained");
    lua_pop(L, 1);

    const char *test_code4 = "local r = Rect.new(0, 0, 10, 10); return r:contains_point(Point.new(15, 15))";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code4), "contains_point test 4 should execute without error");
    bool contains4 = lua_toboolean(L, -1);
    TEST_ASSERT_FALSE_MESSAGE(contains4, "Point should not be contained");
    lua_pop(L, 1);

    const char *test_code5 = "local r = Rect.new(0, 0, 10, 10); return r:contains_point()";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code5), "test_code5 Lua code should execute with error");

    const char *test_code6 = "local r = Rect.new(0, 0, 10, 10); return r:contains_point(5, 5, 5)";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code6), "test_code6 Lua code should execute with error");

    const char *test_code7 = "local r = Rect.new(0, 0, 10, 10); return r:contains_point(\"4\", \"6\")";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code6), "test_code6 Lua code should execute with error");
}

static void test_ese_rect_lua_intersects(void) {
    ese_rect_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *test_code = "local r1 = Rect.new(0, 0, 10, 10); local r2 = Rect.new(5, 5, 10, 10); return r1:intersects(r2)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "intersects test should execute without error");
    bool intersects = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(intersects, "Rectangles should intersect");
    lua_pop(L, 1);

    const char *test_code2 = "local r1 = Rect.new(0, 0, 10, 10); local r2 = Rect.new(20, 20, 5, 5); return r1:intersects(r2)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code2), "intersects test 2 should execute without error");
    bool intersects2 = lua_toboolean(L, -1);
    TEST_ASSERT_FALSE_MESSAGE(intersects2, "Rectangles should not intersect");
    lua_pop(L, 1);
}

static void test_ese_rect_lua_x(void) {
    ese_rect_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local r = Rect.new(0, 0, 0, 0); r.x = 10; return r.x";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua x set/get test 1 should execute without error");
    double x = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, x);
    lua_pop(L, 1);

    const char *test2 = "local r = Rect.new(0, 0, 0, 0); r.x = -10; return r.x";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua x set/get test 2 should execute without error");
    x = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, x);
    lua_pop(L, 1);

    const char *test3 = "local r = Rect.new(0, 0, 0, 0); r.x = 0; return r.x";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua x set/get test 3 should execute without error");
    x = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, x);
    lua_pop(L, 1);
}

static void test_ese_rect_lua_y(void) {
    ese_rect_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local r = Rect.new(0, 0, 0, 0); r.y = 20; return r.y";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua y set/get test 1 should execute without error");
    double y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, y);
    lua_pop(L, 1);

    const char *test2 = "local r = Rect.new(0, 0, 0, 0); r.y = -10; return r.y";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua y set/get test 2 should execute without error");
    y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, y);
    lua_pop(L, 1);

    const char *test3 = "local r = Rect.new(0, 0, 0, 0); r.y = 0; return r.y";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua y set/get test 3 should execute without error");
    y = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, y);
    lua_pop(L, 1);
}

static void test_ese_rect_lua_width(void) {
    ese_rect_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local r = Rect.new(0, 0, 0, 0); r.width = 100; return r.width";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua width set/get test 1 should execute without error");
    double width = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, width);
    lua_pop(L, 1);

    const char *test2 = "local r = Rect.new(0, 0, 0, 0); r.width = -50; return r.width";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua width set/get test 2 should execute without error");
    width = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -50.0f, width);
    lua_pop(L, 1);

    const char *test3 = "local r = Rect.new(0, 0, 0, 0); r.width = 0; return r.width";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua width set/get test 3 should execute without error");
    width = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, width);
    lua_pop(L, 1);
}

static void test_ese_rect_lua_height(void) {
    ese_rect_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local r = Rect.new(0, 0, 0, 0); r.height = 75; return r.height";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua height set/get test 1 should execute without error");
    double height = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 75.0f, height);
    lua_pop(L, 1);

    const char *test2 = "local r = Rect.new(0, 0, 0, 0); r.height = -25; return r.height";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua height set/get test 2 should execute without error");
    height = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -25.0f, height);
    lua_pop(L, 1);

    const char *test3 = "local r = Rect.new(0, 0, 0, 0); r.height = 0; return r.height";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua height set/get test 3 should execute without error");
    height = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, height);
    lua_pop(L, 1);
}

static void test_ese_rect_lua_rotation(void) {
    ese_rect_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test1 = "local r = Rect.new(0, 0, 0, 0); r.rotation = 45; return r.rotation";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "Lua rotation set/get test 1 should execute without error");
    double rotation = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 45.0f, rotation);
    lua_pop(L, 1);

    const char *test2 = "local r = Rect.new(0, 0, 0, 0); r.rotation = -90; return r.rotation";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "Lua rotation set/get test 2 should execute without error");
    rotation = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -90.0f, rotation);
    lua_pop(L, 1);

    const char *test3 = "local r = Rect.new(0, 0, 0, 0); r.rotation = 0; return r.rotation";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "Lua rotation set/get test 3 should execute without error");
    rotation = lua_tonumber(L, -1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, rotation);
    lua_pop(L, 1);
}

static void test_ese_rect_lua_tostring(void) {
    ese_rect_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *test_code = "local r = Rect.new(10.5, 20.25, 100.0, 75.5); return tostring(r)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "tostring test should execute without error");
    const char *result = lua_tostring(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "tostring result should not be NULL");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "x=10.50") != NULL, "tostring should contain 'x=10.50'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "y=20.25") != NULL, "tostring should contain 'y=20.25'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "w=100.00") != NULL, "tostring should contain 'w=100.00'");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "h=75.50") != NULL, "tostring should contain 'h=75.50'");
    lua_pop(L, 1); 
}

static void test_ese_rect_lua_gc(void) {
    ese_rect_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *testA = "local r = Rect.new(5, 10, 100, 75)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "Rect creation should execute without error");
    
    int collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testB = "return Rect.new(5, 10, 100, 75)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testB), "Rect creation should execute without error");
    EseRect *extracted_rect = ese_rect_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_rect, "Extracted rect should not be NULL");
    ese_rect_ref(extracted_rect);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    ese_rect_unref(extracted_rect);

    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");
    
    const char *testC = "return Rect.new(5, 10, 100, 75)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "Rect creation should execute without error");
    extracted_rect = ese_rect_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_rect, "Extracted rect should not be NULL");
    ese_rect_ref(extracted_rect);
    
    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    ese_rect_unref(extracted_rect);
    ese_rect_destroy(extracted_rect);

    collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect");

    // Verify GC didn't crash by running another operation
    const char *verify_code = "return 42";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, verify_code), "Lua should still work after GC");
    int result = (int)lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(42, result, "Lua should return correct value after GC");
    lua_pop(L, 1);
}
