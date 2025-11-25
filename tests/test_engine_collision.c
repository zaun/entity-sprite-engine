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
#include "../src/entity/components/collider.h"
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
#include "../src/types/map.h"
#include "../src/entity/components/entity_component_map.h"

/**
 * Test function declarations
 */
static void test_engine_collision_detection(void);
static void test_no_self_collision(void);
static void test_collision_enter_stay_exit(void);
static void test_collision_with_multiple_entities(void);
static void test_collision_edge_cases(void);
static void test_collision_with_map(void);
static void test_entity_rotated_collision(void);
static void test_entity_offset_collision(void);
static void test_entity_mixed_collision(void);
static void test_entity_corner_cases(void);

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
    RUN_TEST(test_collision_with_map);
    RUN_TEST(test_entity_rotated_collision);
    RUN_TEST(test_entity_offset_collision);
    RUN_TEST(test_entity_mixed_collision);
    RUN_TEST(test_entity_corner_cases);

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

static void test_collision_with_map(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");

    EseLuaEngine *lua_engine = g_engine->lua_engine;
    TEST_ASSERT_NOT_NULL_MESSAGE(lua_engine, "Lua engine should be created");

    // For map collisions, engine dispatches map_collision_* handlers
    const char *script =
        "function ENTITY:map_collision_enter(other) self:add_tag('enter') end\n"
        "function ENTITY:map_collision_stay(other) self:add_tag('stay') end\n"
        "function ENTITY:map_collision_exit(other) self:add_tag('exit') end\n";

    TEST_ASSERT_TRUE_MESSAGE(lua_engine_load_script_from_string(lua_engine, script, "entity_map_collision", "ENTITY"),
        "Collision script should load successfully");

    // Create an entity with a simple collider rect
    EseEntity *entity = entity_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(entity, "Entity should be created");
    entity_component_add(entity, entity_component_lua_create(lua_engine, "entity_map_collision"));
    EseEntityComponent *collider = entity_component_collider_create(lua_engine);
    entity_component_add(entity, collider);
    // Enable collider-map interaction (C API)
    entity_component_collider_set_map_interaction((EseEntityComponentCollider*)entity_component_get_data(collider), true);
    EseRect *rect = ese_rect_create(lua_engine);
    ese_rect_set_x(rect, 0);
    ese_rect_set_y(rect, 0);
    ese_rect_set_width(rect, 32);
    ese_rect_set_height(rect, 32);
    entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider), rect);

    // Create a simple map and attach to the map component (all cells treated as collidable in resolver)
    EseEntityComponent *map_comp = entity_component_map_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(map_comp, "Map component should be created");
    EseEntityComponentMap *map_data = (EseEntityComponentMap *)entity_component_get_data(map_comp);
    TEST_ASSERT_NOT_NULL_MESSAGE(map_data, "Map component data should be valid");
    // Create a 10x1 grid map; mark cell (5,0) as solid
    EseMap *map = ese_map_create(lua_engine, 10, 1, MAP_TYPE_GRID, true);
    TEST_ASSERT_NOT_NULL_MESSAGE(map, "Map should be created");
    map_data->map = map;
    // Ensure layers visibility array is initialized for all layers
    // (normally set via Lua setter; here we mimic after assigning map directly)
    size_t layer_count = ese_map_get_layer_count(map);
    if (layer_count > 0) {
        map_data->show_layer = memory_manager.malloc(sizeof(bool) * layer_count, MMTAG_COMP_MAP);
        map_data->show_layer_count = layer_count;
        for (size_t i = 0; i < layer_count; i++) map_data->show_layer[i] = true;
    }
    // Mark a single cell solid so collisions only happen when overlapping it
    EseMapCell *solid_cell = ese_map_get_cell(map, 5, 0);
    TEST_ASSERT_NOT_NULL_MESSAGE(solid_cell, "Solid cell should be retrievable");
    ese_map_cell_set_flag(solid_cell, MAP_CELL_FLAG_SOLID);

    // Attach map component to a dedicated map entity so world bounds are tracked and updated
    EseEntity *map_entity = entity_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(map_entity, "Map entity should be created");
    entity_component_add(map_entity, map_comp);
    engine_add_entity(g_engine, map_entity);

    // Put entity and map into engine
    engine_add_entity(g_engine, entity);

    EseInputState *input_state = ese_input_state_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(input_state, "Input state should be created");

    // Position the map at origin and the entity so it doesn't collide first
    entity_set_position(map_entity, 0, 0);
    entity_set_position(entity, -50, -50); // starts far away from any map cells
    engine_update(g_engine, 0.016f, input_state);
    TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity, "enter"), "No map collision expected initially");

    // Move entity to overlap the cell (5,0) using the exact world rect from the map component
    EseRect *cell_rect = entity_component_map_get_cell_rect(map_data, 5, 0);
    TEST_ASSERT_NOT_NULL_MESSAGE(cell_rect, "Cell rect should be retrievable");
    float crx = ese_rect_get_x(cell_rect);
    float cry = ese_rect_get_y(cell_rect);
    float crw = ese_rect_get_width(cell_rect);
    float crh = ese_rect_get_height(cell_rect);
    ese_rect_destroy(cell_rect);
    // Place entity inside the cell bounds (entity collider rect is local at 0,0 with 32x32 size)
    entity_set_position(entity, crx + 1, cry + 1);
    engine_update(g_engine, 0.016f, input_state);
    // First overlap should emit ENTER
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity, "enter"), "Entity should get 'enter' on map collision");
    TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity, "exit"), "Entity should not have 'exit' on map collision overlap");

    // Stay collision again (tag remains or is re-added)
    engine_update(g_engine, 0.016f, input_state);
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity, "stay"), "Entity should get 'stay' while colliding with map");

    // Move entity past the cell to clear collision
    entity_set_position(entity, crx + crw + 1, cry + 1);
    engine_update(g_engine, 0.016f, input_state);
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity, "exit"), "Entity should get 'exit' after leaving map collision");

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

