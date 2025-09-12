/*
 * test_engine_collision.c - Unity-based tests for engine collision detection functionality
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
#include "../src/entity/components/entity_component_lua.h"
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

/**
 * Test function declarations
 */
static void test_engine_collision_detection(void);
static void test_no_self_collision(void);
static void test_collision_enter_stay_exit(void);
static void test_collision_with_multiple_entities(void);
static void test_collision_edge_cases(void);

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

    printf("\nEseEngine Collision Tests\n");
    printf("-------------------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_engine_collision_detection);
    RUN_TEST(test_no_self_collision);
    RUN_TEST(test_collision_enter_stay_exit);
    RUN_TEST(test_collision_with_multiple_entities);
    RUN_TEST(test_collision_edge_cases);

    return UNITY_END();
}

/**
 * Test Functions
 */

static void test_engine_collision_detection(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    EseLuaEngine *lua_engine = g_engine->lua_engine;
    TEST_ASSERT_NOT_NULL_MESSAGE(lua_engine, "Lua engine should be created");
    
    const char *script = 
    "function ENTITY:entity_update(delta_time)\n"
    "end\n"
    "function ENTITY:entity_collision_enter(other)\n"
    "    self:add_tag('enter')\n"
    "end\n"
    "function ENTITY:entity_collision_stay(other)\n"
    "    self:add_tag('stay')\n"
    "end\n"
    "function ENTITY:entity_collision_exit(other)\n"
    "    self:add_tag('exit')\n"
    "end\n";
    
    bool load_result = lua_engine_load_script_from_string(lua_engine, script, "test_entity_script", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Test script should load successfully");
    
    if (load_result) {
        EseEntity *entity1 = entity_create(lua_engine);
        EseEntity *entity2 = entity_create(lua_engine);
        TEST_ASSERT_NOT_NULL_MESSAGE(entity1, "Entity1 should be created");
        TEST_ASSERT_NOT_NULL_MESSAGE(entity2, "Entity2 should be created");

        EseEntityComponent *lua_comp1 = entity_component_lua_create(lua_engine, "test_entity_script");
        EseEntityComponent *lua_comp2 = entity_component_lua_create(lua_engine, "test_entity_script");
        TEST_ASSERT_NOT_NULL_MESSAGE(lua_comp1, "Lua component 1 should be created");
        TEST_ASSERT_NOT_NULL_MESSAGE(lua_comp2, "Lua component 2 should be created");

        entity_component_add(entity1, lua_comp1);
        entity_component_add(entity2, lua_comp2);

        EseEntityComponent *collider1 = entity_component_collider_create(lua_engine);
        EseEntityComponent *collider2 = entity_component_collider_create(lua_engine);
        TEST_ASSERT_NOT_NULL_MESSAGE(collider1, "Collider 1 should be created");
        TEST_ASSERT_NOT_NULL_MESSAGE(collider2, "Collider 2 should be created");

        entity_component_add(entity1, collider1);
        entity_component_add(entity2, collider2);

        EseRect *rect1 = ese_rect_create(lua_engine);
        EseRect *rect2 = ese_rect_create(lua_engine);
        TEST_ASSERT_NOT_NULL_MESSAGE(rect1, "Rect 1 should be created");
        TEST_ASSERT_NOT_NULL_MESSAGE(rect2, "Rect 2 should be created");

        ese_rect_set_x(rect1, 0);
        ese_rect_set_y(rect1, 0);
        ese_rect_set_width(rect1, 100);
        ese_rect_set_height(rect1, 100);

        ese_rect_set_x(rect2, 0);
        ese_rect_set_y(rect2, 0);
        ese_rect_set_width(rect2, 100);
        ese_rect_set_height(rect2, 100);

        entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider1), rect1);
        entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider2), rect2);

        // Add entities to engine
        engine_add_entity(g_engine, entity1);
        engine_add_entity(g_engine, entity2);

        // Create input state for engine_update
        EseInputState *input_state = ese_input_state_create(lua_engine);
        TEST_ASSERT_NOT_NULL_MESSAGE(input_state, "Input state should be created");

        // Test 1: No collisions - entities far apart
        entity_set_position(entity1, 0, 0);
        entity_set_position(entity2, 300, 0);
        
        engine_update(g_engine, 0.016f, input_state);
        
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity should not have the enter tag after no collisions");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity should not have the enter tag after no collisions");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity1, "stay"), "Entity should not have the stay tag after no collisions");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity2, "stay"), "Entity should not have the stay tag after no collisions");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity1, "exit"), "Entity should not have the exit tag after no collisions");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity2, "exit"), "Entity should not have the exit tag after no collisions");

        // Test 2: Collision enter - entities overlapping
        entity_set_position(entity1, 150, 0);
        entity_set_position(entity2, 200, 0);
        
        engine_update(g_engine, 0.016f, input_state);
        
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity should have the enter tag after collision enter");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity should have the enter tag after collision enter");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity1, "stay"), "Entity should not have the stay tag after collision enter");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity2, "stay"), "Entity should not have the stay tag after collision enter");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity1, "exit"), "Entity should not have the exit tag after collision enter");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity2, "exit"), "Entity should not have the exit tag after collision enter");

        // Test 3: Collision stay - entities still overlapping
        entity_set_position(entity1, 200, 0);
        entity_set_position(entity2, 200, 0);
        
        engine_update(g_engine, 0.016f, input_state);
        
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity should have the enter tag after collision stay");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity should have the enter tag after collision stay");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "stay"), "Entity should have the stay tag after collision stay");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "stay"), "Entity should have the stay tag after collision stay");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity1, "exit"), "Entity should not have the exit tag after collision stay");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity2, "exit"), "Entity should not have the exit tag after collision stay");

        // Test 4: Collision exit - entities separated again
        entity_set_position(entity1, 301, 0);
        entity_set_position(entity2, 200, 0);
        
        engine_update(g_engine, 0.016f, input_state);
        
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity should have the enter tag after collision exit");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity should have the enter tag after collision exit");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "stay"), "Entity should have the stay tag after collision exit");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "stay"), "Entity should have the stay tag after collision exit");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "exit"), "Entity should have the exit tag after collision exit");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "exit"), "Entity should have the exit tag after collision exit");

        // Clean up
        ese_input_state_destroy(input_state);
    }
}

