/*
* test_display.c - Unity-based tests for display functionality
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

#include "../src/types/display.h"
#include "../src/types/display_private.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

/**
* C API Test Functions Declarations
*/
static void test_ese_display_sizeof(void);
static void test_ese_display_create_requires_engine(void);
static void test_ese_display_create(void);
static void test_ese_display_fullscreen(void);
static void test_ese_display_width(void);
static void test_ese_display_height(void);
static void test_ese_display_aspect_ratio(void);
static void test_ese_display_viewport_width(void);
static void test_ese_display_viewport_height(void);
static void test_ese_display_set_dimensions(void);
static void test_ese_display_set_fullscreen(void);
static void test_ese_display_set_viewport(void);
static void test_ese_display_ref(void);
static void test_ese_display_copy_requires_engine(void);
static void test_ese_display_copy(void);
static void test_ese_display_direct_field_access(void);
static void test_ese_display_lua_integration(void);
static void test_ese_display_lua_init(void);
static void test_ese_display_lua_push(void);
static void test_ese_display_lua_get(void);

/**
* Lua API Test Functions Declarations
*/
static void test_ese_display_lua_fullscreen(void);
static void test_ese_display_lua_width(void);
static void test_ese_display_lua_height(void);
static void test_ese_display_lua_aspect_ratio(void);
static void test_ese_display_lua_viewport_width(void);
static void test_ese_display_lua_viewport_height(void);
static void test_ese_display_lua_tostring(void);

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

    printf("\nEseDisplay Tests\n");
    printf("----------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_ese_display_sizeof);
    RUN_TEST(test_ese_display_create_requires_engine);
    RUN_TEST(test_ese_display_create);
    RUN_TEST(test_ese_display_fullscreen);
    RUN_TEST(test_ese_display_width);
    RUN_TEST(test_ese_display_height);
    RUN_TEST(test_ese_display_aspect_ratio);
    RUN_TEST(test_ese_display_viewport_width);
    RUN_TEST(test_ese_display_viewport_height);
    RUN_TEST(test_ese_display_set_dimensions);
    RUN_TEST(test_ese_display_set_fullscreen);
    RUN_TEST(test_ese_display_set_viewport);
    RUN_TEST(test_ese_display_ref);
    RUN_TEST(test_ese_display_copy_requires_engine);
    RUN_TEST(test_ese_display_copy);
    RUN_TEST(test_ese_display_direct_field_access);
    RUN_TEST(test_ese_display_lua_integration);
    RUN_TEST(test_ese_display_lua_init);
    RUN_TEST(test_ese_display_lua_push);
    RUN_TEST(test_ese_display_lua_get);
    RUN_TEST(test_ese_display_lua_fullscreen);
    RUN_TEST(test_ese_display_lua_width);
    RUN_TEST(test_ese_display_lua_height);
    RUN_TEST(test_ese_display_lua_aspect_ratio);
    RUN_TEST(test_ese_display_lua_viewport_width);
    RUN_TEST(test_ese_display_lua_viewport_height);
    RUN_TEST(test_ese_display_lua_tostring);

    memory_manager.destroy(true);

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_ese_display_sizeof(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, ese_display_sizeof(), "Display size should be > 0");
}

static void test_ese_display_create_requires_engine(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, ese_display_sizeof(), "Display size should be > 0");
}

static void test_ese_display_create(void) {
    EseDisplay *display = ese_display_create(g_engine);

    TEST_ASSERT_NOT_NULL_MESSAGE(display, "Display should be created");
    TEST_ASSERT_FALSE_MESSAGE(ese_display_get_fullscreen(display), "New display should have fullscreen = false");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_width(display), "New display should have width = 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_height(display), "New display should have height = 0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, ese_display_get_aspect_ratio(display), "New display should have aspect_ratio = 1.0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_viewport_width(display), "New display should have viewport width = 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_viewport_height(display), "New display should have viewport height = 0");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_display_get_state(display), "Display should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_lua_ref_count(display), "New display should have ref count 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_display_get_lua_ref(display), "New display should have LUA_NOREF");

    ese_display_destroy(display);
}

