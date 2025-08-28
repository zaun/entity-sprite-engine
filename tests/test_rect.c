#include "test_utils.h"
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
static void test_rect_creation();
static void test_rect_properties();
static void test_rect_copy();
static void test_rect_mathematical_operations();
static void test_rect_collision_detection();
static void test_rect_watcher_system();
static void test_rect_lua_integration();
static void test_rect_lua_script_api();

// Test Lua script content for Rect testing
static const char* test_rect_lua_script = 
"function RECT_TEST_MODULE:test_rect_creation()\n"
"    local r1 = Rect.new(10.5, -5.25, 100.0, 75.5)\n"
"    local r2 = Rect.zero()\n"
"    \n"
"    if r1.x == 10.5 and r1.y == -5.25 and r1.width == 100.0 and r1.height == 75.5 and\n"
"       r2.x == 0 and r2.y == 0 and r2.width == 0 and r2.height == 0 then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n"
"\n"
"function RECT_TEST_MODULE:test_rect_properties()\n"
"    local r = Rect.new(0, 0, 0, 0)\n"
"    \n"
"    r.x = 42.0\n"
"    r.y = -17.5\n"
"    r.width = 200.0\n"
"    r.height = 150.0\n"
"    r.rotation = 0.785398  -- π/4 radians (45 degrees)\n"
"    \n"
"    if r.x == 42.0 and r.y == -17.5 and r.width == 200.0 and r.height == 150.0 and\n"
"       math.abs(r.rotation - 0.785398) < 0.001 then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n"
"\n"
"function RECT_TEST_MODULE:test_rect_operations()\n"
"    local r1 = Rect.new(1, 2, 3, 4)\n"
"    local r2 = Rect.new(5, 6, 7, 8)\n"
"    \n"
"    -- Test basic arithmetic operations\n"
"    if r1.x + r2.x == 6 and r1.y + r2.y == 8 and\n"
"       r1.width + r2.width == 10 and r1.height + r2.height == 12 then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n";

// Mock watcher callback for testing
static bool rect_watcher_called = false;
static EseRect *last_watched_rect = NULL;
static void *last_rect_watcher_userdata = NULL;

static void test_rect_watcher_callback(EseRect *rect, void *userdata) {
    rect_watcher_called = true;
    last_watched_rect = rect;
    last_rect_watcher_userdata = userdata;
}

void segfault_handler(int signo, siginfo_t *info, void *context) {
    void *buffer[32];
    int nptrs = backtrace(buffer, 32);
    char **strings = backtrace_symbols(buffer, nptrs);
    if (strings) {
        fprintf(stderr, "---- BACKTRACE START ----\n");
        for (int i = 0; i < nptrs; i++) {
            fprintf(stderr, "%s\n", strings[i]);
        }
        fprintf(stderr, "---- BACKTRACE  END  ----\n");
        free(strings);
    }

    signal(signo, SIG_DFL);
    raise(signo);
}

int main() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_sigaction = segfault_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);

    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("Error setting SIGSEGV handler");
        return EXIT_FAILURE;
    }

    test_suite_begin("🧪 EseRect Test Suite");
    
    // Initialize required systems
    log_init();
    
    test_rect_creation();
    test_rect_properties();
    test_rect_copy();
    test_rect_mathematical_operations();
    test_rect_collision_detection();
    test_rect_watcher_system();
    test_rect_lua_integration();
    test_rect_lua_script_api();
    
    test_suite_end("🎯 EseRect Test Suite");
        
    return 0;
}

