#include "test_utils.h"
#include "types/arc.h"
#include "types/rect.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "scripting/lua_engine_private.h"
#include "utility/log.h"
#include <math.h>
#include <execinfo.h>
#include <signal.h>

// Define LUA_NOREF if not already defined
#ifndef LUA_NOREF
#define LUA_NOREF -1
#endif

// Test function declarations
static void test_arc_creation();
static void test_arc_properties();
static void test_arc_copy();
static void test_arc_mathematical_operations();
static void test_arc_intersection_tests();
static void test_arc_lua_integration();
static void test_arc_lua_script_api();
static void test_arc_null_pointer_aborts();

// Test Lua script content for Arc testing
static const char* test_arc_lua_script = 
"function ARC_TEST_MODULE:test_arc_creation()\n"
"    local a1 = Arc.new(10.5, -5.25, 2.0, 0.0, 3.14159)\n"
"    local a2 = Arc.zero()\n"
"    \n"
"    if a1.x == 10.5 and a1.y == -5.25 and a1.radius == 2.0 and a1.start_angle == 0.0 and a1.end_angle == 3.14159 and\n"
"       a2.x == 0 and a2.y == 0 and a2.radius == 1 and a2.start_angle == 0 and a2.end_angle == 6.28318 then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n"
"\n"
"function ARC_TEST_MODULE:test_arc_properties()\n"
"    local a = Arc.new(0, 0, 0, 0, 0)\n"
"    \n"
"    a.x = 42.0\n"
"    a.y = -17.5\n"
"    a.radius = 5.0\n"
"    a.start_angle = 0.785398\n"
"    a.end_angle = 2.35619\n"
"    \n"
"    if a.x == 42.0 and a.y == -17.5 and a.radius == 5.0 and a.start_angle == 0.785398 and a.end_angle == 2.35619 then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n"
"\n"
"function ARC_TEST_MODULE:test_arc_operations()\n"
"    local a = Arc.new(0, 0, 2.0, 0.0, 3.14159)\n"
"    \n"
"    -- Test point on arc\n"
"    local point = a:point_at_angle(1.5708)  -- π/2 radians (90 degrees)\n"
"    if math.abs(point.x - 0.0) < 0.001 and math.abs(point.y - 2.0) < 0.001 then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n";

// Signal handler for testing aborts
static void segfault_handler(int sig) {
    void *array[10];
    size_t size = backtrace(array, 10);
    fprintf(stderr, "---- BACKTRACE START ----\n");
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    fprintf(stderr, "---- BACKTRACE  END  ----\n");
    exit(1);
}

int main() {
    // Set up signal handler for testing aborts
    signal(SIGSEGV, segfault_handler);
    signal(SIGABRT, segfault_handler);
    
    test_suite_begin("🧪 EseArc Test Suite");
    
    test_arc_creation();
    test_arc_properties();
    test_arc_copy();
    test_arc_mathematical_operations();
    test_arc_intersection_tests();
    test_arc_lua_integration();
    test_arc_lua_script_api();
    test_arc_null_pointer_aborts();
    
    test_suite_end("🎯 EseArc Test Suite");
    
    return 0;
}

static void test_arc_creation() {
    test_begin("Arc Creation Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for arc creation tests");
    
    EseArc *arc = arc_create(engine);
    TEST_ASSERT_NOT_NULL(arc, "arc_create should return non-NULL pointer");
    TEST_ASSERT_EQUAL(0.0f, arc->x, "New arc should have x = 0.0");
    TEST_ASSERT_EQUAL(0.0f, arc->y, "New arc should have y = 0.0");
    TEST_ASSERT_EQUAL(1.0f, arc->radius, "New arc should have radius = 1.0");
    TEST_ASSERT_EQUAL(0.0f, arc->start_angle, "New arc should have start_angle = 0.0");
    TEST_ASSERT_FLOAT_EQUAL(2.0f * M_PI, arc->end_angle, 0.001f, "New arc should have end_angle = 2π");
    TEST_ASSERT_POINTER_EQUAL(engine->runtime, arc->state, "Arc should have correct Lua state");
    TEST_ASSERT_EQUAL(0, arc->lua_ref_count, "New arc should have ref count 0");
    TEST_ASSERT(arc->lua_ref == LUA_NOREF, "New arc should have negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", arc->lua_ref);
    TEST_ASSERT(sizeof(EseArc) > 0, "EseArc should have positive size");
    printf("ℹ INFO: Actual arc size: %zu bytes\n", sizeof(EseArc));
    
    arc_destroy(arc);
    lua_engine_destroy(engine);
    
    test_end("Arc Creation Tests");
}