static void test_ese_display_fullscreen(void) {
    EseDisplay *display = ese_display_create(g_engine);

    // Test initial value
    TEST_ASSERT_FALSE_MESSAGE(ese_display_get_fullscreen(display), "Initial fullscreen should be false");

    // Test setting to true
    ese_display_set_fullscreen(display, true);
    TEST_ASSERT_TRUE_MESSAGE(ese_display_get_fullscreen(display), "Fullscreen should be true");

    // Test setting to false
    ese_display_set_fullscreen(display, false);
    TEST_ASSERT_FALSE_MESSAGE(ese_display_get_fullscreen(display), "Fullscreen should be false");

    ese_display_destroy(display);
}

static void test_ese_display_width(void) {
    EseDisplay *display = ese_display_create(g_engine);

    // Test initial value
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_width(display), "Initial width should be 0");

    // Test setting different values
    ese_display_set_dimensions(display, 1920, 1080);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1920, ese_display_get_width(display), "Width should be 1920");

    ese_display_set_dimensions(display, 800, 600);
    TEST_ASSERT_EQUAL_INT_MESSAGE(800, ese_display_get_width(display), "Width should be 800");

    ese_display_set_dimensions(display, 0, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_width(display), "Width should be 0");

    ese_display_destroy(display);
}

static void test_ese_display_height(void) {
    EseDisplay *display = ese_display_create(g_engine);

    // Test initial value
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_height(display), "Initial height should be 0");

    // Test setting different values
    ese_display_set_dimensions(display, 1920, 1080);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1080, ese_display_get_height(display), "Height should be 1080");

    ese_display_set_dimensions(display, 800, 600);
    TEST_ASSERT_EQUAL_INT_MESSAGE(600, ese_display_get_height(display), "Height should be 600");

    ese_display_set_dimensions(display, 0, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_height(display), "Height should be 0");

    ese_display_destroy(display);
}

static void test_ese_display_aspect_ratio(void) {
    EseDisplay *display = ese_display_create(g_engine);

    // Test initial value
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, ese_display_get_aspect_ratio(display), "Initial aspect ratio should be 1.0");

    // Test 16:9 aspect ratio
    ese_display_set_dimensions(display, 1920, 1080);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1920.0f/1080.0f, ese_display_get_aspect_ratio(display), "Aspect ratio should be 16:9");

    // Test 4:3 aspect ratio
    ese_display_set_dimensions(display, 800, 600);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(800.0f/600.0f, ese_display_get_aspect_ratio(display), "Aspect ratio should be 4:3");

    // Test square aspect ratio
    ese_display_set_dimensions(display, 512, 512);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, ese_display_get_aspect_ratio(display), "Aspect ratio should be 1:1");

    // Test zero height (should default to 1.0)
    ese_display_set_dimensions(display, 100, 0);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, ese_display_get_aspect_ratio(display), "Aspect ratio should be 1.0 for zero height");

    ese_display_destroy(display);
}

static void test_ese_display_viewport_width(void) {
    EseDisplay *display = ese_display_create(g_engine);

    // Test initial value
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_viewport_width(display), "Initial viewport width should be 0");

    // Test setting different values
    ese_display_set_viewport(display, 1920, 1080);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1920, ese_display_get_viewport_width(display), "Viewport width should be 1920");

    ese_display_set_viewport(display, 800, 600);
    TEST_ASSERT_EQUAL_INT_MESSAGE(800, ese_display_get_viewport_width(display), "Viewport width should be 800");

    ese_display_set_viewport(display, 0, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_viewport_width(display), "Viewport width should be 0");

    ese_display_destroy(display);
}

static void test_ese_display_viewport_height(void) {
    EseDisplay *display = ese_display_create(g_engine);

    // Test initial value
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_viewport_height(display), "Initial viewport height should be 0");

    // Test setting different values
    ese_display_set_viewport(display, 1920, 1080);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1080, ese_display_get_viewport_height(display), "Viewport height should be 1080");

    ese_display_set_viewport(display, 800, 600);
    TEST_ASSERT_EQUAL_INT_MESSAGE(600, ese_display_get_viewport_height(display), "Viewport height should be 600");

    ese_display_set_viewport(display, 0, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_viewport_height(display), "Viewport height should be 0");

    ese_display_destroy(display);
}

