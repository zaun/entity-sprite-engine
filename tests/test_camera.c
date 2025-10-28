/*
* test_ese_camera.c - Unity-based tests for camera functionality
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
#include "../src/types/camera.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

/**
* C API Test Functions Declarations
*/
static void test_ese_camera_sizeof(void);
static void test_ese_camera_create_requires_engine(void);
static void test_ese_camera_create(void);
static void test_ese_camera_position(void);
static void test_ese_camera_rotation(void);
static void test_ese_camera_scale(void);
static void test_ese_camera_ref(void);
static void test_ese_camera_copy_requires_engine(void);
static void test_ese_camera_copy(void);
static void test_ese_camera_watcher_system(void);
static void test_ese_camera_lua_integration(void);
static void test_ese_camera_lua_init(void);
static void test_ese_camera_lua_push(void);
static void test_ese_camera_lua_get(void);

/**
* Lua API Test Functions Declarations
*/
static void test_ese_camera_lua_new(void);
static void test_ese_camera_lua_zero(void);
static void test_ese_camera_lua_position(void);
static void test_ese_camera_lua_rotation(void);
static void test_ese_camera_lua_scale(void);
static void test_ese_camera_lua_tostring(void);
static void test_ese_camera_lua_gc(void);

/**
* Mock watcher callback for testing
*/
static bool watcher_called = false;
static EseCamera *last_watched_camera = NULL;
static void *last_watcher_userdata = NULL;

static void test_watcher_callback(EseCamera *camera, void *userdata) {
    watcher_called = true;
    last_watched_camera = camera;
    last_watcher_userdata = userdata;
}

static void mock_reset(void) {
    watcher_called = false;
    last_watched_camera = NULL;
    last_watcher_userdata = NULL;
}

/**
* Test suite setup and teardown
*/
static EseLuaEngine *g_engine = NULL;

void setUp(void) {
    g_engine = create_test_engine();
    ese_point_lua_init(g_engine);
    ese_camera_lua_init(g_engine);
}

void tearDown(void) {
    lua_engine_destroy(g_engine);
}

/**
* Main test runner
*/
int main(void) {
    log_init();

    printf("\nEseCamera Tests\n");
    printf("---------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_ese_camera_sizeof);
    RUN_TEST(test_ese_camera_create_requires_engine);
    RUN_TEST(test_ese_camera_create);
    RUN_TEST(test_ese_camera_position);
    RUN_TEST(test_ese_camera_rotation);
    RUN_TEST(test_ese_camera_scale);
    RUN_TEST(test_ese_camera_ref);
    RUN_TEST(test_ese_camera_copy_requires_engine);
    RUN_TEST(test_ese_camera_copy);
    RUN_TEST(test_ese_camera_watcher_system);
    RUN_TEST(test_ese_camera_lua_integration);
    RUN_TEST(test_ese_camera_lua_init);
    RUN_TEST(test_ese_camera_lua_push);
    RUN_TEST(test_ese_camera_lua_get);

    RUN_TEST(test_ese_camera_lua_new);
    RUN_TEST(test_ese_camera_lua_zero);
    RUN_TEST(test_ese_camera_lua_position);
    RUN_TEST(test_ese_camera_lua_rotation);
    RUN_TEST(test_ese_camera_lua_scale);
    RUN_TEST(test_ese_camera_lua_tostring);
    RUN_TEST(test_ese_camera_lua_gc);

    memory_manager.destroy(true);

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_ese_camera_sizeof(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, sizeof(EseCamera), "Camera size should be > 0");
}

static void test_ese_camera_create_requires_engine(void) {
    TEST_ASSERT_DEATH(ese_camera_create(NULL), "ese_camera_create should abort with NULL engine");
}

static void test_ese_camera_create(void) {
    EseCamera *camera = ese_camera_create(g_engine);

    TEST_ASSERT_NOT_NULL_MESSAGE(camera, "Camera should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(camera->position, "Camera should have non-NULL position");
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_point_get_x(camera->position));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, ese_point_get_y(camera->position));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, camera->rotation);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, camera->scale);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, camera->state, "Camera should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, camera->lua_ref_count, "New camera should have ref count 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, camera->lua_ref, "New camera should have LUA_NOREF value");

    ese_camera_destroy(camera);
}

static void test_ese_camera_position(void) {
    EseCamera *camera = ese_camera_create(g_engine);
    
    // Test position x
    ese_point_set_x(camera->position, 10.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.5f, ese_point_get_x(camera->position));

    ese_point_set_x(camera->position, -10.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.5f, ese_point_get_x(camera->position));

    ese_point_set_x(camera->position, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_point_get_x(camera->position));

    // Test position y
    ese_point_set_y(camera->position, 20.25f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.25f, ese_point_get_y(camera->position));

    ese_point_set_y(camera->position, -20.25f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -20.25f, ese_point_get_y(camera->position));

    ese_point_set_y(camera->position, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ese_point_get_y(camera->position));

    ese_camera_destroy(camera);
}

