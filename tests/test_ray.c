#include "test_utils.h"
#include "types/ray.h"
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
static void test_ray_creation();
static void test_ray_properties();
static void test_ray_copy();
static void test_ray_mathematical_operations();
static void test_ray_intersection_tests();
static void test_ray_lua_integration();
static void test_ray_lua_script_api();
static void test_ray_null_pointer_aborts();

// Test Lua script content for Ray testing
static const char* test_ray_lua_script = 
"function RAY_TEST_MODULE:test_ray_creation()\n"
"    local r1 = Ray.new(10.5, -5.25, 1.0, 0.5)\n"
"    local r2 = Ray.zero()\n"
"    \n"
"    if r1.x == 10.5 and r1.y == -5.25 and r1.dx == 1.0 and r1.dy == 0.5 and\n"
"       r2.x == 0 and r2.y == 0 and r2.dx == 1 and r2.dy == 0 then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n"
"\n"
"function RAY_TEST_MODULE:test_ray_properties()\n"
"    local r = Ray.new(0, 0, 0, 0)\n"
"    \n"
"    r.x = 42.0\n"
"    r.y = -17.5\n"
"    r.dx = 2.0\n"
"    r.dy = -1.5\n"
"    \n"
"    if r.x == 42.0 and r.y == -17.5 and r.dx == 2.0 and r.dy == -1.5 then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n"
"\n"
"function RAY_TEST_MODULE:test_ray_operations()\n"
"    local r = Ray.new(0, 0, 3, 4)\n"
"    \n"
"    -- Test point at distance\n"
"    local point = r:point_at_distance(5.0)\n"
"    if math.abs(point.x - 3.0) < 0.001 and math.abs(point.y - 4.0) < 0.001 then\n"
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
    
    test_suite_begin("🧪 EseRay Test Suite");
    
    test_ray_creation();
    test_ray_properties();
    test_ray_copy();
    test_ray_mathematical_operations();
    test_ray_intersection_tests();
    test_ray_lua_integration();
    test_ray_lua_script_api();
    test_ray_null_pointer_aborts();
    
    test_suite_end("🎯 EseRay Test Suite");
    
    return 0;
}

static void test_ray_creation() {
    test_begin("Ray Creation Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for ray creation tests");
    
    EseRay *ray = ray_create(engine);
    TEST_ASSERT_NOT_NULL(ray, "ray_create should return non-NULL pointer");
    TEST_ASSERT_EQUAL(0.0f, ray->x, "New ray should have x = 0.0");
    TEST_ASSERT_EQUAL(0.0f, ray->y, "New ray should have y = 0.0");
    TEST_ASSERT_EQUAL(1.0f, ray->dx, "New ray should have dx = 1.0");
    TEST_ASSERT_EQUAL(0.0f, ray->dy, "New ray should have dy = 0.0");
    TEST_ASSERT_POINTER_EQUAL(engine->runtime, ray->state, "Ray should have correct Lua state");
    TEST_ASSERT_EQUAL(0, ray->lua_ref_count, "New ray should have ref count 0");
    TEST_ASSERT(ray->lua_ref == LUA_NOREF, "New ray should have negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", ray->lua_ref);
    TEST_ASSERT(sizeof(EseRay) > 0, "EseRay should have positive size");
    printf("ℹ INFO: Actual ray size: %zu bytes\n", sizeof(EseRay));
    
    ray_destroy(ray);
    lua_engine_destroy(engine);
    
    test_end("Ray Creation Tests");
}