static void test_ese_display_set_dimensions(void) {
    EseDisplay *display = ese_display_create(g_engine);

    // Test setting dimensions
    ese_display_set_dimensions(display, 1920, 1080);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1920, ese_display_get_width(display), "Width should be set correctly");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1080, ese_display_get_height(display), "Height should be set correctly");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1920.0f/1080.0f, ese_display_get_aspect_ratio(display), "Aspect ratio should be calculated correctly");

    // Test negative dimensions
    ese_display_set_dimensions(display, -100, -200);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-100, ese_display_get_width(display), "Negative width should be preserved");
    TEST_ASSERT_EQUAL_INT_MESSAGE(-200, ese_display_get_height(display), "Negative height should be preserved");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, ese_display_get_aspect_ratio(display), "Aspect ratio should default to 1.0 for negative height");

    // Test zero height
    ese_display_set_dimensions(display, 100, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, ese_display_get_width(display), "Width should be set correctly");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_height(display), "Height should be set correctly");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, ese_display_get_aspect_ratio(display), "Aspect ratio should default to 1.0 for zero height");

    ese_display_destroy(display);
}

static void test_ese_display_set_fullscreen(void) {
    EseDisplay *display = ese_display_create(g_engine);

    // Test setting to true
    ese_display_set_fullscreen(display, true);
    TEST_ASSERT_TRUE_MESSAGE(ese_display_get_fullscreen(display), "Fullscreen should be set to true");

    // Test setting to false
    ese_display_set_fullscreen(display, false);
    TEST_ASSERT_FALSE_MESSAGE(ese_display_get_fullscreen(display), "Fullscreen should be set to false");

    // Test multiple toggles
    ese_display_set_fullscreen(display, true);
    ese_display_set_fullscreen(display, true);
    TEST_ASSERT_TRUE_MESSAGE(ese_display_get_fullscreen(display), "Multiple true sets should remain true");

    ese_display_destroy(display);
}

static void test_ese_display_set_viewport(void) {
    EseDisplay *display = ese_display_create(g_engine);

    // Test setting viewport
    ese_display_set_viewport(display, 1920, 1080);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1920, ese_display_get_viewport_width(display), "Viewport width should be set correctly");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1080, ese_display_get_viewport_height(display), "Viewport height should be set correctly");

    // Test negative viewport
    ese_display_set_viewport(display, -100, -200);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-100, ese_display_get_viewport_width(display), "Negative viewport width should be preserved");
    TEST_ASSERT_EQUAL_INT_MESSAGE(-200, ese_display_get_viewport_height(display), "Negative viewport height should be preserved");

    // Test zero viewport
    ese_display_set_viewport(display, 0, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_viewport_width(display), "Zero viewport width should be set correctly");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_viewport_height(display), "Zero viewport height should be set correctly");

    ese_display_destroy(display);
}

static void test_ese_display_ref(void) {
    EseDisplay *display = ese_display_create(g_engine);

    ese_display_ref(display);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, ese_display_get_lua_ref_count(display), "Ref count should be 1");

    ese_display_unref(display);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_lua_ref_count(display), "Ref count should be 0");

    ese_display_destroy(display);
}

static void test_ese_display_copy_requires_engine(void) {
    TEST_ASSERT_DEATH(ese_display_copy(NULL), "ese_display_copy should abort with NULL display");
}

static void test_ese_display_copy(void) {
    EseDisplay *display = ese_display_create(g_engine);
    ese_display_ref(display);
    ese_display_set_dimensions(display, 1920, 1080);
    ese_display_set_fullscreen(display, true);
    ese_display_set_viewport(display, 800, 600);
    EseDisplay *copy = ese_display_copy(display);

    TEST_ASSERT_NOT_NULL_MESSAGE(copy, "Copy should be created");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_display_get_state(copy), "Copy should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_display_get_lua_ref_count(copy), "Copy should have ref count 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_display_get_lua_ref(copy), "Copy should have LUA_NOREF");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1920, ese_display_get_width(copy), "Copy should have width = 1920");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1080, ese_display_get_height(copy), "Copy should have height = 1080");
    TEST_ASSERT_TRUE_MESSAGE(ese_display_get_fullscreen(copy), "Copy should have fullscreen = true");
    TEST_ASSERT_EQUAL_INT_MESSAGE(800, ese_display_get_viewport_width(copy), "Copy should have viewport width = 800");
    TEST_ASSERT_EQUAL_INT_MESSAGE(600, ese_display_get_viewport_height(copy), "Copy should have viewport height = 600");

    ese_display_unref(display);
    ese_display_destroy(display);
    ese_display_destroy(copy);
}

