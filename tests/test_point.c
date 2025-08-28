#include "test_utils.h"
#include "types/point.h"
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
static void test_point_creation();
static void test_point_properties();
static void test_point_copy();
static void test_point_mathematical_operations();
static void test_point_watcher_system();
static void test_point_lua_integration();
static void test_point_lua_script_api();

// Test Lua script content for Point testing
static const char* test_point_lua_script = 
"function POINT_TEST_MODULE:test_point_creation()\n"
"    local p1 = Point.new(10.5, -5.25)\n"
"    local p2 = Point.zero()\n"
"    \n"
"    if p1.x == 10.5 and p1.y == -5.25 and p2.x == 0 and p2.y == 0 then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n"
"\n"
"function POINT_TEST_MODULE:test_point_properties()\n"
"    local p = Point.new(0, 0)\n"
"    \n"
"    p.x = 42.0\n"
"    p.y = -17.5\n"
"    \n"
"    if p.x == 42.0 and p.y == -17.5 then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n"
"\n"
"function POINT_TEST_MODULE:test_point_operations()\n"
"    local p1 = Point.new(1, 2)\n"
"    local p2 = Point.new(3, 4)\n"
"    \n"
"    -- Test arithmetic operations if they exist\n"
"    if p1.x + p2.x == 4 and p1.y + p2.y == 6 then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n";

// Mock watcher callback for testing
static bool watcher_called = false;
static EsePoint *last_watched_point = NULL;
static void *last_watcher_userdata = NULL;

static void test_watcher_callback(EsePoint *point, void *userdata) {
    watcher_called = true;
    last_watched_point = point;
    last_watcher_userdata = userdata;
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
    
    test_suite_begin("🧪 EsePoint Test Suite");
    
    // Initialize required systems
    log_init();
    
    test_point_creation();
    test_point_properties();
    test_point_copy();
    test_point_mathematical_operations();
    test_point_watcher_system();
    test_point_lua_integration();
    test_point_lua_script_api();
    
    test_suite_end("🎯 EsePoint Test Suite");
        
    return 0;
}

static void test_point_creation() {
    test_begin("Point Creation Tests");
    
    EseLuaEngine *mock_engine = lua_engine_create();
    
    // Test point_create
    EsePoint *point = point_create(mock_engine);
    TEST_ASSERT_NOT_NULL(point, "point_create should return non-NULL pointer");
    
    if (point) {
        TEST_ASSERT_EQUAL(0.0f, point_get_x(point), "New point should have x = 0.0");
        TEST_ASSERT_EQUAL(0.0f, point_get_y(point), "New point should have y = 0.0");
        TEST_ASSERT_POINTER_EQUAL(mock_engine->runtime, point_get_state(point), "Point should have correct Lua state");
        TEST_ASSERT_EQUAL(0, point_get_lua_ref_count(point), "New point should have ref count 0");
        
        // Test point_sizeof
        size_t actual_size = point_sizeof();
        TEST_ASSERT(actual_size > 0, "point_sizeof should return positive size");
        printf("ℹ INFO: Actual point size: %zu bytes\n", actual_size);
        
        point_destroy(point);
    }
    
    lua_engine_destroy(mock_engine);
    
    test_end("Point Creation Tests");
}