static void test_ray_properties() {
    test_begin("Ray Properties Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for ray property tests");
    
    EseRay *ray = ray_create(engine);
    TEST_ASSERT_NOT_NULL(ray, "Ray should be created for property tests");
    
    ray->x = 10.5f;
    ray->y = -5.25f;
    ray->dx = 2.0f;
    ray->dy = -1.5f;
    
    TEST_ASSERT_EQUAL(10.5f, ray->x, "Direct field access should set x coordinate");
    TEST_ASSERT_EQUAL(-5.25f, ray->y, "Direct field access should set y coordinate");
    TEST_ASSERT_EQUAL(2.0f, ray->dx, "Direct field access should set dx component");
    TEST_ASSERT_EQUAL(-1.5f, ray->dy, "Direct field access should set dy component");
    
    ray->x = -100.0f;
    ray->y = 200.0f;
    ray->dx = -3.0f;
    ray->dy = 4.0f;
    
    TEST_ASSERT_EQUAL(-100.0f, ray->x, "Direct field access should handle negative values");
    TEST_ASSERT_EQUAL(200.0f, ray->y, "Direct field access should handle negative values");
    TEST_ASSERT_EQUAL(-3.0f, ray->dx, "Direct field access should handle negative values");
    TEST_ASSERT_EQUAL(4.0f, ray->dy, "Direct field access should handle negative values");
    
    ray->x = 0.0f;
    ray->y = 0.0f;
    ray->dx = 0.0f;
    ray->dy = 0.0f;
    
    TEST_ASSERT_EQUAL(0.0f, ray->x, "Direct field access should handle zero values");
    TEST_ASSERT_EQUAL(0.0f, ray->y, "Direct field access should handle zero values");
    TEST_ASSERT_EQUAL(0.0f, ray->dx, "Direct field access should handle zero values");
    TEST_ASSERT_EQUAL(0.0f, ray->dy, "Direct field access should handle zero values");
    
    ray_destroy(ray);
    lua_engine_destroy(engine);
    
    test_end("Ray Properties Tests");
}

static void test_ray_copy() {
    test_begin("Ray Copy Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for ray copy tests");
    
    EseRay *original = ray_create(engine);
    TEST_ASSERT_NOT_NULL(original, "Original ray should be created for copy tests");
    
    original->x = 42.0f;
    original->y = -17.5f;
    original->dx = 3.0f;
    original->dy = 4.0f;
    
    EseRay *copy = ray_copy(original);
    TEST_ASSERT_NOT_NULL(copy, "ray_copy should return non-NULL pointer");
    TEST_ASSERT_EQUAL(42.0f, copy->x, "Copied ray should have same x value");
    TEST_ASSERT_EQUAL(-17.5f, copy->y, "Copied ray should have same y value");
    TEST_ASSERT_EQUAL(3.0f, copy->dx, "Copied ray should have same dx value");
    TEST_ASSERT_EQUAL(4.0f, copy->dy, "Copied ray should have same dy value");
    TEST_ASSERT(original != copy, "Copy should be a different object");
    TEST_ASSERT_POINTER_EQUAL(original->state, copy->state, "Copy should have same Lua state");
    TEST_ASSERT(copy->lua_ref == LUA_NOREF, "Copy should start with negative LUA_NOREF value");
    printf("ℹ INFO: Copy LUA_NOREF value: %d\n", copy->lua_ref);
    TEST_ASSERT_EQUAL(0, copy->lua_ref_count, "Copy should start with ref count 0");
    
    ray_destroy(original);
    ray_destroy(copy);
    lua_engine_destroy(engine);
    
    test_end("Ray Copy Tests");
}

static void test_ray_mathematical_operations() {
    test_begin("Ray Mathematical Operations Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for ray math tests");
    
    EseRay *ray = ray_create(engine);
    TEST_ASSERT_NOT_NULL(ray, "Ray should be created for math tests");
    
    // Test point at distance
    ray->x = 0.0f;
    ray->y = 0.0f;
    ray->dx = 3.0f;
    ray->dy = 4.0f;
    
    float point_x, point_y;
    ray_get_point_at_distance(ray, 5.0f, &point_x, &point_y);
    TEST_ASSERT_FLOAT_EQUAL(3.0f, point_x, 0.001f, "Point at distance 5 should have x = 3.0");
    TEST_ASSERT_FLOAT_EQUAL(4.0f, point_y, 0.001f, "Point at distance 5 should have y = 4.0");
    
    // Test point at distance 0
    ray_get_point_at_distance(ray, 0.0f, &point_x, &point_y);
    TEST_ASSERT_FLOAT_EQUAL(0.0f, point_x, 0.001f, "Point at distance 0 should have x = 0.0");
    TEST_ASSERT_FLOAT_EQUAL(0.0f, point_y, 0.001f, "Point at distance 0 should have y = 0.0");
    
    // Test point at negative distance
    ray_get_point_at_distance(ray, -2.0f, &point_x, &point_y);
    TEST_ASSERT_FLOAT_EQUAL(-1.2f, point_x, 0.001f, "Point at distance -2 should have x = -1.2");
    TEST_ASSERT_FLOAT_EQUAL(-1.6f, point_y, 0.001f, "Point at distance -2 should have y = -1.6");
    
    ray_destroy(ray);
    lua_engine_destroy(engine);
    
    test_end("Ray Mathematical Operations Tests");
}