static void test_rect_creation() {
    test_begin("Rect Creation Tests");
    
    EseLuaEngine *mock_engine = lua_engine_create();
    
    // Test rect_create
    EseRect *rect = rect_create(mock_engine);
    TEST_ASSERT_NOT_NULL(rect, "rect_create should return non-NULL pointer");
    
    if (rect) {
        TEST_ASSERT_EQUAL(0.0f, rect_get_x(rect), "New rect should have x = 0.0");
        TEST_ASSERT_EQUAL(0.0f, rect_get_y(rect), "New rect should have y = 0.0");
        TEST_ASSERT_EQUAL(0.0f, rect_get_width(rect), "New rect should have width = 0.0");
        TEST_ASSERT_EQUAL(0.0f, rect_get_height(rect), "New rect should have height = 0.0");
        TEST_ASSERT_EQUAL(0.0f, rect_get_rotation(rect), "New rect should have rotation = 0.0");
        TEST_ASSERT_POINTER_EQUAL(mock_engine->runtime, rect_get_state(rect), "Rect should have correct Lua state");
        TEST_ASSERT_EQUAL(0, rect_get_lua_ref_count(rect), "New rect should have ref count 0");
        
        // Test LUA_NOREF - check for any negative value (LUA_NOREF is typically -1 but may vary)
        int lua_ref = rect_get_lua_ref(rect);
        TEST_ASSERT(lua_ref < 0, "New rect should have negative LUA_NOREF value");
        printf("ℹ INFO: Actual LUA_NOREF value: %d\n", lua_ref);
        
        // Test rect_sizeof
        size_t actual_size = rect_sizeof();
        TEST_ASSERT(actual_size > 0, "rect_sizeof should return positive size");
        printf("ℹ INFO: Actual rect size: %zu bytes\n", actual_size);
        
        rect_destroy(rect);
    }
    
    lua_engine_destroy(mock_engine);
    
    test_end("Rect Creation Tests");
}

static void test_rect_properties() {
    test_begin("Rect Properties Tests");
    
    EseLuaEngine *mock_engine = lua_engine_create();
    EseRect *rect = rect_create(mock_engine);
    
    TEST_ASSERT_NOT_NULL(rect, "Rect should be created for property tests");
    
    if (rect) {
        // Test setting and getting x coordinate
        rect_set_x(rect, 10.5f);
        TEST_ASSERT_FLOAT_EQUAL(10.5f, rect_get_x(rect), 0.001f, "rect_set_x should set x coordinate");
        
        // Test setting and getting y coordinate
        rect_set_y(rect, -5.25f);
        TEST_ASSERT_FLOAT_EQUAL(-5.25f, rect_get_y(rect), 0.001f, "rect_set_y should set y coordinate");
        
        // Test setting and getting width
        rect_set_width(rect, 100.0f);
        TEST_ASSERT_FLOAT_EQUAL(100.0f, rect_get_width(rect), 0.001f, "rect_set_width should set width");
        
        // Test setting and getting height
        rect_set_height(rect, 75.5f);
        TEST_ASSERT_FLOAT_EQUAL(75.5f, rect_get_height(rect), 0.001f, "rect_set_height should set height");
        
        // Test setting and getting rotation (in radians)
        rect_set_rotation(rect, M_PI / 4.0f); // 45 degrees
        TEST_ASSERT_FLOAT_EQUAL(M_PI / 4.0f, rect_get_rotation(rect), 0.001f, "rect_set_rotation should set rotation in radians");
        
        // Test setting negative values
        rect_set_x(rect, -100.0f);
        rect_set_y(rect, -200.0f);
        rect_set_width(rect, -50.0f); // Should handle negative width
        rect_set_height(rect, -25.0f); // Should handle negative height
        
        TEST_ASSERT_FLOAT_EQUAL(-100.0f, rect_get_x(rect), 0.001f, "rect_set_x should handle negative values");
        TEST_ASSERT_FLOAT_EQUAL(-200.0f, rect_get_y(rect), 0.001f, "rect_set_y should handle negative values");
        TEST_ASSERT_FLOAT_EQUAL(-50.0f, rect_get_width(rect), 0.001f, "rect_set_width should handle negative values");
        TEST_ASSERT_FLOAT_EQUAL(-25.0f, rect_get_height(rect), 0.001f, "rect_set_height should handle negative values");
        
        // Test setting zero values
        rect_set_x(rect, 0.0f);
        rect_set_y(rect, 0.0f);
        rect_set_width(rect, 0.0f);
        rect_set_height(rect, 0.0f);
        rect_set_rotation(rect, 0.0f);
        
        TEST_ASSERT_FLOAT_EQUAL(0.0f, rect_get_x(rect), 0.001f, "rect_set_x should handle zero values");
        TEST_ASSERT_FLOAT_EQUAL(0.0f, rect_get_y(rect), 0.001f, "rect_set_y should handle zero values");
        TEST_ASSERT_FLOAT_EQUAL(0.0f, rect_get_width(rect), 0.001f, "rect_set_width should handle zero values");
        TEST_ASSERT_FLOAT_EQUAL(0.0f, rect_get_height(rect), 0.001f, "rect_set_height should handle zero values");
        TEST_ASSERT_FLOAT_EQUAL(0.0f, rect_get_rotation(rect), 0.001f, "rect_set_rotation should handle zero values");
        
        rect_destroy(rect);
    }
    
    lua_engine_destroy(mock_engine);
    
    test_end("Rect Properties Tests");
}

