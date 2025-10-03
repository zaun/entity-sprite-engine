#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "testing.h"
#include "../src/entity/components/entity_component_lua.h"
#include "../src/entity/components/entity_component.h"
#include "../src/entity/entity.h"
#include "../src/entity/entity_lua.h"
#include "../src/scripting/lua_engine.h"
#include "../src/core/engine.h"
#include "../src/core/engine_private.h"
#include "../src/scripting/lua_value.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"
#include "../src/utility/double_linked_list.h"

// Test data
static EseLuaEngine *test_engine = NULL;
static EseEntity *test_entity = NULL;
static EseEngine mock_engine;
#define TEST_ENTITY_ID "test_entity_12345"

// Test setup and teardown
void setUp(void) {
    test_engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(test_engine);
    
    // Load test script from string
    const char *script_content = 
        "function TEST_SCRIPT:test_func()\n"
        "    self.data.test = 1\n"
        "end\n"
        "function TEST_SCRIPT:entity_init()\n"
        "    self.data.init = 1\n"
        "end\n"
        "function TEST_SCRIPT:entity_update()\n"
        "    self.data.update = 1\n"
        "end\n"
        "function TEST_SCRIPT:entity_collision_enter()\n"
        "    self.data.collision = 1\n"
        "end\n"
        "function TEST_SCRIPT:void_function()\n"
        "end\n";
    bool script_loaded = lua_engine_load_script_from_string(test_engine, script_content, "test_script", "TEST_SCRIPT");
    TEST_ASSERT_TRUE(script_loaded);
    
    // Initialize entity Lua system first (creates ComponentsProxyMeta)
    entity_lua_init(test_engine);
    
    // Initialize Lua component system
    entity_component_lua_init(test_engine);
    
    // Create a mock engine structure for _entity_lua_new
    mock_engine.lua_engine = test_engine;
    mock_engine.entities = dlist_create(NULL); // Simple entity list
    
    // Add the mock engine to the registry so _entity_lua_new can find it
    lua_engine_add_registry_key(test_engine->runtime, ENGINE_KEY, &mock_engine);

    test_entity = entity_create(test_engine);
    TEST_ASSERT_NOT_NULL(test_entity);
}

void tearDown(void) {
    entity_destroy(test_entity);
    test_entity = NULL;

    lua_engine_destroy(test_engine);
    test_engine = NULL;

    // Clean up the mock engine's linked list
    dlist_free(mock_engine.entities);
    mock_engine.entities = NULL;
}

// C API Tests
void test_entity_component_lua_create_null_engine(void) {
    ASSERT_DEATH((entity_component_lua_create(NULL, "test")), "entity_component_lua_create should abort with NULL engine");
}

void test_entity_component_lua_create_basic(void) {
    EseEntityComponent *component = entity_component_lua_create(test_engine, "test_script");
    TEST_ASSERT_NOT_NULL(component);
    TEST_ASSERT_TRUE(component->active);
    TEST_ASSERT_NOT_NULL(component->data);
    
    EseEntityComponentLua *lua_comp = (EseEntityComponentLua *)component->data;
    TEST_ASSERT_NOT_NULL(lua_comp->script);
    TEST_ASSERT_EQUAL_STRING("test_script", lua_comp->script);
    TEST_ASSERT_EQUAL(LUA_NOREF, lua_comp->instance_ref);
    TEST_ASSERT_NOT_NULL(lua_comp->function_cache);
    
    entity_component_destroy(component);
}

void test_entity_component_lua_create_null_script(void) {
    EseEntityComponent *component = entity_component_lua_create(test_engine, NULL);
    TEST_ASSERT_NOT_NULL(component);
    TEST_ASSERT_NULL(((EseEntityComponentLua *)component->data)->script);
    
    entity_component_destroy(component);
}

void test_entity_component_lua_ref_null_component(void) {
    ASSERT_DEATH((entity_component_lua_ref(NULL)), "entity_component_lua_ref should abort with NULL component");
}