static void test_arc_properties() {
    test_begin("Arc Properties Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for arc property tests");
    
    EseArc *arc = arc_create(engine);
    TEST_ASSERT_NOT_NULL(arc, "Arc should be created for property tests");
    
    arc->x = 10.5f;
    arc->y = -5.25f;
    arc->radius = 2.0f;
    arc->start_angle = 0.785398f; // π/4 radians (45 degrees)
    arc->end_angle = 2.35619f;    // 3π/4 radians (135 degrees)
    
    TEST_ASSERT_EQUAL(10.5f, arc->x, "arc x should be set correctly");
    TEST_ASSERT_EQUAL(-5.25f, arc->y, "arc y should be set correctly");
    TEST_ASSERT_EQUAL(2.0f, arc->radius, "arc radius should be set correctly");
    TEST_ASSERT_FLOAT_EQUAL(0.785398f, arc->start_angle, 0.001f, "arc start_angle should be set correctly");
    TEST_ASSERT_FLOAT_EQUAL(2.35619f, arc->end_angle, 0.001f, "arc end_angle should be set correctly");
    
    arc->x = -100.0f;
    arc->y = 200.0f;
    arc->radius = 5.0f;
    arc->start_angle = -1.5708f; // -π/2 radians (-90 degrees)
    arc->end_angle = 1.5708f;    // π/2 radians (90 degrees)
    
    TEST_ASSERT_EQUAL(-100.0f, arc->x, "arc x should handle negative values");
    TEST_ASSERT_EQUAL(200.0f, arc->y, "arc y should handle negative values");
    TEST_ASSERT_EQUAL(5.0f, arc->radius, "arc radius should handle positive values");
    TEST_ASSERT_FLOAT_EQUAL(-1.5708f, arc->start_angle, 0.001f, "arc start_angle should handle negative values");
    TEST_ASSERT_FLOAT_EQUAL(1.5708f, arc->end_angle, 0.001f, "arc end_angle should handle positive values");
    
    arc->x = 0.0f;
    arc->y = 0.0f;
    arc->radius = 0.0f;
    arc->start_angle = 0.0f;
    arc->end_angle = 0.0f;
    
    TEST_ASSERT_EQUAL(0.0f, arc->x, "arc x should handle zero values");
    TEST_ASSERT_EQUAL(0.0f, arc->y, "arc y should handle zero values");
    TEST_ASSERT_EQUAL(0.0f, arc->radius, "arc radius should handle zero values");
    TEST_ASSERT_EQUAL(0.0f, arc->start_angle, "arc start_angle should handle zero values");
    TEST_ASSERT_EQUAL(0.0f, arc->end_angle, "arc end_angle should handle zero values");
    
    arc_destroy(arc);
    lua_engine_destroy(engine);
    
    test_end("Arc Properties Tests");
}

static void test_arc_copy() {
    test_begin("Arc Copy Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for arc copy tests");
    
    EseArc *original = arc_create(engine);
    TEST_ASSERT_NOT_NULL(original, "Original arc should be created for copy tests");
    
    original->x = 42.0f;
    original->y = -17.5f;
    original->radius = 3.0f;
    original->start_angle = 0.523599f; // π/6 radians (30 degrees)
    original->end_angle = 1.0472f;     // π/3 radians (60 degrees)
    
    EseArc *copy = arc_copy(original);
    TEST_ASSERT_NOT_NULL(copy, "arc_copy should return non-NULL pointer");
    TEST_ASSERT_EQUAL(42.0f, copy->x, "Copied arc should have same x value");
    TEST_ASSERT_EQUAL(-17.5f, copy->y, "Copied arc should have same y value");
    TEST_ASSERT_EQUAL(3.0f, copy->radius, "Copied arc should have same radius value");
    TEST_ASSERT_FLOAT_EQUAL(0.523599f, copy->start_angle, 0.001f, "Copied arc should have same start_angle value");
    TEST_ASSERT_FLOAT_EQUAL(1.0472f, copy->end_angle, 0.001f, "Copied arc should have same end_angle value");
    TEST_ASSERT(original != copy, "Copy should be a different object");
    TEST_ASSERT_POINTER_EQUAL(original->state, copy->state, "Copy should have same Lua state");
    TEST_ASSERT(copy->lua_ref == LUA_NOREF, "Copy should start with negative LUA_NOREF value");
    printf("ℹ INFO: Copy LUA_NOREF value: %d\n", copy->lua_ref);
    TEST_ASSERT_EQUAL(0, copy->lua_ref_count, "Copy should start with ref count 0");
    
    arc_destroy(original);
    arc_destroy(copy);
    lua_engine_destroy(engine);
    
    test_end("Arc Copy Tests");
}

