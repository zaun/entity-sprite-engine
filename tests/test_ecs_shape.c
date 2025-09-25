#include "testing.h"
#include "utility/log.h"
#include "core/memory_manager.h"
#include "entity/components/entity_component_shape.h"
#include "entity/components/entity_component.h"
#include "entity/entity_private.h"
#include "types/poly_line.h"
#include "types/point.h"
#include "entity/entity.h"
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
    // Force Lua to collect Lua-owned userdata (e.g., shapes created via Lua.new)
    if (test_engine && test_engine->runtime) {
        lua_gc(test_engine->runtime, LUA_GCCOLLECT, 0);
    }
    entity_destroy(test_entity);
    test_entity = NULL;
    lua_engine_destroy(test_engine);
    test_engine = NULL;
}

// =========================
// C API Tests
// =========================

void test_entity_component_shape_create(void) {
    EseEntityComponent *component = entity_component_shape_create(test_engine);

    TEST_ASSERT_NOT_NULL(component);
    TEST_ASSERT_EQUAL(ENTITY_COMPONENT_SHAPE, component->type);
    TEST_ASSERT_TRUE(component->active);
    TEST_ASSERT_NOT_NULL(component->id);
    TEST_ASSERT_EQUAL(test_engine, component->lua);
    TEST_ASSERT_NOT_EQUAL(LUA_NOREF, component->lua_ref);
    TEST_ASSERT_EQUAL(1, component->lua_ref_count);

    EseEntityComponentShape *shape = (EseEntityComponentShape *)component->data;
    TEST_ASSERT_NOT_NULL(shape->polyline);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, shape->rotation);

    entity_component_destroy(component);
}

void test_entity_component_shape_create_null_engine(void) {
    ASSERT_DEATH((entity_component_shape_create(NULL)), "entity_component_shape_create called with NULL engine");
}

void test_entity_component_shape_copy(void) {
    EseEntityComponent *component = entity_component_shape_create(test_engine);
    EseEntityComponentShape *shape = (EseEntityComponentShape *)component->data;

    // Add a few points to the polyline so we can verify a deep copy
    EsePoint *p1 = ese_point_create(test_engine);
    EsePoint *p2 = ese_point_create(test_engine);
    ese_point_set_x(p1, 10.0f); ese_point_set_y(p1, 20.0f);
    ese_point_set_x(p2, 30.0f); ese_point_set_y(p2, 40.0f);
    ese_poly_line_add_point(shape->polyline, p1);
    ese_poly_line_add_point(shape->polyline, p2);
    // PolyLine stores coordinates, not point objects; free temporaries to avoid leaks
    ese_point_destroy(p1);
    ese_point_destroy(p2);

    EseEntityComponent *copy = _entity_component_shape_copy(shape);
    EseEntityComponentShape *shape_copy = (EseEntityComponentShape *)copy->data;

    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL(ENTITY_COMPONENT_SHAPE, copy->type);
    TEST_ASSERT_TRUE(copy->active);
    TEST_ASSERT_NOT_NULL(copy->id);
    TEST_ASSERT_EQUAL(test_engine, copy->lua);
    TEST_ASSERT_EQUAL(LUA_NOREF, copy->lua_ref); // Copy starts unregistered
    TEST_ASSERT_EQUAL(0, copy->lua_ref_count);

    // Polyline should be distinct with equal point count
    TEST_ASSERT_NOT_EQUAL(shape->polyline, shape_copy->polyline);
    TEST_ASSERT_EQUAL(ese_poly_line_get_point_count(shape->polyline), ese_poly_line_get_point_count(shape_copy->polyline));

    entity_component_destroy(component);
    entity_component_destroy(copy);
}

void test_entity_component_shape_destroy(void) {
    EseEntityComponent *component = entity_component_shape_create(test_engine);
    // Should not crash
    _entity_component_shape_destroy((EseEntityComponentShape *)component->data);
}