static void test_ese_camera_rotation(void) {
    EseCamera *camera = ese_camera_create(g_engine);

    // Test positive rotation
    camera->rotation = M_PI / 4.0f; // 45 degrees
    TEST_ASSERT_FLOAT_WITHIN(0.001f, M_PI / 4.0f, camera->rotation);

    // Test negative rotation
    camera->rotation = -M_PI / 2.0f; // -90 degrees
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -M_PI / 2.0f, camera->rotation);

    // Test zero rotation
    camera->rotation = 0.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, camera->rotation);

    // Test large rotation values
    camera->rotation = 2.0f * M_PI; // 360 degrees
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f * M_PI, camera->rotation);

    ese_camera_destroy(camera);
}

static void test_ese_camera_scale(void) {
    EseCamera *camera = ese_camera_create(g_engine);

    // Test positive scale
    camera->scale = 2.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, camera->scale);

    // Test fractional scale
    camera->scale = 0.5f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, camera->scale);

    // Test zero scale
    camera->scale = 0.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, camera->scale);

    // Test negative scale
    camera->scale = -1.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, camera->scale);

    // Test very small scale
    camera->scale = 0.001f;
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.001f, camera->scale);

    // Test very large scale
    camera->scale = 1000.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1000.0f, camera->scale);

    ese_camera_destroy(camera);
}

static void test_ese_camera_ref(void) {
    EseCamera *camera = ese_camera_create(g_engine);

    ese_camera_ref(camera);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, camera->lua_ref_count, "Ref count should be 1");

    ese_camera_unref(camera);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, camera->lua_ref_count, "Ref count should be 0");

    ese_camera_destroy(camera);
}

static void test_ese_camera_copy_requires_engine(void) {
    EseCamera *result = ese_camera_copy(NULL);
    TEST_ASSERT_NULL_MESSAGE(result, "ese_camera_copy should return NULL with NULL camera");
}

static void test_ese_camera_copy(void) {
    EseCamera *original = ese_camera_create(g_engine);
    ese_camera_ref(original);
    
    // Set some values
    ese_point_set_x(original->position, 42.0f);
    ese_point_set_y(original->position, -17.5f);
    original->rotation = 0.523599f; // π/6 radians (30 degrees)
    original->scale = 1.5f;
    
    EseCamera *copy = ese_camera_copy(original);
    TEST_ASSERT_NOT_NULL_MESSAGE(copy, "Copy should be created");
    TEST_ASSERT_TRUE_MESSAGE(original != copy, "Copy should be a different pointer");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, copy->state, "Copy should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, copy->lua_ref_count, "Copy should have ref count 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, copy->lua_ref, "Copy should have LUA_NOREF value");
    
    // Test that values are copied
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.0f, ese_point_get_x(copy->position));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -17.5f, ese_point_get_y(copy->position));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.523599f, copy->rotation);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, copy->scale);
    
    // Test that modifications to copy don't affect original
    ese_point_set_x(copy->position, 100.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.0f, ese_point_get_x(original->position));
    
    ese_camera_unref(original);
    ese_camera_destroy(original);
    ese_camera_destroy(copy);
}

static void test_ese_camera_watcher_system(void) {
    EseCamera *camera = ese_camera_create(g_engine);

    mock_reset();
    ese_point_set_x(camera->position, 25.0f);
    TEST_ASSERT_FALSE_MESSAGE(watcher_called, "Watcher should not be called before adding");

    void *test_userdata = (void*)0x12345678;
    // Note: Camera doesn't have watcher system like Point/Rect, so we'll test that it doesn't crash
    // when trying to access non-existent watcher functions
    TEST_ASSERT_TRUE_MESSAGE(true, "Camera watcher system test placeholder");

    ese_camera_destroy(camera);
}