static void test_point_properties() {
    test_begin("Point Properties Tests");
    
    EseLuaEngine *mock_engine = lua_engine_create();
    EsePoint *point = point_create(mock_engine);
    
    TEST_ASSERT_NOT_NULL(point, "Point should be created for property tests");
    
    if (point) {
        // Test setting and getting x coordinate
        point_set_x(point, 10.5f);
        TEST_ASSERT_FLOAT_EQUAL(10.5f, point_get_x(point), 0.001f, "point_set_x should set x coordinate");
        
        // Test setting and getting y coordinate
        point_set_y(point, -5.25f);
        TEST_ASSERT_FLOAT_EQUAL(-5.25f, point_get_y(point), 0.001f, "point_set_y should set y coordinate");
        
        // Test setting negative values
        point_set_x(point, -100.0f);
        point_set_y(point, 200.0f);
        TEST_ASSERT_FLOAT_EQUAL(-100.0f, point_get_x(point), 0.001f, "point_set_x should handle negative values");
        TEST_ASSERT_FLOAT_EQUAL(200.0f, point_get_y(point), 0.001f, "point_set_y should handle positive values");
        
        // Test setting zero values
        point_set_x(point, 0.0f);
        point_set_y(point, 0.0f);
        TEST_ASSERT_FLOAT_EQUAL(0.0f, point_get_x(point), 0.001f, "point_set_x should handle zero values");
        TEST_ASSERT_FLOAT_EQUAL(0.0f, point_get_y(point), 0.001f, "point_set_y should handle zero values");
        
        point_destroy(point);
    }
    
    lua_engine_destroy(mock_engine);
    
    test_end("Point Properties Tests");
}

static void test_point_copy() {
    test_begin("Point Copy Tests");
    
    EseLuaEngine *mock_engine = lua_engine_create();
    EsePoint *original = point_create(mock_engine);
    
    TEST_ASSERT_NOT_NULL(original, "Original point should be created for copy tests");
    
    if (original) {
        // Set some values
        point_set_x(original, 42.0f);
        point_set_y(original, -17.5f);
        
        // Test point_copy
        EsePoint *copy = point_copy(original);
        TEST_ASSERT_NOT_NULL(copy, "point_copy should return non-NULL pointer");
        
        if (copy) {
            // Verify values are copied correctly
            TEST_ASSERT_FLOAT_EQUAL(42.0f, point_get_x(copy), 0.001f, "Copied point should have same x value");
            TEST_ASSERT_FLOAT_EQUAL(-17.5f, point_get_y(copy), 0.001f, "Copied point should have same y value");
            
            // Verify it's a different object
            TEST_ASSERT(original != copy, "Copy should be a different object");
            
            // Verify Lua state is copied
            TEST_ASSERT_POINTER_EQUAL(point_get_state(original), point_get_state(copy), "Copy should have same Lua state");
            
            // Verify copy starts with no Lua references
            int copy_lua_ref = point_get_lua_ref(copy);
            TEST_ASSERT(copy_lua_ref < 0, "Copy should start with negative LUA_NOREF value");
            printf("ℹ INFO: Copy LUA_NOREF value: %d\n", copy_lua_ref);
            TEST_ASSERT_EQUAL(0, point_get_lua_ref_count(copy), "Copy should start with ref count 0");
            
            point_destroy(copy);
        }
        
        point_destroy(original);
    }
    
    lua_engine_destroy(mock_engine);
    
    test_end("Point Copy Tests");
}

