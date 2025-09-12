/*
 * test_engine.c - Unity-based tests for engine functionality
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
#include "../src/entity/entity_lua.h"
#include "../src/entity/entity_private.h"
#include "../src/entity/components/entity_component.h"
#include "../src/entity/components/entity_component_collider.h"
#include "../src/scripting/lua_engine.h"
#include "../src/scripting/lua_value.h"
#include "../src/core/memory_manager.h"
#include "../src/core/engine.h"
#include "../src/core/engine_private.h"
#include "../src/utility/log.h"
#include "../src/types/rect.h"
#include "../src/types/point.h"
#include "../src/types/input_state.h"
#include "../src/types/input_state_private.h"
#include "../src/types/uuid.h"
#include "../src/core/console.h"
#include "../src/platform/renderer.h"

/**
 * Test function declarations
 */
static void test_engine_creation(void);
static void test_engine_destroy(void);
static void test_engine_add_entity(void);
static void test_engine_remove_entity(void);
static void test_engine_clear_entities(void);
static void test_engine_start(void);
static void test_engine_update(void);
static void test_engine_detect_collision_rect(void);
static void test_engine_get_sprite(void);
static void test_engine_find_by_tag(void);
static void test_engine_find_by_id(void);
static void test_engine_get_entity_count(void);
static void test_engine_console_functions(void);
static void test_engine_null_pointer_handling(void);
static void test_engine_edge_cases(void);

/**
 * Test suite setup and teardown
 */
static EseEngine *g_engine = NULL;

void setUp(void) {
    g_engine = NULL;
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

    printf("\nEseEngine Tests\n");
    printf("---------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_engine_creation);
    RUN_TEST(test_engine_destroy);
    RUN_TEST(test_engine_add_entity);
    // RUN_TEST(test_engine_remove_entity);
    // RUN_TEST(test_engine_clear_entities);
    RUN_TEST(test_engine_start);
    RUN_TEST(test_engine_get_entity_count);
    RUN_TEST(test_engine_console_functions);
    RUN_TEST(test_engine_update);
    RUN_TEST(test_engine_detect_collision_rect);
    RUN_TEST(test_engine_get_sprite);
    RUN_TEST(test_engine_find_by_tag);
    RUN_TEST(test_engine_find_by_id);
    // RUN_TEST(test_engine_null_pointer_handling);
    RUN_TEST(test_engine_edge_cases);

    return UNITY_END();
}

/**
 * Test Functions
 */

static void test_engine_creation(void) {
    // Test basic engine creation
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    if (!g_engine) {
        return;
    }
    
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine->lua_engine, "Lua engine should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine->entities, "Entities list should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine->del_entities, "Deletion entities list should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine->collision_bin, "Collision bin should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine->console, "Console should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine->draw_list, "Draw list should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine->render_list_a, "Render list A should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine->render_list_b, "Render list B should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine->input_state, "Input state should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine->display_state, "Display state should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine->camera_state, "Camera state should be created");
    
    // Test initial state
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, engine_get_entity_count(g_engine), "New engine should have 0 entities");
    TEST_ASSERT_FALSE_MESSAGE(g_engine->isRunning, "New engine should not be running");
    TEST_ASSERT_FALSE_MESSAGE(g_engine->draw_console, "New engine should not draw console by default");
    TEST_ASSERT_TRUE_MESSAGE(g_engine->active_render_list, "New engine should have active render list set to true");
}

static void test_engine_destroy(void) {
    // Test engine destruction
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    // Add some entities to test cleanup
    EseLuaEngine *lua_engine = g_engine->lua_engine;
    EseEntity *entity1 = entity_create(lua_engine);
    EseEntity *entity2 = entity_create(lua_engine);
    engine_add_entity(g_engine, entity1);
    engine_add_entity(g_engine, entity2);
    
    // Destroy engine - this should clean up all resources
    engine_destroy(g_engine);
    g_engine = NULL; // Prevent double-free in tearDown
}

static void test_engine_add_entity(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    EseLuaEngine *lua_engine = g_engine->lua_engine;
    TEST_ASSERT_NOT_NULL_MESSAGE(lua_engine, "Lua engine should be created");
    
    // Test adding entities
    EseEntity *entity1 = entity_create(lua_engine);
    EseEntity *entity2 = entity_create(lua_engine);
    
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, engine_get_entity_count(g_engine), "Engine should start with 0 entities");
    
    engine_add_entity(g_engine, entity1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, engine_get_entity_count(g_engine), "Engine should have 1 entity after adding");
    
    engine_add_entity(g_engine, entity2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, engine_get_entity_count(g_engine), "Engine should have 2 entities after adding");
}