static void test_rect_copy() {
    test_begin("Rect Copy Tests");
    
    EseLuaEngine *mock_engine = lua_engine_create();
    EseRect *original = rect_create(mock_engine);
    
    TEST_ASSERT_NOT_NULL(original, "Original rect should be created for copy tests");
    
    if (original) {
        // Set some values
        rect_set_x(original, 42.0f);
        rect_set_y(original, -17.5f);
        rect_set_width(original, 100.0f);
        rect_set_height(original, 75.0f);
        rect_set_rotation(original, M_PI / 6.0f); // 30 degrees
        
        // Test rect_copy
        EseRect *copy = rect_copy(original);
        TEST_ASSERT_NOT_NULL(copy, "rect_copy should return non-NULL pointer");
        
        if (copy) {
            // Verify values are copied correctly
            TEST_ASSERT_FLOAT_EQUAL(42.0f, rect_get_x(copy), 0.001f, "Copied rect should have same x value");
            TEST_ASSERT_FLOAT_EQUAL(-17.5f, rect_get_y(copy), 0.001f, "Copied rect should have same y value");
            TEST_ASSERT_FLOAT_EQUAL(100.0f, rect_get_width(copy), 0.001f, "Copied rect should have same width");
            TEST_ASSERT_FLOAT_EQUAL(75.0f, rect_get_height(copy), 0.001f, "Copied rect should have same height");
            TEST_ASSERT_FLOAT_EQUAL(M_PI / 6.0f, rect_get_rotation(copy), 0.001f, "Copied rect should have same rotation");
            
            // Verify it's a different object
            TEST_ASSERT(original != copy, "Copy should be a different object");
            
            // Verify Lua state is copied
            TEST_ASSERT_POINTER_EQUAL(rect_get_state(original), rect_get_state(copy), "Copy should have same Lua state");
            
            // Verify copy starts with no Lua references
            int copy_lua_ref = rect_get_lua_ref(copy);
            TEST_ASSERT(copy_lua_ref < 0, "Copy should start with negative LUA_NOREF value");
            printf("ℹ INFO: Copy LUA_NOREF value: %d\n", copy_lua_ref);
            TEST_ASSERT_EQUAL(0, rect_get_lua_ref_count(copy), "Copy should start with ref count 0");
            
            rect_destroy(copy);
        }
        
        rect_destroy(original);
    }
    
    lua_engine_destroy(mock_engine);
    
    test_end("Rect Copy Tests");
}

static void test_rect_mathematical_operations() {
    test_begin("Rect Mathematical Operations Tests");
    
    EseLuaEngine *mock_engine = lua_engine_create();
    EseRect *rect = rect_create(mock_engine);
    
    TEST_ASSERT_NOT_NULL(rect, "Rect should be created for math tests");
    
    if (rect) {
        // Test area calculation
        rect_set_width(rect, 10.0f);
        rect_set_height(rect, 5.0f);
        
        float area = rect_area(rect);
        TEST_ASSERT_FLOAT_EQUAL(50.0f, area, 0.001f, "Area of 10x5 rect should be 50.0");
        
        // Test area with negative dimensions
        rect_set_width(rect, -10.0f);
        rect_set_height(rect, -5.0f);
        
        area = rect_area(rect);
        TEST_ASSERT_FLOAT_EQUAL(50.0f, area, 0.001f, "Area should be positive even with negative dimensions");
        
        // Test area with zero dimensions
        rect_set_width(rect, 0.0f);
        rect_set_height(rect, 0.0f);
        
        area = rect_area(rect);
        TEST_ASSERT_FLOAT_EQUAL(0.0f, area, 0.001f, "Area of 0x0 rect should be 0.0");
        
        // Test area with NULL rect
        float null_area = rect_area(NULL);
        TEST_ASSERT_FLOAT_EQUAL(0.0f, null_area, 0.001f, "Area of NULL rect should return 0.0");
        
        rect_destroy(rect);
    }
    
    lua_engine_destroy(mock_engine);
    
    test_end("Rect Mathematical Operations Tests");
}

