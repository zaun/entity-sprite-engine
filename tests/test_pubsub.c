/*
* test_pub_sub.c - Unity-based tests for pub_sub functionality
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

#include "../src/core/engine.h"
#include "../src/core/engine_private.h"
#include "../src/entity/entity.h"
#include "../src/entity/entity_private.h"
#include "../src/entity/entity_lua.h"
#include "../src/entity/components/entity_component.h"
#include "../src/entity/components/entity_component_lua.h"
#include "../src/scripting/lua_engine.h"
#include "../src/scripting/lua_value.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

// Test Lua script content for entity pub/sub
static const char* test_entity_script = 
"function TEST_ENTITY:on_test_event(event_name, data)\n"
"    self.data.test_event_called = true\n"
"    self.data.test_event_count = (self.data.test_event_count or 0) + 1\n"
"    self.data.last_event_name = event_name\n"
"    self.data.last_data = data\n"
"    return true\n"
"end\n"
"\n"
"function TEST_ENTITY:on_test_event_2(event_name, data)\n"
"    self.data.test_event_2_called = true\n"
"    self.data.test_event_2_count = (self.data.test_event_2_count or 0) + 10\n"
"    self.data.last_event_name = event_name\n"
"    self.data.last_data = data\n"
"    return true\n"
"end\n"
"\n"
"function TEST_ENTITY:on_custom_event(event_name, data)\n"
"    self.data.custom_event_called = true\n"
"    self.data.custom_event_count = (self.data.custom_event_count or 0) + 1\n"
"    self.data.last_event_name = event_name\n"
"    self.data.last_data = data\n"
"    return true\n"
"end\n";

/**
* C API Test Functions Declarations
*/
static void test_pubsub_create(void);
static void test_pubsub_destroy(void);
static void test_pubsub_subscribe(void);
static void test_pubsub_unsubscribe(void);
static void test_pubsub_publish(void);
static void test_pubsub_multiple_subscribers(void);
static void test_pubsub_multiple_topics(void);
static void test_pubsub_empty_topic(void);
static void test_pubsub_null_handling(void);

// Helper function declarations
static EseEngine* create_test_engine_with_entity_support(void);
static EseEntity* create_test_entity_with_script(EseEngine *engine, const char *script_name);

/**
* Test suite setup and teardown
*/
static EseEngine *g_engine = NULL;

void setUp(void) {
    g_engine = create_test_engine_with_entity_support();
}

void tearDown(void) {
    if (g_engine) {
        engine_destroy(g_engine);
        g_engine = NULL;
    }
}

/**
* Main test runner
*/
int main(void) {
    log_init();

    printf("\nEsePubSub Tests\n");
    printf("---------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_pubsub_create);
    RUN_TEST(test_pubsub_destroy);
    RUN_TEST(test_pubsub_subscribe);
    RUN_TEST(test_pubsub_unsubscribe);
    RUN_TEST(test_pubsub_publish);
    RUN_TEST(test_pubsub_multiple_subscribers);
    RUN_TEST(test_pubsub_multiple_topics);
    RUN_TEST(test_pubsub_empty_topic);
    RUN_TEST(test_pubsub_null_handling);

    return UNITY_END();
}

// Helper function to create and initialize engine
static EseEngine* create_test_engine_with_entity_support(void) {
    EseLuaEngine *lua_engine = create_test_engine();
    if (!lua_engine) {
        return NULL;
    }
    
    EseEngine *engine = engine_create(NULL); // No startup script
    if (engine) {
        // Set up registry keys that entity system needs
        lua_engine_add_registry_key(engine->lua_engine->runtime, LUA_ENGINE_KEY, engine->lua_engine);
        lua_engine_add_registry_key(engine->lua_engine->runtime, ENGINE_KEY, engine);
        
        // Initialize entity system
        entity_lua_init(engine->lua_engine);
        entity_component_lua_init(engine->lua_engine);
    }
    return engine;
}

// Helper function to create test entity with Lua component
static EseEntity* create_test_entity_with_script(EseEngine *engine, const char *script_name) {
    EseEntity *entity = entity_create(engine->lua_engine);
    if (entity) {
        // Load test script
        bool load_result = lua_engine_load_script_from_string(engine->lua_engine, test_entity_script, script_name, "TEST_ENTITY");
        if (load_result) {
            // Create Lua component
            EseEntityComponent *lua_comp = entity_component_lua_create(engine->lua_engine, script_name);
            if (lua_comp) {
                entity_component_add(entity, lua_comp);
            }
        }
    }
    return entity;
}

static void test_pubsub_create(void) {
    TEST_ASSERT_NOT_NULL(g_engine);
    TEST_ASSERT_NOT_NULL(g_engine->pub_sub);
}

static void test_pubsub_destroy(void) {
    // Test that engine destruction properly cleans up pubsub
    EseEngine *test_engine = create_test_engine_with_entity_support();
    TEST_ASSERT_NOT_NULL(test_engine);
    TEST_ASSERT_NOT_NULL(test_engine->pub_sub);
    
    // Should not crash when destroying engine
    engine_destroy(test_engine);
}

static void test_pubsub_subscribe(void) {
    TEST_ASSERT_NOT_NULL(g_engine);
    
    EseEntity *entity = create_test_entity_with_script(g_engine, "test_entity_script");
    TEST_ASSERT_NOT_NULL(entity);
    
    // Subscribe to a topic
    engine_pubsub_sub(g_engine, "test_topic", entity, "on_test_event");
    
    // Verify subscription worked by publishing
    EseLuaValue *data = lua_value_create_string("test_data", "test_data");
    engine_pubsub_pub(g_engine, "test_topic", data);
    
    // Update entity to trigger Lua component
    entity_update(entity, 0.016f);
    
    // Check if the Lua function was called
    lua_State *L = g_engine->lua_engine->runtime;
    entity_lua_push(entity);
    lua_getfield(L, -1, "data");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "test_event_called");
        bool test_event_called = lua_toboolean(L, -1);
        TEST_ASSERT(test_event_called);
        lua_pop(L, 1);
    }
    lua_pop(L, 2); // Pop data table and entity
    
    lua_value_free(data);
    entity_destroy(entity);
}