static void test_engine_remove_entity(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    EseLuaEngine *lua_engine = g_engine->lua_engine;
    TEST_ASSERT_NOT_NULL_MESSAGE(lua_engine, "Lua engine should be created");
    
    EseEntity *entity1 = entity_create(lua_engine);
    EseEntity *entity2 = entity_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(entity1, "Entity1 should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(entity2, "Entity2 should be created");
    
    // Add entities
    engine_add_entity(g_engine, entity1);
    engine_add_entity(g_engine, entity2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, engine_get_entity_count(g_engine), "Engine should have 2 entities");
    
    // Remove one entity
    engine_remove_entity(g_engine, entity1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, engine_get_entity_count(g_engine), "Entity count should remain 2 until update");
    
    // The entity should be in the deletion list
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, dlist_size(g_engine->del_entities), "Deletion list should have 1 entity");
}

static void test_engine_clear_entities(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    EseLuaEngine *lua_engine = g_engine->lua_engine;
    
    // Test 1: Clear empty engine
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, engine_get_entity_count(g_engine), "Empty engine should have 0 entities");
    engine_clear_entities(g_engine, false);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, engine_get_entity_count(g_engine), "Empty engine should still have 0 entities after clear");
    engine_clear_entities(g_engine, true);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, engine_get_entity_count(g_engine), "Empty engine should still have 0 entities after clear with persistent=true");
    
    // Test 2: Clear non-persistent entities
    EseEntity *entity1 = entity_create(lua_engine);
    EseEntity *entity2 = entity_create(lua_engine);
    engine_add_entity(g_engine, entity1);
    engine_add_entity(g_engine, entity2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, engine_get_entity_count(g_engine), "Engine should have 2 entities");
    engine_clear_entities(g_engine, false);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, engine_get_entity_count(g_engine), "Engine should have 0 entities after clearing non-persistent");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, dlist_size(g_engine->del_entities), "Deletion list should have 2 entities");
    // Don't manually clear - let the engine handle it
    
    // Test 3: Clear persistent entities (should preserve them when include_persistent=false)
    EseEntity *entity3 = entity_create(lua_engine);
    entity_set_persistent(entity3, true);
    EseEntity *entity4 = entity_create(lua_engine);
    entity_set_persistent(entity4, true);
    engine_add_entity(g_engine, entity3);
    engine_add_entity(g_engine, entity4);
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, engine_get_entity_count(g_engine), "Engine should have 2 entities");
    engine_clear_entities(g_engine, false);
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, engine_get_entity_count(g_engine), "Engine should still have 2 entities after clearing non-persistent (persistent preserved)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, dlist_size(g_engine->del_entities), "Deletion list should have 0 entities");
    
    // Test 4: Clear all entities (include_persistent=true)
    engine_clear_entities(g_engine, true);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, engine_get_entity_count(g_engine), "Engine should have 0 entities after clearing all");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, dlist_size(g_engine->del_entities), "Deletion list should have 2 entities");
    // Don't manually clear - let the engine handle it
    
    // Test 5: Mixed entities
    EseEntity *entity5 = entity_create(lua_engine);
    EseEntity *entity6 = entity_create(lua_engine);
    entity_set_persistent(entity6, true);
    EseEntity *entity7 = entity_create(lua_engine);
    engine_add_entity(g_engine, entity5);
    engine_add_entity(g_engine, entity6);
    engine_add_entity(g_engine, entity7);
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, engine_get_entity_count(g_engine), "Engine should have 3 entities");
    
    engine_clear_entities(g_engine, false); // Should clear 2 non-persistent, preserve 1 persistent
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, engine_get_entity_count(g_engine), "Engine should have 1 persistent entity remaining");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, dlist_size(g_engine->del_entities), "Deletion list should have 2 non-persistent entities");
    dlist_clear(g_engine->del_entities);
    
    engine_clear_entities(g_engine, true); // Should clear the remaining 1 persistent entity
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, engine_get_entity_count(g_engine), "Engine should have 0 entities after clearing all");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, dlist_size(g_engine->del_entities), "Deletion list should have 1 persistent entity");
    dlist_clear(g_engine->del_entities);
}

static void test_engine_start(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    // Test initial state
    TEST_ASSERT_FALSE_MESSAGE(g_engine->isRunning, "Engine should not be running initially");
    
    // Start the engine
    engine_start(g_engine);
    
    // Test that engine is now running
    TEST_ASSERT_TRUE_MESSAGE(g_engine->isRunning, "Engine should be running after start");
}

