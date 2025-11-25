#include "testing.h"
#include <string.h>
#include "entity/bindings/listener.h"
#include "entity/components/listener.h"
#include "entity/components/entity_component.h"
#include "entity/entity.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"

// Test data
static EseLuaEngine *test_engine = NULL;
static EseEntity *test_entity = NULL;
static EseEngine mock_engine;

// Test setup and teardown
void setUp(void) {
    test_engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(test_engine);

    memset(&mock_engine, 0, sizeof(mock_engine));
    mock_engine.lua_engine = test_engine;
    lua_engine_add_registry_key(test_engine->runtime, ENGINE_KEY, &mock_engine);

    test_entity = entity_create(test_engine);
    TEST_ASSERT_NOT_NULL(test_entity);
}

void tearDown(void) {
    entity_destroy(test_entity);
    test_entity = NULL;

    lua_engine_destroy(test_engine);
    test_engine = NULL;
}

// =========================
// C API Tests
// =========================

void test_entity_component_listener_create_null_engine(void) {
    TEST_ASSERT_DEATH((entity_component_listener_create(NULL)),
                      "entity_component_listener_create called with NULL engine");
}

void test_entity_component_listener_create_basic(void) {
    EseEntityComponent *component = entity_component_listener_create(test_engine);
    TEST_ASSERT_NOT_NULL(component);
    TEST_ASSERT_EQUAL(ENTITY_COMPONENT_LISTENER, component->type);
    TEST_ASSERT_TRUE(component->active);
    TEST_ASSERT_NOT_NULL(component->id);
    TEST_ASSERT_EQUAL(test_engine, component->lua);
    TEST_ASSERT_NOT_EQUAL(LUA_NOREF, component->lua_ref);
    TEST_ASSERT_EQUAL(1, component->lua_ref_count);

    EseEntityComponentListener *listener = (EseEntityComponentListener *)component->data;
    TEST_ASSERT_NOT_NULL(listener);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, listener->volume);
    TEST_ASSERT_FALSE(listener->spatial);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1000.0f, listener->max_distance);

    entity_component_destroy(component);
}

// =========================
// Lua API Tests
// =========================

void test_entity_component_listener_lua_init(void) {
    lua_State *L = test_engine->runtime;
    entity_component_listener_init(test_engine);

    const char *test_code =
        "return type(EntityComponentListener) == 'table' and type(EntityComponentListener.new) == 'function'";
    TEST_ASSERT_LUA(L, test_code, "EntityComponentListener table and new function should exist");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_listener_lua_new_defaults(void) {
    lua_State *L = test_engine->runtime;
    entity_component_listener_init(test_engine);

    const char *test_code =
        "local c = EntityComponentListener.new()\n"
        "return c.volume == 0 and c.spatial == false and c.max_distance == 1000";
    TEST_ASSERT_LUA(L, test_code, "Listener defaults should be correct");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_listener_lua_setters(void) {
    lua_State *L = test_engine->runtime;
    entity_component_listener_init(test_engine);

    const char *test_code =
        "local c = EntityComponentListener.new()\n"
        "c.active = false\n"
        "c.volume = 50\n"
        "c.spatial = true\n"
        "c.max_distance = 500\n"
        "return c.active == false and c.volume == 50 and c.spatial == true and c.max_distance == 500";
    TEST_ASSERT_LUA(L, test_code, "Listener setters should work");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_listener_lua_volume_clamp(void) {
    lua_State *L = test_engine->runtime;
    entity_component_listener_init(test_engine);

    const char *test_code =
        "local c = EntityComponentListener.new()\n"
        "c.volume = -10\n"
        "local v1 = c.volume\n"
        "c.volume = 200\n"
        "local v2 = c.volume\n"
        "return v1 == 0 and v2 == 100";
    TEST_ASSERT_LUA(L, test_code, "Listener volume should be clamped to [0, 100]");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

int main(void) {
    log_init();
    UNITY_BEGIN();

    // C API tests
    RUN_TEST(test_entity_component_listener_create_null_engine);
    RUN_TEST(test_entity_component_listener_create_basic);

    // Lua API tests
    RUN_TEST(test_entity_component_listener_lua_init);
    RUN_TEST(test_entity_component_listener_lua_new_defaults);
    RUN_TEST(test_entity_component_listener_lua_setters);
    RUN_TEST(test_entity_component_listener_lua_volume_clamp);

    memory_manager.destroy(true);
    return UNITY_END();
}