static void test_point_mathematical_operations() {
    test_begin("Point Mathematical Operations Tests");
    
    EseLuaEngine *mock_engine = lua_engine_create();
    
    // Create test points
    EsePoint *point1 = point_create(mock_engine);
    EsePoint *point2 = point_create(mock_engine);
    EsePoint *point3 = point_create(mock_engine);
    
    TEST_ASSERT_NOT_NULL(point1, "Point1 should be created for math tests");
    TEST_ASSERT_NOT_NULL(point2, "Point2 should be created for math tests");
    TEST_ASSERT_NOT_NULL(point3, "Point3 should be created for math tests");
    
    if (point1 && point2 && point3) {
        // Test distance calculations
        point_set_x(point1, 0.0f);
        point_set_y(point1, 0.0f);
        point_set_x(point2, 3.0f);
        point_set_y(point2, 4.0f);
        
        float distance = point_distance(point1, point2);
        TEST_ASSERT_FLOAT_EQUAL(5.0f, distance, 0.001f, "Distance between (0,0) and (3,4) should be 5.0");
        
        float distance_squared = point_distance_squared(point1, point2);
        TEST_ASSERT_FLOAT_EQUAL(25.0f, distance_squared, 0.001f, "Squared distance between (0,0) and (3,4) should be 25.0");
        
        // Test distance with negative coordinates
        point_set_x(point3, -3.0f);
        point_set_y(point3, -4.0f);
        
        float distance_negative = point_distance(point1, point3);
        TEST_ASSERT_FLOAT_EQUAL(5.0f, distance_negative, 0.001f, "Distance between (0,0) and (-3,-4) should be 5.0");
        
        // Test distance between two non-origin points
        float distance_between = point_distance(point2, point3);
        TEST_ASSERT_FLOAT_EQUAL(10.0f, distance_between, 0.001f, "Distance between (3,4) and (-3,-4) should be 10.0");
        
        // Test with NULL pointers (should return 0.0f)
        float null_distance = point_distance(NULL, point1);
        TEST_ASSERT_FLOAT_EQUAL(0.0f, null_distance, 0.001f, "Distance with NULL first point should return 0.0");
        
        null_distance = point_distance(point1, NULL);
        TEST_ASSERT_FLOAT_EQUAL(0.0f, null_distance, 0.001f, "Distance with NULL second point should return 0.0");
        
        null_distance = point_distance(NULL, NULL);
        TEST_ASSERT_FLOAT_EQUAL(0.0f, null_distance, 0.001f, "Distance with both NULL points should return 0.0");
        
        point_destroy(point1);
        point_destroy(point2);
        point_destroy(point3);
    }
    
    lua_engine_destroy(mock_engine);
    
    test_end("Point Mathematical Operations Tests");
}

static void test_point_watcher_system() {
    test_begin("Point Watcher System Tests");
    
    EseLuaEngine *mock_engine = lua_engine_create();
    EsePoint *point = point_create(mock_engine);
    
    TEST_ASSERT_NOT_NULL(point, "Point should be created for watcher tests");
    
    if (point) {
        // Reset watcher state
        watcher_called = false;
        last_watched_point = NULL;
        last_watcher_userdata = NULL;
        
        // Test adding a watcher
        void *test_userdata = (void*)0x12345678;
        bool add_result = point_add_watcher(point, test_watcher_callback, test_userdata);
        TEST_ASSERT(add_result, "point_add_watcher should return true on success");
        
        // Test that watcher is called when x changes
        point_set_x(point, 50.0f);
        TEST_ASSERT(watcher_called, "Watcher should be called when x coordinate changes");
        TEST_ASSERT_POINTER_EQUAL(point, last_watched_point, "Watcher should receive correct point pointer");
        TEST_ASSERT_POINTER_EQUAL(test_userdata, last_watcher_userdata, "Watcher should receive correct userdata");
        
        // Reset watcher state
        watcher_called = false;
        last_watched_point = NULL;
        last_watcher_userdata = NULL;
        
        // Test that watcher is called when y changes
        point_set_y(point, 75.0f);
        TEST_ASSERT(watcher_called, "Watcher should be called when y coordinate changes");
        TEST_ASSERT_POINTER_EQUAL(point, last_watched_point, "Watcher should receive correct point pointer");
        TEST_ASSERT_POINTER_EQUAL(test_userdata, last_watcher_userdata, "Watcher should receive correct userdata");
        
        // Test adding multiple watchers
        void *test_userdata2 = (void*)0x87654321;
        bool add_result2 = point_add_watcher(point, test_watcher_callback, test_userdata2);
        TEST_ASSERT(add_result2, "Adding second watcher should succeed");
        
        // Test removing a watcher
        bool remove_result = point_remove_watcher(point, test_watcher_callback, test_userdata);
        TEST_ASSERT(remove_result, "point_remove_watcher should return true when removing existing watcher");
        
        // Test removing non-existent watcher
        bool remove_fake_result = point_remove_watcher(point, test_watcher_callback, (void*)0x99999999);
        TEST_ASSERT(!remove_fake_result, "point_remove_watcher should return false for non-existent watcher");
        
        // Test removing with NULL callback
        bool remove_null_result = point_remove_watcher(point, NULL, test_userdata2);
        TEST_ASSERT(!remove_null_result, "point_remove_watcher should return false for NULL callback");
        
        // Test adding watcher to NULL point
        bool add_null_result = point_add_watcher(NULL, test_watcher_callback, test_userdata);
        TEST_ASSERT(!add_null_result, "point_add_watcher should return false for NULL point");
        
        // Test adding NULL callback
        bool add_null_callback_result = point_add_watcher(point, NULL, test_userdata);
        TEST_ASSERT(!add_null_callback_result, "point_add_watcher should return false for NULL callback");
        
        point_destroy(point);
    }
    
    lua_engine_destroy(mock_engine);
    
    test_end("Point Watcher System Tests");
}

