#include "testing.h"
#include "utility/log.h"
#include "core/memory_manager.h"
#include "entity/components/entity_component_map.h"
#include "entity/components/entity_component.h"
#include "entity/entity.h"
#include "types/map.h"
#include "types/point.h"
#include "core/engine.h"
#include "scripting/lua_engine.h"

// Test data
static EseLuaEngine *test_engine = NULL;
static EseEntity *test_entity = NULL;

// Test setup and teardown
void setUp(void) {
    test_engine = create_test_engine();
    test_entity = entity_create(test_engine);
}

void tearDown(void) {
    entity_destroy(test_entity);
    test_entity = NULL;

    lua_engine_destroy(test_engine);
    test_engine = NULL;
}

// C API Tests
void test_entity_component_map_create(void) {
    EseEntityComponent *component = entity_component_ese_map_create(test_engine);

    TEST_ASSERT_NOT_NULL(component);
    TEST_ASSERT_EQUAL(ENTITY_COMPONENT_MAP, component->type);
    TEST_ASSERT_TRUE(component->active);
    TEST_ASSERT_NOT_NULL(component->id);
    TEST_ASSERT_EQUAL(test_engine, component->lua);
    TEST_ASSERT_NOT_EQUAL(LUA_NOREF, component->lua_ref);
    TEST_ASSERT_EQUAL(1, component->lua_ref_count);

    EseEntityComponentMap *map_comp = (EseEntityComponentMap *)component->data;
    TEST_ASSERT_NULL(map_comp->map);
    TEST_ASSERT_NOT_NULL(map_comp->position);
    TEST_ASSERT_EQUAL(128, map_comp->size);
    TEST_ASSERT_EQUAL(1000, map_comp->seed);
    TEST_ASSERT_NULL(map_comp->sprite_frames);

    entity_component_destroy(component);
}

void test_entity_component_map_create_null_engine(void) {
    ASSERT_DEATH((entity_component_ese_map_create(NULL)), "entity_component_ese_map_create called with NULL engine");
}

void test_entity_component_map_ref_unref(void) {
    EseEntityComponent *component = entity_component_ese_map_create(test_engine);

    // Initial ref from create
    TEST_ASSERT_EQUAL(1, component->lua_ref_count);

    // Ref twice
    component->vtable->ref(component);
    component->vtable->ref(component);
    TEST_ASSERT_EQUAL(3, component->lua_ref_count);

    // Unref twice
    component->vtable->unref(component);
    component->vtable->unref(component);
    TEST_ASSERT_EQUAL(1, component->lua_ref_count);

    entity_component_destroy(component);
}

// Lua API Tests
void test_entity_component_map_lua_init(void) {
    lua_State *L = test_engine->runtime;

    _entity_component_ese_map_init(test_engine);

    const char *test_code = "return type(EntityComponentMap) == 'table' and type(EntityComponentMap.new) == 'function'";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "EntityComponentMap table and new function should exist");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_map_lua_new_basic(void) {
    lua_State *L = test_engine->runtime;

    _entity_component_ese_map_init(test_engine);

    const char *test_code = "return EntityComponentMap.new()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Map component creation should execute without error");

    TEST_ASSERT_TRUE(lua_isuserdata(L, -1));
    EseEntityComponentMap *map_comp = _entity_component_ese_map_get(L, -1);
    TEST_ASSERT_NOT_NULL(map_comp);

    lua_pop(L, 1);
}

void test_entity_component_map_lua_properties(void) {
    lua_State *L = test_engine->runtime;

    _entity_component_ese_map_init(test_engine);
    ese_point_lua_init(test_engine);

    const char *test_code =
        "local c = EntityComponentMap.new()\n"
        "return c.active == true and type(c.id) == 'string' and c.map == nil and type(c.position) == 'userdata' and type(c.size) == 'number' and type(c.seed) == 'number'";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Property access should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_map_lua_property_setters(void) {
    lua_State *L = test_engine->runtime;

    _entity_component_ese_map_init(test_engine);
    ese_point_lua_init(test_engine);
    ese_map_lua_init(test_engine);

    const char *test_code =
        "local c = EntityComponentMap.new()\n"
        "c.active = false\n"
        "c.size = 64\n"
        "c.seed = 42\n"
        "local p = Point.new(3, 4)\n"
        "c.position = p\n"
        "local m = Map.new(2, 2)\n"
        "c.map = m\n"
        "m = nil; p = nil; c = nil; collectgarbage()\n"
        "return true";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Property setters should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

// Test runner
int main(void) {
    log_init();
    UNITY_BEGIN();

    // C API Tests
    RUN_TEST(test_entity_component_map_create);
    RUN_TEST(test_entity_component_map_create_null_engine);
    RUN_TEST(test_entity_component_map_ref_unref);

    // Lua API Tests
    RUN_TEST(test_entity_component_map_lua_init);
    RUN_TEST(test_entity_component_map_lua_new_basic);
    RUN_TEST(test_entity_component_map_lua_properties);
    RUN_TEST(test_entity_component_map_lua_property_setters);

    memory_manager.destroy();

    return UNITY_END();
}


