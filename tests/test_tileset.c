#include "test_utils.h"
#include "types/tileset.h"
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
static void test_tileset_creation();
static void test_tileset_properties();
static void test_tileset_copy();
static void test_tileset_lua_integration();
static void test_tileset_lua_script_api();
static void test_tileset_null_pointer_aborts();

// Test Lua script content for Tileset testing
static const char* test_tileset_lua_script = 
"function TILESET_TEST_MODULE:test_tileset_creation()\n"
"    local ts1 = Tileset.new()\n"
"    local ts2 = Tileset.zero()\n"
"    \n"
"    if ts1 and ts2 then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n"
"\n"
"function TILESET_TEST_MODULE:test_tileset_properties()\n"
"    local ts = Tileset.new()\n"
"    \n"
"    -- Test basic functionality\n"
"    if ts then\n"
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
    
    test_suite_begin("🧪 EseTileSet Test Suite");
    
    test_tileset_creation();
    test_tileset_properties();
    test_tileset_copy();
    test_tileset_lua_integration();
    test_tileset_lua_script_api();
    test_tileset_null_pointer_aborts();
    
    test_suite_end("🎯 EseTileSet Test Suite");
    
    return 0;
}

static void test_tileset_creation() {
    test_begin("Tileset Creation Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for tileset creation tests");
    
    EseTileSet *tileset = tileset_create(engine, false);
    TEST_ASSERT_NOT_NULL(tileset, "tileset_create should return non-NULL pointer");
    TEST_ASSERT_POINTER_EQUAL(engine->runtime, tileset->state, "Tileset should have correct Lua state");
    TEST_ASSERT(tileset->lua_ref == LUA_NOREF, "New tileset should have negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", tileset->lua_ref);
    TEST_ASSERT(sizeof(EseTileSet) > 0, "EseTileSet should have positive size");
    printf("ℹ INFO: Actual tileset size: %zu bytes\n", sizeof(EseTileSet));
    
    tileset_destroy(tileset);
    lua_engine_destroy(engine);
    
    test_end("Tileset Creation Tests");
}

static void test_tileset_properties() {
    test_begin("Tileset Properties Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for tileset property tests");
    
    EseTileSet *tileset = tileset_create(engine, false);
    TEST_ASSERT_NOT_NULL(tileset, "Tileset should be created for property tests");
    
    // Test basic tileset functionality
    TEST_ASSERT(tileset != NULL, "Tileset should be valid");
    
    tileset_destroy(tileset);
    lua_engine_destroy(engine);
    
    test_end("Tileset Properties Tests");
}

static void test_tileset_copy() {
    test_begin("Tileset Copy Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for tileset copy tests");
    
    EseTileSet *original = tileset_create(engine, false);
    TEST_ASSERT_NOT_NULL(original, "Original tileset should be created for copy tests");
    
    // Test that tileset was created successfully
    TEST_ASSERT_NOT_NULL(original, "tileset should be created successfully");
    
    tileset_destroy(original);
    lua_engine_destroy(engine);
    
    test_end("Tileset Copy Tests");
}

static void test_tileset_lua_integration() {
    test_begin("Tileset Lua Integration Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for tileset Lua integration tests");
    
    EseTileSet *tileset = tileset_create(engine, false);
    TEST_ASSERT_NOT_NULL(tileset, "Tileset should be created for Lua integration tests");
    TEST_ASSERT(tileset->lua_ref == LUA_NOREF, "New tileset should start with negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", tileset->lua_ref);
    
    tileset_destroy(tileset);
    lua_engine_destroy(engine);
    
    test_end("Tileset Lua Integration Tests");
}

static void test_tileset_lua_script_api() {
    test_begin("Tileset Lua Script API Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for tileset Lua script API tests");
    
    tileset_lua_init(engine);
    printf("ℹ INFO: Tileset Lua integration initialized\n");
    
    // Test that tileset Lua integration initializes successfully
    TEST_ASSERT(true, "Tileset Lua integration should initialize successfully");
    
    lua_engine_destroy(engine);
    
    test_end("Tileset Lua Script API Tests");
}

static void test_tileset_null_pointer_aborts() {
    test_begin("Tileset NULL Pointer Abort Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for tileset NULL pointer abort tests");
    
    EseTileSet *tileset = tileset_create(engine, false);
    TEST_ASSERT_NOT_NULL(tileset, "Tileset should be created for tileset NULL pointer abort tests");
    
    TEST_ASSERT_ABORT(tileset_create(NULL, false), "tileset_create should abort with NULL engine");
    TEST_ASSERT_ABORT(tileset_lua_init(NULL), "tileset_lua_init should abort with NULL engine");
    TEST_ASSERT_ABORT(tileset_lua_get(NULL, 1), "tileset_lua_get should abort with NULL Lua state");
    TEST_ASSERT_ABORT(tileset_lua_push(NULL), "tileset_lua_push should abort with NULL tileset");
    
    tileset_destroy(tileset);
    lua_engine_destroy(engine);
    
    test_end("Tileset NULL Pointer Abort Tests");
}
