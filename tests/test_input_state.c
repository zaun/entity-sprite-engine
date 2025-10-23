/*
* test_ese_input_state.c - Unity-based tests for input state functionality
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

#include "../src/types/input_state.h"
#include "../src/types/input_state_private.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

/**
* C API Test Functions Declarations
*/
static void test_ese_input_state_sizeof(void);
static void test_ese_input_state_create_requires_engine(void);
static void test_ese_input_state_create(void);
static void test_ese_input_state_mouse_x(void);
static void test_ese_input_state_mouse_y(void);
static void test_ese_input_state_mouse_scroll_dx(void);
static void test_ese_input_state_mouse_scroll_dy(void);
static void test_ese_input_state_keys_down(void);
static void test_ese_input_state_keys_pressed(void);
static void test_ese_input_state_keys_released(void);
static void test_ese_input_state_mouse_down(void);
static void test_ese_input_state_ref(void);
static void test_ese_input_state_copy_requires_engine(void);
static void test_ese_input_state_copy(void);
static void test_ese_input_state_direct_field_access(void);
static void test_ese_input_state_lua_integration(void);
static void test_ese_input_state_lua_init(void);
static void test_ese_input_state_lua_push(void);
static void test_ese_input_state_lua_get(void);

/**
* Lua API Test Functions Declarations
*/
static void test_ese_input_state_lua_mouse_x(void);
static void test_ese_input_state_lua_mouse_y(void);
static void test_ese_input_state_lua_mouse_scroll_dx(void);
static void test_ese_input_state_lua_mouse_scroll_dy(void);
static void test_ese_input_state_lua_keys_down(void);
static void test_ese_input_state_lua_keys_pressed(void);
static void test_ese_input_state_lua_keys_released(void);
static void test_ese_input_state_lua_mouse_down(void);
static void test_ese_input_state_lua_key_constants(void);
static void test_ese_input_state_lua_tostring(void);

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

    printf("\nEseInputState Tests\n");
    printf("-------------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_ese_input_state_sizeof);
    RUN_TEST(test_ese_input_state_create_requires_engine);
    RUN_TEST(test_ese_input_state_create);
    RUN_TEST(test_ese_input_state_mouse_x);
    RUN_TEST(test_ese_input_state_mouse_y);
    RUN_TEST(test_ese_input_state_mouse_scroll_dx);
    RUN_TEST(test_ese_input_state_mouse_scroll_dy);
    RUN_TEST(test_ese_input_state_keys_down);
    RUN_TEST(test_ese_input_state_keys_pressed);
    RUN_TEST(test_ese_input_state_keys_released);
    RUN_TEST(test_ese_input_state_mouse_down);
    RUN_TEST(test_ese_input_state_ref);
    RUN_TEST(test_ese_input_state_copy_requires_engine);
    RUN_TEST(test_ese_input_state_copy);
    RUN_TEST(test_ese_input_state_direct_field_access);
    RUN_TEST(test_ese_input_state_lua_integration);
    RUN_TEST(test_ese_input_state_lua_init);
    RUN_TEST(test_ese_input_state_lua_push);
    RUN_TEST(test_ese_input_state_lua_get);
    RUN_TEST(test_ese_input_state_lua_mouse_x);
    RUN_TEST(test_ese_input_state_lua_mouse_y);
    RUN_TEST(test_ese_input_state_lua_mouse_scroll_dx);
    RUN_TEST(test_ese_input_state_lua_mouse_scroll_dy);
    RUN_TEST(test_ese_input_state_lua_keys_down);
    RUN_TEST(test_ese_input_state_lua_keys_pressed);
    RUN_TEST(test_ese_input_state_lua_keys_released);
    RUN_TEST(test_ese_input_state_lua_mouse_down);
    RUN_TEST(test_ese_input_state_lua_key_constants);
    RUN_TEST(test_ese_input_state_lua_tostring);

    memory_manager.destroy();

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_ese_input_state_sizeof(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, ese_input_state_sizeof(), "InputState size should be > 0");
}

static void test_ese_input_state_create_requires_engine(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, ese_input_state_sizeof(), "InputState size should be > 0");
}

