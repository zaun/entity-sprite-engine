#include "testing.h"
#include "utility/log.h"
#include "core/memory_manager.h"
#include "entity/components/entity_component_collider.h"
#include "entity/components/entity_component.h"
#include "entity/entity_private.h"
#include "types/rect.h"
#include "types/point.h"
#include "entity/entity.h"
#include "core/engine.h"
#include "scripting/lua_engine.h"

#define COLLIDER_RECT_CAPACITY 5

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
void test_entity_component_collider_create(void) {
    EseEntityComponent *component = entity_component_collider_create(test_engine);
    
    TEST_ASSERT_NOT_NULL(component);
    TEST_ASSERT_EQUAL(ENTITY_COMPONENT_COLLIDER, component->type);
    TEST_ASSERT_TRUE(component->active);
    TEST_ASSERT_NOT_NULL(component->id);
    TEST_ASSERT_EQUAL(test_engine, component->lua);
    TEST_ASSERT_NOT_EQUAL(LUA_NOREF, component->lua_ref);
    TEST_ASSERT_EQUAL(1, component->lua_ref_count);
    
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    TEST_ASSERT_NOT_NULL(collider->rects);
    TEST_ASSERT_EQUAL(COLLIDER_RECT_CAPACITY, collider->rects_capacity);
    TEST_ASSERT_EQUAL(0, collider->rects_count);
    TEST_ASSERT_FALSE(collider->draw_debug);
    
    entity_component_destroy(component);
}

void test_entity_component_collider_create_null_engine(void) {
    TEST_ASSERT_DEATH((entity_component_collider_create(NULL)), "entity_component_collider_create called with NULL engine");
}

void test_entity_component_collider_draw_debug_getter_setter(void) {
    EseEntityComponent *component = entity_component_collider_create(test_engine);
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    
    // Test getter
    TEST_ASSERT_FALSE(entity_component_collider_get_draw_debug(collider));
    
    // Test setter
    entity_component_collider_set_draw_debug(collider, true);
    TEST_ASSERT_TRUE(entity_component_collider_get_draw_debug(collider));
    
    entity_component_collider_set_draw_debug(collider, false);
    TEST_ASSERT_FALSE(entity_component_collider_get_draw_debug(collider));
    
    entity_component_destroy(component);
}

void test_entity_component_collider_draw_debug_null_collider(void) {
    TEST_ASSERT_DEATH((entity_component_collider_get_draw_debug(NULL)), "entity_component_collider_get_draw_debug called with NULL collider");
    TEST_ASSERT_DEATH((entity_component_collider_set_draw_debug(NULL, true)), "entity_component_collider_set_draw_debug called with NULL collider");
}

void test_entity_component_collider_rects_add(void) {
    EseEntityComponent *component = entity_component_collider_create(test_engine);
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    
    EseRect *rect = ese_rect_create(test_engine);
    ese_rect_set_x(rect, 10.0f);
    ese_rect_set_y(rect, 20.0f);
    ese_rect_set_width(rect, 30.0f);
    ese_rect_set_height(rect, 40.0f);
    
    // Add rect to collider
    entity_component_collider_rects_add(collider, rect);
    
    TEST_ASSERT_EQUAL(1, collider->rects_count);
    TEST_ASSERT_EQUAL(rect, collider->rects[0]);
    
    // Test capacity expansion
    for (int i = 1; i < COLLIDER_RECT_CAPACITY + 2; i++) {
        EseRect *new_rect = ese_rect_create(test_engine);
        entity_component_collider_rects_add(collider, new_rect);
    }
    
    TEST_ASSERT_EQUAL(COLLIDER_RECT_CAPACITY + 2, collider->rects_count);
    TEST_ASSERT_TRUE(collider->rects_capacity > COLLIDER_RECT_CAPACITY);
    
    entity_component_destroy(component);
}

void test_entity_component_collider_rects_add_null_params(void) {
    EseEntityComponent *component = entity_component_collider_create(test_engine);
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    EseRect *rect = ese_rect_create(test_engine);
    
    TEST_ASSERT_DEATH((entity_component_collider_rects_add(NULL, rect)), "entity_component_collider_rects_add called with NULL collider");
    TEST_ASSERT_DEATH((entity_component_collider_rects_add(collider, NULL)), "entity_component_collider_rects_add called with NULL rect");
    
    entity_component_destroy(component);
    ese_rect_destroy(rect);
}