static void test_pubsub_unsubscribe(void) {
    TEST_ASSERT_NOT_NULL(g_engine);
    
    EseEntity *entity = create_test_entity_with_script(g_engine, "test_entity_script");
    TEST_ASSERT_NOT_NULL(entity);
    
    // Subscribe to a topic
    engine_pubsub_sub(g_engine, "test_topic", entity, "on_test_event");
    
    // Publish once to verify subscription
    EseLuaValue *data1 = lua_value_create_string("test_data_1", "test_data_1");
    engine_pubsub_pub(g_engine, "test_topic", data1);
    entity_update(entity, 0.016f);
    
    // Check that function was called
    lua_State *L = g_engine->lua_engine->runtime;
    entity_lua_push(entity);
    lua_getfield(L, -1, "data");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "test_event_count");
        int count = lua_tointeger(L, -1);
        TEST_ASSERT_EQUAL_INT(1, count);
        lua_pop(L, 1);
    }
    lua_pop(L, 2);
    
    // Unsubscribe
    engine_pubsub_unsub(g_engine, "test_topic", entity, "on_test_event");
    
    // Publish again - should not call callback
    EseLuaValue *data2 = lua_value_create_string("test_data_2", "test_data_2");
    engine_pubsub_pub(g_engine, "test_topic", data2);
    entity_update(entity, 0.016f);
    
    // Check that function was not called again
    entity_lua_push(entity);
    lua_getfield(L, -1, "data");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "test_event_count");
        int count = lua_tointeger(L, -1);
        TEST_ASSERT_EQUAL_INT(1, count);
        lua_pop(L, 1);
    }
    lua_pop(L, 2);
    
    lua_value_free(data1);
    lua_value_free(data2);
    entity_destroy(entity);
}

static void test_pubsub_publish(void) {
    TEST_ASSERT_NOT_NULL(g_engine);
    
    EseEntity *entity = create_test_entity_with_script(g_engine, "test_entity_script");
    TEST_ASSERT_NOT_NULL(entity);
    
    // Subscribe to a topic
    engine_pubsub_sub(g_engine, "test_topic", entity, "on_test_event");
    
    // Publish data
    EseLuaValue *data = lua_value_create_number("test_number", 42.5);
    engine_pubsub_pub(g_engine, "test_topic", data);
    
    // Update entity to trigger Lua component
    entity_update(entity, 0.016f);
    
    // Check if the Lua function was called
    lua_State *L = g_engine->lua_engine->runtime;
    entity_lua_push(entity);
    lua_getfield(L, -1, "data");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "test_event_called");
        bool test_event_called = lua_toboolean(L, -1);
        TEST_ASSERT(test_event_called);
        lua_pop(L, 1);
    }
    lua_pop(L, 2);
    
    lua_value_free(data);
    entity_destroy(entity);
}