static void test_entity_rotated_collision(void) {
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
    
    bool load_result = lua_engine_load_script_from_string(lua_engine, script, "rotated_collision_script", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Rotated collision script should load successfully");
    
    EseEntity *entity1 = entity_create(lua_engine);
    EseEntity *entity2 = entity_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(entity1, "Entity1 should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(entity2, "Entity2 should be created");

    EseEntityComponent *lua_comp1 = entity_component_lua_create(lua_engine, "rotated_collision_script");
    EseEntityComponent *lua_comp2 = entity_component_lua_create(lua_engine, "rotated_collision_script");
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

    // Set up rectangles with size 40x40
    ese_rect_set_x(rect1, 0);
    ese_rect_set_y(rect1, 0);
    ese_rect_set_width(rect1, 40);
    ese_rect_set_height(rect1, 40);

    ese_rect_set_x(rect2, 0);
    ese_rect_set_y(rect2, 0);
    ese_rect_set_width(rect2, 40);
    ese_rect_set_height(rect2, 40);

    entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider1), rect1);
    entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider2), rect2);

    engine_add_entity(g_engine, entity1);
    engine_add_entity(g_engine, entity2);

    EseInputState *input_state = ese_input_state_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(input_state, "Input state should be created");

    // Test 45 degree rotation collision
    ese_rect_set_rotation(rect1, M_PI / 4.0f); // 45 degrees
    ese_rect_set_rotation(rect2, 0.0f); // No rotation
    
    // Position entities so their centers are close enough to potentially collide
    entity_set_position(entity1, 0, 0);   // Center at (20, 20)
    entity_set_position(entity2, 15, 15); // Center at (35, 35) - much closer for collision
    
    engine_update(g_engine, 0.016f, input_state);
    
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity1 should collide with 45° rotated rect");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity2 should collide with 45° rotated rect");

    // Test 135 degree rotation collision
    entity_remove_tag(entity1, "enter");
    entity_remove_tag(entity1, "stay");
    entity_remove_tag(entity1, "exit");
    entity_remove_tag(entity2, "enter");
    entity_remove_tag(entity2, "stay");
    entity_remove_tag(entity2, "exit");
    
    // Move entities apart to clear collision state
    entity_set_position(entity1, -100, -100);   // Far away
    entity_set_position(entity2, 100, 100);     // Far away
    
    engine_update(g_engine, 0.016f, input_state); // Update to clear collision state
    
    // Now set up 135° rotation test
    ese_rect_set_rotation(rect1, 3.0f * M_PI / 4.0f); // 135 degrees
    ese_rect_set_rotation(rect2, 0.0f); // No rotation
    
    entity_set_position(entity1, 0, 0);   // Center at (20, 20)
    entity_set_position(entity2, 10, 10); // Center at (30, 30) - closer for 135° rotation
    
    engine_update(g_engine, 0.016f, input_state);
    
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity1 should collide with 135° rotated rect");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity2 should collide with 135° rotated rect");

    // Test 225 degree rotation collision
    entity_remove_tag(entity1, "enter");
    entity_remove_tag(entity1, "stay");
    entity_remove_tag(entity1, "exit");
    entity_remove_tag(entity2, "enter");
    entity_remove_tag(entity2, "stay");
    entity_remove_tag(entity2, "exit");
    
    // Move entities apart to clear collision state
    entity_set_position(entity1, -100, -100);   // Far away
    entity_set_position(entity2, 100, 100);     // Far away
    
    engine_update(g_engine, 0.016f, input_state); // Update to clear collision state
    
    // Now set up 225° rotation test
    ese_rect_set_rotation(rect1, 5.0f * M_PI / 4.0f); // 225 degrees
    ese_rect_set_rotation(rect2, 0.0f); // No rotation
    
    entity_set_position(entity1, 0, 0);   // Center at (20, 20)
    entity_set_position(entity2, 10, 10); // Center at (30, 30) - closer for 225° rotation
    
    engine_update(g_engine, 0.016f, input_state);
    
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity1 should collide with 225° rotated rect");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity2 should collide with 225° rotated rect");

    // Test 315 degree rotation collision
    entity_remove_tag(entity1, "enter");
    entity_remove_tag(entity1, "stay");
    entity_remove_tag(entity1, "exit");
    entity_remove_tag(entity2, "enter");
    entity_remove_tag(entity2, "stay");
    entity_remove_tag(entity2, "exit");
    
    // Move entities apart to clear collision state
    entity_set_position(entity1, -100, -100);   // Far away
    entity_set_position(entity2, 100, 100);     // Far away
    
    engine_update(g_engine, 0.016f, input_state); // Update to clear collision state
    
    // Now set up 315° rotation test
    ese_rect_set_rotation(rect1, 7.0f * M_PI / 4.0f); // 315 degrees
    ese_rect_set_rotation(rect2, 0.0f); // No rotation
    
    entity_set_position(entity1, 0, 0);   // Center at (20, 20)
    entity_set_position(entity2, 10, 10); // Center at (30, 30) - closer for 315° rotation
    
    engine_update(g_engine, 0.016f, input_state);
    
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity1 should collide with 315° rotated rect");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity2 should collide with 315° rotated rect");

    // Test both entities rotated
    entity_remove_tag(entity1, "enter");
    entity_remove_tag(entity1, "stay");
    entity_remove_tag(entity1, "exit");
    entity_remove_tag(entity2, "enter");
    entity_remove_tag(entity2, "stay");
    entity_remove_tag(entity2, "exit");
    
    // Move entities apart to clear collision state
    entity_set_position(entity1, -100, -100);   // Far away
    entity_set_position(entity2, 100, 100);     // Far away
    
    engine_update(g_engine, 0.016f, input_state); // Update to clear collision state
    
    // Now set up both 45° rotation test
    ese_rect_set_rotation(rect1, M_PI / 4.0f); // 45 degrees
    ese_rect_set_rotation(rect2, M_PI / 4.0f); // 45 degrees
    
    entity_set_position(entity1, 0, 0);   // Center at (20, 20)
    entity_set_position(entity2, 15, 15); // Center at (35, 35) - much closer for collision
    
    engine_update(g_engine, 0.016f, input_state);
    
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity1 should collide with both 45° rotated rects");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity2 should collide with both 45° rotated rects");

    ese_input_state_destroy(input_state);
}