void test_entity_component_collider_ref_unref(void) {
    EseEntityComponent *component = entity_component_collider_create(test_engine);
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    
    // Test initial ref count
    TEST_ASSERT_EQUAL(1, component->lua_ref_count);
    
    // Test multiple refs
    entity_component_collider_ref(collider);
    entity_component_collider_ref(collider);
    TEST_ASSERT_EQUAL(3, component->lua_ref_count);
    
    // Test unref
    entity_component_collider_unref(collider);
    entity_component_collider_unref(collider);
    TEST_ASSERT_EQUAL(1, component->lua_ref_count);
    
    // Test unref with NULL
    entity_component_collider_unref(NULL); // Should not crash
    
    entity_component_destroy(component);
}

void test_entity_component_collider_ref_null_collider(void) {
    TEST_ASSERT_DEATH((entity_component_collider_ref(NULL)), "entity_component_collider_ref called with NULL component");
}

void test_entity_component_collider_copy(void) {
    EseEntityComponent *component = entity_component_collider_create(test_engine);
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    
    // Add some rects
    EseRect *rect1 = ese_rect_create(test_engine);
    ese_rect_set_x(rect1, 10.0f);
    ese_rect_set_y(rect1, 20.0f);
    ese_rect_set_width(rect1, 30.0f);
    ese_rect_set_height(rect1, 40.0f);
    
    EseRect *rect2 = ese_rect_create(test_engine);
    ese_rect_set_x(rect2, 50.0f);
    ese_rect_set_y(rect2, 60.0f);
    ese_rect_set_width(rect2, 70.0f);
    ese_rect_set_height(rect2, 80.0f);
    
    entity_component_collider_rects_add(collider, rect1);
    entity_component_collider_rects_add(collider, rect2);
    entity_component_collider_set_draw_debug(collider, true);
    
    // Copy the component
    EseEntityComponent *copy = _entity_component_collider_copy(collider);
    EseEntityComponentCollider *copy_collider = (EseEntityComponentCollider *)copy->data;
    
    // Test basic properties
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL(ENTITY_COMPONENT_COLLIDER, copy->type);
    TEST_ASSERT_TRUE(copy->active);
    TEST_ASSERT_NOT_NULL(copy->id);
    TEST_ASSERT_EQUAL(test_engine, copy->lua);
    TEST_ASSERT_EQUAL(LUA_NOREF, copy->lua_ref); // Copy starts unregistered
    TEST_ASSERT_EQUAL(0, copy->lua_ref_count);
    
    // Test collider properties
    TEST_ASSERT_EQUAL(2, copy_collider->rects_count);
    TEST_ASSERT_EQUAL(collider->rects_capacity, copy_collider->rects_capacity);
    TEST_ASSERT_TRUE(copy_collider->draw_debug);
    
    // Test rects are copied (not shared)
    TEST_ASSERT_NOT_NULL(copy_collider->rects[0]);
    TEST_ASSERT_NOT_NULL(copy_collider->rects[1]);
    TEST_ASSERT_NOT_EQUAL(collider->rects[0], copy_collider->rects[0]);
    TEST_ASSERT_NOT_EQUAL(collider->rects[1], copy_collider->rects[1]);
    
    // Test rect properties are copied
    TEST_ASSERT_FLOAT_WITHIN(0.001f, ese_rect_get_x(collider->rects[0]), ese_rect_get_x(copy_collider->rects[0]));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, ese_rect_get_y(collider->rects[0]), ese_rect_get_y(copy_collider->rects[0]));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, ese_rect_get_width(collider->rects[0]), ese_rect_get_width(copy_collider->rects[0]));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, ese_rect_get_height(collider->rects[0]), ese_rect_get_height(copy_collider->rects[0]));
    
    entity_component_destroy(component);
    entity_component_destroy(copy);
}

void test_entity_component_collider_copy_null_src(void) {
    TEST_ASSERT_DEATH((_entity_component_collider_copy(NULL)), "_entity_component_collider_copy called with NULL src");
}

void test_entity_component_collider_destroy(void) {
    EseEntityComponent *component = entity_component_collider_create(test_engine);
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    
    // Add some rects
    EseRect *rect1 = ese_rect_create(test_engine);
    EseRect *rect2 = ese_rect_create(test_engine);
    entity_component_collider_rects_add(collider, rect1);
    entity_component_collider_rects_add(collider, rect2);
    
    // Destroy should not crash
    _entity_component_collider_destroy(collider);
}

