#include "testing.h"
#include <string.h>
#include "utility/log.h"
#include "core/memory_manager.h"
#include "entity/components/entity_component_sprite.h"
#include "entity/components/entity_component.h"
#include "entity/entity.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "scripting/lua_engine.h"

// Test data
static EseLuaEngine *test_engine = NULL;
static EseEntity *test_entity = NULL;
static EseEngine mock_engine;

// Test setup and teardown
void setUp(void) {
    test_engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(test_engine);

    // Minimal engine registry for sprite lookup
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

void test_entity_component_sprite_create_null_engine(void) {
    TEST_ASSERT_DEATH((entity_component_sprite_create(NULL, NULL)), "entity_component_sprite_create called with NULL engine");
}

void test_entity_component_sprite_create_basic(void) {
    EseEntityComponent *component = entity_component_sprite_create(test_engine, NULL);
    TEST_ASSERT_NOT_NULL(component);
    TEST_ASSERT_EQUAL(ENTITY_COMPONENT_SPRITE, component->type);
    TEST_ASSERT_TRUE(component->active);
    TEST_ASSERT_NOT_NULL(component->id);
    TEST_ASSERT_EQUAL(test_engine, component->lua);
    TEST_ASSERT_NOT_EQUAL(LUA_NOREF, component->lua_ref);
    TEST_ASSERT_EQUAL(1, component->lua_ref_count);

    EseEntityComponentSprite *sprite = (EseEntityComponentSprite *)component->data;
    TEST_ASSERT_EQUAL_UINT32(0, (unsigned int)sprite->current_frame);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, sprite->sprite_ellapse_time);

    entity_component_destroy(component);
}

void test_entity_component_sprite_copy(void) {
    EseEntityComponent *component = entity_component_sprite_create(test_engine, NULL);
    EseEntityComponentSprite *sprite = (EseEntityComponentSprite *)component->data;
    (void)sprite;

    EseEntityComponent *copy = _entity_component_sprite_copy((EseEntityComponentSprite*)component->data);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL(ENTITY_COMPONENT_SPRITE, copy->type);
    TEST_ASSERT_TRUE(copy->active);
    TEST_ASSERT_NOT_NULL(copy->id);
    TEST_ASSERT_EQUAL(test_engine, copy->lua);
    TEST_ASSERT_EQUAL(LUA_NOREF, copy->lua_ref);
    TEST_ASSERT_EQUAL(0, copy->lua_ref_count);

    entity_component_destroy(component);
    entity_component_destroy(copy);
}

void test_entity_component_sprite_ref_unref(void) {
    EseEntityComponent *component = entity_component_sprite_create(test_engine, NULL);
    TEST_ASSERT_EQUAL(1, component->lua_ref_count);

    component->vtable->ref(component);
    component->vtable->ref(component);
    TEST_ASSERT_EQUAL(3, component->lua_ref_count);

    component->vtable->unref(component);
    component->vtable->unref(component);
    TEST_ASSERT_EQUAL(1, component->lua_ref_count);

    entity_component_destroy(component);
}

// =========================
// Lua API Tests
// =========================

void test_entity_component_sprite_lua_init(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_sprite_init(test_engine);

    const char *test_code = "return type(EntityComponentSprite) == 'table' and type(EntityComponentSprite.new) == 'function'";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "EntityComponentSprite table and new function should exist");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_sprite_lua_new_basic(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_sprite_init(test_engine);

    const char *test_code = "return EntityComponentSprite.new()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Sprite component creation should execute without error");
    TEST_ASSERT_TRUE(lua_isuserdata(L, -1));
    EseEntityComponentSprite *sprite = _entity_component_sprite_get(L, -1);
    TEST_ASSERT_NOT_NULL(sprite);
    lua_pop(L, 1);
}

void test_entity_component_sprite_lua_properties(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_sprite_init(test_engine);

    const char *test_code =
        "local c = EntityComponentSprite.new()\n"
        "return c.active == true and type(c.id) == 'string' and (c.sprite == nil or type(c.sprite) == 'string')";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Property access should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_sprite_lua_setters(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_sprite_init(test_engine);

    const char *test_code =
        "local c = EntityComponentSprite.new()\n"
        "c.active = false\n"
        "c.sprite = nil\n"
        "return c.active == false";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Property setters should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_sprite_lua_tostring(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_sprite_init(test_engine);

    const char *test_code =
        "local c = EntityComponentSprite.new()\n"
        "local str = tostring(c)\n"
        "return type(str) == 'string' and str:find('EntityComponentSprite') ~= nil";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "tostring should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_sprite_lua_gc(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_sprite_init(test_engine);

    const char *test_code =
        "local c = EntityComponentSprite.new()\n"
        "c = nil\n"
        "collectgarbage()\n"
        "return true";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Garbage collection should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

// =========================
// Test runner
// =========================

int main(void) {
    log_init();
    UNITY_BEGIN();

    // C API Tests
    RUN_TEST(test_entity_component_sprite_create_null_engine);
    RUN_TEST(test_entity_component_sprite_create_basic);
    RUN_TEST(test_entity_component_sprite_copy);
    RUN_TEST(test_entity_component_sprite_ref_unref);

    // Lua API Tests
    RUN_TEST(test_entity_component_sprite_lua_init);
    RUN_TEST(test_entity_component_sprite_lua_new_basic);
    RUN_TEST(test_entity_component_sprite_lua_properties);
    RUN_TEST(test_entity_component_sprite_lua_setters);
    RUN_TEST(test_entity_component_sprite_lua_tostring);
    RUN_TEST(test_entity_component_sprite_lua_gc);

    memory_manager.destroy(true);
    return UNITY_END();
}


