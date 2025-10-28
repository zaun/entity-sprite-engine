#include <string.h>
#include "testing.h"
#include "utility/log.h"
#include "core/memory_manager.h"
#include "entity/components/entity_component_text.h"
#include "entity/components/entity_component.h"
#include "entity/entity.h"
#include "types/point.h"
#include "scripting/lua_engine.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "utility/double_linked_list.h"

// Test data
static EseLuaEngine *test_engine = NULL;
static EseEntity *test_entity = NULL;
static EseEngine mock_engine;

// Test setup and teardown
void setUp(void) {
    test_engine = create_test_engine();
    test_entity = entity_create(test_engine);

    // Create a minimal mock engine and expose it to Lua (mirrors other tests)
    mock_engine.lua_engine = test_engine;
    mock_engine.entities = dlist_create(NULL);
    lua_engine_add_registry_key(test_engine->runtime, ENGINE_KEY, &mock_engine);
}

void tearDown(void) {
    // Force a Lua GC pass before tearing down to free Lua-owned userdata
    if (test_engine && test_engine->runtime) {
        lua_gc(test_engine->runtime, LUA_GCCOLLECT, 0);
    }

    entity_destroy(test_entity);
    test_entity = NULL;

    lua_engine_destroy(test_engine);
    test_engine = NULL;

    // Clean up mock engine list
    dlist_free(mock_engine.entities);
    mock_engine.entities = NULL;
}

// =====================
// C API Tests
// =====================

void test_entity_component_text_create_null_engine(void) {
    TEST_ASSERT_DEATH((entity_component_text_create(NULL, "hello")), "entity_component_text_create called with NULL engine");
}

void test_entity_component_text_create_basic(void) {
    const char *msg = "Hello";
    EseEntityComponent *component = entity_component_text_create(test_engine, msg);

    TEST_ASSERT_NOT_NULL(component);
    TEST_ASSERT_EQUAL(ENTITY_COMPONENT_TEXT, component->type);
    TEST_ASSERT_TRUE(component->active);
    TEST_ASSERT_NOT_NULL(component->id);
    TEST_ASSERT_EQUAL(test_engine, component->lua);
    TEST_ASSERT_NOT_EQUAL(LUA_NOREF, component->lua_ref);
    TEST_ASSERT_NOT_NULL(component->data);

    EseEntityComponentText *text_comp = (EseEntityComponentText *)component->data;
    TEST_ASSERT_NOT_NULL(text_comp->text);
    TEST_ASSERT_EQUAL_STRING(msg, text_comp->text);
    TEST_ASSERT_EQUAL(TEXT_JUSTIFY_LEFT, text_comp->justify);
    TEST_ASSERT_EQUAL(TEXT_ALIGN_TOP, text_comp->align);
    TEST_ASSERT_NOT_NULL(text_comp->offset);

    entity_component_destroy(component);
}

void test_entity_component_text_copy_null_src(void) {
    TEST_ASSERT_DEATH((_entity_component_text_copy(NULL)), "_entity_component_text_copy called with NULL src");
}

void test_entity_component_text_copy_basic(void) {
    EseEntityComponent *component = entity_component_text_create(test_engine, "CopyThis");
    EseEntityComponentText *text_comp = (EseEntityComponentText *)component->data;
    text_comp->justify = TEXT_JUSTIFY_CENTER;
    text_comp->align = TEXT_ALIGN_BOTTOM;
    ese_point_set_x(text_comp->offset, 3.0f);
    ese_point_set_y(text_comp->offset, 7.0f);

    EseEntityComponent *copy = _entity_component_text_copy(text_comp);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL(ENTITY_COMPONENT_TEXT, copy->type);
    TEST_ASSERT_TRUE(copy->active);
    TEST_ASSERT_NOT_NULL(copy->id);
    TEST_ASSERT_EQUAL(test_engine, copy->lua);
    TEST_ASSERT_EQUAL(LUA_NOREF, copy->lua_ref); // copies start unregistered

    EseEntityComponentText *copy_text = (EseEntityComponentText *)copy->data;
    TEST_ASSERT_NOT_NULL(copy_text->text);
    TEST_ASSERT_EQUAL_STRING("CopyThis", copy_text->text);
    TEST_ASSERT_EQUAL(TEXT_JUSTIFY_CENTER, copy_text->justify);
    TEST_ASSERT_EQUAL(TEXT_ALIGN_BOTTOM, copy_text->align);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, ese_point_get_x(copy_text->offset));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.0f, ese_point_get_y(copy_text->offset));

    entity_component_destroy(component);
    entity_component_destroy(copy);
}

void test_entity_component_text_update_and_destroy(void) {
    EseEntityComponent *component = entity_component_text_create(test_engine, "Update");
    // Should not crash
    entity_component_update(component, test_entity, 0.016f);

    // Destroy should not crash
    entity_component_destroy(component);
}

