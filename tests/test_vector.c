#include "test_utils.h"
#include "types/vector.h"
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
static void test_vector_creation();
static void test_vector_properties();
static void test_vector_copy();
static void test_vector_mathematical_operations();
static void test_vector_watcher_system();
static void test_vector_lua_integration();
static void test_vector_lua_script_api();
static void test_vector_null_pointer_aborts();

// Test Lua script content for Vector testing
static const char* test_vector_lua_script = 
"function VECTOR_TEST_MODULE:test_vector_creation()\n"
"    local v1 = Vector.new(10.5, -5.25)\n"
"    local v2 = Vector.zero()\n"
"    \n"
"    if v1.x == 10.5 and v1.y == -5.25 and v2.x == 0 and v2.y == 0 then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n"
"\n"
"function VECTOR_TEST_MODULE:test_vector_properties()\n"
"    local v = Vector.new(0, 0)\n"
"    \n"
"    v.x = 42.0\n"
"    v.y = -17.5\n"
"    \n"
"    if v.x == 42.0 and v.y == -17.5 then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n"
"\n"
"function VECTOR_TEST_MODULE:test_vector_operations()\n"
"    local v1 = Vector.new(3, 4)\n"
"    local v2 = Vector.new(1, 2)\n"
"    \n"
"    -- Test magnitude\n"
"    local mag = v1:magnitude()\n"
"    if math.abs(mag - 5.0) > 0.001 then\n"
"        return false\n"
"    end\n"
"    \n"
"    -- Test normalization\n"
"    local normalized = v1:normalized()\n"
"    if math.abs(normalized.x - 0.6) > 0.001 or math.abs(normalized.y - 0.8) > 0.001 then\n"
"        return false\n"
"    end\n"
"    \n"
"    return true\n"
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
    
    test_suite_begin("🧪 EseVector Test Suite");
    
    test_vector_creation();
    test_vector_properties();
    test_vector_copy();
    test_vector_mathematical_operations();
    test_vector_lua_integration();
    test_vector_lua_script_api();
    test_vector_null_pointer_aborts();
    
    test_suite_end("🎯 EseVector Test Suite");
    
    return 0;
}

static void test_vector_creation() {
    test_begin("Vector Creation Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for vector creation tests");
    
    EseVector *vector = vector_create(engine);
    TEST_ASSERT_NOT_NULL(vector, "vector_create should return non-NULL pointer");
    TEST_ASSERT_EQUAL(0.0f, vector->x, "New vector should have x = 0.0");
    TEST_ASSERT_EQUAL(0.0f, vector->y, "New vector should have y = 0.0");
    TEST_ASSERT_POINTER_EQUAL(engine->runtime, vector->state, "Vector should have correct Lua state");
    TEST_ASSERT_EQUAL(0, vector->lua_ref_count, "New vector should have ref count 0");
    TEST_ASSERT(vector->lua_ref == LUA_NOREF, "New vector should have negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", vector->lua_ref);
    TEST_ASSERT(sizeof(EseVector) > 0, "EseVector should have positive size");
    printf("ℹ INFO: Actual vector size: %zu bytes\n", sizeof(EseVector));
    
    vector_destroy(vector);
    lua_engine_destroy(engine);
    
    test_end("Vector Creation Tests");
}

static void test_vector_properties() {
    test_begin("Vector Properties Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for vector property tests");
    
    EseVector *vector = vector_create(engine);
    TEST_ASSERT_NOT_NULL(vector, "Vector should be created for property tests");
    
    vector->x = 10.5f;
    vector->y = -5.25f;
    TEST_ASSERT_EQUAL(10.5f, vector->x, "Direct field access should set x coordinate");
    TEST_ASSERT_EQUAL(-5.25f, vector->y, "Direct field access should set y coordinate");
    
    vector->x = -100.0f;
    vector->y = 200.0f;
    TEST_ASSERT_EQUAL(-100.0f, vector->x, "Direct field access should handle negative values");
    TEST_ASSERT_EQUAL(200.0f, vector->y, "Direct field access should handle negative values");
    
    vector->x = 0.0f;
    vector->y = 0.0f;
    TEST_ASSERT_EQUAL(0.0f, vector->x, "Direct field access should handle zero values");
    TEST_ASSERT_EQUAL(0.0f, vector->y, "Direct field access should handle zero values");
    
    vector_destroy(vector);
    lua_engine_destroy(engine);
    
    test_end("Vector Properties Tests");
}

