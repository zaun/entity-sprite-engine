/*
* test_collision_hit.c - Unity-based tests for collision hit functionality
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

#include "../src/types/collision_hit.h"
#include "../src/types/rect.h"
#include "../src/types/map.h"
#include "../src/entity/entity.h"
#include "../src/entity/entity_lua.h"
#include "../src/scripting/lua_engine.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

/**
* C API Test Functions Declarations
*/
static void test_collision_hit_create_requires_engine(void);
static void test_collision_hit_create_defaults(void);
static void test_collision_hit_state_set_get(void);
static void test_collision_hit_entity_target_set_get(void);
static void test_collision_hit_rect_set_get_and_clear(void);
static void test_collision_hit_map_set_get_and_kind_switch(void);
static void test_collision_hit_invalid_access_asserts(void);
static void test_collision_hit_copy(void);
static void test_collision_hit_ref_unref_safe(void);
static void test_collision_hit_destroy_null_safe(void);

/**
* Lua API Test Functions Declarations
*/
static void test_collision_hit_lua_init(void);
static void test_collision_hit_lua_push_get(void);
static void test_collision_hit_lua_index_properties_collider(void);
static void test_collision_hit_lua_index_properties_map(void);
static void test_collision_hit_lua_invalid_property_kinds(void);
static void test_collision_hit_lua_readonly_properties(void);
static void test_collision_hit_lua_tostring(void);
static void test_collision_hit_lua_gc_with_ref(void);

/**
* Test suite setup and teardown
*/
static EseLuaEngine *g_engine = NULL;

void setUp(void) {
    g_engine = create_test_engine();
    // Ensure essential Lua bindings for entities are available when needed
    entity_lua_init(g_engine);
}

void tearDown(void) {
    lua_engine_destroy(g_engine);
}