void test_entity_component_lua_ref_basic(void) {
    EseEntityComponent *component = entity_component_lua_create(test_engine, "test_script");
    EseEntityComponentLua *lua_comp = (EseEntityComponentLua *)component->data;
    
    // Component should already be referenced from create
    TEST_ASSERT_NOT_EQUAL(LUA_NOREF, lua_comp->base.lua_ref);
    TEST_ASSERT_EQUAL(1, lua_comp->base.lua_ref_count);
    
    // Ref again should increment count
    entity_component_lua_ref(lua_comp);
    TEST_ASSERT_EQUAL(2, lua_comp->base.lua_ref_count);
        
    entity_component_destroy(component);
    // Not destroyed but ref count should be 1
    TEST_ASSERT_EQUAL(1, lua_comp->base.lua_ref_count);

    entity_component_destroy(component);
}

void test_entity_component_lua_unref_null_component(void) {
    ASSERT_DEATH((entity_component_lua_unref(NULL)), "entity_component_lua_unref should abort with NULL component");
}

void test_entity_component_lua_unref_basic(void) {
    EseEntityComponent *component = entity_component_lua_create(test_engine, "test_script");
    EseEntityComponentLua *lua_comp = (EseEntityComponentLua *)component->data;
    
    // Ref twice
    entity_component_lua_ref(lua_comp);
    TEST_ASSERT_EQUAL(2, lua_comp->base.lua_ref_count);
    
    // Unref once
    entity_component_lua_unref(lua_comp);
    TEST_ASSERT_EQUAL(1, lua_comp->base.lua_ref_count);
    TEST_ASSERT_NOT_EQUAL(LUA_NOREF, lua_comp->base.lua_ref);
    
    // Unref again should clear reference
    entity_component_lua_unref(lua_comp);
    TEST_ASSERT_EQUAL(0, lua_comp->base.lua_ref_count);
    TEST_ASSERT_EQUAL(LUA_NOREF, lua_comp->base.lua_ref);
    
    entity_component_destroy(component);
}

void test_entity_component_lua_run_null_component(void) {
    ASSERT_DEATH((entity_component_lua_run(NULL, test_entity, "test", 0, NULL)), "entity_component_lua_run should abort with NULL component");
}

void test_entity_component_lua_run_null_entity(void) {
    EseEntityComponent *component = entity_component_lua_create(test_engine, "test_script");
    EseEntityComponentLua *lua_comp = (EseEntityComponentLua *)component->data;
    
    ASSERT_DEATH((entity_component_lua_run(lua_comp, NULL, "test", 0, NULL)), "entity_component_lua_run should abort with NULL entity");
    
    entity_component_destroy(component);
}

void test_entity_component_lua_run_basic(void) {
    printf("DEBUG: Starting test_entity_component_lua_run_basic\n");
    fflush(stdout);
    printf("DEBUG: test_engine = %p\n", test_engine);
    fflush(stdout);
    printf("DEBUG: test_entity = %p\n", test_entity);
    fflush(stdout);
    
    EseEntityComponent *component = entity_component_lua_create(test_engine, "test_script");
    printf("DEBUG: component = %p\n", component);
    fflush(stdout);
    
    EseEntityComponentLua *lua_comp = (EseEntityComponentLua *)component->data;
    printf("DEBUG: lua_comp = %p\n", lua_comp);
    fflush(stdout);
    
    // Add component to entity
    printf("DEBUG: About to call entity_component_add\n");
    fflush(stdout);
    entity_component_add(test_entity, component);
    printf("DEBUG: entity_component_add completed\n");
    fflush(stdout);
    
    // Run the function
    printf("DEBUG: About to call entity_component_lua_run\n");
    fflush(stdout);
    bool result = entity_component_lua_run(lua_comp, test_entity, "test_func", 0, NULL);
    printf("DEBUG: entity_component_lua_run completed\n");
    fflush(stdout);
    printf("Function execution result: %s\n", result ? "true" : "false");
    fflush(stdout);
    printf("Instance ref: %d\n", lua_comp->instance_ref);
    fflush(stdout);
    printf("Script: %s\n", lua_comp->script ? lua_comp->script : "NULL");
    fflush(stdout);
    TEST_ASSERT_TRUE(result);
    
    // Don't manually destroy component - entity_destroy will handle it
    printf("DEBUG: Skipping manual component destroy - entity will handle it\n");
    fflush(stdout);
}