static void test_rect_collision_detection() {
    test_begin("Rect Collision Detection Tests");
    
    EseLuaEngine *mock_engine = lua_engine_create();
    
    // Create test rectangles
    EseRect *rect1 = rect_create(mock_engine);
    EseRect *rect2 = rect_create(mock_engine);
    EseRect *rect3 = rect_create(mock_engine);
    
    TEST_ASSERT_NOT_NULL(rect1, "Rect1 should be created for collision tests");
    TEST_ASSERT_NOT_NULL(rect2, "Rect2 should be created for collision tests");
    TEST_ASSERT_NOT_NULL(rect3, "Rect3 should be created for collision tests");
    
    if (rect1 && rect2 && rect3) {
        // Test axis-aligned rectangle intersection
        rect_set_x(rect1, 0.0f);
        rect_set_y(rect1, 0.0f);
        rect_set_width(rect1, 10.0f);
        rect_set_height(rect1, 10.0f);
        rect_set_rotation(rect1, 0.0f);
        
        rect_set_x(rect2, 5.0f);
        rect_set_y(rect2, 5.0f);
        rect_set_width(rect2, 10.0f);
        rect_set_height(rect2, 10.0f);
        rect_set_rotation(rect2, 0.0f);
        
        bool intersects = rect_intersects(rect1, rect2);
        TEST_ASSERT(intersects, "Overlapping axis-aligned rectangles should intersect");
        
        // Test non-overlapping rectangles
        rect_set_x(rect3, 20.0f);
        rect_set_y(rect3, 20.0f);
        rect_set_width(rect3, 5.0f);
        rect_set_height(rect3, 5.0f);
        rect_set_rotation(rect3, 0.0f);
        
        bool not_intersects = rect_intersects(rect1, rect3);
        TEST_ASSERT(!not_intersects, "Non-overlapping rectangles should not intersect");
        
        // Test point containment
        bool contains = rect_contains_point(rect1, 5.0f, 5.0f);
        TEST_ASSERT(contains, "Point (5,5) should be inside rect1");
        
        bool not_contains = rect_contains_point(rect1, 15.0f, 15.0f);
        TEST_ASSERT(!not_contains, "Point (15,15) should not be inside rect1");
        
        // Test edge cases
        bool edge_contains = rect_contains_point(rect1, 0.0f, 0.0f);
        TEST_ASSERT(edge_contains, "Point (0,0) on edge should be inside rect1");
        
        bool edge_contains2 = rect_contains_point(rect1, 10.0f, 10.0f);
        TEST_ASSERT(edge_contains2, "Point (10,10) on edge should be inside rect1");
        
        // Test with NULL pointers
        bool null_intersects = rect_intersects(NULL, rect1);
        TEST_ASSERT(!null_intersects, "Intersection with NULL first rect should return false");
        
        null_intersects = rect_intersects(rect1, NULL);
        TEST_ASSERT(!null_intersects, "Intersection with NULL second rect should return false");
        
        bool null_contains = rect_contains_point(NULL, 5.0f, 5.0f);
        TEST_ASSERT(!null_contains, "Contains point with NULL rect should return false");
        
        rect_destroy(rect1);
        rect_destroy(rect2);
        rect_destroy(rect3);
    }
    
    lua_engine_destroy(mock_engine);
    
    test_end("Rect Collision Detection Tests");
}