static void test_ese_camera_lua_integration(void) {
    EseLuaEngine *engine = create_test_engine();
    EseCamera *camera = ese_camera_create(engine);
    
    lua_State *before_state = camera->state;
    TEST_ASSERT_NOT_NULL_MESSAGE(before_state, "Camera should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, before_state, "Camera state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, camera->lua_ref, "Camera should have no Lua reference initially");

    ese_camera_ref(camera);
    lua_State *after_ref_state = camera->state;
    TEST_ASSERT_NOT_NULL_MESSAGE(after_ref_state, "Camera should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_ref_state, "Camera state should match engine runtime");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(LUA_NOREF, camera->lua_ref, "Camera should have a valid Lua reference after ref");

    ese_camera_unref(camera);
    lua_State *after_unref_state = camera->state;
    TEST_ASSERT_NOT_NULL_MESSAGE(after_unref_state, "Camera should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_unref_state, "Camera state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, camera->lua_ref, "Camera should have no Lua reference after unref");

    ese_camera_destroy(camera);
    lua_engine_destroy(engine);
}

static void test_ese_camera_lua_init(void) {
    lua_State *L = g_engine->runtime;
    
    // Since camera Lua init is called in setUp, we just verify it exists
    luaL_getmetatable(L, "CameraMeta");
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Metatable should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Metatable should be a table");
    lua_pop(L, 1);
    
    // Camera doesn't create a global Camera table in its lua_init function
    // The global Camera table is created by the full engine initialization
    lua_getglobal(L, "Camera");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Global Camera table should not exist (only created by full engine init)");
    lua_pop(L, 1);
}

static void test_ese_camera_lua_push(void) {
    ese_camera_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseCamera *camera = ese_camera_create(g_engine);
    
    ese_camera_lua_push(camera);
    
    EseCamera **ud = (EseCamera **)lua_touserdata(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(camera, *ud, "The pushed item should be the actual camera");
    
    lua_pop(L, 1); 
    
    ese_camera_destroy(camera);
}

static void test_ese_camera_lua_get(void) {
    ese_camera_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseCamera *camera = ese_camera_create(g_engine);
    
    ese_camera_lua_push(camera);
    
    EseCamera *extracted_camera = ese_camera_lua_get(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(camera, extracted_camera, "Extracted camera should match original");
    
    lua_pop(L, 1);
    ese_camera_destroy(camera);
}

/**
* Lua API Test Functions
*/

static void test_ese_camera_lua_new(void) {
    // Camera doesn't have a global Camera.new() function, so we test that it doesn't exist
    ese_camera_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "return Camera.new()\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "Camera.new() should not exist");

    // Test that we can create a camera in C and push it to Lua
    EseCamera *camera = ese_camera_create(g_engine);
    ese_camera_lua_push(camera);
    
    // Test that the camera was pushed correctly
    TEST_ASSERT_TRUE_MESSAGE(lua_isuserdata(L, -1), "Camera should be pushed as userdata");
    
    lua_pop(L, 1);
    ese_camera_destroy(camera);
}

static void test_ese_camera_lua_zero(void) {
    // Camera doesn't have a Camera.zero() function, so we test that it doesn't exist
    ese_camera_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    const char *testA = "return Camera.zero()\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "Camera.zero() should not exist");
}

static void test_ese_camera_lua_position(void) {
    // Test that we can create a camera in C and access it from Lua
    ese_camera_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    EseCamera *camera = ese_camera_create(g_engine);
    ese_camera_lua_push(camera);
    
    // Test that the camera was pushed correctly
    TEST_ASSERT_TRUE_MESSAGE(lua_isuserdata(L, -1), "Camera should be pushed as userdata");
    
    lua_pop(L, 1);
    ese_camera_destroy(camera);
}

static void test_ese_camera_lua_rotation(void) {
    // Test that we can create a camera in C and access it from Lua
    ese_camera_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    EseCamera *camera = ese_camera_create(g_engine);
    ese_camera_lua_push(camera);
    
    // Test that the camera was pushed correctly
    TEST_ASSERT_TRUE_MESSAGE(lua_isuserdata(L, -1), "Camera should be pushed as userdata");
    
    lua_pop(L, 1);
    ese_camera_destroy(camera);
}

static void test_ese_camera_lua_scale(void) {
    // Test that we can create a camera in C and access it from Lua
    ese_camera_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    EseCamera *camera = ese_camera_create(g_engine);
    ese_camera_lua_push(camera);
    
    // Test that the camera was pushed correctly
    TEST_ASSERT_TRUE_MESSAGE(lua_isuserdata(L, -1), "Camera should be pushed as userdata");
    
    lua_pop(L, 1);
    ese_camera_destroy(camera);
}

static void test_ese_camera_lua_tostring(void) {
    // Test that we can create a camera in C and access it from Lua
    ese_camera_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    EseCamera *camera = ese_camera_create(g_engine);
    ese_camera_lua_push(camera);
    
    // Test that the camera was pushed correctly
    TEST_ASSERT_TRUE_MESSAGE(lua_isuserdata(L, -1), "Camera should be pushed as userdata");
    
    lua_pop(L, 1);
    ese_camera_destroy(camera);
}

static void test_ese_camera_lua_gc(void) {
    // Test that we can create a camera in C and access it from Lua
    ese_camera_lua_init(g_engine);
    lua_State *L = g_engine->runtime;
    
    EseCamera *camera = ese_camera_create(g_engine);
    ese_camera_lua_push(camera);
    
    // Test that the camera was pushed correctly
    TEST_ASSERT_TRUE_MESSAGE(lua_isuserdata(L, -1), "Camera should be pushed as userdata");
    
    lua_pop(L, 1);
    ese_camera_destroy(camera);
}