static void test_no_self_collision(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");

    EseLuaEngine *lua_engine = g_engine->lua_engine;
    TEST_ASSERT_NOT_NULL_MESSAGE(lua_engine, "Lua engine should be created");

    // Create a single entity with a collider and a simple collision handler script
    const char *script =
        "function ENTITY:entity_collision_enter(other) self:add_tag('enter') end\n"
        "function ENTITY:entity_collision_stay(other) self:add_tag('stay') end\n"
        "function ENTITY:entity_collision_exit(other) self:add_tag('exit') end\n";

    bool load_result = lua_engine_load_script_from_string(lua_engine, script, "self_collision_script", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Self-collision script should load successfully");

    EseEntity *entity = entity_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(entity, "Entity should be created");

    EseEntityComponent *lua_comp = entity_component_lua_create(lua_engine, "self_collision_script");
    TEST_ASSERT_NOT_NULL_MESSAGE(lua_comp, "Lua component should be created");
    entity_component_add(entity, lua_comp);

    EseEntityComponent *collider = entity_component_collider_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(collider, "Collider should be created");
    entity_component_add(entity, collider);

    // Add a single rect to the collider
    EseRect *rect = ese_rect_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(rect, "Rect should be created");
    ese_rect_set_x(rect, 0);
    ese_rect_set_y(rect, 0);
    ese_rect_set_width(rect, 64);
    ese_rect_set_height(rect, 64);
    entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider), rect);

    engine_add_entity(g_engine, entity);

    // Run an update and ensure no collision tags appear (self-pairs would create them)
    EseInputState *input_state = ese_input_state_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(input_state, "Input state should be created");

    entity_set_position(entity, 100, 100);
    engine_update(g_engine, 0.016f, input_state);

    TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity, "enter"), "Entity must not receive 'enter' from self-collision");
    TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity, "stay"), "Entity must not receive 'stay' from self-collision");
    TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity, "exit"), "Entity must not receive 'exit' from self-collision");

    // Clean up
    ese_input_state_destroy(input_state);
}