// Lua API Tests
void test_entity_component_lua_lua_init(void) {
    lua_State *L = test_engine->runtime;
    
    // Check if EntityComponentLua is available (should be initialized in setUp)
    lua_getglobal(L, "EntityComponentLua");
    TEST_ASSERT_FALSE(lua_isnil(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_lua_lua_new_basic(void) {
    lua_State *L = test_engine->runtime;
    
    // Test basic creation
    const char *test_code = 
        "local comp = EntityComponentLua.new()\n"
        "return comp ~= nil and type(comp) == 'userdata'";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Basic creation should work");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_lua_lua_new_with_script(void) {
    lua_State *L = test_engine->runtime;
    
    // Test creation with script
    const char *test_code = 
        "local comp = EntityComponentLua.new('print(\"hello\")')\n"
        "return comp ~= nil and type(comp) == 'userdata'";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Creation with script should work");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_lua_lua_properties(void) {
    lua_State *L = test_engine->runtime;
    
    // Test properties using Lua script
    const char *test_code = 
        "local c = EntityComponentLua.new('test_script')\n"
        "return c.active == true and type(c.id) == 'string' and c.script == 'test_script'";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Property access should execute without error");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_lua_lua_properties_with_script(void) {
    lua_State *L = test_engine->runtime;
    
    // Test properties with script
    const char *test_code = 
        "local c = EntityComponentLua.new('print(\"test\")')\n"
        "return c.active == true and type(c.id) == 'string' and c.script == 'print(\"test\")'";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Property access with script should work");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_lua_lua_property_setting(void) {
    lua_State *L = test_engine->runtime;
    
    // Test property setting
    const char *test_code = 
        "local c = EntityComponentLua.new()\n"
        "c.active = false\n"
        "c.script = 'print(\"modified\")'\n"
        "return c.active == false and c.script == 'print(\"modified\")'";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Property setting should work");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_lua_lua_tostring(void) {
    lua_State *L = test_engine->runtime;
    
    // Test tostring
    const char *test_code = 
        "local c = EntityComponentLua.new('print(\"test\")')\n"
        "local str = tostring(c)\n"
        "return type(str) == 'string' and string.find(str, 'EntityComponentLua') ~= nil";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "tostring should work");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_lua_lua_gc(void) {
    lua_State *L = test_engine->runtime;
    
    // Test garbage collection
    const char *test_code = 
        "local c = EntityComponentLua.new('print(\"test\")')\n"
        "c = nil\n"
        "collectgarbage()\n"
        "return true"; // Just test that GC doesn't crash
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Garbage collection should work");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

void test_entity_component_lua_lua_function_execution(void) {
    // Add component to the global test_entity
    EseEntityComponent *component = entity_component_lua_create(test_engine, "test_script");
    entity_component_add(test_entity, component);
    
    // Test function execution by calling dispatch directly from C
    entity_run_function_with_args(test_entity, "test_func", 0, NULL);
    
    // Verify that Lua updated entity.__data.test = 1
    lua_State *L = test_engine->runtime;
    lua_rawgeti(L, LUA_REGISTRYINDEX, entity_get_lua_ref(test_entity));
    lua_getfield(L, -1, "__data");
    lua_getfield(L, -1, "test");
    int test_val = (int)lua_tointeger(L, -1);
    lua_pop(L, 3);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, test_val, "test_func should set self.data.test = 1");
}

void test_entity_component_lua_lua_update_function(void) {
    // Add component to the global test_entity
    EseEntityComponent *component = entity_component_lua_create(test_engine, "test_script");
    entity_component_add(test_entity, component);
    
    // Test update function by calling dispatch directly from C with delta time argument
    EseLuaValue *delta_time = lua_value_create_number("delta_time", 1.5);
    EseLuaValue *args[] = {delta_time};
    entity_run_function_with_args(test_entity, "entity_update", 1, args);
    lua_value_destroy(delta_time);
    
    // Verify that Lua updated entity.__data.update = 1
    lua_State *L = test_engine->runtime;
    lua_rawgeti(L, LUA_REGISTRYINDEX, entity_get_lua_ref(test_entity));
    lua_getfield(L, -1, "__data");
    lua_getfield(L, -1, "update");
    int update_val = (int)lua_tointeger(L, -1);
    lua_pop(L, 3);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, update_val, "entity_update should set self.data.update = 1");
}

void test_entity_component_lua_lua_collision_functions(void) {
    // Add component to the global test_entity
    EseEntityComponent *component = entity_component_lua_create(test_engine, "test_script");
    entity_component_add(test_entity, component);
    
    // Test collision function by calling dispatch directly from C
    entity_run_function_with_args(test_entity, "entity_collision_enter", 0, NULL);
    
    // Verify that Lua updated entity.__data.collision = 1
    lua_State *L = test_engine->runtime;
    lua_rawgeti(L, LUA_REGISTRYINDEX, entity_get_lua_ref(test_entity));
    lua_getfield(L, -1, "__data");
    lua_getfield(L, -1, "collision");
    int collision_val = (int)lua_tointeger(L, -1);
    lua_pop(L, 3);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, collision_val, "entity_collision_enter should set self.data.collision = 1");
}

void test_entity_component_lua_lua_memory_management(void) {
    lua_State *L = test_engine->runtime;
    
    // Test memory management with multiple components
    const char *test_code = 
        "local components = {}\n"
        "for i = 1, 10 do\n"
        "    components[i] = EntityComponentLua.new('test_script')\n"
        "end\n"
        "return true"; // Just test that it doesn't crash
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, test_code), "Memory management should work");
    TEST_ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