/**
* Main test runner
*/
int main(void) {
    log_init();

    printf("\nEseCollisionHit Tests\n");
    printf("---------------------\n");

    UNITY_BEGIN();

    // C API
    RUN_TEST(test_collision_hit_create_requires_engine);
    RUN_TEST(test_collision_hit_create_defaults);
    RUN_TEST(test_collision_hit_state_set_get);
    RUN_TEST(test_collision_hit_entity_target_set_get);
    RUN_TEST(test_collision_hit_rect_set_get_and_clear);
    RUN_TEST(test_collision_hit_map_set_get_and_kind_switch);
    RUN_TEST(test_collision_hit_invalid_access_asserts);
    RUN_TEST(test_collision_hit_copy);
    RUN_TEST(test_collision_hit_ref_unref_safe);
    RUN_TEST(test_collision_hit_destroy_null_safe);

    // Lua API
    RUN_TEST(test_collision_hit_lua_init);
    RUN_TEST(test_collision_hit_lua_push_get);
    RUN_TEST(test_collision_hit_lua_index_properties_collider);
    RUN_TEST(test_collision_hit_lua_index_properties_map);
    RUN_TEST(test_collision_hit_lua_invalid_property_kinds);
    RUN_TEST(test_collision_hit_lua_readonly_properties);
    RUN_TEST(test_collision_hit_lua_tostring);
    RUN_TEST(test_collision_hit_lua_gc_with_ref);

    memory_manager.destroy();

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_collision_hit_create_requires_engine(void) {
    TEST_ASSERT_DEATH((ese_collision_hit_create(NULL)), "ese_collision_hit_create should abort with NULL engine");
}

static void test_collision_hit_create_defaults(void) {
    EseCollisionHit *hit = ese_collision_hit_create(g_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(hit, "CollisionHit should be created");

    TEST_ASSERT_EQUAL_INT_MESSAGE(COLLISION_KIND_COLLIDER, ese_collision_hit_get_kind(hit), "Default kind should be COLLIDER");
    TEST_ASSERT_EQUAL_INT_MESSAGE(COLLISION_STATE_ENTER, ese_collision_hit_get_state(hit), "Default state should be ENTER");
    TEST_ASSERT_NULL_MESSAGE(ese_collision_hit_get_entity(hit), "Default entity should be NULL");
    TEST_ASSERT_NULL_MESSAGE(ese_collision_hit_get_target(hit), "Default target should be NULL");
    TEST_ASSERT_NULL_MESSAGE(ese_collision_hit_get_rect(hit), "Default rect should be NULL for COLLIDER kind");

    // get_map on COLLIDER kind should abort
    TEST_ASSERT_DEATH((ese_collision_hit_get_map(hit)), "ese_collision_hit_get_map should abort for COLLIDER kind");

    ese_collision_hit_destroy(hit);
}

static void test_collision_hit_state_set_get(void) {
    EseCollisionHit *hit = ese_collision_hit_create(g_engine);
    ese_collision_hit_set_state(hit, COLLISION_STATE_STAY);
    TEST_ASSERT_EQUAL_INT_MESSAGE(COLLISION_STATE_STAY, ese_collision_hit_get_state(hit), "State should be STAY");
    ese_collision_hit_set_state(hit, COLLISION_STATE_LEAVE);
    TEST_ASSERT_EQUAL_INT_MESSAGE(COLLISION_STATE_LEAVE, ese_collision_hit_get_state(hit), "State should be LEAVE");
    ese_collision_hit_destroy(hit);
}

static void test_collision_hit_entity_target_set_get(void) {
    EseCollisionHit *hit = ese_collision_hit_create(g_engine);
    EseEntity *e1 = entity_create(g_engine);
    EseEntity *e2 = entity_create(g_engine);

    ese_collision_hit_set_entity(hit, e1);
    ese_collision_hit_set_target(hit, e2);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(e1, ese_collision_hit_get_entity(hit), "Entity getter should return value set");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(e2, ese_collision_hit_get_target(hit), "Target getter should return value set");

    entity_destroy(e1);
    entity_destroy(e2);
    ese_collision_hit_destroy(hit);
}

static void test_collision_hit_rect_set_get_and_clear(void) {
    EseCollisionHit *hit = ese_collision_hit_create(g_engine);

    // Start in COLLIDER kind by default
    EseRect *r = ese_rect_create(g_engine);
    ese_rect_set_x(r, 10.0f);
    ese_rect_set_y(r, 20.0f);
    ese_rect_set_width(r, 30.0f);
    ese_rect_set_height(r, 40.0f);

    ese_collision_hit_set_rect(hit, r);
    EseRect *owned = ese_collision_hit_get_rect(hit);
    TEST_ASSERT_NOT_NULL_MESSAGE(owned, "Rect should be stored on hit");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_rect_get_x(owned));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, ese_rect_get_y(owned));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.0f, ese_rect_get_width(owned));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 40.0f, ese_rect_get_height(owned));

    // Clear via NULL
    ese_collision_hit_set_rect(hit, NULL);
    TEST_ASSERT_NULL_MESSAGE(ese_collision_hit_get_rect(hit), "Rect should be cleared when set to NULL");

    ese_rect_destroy(r);
    ese_collision_hit_destroy(hit);
}

static void test_collision_hit_map_set_get_and_kind_switch(void) {
    EseCollisionHit *hit = ese_collision_hit_create(g_engine);

    // Switch to MAP and set map
    ese_collision_hit_set_kind(hit, COLLISION_KIND_MAP);
    EseMap *map = ese_map_create(g_engine, 8, 8, MAP_TYPE_GRID, true);
    ese_collision_hit_set_map(hit, map);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(map, ese_collision_hit_get_map(hit), "Map getter should return value set");

    // Setting rect in MAP kind should abort (argument value doesn't matter)
    TEST_ASSERT_DEATH((ese_collision_hit_set_rect(hit, NULL)), "ese_collision_hit_set_rect should abort for MAP kind");

    // Switching back to COLLIDER should clear map-side data (access should abort)
    ese_collision_hit_set_kind(hit, COLLISION_KIND_COLLIDER);
    TEST_ASSERT_DEATH((ese_collision_hit_get_map(hit)), "ese_collision_hit_get_map should abort after switching to COLLIDER");

    ese_map_destroy(map);
    ese_collision_hit_destroy(hit);
}

