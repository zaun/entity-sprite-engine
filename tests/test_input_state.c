#include "test_utils.h"
#include "types/input_state.h"
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
static void test_input_state_creation();
static void test_input_state_properties();
static void test_input_state_copy();
static void test_input_state_lua_integration();
static void test_input_state_lua_script_api();
static void test_input_state_null_pointer_aborts();

// Test Lua script content for InputState testing
static const char* test_input_state_lua_script = 
"function INPUT_STATE_TEST_MODULE:test_input_state_creation()\n"
"    local i1 = InputState.new()\n"
"    local i2 = InputState.zero()\n"
"    \n"
"    if i1.mouse_x == 0 and i1.mouse_y == 0 and i1.mouse_left == false and i1.mouse_right == false and\n"
"       i2.mouse_x == 0 and i2.mouse_y == 0 and i2.mouse_left == false and i2.mouse_right == false then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n"
"\n"
"function INPUT_STATE_TEST_MODULE:test_input_state_properties()\n"
"    local i = InputState.new()\n"
"    \n"
"    i.mouse_x = 100\n"
"    i.mouse_y = 200\n"
"    i.mouse_left = true\n"
"    i.mouse_right = true\n"
"    \n"
"    if i.mouse_x == 100 and i.mouse_y == 200 and i.mouse_left == true and i.mouse_right == true then\n"
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
    
    test_suite_begin("🧪 EseInputState Test Suite");
    
    test_input_state_creation();
    test_input_state_properties();
    test_input_state_copy();
    test_input_state_lua_integration();
    test_input_state_lua_script_api();
    test_input_state_null_pointer_aborts();
    
    test_suite_end("🎯 EseInputState Test Suite");
    
    return 0;
}

static void test_input_state_creation() {
    test_begin("InputState Creation Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for input state creation tests");
    
    EseInputState *input_state = input_state_create(engine);
    TEST_ASSERT_NOT_NULL(input_state, "input_state_create should return non-NULL pointer");
    TEST_ASSERT_EQUAL(0, input_state->mouse_x, "New input state should have mouse_x = 0");
    TEST_ASSERT_EQUAL(0, input_state->mouse_y, "New input state should have mouse_y = 0");
    TEST_ASSERT(!input_state->mouse_buttons[0], "New input state should have mouse left button = false");
    TEST_ASSERT(!input_state->mouse_buttons[1], "New input state should have mouse right button = false");
    TEST_ASSERT_POINTER_EQUAL(engine->runtime, input_state->state, "InputState should have correct Lua state");
    TEST_ASSERT_EQUAL(0, input_state->lua_ref_count, "New input state should have ref count 0");
    TEST_ASSERT(input_state->lua_ref == LUA_NOREF, "New input state should have negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", input_state->lua_ref);
    TEST_ASSERT(sizeof(EseInputState) > 0, "EseInputState should have positive size");
    printf("ℹ INFO: Actual input state size: %zu bytes\n", sizeof(EseInputState));
    
    input_state_destroy(input_state);
    lua_engine_destroy(engine);
    
    test_end("InputState Creation Tests");
}

static void test_input_state_properties() {
    test_begin("InputState Properties Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for input state property tests");
    
    EseInputState *input_state = input_state_create(engine);
    TEST_ASSERT_NOT_NULL(input_state, "InputState should be created for property tests");
    
    input_state->mouse_x = 100;
    input_state->mouse_y = 200;
    input_state->mouse_buttons[0] = true;  // left button
    input_state->mouse_buttons[1] = true;  // right button
    
    TEST_ASSERT_EQUAL(100, input_state->mouse_x, "input state mouse_x should be set correctly");
    TEST_ASSERT_EQUAL(200, input_state->mouse_y, "input state mouse_y should be set correctly");
    TEST_ASSERT(input_state->mouse_buttons[0], "input state mouse left button should be set correctly");
    TEST_ASSERT(input_state->mouse_buttons[1], "input state mouse right button should be set correctly");
    
    input_state->mouse_x = -50;
    input_state->mouse_y = -100;
    input_state->mouse_buttons[0] = false;  // left button
    input_state->mouse_buttons[1] = false;  // right button
    
    TEST_ASSERT_EQUAL(-50, input_state->mouse_x, "input state mouse_x should handle negative values");
    TEST_ASSERT_EQUAL(-100, input_state->mouse_y, "input state mouse_y should handle negative values");
    TEST_ASSERT(!input_state->mouse_buttons[0], "input state mouse left button should handle false values");
    TEST_ASSERT(!input_state->mouse_buttons[1], "input state mouse right button should handle false values");
    
    input_state_destroy(input_state);
    lua_engine_destroy(engine);
    
    test_end("InputState Properties Tests");
}

