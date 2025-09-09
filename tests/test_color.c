/*
 * Test file for color functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <execinfo.h>
#include <signal.h>
#include "test_utils.h"
#include "../src/types/color.h"
#include "../src/scripting/lua_engine.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

// Test function declarations
static void test_color_creation();
static void test_color_copy();
static void test_color_properties();
static void test_color_hex_conversion();
static void test_color_byte_conversion();
static void test_color_watchers();
static void test_color_lua_integration();
static void test_color_null_pointer_aborts();

// Helper function to create and initialize engine
static EseLuaEngine* create_test_engine() {
    EseLuaEngine *engine = lua_engine_create();
    if (engine) {
        // Set up registry keys that color system needs
        lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);
        
        // Initialize color system
        color_lua_init(engine);
    }
    return engine;
}

// Mock watcher callback
static bool mock_watcher_called = false;
static EseColor *mock_watcher_color = NULL;
static void *mock_watcher_userdata = NULL;

static void mock_watcher_callback(EseColor *color, void *userdata) {
    mock_watcher_called = true;
    mock_watcher_color = color;
    mock_watcher_userdata = userdata;
}

static void mock_reset() {
    mock_watcher_called = false;
    mock_watcher_color = NULL;
    mock_watcher_userdata = NULL;
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

    test_suite_begin("Color Tests");

    // Initialize required systems
    log_init();

    // Run all test suites
    test_color_creation();
    test_color_copy();
    test_color_properties();
    test_color_hex_conversion();
    test_color_byte_conversion();
    test_color_watchers();
    test_color_lua_integration();
    test_color_null_pointer_aborts();

    test_suite_end("Color Tests");

    return 0;
}

// Test basic color creation
static void test_color_creation() {
    test_begin("Color Creation");
    
    EseLuaEngine *engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");
    
    EseColor *color = color_create(engine);
    TEST_ASSERT_NOT_NULL(color, "Color should be created");
    
    // Test default values
    TEST_ASSERT_FLOAT_EQUAL(0.0f, color_get_r(color), 0.001f, "Default red should be 0.0");
    TEST_ASSERT_FLOAT_EQUAL(0.0f, color_get_g(color), 0.001f, "Default green should be 0.0");
    TEST_ASSERT_FLOAT_EQUAL(0.0f, color_get_b(color), 0.001f, "Default blue should be 0.0");
    TEST_ASSERT_FLOAT_EQUAL(1.0f, color_get_a(color), 0.001f, "Default alpha should be 1.0");
    
    // Clean up
    color_destroy(color);
    lua_engine_destroy(engine);
    
    test_end("Color Creation");
}

// Test color copying
static void test_color_copy() {
    test_begin("Color Copy");
    
    EseLuaEngine *engine = create_test_engine();
    EseColor *original = color_create(engine);
    
    // Set some values
    color_set_r(original, 0.5f);
    color_set_g(original, 0.25f);
    color_set_b(original, 0.75f);
    color_set_a(original, 0.8f);
    
    EseColor *copy = color_copy(original);
    TEST_ASSERT_NOT_NULL(copy, "Copy should be created");
    TEST_ASSERT(original != copy, "Copy should be a different pointer");
    
    // Test that values are copied
    TEST_ASSERT_FLOAT_EQUAL(0.5f, color_get_r(copy), 0.001f, "Copied red should match original");
    TEST_ASSERT_FLOAT_EQUAL(0.25f, color_get_g(copy), 0.001f, "Copied green should match original");
    TEST_ASSERT_FLOAT_EQUAL(0.75f, color_get_b(copy), 0.001f, "Copied blue should match original");
    TEST_ASSERT_FLOAT_EQUAL(0.8f, color_get_a(copy), 0.001f, "Copied alpha should match original");
    
    // Test that modifications to copy don't affect original
    color_set_r(copy, 1.0f);
    TEST_ASSERT_FLOAT_EQUAL(0.5f, color_get_r(original), 0.001f, "Original should not be affected by copy modification");
    
    // Clean up
    color_destroy(copy);
    color_destroy(original);
    lua_engine_destroy(engine);
    
    test_end("Color Copy");
}

// Test color property access
static void test_color_properties() {
    test_begin("Color Properties");
    
    EseLuaEngine *engine = create_test_engine();
    EseColor *color = color_create(engine);
    
    // Test setting and getting properties
    color_set_r(color, 0.1f);
    TEST_ASSERT_FLOAT_EQUAL(0.1f, color_get_r(color), 0.001f, "Red should be set and retrieved correctly");
    
    color_set_g(color, 0.2f);
    TEST_ASSERT_FLOAT_EQUAL(0.2f, color_get_g(color), 0.001f, "Green should be set and retrieved correctly");
    
    color_set_b(color, 0.3f);
    TEST_ASSERT_FLOAT_EQUAL(0.3f, color_get_b(color), 0.001f, "Blue should be set and retrieved correctly");
    
    color_set_a(color, 0.4f);
    TEST_ASSERT_FLOAT_EQUAL(0.4f, color_get_a(color), 0.001f, "Alpha should be set and retrieved correctly");
    
    // Test clamping behavior (values should be clamped to 0.0-1.0 range)
    color_set_r(color, 1.5f);
    TEST_ASSERT_FLOAT_EQUAL(1.5f, color_get_r(color), 0.001f, "Values above 1.0 should be stored as-is (no clamping)");
    
    color_set_g(color, -0.5f);
    TEST_ASSERT_FLOAT_EQUAL(-0.5f, color_get_g(color), 0.001f, "Values below 0.0 should be stored as-is (no clamping)");
    
    // Clean up
    color_destroy(color);
    lua_engine_destroy(engine);
    
    test_end("Color Properties");
}

// Test hex string conversion
static void test_color_hex_conversion() {
    test_begin("Color Hex Conversion");
    
    EseLuaEngine *engine = create_test_engine();
    EseColor *color = color_create(engine);
    
    // Test #RGB format
    bool success = color_set_hex(color, "#F0A");
    TEST_ASSERT(success, "Should successfully parse #F0A");
    TEST_ASSERT_FLOAT_EQUAL(1.0f, color_get_r(color), 0.001f, "Red should be 1.0 for #F0A");
    TEST_ASSERT_FLOAT_EQUAL(0.0f, color_get_g(color), 0.001f, "Green should be 0.0 for #F0A");
    TEST_ASSERT_FLOAT_EQUAL(0.67f, color_get_b(color), 0.01f, "Blue should be ~0.67 for #F0A");
    TEST_ASSERT_FLOAT_EQUAL(1.0f, color_get_a(color), 0.001f, "Alpha should be 1.0 for #F0A");
    
    // Test #RRGGBB format
    success = color_set_hex(color, "#FF0000");
    TEST_ASSERT(success, "Should successfully parse #FF0000");
    TEST_ASSERT_FLOAT_EQUAL(1.0f, color_get_r(color), 0.001f, "Red should be 1.0 for #FF0000");
    TEST_ASSERT_FLOAT_EQUAL(0.0f, color_get_g(color), 0.001f, "Green should be 0.0 for #FF0000");
    TEST_ASSERT_FLOAT_EQUAL(0.0f, color_get_b(color), 0.001f, "Blue should be 0.0 for #FF0000");
    TEST_ASSERT_FLOAT_EQUAL(1.0f, color_get_a(color), 0.001f, "Alpha should be 1.0 for #FF0000");
    
    // Test #RGBA format
    success = color_set_hex(color, "#FF0080");
    TEST_ASSERT(success, "Should successfully parse #FF0080");
    TEST_ASSERT_FLOAT_EQUAL(1.0f, color_get_r(color), 0.001f, "Red should be 1.0 for #FF0080");
    TEST_ASSERT_FLOAT_EQUAL(0.0f, color_get_g(color), 0.001f, "Green should be 0.0 for #FF0080");
    TEST_ASSERT_FLOAT_EQUAL(0.5f, color_get_b(color), 0.01f, "Blue should be ~0.5 for #FF0080");
    TEST_ASSERT_FLOAT_EQUAL(1.0f, color_get_a(color), 0.001f, "Alpha should be 1.0 for #FF0080");
    
    // Test #RRGGBBAA format
    success = color_set_hex(color, "#FF000080");
    TEST_ASSERT(success, "Should successfully parse #FF000080");
    TEST_ASSERT_FLOAT_EQUAL(1.0f, color_get_r(color), 0.001f, "Red should be 1.0 for #FF000080");
    TEST_ASSERT_FLOAT_EQUAL(0.0f, color_get_g(color), 0.001f, "Green should be 0.0 for #FF000080");
    TEST_ASSERT_FLOAT_EQUAL(0.0f, color_get_b(color), 0.001f, "Blue should be 0.0 for #FF000080");
    TEST_ASSERT_FLOAT_EQUAL(0.5f, color_get_a(color), 0.01f, "Alpha should be ~0.5 for #FF000080");
    
    // Test invalid formats
    success = color_set_hex(color, "invalid");
    TEST_ASSERT(!success, "Should fail to parse invalid format");
    
    success = color_set_hex(color, "#GG");
    TEST_ASSERT(!success, "Should fail to parse invalid hex characters");
    
    success = color_set_hex(color, "#");
    TEST_ASSERT(!success, "Should fail to parse incomplete hex string");
    
    // Note: Testing NULL string would cause abort, so we skip this test
    
    // Clean up
    color_destroy(color);
    lua_engine_destroy(engine);
    
    test_end("Color Hex Conversion");
}

// Test byte conversion
static void test_color_byte_conversion() {
    test_begin("Color Byte Conversion");
    
    EseLuaEngine *engine = create_test_engine();
    EseColor *color = color_create(engine);
    
    // Test setting from byte values
    color_set_byte(color, 255, 128, 64, 192);
    TEST_ASSERT_FLOAT_EQUAL(1.0f, color_get_r(color), 0.001f, "Red should be 1.0 for byte 255");
    TEST_ASSERT_FLOAT_EQUAL(0.502f, color_get_g(color), 0.01f, "Green should be ~0.502 for byte 128");
    TEST_ASSERT_FLOAT_EQUAL(0.251f, color_get_b(color), 0.01f, "Blue should be ~0.251 for byte 64");
    TEST_ASSERT_FLOAT_EQUAL(0.753f, color_get_a(color), 0.01f, "Alpha should be ~0.753 for byte 192");
    
    // Test getting byte values
    unsigned char r, g, b, a;
    color_get_byte(color, &r, &g, &b, &a);
    TEST_ASSERT_EQUAL(255, r, "Red byte should be 255");
    TEST_ASSERT_EQUAL(128, g, "Green byte should be 128");
    TEST_ASSERT_EQUAL(64, b, "Blue byte should be 64");
    TEST_ASSERT_EQUAL(192, a, "Alpha byte should be 192");
    
    // Test edge cases
    color_set_byte(color, 0, 0, 0, 0);
    TEST_ASSERT_FLOAT_EQUAL(0.0f, color_get_r(color), 0.001f, "Red should be 0.0 for byte 0");
    TEST_ASSERT_FLOAT_EQUAL(0.0f, color_get_g(color), 0.001f, "Green should be 0.0 for byte 0");
    TEST_ASSERT_FLOAT_EQUAL(0.0f, color_get_b(color), 0.001f, "Blue should be 0.0 for byte 0");
    TEST_ASSERT_FLOAT_EQUAL(0.0f, color_get_a(color), 0.001f, "Alpha should be 0.0 for byte 0");
    
    // Clean up
    color_destroy(color);
    lua_engine_destroy(engine);
    
    test_end("Color Byte Conversion");
}

// Test watcher system
static void test_color_watchers() {
    test_begin("Color Watchers");
    
    EseLuaEngine *engine = create_test_engine();
    EseColor *color = color_create(engine);
    
    // Test adding watcher
    bool success = color_add_watcher(color, mock_watcher_callback, (void*)0x1234);
    TEST_ASSERT(success, "Should successfully add watcher");
    
    // Test that watcher is called on property change
    mock_reset();
    color_set_r(color, 0.5f);
    TEST_ASSERT(mock_watcher_called, "Watcher should be called on property change");
    TEST_ASSERT(mock_watcher_color == color, "Watcher should receive correct color pointer");
    TEST_ASSERT(mock_watcher_userdata == (void*)0x1234, "Watcher should receive correct userdata");
    
    // Test that watcher is called on other property changes
    mock_reset();
    color_set_g(color, 0.25f);
    TEST_ASSERT(mock_watcher_called, "Watcher should be called on green change");
    
    mock_reset();
    color_set_b(color, 0.75f);
    TEST_ASSERT(mock_watcher_called, "Watcher should be called on blue change");
    
    mock_reset();
    color_set_a(color, 0.8f);
    TEST_ASSERT(mock_watcher_called, "Watcher should be called on alpha change");
    
    // Test removing watcher
    success = color_remove_watcher(color, mock_watcher_callback, (void*)0x1234);
    TEST_ASSERT(success, "Should successfully remove watcher");
    
    // Test that watcher is not called after removal
    mock_reset();
    color_set_r(color, 1.0f);
    TEST_ASSERT(!mock_watcher_called, "Watcher should not be called after removal");
    
    // Test removing non-existent watcher
    success = color_remove_watcher(color, mock_watcher_callback, (void*)0x1234);
    TEST_ASSERT(!success, "Should fail to remove non-existent watcher");
    
    // Clean up
    color_destroy(color);
    lua_engine_destroy(engine);
    
    test_end("Color Watchers");
}

// Test Lua integration
static void test_color_lua_integration() {
    test_begin("Color Lua Integration");
    
    EseLuaEngine *engine = create_test_engine();
    EseColor *color = color_create(engine);
    
    // Test getting Lua reference
    int lua_ref = color_get_lua_ref(color);
    TEST_ASSERT(lua_ref == LUA_NOREF, "Color should have no Lua reference initially");
    
    // Test referencing
    color_ref(color);
    lua_ref = color_get_lua_ref(color);
    TEST_ASSERT(lua_ref != LUA_NOREF, "Color should have a valid Lua reference after ref");
    TEST_ASSERT_EQUAL(1, color_get_lua_ref_count(color), "Color should have ref count of 1");
    
    // Test unreferencing
    color_unref(color);
    TEST_ASSERT_EQUAL(0, color_get_lua_ref_count(color), "Color should have ref count of 0 after unref");
    
    // Test Lua state
    lua_State *state = color_get_state(color);
    TEST_ASSERT_NOT_NULL(state, "Color should have a valid Lua state");
    TEST_ASSERT(state == engine->runtime, "Color state should match engine runtime");
    
    // Clean up
    color_destroy(color);
    lua_engine_destroy(engine);
    
    test_end("Color Lua Integration");
}

// Test NULL pointer aborts
static void test_color_null_pointer_aborts() {
    test_begin("Color NULL Pointer Abort Tests");
    
    EseLuaEngine *engine = create_test_engine();
    EseColor *color = color_create(engine);
    
    // Test that creation functions abort with NULL pointers
    TEST_ASSERT_ABORT(color_create(NULL), "color_create should abort with NULL engine");
    TEST_ASSERT_ABORT(color_copy(NULL), "color_copy should abort with NULL color");
    // color_destroy should ignore NULL color (not abort)
    color_destroy(NULL);
    
    // Test that property functions abort with NULL pointers
    TEST_ASSERT_ABORT(color_set_r(NULL, 0.5f), "color_set_r should abort with NULL color");
    TEST_ASSERT_ABORT(color_get_r(NULL), "color_get_r should abort with NULL color");
    TEST_ASSERT_ABORT(color_set_g(NULL, 0.5f), "color_set_g should abort with NULL color");
    TEST_ASSERT_ABORT(color_get_g(NULL), "color_get_g should abort with NULL color");
    TEST_ASSERT_ABORT(color_set_b(NULL, 0.5f), "color_set_b should abort with NULL color");
    TEST_ASSERT_ABORT(color_get_b(NULL), "color_get_b should abort with NULL color");
    TEST_ASSERT_ABORT(color_set_a(NULL, 0.5f), "color_set_a should abort with NULL color");
    TEST_ASSERT_ABORT(color_get_a(NULL), "color_get_a should abort with NULL color");
    
    // Test that utility functions abort with NULL pointers
    TEST_ASSERT_ABORT(color_set_hex(NULL, "#FF0000"), "color_set_hex should abort with NULL color");
    TEST_ASSERT_ABORT(color_set_hex(color, NULL), "color_set_hex should abort with NULL hex string");
    TEST_ASSERT_ABORT(color_set_byte(NULL, 255, 128, 64, 192), "color_set_byte should abort with NULL color");
    
    unsigned char r, g, b, a;
    TEST_ASSERT_ABORT(color_get_byte(NULL, &r, &g, &b, &a), "color_get_byte should abort with NULL color");
    TEST_ASSERT_ABORT(color_get_byte(color, NULL, &g, &b, &a), "color_get_byte should abort with NULL r pointer");
    TEST_ASSERT_ABORT(color_get_byte(color, &r, NULL, &b, &a), "color_get_byte should abort with NULL g pointer");
    TEST_ASSERT_ABORT(color_get_byte(color, &r, &g, NULL, &a), "color_get_byte should abort with NULL b pointer");
    TEST_ASSERT_ABORT(color_get_byte(color, &r, &g, &b, NULL), "color_get_byte should abort with NULL a pointer");
    
    // Test that watcher functions abort with NULL pointers
    TEST_ASSERT_ABORT(color_add_watcher(NULL, mock_watcher_callback, NULL), "color_add_watcher should abort with NULL color");
    TEST_ASSERT_ABORT(color_add_watcher(color, NULL, NULL), "color_add_watcher should abort with NULL callback");
    TEST_ASSERT_ABORT(color_remove_watcher(NULL, mock_watcher_callback, NULL), "color_remove_watcher should abort with NULL color");
    TEST_ASSERT_ABORT(color_remove_watcher(color, NULL, NULL), "color_remove_watcher should abort with NULL callback");
    
    // Test that Lua functions abort with NULL pointers
    TEST_ASSERT_ABORT(color_lua_init(NULL), "color_lua_init should abort with NULL engine");
    TEST_ASSERT_ABORT(color_lua_push(NULL), "color_lua_push should abort with NULL color");
    TEST_ASSERT_ABORT(color_lua_get(NULL, 1), "color_lua_get should abort with NULL Lua state");
    TEST_ASSERT_ABORT(color_ref(NULL), "color_ref should abort with NULL color");
    // color_unref should ignore NULL color (not abort)
    color_unref(NULL);
    TEST_ASSERT_ABORT(color_get_state(NULL), "color_get_state should abort with NULL color");
    TEST_ASSERT_ABORT(color_get_lua_ref(NULL), "color_get_lua_ref should abort with NULL color");
    TEST_ASSERT_ABORT(color_get_lua_ref_count(NULL), "color_get_lua_ref_count should abort with NULL color");
    
    // Clean up
    color_destroy(color);
    lua_engine_destroy(engine);
    
    test_end("Color NULL Pointer Abort Tests");
}