void test_entity_component_text_update_null_args(void) {
    TEST_ASSERT_DEATH((_entity_component_text_update(NULL, test_entity, 0.0f)), "_entity_component_text_update called with NULL component");
    TEST_ASSERT_DEATH((_entity_component_text_update((EseEntityComponentText *)0x1, NULL, 0.0f)), "_entity_component_text_update called with NULL entity");
}

void test_entity_component_text_destroy_null(void) {
    TEST_ASSERT_DEATH((_entity_component_text_destroy(NULL)), "_entity_component_text_destroy called with NULL src");
}

// =====================
// Lua API Tests
// =====================

void test_entity_component_text_lua_init(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_text_init(test_engine);
    const char *test_code = "return type(EntityComponentText) == 'table' and type(EntityComponentText.new) == 'function'";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "EntityComponentText table and new function should exist");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_text_lua_new_basic(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_text_init(test_engine);
    const char *test_code =
        "local c = EntityComponentText.new()\n"
        "c = nil; collectgarbage()\n"
        "return true";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Text component creation should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_text_lua_new_with_text(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_text_init(test_engine);
    const char *test_code =
        "local c = EntityComponentText.new('Hi')\n"
        "local ok = (c ~= nil and type(c) == 'userdata' and c.text == 'Hi')\n"
        "c = nil; collectgarbage()\n"
        "return ok";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Text component creation with text should work");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_text_lua_properties(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_text_init(test_engine);
    const char *test_code =
        "local c = EntityComponentText.new('abc')\n"
        "local ok = (type(c) == 'userdata' and c.text == 'abc' and type(c.justify) == 'number' and type(c.align) == 'number' and type(c.offset) == 'userdata')\n"
        "c = nil; collectgarbage()\n"
        "return ok";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Property access should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_text_lua_setters(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_text_init(test_engine);
    const char *test_code =
        "local c = EntityComponentText.new('abc')\n"
        "c.text = 'xyz'\n"
        "c.justify = 1\n"
        "c.align = 2\n"
        "local ok = (c.text == 'xyz' and c.justify == 1 and c.align == 2)\n"
        "c = nil; collectgarbage()\n"
        "return ok";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Property setters should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_text_lua_constants(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_text_init(test_engine);
    const char *test_code =
        "return type(EntityComponentText.JUSTIFY) == 'table' and\n"
        "type(EntityComponentText.ALIGN) == 'table' and\n"
        "EntityComponentText.JUSTIFY.LEFT == 0 and\n"
        "EntityComponentText.JUSTIFY.CENTER == 1 and\n"
        "EntityComponentText.JUSTIFY.RIGHT == 2 and\n"
        "EntityComponentText.ALIGN.TOP == 0 and\n"
        "EntityComponentText.ALIGN.CENTER == 1 and\n"
        "EntityComponentText.ALIGN.BOTTOM == 2";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Constants tables should exist with expected values");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_text_lua_offset_setter(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_text_init(test_engine);
    ese_point_lua_init(test_engine);
    const char *test_code =
        "local c = EntityComponentText.new('p')\n"
        "local p = c.offset\n"
        "p.x = 12\n"
        "p.y = 34\n"
        "c.offset = p\n"
        "local p2 = c.offset\n"
        "local ok = (p2.x == 12 and p2.y == 34)\n"
        "c = nil; p = nil; p2 = nil; collectgarbage()\n"
        "return ok";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Offset setter should accept point proxy and copy values");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_text_lua_tostring(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_text_init(test_engine);
    const char *test_code =
        "local c = EntityComponentText.new('str')\n"
        "local s = tostring(c)\n"
        "local ok = (type(s) == 'string' and s:find('EntityComponentText') ~= nil)\n"
        "c = nil; s = nil; collectgarbage()\n"
        "return ok";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "tostring should work");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_text_lua_gc(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_text_init(test_engine);
    const char *test_code =
        "local c = EntityComponentText.new('gc')\n"
        "c = nil\n"
        "collectgarbage()\n"
        "return true";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Garbage collection should work");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

int main(void) {
    log_init();
    UNITY_BEGIN();

    // C API Tests
    RUN_TEST(test_entity_component_text_create_null_engine);
    RUN_TEST(test_entity_component_text_create_basic);
    RUN_TEST(test_entity_component_text_copy_null_src);
    RUN_TEST(test_entity_component_text_copy_basic);
    RUN_TEST(test_entity_component_text_update_and_destroy);
    RUN_TEST(test_entity_component_text_update_null_args);
    RUN_TEST(test_entity_component_text_destroy_null);

    // Lua API Tests
    RUN_TEST(test_entity_component_text_lua_init);
    RUN_TEST(test_entity_component_text_lua_new_basic);
    RUN_TEST(test_entity_component_text_lua_new_with_text);
    RUN_TEST(test_entity_component_text_lua_properties);
    RUN_TEST(test_entity_component_text_lua_setters);
    RUN_TEST(test_entity_component_text_lua_constants);
    RUN_TEST(test_entity_component_text_lua_offset_setter);
    RUN_TEST(test_entity_component_text_lua_tostring);
    RUN_TEST(test_entity_component_text_lua_gc);

    memory_manager.destroy(true);

    return UNITY_END();
}