static void test_engine_update(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    // Create test input state
    EseInputState test_input;
    memset(&test_input, 0, sizeof(EseInputState));
    test_input.mouse_x = 100;
    test_input.mouse_y = 200;
    test_input.keys_down[InputKey_A] = true;
    test_input.keys_pressed[InputKey_B] = true;
    
    // Test engine update
    engine_update(g_engine, 0.016f, &test_input);
    
    // Verify input state was updated
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, g_engine->input_state->mouse_x, "Mouse X should be updated");
    TEST_ASSERT_EQUAL_INT_MESSAGE(200, g_engine->input_state->mouse_y, "Mouse Y should be updated");
    TEST_ASSERT_TRUE_MESSAGE(g_engine->input_state->keys_down[InputKey_A], "Key down state should be updated");
    TEST_ASSERT_TRUE_MESSAGE(g_engine->input_state->keys_pressed[InputKey_B], "Key pressed state should be updated");
}

static void test_engine_detect_collision_rect(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    EseLuaEngine *lua_engine = g_engine->lua_engine;
    
    // Create test rectangle
    EseRect *test_rect = ese_rect_create(lua_engine);
    ese_rect_set_x(test_rect, 0);
    ese_rect_set_y(test_rect, 0);
    ese_rect_set_width(test_rect, 100);
    ese_rect_set_height(test_rect, 100);
    
    // Test with no entities
    EseEntity **results = engine_detect_collision_rect(g_engine, test_rect, 10);
    TEST_ASSERT_NOT_NULL_MESSAGE(results, "Results array should be allocated");
    TEST_ASSERT_NULL_MESSAGE(results[0], "First result should be NULL with no entities");
    memory_manager.free(results);
    
    // Add an entity with a collider
    EseEntity *entity = entity_create(lua_engine);
    EseEntityComponent *collider = entity_component_collider_create(lua_engine);
    entity_component_add(entity, collider);
    engine_add_entity(g_engine, entity);
    
    // Test collision detection
    results = engine_detect_collision_rect(g_engine, test_rect, 10);
    TEST_ASSERT_NOT_NULL_MESSAGE(results, "Results array should be allocated");
    memory_manager.free(results);
    
    ese_rect_destroy(test_rect);
}

static void test_engine_get_sprite(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    // Test getting sprite when no asset manager - this will assert, so we skip this test
    // The function asserts on NULL asset manager, which is expected behavior
    // We can't test this without setting up a renderer first
}

static void test_engine_find_by_tag(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    EseLuaEngine *lua_engine = g_engine->lua_engine;
    
    // Test with no entities
    EseEntity **results = engine_find_by_tag(g_engine, "test", 10);
    TEST_ASSERT_NOT_NULL_MESSAGE(results, "Results array should be allocated");
    TEST_ASSERT_NULL_MESSAGE(results[0], "First result should be NULL with no entities");
    memory_manager.free(results);
    
    // Add entities with tags
    EseEntity *entity1 = entity_create(lua_engine);
    EseEntity *entity2 = entity_create(lua_engine);
    entity_add_tag(entity1, "test");
    entity_add_tag(entity2, "other");
    engine_add_entity(g_engine, entity1);
    engine_add_entity(g_engine, entity2);
    
    // Test finding by tag
    results = engine_find_by_tag(g_engine, "test", 10);
    TEST_ASSERT_NOT_NULL_MESSAGE(results, "Results array should be allocated");
    TEST_ASSERT_NOT_NULL_MESSAGE(results[0], "Should find entity with tag");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(entity1, results[0], "Should find correct entity");
    TEST_ASSERT_NULL_MESSAGE(results[1], "Should be NULL terminated");
    memory_manager.free(results);
    
    // Test case insensitive search
    results = engine_find_by_tag(g_engine, "TEST", 10);
    TEST_ASSERT_NOT_NULL_MESSAGE(results, "Results array should be allocated");
    TEST_ASSERT_NOT_NULL_MESSAGE(results[0], "Should find entity with uppercase tag");
    memory_manager.free(results);
}

static void test_engine_find_by_id(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    EseLuaEngine *lua_engine = g_engine->lua_engine;
    
    // Test with no entities
    EseEntity *result = engine_find_by_id(g_engine, "nonexistent");
    TEST_ASSERT_NULL_MESSAGE(result, "Should return NULL for nonexistent ID");
    
    // Add an entity
    EseEntity *entity = entity_create(lua_engine);
    engine_add_entity(g_engine, entity);
    
    // Get the entity's ID
    const char *entity_id = ese_uuid_get_value(entity->id);
    TEST_ASSERT_NOT_NULL_MESSAGE(entity_id, "Entity should have an ID");
    
    // Test finding by ID
    result = engine_find_by_id(g_engine, entity_id);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(entity, result, "Should find entity by ID");
    
    // Test with wrong ID
    result = engine_find_by_id(g_engine, "wrong-id");
    TEST_ASSERT_NULL_MESSAGE(result, "Should return NULL for wrong ID");
}

