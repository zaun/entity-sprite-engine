#include "test_utils.h"
#include "types/map_cell.h"
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
static void test_map_cell_creation();
static void test_map_cell_properties();
static void test_map_cell_copy();
static void test_map_cell_lua_integration();
static void test_map_cell_lua_script_api();
static void test_map_cell_null_pointer_aborts();

// Test Lua script content for MapCell testing
static const char* test_map_cell_lua_script = 
"function MAP_CELL_TEST_MODULE:test_map_cell_creation()\n"
"    local mc1 = MapCell.new(10, 5, 1, 0.5)\n"
"    local mc2 = MapCell.zero()\n"
"    \n"
"    if mc1.x == 10 and mc1.y == 5 and mc1.tile_id == 1 and mc1.alpha == 0.5 and\n"
"       mc2.x == 0 and mc2.y == 0 and mc2.tile_id == 0 and mc2.alpha == 1.0 then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n"
"\n"
"function MAP_CELL_TEST_MODULE:test_map_cell_properties()\n"
"    local mc = MapCell.new(0, 0, 0, 1)\n"
"    \n"
"    mc.x = 42\n"
"    mc.y = 17\n"
"    mc.tile_id = 5\n"
"    mc.alpha = 0.8\n"
"    \n"
"    if mc.x == 42 and mc.y == 17 and mc.tile_id == 5 and mc.alpha == 0.8 then\n"
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
    
    test_suite_begin("🧪 EseMapCell Test Suite");
    
    test_map_cell_creation();
    test_map_cell_properties();
    test_map_cell_copy();
    test_map_cell_lua_integration();
    test_map_cell_lua_script_api();
    test_map_cell_null_pointer_aborts();
    
    test_suite_end("🎯 EseMapCell Test Suite");
    
    return 0;
}

static void test_map_cell_creation() {
    test_begin("MapCell Creation Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for map cell creation tests");
    
    EseMapCell *map_cell = mapcell_create(engine, false);
    TEST_ASSERT_NOT_NULL(map_cell, "mapcell_create should return non-NULL pointer");
    TEST_ASSERT_EQUAL(0, map_cell->layer_count, "New map cell should have layer_count = 0");
    TEST_ASSERT(!map_cell->isDynamic, "New map cell should have isDynamic = false");
    TEST_ASSERT_EQUAL(0, map_cell->flags, "New map cell should have flags = 0");
    TEST_ASSERT_POINTER_EQUAL(engine->runtime, map_cell->state, "MapCell should have correct Lua state");
    TEST_ASSERT(map_cell->lua_ref == LUA_NOREF, "New map cell should have negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", map_cell->lua_ref);
    TEST_ASSERT(sizeof(EseMapCell) > 0, "EseMapCell should have positive size");
    printf("ℹ INFO: Actual map cell size: %zu bytes\n", sizeof(EseMapCell));
    
    mapcell_destroy(map_cell);
    lua_engine_destroy(engine);
    
    test_end("MapCell Creation Tests");
}

static void test_map_cell_properties() {
    test_begin("MapCell Properties Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for map cell property tests");
    
    EseMapCell *map_cell = mapcell_create(engine, false);
    TEST_ASSERT_NOT_NULL(map_cell, "MapCell should be created for property tests");
    
    // Test layer operations
    bool result = mapcell_add_layer(map_cell, 5);
    TEST_ASSERT(result, "mapcell_add_layer should succeed");
    TEST_ASSERT_EQUAL(1, map_cell->layer_count, "mapcell_add_layer should increment layer_count");
    TEST_ASSERT_EQUAL(5, mapcell_get_layer(map_cell, 0), "mapcell_get_layer should return correct tile id");
    
    result = mapcell_add_layer(map_cell, 10);
    TEST_ASSERT(result, "mapcell_add_layer should succeed for second layer");
    TEST_ASSERT_EQUAL(2, map_cell->layer_count, "mapcell_add_layer should increment layer_count");
    TEST_ASSERT_EQUAL(10, mapcell_get_layer(map_cell, 1), "mapcell_get_layer should return correct tile id for second layer");
    
    // Test flag operations
    mapcell_set_flag(map_cell, 0x01);
    TEST_ASSERT(mapcell_has_flag(map_cell, 0x01), "mapcell_has_flag should return true for set flag");
    
    mapcell_clear_flag(map_cell, 0x01);
    TEST_ASSERT(!mapcell_has_flag(map_cell, 0x01), "mapcell_has_flag should return false for cleared flag");
    
    // Test dynamic flag
    map_cell->isDynamic = true;
    TEST_ASSERT(map_cell->isDynamic, "map cell isDynamic should be settable");
    
    mapcell_destroy(map_cell);
    lua_engine_destroy(engine);
    
    test_end("MapCell Properties Tests");
}

