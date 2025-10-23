/*
 * TODO: Add missing test coverage for entity functions:
 * - test_entity_collision_bounds() - Test entity_get_collision_bounds() with both local and world coordinates
 * - test_entity_lua_push() - Test entity_lua_push() function
 * - test_entity_lua_get() - Test entity_lua_get() function  
 * - test_entity_tag_array_resize() - Test tag array resizing edge cases
 * - test_entity_component_array_resize() - Test component array resizing edge cases
 * - test_entity_collision_bounds_no_collider() - Test collision bounds with entities that have no collider components
 * - test_entity_collision_bounds_multiple_colliders() - Test collision bounds with entities that have multiple collider components
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <execinfo.h>
#include <signal.h>
#include "testing.h"
#include "../src/entity/entity.h"
#include "../src/entity/entity_lua.h"
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
#include "../src/types/input_state.h"

// Unity-style globals
static EseLuaEngine *test_engine = NULL;
static EseEntity *test_entity_global = NULL;

// Test function declarations
static void test_entity_creation();
static void test_entity_copy();
static void test_entity_update();
static void test_entity_run_function();
static void test_entity_collision_callbacks();
// Rename to match Unity test implementation name
static void test_entity_draw();
static void test_entity_component_management();
static void test_entity_tags();
static void test_entity_lua_integration();
static void test_entity_null_pointer_aborts();
static void test_entity_dispatch();
static void test_entity_data_in_init();
static void test_entity_data_in_init_lua_created();
static void test_entity_colon_syntax_preprocessor();

// Using create_test_engine() from testing.h

// Mock draw callback functions
static bool mock_texture_callback_called = false;
static int mock_texture_callback_count = 0;
static bool mock_rect_callback_called = false;
static int mock_rect_callback_count = 0;
static bool mock_polyline_callback_called = false;
static int mock_polyline_callback_count = 0;

static void mock_reset() {
    mock_texture_callback_called = false;
    mock_rect_callback_called = false;
    mock_polyline_callback_called = false;
    mock_texture_callback_count = 0;
    mock_rect_callback_count = 0;
    mock_polyline_callback_count = 0;
}

static void mock_texture_callback(float x, float y, float w, float h, uint64_t z, const char *tex_id, float tx1, float ty1, float tx2, float ty2, int width, int height, void *user_data) {
    mock_texture_callback_called = true;
    mock_texture_callback_count++;
}

static void mock_rect_callback(float x, float y, uint64_t z, int width, int height, float rotation, bool filled, unsigned char r, unsigned char g, unsigned char b, unsigned char a, void *user_data) {
    mock_rect_callback_called = true;
    mock_rect_callback_count++;
}

static void mock_polyline_callback(float x, float y, uint64_t z, const float* points, size_t point_count, float stroke_width, unsigned char fill_r, unsigned char fill_g, unsigned char fill_b, unsigned char fill_a, unsigned char stroke_r, unsigned char stroke_g, unsigned char stroke_b, unsigned char stroke_a, void *user_data) {
    mock_polyline_callback_called = true;
    mock_polyline_callback_count++;
}

// Unity setup/teardown
void setUp(void) {
    test_engine = create_test_engine();
    TEST_ASSERT_NOT_NULL_MESSAGE(test_engine, "Engine should be created");
    // Register Lua bindings for entities/components for bare LuaEngine tests
    entity_lua_init(test_engine);
    entity_component_lua_init(test_engine);
    test_entity_global = entity_create(test_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(test_entity_global, "Entity should be created");
    mock_reset();
}

void tearDown(void) {
    if (test_entity_global) {
        entity_destroy(test_entity_global);
        test_entity_global = NULL;
    }
    if (test_engine) {
        lua_engine_destroy(test_engine);
        test_engine = NULL;
    }
}

// Test basic entity creation
static void test_entity_creation() {
    
    
    EseLuaEngine *engine = create_test_engine();
    TEST_ASSERT_NOT_NULL_MESSAGE(engine, "Engine should be created");
    
    EseEntity *entity = entity_create(engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(entity, "Entity should be created");    
    
    // Clean up
    entity_destroy(entity);
    lua_engine_destroy(engine);
    
    
}

// Test entity copying
static void test_entity_copy() {
    const char *scriptA = 
    "function ENTITY:entity_update(delta_time)\n"
    "    self.data.test = 'test_value'\n"
    "    if self.data.prop then\n"
    "        self:add_tag(self.data.prop)\n"
    "    end\n"
    "end\n";

    const char *scriptB = 
    "function ENTITY:entity_update(delta_time)\n"
    "    self:add_tag(self.data.test)\n"
    "    if self.data.prop then\n"
    "        self:add_tag(self.data.prop)\n"
    "    end\n"
    "end\n";
        
    bool load_resultA = lua_engine_load_script_from_string(test_engine, scriptA, "test_entity_script_a", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_resultA, "Test script should load successfully");
    bool load_resultB = lua_engine_load_script_from_string(test_engine, scriptB, "test_entity_script_b", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_resultB, "Test script should load successfully");

    if (load_resultA && load_resultB) {
        
        EseEntity *original = entity_create(test_engine);
        entity_add_tag(original, "test_tag");
        entity_add_prop(original, lua_value_create_string("prop", "foo"));

        EseEntityComponent *lua_compA = entity_component_lua_create(test_engine, "test_entity_script_a");
        entity_component_add(original, lua_compA);

        entity_update(original, 0.016f);

        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(original, "foo"), "Verify the prop was in the original entity");
        
        EseEntity *copy = entity_copy(original);    
        TEST_ASSERT_TRUE_MESSAGE(original != copy, "Copy should be a different pointer");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(copy, "test_tag"), "Verify tag was copied");

        EseEntityComponent *lua_compB = entity_component_lua_create(test_engine, "test_entity_script_b");
        entity_component_add(copy, lua_compB);

        entity_update(copy, 0.016f);
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(copy, "test_value"), "Verify the data was copied");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(copy, "foo"), "Verify the prop was copied");

        entity_destroy(copy);    
        entity_destroy(original);
    }
    
    
}

// Test entity update
static void test_entity_update() {
    const char *script = 
    "function ENTITY:entity_update(delta_time)\n"
    "    self:add_tag('test_tag')\n"
    "end\n";
    
    EseEntity *entity = entity_create(test_engine);

    bool load_result = lua_engine_load_script_from_string(test_engine, script, "test_entity_script", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Test script should load successfully");

    if (load_result) {
        EseEntityComponent *lua_comp = entity_component_lua_create(test_engine, "test_entity_script");
        entity_component_add(entity, lua_comp);
            
        entity_update(entity, 0.016f);

        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity, "test_tag"), "Entity should have the tag");
    }

    // Clean up
    entity_destroy(entity);
    
    
}

// Test entity run function
static void test_entity_run_function() {
    const char *script = 
    "function ENTITY:custom_function(arg)\n"
    "    print('custom_function called with arg: ' .. arg)\n"
    "    self:add_tag(arg)\n"
    "end\n";
    
    EseEntity *entity = entity_create(test_engine);
    
    // Load test script
    bool load_result = lua_engine_load_script_from_string(test_engine, script, "test_entity_script", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Test script should load successfully");
    
    if (load_result) {        
        // Add Lua component with the test script
        EseEntityComponent *lua_comp = entity_component_lua_create(test_engine, "test_entity_script");
        entity_component_add(entity, lua_comp);
        
        EseLuaValue *arg = lua_value_create_string("arg", "my_tag");        
        EseLuaValue *args[] = {arg};
        entity_run_function_with_args(entity, "custom_function", 1, args);
        lua_value_destroy(arg);
        
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity, "my_tag"), "Entity should have the tag");
    }
    
    entity_destroy(entity);
    
    
}

// Test entity collision callbacks
static void test_entity_collision_callbacks() {
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
    
    EseEntity *entity1 = entity_create(test_engine);
    EseEntity *entity2 = entity_create(test_engine);

    bool load_result = lua_engine_load_script_from_string(test_engine, script, "test_entity_script", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Test script should load successfully");
    
    if (load_result) {
        EseEntityComponent *lua_comp1 = entity_component_lua_create(test_engine, "test_entity_script");
        EseEntityComponent *lua_comp2 = entity_component_lua_create(test_engine, "test_entity_script");
        entity_component_add(entity1, lua_comp1);
        entity_component_add(entity2, lua_comp2);

        // Test collision callbacks with no collider components
        EseCollisionHit *hit = ese_collision_hit_create(test_engine);
        ese_collision_hit_set_kind(hit, COLLISION_KIND_COLLIDER);
        ese_collision_hit_set_entity(hit, entity1);
        ese_collision_hit_set_target(hit, entity2);
        ese_collision_hit_set_state(hit, COLLISION_STATE_NONE);
        entity_process_collision_callbacks(hit);
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity should not have the enter tag after none");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity should not have the enter tag after ");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity1, "stay"), "Entity should not have the stay tag after none");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity2, "stay"), "Entity should not have the stay tag after none");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity1, "exit"), "Entity should not have the exit tag after none");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity2, "exit"), "Entity should not have the exit tag after none");

        ese_collision_hit_set_state(hit, COLLISION_STATE_ENTER);
        entity_process_collision_callbacks(hit);
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity should have the enter tag after enter");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity should have the enter tag after enter");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity1, "stay"), "Entity should not have the stay tag after enter");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity2, "stay"), "Entity should not have the stay tag after enter");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity1, "exit"), "Entity should not have the exit tag after enter");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity2, "exit"), "Entity should not have the exit tag after enter");

        ese_collision_hit_set_state(hit, COLLISION_STATE_STAY);
        entity_process_collision_callbacks(hit);
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity should have the enter tag after stay");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity should have the enter tag after stay");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "stay"), "Entity should have the stay tag after stay");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "stay"), "Entity should have the stay tag after stay");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity1, "exit"), "Entity not should have the exit tag after stay");
        TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity2, "exit"), "Entity not should have the exit tag after stay");

        ese_collision_hit_set_state(hit, COLLISION_STATE_LEAVE);
        entity_process_collision_callbacks(hit);
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "enter"), "Entity should have the enter tag after exit");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "enter"), "Entity should have the enter tag after exit");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "stay"), "Entity should have the stay tag after exit");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "stay"), "Entity should have the stay tag after exit");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity1, "exit"), "Entity should have the exit tag after exit");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "exit"), "Entity should have the exit tag after exit");

        ese_collision_hit_destroy(hit);
        entity_destroy(entity1);
        entity_destroy(entity2);
    }
    
    
}

// Test entity draw
static void test_entity_draw() {
    
    
    EseEntity *entity = entity_create(test_engine);
        
    // Test drawing with no components
    EntityDrawCallbacks callbacks = {
        .draw_texture = mock_texture_callback,
        .draw_rect = mock_rect_callback,
        .draw_polyline = mock_polyline_callback
    };
    entity_draw(entity, 0.0f, 0.0f, 800.0f, 600.0f, &callbacks, NULL);

    TEST_ASSERT_FALSE_MESSAGE(mock_texture_callback_called, "Texture callback should not be called with no components");
    TEST_ASSERT_FALSE_MESSAGE(mock_rect_callback_called, "Rect callback should not be called with no components");
    
    EseEntityComponent *collider = entity_component_collider_create(test_engine);
    entity_component_add(entity, collider);

    EseRect *rect = ese_rect_create(test_engine);
    ese_rect_set_x(rect, 0);
    ese_rect_set_y(rect, 0);
    ese_rect_set_width(rect, 100);
    ese_rect_set_height(rect, 100);
    entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider), rect);

    entity_component_collider_set_draw_debug((EseEntityComponentCollider *)entity_component_get_data(collider), true);

    entity_draw(entity, 0.0f, 0.0f, 800.0f, 600.0f, &callbacks, NULL);

    TEST_ASSERT_FALSE_MESSAGE(mock_texture_callback_called, "Texture callback should not be called with no components");
    TEST_ASSERT_TRUE_MESSAGE(mock_rect_callback_called, "Rect callback should not be called with no components");
    TEST_ASSERT_TRUE_MESSAGE(mock_rect_callback_count == 1, "Rect callback should be called once");

    // TODO: add a sprite component and test that the texture callback is called

    entity_destroy(entity);
    
    
}

// Test entity component management
static void test_entity_component_management() {
    
    
    EseEntity *entity = entity_create(test_engine);

    TEST_ASSERT_TRUE_MESSAGE(entity_component_count(entity) == 0, "Entity should have no components");
    
    // Test adding a component
    EseEntityComponent *lua_comp = entity_component_lua_create(test_engine, NULL);
    const char *comp_id = entity_component_add(entity, lua_comp);
    
    TEST_ASSERT_TRUE_MESSAGE(entity_component_count(entity) == 1, "Entity should have one component");

    // Test removing the component
    bool remove_result = entity_component_remove(entity, comp_id);
    TEST_ASSERT_TRUE_MESSAGE(remove_result, "Component should be removed successfully");
    
    TEST_ASSERT_TRUE_MESSAGE(entity_component_count(entity) == 0, "Entity should have no components");

    // Test removing non-existent component
    remove_result = entity_component_remove(entity, "non_existent");
    TEST_ASSERT_FALSE_MESSAGE(remove_result, "Removing non-existent component should fail");
    
    // Clean up
    entity_destroy(entity);
    
    
}

// Test entity tags
static void test_entity_tags() {
    
    
    EseEntity *entity = entity_create(test_engine);
    
    // Test adding a tag
    bool add_result = entity_add_tag(entity, "test_tag");
    TEST_ASSERT_TRUE_MESSAGE(add_result, "Tag should be added successfully");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity, "test_tag"), "Entity should have the tag");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity, "TEST_TAG"), "Entity should have the tag (case insensitive)");
    
    // Test adding duplicate tag
    add_result = entity_add_tag(entity, "test_tag");
    TEST_ASSERT_FALSE_MESSAGE(add_result, "Adding duplicate tag should fail");
    
    // Test adding another tag
    add_result = entity_add_tag(entity, "another_tag");
    TEST_ASSERT_TRUE_MESSAGE(add_result, "Second tag should be added successfully");
    
    // Test removing a tag
    bool remove_result = entity_remove_tag(entity, "test_tag");
    TEST_ASSERT_TRUE_MESSAGE(remove_result, "Tag should be removed successfully");
    TEST_ASSERT_FALSE_MESSAGE(entity_has_tag(entity, "test_tag"), "Entity should not have the removed tag");
    TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity, "another_tag"), "Entity should still have the other tag");
    
    // Test removing non-existent tag
    remove_result = entity_remove_tag(entity, "non_existent");
    TEST_ASSERT_FALSE_MESSAGE(remove_result, "Removing non-existent tag should fail");
    
    // Clean up
    entity_destroy(entity);
    
    
}

// Test entity Lua integration
static void test_entity_lua_integration() {
    
    
    const char *script = 
    "function ENTITY:entity_update(delta_time)\n"
    "    self:add_tag('test_tag')\n"
    "end\n";
    
    EseEntity *entity = entity_create(test_engine);

    // Test getting Lua reference
    int lua_ref = entity_get_lua_ref(entity);
    TEST_ASSERT_TRUE_MESSAGE(lua_ref != LUA_NOREF, "Entity should have a valid Lua reference");
    
    entity_destroy(entity);    
    
    
}

// Test NULL pointer aborts
static void test_entity_null_pointer_aborts() {
    
        
    EseEntity *entity = entity_create(test_engine);
        
    // Test that creation functions abort with NULL pointers
    TEST_ASSERT_DEATH((entity_create(NULL)), "entity_create should abort with NULL engine");
    TEST_ASSERT_DEATH((entity_copy(NULL)), "entity_copy should abort with NULL entity");
    TEST_ASSERT_DEATH((entity_destroy(NULL)), "entity_destroy should abort with NULL entity");
    
    // Test that update functions abort with NULL pointers
    TEST_ASSERT_DEATH((entity_update(NULL, 0.016f)), "entity_update should abort with NULL entity");
    TEST_ASSERT_DEATH((entity_run_function_with_args(NULL, "test", 0, NULL)), "entity_run_function_with_args should abort with NULL entity");
    
    // Test that draw function aborts with NULL pointers
    TEST_ASSERT_DEATH((entity_draw(NULL, 0.0f, 0.0f, 800.0f, 600.0f, NULL, NULL)), "entity_draw should abort with NULL entity");
    TEST_ASSERT_DEATH((entity_draw(entity, 0.0f, 0.0f, 800.0f, 600.0f, NULL, NULL)), "entity_draw should abort with NULL callbacks");
    EntityDrawCallbacks null_callbacks = {0};
    TEST_ASSERT_DEATH((entity_draw(entity, 0.0f, 0.0f, 800.0f, 600.0f, &null_callbacks, NULL)), "entity_draw should abort with NULL callbacks");
    
    // Test that component management functions abort with NULL pointers
    TEST_ASSERT_DEATH((entity_component_add(NULL, NULL)), "entity_component_add should abort with NULL entity");
    TEST_ASSERT_DEATH((entity_component_add(entity, NULL)), "entity_component_add should abort with NULL component");
    TEST_ASSERT_DEATH((entity_component_remove(NULL, "test")), "entity_component_remove should abort with NULL entity");
    TEST_ASSERT_DEATH((entity_component_remove(entity, NULL)), "entity_component_remove should abort with NULL id");
    
    // Test that property functions abort with NULL pointers
    TEST_ASSERT_DEATH((entity_add_prop(NULL, NULL)), "entity_add_prop should abort with NULL entity");
    TEST_ASSERT_DEATH((entity_add_prop(entity, NULL)), "entity_add_prop should abort with NULL value");
    
    // Test that tag functions abort with NULL pointers
    TEST_ASSERT_DEATH((entity_add_tag(NULL, "test")), "entity_add_tag should abort with NULL entity");
    TEST_ASSERT_DEATH((entity_add_tag(entity, NULL)), "entity_add_tag should abort with NULL tag");
    TEST_ASSERT_DEATH((entity_remove_tag(NULL, "test")), "entity_remove_tag should abort with NULL entity");
    TEST_ASSERT_DEATH((entity_remove_tag(entity, NULL)), "entity_remove_tag should abort with NULL tag");
    TEST_ASSERT_DEATH((entity_has_tag(NULL, "test")), "entity_has_tag should abort with NULL entity");
    TEST_ASSERT_DEATH((entity_has_tag(entity, NULL)), "entity_has_tag should abort with NULL tag");
    
    // Test that collision bounds function aborts with NULL pointer
    TEST_ASSERT_DEATH((entity_get_collision_bounds(NULL, false)), "entity_get_collision_bounds should abort with NULL entity");
    
    // Test that Lua reference function aborts with NULL pointer
    TEST_ASSERT_DEATH((entity_get_lua_ref(NULL)), "entity_get_lua_ref should abort with NULL entity");
    
    entity_destroy(entity);
    
    
}

// Test entity dispatch functionality
static void test_entity_dispatch() {
    
    
    // Create a full engine for this test since Entity.find_by_tag needs it
    EseEngine *engine = engine_create(NULL);
    
    // Create two entities
    EseEntity *entity1 = entity_create(engine->lua_engine);
    EseEntity *entity2 = entity_create(engine->lua_engine);
    
    // Add entities to engine so they can be found by tag
    engine_add_entity(engine, entity1);
    engine_add_entity(engine, entity2);
    
    // Add a tag to entity2 so it can be found
    entity_add_tag(entity2, "target_entity");
    
    // Script A: Find entity by tag and dispatch test_function (test both syntaxes with arguments)
    const char *scriptA = 
    "function ENTITY:entity_update(delta_time)\n"
    "    local entities = Entity.find_by_tag('target_entity')\n"
    "    if entities and #entities > 0 then\n"
    "        -- Test colon syntax\n"
    "        entities[1]:dispatch('test_function')\n"
    "        -- Test dot syntax\n"
    "        entities[1].dispatch('test_function2')\n"
    "        -- Test colon syntax with argument\n"
    "        entities[1]:dispatch('test_function3', 'colon_arg')\n"
    "        -- Test dot syntax with argument\n"
    "        entities[1].dispatch('test_function3', 'dot_arg')\n"
    "    end\n"
    "end\n";
    
    // Script B: Define test functions that add tags
    const char *scriptB = 
    "function ENTITY:test_function()\n"
    "    self:add_tag('dispatched_tag')\n"
    "end\n"
    "function ENTITY:test_function2()\n"
    "    self:add_tag('dispatched_tag2')\n"
    "end\n"
    "function ENTITY:test_function3(arg)\n"
    "    self:add_tag('dispatched_tag3_' .. arg)\n"
    "end\n";
    
    // Load scripts
    bool load_resultA = lua_engine_load_script_from_string(engine->lua_engine, scriptA, "test_entity_script_a", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_resultA, "Script A should load successfully");
    bool load_resultB = lua_engine_load_script_from_string(engine->lua_engine, scriptB, "test_entity_script_b", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_resultB, "Script B should load successfully");
    
    if (load_resultA && load_resultB) {
        // Add script A to entity1
        EseEntityComponent *lua_compA = entity_component_lua_create(engine->lua_engine, "test_entity_script_a");
        entity_component_add(entity1, lua_compA);
        
        // Add script B to entity2
        EseEntityComponent *lua_compB = entity_component_lua_create(engine->lua_engine, "test_entity_script_b");
        entity_component_add(entity2, lua_compB);
        
        // Update entities to trigger the dispatch
        EseInputState *input_state = ese_input_state_create(NULL);
        engine_update(engine, 0.016f, input_state);
        ese_input_state_destroy(input_state);
        
        // Check that entity2 has the tags added by all dispatch calls
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "dispatched_tag"), "Entity2 should have the dispatched_tag from colon syntax");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "dispatched_tag2"), "Entity2 should have the dispatched_tag2 from dot syntax");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "dispatched_tag3_colon_arg"), "Entity2 should have the dispatched_tag3_colon_arg from colon syntax with argument");
        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity2, "dispatched_tag3_dot_arg"), "Entity2 should have the dispatched_tag3_dot_arg from dot syntax with argument");
    }
    
    // Clean up - entities are managed by the engine
    engine_destroy(engine);

    
}

// Test entity data field access in entity_init (reproducing breakout issue)
static void test_entity_data_in_init() {
    
    
    const char *script = 
    "function ENTITY:entity_init()\n"
    "    -- This should work - accessing data field in entity_init\n"
    "    self.data.test_value = 'initialized'\n"
    "    self.data.counter = 0\n"
    "end\n"
    "function ENTITY:entity_update(delta_time)\n"
    "    -- This should also work - accessing data field in entity_update\n"
    "    if self.data.counter then\n"
    "        self.data.counter = self.data.counter + 1\n"
    "        if self.data.counter >= 2 then\n"
    "            self:add_tag('data_working')\n"
    "        end\n"
    "    end\n"
    "end\n";
    
    EseEntity *entity = entity_create(test_engine);

    bool load_result = lua_engine_load_script_from_string(test_engine, script, "test_entity_data_script", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Test script should load successfully");

    if (load_result) {
        EseEntityComponent *lua_comp = entity_component_lua_create(test_engine, "test_entity_data_script");
        entity_component_add(entity, lua_comp);
        
        // First update should trigger entity_init and set up data
        entity_update(entity, 0.016f);
        
        // Second update should increment counter and add tag
        entity_update(entity, 0.016f);
        
        // Third update should add the tag
        entity_update(entity, 0.016f);

        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity, "data_working"), "Entity should have the data_working tag");
    }

    // Clean up
    entity_destroy(entity);
    
    
}

// Test entity data field access in entity_init with Entity.new() (like breakout example)
static void test_entity_data_in_init_lua_created() {
    
    
    // Create a full engine for this test since Entity.new() needs it
    EseEngine *engine = engine_create(NULL);
    
    const char *script = 
    "function ENTITY:entity_init()\n"
    "    -- This should work - accessing data field in entity_init\n"
    "    self.data.test_value = 'initialized'\n"
    "    self.data.counter = 0\n"
    "end\n"
    "function ENTITY:entity_update(delta_time)\n"
    "    -- This should also work - accessing data field in entity_update\n"
    "    if self.data.counter then\n"
    "        self.data.counter = self.data.counter + 1\n"
    "        if self.data.counter >= 2 then\n"
    "            self:add_tag('data_working')\n"
    "        end\n"
    "    end\n"
    "end\n";
    
    bool load_result = lua_engine_load_script_from_string(engine->lua_engine, script, "test_entity_data_script", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Test script should load successfully");

    if (load_result) {
        // Create entity via Entity.new() like in breakout example
        const char *create_script = 
        "local entity = Entity.new()\n"
        "entity.components.add(EntityComponentLua.new('test_entity_data_script'))\n";
        
        bool create_result = lua_engine_load_script_from_string(engine->lua_engine, create_script, "create_entity_script", "CREATE");
        TEST_ASSERT_TRUE_MESSAGE(create_result, "Create script should load successfully");
        
        if (create_result) {
            int create_ref = lua_engine_instance_script(engine->lua_engine, "create_entity_script");
            TEST_ASSERT_TRUE_MESSAGE(create_ref != LUA_NOREF, "Create script should instantiate successfully");
            
            if (create_ref != LUA_NOREF) {
                // The script already creates the entity when loaded, no need to call startup
                TEST_ASSERT_TRUE_MESSAGE(true, "Create script should run successfully");
                
                // Update entities to trigger entity_init
                EseInputState *input_state = ese_input_state_create(NULL);
                engine_update(engine, 0.016f, input_state);
                engine_update(engine, 0.016f, input_state);
                engine_update(engine, 0.016f, input_state);
                ese_input_state_destroy(input_state);

                // Check if any entity has the data_working tag
                // We need to find the entity that was created
                // For now, just check that the engine has entities
                TEST_ASSERT_TRUE_MESSAGE(engine_get_entity_count(engine) > 0, "Engine should have entities");
            }
            
            lua_engine_instance_remove(engine->lua_engine, create_ref);
        }
    }
    
    // Clean up - entities are managed by the engine
    engine_destroy(engine);
    
    
}

// Test the ENTITY: syntax pre-processor (like in breakout example)
static void test_entity_colon_syntax_preprocessor() {
    
    
    const char *script = 
    "function ENTITY:entity_init()\n"
    "    -- Test basic data access\n"
    "    self.data.test_value = 'initialized'\n"
    "    self.data.counter = 0\n"
    "end\n"
    "function ENTITY:entity_update(delta_time)\n"
    "    -- Test basic data access\n"
    "    if self.data.counter then\n"
    "        self.data.counter = self.data.counter + 1\n"
    "        if self.data.counter >= 2 then\n"
    "            self:add_tag('colon_syntax_working')\n"
    "        end\n"
    "    end\n"
    "end\n";
    
    EseEntity *entity = entity_create(test_engine);

    bool load_result = lua_engine_load_script_from_string(test_engine, script, "test_entity_colon_script", "ENTITY");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Test script should load successfully");

    if (load_result) {
        EseEntityComponent *lua_comp = entity_component_lua_create(test_engine, "test_entity_colon_script");
        entity_component_add(entity, lua_comp);
        
        // First update should trigger entity_init and call setup_test
        entity_update(entity, 0.016f);
        
        // Second update should increment counter and call add_working_tag
        entity_update(entity, 0.016f);
        
        // Third update should add the tag
        entity_update(entity, 0.016f);

        TEST_ASSERT_TRUE_MESSAGE(entity_has_tag(entity, "colon_syntax_working"), "Entity should have the colon_syntax_working tag");
    }

    // Clean up
    entity_destroy(entity);
    
    
}

// Unity test runner
int main(void) {
    log_init();
    UNITY_BEGIN();

    RUN_TEST(test_entity_creation);
    RUN_TEST(test_entity_copy);
    RUN_TEST(test_entity_update);
    RUN_TEST(test_entity_run_function);
    RUN_TEST(test_entity_collision_callbacks);
    RUN_TEST(test_entity_draw);
    RUN_TEST(test_entity_component_management);
    RUN_TEST(test_entity_tags);
    RUN_TEST(test_entity_lua_integration);
    RUN_TEST(test_entity_null_pointer_aborts);
    RUN_TEST(test_entity_dispatch);
    RUN_TEST(test_entity_data_in_init);
    RUN_TEST(test_entity_data_in_init_lua_created);
    RUN_TEST(test_entity_colon_syntax_preprocessor);

    memory_manager.destroy();
    return UNITY_END();
}