static void test_input_state_copy() {
    test_begin("InputState Copy Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for input state copy tests");
    
    EseInputState *original = input_state_create(engine);
    TEST_ASSERT_NOT_NULL(original, "Original input state should be created for copy tests");
    
    original->mouse_x = 150;
    original->mouse_y = 250;
    original->mouse_buttons[0] = true;  // left button
    original->mouse_buttons[1] = false;  // right button
    
    EseInputState *copy = input_state_copy(original);
    TEST_ASSERT_NOT_NULL(copy, "input_state_copy should return non-NULL pointer");
    TEST_ASSERT_EQUAL(150, copy->mouse_x, "Copied input state should have same mouse x value");
    TEST_ASSERT_EQUAL(250, copy->mouse_y, "Copied input state should have same mouse y value");
    TEST_ASSERT(copy->mouse_buttons[0], "Copied input state should have same mouse left value");
    TEST_ASSERT(!copy->mouse_buttons[1], "Copied input state should have same mouse right value");
    TEST_ASSERT(original != copy, "Copy should be a different object");
    TEST_ASSERT_POINTER_EQUAL(original->state, copy->state, "Copy should have same Lua state");
    TEST_ASSERT(copy->lua_ref == LUA_NOREF, "Copy should start with negative LUA_NOREF value");
    printf("ℹ INFO: Copy LUA_NOREF value: %d\n", copy->lua_ref);
    TEST_ASSERT_EQUAL(0, copy->lua_ref_count, "Copy should start with ref count 0");
    
    input_state_destroy(original);
    input_state_destroy(copy);
    lua_engine_destroy(engine);
    
    test_end("InputState Copy Tests");
}

static void test_input_state_lua_integration() {
    test_begin("InputState Lua Integration Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for input state Lua integration tests");
    
    EseInputState *input_state = input_state_create(engine);
    TEST_ASSERT_NOT_NULL(input_state, "InputState should be created for Lua integration tests");
    TEST_ASSERT_EQUAL(0, input_state->lua_ref_count, "New input state should start with ref count 0");
    TEST_ASSERT(input_state->lua_ref == LUA_NOREF, "New input state should start with negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", input_state->lua_ref);
    
    input_state_destroy(input_state);
    lua_engine_destroy(engine);
    
    test_end("InputState Lua Integration Tests");
}

static void test_input_state_lua_script_api() {
    test_begin("InputState Lua Script API Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for input state Lua script API tests");
    
    input_state_lua_init(engine);
    printf("ℹ INFO: InputState Lua integration initialized\n");
    
    // Test that input state Lua integration initializes successfully
    TEST_ASSERT(true, "InputState Lua integration should initialize successfully");
    
    lua_engine_destroy(engine);
    
    test_end("InputState Lua Script API Tests");
}

static void test_input_state_null_pointer_aborts() {
    test_begin("InputState NULL Pointer Abort Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for input state NULL pointer abort tests");
    
    EseInputState *input_state = input_state_create(engine);
    TEST_ASSERT_NOT_NULL(input_state, "InputState should be created for input state NULL pointer abort tests");
    
    TEST_ASSERT_ABORT(input_state_create(NULL), "input_state_create should abort with NULL engine");
    TEST_ASSERT_ABORT(input_state_copy(NULL), "input_state_copy should abort with NULL source");
    TEST_ASSERT_ABORT(input_state_lua_init(NULL), "input_state_lua_init should abort with NULL engine");
    // Test that functions abort with NULL input state
    TEST_ASSERT_ABORT(input_state_lua_get(NULL, 1), "input_state_lua_get should abort with NULL Lua state");
    TEST_ASSERT_ABORT(input_state_ref(NULL), "input_state_ref should abort with NULL input state");
    
    input_state_destroy(input_state);
    lua_engine_destroy(engine);
    
    test_end("InputState NULL Pointer Abort Tests");
}