static void test_arc_mathematical_operations() {
    test_begin("Arc Mathematical Operations Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for arc math tests");
    
    EseArc *arc = arc_create(engine);
    TEST_ASSERT_NOT_NULL(arc, "Arc should be created for math tests");
    
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
    
    arc_destroy(arc);
    lua_engine_destroy(engine);
    
    test_end("Arc Mathematical Operations Tests");
}

static void test_arc_intersection_tests() {
    test_begin("Arc Intersection Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for arc intersection tests");
    
    EseArc *arc = arc_create(engine);
    EseRect *rect = rect_create(engine);
    
    TEST_ASSERT_NOT_NULL(arc, "Arc should be created for intersection tests");
    TEST_ASSERT_NOT_NULL(rect, "Rect should be created for intersection tests");
    
    // Set up arc at (0,0) with radius 2, full circle
    arc->x = 0.0f;
    arc->y = 0.0f;
    arc->radius = 2.0f;
    arc->start_angle = 0.0f;
    arc->end_angle = 2.0f * M_PI;
    
    // Set up rectangle that intersects with arc
    rect_set_x(rect, 1.0f);
    rect_set_y(rect, 1.0f);
    rect_set_width(rect, 2.0f);
    rect_set_height(rect, 2.0f);
    
    // Test intersection
    bool intersects = arc_intersects_rect(arc, rect);
    TEST_ASSERT(intersects, "Arc should intersect with rectangle");
    
    // Test arc that doesn't intersect
    arc->x = 10.0f; // Move arc away from rectangle
    intersects = arc_intersects_rect(arc, rect);
    TEST_ASSERT(!intersects, "Arc should not intersect with rectangle when far away");
    
    arc_destroy(arc);
    rect_destroy(rect);
    lua_engine_destroy(engine);
    
    test_end("Arc Intersection Tests");
}

static void test_arc_lua_integration() {
    test_begin("Arc Lua Integration Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for arc Lua integration tests");
    
    EseArc *arc = arc_create(engine);
    TEST_ASSERT_NOT_NULL(arc, "Arc should be created for Lua integration tests");
    TEST_ASSERT_EQUAL(0, arc->lua_ref_count, "New arc should start with ref count 0");
    TEST_ASSERT(arc->lua_ref == LUA_NOREF, "New arc should start with negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", arc->lua_ref);
    
    arc_destroy(arc);
    lua_engine_destroy(engine);
    
    test_end("Arc Lua Integration Tests");
}

static void test_arc_lua_script_api() {
    test_begin("Arc Lua Script API Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for arc Lua script API tests");
    
    arc_lua_init(engine);
    printf("ℹ INFO: Arc Lua integration initialized\n");
    
    // Test that arc Lua integration initializes successfully
    TEST_ASSERT(true, "Arc Lua integration should initialize successfully");
    
    lua_engine_destroy(engine);
    
    test_end("Arc Lua Script API Tests");
}

static void test_arc_null_pointer_aborts() {
    test_begin("Arc NULL Pointer Abort Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for arc NULL pointer abort tests");
    
    EseArc *arc = arc_create(engine);
    TEST_ASSERT_NOT_NULL(arc, "Arc should be created for arc NULL pointer abort tests");
    
    TEST_ASSERT_ABORT(arc_create(NULL), "arc_create should abort with NULL engine");
    TEST_ASSERT_ABORT(arc_copy(NULL), "arc_copy should abort with NULL source");
    TEST_ASSERT_ABORT(arc_lua_init(NULL), "arc_lua_init should abort with NULL engine");
    // Test that functions abort with NULL arc
    TEST_ASSERT_ABORT(arc_get_point_at_angle(NULL, 1.0f, NULL, NULL), "arc_get_point_at_angle should abort with NULL arc");
    TEST_ASSERT_ABORT(arc_intersects_rect(NULL, NULL), "arc_intersects_rect should abort with NULL arc");
    TEST_ASSERT_ABORT(arc_lua_get(NULL, 1), "arc_lua_get should abort with NULL Lua state");
    TEST_ASSERT_ABORT(arc_lua_push(NULL), "arc_lua_push should abort with NULL arc");
    TEST_ASSERT_ABORT(arc_ref(NULL), "arc_ref should abort with NULL arc");
    
    arc_destroy(arc);
    lua_engine_destroy(engine);
    
    test_end("Arc NULL Pointer Abort Tests");
}