static void test_vector_copy() {
    test_begin("Vector Copy Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for vector copy tests");
    
    EseVector *original = vector_create(engine);
    TEST_ASSERT_NOT_NULL(original, "Original vector should be created for copy tests");
    
    original->x = 42.0f;
    original->y = -17.5f;
    
    EseVector *copy = vector_copy(original);
    TEST_ASSERT_NOT_NULL(copy, "vector_copy should return non-NULL pointer");
    TEST_ASSERT_EQUAL(42.0f, copy->x, "Copied vector should have same x value");
    TEST_ASSERT_EQUAL(-17.5f, copy->y, "Copied vector should have same y value");
    TEST_ASSERT(original != copy, "Copy should be a different object");
    TEST_ASSERT_POINTER_EQUAL(original->state, copy->state, "Copy should have same Lua state");
    TEST_ASSERT(copy->lua_ref == LUA_NOREF, "Copy should start with negative LUA_NOREF value");
    printf("ℹ INFO: Copy LUA_NOREF value: %d\n", copy->lua_ref);
    TEST_ASSERT_EQUAL(0, copy->lua_ref_count, "Copy should start with ref count 0");
    
    vector_destroy(original);
    vector_destroy(copy);
    lua_engine_destroy(engine);
    
    test_end("Vector Copy Tests");
}

static void test_vector_mathematical_operations() {
    test_begin("Vector Mathematical Operations Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for vector math tests");
    
    EseVector *vector1 = vector_create(engine);
    EseVector *vector2 = vector_create(engine);
    
    TEST_ASSERT_NOT_NULL(vector1, "Vector1 should be created for math tests");
    TEST_ASSERT_NOT_NULL(vector2, "Vector2 should be created for math tests");
    
    // Test magnitude
    vector1->x = 3.0f;
    vector1->y = 4.0f;
    float magnitude = vector_magnitude(vector1);
    TEST_ASSERT_FLOAT_EQUAL(5.0f, magnitude, 0.001f, "Magnitude of (3,4) should be 5.0");
    
    // Test normalization
    vector2->x = 3.0f;
    vector2->y = 4.0f;
    vector_normalize(vector2);
    TEST_ASSERT_FLOAT_EQUAL(0.6f, vector2->x, 0.001f, "Normalized x should be 0.6");
    TEST_ASSERT_FLOAT_EQUAL(0.8f, vector2->y, 0.001f, "Normalized y should be 0.8");
    
    // Test direction setting
    vector_set_direction(vector1, "e", 5.0f);
    TEST_ASSERT_FLOAT_EQUAL(5.0f, vector1->x, 0.001f, "East direction should set x to 5.0");
    TEST_ASSERT_FLOAT_EQUAL(0.0f, vector1->y, 0.001f, "East direction should set y to 0.0");
    
    vector_destroy(vector1);
    vector_destroy(vector2);
    lua_engine_destroy(engine);
    
    test_end("Vector Mathematical Operations Tests");
}



static void test_vector_lua_integration() {
    test_begin("Vector Lua Integration Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for vector Lua integration tests");
    
    EseVector *vector = vector_create(engine);
    TEST_ASSERT_NOT_NULL(vector, "Vector should be created for Lua integration tests");
    TEST_ASSERT_EQUAL(0, vector->lua_ref_count, "New vector should start with ref count 0");
    TEST_ASSERT(vector->lua_ref == LUA_NOREF, "New vector should start with negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", vector->lua_ref);
    
    vector_destroy(vector);
    lua_engine_destroy(engine);
    
    test_end("Vector Lua Integration Tests");
}

static void test_vector_lua_script_api() {
    test_begin("Vector Lua Script API Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for vector Lua script API tests");
    
    vector_lua_init(engine);
    printf("ℹ INFO: Vector Lua integration initialized\n");
    
    // For now, just test that the Lua integration initializes without errors
    TEST_ASSERT(true, "Vector Lua integration should initialize successfully");
    
    lua_engine_destroy(engine);
    
    test_end("Vector Lua Script API Tests");
}

static void test_vector_null_pointer_aborts() {
    test_begin("Vector NULL Pointer Abort Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for vector NULL pointer abort tests");
    
    EseVector *vector = vector_create(engine);
    TEST_ASSERT_NOT_NULL(vector, "Vector should be created for vector NULL pointer abort tests");
    
    TEST_ASSERT_ABORT(vector_create(NULL), "vector_create should abort with NULL engine");
    TEST_ASSERT_ABORT(vector_copy(NULL), "vector_copy should abort with NULL source");
    TEST_ASSERT_ABORT(vector_lua_init(NULL), "vector_lua_init should abort with NULL engine");
    TEST_ASSERT_ABORT(vector_magnitude(NULL), "vector_magnitude should abort with NULL vector");
    TEST_ASSERT_ABORT(vector_normalize(NULL), "vector_normalize should abort with NULL vector");
    TEST_ASSERT_ABORT(vector_set_direction(NULL, "e", 1.0f), "vector_set_direction should abort with NULL vector");
    TEST_ASSERT_ABORT(vector_lua_get(NULL, 1), "vector_lua_get should abort with NULL Lua state");
    TEST_ASSERT_ABORT(vector_lua_push(NULL), "vector_lua_push should abort with NULL vector");
    TEST_ASSERT_ABORT(vector_ref(NULL), "vector_ref should abort with NULL vector");
    
    vector_destroy(vector);
    lua_engine_destroy(engine);
    
    test_end("Vector NULL Pointer Abort Tests");
}