static void test_entity_offset_collision(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    EseLuaEngine *lua_engine = g_engine->lua_engine;
    TEST_ASSERT_NOT_NULL_MESSAGE(lua_engine, "Lua engine should be created");
    
    const char *script = 
    "function ENTITY:entity_collision_enter(other)\n"
    "    self:add_tag('enter')\n"
    "end\n";
    
    bool load_result = lua_engine_load_script_from_string(lua_engine, script, "offset_collision_script", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Offset collision script should load successfully");
    
    EseEntity *entity1 = entity_create(lua_engine);
    EseEntity *entity2 = entity_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(entity1, "Entity1 should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(entity2, "Entity2 should be created");

    EseEntityComponent *lua_comp1 = entity_component_lua_create(lua_engine, "offset_collision_script");
    EseEntityComponent *lua_comp2 = entity_component_lua_create(lua_engine, "offset_collision_script");
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

    // Set up rectangles with offset positions
    ese_rect_set_x(rect1, 10);  // Offset from entity position
    ese_rect_set_y(rect1, 10);
    ese_rect_set_width(rect1, 30);
    ese_rect_set_height(rect1, 30);

    ese_rect_set_x(rect2, -5);  // Negative offset
    ese_rect_set_y(rect2, -5);
    ese_rect_set_width(rect2, 30);
    ese_rect_set_height(rect2, 30);

    entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider1), rect1);
    entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider2), rect2);

    engine_add_entity(g_engine, entity1);
    engine_add_entity(g_engine, entity2);

    EseInputState *input_state = ese_input_state_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(input_state, "Input state should be created");

    // Test collision with offset rectangles
    entity_set_position(entity1, 0, 0);   // Rect1 world position: (10, 10) to (40, 40)
    entity_set_position(entity2, 20, 20); // Rect2 world position: (15, 15) to (45, 45)
    
    engine_update(g_engine, 0.016f, input_state);
    
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity1 should collide with offset rects");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity2 should collide with offset rects");

    // Test no collision when separated
    entity_remove_tag(entity1, "enter");
    entity_remove_tag(entity2, "enter");
    
    entity_set_position(entity1, 0, 0);   // Rect1 world position: (10, 10) to (40, 40)
    entity_set_position(entity2, 100, 100); // Rect2 world position: (95, 95) to (125, 125)
    
    engine_update(g_engine, 0.016f, input_state);
    
    TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity1 should not collide when separated");
    TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity2 should not collide when separated");

    ese_input_state_destroy(input_state);
}