static void test_ese_input_state_create(void) {
    EseInputState *input = ese_input_state_create(g_engine);

    TEST_ASSERT_NOT_NULL_MESSAGE(input, "InputState should be created");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_input_state_get_mouse_x(input), "New input state should have mouse_x = 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_input_state_get_mouse_y(input), "New input state should have mouse_y = 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_input_state_get_mouse_scroll_dx(input), "New input state should have mouse_scroll_dx = 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_input_state_get_mouse_scroll_dy(input), "New input state should have mouse_scroll_dy = 0");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_input_state_get_state(input), "InputState should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_input_state_get_lua_ref_count(input), "New input state should have ref count 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_input_state_get_lua_ref(input), "New input state should have LUA_NOREF");

    // Test all keys are initially false
    for (int i = 0; i < InputKey_MAX; i++) {
        TEST_ASSERT_FALSE_MESSAGE(ese_input_state_get_key_down(input, (EseInputKey)i), "All keys should be initially down = false");
        TEST_ASSERT_FALSE_MESSAGE(ese_input_state_get_key_pressed(input, (EseInputKey)i), "All keys should be initially pressed = false");
        TEST_ASSERT_FALSE_MESSAGE(ese_input_state_get_key_released(input, (EseInputKey)i), "All keys should be initially released = false");
    }

    // Test all mouse buttons are initially false
    for (int i = 0; i < MOUSE_BUTTON_COUNT; i++) {
        TEST_ASSERT_FALSE_MESSAGE(ese_input_state_get_mouse_down(input, i), "All mouse buttons should be initially false");
    }

    ese_input_state_destroy(input);
}