void test_entity_component_collider_destroy_null_collider(void) {
    TEST_ASSERT_DEATH((_entity_component_collider_destroy(NULL)), "_entity_component_collider_destroy called with NULL src");
}

void test_entity_component_collider_update_bounds(void) {
    EseEntityComponent *component = entity_component_collider_create(test_engine);
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    
    // Attach to entity
    entity_component_add(test_entity, component);
    ese_point_set_x(test_entity->position, 100.0f);
    ese_point_set_y(test_entity->position, 200.0f);
    
    // Add rects
    EseRect *rect1 = ese_rect_create(test_engine);
    ese_rect_set_x(rect1, 10.0f);
    ese_rect_set_y(rect1, 20.0f);
    ese_rect_set_width(rect1, 30.0f);
    ese_rect_set_height(rect1, 40.0f);
    
    EseRect *rect2 = ese_rect_create(test_engine);
    ese_rect_set_x(rect2, 50.0f);
    ese_rect_set_y(rect2, 60.0f);
    ese_rect_set_width(rect2, 70.0f);
    ese_rect_set_height(rect2, 80.0f);
    
    entity_component_collider_rects_add(collider, rect1);
    entity_component_collider_rects_add(collider, rect2);
    
    // Update bounds
    entity_component_collider_update_bounds(collider);
    
    // Test entity bounds (relative to entity)
    TEST_ASSERT_NOT_NULL(test_entity->collision_bounds);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ese_rect_get_x(test_entity->collision_bounds));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, ese_rect_get_y(test_entity->collision_bounds));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 110.0f, ese_rect_get_width(test_entity->collision_bounds)); // 30 + 70 + 10
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 120.0f, ese_rect_get_height(test_entity->collision_bounds)); // max(60, 140) - 20 = 120
    
    // Test world bounds (entity position + entity bounds)
    TEST_ASSERT_NOT_NULL(test_entity->collision_world_bounds);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 110.0f, ese_rect_get_x(test_entity->collision_world_bounds)); // 100 + 10
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 220.0f, ese_rect_get_y(test_entity->collision_world_bounds)); // 200 + 20
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 110.0f, ese_rect_get_width(test_entity->collision_world_bounds));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 120.0f, ese_rect_get_height(test_entity->collision_world_bounds));
    
}

void test_entity_component_collider_update_bounds_no_entity(void) {
    EseEntityComponent *component = entity_component_collider_create(test_engine);
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    
    // Should not crash when no entity attached
    entity_component_collider_update_bounds(collider);
    
    entity_component_destroy(component);
}

void test_entity_component_collider_update_bounds_no_rects(void) {
    EseEntityComponent *component = entity_component_collider_create(test_engine);
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    
    // Attach to entity
    entity_component_add(test_entity, component);
    
    // Update bounds with no rects
    entity_component_collider_update_bounds(collider);
    
    // Bounds should be cleared
    TEST_ASSERT_NULL(test_entity->collision_bounds);
    TEST_ASSERT_NULL(test_entity->collision_world_bounds);
}

void test_entity_component_collider_update_bounds_null_collider(void) {
    TEST_ASSERT_DEATH((entity_component_collider_update_bounds(NULL)), "entity_component_collider_update_bounds called with NULL collider");
}

void test_entity_component_collider_rect_updated(void) {
    EseEntityComponent *component = entity_component_collider_create(test_engine);
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    
    // Should not crash
    entity_component_collider_rect_updated(collider);
    
    entity_component_destroy(component);
}

void test_entity_component_collider_rect_updated_null_collider(void) {
    TEST_ASSERT_DEATH((entity_component_collider_rect_updated(NULL)), "entity_component_collider_rect_updated called with NULL collider");
}

void test_entity_component_collider_position_changed(void) {
    EseEntityComponent *component = entity_component_collider_create(test_engine);
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    
    // Should not crash
    entity_component_collider_position_changed(collider);
    
    entity_component_destroy(component);
}

void test_entity_component_collider_position_changed_null_collider(void) {
    TEST_ASSERT_DEATH((entity_component_collider_position_changed(NULL)), "entity_component_collider_position_changed called with NULL collider");
}