static void test_entity_mixed_collision(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    EseLuaEngine *lua_engine = g_engine->lua_engine;
    TEST_ASSERT_NOT_NULL_MESSAGE(lua_engine, "Lua engine should be created");
    
    const char *script = 
    "function ENTITY:entity_collision_enter(other)\n"
    "    self:add_tag('enter')\n"
    "end\n";
    
    bool load_result = lua_engine_load_script_from_string(lua_engine, script, "mixed_collision_script", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Mixed collision script should load successfully");
    
    EseEntity *entity1 = entity_create(lua_engine);
    EseEntity *entity2 = entity_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(entity1, "Entity1 should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(entity2, "Entity2 should be created");

    EseEntityComponent *lua_comp1 = entity_component_lua_create(lua_engine, "mixed_collision_script");
    EseEntityComponent *lua_comp2 = entity_component_lua_create(lua_engine, "mixed_collision_script");
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

    // Set up mixed collision: rotated vs axis-aligned
    ese_rect_set_x(rect1, 0);
    ese_rect_set_y(rect1, 0);
    ese_rect_set_width(rect1, 40);
    ese_rect_set_height(rect1, 40);
    ese_rect_set_rotation(rect1, M_PI / 4.0f); // 45 degrees

    ese_rect_set_x(rect2, 0);
    ese_rect_set_y(rect2, 0);
    ese_rect_set_width(rect2, 40);
    ese_rect_set_height(rect2, 40);
    ese_rect_set_rotation(rect2, 0.0f); // No rotation

    entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider1), rect1);
    entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider2), rect2);

    engine_add_entity(g_engine, entity1);
    engine_add_entity(g_engine, entity2);

    EseInputState *input_state = ese_input_state_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(input_state, "Input state should be created");

    // Test collision between rotated and axis-aligned rectangles
    entity_set_position(entity1, 0, 0);   // Rotated rect center at (20, 20)
    entity_set_position(entity2, 25, 25); // Axis-aligned rect center at (45, 45) - closer for collision
    
    engine_update(g_engine, 0.016f, input_state);
    
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Rotated entity should collide with axis-aligned entity");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Axis-aligned entity should collide with rotated entity");

    // Test different rotation combinations
    entity_remove_tag(entity1, "enter");
    entity_remove_tag(entity2, "enter");
    
    // Move entities apart to clear collision state
    entity_set_position(entity1, -100, -100);   // Far away
    entity_set_position(entity2, 100, 100);     // Far away
    
    engine_update(g_engine, 0.016f, input_state); // Update to clear collision state
    
    // Now set up mixed rotation test
    ese_rect_set_rotation(rect1, M_PI / 2.0f); // 90 degrees
    ese_rect_set_rotation(rect2, M_PI / 6.0f); // 30 degrees
    
    entity_set_position(entity1, 0, 0);   // Center at (20, 20)
    entity_set_position(entity2, 10, 10); // Center at (30, 30) - very close for collision
    
    engine_update(g_engine, 0.016f, input_state);
    
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "90° rotated entity should collide with 30° rotated entity");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "30° rotated entity should collide with 90° rotated entity");

    ese_input_state_destroy(input_state);
}