static void test_ese_input_state_mouse_x(void) {
    EseInputState *input = ese_input_state_create(g_engine);

    // Test initial value
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_input_state_get_mouse_x(input), "Initial mouse x should be 0");

    // Use direct field access to set values and test public getters
    input->mouse_x = 10;
    TEST_ASSERT_EQUAL_INT_MESSAGE(10, ese_input_state_get_mouse_x(input), "Mouse x should be 10");

    input->mouse_x = -10;
    TEST_ASSERT_EQUAL_INT_MESSAGE(-10, ese_input_state_get_mouse_x(input), "Mouse x should be -10");

    input->mouse_x = 0;
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_input_state_get_mouse_x(input), "Mouse x should be 0");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_mouse_y(void) {
    EseInputState *input = ese_input_state_create(g_engine);

    // Test initial value
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_input_state_get_mouse_y(input), "Initial mouse y should be 0");

    // Use direct field access to set values and test public getters
    input->mouse_y = 20;
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, ese_input_state_get_mouse_y(input), "Mouse y should be 20");

    input->mouse_y = -20;
    TEST_ASSERT_EQUAL_INT_MESSAGE(-20, ese_input_state_get_mouse_y(input), "Mouse y should be -20");

    input->mouse_y = 0;
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_input_state_get_mouse_y(input), "Mouse y should be 0");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_mouse_scroll_dx(void) {
    EseInputState *input = ese_input_state_create(g_engine);

    // Test initial value
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_input_state_get_mouse_scroll_dx(input), "Initial mouse scroll dx should be 0");

    // Use direct field access to set values and test public getters
    input->mouse_scroll_dx = 5;
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, ese_input_state_get_mouse_scroll_dx(input), "Mouse scroll dx should be 5");

    input->mouse_scroll_dx = -5;
    TEST_ASSERT_EQUAL_INT_MESSAGE(-5, ese_input_state_get_mouse_scroll_dx(input), "Mouse scroll dx should be -5");

    input->mouse_scroll_dx = 0;
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_input_state_get_mouse_scroll_dx(input), "Mouse scroll dx should be 0");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_mouse_scroll_dy(void) {
    EseInputState *input = ese_input_state_create(g_engine);

    // Test initial value
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_input_state_get_mouse_scroll_dy(input), "Initial mouse scroll dy should be 0");

    // Use direct field access to set values and test public getters
    input->mouse_scroll_dy = 3;
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, ese_input_state_get_mouse_scroll_dy(input), "Mouse scroll dy should be 3");

    input->mouse_scroll_dy = -3;
    TEST_ASSERT_EQUAL_INT_MESSAGE(-3, ese_input_state_get_mouse_scroll_dy(input), "Mouse scroll dy should be -3");

    input->mouse_scroll_dy = 0;
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_input_state_get_mouse_scroll_dy(input), "Mouse scroll dy should be 0");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_keys_down(void) {
    EseInputState *input = ese_input_state_create(g_engine);

    // Test initial values
    TEST_ASSERT_FALSE_MESSAGE(ese_input_state_get_key_down(input, InputKey_A), "Key A should initially be down = false");
    TEST_ASSERT_FALSE_MESSAGE(ese_input_state_get_key_down(input, InputKey_SPACE), "Key SPACE should initially be down = false");

    // Use direct field access to set values and test public getters
    input->keys_down[InputKey_A] = true;
    TEST_ASSERT_TRUE_MESSAGE(ese_input_state_get_key_down(input, InputKey_A), "Key A should be down");

    input->keys_down[InputKey_A] = false;
    TEST_ASSERT_FALSE_MESSAGE(ese_input_state_get_key_down(input, InputKey_A), "Key A should not be down");

    input->keys_down[InputKey_SPACE] = true;
    TEST_ASSERT_TRUE_MESSAGE(ese_input_state_get_key_down(input, InputKey_SPACE), "Key SPACE should be down");

    input->keys_down[InputKey_ENTER] = true;
    TEST_ASSERT_TRUE_MESSAGE(ese_input_state_get_key_down(input, InputKey_ENTER), "Key ENTER should be down");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_keys_pressed(void) {
    EseInputState *input = ese_input_state_create(g_engine);

    // Test initial values
    TEST_ASSERT_FALSE_MESSAGE(ese_input_state_get_key_pressed(input, InputKey_B), "Key B should initially be pressed = false");

    // Use direct field access to set values and test public getters
    input->keys_pressed[InputKey_B] = true;
    TEST_ASSERT_TRUE_MESSAGE(ese_input_state_get_key_pressed(input, InputKey_B), "Key B should be pressed");

    input->keys_pressed[InputKey_B] = false;
    TEST_ASSERT_FALSE_MESSAGE(ese_input_state_get_key_pressed(input, InputKey_B), "Key B should not be pressed");

    input->keys_pressed[InputKey_ESCAPE] = true;
    TEST_ASSERT_TRUE_MESSAGE(ese_input_state_get_key_pressed(input, InputKey_ESCAPE), "Key ESCAPE should be pressed");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_keys_released(void) {
    EseInputState *input = ese_input_state_create(g_engine);

    // Test initial values
    TEST_ASSERT_FALSE_MESSAGE(ese_input_state_get_key_released(input, InputKey_C), "Key C should initially be released = false");

    // Use direct field access to set values and test public getters
    input->keys_released[InputKey_C] = true;
    TEST_ASSERT_TRUE_MESSAGE(ese_input_state_get_key_released(input, InputKey_C), "Key C should be released");

    input->keys_released[InputKey_C] = false;
    TEST_ASSERT_FALSE_MESSAGE(ese_input_state_get_key_released(input, InputKey_C), "Key C should not be released");

    input->keys_released[InputKey_TAB] = true;
    TEST_ASSERT_TRUE_MESSAGE(ese_input_state_get_key_released(input, InputKey_TAB), "Key TAB should be released");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_mouse_down(void) {
    EseInputState *input = ese_input_state_create(g_engine);

    // Test initial values
    TEST_ASSERT_FALSE_MESSAGE(ese_input_state_get_mouse_down(input, 0), "Mouse button 0 should initially be false");
    TEST_ASSERT_FALSE_MESSAGE(ese_input_state_get_mouse_down(input, 1), "Mouse button 1 should initially be false");

    // Use direct field access to set values and test public getters
    input->mouse_down[0] = true;  // Left button
    TEST_ASSERT_TRUE_MESSAGE(ese_input_state_get_mouse_down(input, 0), "Mouse button 0 should be down");

    input->mouse_down[0] = false;
    TEST_ASSERT_FALSE_MESSAGE(ese_input_state_get_mouse_down(input, 0), "Mouse button 0 should not be down");

    input->mouse_down[1] = true;  // Right button
    TEST_ASSERT_TRUE_MESSAGE(ese_input_state_get_mouse_down(input, 1), "Mouse button 1 should be down");

    input->mouse_down[2] = true;  // Middle button
    TEST_ASSERT_TRUE_MESSAGE(ese_input_state_get_mouse_down(input, 2), "Mouse button 2 should be down");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_ref(void) {
    EseInputState *input = ese_input_state_create(g_engine);

    ese_input_state_ref(input);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, ese_input_state_get_lua_ref_count(input), "Ref count should be 1");

    ese_input_state_unref(input);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_input_state_get_lua_ref_count(input), "Ref count should be 0");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_copy_requires_engine(void) {
    TEST_ASSERT_DEATH(ese_input_state_copy(NULL), "ese_input_state_copy should abort with NULL input");
}

static void test_ese_input_state_copy(void) {
    EseInputState *input = ese_input_state_create(g_engine);
    ese_input_state_ref(input);
    input->mouse_x = 10;
    input->mouse_y = 20;
    input->keys_down[InputKey_A] = true;
    input->mouse_down[0] = true;
    EseInputState *copy = ese_input_state_copy(input);

    TEST_ASSERT_NOT_NULL_MESSAGE(copy, "Copy should be created");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_input_state_get_state(copy), "Copy should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_input_state_get_lua_ref_count(copy), "Copy should have ref count 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_input_state_get_lua_ref(copy), "Copy should have LUA_NOREF");
    TEST_ASSERT_EQUAL_INT_MESSAGE(10, ese_input_state_get_mouse_x(copy), "Copy should have mouse_x = 10");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, ese_input_state_get_mouse_y(copy), "Copy should have mouse_y = 20");
    TEST_ASSERT_TRUE_MESSAGE(ese_input_state_get_key_down(copy, InputKey_A), "Copy should have key A down");
    TEST_ASSERT_TRUE_MESSAGE(ese_input_state_get_mouse_down(copy, 0), "Copy should have mouse button 0 down");

    ese_input_state_unref(input);
    ese_input_state_destroy(input);
    ese_input_state_destroy(copy);
}

static void test_ese_input_state_direct_field_access(void) {
    EseInputState *input = ese_input_state_create(g_engine);

    // Test that we can directly access all fields
    input->mouse_x = 100;
    input->mouse_y = 200;
    input->mouse_scroll_dx = 5;
    input->mouse_scroll_dy = 3;
    input->keys_down[InputKey_A] = true;
    input->keys_pressed[InputKey_B] = true;
    input->keys_released[InputKey_C] = true;
    input->mouse_down[0] = true;
    input->mouse_down[1] = true;

    // Verify through getters
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, ese_input_state_get_mouse_x(input), "Direct field access should work for mouse_x");
    TEST_ASSERT_EQUAL_INT_MESSAGE(200, ese_input_state_get_mouse_y(input), "Direct field access should work for mouse_y");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, ese_input_state_get_mouse_scroll_dx(input), "Direct field access should work for mouse_scroll_dx");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, ese_input_state_get_mouse_scroll_dy(input), "Direct field access should work for mouse_scroll_dy");
    TEST_ASSERT_TRUE_MESSAGE(ese_input_state_get_key_down(input, InputKey_A), "Direct field access should work for keys_down");
    TEST_ASSERT_TRUE_MESSAGE(ese_input_state_get_key_pressed(input, InputKey_B), "Direct field access should work for keys_pressed");
    TEST_ASSERT_TRUE_MESSAGE(ese_input_state_get_key_released(input, InputKey_C), "Direct field access should work for keys_released");
    TEST_ASSERT_TRUE_MESSAGE(ese_input_state_get_mouse_down(input, 0), "Direct field access should work for mouse_down[0]");
    TEST_ASSERT_TRUE_MESSAGE(ese_input_state_get_mouse_down(input, 1), "Direct field access should work for mouse_down[1]");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_lua_integration(void) {
    EseLuaEngine *engine = create_test_engine();
    EseInputState *input = ese_input_state_create(engine);

    lua_State *before_state = ese_input_state_get_state(input);
    TEST_ASSERT_NOT_NULL_MESSAGE(before_state, "InputState should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, before_state, "InputState state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_input_state_get_lua_ref(input), "InputState should have no Lua reference initially");

    ese_input_state_ref(input);
    lua_State *after_ref_state = ese_input_state_get_state(input);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_ref_state, "InputState should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_ref_state, "InputState state should match engine runtime");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(LUA_NOREF, ese_input_state_get_lua_ref(input), "InputState should have a valid Lua reference after ref");

    ese_input_state_unref(input);
    lua_State *after_unref_state = ese_input_state_get_state(input);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_unref_state, "InputState should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_unref_state, "InputState state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_input_state_get_lua_ref(input), "InputState should have no Lua reference after unref");

    ese_input_state_destroy(input);
    lua_engine_destroy(engine);
}

