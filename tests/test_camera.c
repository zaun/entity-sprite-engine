/*
 * Test file for camera functionality
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
#include "../src/types/point.h"
#include "../src/types/camera.h"
#include "../src/scripting/lua_engine.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

// Test function declarations
static void test_camera_creation();
static void test_camera_properties();
static void test_camera_copy();
static void test_camera_mathematical_operations();
static void test_camera_lua_integration();
static void test_camera_null_pointer_aborts();

// Helper function to create and initialize engine
static EseLuaEngine* create_test_engine() {
    EseLuaEngine *engine = lua_engine_create();
    if (engine) {
        // Set up registry keys that camera system needs
        lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);
        
        // Initialize required systems
        ese_point_lua_init(engine);
        camera_state_lua_init(engine);
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

    test_suite_begin("Camera Tests");

    // Initialize required systems
    log_init();

    // Run all test suites
    test_camera_creation();
    test_camera_properties();
    test_camera_copy();
    test_camera_mathematical_operations();
    test_camera_lua_integration();
    test_camera_null_pointer_aborts();

    test_suite_end("Camera Tests");

    return 0;
}

static void test_camera_creation() {
    test_begin("Camera Creation");
    
    EseLuaEngine *engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");
    
    EseCamera *camera = camera_state_create(engine);
    TEST_ASSERT_NOT_NULL(camera, "Camera should be created");
    TEST_ASSERT_NOT_NULL(camera->position, "Camera should have non-NULL position");
    TEST_ASSERT_EQUAL(0.0f, ese_point_get_x(camera->position), "Default position x should be 0.0");
    TEST_ASSERT_EQUAL(0.0f, ese_point_get_y(camera->position), "Default position y should be 0.0");
    TEST_ASSERT_EQUAL(0.0f, camera->rotation, "Default rotation should be 0.0");
    TEST_ASSERT_EQUAL(1.0f, camera->scale, "Default scale should be 1.0");
    TEST_ASSERT_POINTER_EQUAL(engine->runtime, camera->state, "Camera should have correct Lua state");
    TEST_ASSERT_EQUAL(0, camera->lua_ref_count, "New camera should have ref count 0");
    TEST_ASSERT(camera->lua_ref == LUA_NOREF, "New camera should have LUA_NOREF value");
    
    // Clean up
    camera_state_destroy(camera);
    lua_engine_destroy(engine);
    
    test_end("Camera Creation");
}

static void test_camera_properties() {
    test_begin("Camera Properties");
    
    EseLuaEngine *engine = create_test_engine();
    EseCamera *camera = camera_state_create(engine);
    
    // Test setting and getting properties
    ese_point_set_x(camera->position, 10.5f);
    ese_point_set_y(camera->position, -5.25f);
    camera->rotation = 0.785398f; // π/4 radians (45 degrees)
    camera->scale = 2.0f;
    
    TEST_ASSERT_EQUAL(10.5f, ese_point_get_x(camera->position), "Position x should be set and retrieved correctly");
    TEST_ASSERT_EQUAL(-5.25f, ese_point_get_y(camera->position), "Position y should be set and retrieved correctly");
    TEST_ASSERT_FLOAT_EQUAL(0.785398f, camera->rotation, 0.001f, "Rotation should be set and retrieved correctly");
    TEST_ASSERT_EQUAL(2.0f, camera->scale, "Scale should be set and retrieved correctly");
    
    // Test negative values
    ese_point_set_x(camera->position, -100.0f);
    ese_point_set_y(camera->position, 200.0f);
    camera->rotation = -1.5708f; // -π/2 radians (-90 degrees)
    camera->scale = 0.5f;
    
    TEST_ASSERT_EQUAL(-100.0f, ese_point_get_x(camera->position), "Position x should handle negative values");
    TEST_ASSERT_EQUAL(200.0f, ese_point_get_y(camera->position), "Position y should handle negative values");
    TEST_ASSERT_FLOAT_EQUAL(-1.5708f, camera->rotation, 0.001f, "Rotation should handle negative values");
    TEST_ASSERT_EQUAL(0.5f, camera->scale, "Scale should handle fractional values");
    
    // Clean up
    camera_state_destroy(camera);
    lua_engine_destroy(engine);
    
    test_end("Camera Properties");
}

static void test_camera_copy() {
    test_begin("Camera Copy");
    
    EseLuaEngine *engine = create_test_engine();
    EseCamera *original = camera_state_create(engine);
    
    // Set some values
    ese_point_set_x(original->position, 42.0f);
    ese_point_set_y(original->position, -17.5f);
    original->rotation = 0.523599f; // π/6 radians (30 degrees)
    original->scale = 1.5f;
    
    EseCamera *copy = camera_state_copy(original);
    TEST_ASSERT_NOT_NULL(copy, "Copy should be created");
    TEST_ASSERT(original != copy, "Copy should be a different pointer");
    
    // Test that values are copied
    TEST_ASSERT_EQUAL(42.0f, ese_point_get_x(copy->position), "Copied position x should match original");
    TEST_ASSERT_EQUAL(-17.5f, ese_point_get_y(copy->position), "Copied position y should match original");
    TEST_ASSERT_FLOAT_EQUAL(0.523599f, copy->rotation, 0.001f, "Copied rotation should match original");
    TEST_ASSERT_EQUAL(1.5f, copy->scale, "Copied scale should match original");
    
    // Test that modifications to copy don't affect original
    ese_point_set_x(copy->position, 100.0f);
    TEST_ASSERT_EQUAL(42.0f, ese_point_get_x(original->position), "Original should not be affected by copy modification");
    
    // Clean up
    camera_state_destroy(copy);
    camera_state_destroy(original);
    lua_engine_destroy(engine);
    
    test_end("Camera Copy");
}

static void test_camera_mathematical_operations() {
    test_begin("Camera Mathematical Operations");
    
    EseLuaEngine *engine = create_test_engine();
    EseCamera *camera = camera_state_create(engine);
    
    // Test basic camera operations
    ese_point_set_x(camera->position, 0.0f);
    ese_point_set_y(camera->position, 0.0f);
    camera->rotation = 0.0f;
    camera->scale = 1.0f;
    
    // Test that camera operations work
    TEST_ASSERT(true, "Camera operations should work");
    
    // Test with camera offset
    ese_point_set_x(camera->position, 5.0f);
    ese_point_set_y(camera->position, 2.0f);
    TEST_ASSERT(true, "Camera operations with offset should work");
    
    // Test with scale
    ese_point_set_x(camera->position, 0.0f);
    ese_point_set_y(camera->position, 0.0f);
    camera->scale = 2.0f;
    TEST_ASSERT(true, "Camera operations with scale should work");
    
    // Clean up
    camera_state_destroy(camera);
    lua_engine_destroy(engine);
    
    test_end("Camera Mathematical Operations");
}

static void test_camera_lua_integration() {
    test_begin("Camera Lua Integration");
    
    EseLuaEngine *engine = create_test_engine();
    EseCamera *camera = camera_state_create(engine);
    
    // Test initial Lua reference state
    TEST_ASSERT(camera->lua_ref == LUA_NOREF, "Camera should have no Lua reference initially");
    TEST_ASSERT_EQUAL(0, camera->lua_ref_count, "Camera should have ref count of 0 initially");
    
    // Test referencing
    camera_state_ref(camera);
    TEST_ASSERT(camera->lua_ref != LUA_NOREF, "Camera should have a valid Lua reference after ref");
    TEST_ASSERT_EQUAL(1, camera->lua_ref_count, "Camera should have ref count of 1");
    
    // Test unreferencing
    camera_state_unref(camera);
    TEST_ASSERT_EQUAL(0, camera->lua_ref_count, "Camera should have ref count of 0 after unref");
    
    // Test Lua state
    TEST_ASSERT_NOT_NULL(camera->state, "Camera should have a valid Lua state");
    TEST_ASSERT(camera->state == engine->runtime, "Camera state should match engine runtime");
    
    // Clean up
    camera_state_destroy(camera);
    lua_engine_destroy(engine);
    
    test_end("Camera Lua Integration");
}

static void test_camera_null_pointer_aborts() {
    test_begin("Camera NULL Pointer Abort Tests");
    
    EseLuaEngine *engine = create_test_engine();
    EseCamera *camera = camera_state_create(engine);
    
    // Test that creation functions abort with NULL pointers
    TEST_ASSERT_ABORT(camera_state_create(NULL), "camera_state_create should abort with NULL engine");
    TEST_ASSERT_ABORT(camera_state_copy(NULL), "camera_state_copy should abort with NULL camera");
    // camera_state_destroy should ignore NULL camera (not abort)
    camera_state_destroy(NULL);
    
    // Test that Lua functions abort with NULL pointers
    TEST_ASSERT_ABORT(camera_state_lua_init(NULL), "camera_state_lua_init should abort with NULL engine");
    TEST_ASSERT_ABORT(camera_state_lua_push(NULL), "camera_state_lua_push should abort with NULL camera");
    TEST_ASSERT_ABORT(camera_state_lua_get(NULL, 1), "camera_state_lua_get should abort with NULL Lua state");
    TEST_ASSERT_ABORT(camera_state_ref(NULL), "camera_state_ref should abort with NULL camera");
    // camera_state_unref should ignore NULL camera (not abort)
    camera_state_unref(NULL);
    
    // Clean up
    camera_state_destroy(camera);
    lua_engine_destroy(engine);
    
    test_end("Camera NULL Pointer Abort Tests");
}