static void test_collision_enter_stay_exit(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    EseLuaEngine *lua_engine = g_engine->lua_engine;
    TEST_ASSERT_NOT_NULL_MESSAGE(lua_engine, "Lua engine should be created");
    
    const char *script = 
    "function ENTITY:entity_collision_enter(other)\n"
    "    self:add_tag('enter')\n"
    "end\n"
    "function ENTITY:entity_collision_stay(other)\n"
    "    self:add_tag('stay')\n"
    "end\n"
    "function ENTITY:entity_collision_exit(other)\n"
    "    self:add_tag('exit')\n"
    "end\n";
    
    bool load_result = lua_engine_load_script_from_string(lua_engine, script, "collision_script", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Collision script should load successfully");
    
    EseEntity *entity1 = entity_create(lua_engine);
    EseEntity *entity2 = entity_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(entity1, "Entity1 should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(entity2, "Entity2 should be created");

    EseEntityComponent *lua_comp1 = entity_component_lua_create(lua_engine, "collision_script");
    EseEntityComponent *lua_comp2 = entity_component_lua_create(lua_engine, "collision_script");
    TEST_ASSERT_NOT_NULL_MESSAGE(lua_comp1, "Lua component 1 should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(lua_comp2, "Lua component 2 should be created");

    entity_component_add(entity1, lua_comp1);
    entity_component_add(entity2, lua_comp2);

    EseEntityComponent *collider1 = entity_component_collider_create(lua_engine);
    EseEntityComponent *collider2 = entity_component_collider_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(collider1, "Collider 1 should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(collider2, "Collider 2 should be created");

    entity_component_add(entity1, collider1);
    entity_component_add(entity2, collider2);

    EseRect *rect1 = ese_rect_create(lua_engine);
    EseRect *rect2 = ese_rect_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(rect1, "Rect 1 should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(rect2, "Rect 2 should be created");

    ese_rect_set_x(rect1, 0);
    ese_rect_set_y(rect1, 0);
    ese_rect_set_width(rect1, 50);
    ese_rect_set_height(rect1, 50);

    ese_rect_set_x(rect2, 0);
    ese_rect_set_y(rect2, 0);
    ese_rect_set_width(rect2, 50);
    ese_rect_set_height(rect2, 50);

    entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider1), rect1);
    entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider2), rect2);

    engine_add_entity(g_engine, entity1);
    engine_add_entity(g_engine, entity2);

    EseInputState *input_state = ese_input_state_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(input_state, "Input state should be created");

    // Test collision enter
    entity_set_position(entity1, 0, 0);
    entity_set_position(entity2, 25, 0); // Overlapping
    
    engine_update(g_engine, 0.016f, input_state);
    
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity1 should have enter tag");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity2 should have enter tag");
    TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity1, "stay"), "Entity1 should not have stay tag yet");
    TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity2, "stay"), "Entity2 should not have stay tag yet");

    // Test collision stay
    engine_update(g_engine, 0.016f, input_state);
    
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity1 should still have enter tag");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity2 should still have enter tag");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "stay"), "Entity1 should have stay tag");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "stay"), "Entity2 should have stay tag");

    // Test collision exit
    entity_set_position(entity1, 100, 0); // Move apart
    
    engine_update(g_engine, 0.016f, input_state);
    
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "exit"), "Entity1 should have exit tag");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "exit"), "Entity2 should have exit tag");

    ese_input_state_destroy(input_state);
}