void test_entity_component_shape_ref_unref(void) {
    EseEntityComponent *component = entity_component_shape_create(test_engine);
    // initial
    TEST_ASSERT_EQUAL(1, component->lua_ref_count);

    // vtable ref/unref
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

void test_entity_component_shape_lua_init(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_shape_init(test_engine);

    const char *test_code = "return type(EntityComponentShape) == 'table' and type(EntityComponentShape.new) == 'function'";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "EntityComponentShape table and new function should exist");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_shape_lua_new(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_shape_init(test_engine);

    const char *test_code = "return EntityComponentShape.new()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Shape creation should execute without error");
    TEST_ASSERT_TRUE(lua_isuserdata(L, -1));
    EseEntityComponentShape *shape = _entity_component_shape_get(L, -1);
    TEST_ASSERT_NOT_NULL(shape);
    TEST_ASSERT_NOT_NULL(shape->polyline);
    lua_pop(L, 1);
}

void test_entity_component_shape_lua_properties(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_shape_init(test_engine);
    ese_poly_line_lua_init(test_engine);

    const char *test_code =
        "local s = EntityComponentShape.new()\n"
        "return s.active == true and type(s.id) == 'string' and s.rotation == 0 and type(s.polyline) == 'userdata'";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Property access should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_shape_lua_property_setters(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_shape_init(test_engine);
    ese_poly_line_lua_init(test_engine);

    const char *test_code =
        "local s = EntityComponentShape.new()\n"
        "s.active = false\n"
        "s.rotation = -45\n" // should wrap to 315
        "return s.active == false and s.rotation == 315";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Property setters should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_shape_lua_polyline_set(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_shape_init(test_engine);
    ese_poly_line_lua_init(test_engine);

    const char *test_code =
        "local s = EntityComponentShape.new()\n"
        "local pl = PolyLine.new()\n"
        "s.polyline = pl\n"
        "return type(s.polyline) == 'userdata'";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Polyline setter should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_shape_lua_tostring(void) {
    lua_State *L = test_engine->runtime;
    _entity_component_shape_init(test_engine);

    const char *test_code =
        "local s = EntityComponentShape.new()\n"
        "local str = tostring(s)\n"
        "return str:find('EntityComponentShape') ~= nil and str:find('active=true') ~= nil";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Tostring should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_shape_lua_gc(void) {
    // Create directly via C factory to test ref counting and unref behavior
    EseEntityComponent *component = entity_component_shape_create(test_engine);
    TEST_ASSERT_NOT_NULL(component);
    TEST_ASSERT_EQUAL(1, component->lua_ref_count);

    component->vtable->unref(component);
    TEST_ASSERT_EQUAL(0, component->lua_ref_count);
    component->vtable->ref(component);
    TEST_ASSERT_EQUAL(1, component->lua_ref_count);

    component->vtable->unref(component);
    entity_component_destroy(component);
}

// =========================
// Test runner
// =========================

int main(void) {
    log_init();
    UNITY_BEGIN();

    // C API Tests
    RUN_TEST(test_entity_component_shape_create);
    RUN_TEST(test_entity_component_shape_create_null_engine);
    RUN_TEST(test_entity_component_shape_copy);
    RUN_TEST(test_entity_component_shape_destroy);
    RUN_TEST(test_entity_component_shape_ref_unref);

    // Lua API Tests
    RUN_TEST(test_entity_component_shape_lua_init);
    RUN_TEST(test_entity_component_shape_lua_new);
    RUN_TEST(test_entity_component_shape_lua_properties);
    RUN_TEST(test_entity_component_shape_lua_property_setters);
    RUN_TEST(test_entity_component_shape_lua_polyline_set);
    RUN_TEST(test_entity_component_shape_lua_tostring);
    RUN_TEST(test_entity_component_shape_lua_gc);

    memory_manager.destroy();
    return UNITY_END();
}