static void test_collision_hit_invalid_access_asserts(void) {
    EseCollisionHit *hit = ese_collision_hit_create(g_engine);

    // In COLLIDER kind, get_map should abort (already tested), and set_map should abort
    TEST_ASSERT_DEATH((ese_collision_hit_set_map(hit, NULL)), "ese_collision_hit_set_map should abort for COLLIDER kind");

    // Switch to MAP and ensure rect accessors assert
    ese_collision_hit_set_kind(hit, COLLISION_KIND_MAP);
    TEST_ASSERT_DEATH((ese_collision_hit_get_rect(hit)), "ese_collision_hit_get_rect should abort for MAP kind");

    ese_collision_hit_destroy(hit);
}

static void test_collision_hit_copy(void) {
    EseCollisionHit *hit = ese_collision_hit_create(g_engine);
    EseEntity *e1 = entity_create(g_engine);
    EseEntity *e2 = entity_create(g_engine);
    EseRect *r = ese_rect_create(g_engine);
    ese_rect_set_x(r, 1.0f);
    ese_rect_set_y(r, 2.0f);
    ese_rect_set_width(r, 3.0f);
    ese_rect_set_height(r, 4.0f);

    ese_collision_hit_set_entity(hit, e1);
    ese_collision_hit_set_target(hit, e2);
    ese_collision_hit_set_state(hit, COLLISION_STATE_STAY);
    ese_collision_hit_set_rect(hit, r);

    EseCollisionHit *copy = ese_collision_hit_copy(hit);
    TEST_ASSERT_NOT_NULL_MESSAGE(copy, "Copy should be created");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ese_collision_hit_get_kind(hit), ese_collision_hit_get_kind(copy), "Kind should match on copy");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ese_collision_hit_get_state(hit), ese_collision_hit_get_state(copy), "State should match on copy");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(ese_collision_hit_get_entity(hit), ese_collision_hit_get_entity(copy), "Entity should match on copy");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(ese_collision_hit_get_target(hit), ese_collision_hit_get_target(copy), "Target should match on copy");
    EseRect *src_rect = ese_collision_hit_get_rect(hit);
    EseRect *dst_rect = ese_collision_hit_get_rect(copy);
    TEST_ASSERT_NOT_NULL_MESSAGE(src_rect, "Source rect should exist");
    TEST_ASSERT_NOT_NULL_MESSAGE(dst_rect, "Copied rect should exist");
    TEST_ASSERT_TRUE_MESSAGE(src_rect != dst_rect, "Copy should have a distinct rect instance");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, ese_rect_get_x(src_rect), ese_rect_get_x(dst_rect));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, ese_rect_get_y(src_rect), ese_rect_get_y(dst_rect));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, ese_rect_get_width(src_rect), ese_rect_get_width(dst_rect));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, ese_rect_get_height(src_rect), ese_rect_get_height(dst_rect));

    ese_rect_destroy(r);
    entity_destroy(e1);
    entity_destroy(e2);
    ese_collision_hit_destroy(hit);
    ese_collision_hit_destroy(copy);
}

static void test_collision_hit_ref_unref_safe(void) {
    EseCollisionHit *hit = ese_collision_hit_create(g_engine);
    // Round-trip should not crash
    ese_collision_hit_ref(hit);
    ese_collision_hit_unref(hit);
    ese_collision_hit_destroy(hit);
}

static void test_collision_hit_destroy_null_safe(void) {
    // Should be safe (no crash)
    ese_collision_hit_destroy(NULL);
}

/**
* Lua API Test Functions
*/

