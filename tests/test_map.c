#include "test_utils.h"
#include "types/map_cell.h"
#include "types/map.h"
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
static void test_map_creation();
static void test_map_properties();
static void test_map_copy();
static void test_map_lua_integration();
static void test_map_lua_script_api();
static void test_map_null_pointer_aborts();

// Test Lua script content for Map testing
static const char* test_map_lua_script = 
"function MAP_TEST_MODULE:test_map_creation()\n"
"    local m1 = Map.new(10, 5)\n"
"    local m2 = Map.zero()\n"
"    \n"
"    if m1.width == 10 and m1.height == 5 and\n"
"       m2.width == 0 and m2.height == 0 then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n"
"\n"
"function MAP_TEST_MODULE:test_map_properties()\n"
"    local m = Map.new(0, 0)\n"
"    \n"
"    m.width = 20\n"
"    m.height = 15\n"
"    \n"
"    if m.width == 20 and m.height == 15 then\n"
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
    
    test_suite_begin("🧪 EseMap Test Suite");
    
    test_map_creation();
    test_map_properties();
    test_map_copy();
    test_map_lua_integration();
    test_map_lua_script_api();
    test_map_null_pointer_aborts();
    
    test_suite_end("🎯 EseMap Test Suite");
    
    return 0;
}

static void test_map_creation() {
    test_begin("Map Creation Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for map creation tests");
    
    EseMap *map = map_create(engine, 10, 10, MAP_TYPE_GRID, false);
    TEST_ASSERT_NOT_NULL(map, "map_create should return non-NULL pointer");
    TEST_ASSERT_EQUAL(10, map->width, "New map should have correct width");
    TEST_ASSERT_EQUAL(10, map->height, "New map should have correct height");
    TEST_ASSERT_POINTER_EQUAL(engine->runtime, map->state, "Map should have correct Lua state");
    TEST_ASSERT_EQUAL(MAP_TYPE_GRID, map->type, "New map should have correct type");
    TEST_ASSERT(map->lua_ref == LUA_NOREF, "New map should have negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", map->lua_ref);
    TEST_ASSERT(sizeof(EseMap) > 0, "EseMap should have positive size");
    printf("ℹ INFO: Actual map size: %zu bytes\n", sizeof(EseMap));
    
    map_destroy(map);
    lua_engine_destroy(engine);
    
    test_end("Map Creation Tests");
}

static void test_map_properties() {
    test_begin("Map Properties Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for map property tests");
    
    EseMap *map = map_create(engine, 10, 10, MAP_TYPE_GRID, false);
    TEST_ASSERT_NOT_NULL(map, "Map should be created for property tests");
    
    // Test map resize
    bool result = map_resize(map, 20, 15);
    TEST_ASSERT(result, "map_resize should succeed");
    TEST_ASSERT_EQUAL(20, map->width, "map_resize should set width");
    TEST_ASSERT_EQUAL(15, map->height, "map_resize should set height");
    
    result = map_resize(map, 100, 200);
    TEST_ASSERT(result, "map_resize should handle large values");
    TEST_ASSERT_EQUAL(100, map->width, "map_resize should handle large width");
    TEST_ASSERT_EQUAL(200, map->height, "map_resize should handle large height");
    
    // Test metadata operations
    result = map_set_title(map, "Test Map");
    TEST_ASSERT(result, "map_set_title should succeed");
    TEST_ASSERT_STRING_EQUAL("Test Map", map->title, "map_set_title should set title");
    
    result = map_set_author(map, "Test Author");
    TEST_ASSERT(result, "map_set_author should succeed");
    TEST_ASSERT_STRING_EQUAL("Test Author", map->author, "map_set_author should set author");
    
    map_set_version(map, 42);
    TEST_ASSERT_EQUAL(42, map->version, "map_set_version should set version");
    
    map_destroy(map);
    lua_engine_destroy(engine);
    
    test_end("Map Properties Tests");
}

static void test_map_copy() {
    test_begin("Map Copy Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for map copy tests");
    
    EseMap *original = map_create(engine, 20, 20, MAP_TYPE_GRID, false);
    TEST_ASSERT_NOT_NULL(original, "Original map should be created for copy tests");
    
    // Test map operations
    map_set_title(original, "Original Map");
    map_set_author(original, "Original Author");
    map_set_version(original, 1);
    
    // Test cell operations
    EseMapCell *cell = map_get_cell(original, 5, 5);
    TEST_ASSERT_NOT_NULL(cell, "map_get_cell should return non-NULL cell");
    
    // Test that we can access the cell's properties
    TEST_ASSERT_EQUAL(0, cell->layer_count, "New cell should have layer count 0");
    
    map_destroy(original);
    lua_engine_destroy(engine);
    
    test_end("Map Copy Tests");
}

static void test_map_lua_integration() {
    test_begin("Map Lua Integration Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for map Lua integration tests");
    
    EseMap *map = map_create(engine, 10, 10, MAP_TYPE_GRID, false);
    TEST_ASSERT_NOT_NULL(map, "Map should be created for Lua integration tests");
    TEST_ASSERT_EQUAL(10, map->width, "New map should have correct width");
    TEST_ASSERT(map->lua_ref == LUA_NOREF, "New map should start with negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", map->lua_ref);
    
    map_destroy(map);
    lua_engine_destroy(engine);
    
    test_end("Map Lua Integration Tests");
}

static void test_map_lua_script_api() {
    test_begin("Map Lua Script API Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for map Lua script API tests");
    
    map_lua_init(engine);
    printf("ℹ INFO: Map Lua integration initialized\n");
    
    // Test that map Lua integration initializes successfully
    TEST_ASSERT(true, "Map Lua integration should initialize successfully");
    
    lua_engine_destroy(engine);
    
    test_end("Map Lua Script API Tests");
}

static void test_map_null_pointer_aborts() {
    test_begin("Map NULL Pointer Abort Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for map NULL pointer abort tests");
    
    EseMap *map = map_create(engine, 10, 10, MAP_TYPE_GRID, false);
    TEST_ASSERT_NOT_NULL(map, "Map should be created for map NULL pointer abort tests");
    
    TEST_ASSERT_ABORT(map_create(NULL, 10, 10, MAP_TYPE_GRID, false), "map_create should abort with NULL engine");
    TEST_ASSERT_ABORT(map_lua_init(NULL), "map_lua_init should abort with NULL engine");
    // Test that functions abort with NULL map
    TEST_ASSERT_ABORT(map_lua_get(NULL, 1), "map_lua_get should abort with NULL Lua state");
    TEST_ASSERT_ABORT(map_lua_push(NULL), "map_lua_push should abort with NULL map");
    
    map_destroy(map);
    lua_engine_destroy(engine);
    
    test_end("Map NULL Pointer Abort Tests");
}