static void test_entity_corner_cases(void) {
    g_engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_engine, "Engine should be created");
    
    EseLuaEngine *lua_engine = g_engine->lua_engine;
    TEST_ASSERT_NOT_NULL_MESSAGE(lua_engine, "Lua engine should be created");
    
    const char *script = 
    "function ENTITY:entity_collision_enter(other)\n"
    "    self:add_tag('enter')\n"
    "end\n";
    
    bool load_result = lua_engine_load_script_from_string(lua_engine, script, "corner_case_script", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Corner case script should load successfully");
    
    EseEntity *entity1 = entity_create(lua_engine);
    EseEntity *entity2 = entity_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(entity1, "Entity1 should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(entity2, "Entity2 should be created");

    EseEntityComponent *lua_comp1 = entity_component_lua_create(lua_engine, "corner_case_script");
    EseEntityComponent *lua_comp2 = entity_component_lua_create(lua_engine, "corner_case_script");
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

    // Set up small rectangles for precise testing
    ese_rect_set_x(rect1, 0);
    ese_rect_set_y(rect1, 0);
    ese_rect_set_width(rect1, 20);
    ese_rect_set_height(rect1, 20);
    ese_rect_set_rotation(rect1, M_PI / 4.0f); // 45 degrees

    ese_rect_set_x(rect2, 0);
    ese_rect_set_y(rect2, 0);
    ese_rect_set_width(rect2, 20);
    ese_rect_set_height(rect2, 20);
    ese_rect_set_rotation(rect2, 0.0f); // No rotation

    entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider1), rect1);
    entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider2), rect2);

    engine_add_entity(g_engine, entity1);
    engine_add_entity(g_engine, entity2);

    EseInputState *input_state = ese_input_state_create(lua_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(input_state, "Input state should be created");

    // Test corner-to-corner collision
    entity_set_position(entity1, 0, 0);   // Center at (10, 10)
    entity_set_position(entity2, 10, 10); // Center at (20, 20) - should overlap
    
    engine_update(g_engine, 0.016f, input_state);
    
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Entities should collide at corners");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Entities should collide at corners");

    // Test partial overlap
    entity_remove_tag(entity1, "enter");
    entity_remove_tag(entity2, "enter");
    
    // Move entities apart to clear collision state
    entity_set_position(entity1, -100, -100);   // Far away
    entity_set_position(entity2, 100, 100);     // Far away
    
    engine_update(g_engine, 0.016f, input_state); // Update to clear collision state
    
    // Now set up partial overlap test
    entity_set_position(entity1, 0, 0);   // Center at (10, 10)
    entity_set_position(entity2, 5, 5);  // Center at (15, 15) - partial overlap
    
    engine_update(g_engine, 0.016f, input_state);
    
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Entities should collide with partial overlap");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Entities should collide with partial overlap");

    // Test just separated (no collision)
    entity_remove_tag(entity1, "enter");
    entity_remove_tag(entity2, "enter");
    
    entity_set_position(entity1, 0, 0);   // Center at (10, 10)
    entity_set_position(entity2, 30, 30); // Center at (40, 40) - well separated
    
    engine_update(g_engine, 0.016f, input_state);
    
    TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity1, "enter"), "Entities should not collide when separated");
    TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity2, "enter"), "Entities should not collide when separated");

    ese_input_state_destroy(input_state);
}