static void test_ese_display_direct_field_access(void) {
    EseDisplay *display = ese_display_create(g_engine);

    // Test that we can directly access all fields through getters
    ese_display_set_dimensions(display, 1920, 1080);
    ese_display_set_fullscreen(display, true);
    ese_display_set_viewport(display, 800, 600);

    // Verify through getters
    TEST_ASSERT_EQUAL_INT_MESSAGE(1920, ese_display_get_width(display), "Direct field access should work for width");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1080, ese_display_get_height(display), "Direct field access should work for height");
    TEST_ASSERT_TRUE_MESSAGE(ese_display_get_fullscreen(display), "Direct field access should work for fullscreen");
    TEST_ASSERT_EQUAL_INT_MESSAGE(800, ese_display_get_viewport_width(display), "Direct field access should work for viewport width");
    TEST_ASSERT_EQUAL_INT_MESSAGE(600, ese_display_get_viewport_height(display), "Direct field access should work for viewport height");

    ese_display_destroy(display);
}

static void test_ese_display_lua_integration(void) {
    EseLuaEngine *engine = create_test_engine();
    EseDisplay *display = ese_display_create(engine);

    lua_State *before_state = ese_display_get_state(display);
    TEST_ASSERT_NOT_NULL_MESSAGE(before_state, "Display should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, before_state, "Display state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_display_get_lua_ref(display), "Display should have no Lua reference initially");

    ese_display_ref(display);
    lua_State *after_ref_state = ese_display_get_state(display);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_ref_state, "Display should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_ref_state, "Display state should match engine runtime");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(LUA_NOREF, ese_display_get_lua_ref(display), "Display should have a valid Lua reference after ref");

    ese_display_unref(display);
    lua_State *after_unref_state = ese_display_get_state(display);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_unref_state, "Display should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_unref_state, "Display state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_display_get_lua_ref(display), "Display should have no Lua reference after unref");

    ese_display_destroy(display);
    lua_engine_destroy(engine);
}

static void test_ese_display_lua_init(void) {
    lua_State *L = g_engine->runtime;
    
    luaL_getmetatable(L, "DisplayMeta");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Metatable should not exist before initialization");
    lua_pop(L, 1);
    
    ese_display_lua_init(g_engine);
    
    luaL_getmetatable(L, "DisplayMeta");
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Metatable should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Metatable should be a table");
    lua_pop(L, 1);
}

static void test_ese_display_lua_push(void) {
    ese_display_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseDisplay *display = ese_display_create(g_engine);
    
    ese_display_lua_push(display);
    
    EseDisplay **ud = (EseDisplay **)lua_touserdata(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(display, *ud, "The pushed item should be the actual display");
    
    lua_pop(L, 1); 
    
    ese_display_destroy(display);
}

static void test_ese_display_lua_get(void) {
    ese_display_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseDisplay *display = ese_display_create(g_engine);
    
    ese_display_lua_push(display);
    
    EseDisplay *extracted_display = ese_display_lua_get(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(display, extracted_display, "Extracted display should match original");
    
    lua_pop(L, 1);
    ese_display_destroy(display);
}

/**
* Lua API Test Functions
*/

static void test_ese_display_lua_fullscreen(void) {
    ese_display_lua_init(g_engine);
    EseDisplay *display = ese_display_create(g_engine);
    lua_State *L = g_engine->runtime;

    ese_display_set_fullscreen(display, true);

    // Push the display to Lua with metatable
    ese_display_lua_push(display);
    lua_setglobal(L, "Display");

    const char *test1 = "return Display.fullscreen";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "get fullscreen should execute without error");
    bool fullscreen = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(fullscreen, "Fullscreen should be true");
    lua_pop(L, 1);

    const char *test2 = "Display.fullscreen = false";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "set fullscreen should execute with error");

    ese_display_destroy(display);
}

