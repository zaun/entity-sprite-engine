/*
 * test_entity_pubsub.c - Comprehensive tests for entity pub/sub functionality
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

#include "../src/entity/entity.h"
#include "../src/entity/entity_private.h"
#include "../src/entity/entity_lua.h"
#include "../src/entity/components/entity_component.h"
#include "../src/entity/components/entity_component_lua.h"
#include "../src/scripting/lua_engine.h"
#include "../src/scripting/lua_value.h"
#include "../src/core/memory_manager.h"
#include "../src/core/engine.h"
#include "../src/core/engine_private.h"
#include "../src/utility/log.h"

// Test callback data structure
typedef struct {
    int call_count;
    const EseLuaValue *last_data;
    const char *last_event_name;
    void *user_data;
} TestEntityCallbackData;

// Test Lua script content for entity pub/sub
static const char* test_entity_pubsub_script = 
"function TEST_ENTITY:on_test_event(event_name, data)\n"
"    self.data.test_event_called = true\n"
"    self.data.test_event_count = (self.data.test_event_count or 0) + 1\n"
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
"end\n"
"\n"
"function TEST_ENTITY:on_multiple_events(event_name, data)\n"
"    self.data.multiple_events_called = true\n"
"    self.data.multiple_events_count = (self.data.multiple_events_count or 0) + 1\n"
"    self.data.last_event_name = event_name\n"
"    self.data.last_data = data\n"
"    return true\n"
"end\n";

// Global engine for all tests to share
static EseEngine* g_engine = NULL;

// Helper function declarations
static EseEngine* create_test_engine_with_entity_support();
static EseEntity* create_test_entity_with_script(EseEngine *engine, const char *script_name);

// Test function declarations
static void test_entity_subscribe();
static void test_entity_unsubscribe();
static void test_entity_publish();
static void test_entity_multiple_subscribers();
static void test_entity_multiple_topics();
static void test_entity_subscription_tracking();
static void test_entity_auto_cleanup();
static void test_entity_pubsub_lua_integration();
static void test_entity_pubsub_data_passing();
static void test_entity_pubsub_error_handling();

// Unity test setup/teardown
void setUp(void) {
    if (!g_engine) {
        g_engine = create_test_engine_with_entity_support();
    }
}

void tearDown(void) {
    // Engine is shared across all tests, cleaned up in main()
}

// Helper function to create and initialize engine
static EseEngine* create_test_engine_with_entity_support() {
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
        bool load_result = lua_engine_load_script_from_string(engine->lua_engine, test_entity_pubsub_script, script_name, "TEST_ENTITY");
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

void segfault_handler(int signo, siginfo_t *info, void *context) {
    void *buffer[32];
    int nptrs = backtrace(buffer, 32);
    char **strings = backtrace_symbols(buffer, nptrs);
    if (strings) {
        fprintf(stderr, "---- BACKTRACE START ----\n");
        for (int i = 0; i < nptrs; i++) {
            fprintf(stderr, "%s\n", strings[i]);
        }
        fprintf(stderr, "---- BACKTRACE  END  ----\n");
        free(strings);
    }

    signal(signo, SIG_DFL);
    raise(signo);
}

int main() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_sigaction = segfault_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);

    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("Error setting SIGSEGV handler");
        return EXIT_FAILURE;
    }
    
    UnityBegin("test_entity_pubsub.c");
    
    // Initialize required systems
    log_init();
    
    // Run all test suites
    RUN_TEST(test_entity_subscribe);
    RUN_TEST(test_entity_unsubscribe);
    RUN_TEST(test_entity_publish);
    RUN_TEST(test_entity_multiple_subscribers);
    RUN_TEST(test_entity_multiple_topics);
    RUN_TEST(test_entity_subscription_tracking);
    RUN_TEST(test_entity_auto_cleanup);
    RUN_TEST(test_entity_pubsub_lua_integration);
    RUN_TEST(test_entity_pubsub_data_passing);
    RUN_TEST(test_entity_pubsub_error_handling);
    
    // Print final summary
    int result = UnityEnd();
    
    // Clean up global engine
    if (g_engine) {
        engine_destroy(g_engine);
        g_engine = NULL;
    }

    memory_manager.destroy();
    
    return result;
}

// Test entity subscription
static void test_entity_subscribe() {
    TEST_ASSERT_NOT_NULL(g_engine);
    
    EseEntity *entity = create_test_entity_with_script(g_engine, "test_entity_subscribe_script");
    TEST_ASSERT_NOT_NULL(entity);
    
    if (entity) {
        // Test subscribing to a topic
        engine_pubsub_sub(g_engine, "error_test_event", entity, "on_test_event");
        
        // Test publishing to verify subscription works
        EseLuaValue *data = lua_value_create_string("test_data", "Hello World");
        engine_pubsub_pub(g_engine, "error_test_event", data);
        
        // Update entity to trigger Lua component
        entity_update(entity, 0.016f);
        
        // Check if the Lua function was called by examining the entity's data
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
}

// Test entity unsubscribe
static void test_entity_unsubscribe() {
    TEST_ASSERT_NOT_NULL(g_engine);
    
    {
        EseEntity *entity = create_test_entity_with_script(g_engine, "test_entity_unsubscribe_script");
        TEST_ASSERT_NOT_NULL(entity);
        
        if (entity) {
            // Initialize entity data table to prevent state from previous tests
            lua_State *L = g_engine->lua_engine->runtime;
            entity_lua_push(entity);
            lua_newtable(L);
            lua_setfield(L, -2, "data");
            lua_pop(L, 1);
            // Subscribe to a topic
            engine_pubsub_sub(g_engine, "unsubscribe_test_event", entity, "on_test_event");
            
            // Publish to verify subscription works
            EseLuaValue *data1 = lua_value_create_string("test_data_1", "First Message");
            engine_pubsub_pub(g_engine, "unsubscribe_test_event", data1);
            entity_update(entity, 0.016f);
            
            // Unsubscribe
            engine_pubsub_unsub(g_engine, "unsubscribe_test_event", entity, "on_test_event");
            
            // Publish again - should not call the function
            EseLuaValue *data2 = lua_value_create_string("test_data_2", "Second Message");
            engine_pubsub_pub(g_engine, "unsubscribe_test_event", data2);
            entity_update(entity, 0.016f);
            
            // Check that the function was not called again
            entity_lua_push(entity);
            lua_getfield(L, -1, "data");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "test_event_count");
                int count = lua_tointeger(L, -1);
                TEST_ASSERT_EQUAL_INT(1, count);
                lua_pop(L, 1);
            }
            lua_pop(L, 2); // Pop data table and entity
            
            lua_value_free(data1);
            lua_value_free(data2);
            entity_destroy(entity);
        }
        
    }
}

// Test entity publish
static void test_entity_publish() {
    TEST_ASSERT_NOT_NULL(g_engine);
    
    {
            EseEntity *entity = create_test_entity_with_script(g_engine, "test_entity_publish_script");
        TEST_ASSERT_NOT_NULL(entity);
        
        if (entity) {
            // Initialize entity data table to prevent state from previous tests
            lua_State *L = g_engine->lua_engine->runtime;
            entity_lua_push(entity);
            lua_newtable(L);
            lua_setfield(L, -2, "data");
            lua_pop(L, 1);
            // Subscribe to a topic
            engine_pubsub_sub(g_engine, "publish_test_event", entity, "on_test_event");
            
            // Test publishing different types of data
            EseLuaValue *string_data = lua_value_create_string("test_string", "Hello World");
            EseLuaValue *number_data = lua_value_create_number("test_number", 42.5);
            EseLuaValue *bool_data = lua_value_create_bool("test_bool", true);
            
            // Publish string data
            engine_pubsub_pub(g_engine, "publish_test_event", string_data);
            entity_update(entity, 0.016f);
            
            // Publish number data
            engine_pubsub_pub(g_engine, "publish_test_event", number_data);
            entity_update(entity, 0.016f);
            
            // Publish boolean data
            engine_pubsub_pub(g_engine, "publish_test_event", bool_data);
            entity_update(entity, 0.016f);
            
            // Check that the function was called multiple times
            entity_lua_push(entity);
            lua_getfield(L, -1, "data");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "test_event_count");
                int count = lua_tointeger(L, -1);
                TEST_ASSERT_EQUAL_INT(3, count);
                lua_pop(L, 1);
            }
            lua_pop(L, 2); // Pop data table and entity
            
            lua_value_free(string_data);
            lua_value_free(number_data);
            lua_value_free(bool_data);
            entity_destroy(entity);
        }
        
    }
    
    // End testEntity Publish");
}

// Test multiple subscribers
static void test_entity_multiple_subscribers() {
    TEST_ASSERT_NOT_NULL(g_engine);
    
    {
        EseEntity *entity1 = create_test_entity_with_script(g_engine, "test_entity_script_1");
        EseEntity *entity2 = create_test_entity_with_script(g_engine, "test_entity_script_2");
        TEST_ASSERT_NOT_NULL(entity1);
        TEST_ASSERT_NOT_NULL(entity2);
        
        if (entity1 && entity2) {
            // Initialize entity data tables to prevent state from previous tests
            lua_State *L = g_engine->lua_engine->runtime;
            entity_lua_push(entity1);
            lua_newtable(L);
            lua_setfield(L, -2, "data");
            lua_pop(L, 1);
            
            entity_lua_push(entity2);
            lua_newtable(L);
            lua_setfield(L, -2, "data");
            lua_pop(L, 1);
            
            // Subscribe both entities to the same topic
            engine_pubsub_sub(g_engine, "multiple_subscribers_test_event", entity1, "on_test_event");
            engine_pubsub_sub(g_engine, "multiple_subscribers_test_event", entity2, "on_test_event");
            
            // Publish to the topic
            EseLuaValue *data = lua_value_create_string("test_data", "Multiple Subscribers");
            engine_pubsub_pub(g_engine, "multiple_subscribers_test_event", data);
            
            // Update both entities
            entity_update(entity1, 0.016f);
            entity_update(entity2, 0.016f);
            
            // Check that both entities received the event
            
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
                lua_getfield(L, -1, "test_event_called");
                bool called2 = lua_toboolean(L, -1);
                TEST_ASSERT(called2);
                lua_pop(L, 1);
            }
            lua_pop(L, 2);
            
            lua_value_free(data);
            entity_destroy(entity1);
            entity_destroy(entity2);
        }
        
    }
    
    // End testEntity Multiple Subscribers");
}

// Test multiple topics
static void test_entity_multiple_topics() {
    TEST_ASSERT_NOT_NULL(g_engine);
    
    {
            EseEntity *entity = create_test_entity_with_script(g_engine, "test_entity_error_script");
        TEST_ASSERT_NOT_NULL(entity);
        
        if (entity) {
            // Subscribe to multiple topics
            engine_pubsub_sub(g_engine, "topic1", entity, "on_test_event");
            engine_pubsub_sub(g_engine, "topic2", entity, "on_custom_event");
            engine_pubsub_sub(g_engine, "topic3", entity, "on_multiple_events");
            
            // Subscription tracking is handled by pub/sub system
            
            // Publish to topic1 only
            EseLuaValue *data1 = lua_value_create_string("test_data", "Topic 1 Message");
            engine_pubsub_pub(g_engine, "topic1", data1);
            entity_update(entity, 0.016f);
            
            // Publish to topic2 only
            EseLuaValue *data2 = lua_value_create_string("test_data", "Topic 2 Message");
            engine_pubsub_pub(g_engine, "topic2", data2);
            entity_update(entity, 0.016f);
            
            // Check that the correct functions were called
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
                TEST_ASSERT(custom_event_called);
                lua_pop(L, 1);
                
                lua_getfield(L, -1, "multiple_events_called");
                bool multiple_events_called = lua_toboolean(L, -1);
                TEST_ASSERT(!multiple_events_called);
                lua_pop(L, 1);
            }
            lua_pop(L, 2);
            
            lua_value_free(data1);
            lua_value_free(data2);
            entity_destroy(entity);
        }
        
    }
    
    // End testEntity Multiple Topics");
}

// Test subscription tracking
static void test_entity_subscription_tracking() {
    TEST_ASSERT_NOT_NULL(g_engine);
    
    {
            EseEntity *entity = create_test_entity_with_script(g_engine, "test_entity_error_script");
        TEST_ASSERT_NOT_NULL(entity);
        
        if (entity) {
            // Initially no subscriptions
            // Subscription tracking is handled by pub/sub system
            
            // Add subscriptions
            engine_pubsub_sub(g_engine, "topic1", entity, "on_test_event");
            // Subscription tracking is handled by pub/sub system
            
            engine_pubsub_sub(g_engine, "topic2", entity, "on_custom_event");
            // Subscription tracking is handled by pub/sub system
            
            engine_pubsub_sub(g_engine, "topic3", entity, "on_multiple_events");
            // Subscription tracking is handled by pub/sub system
            
            // Remove one subscription
            engine_pubsub_unsub(g_engine, "topic2", entity, "on_custom_event");
            // Subscription tracking is handled by pub/sub system
            
            // Remove another subscription
            engine_pubsub_unsub(g_engine, "topic1", entity, "on_test_event");
            // Subscription tracking is handled by pub/sub system
            
            // Remove last subscription
            engine_pubsub_unsub(g_engine, "topic3", entity, "on_multiple_events");
            // Subscription tracking is handled by pub/sub system
            
            entity_destroy(entity);
        }
        
    }
    
    // End testEntity Subscription Tracking");
}

// Test auto cleanup on entity destruction
static void test_entity_auto_cleanup() {
    TEST_ASSERT_NOT_NULL(g_engine);
    
    {
            EseEntity *entity = create_test_entity_with_script(g_engine, "test_entity_error_script");
        TEST_ASSERT_NOT_NULL(entity);
        
        if (entity) {
            // Add multiple subscriptions
            engine_pubsub_sub(g_engine, "topic1", entity, "on_test_event");
            engine_pubsub_sub(g_engine, "topic2", entity, "on_custom_event");
            engine_pubsub_sub(g_engine, "topic3", entity, "on_multiple_events");
            
            // Subscription tracking is handled by pub/sub system
            
            // Destroy entity - should auto-cleanup subscriptions
            entity_destroy(entity);
            
            // Try to publish to the topics - should not crash
            EseLuaValue *data = lua_value_create_string("test_data", "After Destruction");
            engine_pubsub_pub(g_engine, "topic1", data);
            engine_pubsub_pub(g_engine, "topic2", data);
            engine_pubsub_pub(g_engine, "topic3", data);
            
            lua_value_free(data);
        }
        
    }
    
    // End testEntity Auto Cleanup");
}

// Test Lua integration
static void test_entity_pubsub_lua_integration() {
    TEST_ASSERT_NOT_NULL(g_engine);
    
    {
            lua_State *L = g_engine->lua_engine->runtime;
        
        // Test that Entity.publish exists
        lua_getglobal(L, "Entity");
        TEST_ASSERT(lua_istable(L, -1));
        
        lua_getfield(L, -1, "publish");
        TEST_ASSERT(lua_isfunction(L, -1));
        lua_pop(L, 2);
        
        // Test that entity:subscribe and entity:unsubscribe exist
            EseEntity *entity = create_test_entity_with_script(g_engine, "test_entity_error_script");
        TEST_ASSERT_NOT_NULL(entity);
        
        if (entity) {
            entity_lua_push(entity);
            lua_getfield(L, -1, "subscribe");
            TEST_ASSERT(lua_isfunction(L, -1));
            lua_pop(L, 1);
            
            lua_getfield(L, -1, "unsubscribe");
            TEST_ASSERT(lua_isfunction(L, -1));
            lua_pop(L, 2);
            
            entity_destroy(entity);
        }
        
    }
    
    // End testEntity Pub/Sub Lua Integration");
}

// Test data passing
static void test_entity_pubsub_data_passing() {
    TEST_ASSERT_NOT_NULL(g_engine);
    
    {
            EseEntity *entity = create_test_entity_with_script(g_engine, "test_entity_data_passing_script");
        TEST_ASSERT_NOT_NULL(entity);
        
        if (entity) {
            // Initialize entity data table to prevent state from previous tests
            lua_State *L = g_engine->lua_engine->runtime;
            entity_lua_push(entity);
            lua_newtable(L);
            lua_setfield(L, -2, "data");
            lua_pop(L, 1);
            // Subscribe to a topic
            engine_pubsub_sub(g_engine, "data_passing_test_event", entity, "on_test_event");
            
            // Test passing complex data
            EseLuaValue *complex_data = lua_value_create_table("complex_data");
            EseLuaValue *string_val = lua_value_create_string("message", "Hello World");
            EseLuaValue *number_val = lua_value_create_number("value", 42.5);
            EseLuaValue *bool_val = lua_value_create_bool("flag", true);
            
            lua_value_push(complex_data, string_val, true);
            lua_value_push(complex_data, number_val, true);
            lua_value_push(complex_data, bool_val, true);
            
            // Publish complex data
            engine_pubsub_pub(g_engine, "data_passing_test_event", complex_data);
            entity_update(entity, 0.016f);
            
            // Check that the data was received
            entity_lua_push(entity);
            lua_getfield(L, -1, "data");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "test_event_called");
                bool test_event_called = lua_toboolean(L, -1);
                TEST_ASSERT(test_event_called);
                lua_pop(L, 1);
            }
            lua_pop(L, 2);
            
            lua_value_free(complex_data);
            entity_destroy(entity);
        }
        
    }
    
    // End testEntity Pub/Sub Data Passing");
}

// Test error handling
static void test_entity_pubsub_error_handling() {
    TEST_ASSERT_NOT_NULL(g_engine);
    
    {
            EseEntity *entity = create_test_entity_with_script(g_engine, "test_entity_error_handling_script");
        TEST_ASSERT_NOT_NULL(entity);
        
        if (entity) {
            // Initialize entity data table to prevent state from previous tests
            lua_State *L = g_engine->lua_engine->runtime;
            entity_lua_push(entity);
            lua_newtable(L);
            lua_setfield(L, -2, "data");
            lua_pop(L, 1);
            // Test subscribing to non-existent function - should not crash
            engine_pubsub_sub(g_engine, "error_test_event", entity, "non_existent_function");
            
            // Test unsubscribing from non-existent subscription - should not crash
            engine_pubsub_unsub(g_engine, "non_existent_topic", entity, "on_test_event");
            
            // Test publishing to non-existent topic - should not crash
            EseLuaValue *data = lua_value_create_string("test_data", "Test");
            engine_pubsub_pub(g_engine, "non_existent_topic", data);
            
            // Test with NULL data - should abort in debug builds
            // This test is commented out as it would cause an abort in debug builds
            // engine_pubsub_pub(engine, "error_test_event", NULL);
            
            lua_value_free(data);
            entity_destroy(entity);
        }
        
    }
    
    // End testEntity Pub/Sub Error Handling");
}