static void test_point_lua_integration() {
    test_begin("Point Lua Integration Tests");
    
    EseLuaEngine *mock_engine = lua_engine_create();
    EsePoint *point = point_create(mock_engine);
    
    TEST_ASSERT_NOT_NULL(point, "Point should be created for Lua integration tests");
    
    if (point) {
        // Test basic functionality without Lua operations
        TEST_ASSERT_EQUAL(0, point_get_lua_ref_count(point), "New point should start with ref count 0");
        
        // Test LUA_NOREF - check for any negative value (LUA_NOREF is typically -1 but may vary)
        int lua_ref = point_get_lua_ref(point);
        TEST_ASSERT(lua_ref < 0, "New point should have negative LUA_NOREF value");
        printf("ℹ INFO: Actual LUA_NOREF value: %d\n", lua_ref);
        
        point_destroy(point);
    }
    
    lua_engine_destroy(mock_engine);
    
    test_end("Point Lua Integration Tests");
}

static void test_point_lua_script_api() {
    test_begin("Point Lua Script API Tests");

    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for Lua script API tests");

    if (engine) {
        // Initialize the Point Lua type
        point_lua_init(engine);
        
        // Load the test script
        bool load_result = lua_engine_load_script_from_string(engine, test_point_lua_script, "test_point_script", "POINT_TEST_MODULE");
        TEST_ASSERT(load_result, "Test script should load successfully");
        
        if (load_result) {
            // Create an instance of the script
            int instance_ref = lua_engine_instance_script(engine, "test_point_script");
            TEST_ASSERT(instance_ref > 0, "Script instance should be created successfully");
            
            if (instance_ref > 0) {
                // Create a dummy self object for function calls
                lua_State* L = engine->runtime;
                lua_newtable(L);  // Create empty table as dummy self
                int dummy_self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
                
                // Test test_point_creation
                EseLuaValue *result = lua_value_create_nil("result");
                bool exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "test_point_creation", 0, NULL, result);
                TEST_ASSERT(exec_result, "test_point_creation should execute successfully");
                TEST_ASSERT(result->type == 1, "test_point_creation should return boolean"); // LUA_VAL_BOOL = 1
                TEST_ASSERT(result->value.boolean == true, "test_point_creation should return true");
                
                // Test test_point_properties
                lua_value_set_nil(result);
                exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "test_point_properties", 0, NULL, result);
                TEST_ASSERT(exec_result, "test_point_properties should execute successfully");
                TEST_ASSERT(result->type == 1, "test_point_properties should return boolean");
                TEST_ASSERT(result->value.boolean == true, "test_point_properties should return true");
                
                // Test test_point_operations
                lua_value_set_nil(result);
                exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "test_point_operations", 0, NULL, result);
                TEST_ASSERT(exec_result, "test_point_operations should execute successfully");
                TEST_ASSERT(result->type == 1, "test_point_operations should return boolean");
                TEST_ASSERT(result->value.boolean == true, "test_point_operations should return true");
                
                // Clean up result
                lua_value_free(result);
                // Clean up dummy self and instance
                luaL_unref(L, LUA_REGISTRYINDEX, dummy_self_ref);
                lua_engine_instance_remove(engine, instance_ref);
            }
        }
        
        lua_engine_destroy(engine);
    }

    test_end("Point Lua Script API Tests");
}