int main(void) {
    log_init();
    UNITY_BEGIN();
    
    // C API Tests
    RUN_TEST(test_entity_component_lua_create_null_engine);
    RUN_TEST(test_entity_component_lua_create_basic);
    RUN_TEST(test_entity_component_lua_create_null_script);
    RUN_TEST(test_entity_component_lua_ref_null_component);
    RUN_TEST(test_entity_component_lua_ref_basic);
    RUN_TEST(test_entity_component_lua_unref_null_component);
    RUN_TEST(test_entity_component_lua_unref_basic);
    RUN_TEST(test_entity_component_lua_run_null_component);
    RUN_TEST(test_entity_component_lua_run_null_entity);
    RUN_TEST(test_entity_component_lua_run_basic);
    
    // Lua API Tests
    RUN_TEST(test_entity_component_lua_lua_init);
    RUN_TEST(test_entity_component_lua_lua_new_basic);
    RUN_TEST(test_entity_component_lua_lua_new_with_script);
    RUN_TEST(test_entity_component_lua_lua_properties);
    RUN_TEST(test_entity_component_lua_lua_properties_with_script);
    RUN_TEST(test_entity_component_lua_lua_property_setting);
    RUN_TEST(test_entity_component_lua_lua_tostring);
    RUN_TEST(test_entity_component_lua_lua_gc);
    RUN_TEST(test_entity_component_lua_lua_function_execution);
    RUN_TEST(test_entity_component_lua_lua_update_function);
    RUN_TEST(test_entity_component_lua_lua_collision_functions);
    RUN_TEST(test_entity_component_lua_lua_memory_management);
    
    memory_manager.destroy();
    
    return UNITY_END();
}