static void test_rect_watcher_system() {
    test_begin("Rect Watcher System Tests");
    
    EseLuaEngine *mock_engine = lua_engine_create();
    EseRect *rect = rect_create(mock_engine);
    
    TEST_ASSERT_NOT_NULL(rect, "Rect should be created for watcher tests");
    
    if (rect) {
        // Reset watcher state
        rect_watcher_called = false;
        last_watched_rect = NULL;
        last_rect_watcher_userdata = NULL;
        
        // Test adding a watcher
        void *test_userdata = (void*)0x12345678;
        bool add_result = rect_add_watcher(rect, test_rect_watcher_callback, test_userdata);
        TEST_ASSERT(add_result, "rect_add_watcher should return true on success");
        
        // Test that watcher is called when x changes
        rect_set_x(rect, 50.0f);
        TEST_ASSERT(rect_watcher_called, "Watcher should be called when x coordinate changes");
        TEST_ASSERT_POINTER_EQUAL(rect, last_watched_rect, "Watcher should receive correct rect pointer");
        TEST_ASSERT_POINTER_EQUAL(test_userdata, last_rect_watcher_userdata, "Watcher should receive correct userdata");
        
        // Reset watcher state
        rect_watcher_called = false;
        last_watched_rect = NULL;
        last_rect_watcher_userdata = NULL;
        
        // Test that watcher is called when y changes
        rect_set_y(rect, 75.0f);
        TEST_ASSERT(rect_watcher_called, "Watcher should be called when y coordinate changes");
        TEST_ASSERT_POINTER_EQUAL(rect, last_watched_rect, "Watcher should receive correct rect pointer");
        TEST_ASSERT_POINTER_EQUAL(test_userdata, last_rect_watcher_userdata, "Watcher should receive correct userdata");
        
        // Reset watcher state
        rect_watcher_called = false;
        last_watched_rect = NULL;
        last_rect_watcher_userdata = NULL;
        
        // Test that watcher is called when width changes
        rect_set_width(rect, 200.0f);
        TEST_ASSERT(rect_watcher_called, "Watcher should be called when width changes");
        TEST_ASSERT_POINTER_EQUAL(rect, last_watched_rect, "Watcher should receive correct rect pointer");
        TEST_ASSERT_POINTER_EQUAL(test_userdata, last_rect_watcher_userdata, "Watcher should receive correct userdata");
        
        // Reset watcher state
        rect_watcher_called = false;
        last_watched_rect = NULL;
        last_rect_watcher_userdata = NULL;
        
        // Test that watcher is called when height changes
        rect_set_height(rect, 150.0f);
        TEST_ASSERT(rect_watcher_called, "Watcher should be called when height changes");
        TEST_ASSERT_POINTER_EQUAL(rect, last_watched_rect, "Watcher should receive correct rect pointer");
        TEST_ASSERT_POINTER_EQUAL(test_userdata, last_rect_watcher_userdata, "Watcher should receive correct userdata");
        
        // Reset watcher state
        rect_watcher_called = false;
        last_watched_rect = NULL;
        last_rect_watcher_userdata = NULL;
        
        // Test that watcher is called when rotation changes
        rect_set_rotation(rect, M_PI / 2.0f);
        TEST_ASSERT(rect_watcher_called, "Watcher should be called when rotation changes");
        TEST_ASSERT_POINTER_EQUAL(rect, last_watched_rect, "Watcher should receive correct rect pointer");
        TEST_ASSERT_POINTER_EQUAL(test_userdata, last_rect_watcher_userdata, "Watcher should receive correct userdata");
        
        // Test adding multiple watchers
        void *test_userdata2 = (void*)0x87654321;
        bool add_result2 = rect_add_watcher(rect, test_rect_watcher_callback, test_userdata2);
        TEST_ASSERT(add_result2, "Adding second watcher should succeed");
        
        // Test removing a watcher
        bool remove_result = rect_remove_watcher(rect, test_rect_watcher_callback, test_userdata);
        TEST_ASSERT(remove_result, "rect_remove_watcher should return true when removing existing watcher");
        
        // Test removing non-existent watcher
        bool remove_fake_result = rect_remove_watcher(rect, test_rect_watcher_callback, (void*)0x99999999);
        TEST_ASSERT(!remove_fake_result, "rect_remove_watcher should return false for non-existent watcher");
        
        // Test removing with NULL callback
        bool remove_null_result = rect_remove_watcher(rect, NULL, test_userdata2);
        TEST_ASSERT(!remove_null_result, "rect_remove_watcher should return false for NULL callback");
        
        // Test adding watcher to NULL rect
        bool add_null_result = rect_add_watcher(NULL, test_rect_watcher_callback, test_userdata);
        TEST_ASSERT(!add_null_result, "rect_add_watcher should return false for NULL rect");
        
        // Test adding NULL callback
        bool add_null_callback_result = rect_add_watcher(rect, NULL, test_userdata);
        TEST_ASSERT(!add_null_callback_result, "rect_add_watcher should return false for NULL callback");
        
        rect_destroy(rect);
    }
    
    lua_engine_destroy(mock_engine);
    
    test_end("Rect Watcher System Tests");
}