static void test_engine_get_entity_count(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    EseLuaEngine *lua_engine = g_engine->lua_engine;
    
    // Test initial count
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, engine_get_entity_count(g_engine), "Should start with 0 entities");
    
    // Add entities and test count
    EseEntity *entity1 = entity_create(lua_engine);
    EseEntity *entity2 = entity_create(lua_engine);
    
    engine_add_entity(g_engine, entity1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, engine_get_entity_count(g_engine), "Should have 1 entity after adding");
    
    engine_add_entity(g_engine, entity2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, engine_get_entity_count(g_engine), "Should have 2 entities after adding");
}

static void test_engine_console_functions(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    // Test initial console state
    TEST_ASSERT_FALSE_MESSAGE(g_engine->draw_console, "Console should not be drawn initially");
    
    // Test showing console
    engine_show_console(g_engine, true);
    TEST_ASSERT_TRUE_MESSAGE(g_engine->draw_console, "Console should be shown after setting true");
    
    // Test hiding console
    engine_show_console(g_engine, false);
    TEST_ASSERT_FALSE_MESSAGE(g_engine->draw_console, "Console should be hidden after setting false");
    
    // Test adding to console
    engine_add_to_console(g_engine, ESE_CONSOLE_NORMAL, "TEST", "Test message");
    // We can't easily test the console content without exposing internal functions
    // but we can test that the function doesn't crash
}

static void test_engine_null_pointer_handling(void) {
    // Test that functions assert on NULL engine
    ASSERT_DEATH((engine_destroy(NULL)), "engine_destroy should assert on NULL engine");
    ASSERT_DEATH((engine_add_entity(NULL, NULL)), "engine_add_entity should assert on NULL engine");
    ASSERT_DEATH((engine_remove_entity(NULL, NULL)), "engine_remove_entity should assert on NULL engine");
    ASSERT_DEATH((engine_clear_entities(NULL, false)), "engine_clear_entities should assert on NULL engine");
    ASSERT_DEATH((engine_start(NULL)), "engine_start should assert on NULL engine");
    ASSERT_DEATH((engine_update(NULL, 0.0f, NULL)), "engine_update should assert on NULL engine");
    ASSERT_DEATH((engine_detect_collision_rect(NULL, NULL, 0)), "engine_detect_collision_rect should assert on NULL engine");
    ASSERT_DEATH((engine_get_sprite(NULL, NULL)), "engine_get_sprite should assert on NULL engine");
    ASSERT_DEATH((engine_find_by_tag(NULL, NULL, 0)), "engine_find_by_tag should assert on NULL engine");
    ASSERT_DEATH((engine_find_by_id(NULL, NULL)), "engine_find_by_id should assert on NULL engine");
    ASSERT_DEATH((engine_get_entity_count(NULL)), "engine_get_entity_count should assert on NULL engine");
    ASSERT_DEATH((engine_add_to_console(NULL, ESE_CONSOLE_NORMAL, NULL, NULL)), "engine_add_to_console should assert on NULL engine");
    ASSERT_DEATH((engine_show_console(NULL, false)), "engine_show_console should assert on NULL engine");
}

static void test_engine_edge_cases(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    EseLuaEngine *lua_engine = g_engine->lua_engine;
    
    // Test collision detection with max_count = 0
    EseRect *test_rect = ese_rect_create(lua_engine);
    EseEntity **results = engine_detect_collision_rect(g_engine, test_rect, 0);
    TEST_ASSERT_NOT_NULL_MESSAGE(results, "Results array should be allocated even with max_count 0");
    TEST_ASSERT_NULL_MESSAGE(results[0], "First result should be NULL");
    memory_manager.free(results);
    
    // Test find_by_tag with max_count = 0
    results = engine_find_by_tag(g_engine, "test", 0);
    TEST_ASSERT_NOT_NULL_MESSAGE(results, "Results array should be allocated even with max_count 0");
    TEST_ASSERT_NULL_MESSAGE(results[0], "First result should be NULL");
    memory_manager.free(results);
    
    // Test find_by_tag with empty tag
    results = engine_find_by_tag(g_engine, "", 10);
    TEST_ASSERT_NOT_NULL_MESSAGE(results, "Results array should be allocated");
    TEST_ASSERT_NULL_MESSAGE(results[0], "Should not find entities with empty tag");
    memory_manager.free(results);
    
    // Test find_by_id with empty string
    EseEntity *result = engine_find_by_id(g_engine, "");
    TEST_ASSERT_NULL_MESSAGE(result, "Should return NULL for empty ID");
    
    // Test update with zero delta time
    EseInputState test_input;
    memset(&test_input, 0, sizeof(EseInputState));
    engine_update(g_engine, 0.0f, &test_input);
    // Should not crash
    
    // Test update with negative delta time
    engine_update(g_engine, -1.0f, &test_input);
    // Should not crash
    
    ese_rect_destroy(test_rect);
}