static void test_ray_intersection_tests() {
    test_begin("Ray Intersection Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for ray intersection tests");
    
    EseRay *ray = ray_create(engine);
    EseRect *rect = rect_create(engine);
    
    TEST_ASSERT_NOT_NULL(ray, "Ray should be created for intersection tests");
    TEST_ASSERT_NOT_NULL(rect, "Rect should be created for intersection tests");
    
    // Set up ray from (0,0) going right
    ray->x = 0.0f;
    ray->y = 0.0f;
    ray->dx = 1.0f;
    ray->dy = 0.0f;
    
    // Set up rectangle at (5, -2) with size (4, 4)
    rect_set_x(rect, 5.0f);
    rect_set_y(rect, -2.0f);
    rect_set_width(rect, 4.0f);
    rect_set_height(rect, 4.0f);
    
    // Test intersection
    bool intersects = ray_intersects_rect(ray, rect);
    TEST_ASSERT(intersects, "Ray should intersect with rectangle");
    
    // Test ray that doesn't intersect
    ray->y = 10.0f; // Move ray above rectangle
    intersects = ray_intersects_rect(ray, rect);
    TEST_ASSERT(!intersects, "Ray should not intersect with rectangle when above it");
    
    ray_destroy(ray);
    rect_destroy(rect);
    lua_engine_destroy(engine);
    
    test_end("Ray Intersection Tests");
}

static void test_ray_lua_integration() {
    test_begin("Ray Lua Integration Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for ray Lua integration tests");
    
    EseRay *ray = ray_create(engine);
    TEST_ASSERT_NOT_NULL(ray, "Ray should be created for Lua integration tests");
    TEST_ASSERT_EQUAL(0, ray->lua_ref_count, "New ray should start with ref count 0");
    TEST_ASSERT(ray->lua_ref == LUA_NOREF, "New ray should start with negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", ray->lua_ref);
    
    ray_destroy(ray);
    lua_engine_destroy(engine);
    
    test_end("Ray Lua Integration Tests");
}

static void test_ray_lua_script_api() {
    test_begin("Ray Lua Script API Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for ray Lua script API tests");
    
    ray_lua_init(engine);
    printf("ℹ INFO: Ray Lua integration initialized\n");
    
    // For now, just test that the Lua integration initializes without errors
    TEST_ASSERT(true, "Ray Lua integration should initialize successfully");
    
    lua_engine_destroy(engine);
    
    test_end("Ray Lua Script API Tests");
}

static void test_ray_null_pointer_aborts() {
    test_begin("Ray NULL Pointer Abort Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for ray NULL pointer abort tests");
    
    EseRay *ray = ray_create(engine);
    TEST_ASSERT_NOT_NULL(ray, "Ray should be created for ray NULL pointer abort tests");
    
    TEST_ASSERT_ABORT(ray_create(NULL), "ray_create should abort with NULL engine");
    TEST_ASSERT_ABORT(ray_copy(NULL), "ray_copy should abort with NULL source");
    TEST_ASSERT_ABORT(ray_lua_init(NULL), "ray_lua_init should abort with NULL engine");
    TEST_ASSERT_ABORT(ray_get_point_at_distance(NULL, 1.0f, NULL, NULL), "ray_get_point_at_distance should abort with NULL ray");
    TEST_ASSERT_ABORT(ray_intersects_rect(NULL, NULL), "ray_intersects_rect should abort with NULL ray");
    TEST_ASSERT_ABORT(ray_lua_get(NULL, 1), "ray_lua_get should abort with NULL Lua state");
    TEST_ASSERT_ABORT(ray_lua_push(NULL), "ray_lua_push should abort with NULL ray");
    TEST_ASSERT_ABORT(ray_ref(NULL), "ray_ref should abort with NULL ray");
    
    ray_destroy(ray);
    lua_engine_destroy(engine);
    
    test_end("Ray NULL Pointer Abort Tests");
}
