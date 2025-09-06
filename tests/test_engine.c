/*
 * Test file for engine functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <execinfo.h>
#include <signal.h>
#include "test_utils.h"
#include "../src/entity/entity.h"
#include "../src/entity/entity_lua.h"
#include "../src/scripting/lua_engine.h"
#include "../src/scripting/lua_value.h"
#include "../src/core/memory_manager.h"
#include "../src/core/engine.h"
#include "../src/core/engine_private.h"
#include "../src/utility/log.h"

// Test function declarations
static void test_engine_creation();
static void test_engine_clear_entities();

// Signal handler for segfaults
static void segfault_handler(int sig, siginfo_t *info, void *context) {
    printf("---- BACKTRACE START ----\n");
    void *array[10];
    size_t size = backtrace(array, 10);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    printf("---- BACKTRACE  END  ----\n");
    exit(1);
}

int main() {
    // Register signal handler for segfaults
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = segfault_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);

    test_suite_begin("Engine Tests");

    // Run all test suites
    test_engine_creation();
    test_engine_clear_entities();

    test_suite_end("Engine Tests");

    return 0;
}

// Test engine_creation functionality
static void test_engine_creation() {
    test_begin("Engine Creation");

    EseEngine *engine = engine_create("dummy_startup.lua");
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");
    TEST_ASSERT_NOT_NULL(engine->lua_engine, "Lua engine should be created");
    TEST_ASSERT_NOT_NULL(engine->entities, "Entities list should be created");
    TEST_ASSERT_NOT_NULL(engine->del_entities, "Deletion entities list should be created");
    TEST_ASSERT_NOT_NULL(engine->collision_bin, "Collision bin should be created");

    engine_destroy(engine);
    test_end("Engine Creation");
}

// Test engine_clear_entities functionality
static void test_engine_clear_entities() {
    test_begin("Engine Entities Clear");

    EseEngine *engine = engine_create("dummy_startup.lua");
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");

    EseLuaEngine *lua_engine = engine->lua_engine;
    TEST_ASSERT_NOT_NULL(lua_engine, "Lua engine should be created");

    // Test 1: Clear empty engine
    TEST_ASSERT(engine_get_entity_count(engine) == 0, "Empty engine should have 0 entities");
    engine_clear_entities(engine, false);
    TEST_ASSERT(engine_get_entity_count(engine) == 0, "Empty engine should still have 0 entities after clear");
    engine_clear_entities(engine, true);
    TEST_ASSERT(engine_get_entity_count(engine) == 0, "Empty engine should still have 0 entities after clear with persistent=true");

    // Test 2: Clear non-persistent entities
    EseEntity *entity1 = entity_create(lua_engine);
    EseEntity *entity2 = entity_create(lua_engine);
    engine_add_entity(engine, entity1);
    engine_add_entity(engine, entity2);
    TEST_ASSERT(engine_get_entity_count(engine) == 2, "Engine should have 2 entities");
    engine_clear_entities(engine, false);
    TEST_ASSERT(engine_get_entity_count(engine) == 0, "Engine should have 0 entities after clearing non-persistent");
    TEST_ASSERT(dlist_size(engine->del_entities) == 2, "Deletion list should have 2 entities");
    dlist_clear(engine->del_entities); // Clear for next test

    // Test 3: Clear persistent entities (should preserve them when include_persistent=false)
    EseEntity *entity3 = entity_create(lua_engine);
    entity_set_persistent(entity3, true);
    EseEntity *entity4 = entity_create(lua_engine);
    entity_set_persistent(entity4, true);
    engine_add_entity(engine, entity3);
    engine_add_entity(engine, entity4);
    TEST_ASSERT(engine_get_entity_count(engine) == 2, "Engine should have 2 entities");
    engine_clear_entities(engine, false);
    TEST_ASSERT(engine_get_entity_count(engine) == 2, "Engine should still have 2 entities after clearing non-persistent (persistent preserved)");
    TEST_ASSERT(dlist_size(engine->del_entities) == 0, "Deletion list should have 0 entities");

    // Test 4: Clear all entities (include_persistent=true)
    engine_clear_entities(engine, true);
    TEST_ASSERT(engine_get_entity_count(engine) == 0, "Engine should have 0 entities after clearing all");
    TEST_ASSERT(dlist_size(engine->del_entities) == 2, "Deletion list should have 2 entities");
    dlist_clear(engine->del_entities); // Clear for next test

    // Test 5: Mixed entities
    EseEntity *entity5 = entity_create(lua_engine);
    EseEntity *entity6 = entity_create(lua_engine);
    entity_set_persistent(entity6, true);
    EseEntity *entity7 = entity_create(lua_engine);
    engine_add_entity(engine, entity5);
    engine_add_entity(engine, entity6);
    engine_add_entity(engine, entity7);
    TEST_ASSERT(engine_get_entity_count(engine) == 3, "Engine should have 3 entities");

    engine_clear_entities(engine, false); // Should clear 2 non-persistent, preserve 1 persistent
    TEST_ASSERT(engine_get_entity_count(engine) == 1, "Engine should have 1 persistent entity remaining");
    TEST_ASSERT(dlist_size(engine->del_entities) == 2, "Deletion list should have 2 non-persistent entities");
    dlist_clear(engine->del_entities);

    engine_clear_entities(engine, true); // Should clear the remaining 1 persistent entity
    TEST_ASSERT(engine_get_entity_count(engine) == 0, "Engine should have 0 entities after clearing all");
    TEST_ASSERT(dlist_size(engine->del_entities) == 1, "Deletion list should have 1 persistent entity");
    dlist_clear(engine->del_entities);

    engine_destroy(engine);
    test_end("Engine Entities Clear");
}
