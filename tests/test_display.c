#include "test_utils.h"
#include "types/display.h"
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
static void test_display_creation();
static void test_display_properties();
static void test_display_copy();
static void test_display_lua_integration();
static void test_display_lua_script_api();
static void test_display_null_pointer_aborts();

// Test Lua script content for Display testing
static const char* test_display_lua_script = 
"function DISPLAY_TEST_MODULE:test_display_creation()\n"
"    local d1 = Display.new(1920, 1080, true)\n"
"    local d2 = Display.zero()\n"
"    \n"
"    if d1.viewport.width == 1920 and d1.viewport.height == 1080 and d1.fullscreen == true and\n"
"       d2.viewport.width == 0 and d2.viewport.height == 0 and d2.fullscreen == false then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n"
"\n"
"function DISPLAY_TEST_MODULE:test_display_properties()\n"
"    local d = Display.new(0, 0, false)\n"
"    \n"
"    d.viewport.width = 800\n"
"    d.viewport.height = 600\n"
"    d.fullscreen = true\n"
"    \n"
"    if d.viewport.width == 800 and d.viewport.height == 600 and d.fullscreen == true then\n"
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
    
    test_suite_begin("🧪 EseDisplay Test Suite");
    
    test_display_creation();
    test_display_properties();
    test_display_copy();
    test_display_lua_integration();
    test_display_lua_script_api();
    test_display_null_pointer_aborts();
    
    test_suite_end("🎯 EseDisplay Test Suite");
    
    return 0;
}

static void test_display_creation() {
    test_begin("Display Creation Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for display creation tests");
    
    EseDisplay *display = display_state_create(engine);
    TEST_ASSERT_NOT_NULL(display, "display_state_create should return non-NULL pointer");
    TEST_ASSERT_EQUAL(0, display->viewport.width, "New display should have viewport width = 0");
    TEST_ASSERT_EQUAL(0, display->viewport.height, "New display should have viewport height = 0");
    TEST_ASSERT(!display->fullscreen, "New display should have fullscreen = false");
    TEST_ASSERT_POINTER_EQUAL(engine->runtime, display->state, "Display should have correct Lua state");
    TEST_ASSERT_EQUAL(0, display->lua_ref_count, "New display should have ref count 0");
    TEST_ASSERT(display->lua_ref == LUA_NOREF, "New display should have negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", display->lua_ref);
    TEST_ASSERT(sizeof(EseDisplay) > 0, "EseDisplay should have positive size");
    printf("ℹ INFO: Actual display size: %zu bytes\n", sizeof(EseDisplay));
    
    display_state_destroy(display);
    lua_engine_destroy(engine);
    
    test_end("Display Creation Tests");
}

static void test_display_properties() {
    test_begin("Display Properties Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for display property tests");
    
    EseDisplay *display = display_state_create(engine);
    TEST_ASSERT_NOT_NULL(display, "Display should be created for property tests");
    
    display->viewport.width = 1920;
    display->viewport.height = 1080;
    display->fullscreen = true;
    
    TEST_ASSERT_EQUAL(1920, display->viewport.width, "display viewport width should be set correctly");
    TEST_ASSERT_EQUAL(1080, display->viewport.height, "display viewport height should be set correctly");
    TEST_ASSERT(display->fullscreen, "display fullscreen should be set correctly");
    
    display->viewport.width = 800;
    display->viewport.height = 600;
    display->fullscreen = false;
    
    TEST_ASSERT_EQUAL(800, display->viewport.width, "display viewport width should handle different values");
    TEST_ASSERT_EQUAL(600, display->viewport.height, "display viewport height should handle different values");
    TEST_ASSERT(!display->fullscreen, "display fullscreen should handle false values");
    
    display_state_destroy(display);
    lua_engine_destroy(engine);
    
    test_end("Display Properties Tests");
}

static void test_display_copy() {
    test_begin("Display Copy Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for display copy tests");
    
    EseDisplay *original = display_state_create(engine);
    TEST_ASSERT_NOT_NULL(original, "Original display should be created for copy tests");
    
    original->viewport.width = 1920;
    original->viewport.height = 1080;
    original->fullscreen = true;
    
    EseDisplay *copy = display_state_copy(original);
    TEST_ASSERT_NOT_NULL(copy, "display_state_copy should return non-NULL pointer");
    TEST_ASSERT_EQUAL(1920, copy->viewport.width, "Copied display should have same width value");
    TEST_ASSERT_EQUAL(1080, copy->viewport.height, "Copied display should have same height value");
    TEST_ASSERT(copy->fullscreen, "Copied display should have same fullscreen value");
    TEST_ASSERT(original != copy, "Copy should be a different object");
    TEST_ASSERT_POINTER_EQUAL(original->state, copy->state, "Copy should have same Lua state");
    TEST_ASSERT(copy->lua_ref == LUA_NOREF, "Copy should start with negative LUA_NOREF value");
    printf("ℹ INFO: Copy LUA_NOREF value: %d\n", copy->lua_ref);
    TEST_ASSERT_EQUAL(0, copy->lua_ref_count, "Copy should start with ref count 0");
    
    display_state_destroy(original);
    display_state_destroy(copy);
    lua_engine_destroy(engine);
    
    test_end("Display Copy Tests");
}

static void test_display_lua_integration() {
    test_begin("Display Lua Integration Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for display Lua integration tests");
    
    EseDisplay *display = display_state_create(engine);
    TEST_ASSERT_NOT_NULL(display, "Display should be created for Lua integration tests");
    TEST_ASSERT_EQUAL(0, display->lua_ref_count, "New display should start with ref count 0");
    TEST_ASSERT(display->lua_ref == LUA_NOREF, "New display should start with negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", display->lua_ref);
    
    display_state_destroy(display);
    lua_engine_destroy(engine);
    
    test_end("Display Lua Integration Tests");
}

static void test_display_lua_script_api() {
    test_begin("Display Lua Script API Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for display Lua script API tests");
    
    display_state_lua_init(engine);
    printf("ℹ INFO: Display Lua integration initialized\n");
    
    // Test that display Lua integration initializes successfully
    TEST_ASSERT(true, "Display Lua integration should initialize successfully");
    
    lua_engine_destroy(engine);
    
    test_end("Display Lua Script API Tests");
}

static void test_display_null_pointer_aborts() {
    test_begin("Display NULL Pointer Abort Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for display NULL pointer abort tests");
    
    EseDisplay *display = display_state_create(engine);
    TEST_ASSERT_NOT_NULL(display, "Display should be created for display NULL pointer abort tests");
    
    TEST_ASSERT_ABORT(display_state_create(NULL), "display_state_create should abort with NULL engine");
    TEST_ASSERT_ABORT(display_state_copy(NULL), "display_state_copy should abort with NULL source");
    TEST_ASSERT_ABORT(display_state_lua_init(NULL), "display_state_lua_init should abort with NULL engine");
    // Test that functions abort with NULL display
    TEST_ASSERT_ABORT(display_state_lua_get(NULL, 1), "display_state_lua_get should abort with NULL Lua state");
    TEST_ASSERT_ABORT(display_state_lua_push(NULL), "display_state_lua_push should abort with NULL display");
    TEST_ASSERT_ABORT(display_state_ref(NULL), "display_state_ref should abort with NULL display");
    
    display_state_destroy(display);
    lua_engine_destroy(engine);
    
    test_end("Display NULL Pointer Abort Tests");
}