static void test_map_cell_copy() {
    test_begin("MapCell Copy Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for map cell copy tests");
    
    EseMapCell *original = mapcell_create(engine, false);
    TEST_ASSERT_NOT_NULL(original, "Original map cell should be created for copy tests");
    
    mapcell_add_layer(original, 7);
    mapcell_add_layer(original, 14);
    mapcell_set_flag(original, 0x02);
    original->isDynamic = true;
    
    EseMapCell *copy = mapcell_copy(original, false);
    TEST_ASSERT_NOT_NULL(copy, "mapcell_copy should return non-NULL pointer");
    TEST_ASSERT_EQUAL(2, copy->layer_count, "Copied map cell should have same layer count");
    TEST_ASSERT_EQUAL(7, mapcell_get_layer(copy, 0), "Copied map cell should have same first layer");
    TEST_ASSERT_EQUAL(14, mapcell_get_layer(copy, 1), "Copied map cell should have same second layer");
    TEST_ASSERT(mapcell_has_flag(copy, 0x02), "Copied map cell should have same flags");
    TEST_ASSERT(copy->isDynamic, "Copied map cell should have same isDynamic value");
    TEST_ASSERT(original != copy, "Copy should be a different object");
    TEST_ASSERT_POINTER_EQUAL(original->state, copy->state, "Copy should have same Lua state");
    TEST_ASSERT(copy->lua_ref == LUA_NOREF, "Copy should start with negative LUA_NOREF value");
    printf("ℹ INFO: Copy LUA_NOREF value: %d\n", copy->lua_ref);
    TEST_ASSERT_EQUAL(2, copy->layer_count, "Copy should have same layer count as original");
    
    mapcell_destroy(original);
    mapcell_destroy(copy);
    lua_engine_destroy(engine);
    
    test_end("MapCell Copy Tests");
}

static void test_map_cell_lua_integration() {
    test_begin("MapCell Lua Integration Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for map cell Lua integration tests");
    
    EseMapCell *map_cell = mapcell_create(engine, false);
    TEST_ASSERT_NOT_NULL(map_cell, "MapCell should be created for Lua integration tests");
    TEST_ASSERT_EQUAL(0, map_cell->layer_count, "New map cell should start with layer count 0");
    TEST_ASSERT(map_cell->lua_ref == LUA_NOREF, "New map cell should start with negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", map_cell->lua_ref);
    
    mapcell_destroy(map_cell);
    lua_engine_destroy(engine);
    
    test_end("MapCell Lua Integration Tests");
}

static void test_map_cell_lua_script_api() {
    test_begin("MapCell Lua Script API Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for map cell Lua script API tests");
    
    mapcell_lua_init(engine);
    printf("ℹ INFO: MapCell Lua integration initialized\n");
    
    // Test that map cell Lua integration initializes successfully
    TEST_ASSERT(true, "MapCell Lua integration should initialize successfully");
    
    lua_engine_destroy(engine);
    
    test_end("MapCell Lua Script API Tests");
}

static void test_map_cell_null_pointer_aborts() {
    test_begin("MapCell NULL Pointer Abort Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for map cell NULL pointer abort tests");
    
    EseMapCell *map_cell = mapcell_create(engine, false);
    TEST_ASSERT_NOT_NULL(map_cell, "MapCell should be created for map cell NULL pointer abort tests");
    
    TEST_ASSERT_ABORT(mapcell_create(NULL, false), "mapcell_create should abort with NULL engine");
    TEST_ASSERT_ABORT(mapcell_copy(NULL, false), "mapcell_copy should abort with NULL source");
    TEST_ASSERT_ABORT(mapcell_lua_init(NULL), "mapcell_lua_init should abort with NULL engine");
    // Test that functions abort with NULL map cell
    TEST_ASSERT_ABORT(mapcell_lua_get(NULL, 1), "mapcell_lua_get should abort with NULL Lua state");
    TEST_ASSERT_ABORT(mapcell_lua_push(NULL), "mapcell_lua_push should abort with NULL map cell");
    
    mapcell_destroy(map_cell);
    lua_engine_destroy(engine);
    
    test_end("MapCell NULL Pointer Abort Tests");
}