static void test_collision_hit_lua_init(void) {
    lua_State *L = g_engine->runtime;

    // Before init
    luaL_getmetatable(L, COLLISION_HIT_META);
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Metatable should not exist before initialization");
    lua_pop(L, 1);

    lua_getglobal(L, "EseCollisionHit");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Global EseCollisionHit should not exist before initialization");
    lua_pop(L, 1);

    ese_collision_hit_lua_init(g_engine);

    // After init
    luaL_getmetatable(L, COLLISION_HIT_META);
    TEST_ASSERT_FALSE_MESSAGE(lua_isnil(L, -1), "Metatable should exist after initialization");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Metatable should be a table");
    lua_pop(L, 1);

    lua_getglobal(L, "EseCollisionHit");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Global EseCollisionHit should exist after initialization");
    // TYPE constants
    lua_getfield(L, -1, "TYPE");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "TYPE table should exist");
    lua_getfield(L, -1, "COLLIDER");
    TEST_ASSERT_EQUAL_INT_MESSAGE(COLLISION_KIND_COLLIDER, (int)lua_tointeger(L, -1), "TYPE.COLLIDER constant should match");
    lua_pop(L, 1);
    lua_getfield(L, -1, "MAP");
    TEST_ASSERT_EQUAL_INT_MESSAGE(COLLISION_KIND_MAP, (int)lua_tointeger(L, -1), "TYPE.MAP constant should match");
    lua_pop(L, 2);
    // STATE constants
    lua_getfield(L, -1, "STATE");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "STATE table should exist");
    lua_getfield(L, -1, "ENTER");
    TEST_ASSERT_EQUAL_INT_MESSAGE(COLLISION_STATE_ENTER, (int)lua_tointeger(L, -1), "STATE.ENTER constant should match");
    lua_pop(L, 1);
    lua_getfield(L, -1, "STAY");
    TEST_ASSERT_EQUAL_INT_MESSAGE(COLLISION_STATE_STAY, (int)lua_tointeger(L, -1), "STATE.STAY constant should match");
    lua_pop(L, 1);
    lua_getfield(L, -1, "LEAVE");
    TEST_ASSERT_EQUAL_INT_MESSAGE(COLLISION_STATE_LEAVE, (int)lua_tointeger(L, -1), "STATE.LEAVE constant should match");
    lua_pop(L, 2); // pop STATE and EseCollisionHit
}

static void test_collision_hit_lua_push_get(void) {
    ese_collision_hit_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    EseCollisionHit *hit = ese_collision_hit_create(g_engine);
    ese_collision_hit_ref(hit);
    ese_collision_hit_lua_push(hit);
    EseCollisionHit *extracted = ese_collision_hit_lua_get(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(hit, extracted, "Extracted hit should match original");
    lua_pop(L, 1);
    ese_collision_hit_unref(hit);
    ese_collision_hit_destroy(hit);
}

static void test_collision_hit_lua_index_properties_collider(void) {
    ese_rect_lua_init(g_engine);
    ese_collision_hit_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    EseCollisionHit *hit = ese_collision_hit_create(g_engine);
    ese_collision_hit_ref(hit);
    EseEntity *e1 = entity_create(g_engine);
    EseEntity *e2 = entity_create(g_engine);
    EseRect *r = ese_rect_create(g_engine);
    ese_rect_set_x(r, 7.0f);

    ese_collision_hit_set_entity(hit, e1);
    ese_collision_hit_set_target(hit, e2);
    ese_collision_hit_set_state(hit, COLLISION_STATE_STAY);
    ese_collision_hit_set_rect(hit, r);

    ese_collision_hit_lua_push(hit);

    // kind
    lua_getfield(L, -1, "kind");
    TEST_ASSERT_EQUAL_INT_MESSAGE(COLLISION_KIND_COLLIDER, (int)lua_tointeger(L, -1), "Lua getter kind should match");
    lua_pop(L, 1);

    // state
    lua_getfield(L, -1, "state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(COLLISION_STATE_STAY, (int)lua_tointeger(L, -1), "Lua getter state should match");
    lua_pop(L, 1);

    // entity
    lua_getfield(L, -1, "entity");
    EseEntity *got_entity = entity_lua_get(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(e1, got_entity, "Lua getter entity should match");
    lua_pop(L, 1);

    // target
    lua_getfield(L, -1, "target");
    EseEntity *got_target = entity_lua_get(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(e2, got_target, "Lua getter target should match");
    lua_pop(L, 1);

    // rect
    lua_getfield(L, -1, "rect");
    EseRect *got_rect = ese_rect_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(got_rect, "Lua getter rect should return rect");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.0f, ese_rect_get_x(got_rect));
    lua_pop(L, 1);

    // cleanup
    lua_pop(L, 1); // pop hit userdata
    ese_rect_destroy(r);
    entity_destroy(e1);
    entity_destroy(e2);
    ese_collision_hit_unref(hit);
    ese_collision_hit_destroy(hit);
}

static void test_collision_hit_lua_index_properties_map(void) {
    ese_map_lua_init(g_engine);
    ese_collision_hit_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    EseCollisionHit *hit = ese_collision_hit_create(g_engine);
    ese_collision_hit_ref(hit);
    ese_collision_hit_set_kind(hit, COLLISION_KIND_MAP);
    EseMap *map = ese_map_create(g_engine, 4, 4, MAP_TYPE_GRID, true);
    ese_collision_hit_set_map(hit, map);
    ese_collision_hit_set_cell_x(hit, 3);
    ese_collision_hit_set_cell_y(hit, 2);

    ese_collision_hit_lua_push(hit);

    // map
    lua_getfield(L, -1, "map");
    EseMap *got_map = ese_map_lua_get(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(map, got_map, "Lua getter map should match");
    lua_pop(L, 1);

    // cell_x
    lua_getfield(L, -1, "cell_x");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnumber(L, -1), "cell_x should be a number");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, (int)lua_tointeger(L, -1), "cell_x should match");
    lua_pop(L, 1);

    // cell_y
    lua_getfield(L, -1, "cell_y");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnumber(L, -1), "cell_y should be a number");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, (int)lua_tointeger(L, -1), "cell_y should match");
    lua_pop(L, 1);

    // cleanup
    lua_pop(L, 1); // pop hit userdata
    ese_map_destroy(map);
    ese_collision_hit_unref(hit);
    ese_collision_hit_destroy(hit);
}