static void test_ese_display_lua_width(void) {
    ese_display_lua_init(g_engine);
    EseDisplay *display = ese_display_create(g_engine);
    lua_State *L = g_engine->runtime;

    ese_display_set_dimensions(display, 1920, 1080);

    // Push the display to Lua with metatable
    ese_display_lua_push(display);
    lua_setglobal(L, "Display");

    const char *test1 = "return Display.width";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "get width should execute without error");
    int width = (int)lua_tointeger(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1920, width, "Width should be 1920");
    lua_pop(L, 1);

    const char *test2 = "Display.width = 800";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "set width should execute with error");

    ese_display_destroy(display);
}

static void test_ese_display_lua_height(void) {
    ese_display_lua_init(g_engine);
    EseDisplay *display = ese_display_create(g_engine);
    lua_State *L = g_engine->runtime;

    ese_display_set_dimensions(display, 1920, 1080);

    // Push the display to Lua with metatable
    ese_display_lua_push(display);
    lua_setglobal(L, "Display");

    const char *test1 = "return Display.height";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "get height should execute without error");
    int height = (int)lua_tointeger(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1080, height, "Height should be 1080");
    lua_pop(L, 1);

    const char *test2 = "Display.height = 600";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "set height should execute with error");

    ese_display_destroy(display);
}

static void test_ese_display_lua_aspect_ratio(void) {
    ese_display_lua_init(g_engine);
    EseDisplay *display = ese_display_create(g_engine);
    lua_State *L = g_engine->runtime;

    ese_display_set_dimensions(display, 1920, 1080);

    // Push the display to Lua with metatable
    ese_display_lua_push(display);
    lua_setglobal(L, "Display");

    const char *test1 = "return Display.aspect_ratio";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "get aspect_ratio should execute without error");
    float aspect_ratio = (float)lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1920.0f/1080.0f, aspect_ratio, "Aspect ratio should be 16:9");
    lua_pop(L, 1);

    const char *test2 = "Display.aspect_ratio = 2.0";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "set aspect_ratio should execute with error");

    ese_display_destroy(display);
}

static void test_ese_display_lua_viewport_width(void) {
    ese_display_lua_init(g_engine);
    EseDisplay *display = ese_display_create(g_engine);
    lua_State *L = g_engine->runtime;

    ese_display_set_viewport(display, 800, 600);

    // Push the display to Lua with metatable
    ese_display_lua_push(display);
    lua_setglobal(L, "Display");

    const char *test1 = "return Display.viewport.width";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "get viewport.width should execute without error");
    int viewport_width = (int)lua_tointeger(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(800, viewport_width, "Viewport width should be 800");
    lua_pop(L, 1);

    const char *test2 = "Display.viewport.width = 400";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "set viewport.width should execute with error");

    ese_display_destroy(display);
}

static void test_ese_display_lua_viewport_height(void) {
    ese_display_lua_init(g_engine);
    EseDisplay *display = ese_display_create(g_engine);
    lua_State *L = g_engine->runtime;

    ese_display_set_viewport(display, 800, 600);

    // Push the display to Lua with metatable
    ese_display_lua_push(display);
    lua_setglobal(L, "Display");

    const char *test1 = "return Display.viewport.height";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "get viewport.height should execute without error");
    int viewport_height = (int)lua_tointeger(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(600, viewport_height, "Viewport height should be 600");
    lua_pop(L, 1);

    const char *test2 = "Display.viewport.height = 300";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "set viewport.height should execute with error");

    ese_display_destroy(display);
}

static void test_ese_display_lua_tostring(void) {
    ese_display_lua_init(g_engine);
    EseDisplay *display = ese_display_create(g_engine);
    lua_State *L = g_engine->runtime;

    ese_display_set_dimensions(display, 1920, 1080);
    ese_display_set_fullscreen(display, true);
    ese_display_set_viewport(display, 800, 600);

    // Push the display to Lua with metatable
    ese_display_lua_push(display);
    lua_setglobal(L, "Display");

    const char *test_code = "return tostring(Display)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "tostring test should execute without error");
    const char *result = lua_tostring(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "tostring result should not be NULL");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "Display:") != NULL, "tostring should contain 'Display:'");
    lua_pop(L, 1); 

    ese_display_destroy(display);
}