void test_entity_component_collider_update_world_bounds_only(void) {
    EseEntityComponent *component = entity_component_collider_create(test_engine);
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    
    // Attach to entity
    entity_component_add(test_entity, component);
    ese_point_set_x(test_entity->position, 100.0f);
    ese_point_set_y(test_entity->position, 200.0f);
    
    // Add rect and update bounds first
    EseRect *rect = ese_rect_create(test_engine);
    ese_rect_set_x(rect, 10.0f);
    ese_rect_set_y(rect, 20.0f);
    ese_rect_set_width(rect, 30.0f);
    ese_rect_set_height(rect, 40.0f);
    
    entity_component_collider_rects_add(collider, rect);
    entity_component_collider_update_bounds(collider);
    
    // Change entity position
    ese_point_set_x(test_entity->position, 300.0f);
    ese_point_set_y(test_entity->position, 400.0f);
    
    // Update only world bounds
    entity_component_collider_update_world_bounds_only(collider);
    
    // World bounds should be updated with new position
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 310.0f, ese_rect_get_x(test_entity->collision_world_bounds)); // 300 + 10
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 420.0f, ese_rect_get_y(test_entity->collision_world_bounds)); // 400 + 20
}

void test_entity_component_collider_update_world_bounds_only_no_entity(void) {
    EseEntityComponent *component = entity_component_collider_create(test_engine);
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    
    // Should not crash when no entity attached
    entity_component_collider_update_world_bounds_only(collider);
    
    entity_component_destroy(component);
}

void test_entity_component_collider_update_world_bounds_only_null_collider(void) {
    TEST_ASSERT_DEATH((entity_component_collider_update_world_bounds_only(NULL)), "entity_component_collider_update_world_bounds_only called with NULL collider");
}

// Lua API Tests
void test_entity_component_collider_lua_init(void) {
    lua_State *L = test_engine->runtime;
    
    // Initialize collider component
    _entity_component_collider_init(test_engine);
    
    // Test that the global table exists and has expected functions
    const char *test_code = "return type(EntityComponentCollider) == 'table' and type(EntityComponentCollider.new) == 'function'";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "EntityComponentCollider table and new function should exist");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_collider_lua_init_null_engine(void) {
    TEST_ASSERT_DEATH((_entity_component_collider_init(NULL)), "_entity_component_collider_init called with NULL engine");
}

void test_entity_component_collider_lua_new(void) {
    lua_State *L = test_engine->runtime;
    
    _entity_component_collider_init(test_engine);
    
    // Test new() with no arguments
    const char *test_code = "return EntityComponentCollider.new()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Collider creation should execute without error");
    
    TEST_ASSERT_TRUE(lua_isuserdata(L, -1));
    EseEntityComponentCollider *collider = _entity_component_collider_get(L, -1);
    TEST_ASSERT_NOT_NULL(collider);
    TEST_ASSERT_EQUAL(0, collider->rects_count);
    TEST_ASSERT_FALSE(collider->draw_debug);
    
    lua_pop(L, 1);
}

void test_entity_component_collider_lua_new_with_rect(void) {
    lua_State *L = test_engine->runtime;
    
    _entity_component_collider_init(test_engine);
    ese_rect_lua_init(test_engine);
    
    // Create collider with rect using Lua script
    const char *test_code = 
        "local rect = Rect.new(10, 20, 30, 40)\n"
        "return EntityComponentCollider.new(rect)";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Collider creation with rect should execute without error");
    
    TEST_ASSERT_TRUE(lua_isuserdata(L, -1));
    EseEntityComponentCollider *collider = _entity_component_collider_get(L, -1);
    TEST_ASSERT_NOT_NULL(collider);
    TEST_ASSERT_EQUAL(1, collider->rects_count);
    
    lua_pop(L, 1);
}

void test_entity_component_collider_lua_new_invalid_args(void) {
    lua_State *L = test_engine->runtime;
    
    _entity_component_collider_init(test_engine);
    
    // Test with invalid argument count - should fail
    const char *test_code = "return EntityComponentCollider.new(1, 2, 3)";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Collider creation with invalid args should fail");
}

void test_entity_component_collider_lua_get(void) {
    lua_State *L = test_engine->runtime;
    
    _entity_component_collider_init(test_engine);
    
    // Test with valid userdata
    const char *test_code = "return EntityComponentCollider.new()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Collider creation should execute without error");
    
    EseEntityComponentCollider *collider = _entity_component_collider_get(L, -1);
    TEST_ASSERT_NOT_NULL(collider);
    
    lua_pop(L, 1);
}