static void test_rect_lua_integration() {
    test_begin("Rect Lua Integration Tests");
    
    EseLuaEngine *mock_engine = lua_engine_create();
    EseRect *rect = rect_create(mock_engine);
    
    TEST_ASSERT_NOT_NULL(rect, "Rect should be created for Lua integration tests");
    
    if (rect) {
        // Test basic functionality without Lua operations
        TEST_ASSERT_EQUAL(0, rect_get_lua_ref_count(rect), "New rect should start with ref count 0");
        
        // Test LUA_NOREF - check for any negative value
        int lua_ref = rect_get_lua_ref(rect);
        TEST_ASSERT(lua_ref < 0, "New rect should start with negative LUA_NOREF value");
        printf("ℹ INFO: Actual LUA_NOREF value: %d\n", lua_ref);
        
        rect_destroy(rect);
    }
    
    lua_engine_destroy(mock_engine);
    
    test_end("Rect Lua Integration Tests");
}

static void test_rect_lua_script_api() {
    test_begin("Rect Lua Script API Tests");

    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for Lua script API tests");

    if (engine) {
        // Initialize the Rect Lua type
        rect_lua_init(engine);
        
        // Load the test script
        bool load_result = lua_engine_load_script_from_string(engine, test_rect_lua_script, "test_rect_script", "RECT_TEST_MODULE");
        TEST_ASSERT(load_result, "Test script should load successfully");
        
        if (load_result) {
            // Create an instance of the script
            int instance_ref = lua_engine_instance_script(engine, "test_rect_script");
            TEST_ASSERT(instance_ref > 0, "Script instance should be created successfully");
            
            if (instance_ref > 0) {
                // Create a dummy self object for function calls
                lua_State* L = engine->runtime;
                lua_newtable(L);  // Create empty table as dummy self
                int dummy_self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
                
                // Test test_rect_creation
                EseLuaValue *result = lua_value_create_nil("result");
                bool exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "test_rect_creation", 0, NULL, result);
                TEST_ASSERT(exec_result, "test_rect_creation should execute successfully");
                TEST_ASSERT(result->type == 1, "test_rect_creation should return boolean"); // LUA_VAL_BOOL = 1
                TEST_ASSERT(result->value.boolean == true, "test_rect_creation should return true");
                
                // Test test_rect_properties
                lua_value_set_nil(result);
                exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "test_rect_properties", 0, NULL, result);
                TEST_ASSERT(exec_result, "test_rect_properties should execute successfully");
                TEST_ASSERT(result->type == 1, "test_rect_properties should return boolean");
                TEST_ASSERT(result->value.boolean == true, "test_rect_properties should return true");
                
                // Test test_rect_operations
                lua_value_set_nil(result);
                exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "test_rect_operations", 0, NULL, result);
                TEST_ASSERT(exec_result, "test_rect_operations should execute successfully");
                TEST_ASSERT(result->type == 1, "test_rect_operations should return boolean");
                TEST_ASSERT(result->value.boolean == true, "test_rect_operations should return true");
                
                // Clean up result
                lua_value_free(result);
                
                // Clean up dummy self and instance
                luaL_unref(L, LUA_REGISTRYINDEX, dummy_self_ref);
                lua_engine_instance_remove(engine, instance_ref);
            }
        }
        
        lua_engine_destroy(engine);
    }

    test_end("Rect Lua Script API Tests");
}