static void test_collision_with_multiple_entities(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    EseLuaEngine *lua_engine = g_engine->lua_engine;
    TEST_ASSERT_NOT_NULL_MESSAGE(lua_engine, "Lua engine should be created");
    
    const char *script = 
    "function ENTITY:entity_collision_enter(other)\n"
    "    self:add_tag('enter')\n"
    "end\n";
    
    bool load_result = lua_engine_load_script_from_string(lua_engine, script, "multi_collision_script", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Multi-collision script should load successfully");
    
    // Create 3 entities
    EseEntity *entities[3];
    for (int i = 0; i < 3; i++) {
        entities[i] = entity_create(lua_engine);
        TEST_ASSERT_NOT_NULL_MESSAGE(entities[i], "Entity should be created");
        
        EseEntityComponent *lua_comp = entity_component_lua_create(lua_engine, "multi_collision_script");
        TEST_ASSERT_NOT_NULL_MESSAGE(lua_comp, "Lua component should be created");
        entity_component_add(entities[i], lua_comp);
        
        EseEntityComponent *collider = entity_component_collider_create(lua_engine);
        TEST_ASSERT_NOT_NULL_MESSAGE(collider, "Collider should be created");
        entity_component_add(entities[i], collider);
        
        EseRect *rect = ese_rect_create(lua_engine);
        TEST_ASSERT_NOT_NULL_MESSAGE(rect, "Rect should be created");
        ese_rect_set_x(rect, 0);
        ese_rect_set_y(rect, 0);
        ese_rect_set_width(rect, 30);
        ese_rect_set_height(rect, 30);
        entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider), rect);
        
        engine_add_entity(g_engine, entities[i]);
    }

    EseInputState *input_state = ese_input_state_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(input_state, "Input state should be created");

    // Position entities so they all collide
    entity_set_position(entities[0], 0, 0);
    entity_set_position(entities[1], 15, 0); // Overlapping with entity 0
    entity_set_position(entities[2], 30, 0); // Overlapping with entity 1
    
    engine_update(g_engine, 0.016f, input_state);
    
    // All entities should have collision tags
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entities[0], "enter"), "Entity 0 should have enter tag");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entities[1], "enter"), "Entity 1 should have enter tag");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entities[2], "enter"), "Entity 2 should have enter tag");

    ese_input_state_destroy(input_state);
}

static void test_collision_edge_cases(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    EseLuaEngine *lua_engine = g_engine->lua_engine;
    TEST_ASSERT_NOT_NULL_MESSAGE(lua_engine, "Lua engine should be created");
    
    const char *script = 
    "function ENTITY:entity_collision_enter(other)\n"
    "    self:add_tag('enter')\n"
    "end\n";
    
    bool load_result = lua_engine_load_script_from_string(lua_engine, script, "edge_case_script", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Edge case script should load successfully");
    
    EseEntity *entity1 = entity_create(lua_engine);
    EseEntity *entity2 = entity_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(entity1, "Entity1 should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(entity2, "Entity2 should be created");

    EseEntityComponent *lua_comp1 = entity_component_lua_create(lua_engine, "edge_case_script");
    EseEntityComponent *lua_comp2 = entity_component_lua_create(lua_engine, "edge_case_script");
    TEST_ASSERT_NOT_NULL_MESSAGE(lua_comp1, "Lua component 1 should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(lua_comp2, "Lua component 2 should be created");

    entity_component_add(entity1, lua_comp1);
    entity_component_add(entity2, lua_comp2);

    EseEntityComponent *collider1 = entity_component_collider_create(lua_engine);
    EseEntityComponent *collider2 = entity_component_collider_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(collider1, "Collider 1 should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(collider2, "Collider 2 should be created");

    entity_component_add(entity1, collider1);
    entity_component_add(entity2, collider2);

    EseRect *rect1 = ese_rect_create(lua_engine);
    EseRect *rect2 = ese_rect_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(rect1, "Rect 1 should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(rect2, "Rect 2 should be created");

    // Test edge-to-edge collision (just touching)
    ese_rect_set_x(rect1, 0);
    ese_rect_set_y(rect1, 0);
    ese_rect_set_width(rect1, 10);
    ese_rect_set_height(rect1, 10);

    ese_rect_set_x(rect2, 0);
    ese_rect_set_y(rect2, 0);
    ese_rect_set_width(rect2, 10);
    ese_rect_set_height(rect2, 10);

    entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider1), rect1);
    entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider2), rect2);

    engine_add_entity(g_engine, entity1);
    engine_add_entity(g_engine, entity2);

    EseInputState *input_state = ese_input_state_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(input_state, "Input state should be created");

    // Test entities just touching (edge case) - overlapping slightly
    entity_set_position(entity1, 0, 0);
    entity_set_position(entity2, 5, 0); // Overlapping slightly
    
    engine_update(g_engine, 0.016f, input_state);
    
    // This should trigger collision (overlapping counts as collision)
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity1 should have enter tag for edge collision");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity2 should have enter tag for edge collision");

    // Test entities just separated (no collision)
    entity_set_position(entity1, 0, 0);
    entity_set_position(entity2, 11, 0); // Just separated
    
    // Clear tags first
    entity_remove_tag(entity1, "enter");
    entity_remove_tag(entity2, "enter");
    
    engine_update(g_engine, 0.016f, input_state);
    
    TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity1 should not have enter tag when separated");
    TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity2 should not have enter tag when separated");

    ese_input_state_destroy(input_state);
}