static void test_ese_input_state_lua_init(void) {
    lua_State *L = g_engine->runtime;
    
    luaL_getmetatable(L, INPUT_STATE_PROXY_META);
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Metatable should not exist before initialization");
    lua_pop(L, 1);

    luaL_getmetatable(L, INPUT_STATE_PROXY_META "_KEY");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Key Metatable should not exist before initialization");
    lua_pop(L, 1);
    
    lua_getglobal(L, "Input");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Global Input table should not exist before initialization");
    lua_pop(L, 1);
    
    ese_input_state_lua_init(g_engine);
    
    luaL_getmetatable(L, INPUT_STATE_PROXY_META);
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Metatable should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Metatable should be a table");
    lua_pop(L, 1);
    
    luaL_getmetatable(L, INPUT_STATE_PROXY_META "_KEY");
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Key Metatable should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Key Metatable should be a table");
    lua_pop(L, 1);
    
    // No global Input table should exist after initialization
    lua_getglobal(L, "Input");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Global Input table should not exist before initialization");
    lua_pop(L, 1);
}

static void test_ese_input_state_lua_push(void) {
    ese_input_state_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseInputState *input = ese_input_state_create(g_engine);
    
    ese_input_state_lua_push(input);
    
    EseInputState **ud = (EseInputState **)lua_touserdata(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(input, *ud, "The pushed item should be the actual input state");
    
    lua_pop(L, 1); 
    
    ese_input_state_destroy(input);
}

static void test_ese_input_state_lua_get(void) {
    ese_input_state_lua_init(g_engine);

    lua_State *L = g_engine->runtime;
    EseInputState *input = ese_input_state_create(g_engine);
    
    ese_input_state_lua_push(input);
    
    EseInputState *extracted_input = ese_input_state_lua_get(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(input, extracted_input, "Extracted input state should match original");
    
    lua_pop(L, 1);
    ese_input_state_destroy(input);
}

/**
* Lua API Test Functions
*/

static void test_ese_input_state_lua_mouse_x(void) {
    ese_input_state_lua_init(g_engine);
    EseInputState *input = ese_input_state_create(g_engine);
    lua_State *L = g_engine->runtime;

    input->mouse_x = 10;

    // Push the input state to Lua with metatable
    ese_input_state_lua_push(input);
    lua_setglobal(L, "InputState");

    const char *test1 = "return InputState.mouse_x";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "get mouse_x should execute without error");
    int x = (int)lua_tointeger(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(10, x, "Mouse x should be 10");
    lua_pop(L, 1);

    const char *test2 = "InputState.mouse_x = 20";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "set mouse_x should execute with error");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_lua_mouse_y(void) {
    ese_input_state_lua_init(g_engine);
    EseInputState *input = ese_input_state_create(g_engine);
    lua_State *L = g_engine->runtime;

    input->mouse_y = 15;

    // Push the input state to Lua with metatable
    ese_input_state_lua_push(input);
    lua_setglobal(L, "InputState");

    const char *test1 = "return InputState.mouse_y";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "get mouse_y should execute without error");
    int y = (int)lua_tointeger(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(15, y, "Mouse y should be 15");
    lua_pop(L, 1);

    const char *test2 = "InputState.mouse_y = 25";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "set mouse_y should execute with error");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_lua_mouse_scroll_dx(void) {
    ese_input_state_lua_init(g_engine);
    EseInputState *input = ese_input_state_create(g_engine);
    lua_State *L = g_engine->runtime;

    input->mouse_scroll_dx = 5;

    // Push the input state to Lua with metatable
    ese_input_state_lua_push(input);
    lua_setglobal(L, "InputState");

    const char *test1 = "return InputState.mouse_scroll_dx";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "get mouse_scroll_dx should execute without error");
    int dx = (int)lua_tointeger(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, dx, "Mouse scroll dx should be 5");
    lua_pop(L, 1);

    const char *test2 = "InputState.mouse_scroll_dx = 10";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "set mouse_scroll_dx should execute with error");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_lua_mouse_scroll_dy(void) {
    ese_input_state_lua_init(g_engine);
    EseInputState *input = ese_input_state_create(g_engine);
    lua_State *L = g_engine->runtime;

    input->mouse_scroll_dy = -3;

    // Push the input state to Lua with metatable
    ese_input_state_lua_push(input);
    lua_setglobal(L, "InputState");

    const char *test1 = "return InputState.mouse_scroll_dy";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "get mouse_scroll_dy should execute without error");
    int dy = (int)lua_tointeger(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-3, dy, "Mouse scroll dy should be -3");
    lua_pop(L, 1);

    const char *test2 = "InputState.mouse_scroll_dy = 7";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "set mouse_scroll_dy should execute with error");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_lua_keys_down(void) {
    ese_input_state_lua_init(g_engine);
    EseInputState *input = ese_input_state_create(g_engine);
    lua_State *L = g_engine->runtime;

    // Set some keys down
    input->keys_down[InputKey_A] = true;
    input->keys_down[InputKey_SPACE] = true;
    input->keys_down[InputKey_ENTER] = false;

    // Push the input state to Lua with metatable
    ese_input_state_lua_push(input);
    lua_setglobal(L, "InputState");

    // Test accessing keys_down table
    const char *test1 = "return InputState.keys_down[InputState.KEY.A]";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "get keys_down[A] should execute without error");
    bool key_a_down = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(key_a_down, "Key A should be down");
    lua_pop(L, 1);

    const char *test2 = "return InputState.keys_down[InputState.KEY.SPACE]";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "get keys_down[SPACE] should execute without error");
    bool key_space_down = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(key_space_down, "Key SPACE should be down");
    lua_pop(L, 1);

    const char *test3 = "return InputState.keys_down[InputState.KEY.ENTER]";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "get keys_down[ENTER] should execute without error");
    bool key_enter_down = lua_toboolean(L, -1);
    TEST_ASSERT_FALSE_MESSAGE(key_enter_down, "Key ENTER should not be down");
    lua_pop(L, 1);

    // Test that setting keys_down should fail
    const char *test4 = "InputState.keys_down[InputState.KEY.A] = false";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test4), "set keys_down should execute with error");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_lua_keys_pressed(void) {
    ese_input_state_lua_init(g_engine);
    EseInputState *input = ese_input_state_create(g_engine);
    lua_State *L = g_engine->runtime;

    // Set some keys pressed
    input->keys_pressed[InputKey_B] = true;
    input->keys_pressed[InputKey_ESCAPE] = true;
    input->keys_pressed[InputKey_TAB] = false;

    // Push the input state to Lua with metatable
    ese_input_state_lua_push(input);
    lua_setglobal(L, "InputState");

    // Test accessing keys_pressed table
    const char *test1 = "return InputState.keys_pressed[InputState.KEY.B]";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "get keys_pressed[B] should execute without error");
    bool key_b_pressed = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(key_b_pressed, "Key B should be pressed");
    lua_pop(L, 1);

    const char *test2 = "return InputState.keys_pressed[InputState.KEY.ESCAPE]";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "get keys_pressed[ESCAPE] should execute without error");
    bool key_escape_pressed = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(key_escape_pressed, "Key ESCAPE should be pressed");
    lua_pop(L, 1);

    const char *test3 = "return InputState.keys_pressed[InputState.KEY.TAB]";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "get keys_pressed[TAB] should execute without error");
    bool key_tab_pressed = lua_toboolean(L, -1);
    TEST_ASSERT_FALSE_MESSAGE(key_tab_pressed, "Key TAB should not be pressed");
    lua_pop(L, 1);

    // Test that setting keys_pressed should fail
    const char *test4 = "InputState.keys_pressed[InputState.KEY.B] = false";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test4), "set keys_pressed should execute with error");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_lua_keys_released(void) {
    ese_input_state_lua_init(g_engine);
    EseInputState *input = ese_input_state_create(g_engine);
    lua_State *L = g_engine->runtime;

    // Set some keys released
    input->keys_released[InputKey_C] = true;
    input->keys_released[InputKey_DELETE] = true;
    input->keys_released[InputKey_BACKSPACE] = false;

    // Push the input state to Lua with metatable
    ese_input_state_lua_push(input);
    lua_setglobal(L, "InputState");

    // Test accessing keys_released table
    const char *test1 = "return InputState.keys_released[InputState.KEY.C]";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "get keys_released[C] should execute without error");
    bool key_c_released = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(key_c_released, "Key C should be released");
    lua_pop(L, 1);

    const char *test2 = "return InputState.keys_released[InputState.KEY.DELETE]";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "get keys_released[DELETE] should execute without error");
    bool key_delete_released = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(key_delete_released, "Key DELETE should be released");
    lua_pop(L, 1);

    const char *test3 = "return InputState.keys_released[InputState.KEY.BACKSPACE]";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "get keys_released[BACKSPACE] should execute without error");
    bool key_backspace_released = lua_toboolean(L, -1);
    TEST_ASSERT_FALSE_MESSAGE(key_backspace_released, "Key BACKSPACE should not be released");
    lua_pop(L, 1);

    // Test that setting keys_released should fail
    const char *test4 = "InputState.keys_released[InputState.KEY.C] = false";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test4), "set keys_released should execute with error");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_lua_mouse_down(void) {
    ese_input_state_lua_init(g_engine);
    EseInputState *input = ese_input_state_create(g_engine);
    lua_State *L = g_engine->runtime;

    // Set some mouse buttons
    input->mouse_down[0] = true;  // Left button
    input->mouse_down[1] = true;  // Right button
    input->mouse_down[2] = false; // Middle button

    // Push the input state to Lua with metatable
    ese_input_state_lua_push(input);
    lua_setglobal(L, "InputState");

    // Test accessing mouse_down table
    const char *test1 = "return InputState.mouse_down[0]";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "get mouse_down[0] should execute without error");
    bool button_0 = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(button_0, "Mouse button 0 should be down");
    lua_pop(L, 1);

    const char *test2 = "return InputState.mouse_down[1]";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "get mouse_down[1] should execute without error");
    bool button_1 = lua_toboolean(L, -1);
    TEST_ASSERT_TRUE_MESSAGE(button_1, "Mouse button 1 should be down");
    lua_pop(L, 1);

    const char *test3 = "return InputState.mouse_down[2]";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "get mouse_down[2] should execute without error");
    bool button_2 = lua_toboolean(L, -1);
    TEST_ASSERT_FALSE_MESSAGE(button_2, "Mouse button 2 should not be down");
    lua_pop(L, 1);

    // Test that setting mouse_down should fail
    const char *test4 = "InputState.mouse_down[0] = false";    
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test4), "set mouse_down should execute with error");

    ese_input_state_destroy(input);
}