void test_entity_component_collider_lua_get_null_lua_state(void) {
    TEST_ASSERT_DEATH((_entity_component_collider_get(NULL, 1)), "_entity_component_collider_get called with NULL Lua state");
}

void test_entity_component_collider_lua_properties(void) {
    lua_State *L = test_engine->runtime;
    
    _entity_component_collider_init(test_engine);
    
    // Test properties using Lua script
    const char *test_code = 
        "local c = EntityComponentCollider.new()\n"
        "return c.active == true and type(c.id) == 'string' and c.draw_debug == false and type(c.rects) == 'userdata'";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Property access should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_collider_lua_property_setters(void) {
    lua_State *L = test_engine->runtime;
    
    _entity_component_collider_init(test_engine);
    
    // Test property setters using Lua script
    const char *test_code = 
        "local c = EntityComponentCollider.new()\n"
        "c.active = false\n"
        "c.draw_debug = true\n"
        "return c.active == false and c.draw_debug == true";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Property setters should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_collider_lua_rects_operations(void) {
    lua_State *L = test_engine->runtime;
    
    _entity_component_collider_init(test_engine);
    
    // Test rects operations using Lua script
    const char *test_code = 
        "local c = EntityComponentCollider.new()\n"
        "return c.rects.count == 0 and type(c.rects.add) == 'function' and type(c.rects.remove) == 'function' and type(c.rects.insert) == 'function' and type(c.rects.pop) == 'function' and type(c.rects.shift) == 'function'";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Rects operations should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_collider_lua_rects_add(void) {
    lua_State *L = test_engine->runtime;
    
    _entity_component_collider_init(test_engine);
    ese_rect_lua_init(test_engine);
    
    // Test rects add using Lua script
    const char *test_code = 
        "local c = EntityComponentCollider.new()\n"
        "local rect = Rect.new(10, 20, 30, 40)\n"
        "c.rects:add(rect)\n"
        "return c.rects.count == 1";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Rects add should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_collider_lua_rects_remove(void) {
    lua_State *L = test_engine->runtime;
    
    _entity_component_collider_init(test_engine);
    ese_rect_lua_init(test_engine);
    
    // Test rects remove using Lua script
    const char *test_code = 
        "local c = EntityComponentCollider.new()\n"
        "local rect = Rect.new(10, 20, 30, 40)\n"
        "c.rects:add(rect)\n"
        "local removed = c.rects:remove(rect)\n"
        "return removed == true and c.rects.count == 0";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Rects remove should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_collider_lua_rects_insert(void) {
    lua_State *L = test_engine->runtime;
    
    _entity_component_collider_init(test_engine);
    ese_rect_lua_init(test_engine);
    
    // Test rects insert using Lua script
    const char *test_code = 
        "local c = EntityComponentCollider.new()\n"
        "local rect = Rect.new(10, 20, 30, 40)\n"
        "c.rects:insert(rect, 1)\n"
        "return c.rects.count == 1";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Rects insert should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_collider_lua_rects_pop(void) {
    lua_State *L = test_engine->runtime;
    
    _entity_component_collider_init(test_engine);
    ese_rect_lua_init(test_engine);
    
    // Test rects pop using Lua script
    const char *test_code = 
        "local c = EntityComponentCollider.new()\n"
        "local rect = Rect.new(10, 20, 30, 40)\n"
        "c.rects:add(rect)\n"
        "local popped = c.rects:pop()\n"
        "return c.rects.count == 0";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Rects pop should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_collider_lua_rects_shift(void) {
    lua_State *L = test_engine->runtime;
    
    _entity_component_collider_init(test_engine);
    ese_rect_lua_init(test_engine);
    
    // Test rects shift using Lua script
    const char *test_code = 
        "local c = EntityComponentCollider.new()\n"
        "local rect = Rect.new(10, 20, 30, 40)\n"
        "c.rects:add(rect)\n"
        "local shifted = c.rects:shift()\n"
        "return c.rects.count == 0";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Rects shift should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_collider_lua_tostring(void) {
    lua_State *L = test_engine->runtime;
    
    _entity_component_collider_init(test_engine);
    
    // Test tostring using Lua script
    const char *test_code = 
        "local c = EntityComponentCollider.new()\n"
        "local str = tostring(c)\n"
        "return str:find('EntityComponentCollider') ~= nil and str:find('active=true') ~= nil and str:find('draw_debug=false') ~= nil";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Tostring should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_collider_lua_gc(void) {
    lua_State *L = test_engine->runtime;
    
    _entity_component_collider_init(test_engine);
    
    // Create collider directly in C to test reference counting
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)entity_component_collider_create(test_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(collider, "Collider should not be NULL");
    
    // Test reference counting directly
    int initial_ref_count = collider->base.lua_ref_count;
    TEST_ASSERT_EQUAL_MESSAGE(1, initial_ref_count, "Initial reference count should be 1");
    
    // Test unref
    entity_component_collider_unref(collider);
    TEST_ASSERT_EQUAL_MESSAGE(0, collider->base.lua_ref_count, "Reference count should be 0 after unref");
    
    // Test ref again
    entity_component_collider_ref(collider);
    TEST_ASSERT_EQUAL_MESSAGE(1, collider->base.lua_ref_count, "Reference count should be 1 after ref");
    
    // Clean up
    entity_component_collider_unref(collider);
    entity_component_destroy((EseEntityComponent *)collider);
}

// Test runner
int main(void) {
    log_init();
    UNITY_BEGIN();
    
    // // C API Tests
    RUN_TEST(test_entity_component_collider_create);
    RUN_TEST(test_entity_component_collider_create_null_engine);
    RUN_TEST(test_entity_component_collider_draw_debug_getter_setter);
    RUN_TEST(test_entity_component_collider_draw_debug_null_collider);
    RUN_TEST(test_entity_component_collider_rects_add);
    RUN_TEST(test_entity_component_collider_rects_add_null_params);
    RUN_TEST(test_entity_component_collider_ref_unref);
    RUN_TEST(test_entity_component_collider_ref_null_collider);
    RUN_TEST(test_entity_component_collider_copy);
    RUN_TEST(test_entity_component_collider_copy_null_src);
    RUN_TEST(test_entity_component_collider_destroy);
    RUN_TEST(test_entity_component_collider_destroy_null_collider);
    RUN_TEST(test_entity_component_collider_update_bounds);
    RUN_TEST(test_entity_component_collider_update_bounds_no_entity);
    RUN_TEST(test_entity_component_collider_update_bounds_no_rects);
    RUN_TEST(test_entity_component_collider_update_bounds_null_collider);
    RUN_TEST(test_entity_component_collider_rect_updated);
    RUN_TEST(test_entity_component_collider_rect_updated_null_collider);
    RUN_TEST(test_entity_component_collider_position_changed);
    RUN_TEST(test_entity_component_collider_position_changed_null_collider);
    RUN_TEST(test_entity_component_collider_update_world_bounds_only);
    RUN_TEST(test_entity_component_collider_update_world_bounds_only_no_entity);
    RUN_TEST(test_entity_component_collider_update_world_bounds_only_null_collider);
    
    // Lua API Tests
    RUN_TEST(test_entity_component_collider_lua_init);
    RUN_TEST(test_entity_component_collider_lua_init_null_engine);
    RUN_TEST(test_entity_component_collider_lua_new);
    RUN_TEST(test_entity_component_collider_lua_new_with_rect);
    RUN_TEST(test_entity_component_collider_lua_new_invalid_args);
    RUN_TEST(test_entity_component_collider_lua_get);
    RUN_TEST(test_entity_component_collider_lua_get_null_lua_state);
    RUN_TEST(test_entity_component_collider_lua_properties);
    RUN_TEST(test_entity_component_collider_lua_property_setters);
    RUN_TEST(test_entity_component_collider_lua_rects_operations);
    RUN_TEST(test_entity_component_collider_lua_rects_add);
    RUN_TEST(test_entity_component_collider_lua_rects_remove);
    RUN_TEST(test_entity_component_collider_lua_rects_insert);
    RUN_TEST(test_entity_component_collider_lua_rects_pop);
    RUN_TEST(test_entity_component_collider_lua_rects_shift);
    RUN_TEST(test_entity_component_collider_lua_tostring);
    RUN_TEST(test_entity_component_collider_lua_gc);
    
    memory_manager.destroy(true);
    
    return UNITY_END();
}