static void test_collision_hit_lua_invalid_property_kinds(void) {
    ese_collision_hit_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    EseCollisionHit *hit = ese_collision_hit_create(g_engine);
    // Create a registry-backed userdata so repeated pushes reuse the same object
    ese_collision_hit_ref(hit);

    // COLLIDER kind: map/cell_x/cell_y should be nil
    ese_collision_hit_lua_push(hit);
    lua_getfield(L, -1, "map");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "map should be nil for COLLIDER kind");
    lua_pop(L, 1);
    lua_getfield(L, -1, "cell_x");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "cell_x should be nil for COLLIDER kind");
    lua_pop(L, 1);
    lua_getfield(L, -1, "cell_y");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "cell_y should be nil for COLLIDER kind");
    lua_pop(L, 2); // pop last value and userdata

    // MAP kind: rect should be nil
    ese_collision_hit_set_kind(hit, COLLISION_KIND_MAP);
    ese_collision_hit_lua_push(hit);
    lua_getfield(L, -1, "rect");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "rect should be nil for MAP kind");
    lua_pop(L, 2);

    // Release registry ref and destroy from C side to avoid double-free via multiple userdatas
    ese_collision_hit_unref(hit);
    ese_collision_hit_destroy(hit);
}

static void test_collision_hit_lua_readonly_properties(void) {
    ese_collision_hit_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    EseCollisionHit *hit = ese_collision_hit_create(g_engine);
    ese_collision_hit_ref(hit);
    ese_collision_hit_lua_push(hit);
    lua_setglobal(L, "H"); // expose as global for script

    const char *code = "H.kind = 2"; // any assignment should error
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, code), "Setting property should error (read-only)");

    // Clear global reference and clean up from C side
    lua_pushnil(L);
    lua_setglobal(L, "H");
    ese_collision_hit_unref(hit);
    ese_collision_hit_destroy(hit);
}

static void test_collision_hit_lua_tostring(void) {
    ese_collision_hit_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    EseCollisionHit *hit = ese_collision_hit_create(g_engine);
    ese_collision_hit_ref(hit);
    ese_collision_hit_lua_push(hit);
    lua_setglobal(L, "H");

    const char *code = "return tostring(H)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, code), "tostring should execute without error");
    const char *str = lua_tostring(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(str, "tostring result should not be NULL");
    TEST_ASSERT_TRUE_MESSAGE(strstr(str, "EseCollisionHit:") != NULL, "tostring should contain 'EseCollisionHit:'");
    lua_pop(L, 1);

    // Clear global reference and clean up from C side
    lua_pushnil(L);
    lua_setglobal(L, "H");
    ese_collision_hit_unref(hit);
    ese_collision_hit_destroy(hit);
}

static void test_collision_hit_lua_gc_with_ref(void) {
    ese_collision_hit_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    EseCollisionHit *hit = ese_collision_hit_create(g_engine);
    ese_collision_hit_ref(hit);

    int collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected == 0, "Garbage collection should not collect referenced hit");

    ese_collision_hit_unref(hit);
    ese_collision_hit_destroy(hit);
}