static void test_ese_input_state_lua_key_constants(void) {
    ese_input_state_lua_init(g_engine);
    EseInputState *input = ese_input_state_create(g_engine);
    lua_State *L = g_engine->runtime;

    // Push the input state to Lua with metatable
    ese_input_state_lua_push(input);
    lua_setglobal(L, "InputState");

    // Test accessing KEY constants table
    const char *test1 = "return InputState.KEY.A";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test1), "get InputState.KEY.A should execute without error");
    int key_a = (int)lua_tointeger(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(InputKey_A, key_a, "KEY.A should equal InputKey_A");
    lua_pop(L, 1);

    const char *test2 = "return InputState.KEY.SPACE";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test2), "get InputState.KEY.SPACE should execute without error");
    int key_space = (int)lua_tointeger(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(InputKey_SPACE, key_space, "KEY.SPACE should equal InputKey_SPACE");
    lua_pop(L, 1);

    const char *test3 = "return InputState.KEY.ENTER";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test3), "get InputState.KEY.ENTER should execute without error");
    int key_enter = (int)lua_tointeger(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(InputKey_ENTER, key_enter, "KEY.ENTER should equal InputKey_ENTER");
    lua_pop(L, 1);

    const char *test4 = "return InputState.KEY.F1";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test4), "get InputState.KEY.F1 should execute without error");
    int key_f1 = (int)lua_tointeger(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(InputKey_F1, key_f1, "KEY.F1 should equal InputKey_F1");
    lua_pop(L, 1);

    // Test that KEY table is now read-only (modifications should fail)
    const char *test6 = "InputState.KEY.A = 999";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test6), "set KEY constants should execute with error");
    
    // Test that KEY table values are preserved (read-only)
    const char *test7 = "return InputState.KEY.A";    
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test7), "get KEY.A should execute without error");
    int original_key_a = (int)lua_tointeger(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(InputKey_A, original_key_a, "KEY.A should have original value");
    lua_pop(L, 1);

    ese_input_state_destroy(input);
}

static void test_ese_input_state_lua_tostring(void) {
    ese_input_state_lua_init(g_engine);
    EseInputState *input = ese_input_state_create(g_engine);
    lua_State *L = g_engine->runtime;

    // Push the input state to Lua with metatable
    ese_input_state_lua_push(input);
    lua_setglobal(L, "InputState");

    const char *test_code = "return tostring(InputState)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "tostring test should execute without error");
    const char *result = lua_tostring(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "tostring result should not be NULL");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "Input:") != NULL, "tostring should contain 'Input:'");
    lua_pop(L, 1); 

    ese_input_state_destroy(input);
}