static void test_pubsub_multiple_subscribers(void) {
    TEST_ASSERT_NOT_NULL(g_engine);
    
    EseEntity *entity1 = create_test_entity_with_script(g_engine, "test_entity_script_1");
    EseEntity *entity2 = create_test_entity_with_script(g_engine, "test_entity_script_2");
    TEST_ASSERT_NOT_NULL(entity1);
    TEST_ASSERT_NOT_NULL(entity2);
    
    // Subscribe two different entities to the same topic
    engine_pubsub_sub(g_engine, "test_topic", entity1, "on_test_event");
    engine_pubsub_sub(g_engine, "test_topic", entity2, "on_test_event_2");
    
    // Publish data
    EseLuaValue *data = lua_value_create_bool("test_bool", true);
    engine_pubsub_pub(g_engine, "test_topic", data);
    
    // Update both entities
    entity_update(entity1, 0.016f);
    entity_update(entity2, 0.016f);
    
    // Check that both entities received the event
    lua_State *L = g_engine->lua_engine->runtime;
    
    // Check entity1
    entity_lua_push(entity1);
    lua_getfield(L, -1, "data");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "test_event_called");
        bool called1 = lua_toboolean(L, -1);
        TEST_ASSERT(called1);
        lua_pop(L, 1);
    }
    lua_pop(L, 2);
    
    // Check entity2
    entity_lua_push(entity2);
    lua_getfield(L, -1, "data");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "test_event_2_called");
        bool called2 = lua_toboolean(L, -1);
        TEST_ASSERT(called2);
        lua_pop(L, 1);
    }
    lua_pop(L, 2);
    
    lua_value_free(data);
    entity_destroy(entity1);
    entity_destroy(entity2);
}

static void test_pubsub_multiple_topics(void) {
    TEST_ASSERT_NOT_NULL(g_engine);
    
    EseEntity *entity = create_test_entity_with_script(g_engine, "test_entity_script");
    TEST_ASSERT_NOT_NULL(entity);
    
    // Subscribe to different topics
    engine_pubsub_sub(g_engine, "topic1", entity, "on_test_event");
    engine_pubsub_sub(g_engine, "topic2", entity, "on_custom_event");
    
    // Publish to topic1 only
    EseLuaValue *data1 = lua_value_create_string("topic1_data", "topic1_data");
    engine_pubsub_pub(g_engine, "topic1", data1);
    entity_update(entity, 0.016f);
    
    // Check that only the first function was called
    lua_State *L = g_engine->lua_engine->runtime;
    entity_lua_push(entity);
    lua_getfield(L, -1, "data");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "test_event_called");
        bool test_event_called = lua_toboolean(L, -1);
        TEST_ASSERT(test_event_called);
        lua_pop(L, 1);
        
        lua_getfield(L, -1, "custom_event_called");
        bool custom_event_called = lua_toboolean(L, -1);
        TEST_ASSERT(!custom_event_called);
        lua_pop(L, 1);
    }
    lua_pop(L, 2);
    
    // Publish to topic2 only
    EseLuaValue *data2 = lua_value_create_string("topic2_data", "topic2_data");
    engine_pubsub_pub(g_engine, "topic2", data2);
    entity_update(entity, 0.016f);
    
    // Check that both functions were called
    entity_lua_push(entity);
    lua_getfield(L, -1, "data");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "test_event_called");
        bool test_event_called = lua_toboolean(L, -1);
        TEST_ASSERT(test_event_called);
        lua_pop(L, 1);
        
        lua_getfield(L, -1, "custom_event_called");
        bool custom_event_called = lua_toboolean(L, -1);
        TEST_ASSERT(custom_event_called);
        lua_pop(L, 1);
    }
    lua_pop(L, 2);
    
    lua_value_free(data1);
    lua_value_free(data2);
    entity_destroy(entity);
}

static void test_pubsub_empty_topic(void) {
    TEST_ASSERT_NOT_NULL(g_engine);
    
    // Publish to non-existent topic - should not crash
    EseLuaValue *data = lua_value_create_string("test", "test");
    engine_pubsub_pub(g_engine, "non_existent_topic", data);
    
    // Unsubscribe from non-existent topic - should not crash
    EseEntity *entity = create_test_entity_with_script(g_engine, "test_entity_script");
    if (entity) {
        engine_pubsub_unsub(g_engine, "non_existent_topic", entity, "on_test_event");
        entity_destroy(entity);
    }
    
    lua_value_free(data);
}

static void test_pubsub_null_handling(void) {
    TEST_ASSERT_NOT_NULL(g_engine);
    
    EseEntity *entity = create_test_entity_with_script(g_engine, "test_entity_script");
    TEST_ASSERT_NOT_NULL(entity);
    
    EseLuaValue *data = lua_value_create_string("test", "test");
    
    // Test with NULL data - should abort in debug builds
    ASSERT_DEATH((engine_pubsub_pub(g_engine, "test_topic", NULL)), "Should abort on NULL data");
    
    lua_value_free(data);
    entity_destroy(entity);
}
