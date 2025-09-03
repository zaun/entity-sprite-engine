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

// Test function declarations
static void test_entity_creation();
static void test_entity_copy();
static void test_entity_update();
static void test_entity_run_function();
static void test_entity_collision_detection();
static void test_entity_collision_callbacks();
static void test_entity_collision();
static void test_entity_draw();
static void test_entity_component_management();
static void test_entity_tags();
static void test_entity_lua_integration();
static void test_entity_null_pointer_aborts();
static void test_entity_dispatch();

// Helper function to create and initialize engine
static EseLuaEngine* create_test_engine() {
    EseLuaEngine *engine = lua_engine_create();
    if (engine) {
        // Set up registry keys that entity system needs
        lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);
        
        // Initialize entity system
        entity_lua_init(engine);
        entity_component_lua_init(engine);
    }
    return engine;
}

// Mock draw callback functions
static bool mock_texture_callback_called = false;
static int mock_texture_callback_count = 0;
static bool mock_rect_callback_called = false;
static int mock_rect_callback_count = 0;

static void mock_reset() {
    mock_texture_callback_called = false;
    mock_rect_callback_called = false;
    mock_texture_callback_count = 0;
    mock_rect_callback_count = 0;
}

static void mock_texture_callback(float x, float y, float w, float h, int z, const char *tex_id, float tx1, float ty1, float tx2, float ty2, int width, int height, void *user_data) {
    mock_texture_callback_called = true;
    mock_texture_callback_count++;
}

static void mock_rect_callback(float x, float y, int z, int width, int height, float rotation, bool filled, unsigned char r, unsigned char g, unsigned char b, unsigned char a, void *user_data) {
    mock_rect_callback_called = true;
    mock_rect_callback_count++;
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
    
    test_suite_begin("🧪 Starting Entity Tests");
    
    // Initialize required systems
    log_init();
    
    // Run all test suites
    test_entity_creation();
    test_entity_copy();
    test_entity_update();
    test_entity_run_function();
    test_entity_collision_detection();
    test_entity_collision_callbacks();
    test_entity_collision();
    test_entity_draw();
    test_entity_component_management();
    test_entity_tags();
    test_entity_lua_integration();
    test_entity_null_pointer_aborts();
    test_entity_dispatch();
    
    // Print final summary
    test_suite_end("🎯 Final Test Summary");
    
    return 0;
}

// Test basic entity creation
static void test_entity_creation() {
    test_begin("Entity Creation");
    
    EseLuaEngine *engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");
    
    EseEntity *entity = entity_create(engine);
    TEST_ASSERT_NOT_NULL(entity, "Entity should be created");    
    
    // Clean up
    entity_destroy(entity);
    lua_engine_destroy(engine);
    
    test_end("Entity Creation");
}

// Test entity copying
static void test_entity_copy() {
    test_begin("Entity Copy");

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
    
    EseLuaEngine *engine = create_test_engine();
    
    bool load_resultA = lua_engine_load_script_from_string(engine, scriptA, "test_entity_script_a", "ENTITY");
    TEST_ASSERT(load_resultA, "Test script should load successfully");
    bool load_resultB = lua_engine_load_script_from_string(engine, scriptB, "test_entity_script_b", "ENTITY");
    TEST_ASSERT(load_resultB, "Test script should load successfully");

    if (load_resultA && load_resultB) {
        
        EseEntity *original = entity_create(engine);
        entity_add_tag(original, "test_tag");
        entity_add_prop(original, lua_value_create_string("prop", "foo"));

        EseEntityComponent *lua_compA = entity_component_lua_create(engine, "test_entity_script_a");
        entity_component_add(original, lua_compA);

        entity_update(original, 0.016f);

        TEST_ASSERT(entity_has_tag(original, "foo"), "Verify the prop was in the original entity");
        
        EseEntity *copy = entity_copy(original);    
        TEST_ASSERT(original != copy, "Copy should be a different pointer");
        TEST_ASSERT(entity_has_tag(copy, "test_tag"), "Verify tag was copied");

        EseEntityComponent *lua_compB = entity_component_lua_create(engine, "test_entity_script_b");
        entity_component_add(copy, lua_compB);

        entity_update(copy, 0.016f);
        TEST_ASSERT(entity_has_tag(copy, "test_value"), "Verify the data was copied");
        TEST_ASSERT(entity_has_tag(copy, "foo"), "Verify the prop was copied");

        entity_destroy(copy);    
        entity_destroy(original);
    }

    // Clean up
    lua_engine_destroy(engine);
    
    test_end("Entity Copy");
}

// Test entity update
static void test_entity_update() {
    test_begin("Entity Update");

    const char *script = 
    "function ENTITY:entity_update(delta_time)\n"
    "    self:add_tag('test_tag')\n"
    "end\n";
    
    EseLuaEngine *engine = create_test_engine();
    EseEntity *entity = entity_create(engine);

    bool load_result = lua_engine_load_script_from_string(engine, script, "test_entity_script", "ENTITY");
    TEST_ASSERT(load_result, "Test script should load successfully");

    if (load_result) {
        EseEntityComponent *lua_comp = entity_component_lua_create(engine, "test_entity_script");
        entity_component_add(entity, lua_comp);
            
        entity_update(entity, 0.016f);

        TEST_ASSERT(entity_has_tag(entity, "test_tag"), "Entity should have the tag");
    }

    // Clean up
    entity_destroy(entity);
    lua_engine_destroy(engine);
    
    test_end("Entity Update");
}

// Test entity run function
static void test_entity_run_function() {
    test_begin("Entity Run Function");

    const char *script = 
    "function ENTITY:custom_function(arg)\n"
    "    print('custom_function called with arg: ' .. arg)\n"
    "    self:add_tag(arg)\n"
    "end\n";
    
    EseLuaEngine *engine = create_test_engine();
    EseEntity *entity = entity_create(engine);
    
    // Load test script
    bool load_result = lua_engine_load_script_from_string(engine, script, "test_entity_script", "ENTITY");
    TEST_ASSERT(load_result, "Test script should load successfully");
    
    if (load_result) {        
        // Add Lua component with the test script
        EseEntityComponent *lua_comp = entity_component_lua_create(engine, "test_entity_script");
        entity_component_add(entity, lua_comp);
        
        EseLuaValue *arg = lua_value_create_string("arg", "my_tag");        
        EseLuaValue *args[] = {arg};
        entity_run_function_with_args(entity, "custom_function", 1, args);
        lua_value_free(arg);
        
        TEST_ASSERT(entity_has_tag(entity, "my_tag"), "Entity should have the tag");
    }
    
    entity_destroy(entity);
    lua_engine_destroy(engine);
    
    test_end("Entity Run Function");
}

// Test entity collision detection
static void test_entity_collision_detection() {
    test_begin("Entity Collision Detection");
    
    EseLuaEngine *engine = create_test_engine();
    EseEntity *entity1 = entity_create(engine);
    EseEntity *entity2 = entity_create(engine);
    
    // Test collision state with no collider components
    int collision_state = entity_check_collision_state(entity1, entity2);
    TEST_ASSERT_EQUAL(0, collision_state, "Entities with no colliders should not collide");
    
    // Test collision state with same entity
    collision_state = entity_check_collision_state(entity1, entity1);
    TEST_ASSERT_EQUAL(0, collision_state, "Entity should not collide with itself");
    
    entity_destroy(entity1);
    entity_destroy(entity2);
    lua_engine_destroy(engine);
    
    test_end("Entity Collision Detection");
}

// Test entity collision callbacks
static void test_entity_collision_callbacks() {
    test_begin("Entity Collision Callbacks");

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
    
    EseLuaEngine *engine = create_test_engine();
    EseEntity *entity1 = entity_create(engine);
    EseEntity *entity2 = entity_create(engine);

    bool load_result = lua_engine_load_script_from_string(engine, script, "test_entity_script", "ENTITY");
    TEST_ASSERT(load_result, "Test script should load successfully");
    
    if (load_result) {
        EseEntityComponent *lua_comp1 = entity_component_lua_create(engine, "test_entity_script");
        EseEntityComponent *lua_comp2 = entity_component_lua_create(engine, "test_entity_script");
        entity_component_add(entity1, lua_comp1);
        entity_component_add(entity2, lua_comp2);

        // Test collision callbacks with no collider components
        entity_process_collision_callbacks(entity1, entity2, 0); // NONE
        TEST_ASSERT(!entity_has_tag(entity1, "enter"), "Entity should not have the enter tag after none");
        TEST_ASSERT(!entity_has_tag(entity2, "enter"), "Entity should not have the enter tag after ");
        TEST_ASSERT(!entity_has_tag(entity1, "stay"), "Entity should not have the stay tag after none");
        TEST_ASSERT(!entity_has_tag(entity2, "stay"), "Entity should not have the stay tag after none");
        TEST_ASSERT(!entity_has_tag(entity1, "exit"), "Entity should not have the exit tag after none");
        TEST_ASSERT(!entity_has_tag(entity2, "exit"), "Entity should not have the exit tag after none");

        entity_process_collision_callbacks(entity1, entity2, 1); // ENTER
        TEST_ASSERT(entity_has_tag(entity1, "enter"), "Entity should have the enter tag after enter");
        TEST_ASSERT(entity_has_tag(entity2, "enter"), "Entity should have the enter tag after enter");
        TEST_ASSERT(!entity_has_tag(entity1, "stay"), "Entity should not have the stay tag after enter");
        TEST_ASSERT(!entity_has_tag(entity2, "stay"), "Entity should not have the stay tag after enter");
        TEST_ASSERT(!entity_has_tag(entity1, "exit"), "Entity should not have the exit tag after enter");
        TEST_ASSERT(!entity_has_tag(entity2, "exit"), "Entity should not have the exit tag after enter");

        entity_process_collision_callbacks(entity1, entity2, 2); // STAY
        TEST_ASSERT(entity_has_tag(entity1, "enter"), "Entity should have the enter tag after stay");
        TEST_ASSERT(entity_has_tag(entity2, "enter"), "Entity should have the enter tag after stay");
        TEST_ASSERT(entity_has_tag(entity1, "stay"), "Entity should have the stay tag after stay");
        TEST_ASSERT(entity_has_tag(entity2, "stay"), "Entity should have the stay tag after stay");
        TEST_ASSERT(!entity_has_tag(entity1, "exit"), "Entity not should have the exit tag after stay");
        TEST_ASSERT(!entity_has_tag(entity2, "exit"), "Entity not should have the exit tag after stay");

        entity_process_collision_callbacks(entity1, entity2, 3); // EXIT
        TEST_ASSERT(entity_has_tag(entity1, "enter"), "Entity should have the enter tag after exit");
        TEST_ASSERT(entity_has_tag(entity2, "enter"), "Entity should have the enter tag after exit");
        TEST_ASSERT(entity_has_tag(entity1, "stay"), "Entity should have the stay tag after exit");
        TEST_ASSERT(entity_has_tag(entity2, "stay"), "Entity should have the stay tag after exit");
        TEST_ASSERT(entity_has_tag(entity1, "exit"), "Entity should have the exit tag after exit");
        TEST_ASSERT(entity_has_tag(entity2, "exit"), "Entity should have the exit tag after exit");

        entity_destroy(entity1);
        entity_destroy(entity2);
    }
    lua_engine_destroy(engine);
    
    test_end("Entity Collision Callbacks");
}

// Test entity collision with rect
static void test_entity_collision() {
    test_begin("Entity Collision with Rect");

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
    
    EseLuaEngine *engine = create_test_engine();

    bool load_result = lua_engine_load_script_from_string(engine, script, "test_entity_script", "ENTITY");
    TEST_ASSERT(load_result, "Test script should load successfully");
    
    if (load_result) {
        EseEntity *entity1 = entity_create(engine);
        EseEntity *entity2 = entity_create(engine);

        EseEntityComponent *lua_comp1 = entity_component_lua_create(engine, "test_entity_script");
        EseEntityComponent *lua_comp2 = entity_component_lua_create(engine, "test_entity_script");

        entity_component_add(entity1, lua_comp1);
        entity_component_add(entity2, lua_comp2);

        EseEntityComponent *collider1 = entity_component_collider_create(engine);
        EseEntityComponent *collider2 = entity_component_collider_create(engine);

        entity_component_add(entity1, collider1);
        entity_component_add(entity2, collider2);

        EseRect *rect1 = rect_create(engine);
        EseRect *rect2 = rect_create(engine);

        rect_set_x(rect1, 0);
        rect_set_y(rect1, 0);
        rect_set_width(rect1, 100);
        rect_set_height(rect1, 100);

        rect_set_x(rect2, 0);
        rect_set_y(rect2, 0);
        rect_set_width(rect2, 100);
        rect_set_height(rect2, 100);

        entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider1), rect1);
        entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider2), rect2);

        entity_set_position(entity1, 0, 0);
        entity_set_position(entity2, 300, 0);

        int collision_state = 0;

        // No collisions
        entity_update(entity1, 0.016f);
        entity_update(entity2, 0.016f);
        collision_state = entity_check_collision_state(entity1, entity2);
        entity_process_collision_callbacks(entity1, entity2, collision_state);
        TEST_ASSERT(!entity_has_tag(entity1, "enter"), "Entity should not have the enter tag after no collisions");
        TEST_ASSERT(!entity_has_tag(entity2, "enter"), "Entity should not have the enter tag after no collisions");
        TEST_ASSERT(!entity_has_tag(entity1, "stay"), "Entity should not have the stay tag after no collisions");
        TEST_ASSERT(!entity_has_tag(entity2, "stay"), "Entity should not have the stay tag after no collisions");
        TEST_ASSERT(!entity_has_tag(entity1, "exit"), "Entity should not have the exit tag after no collisions");
        TEST_ASSERT(!entity_has_tag(entity2, "exit"), "Entity should not have the exit tag after no collisions");

        // Collisions entier
        entity_set_position(entity1, 150, 0);
        entity_set_position(entity2, 200, 0);
        entity_update(entity1, 0.016f);
        entity_update(entity2, 0.016f);
        collision_state = entity_check_collision_state(entity1, entity2);
        entity_process_collision_callbacks(entity1, entity2, collision_state);
        TEST_ASSERT(entity_has_tag(entity1, "enter"), "Entity should have the enter tag after collisions");
        TEST_ASSERT(entity_has_tag(entity2, "enter"), "Entity should have the enter tag after collisions");
        TEST_ASSERT(!entity_has_tag(entity1, "stay"), "Entity should not have the stay tag after collisions");
        TEST_ASSERT(!entity_has_tag(entity2, "stay"), "Entity should not have the stay tag after collisions");
        TEST_ASSERT(!entity_has_tag(entity1, "exit"), "Entity should not have the exit tag after collisions");
        TEST_ASSERT(!entity_has_tag(entity2, "exit"), "Entity should not have the exit tag after collisions");

        // Collisions entier
        entity_set_position(entity1, 200, 0);
        entity_set_position(entity2, 200, 0);
        entity_update(entity1, 0.016f);
        entity_update(entity2, 0.016f);
        collision_state = entity_check_collision_state(entity1, entity2);
        entity_process_collision_callbacks(entity1, entity2, collision_state);
        TEST_ASSERT(entity_has_tag(entity1, "enter"), "Entity should have the enter tag after collisions");
        TEST_ASSERT(entity_has_tag(entity2, "enter"), "Entity should have the enter tag after collisions");
        TEST_ASSERT(entity_has_tag(entity1, "stay"), "Entity should have the stay tag after collisions");
        TEST_ASSERT(entity_has_tag(entity2, "stay"), "Entity should have the stay tag after collisions");
        TEST_ASSERT(!entity_has_tag(entity1, "exit"), "Entity should not have the exit tag after collisions");
        TEST_ASSERT(!entity_has_tag(entity2, "exit"), "Entity should not have the exit tag after collisions");

        // Collisions entier
        entity_set_position(entity1, 301, 0);
        entity_set_position(entity2, 200, 0);
        entity_update(entity1, 0.016f);
        entity_update(entity2, 0.016f);
        collision_state = entity_check_collision_state(entity1, entity2);
        entity_process_collision_callbacks(entity1, entity2, collision_state);
        TEST_ASSERT(entity_has_tag(entity1, "enter"), "Entity should have the enter tag after collisions");
        TEST_ASSERT(entity_has_tag(entity2, "enter"), "Entity should have the enter tag after collisions");
        TEST_ASSERT(entity_has_tag(entity1, "stay"), "Entity should have the stay tag after collisions");
        TEST_ASSERT(entity_has_tag(entity2, "stay"), "Entity should have the stay tag after collisions");
        TEST_ASSERT(entity_has_tag(entity1, "exit"), "Entity should have the exit tag after collisions");
        TEST_ASSERT(entity_has_tag(entity2, "exit"), "Entity should have the exit tag after collisions");

        entity_destroy(entity1);
        entity_destroy(entity2);
    }            
        
    lua_engine_destroy(engine);
    
    test_end("Entity Collision with Rect");
}

// Test entity draw
static void test_entity_draw() {
    test_begin("Entity Draw");
    
    EseLuaEngine *engine = create_test_engine();
    EseEntity *entity = entity_create(engine);
        
    // Test drawing with no components
    entity_draw(entity, 0.0f, 0.0f, 800.0f, 600.0f, mock_texture_callback, mock_rect_callback, NULL);

    TEST_ASSERT(!mock_texture_callback_called, "Texture callback should not be called with no components");
    TEST_ASSERT(!mock_rect_callback_called, "Rect callback should not be called with no components");
    
    EseEntityComponent *collider = entity_component_collider_create(engine);
    entity_component_add(entity, collider);

    EseRect *rect = rect_create(engine);
    rect_set_x(rect, 0);
    rect_set_y(rect, 0);
    rect_set_width(rect, 100);
    rect_set_height(rect, 100);
    entity_component_collider_rects_add((EseEntityComponentCollider *)entity_component_get_data(collider), rect);

    entity_component_collider_set_draw_debug((EseEntityComponentCollider *)entity_component_get_data(collider), true);

    entity_draw(entity, 0.0f, 0.0f, 800.0f, 600.0f, mock_texture_callback, mock_rect_callback, NULL);

    TEST_ASSERT(!mock_texture_callback_called, "Texture callback should not be called with no components");
    TEST_ASSERT(mock_rect_callback_called, "Rect callback should not be called with no components");
    TEST_ASSERT(mock_rect_callback_count == 1, "Rect callback should be called once");

    // TODO: add a sprite component and test that the texture callback is called

    entity_destroy(entity);
    lua_engine_destroy(engine);
    
    test_end("Entity Draw");
}

// Test entity component management
static void test_entity_component_management() {
    test_begin("Entity Component Management");
    
    EseLuaEngine *engine = create_test_engine();
    EseEntity *entity = entity_create(engine);

    TEST_ASSERT(entity_component_count(entity) == 0, "Entity should have no components");
    
    // Test adding a component
    EseEntityComponent *lua_comp = entity_component_lua_create(engine, NULL);
    const char *comp_id = entity_component_add(entity, lua_comp);
    
    TEST_ASSERT(entity_component_count(entity) == 1, "Entity should have one component");

    // Test removing the component
    bool remove_result = entity_component_remove(entity, comp_id);
    TEST_ASSERT(remove_result, "Component should be removed successfully");
    
    TEST_ASSERT(entity_component_count(entity) == 0, "Entity should have no components");

    // Test removing non-existent component
    remove_result = entity_component_remove(entity, "non_existent");
    TEST_ASSERT(!remove_result, "Removing non-existent component should fail");
    
    // Clean up
    entity_destroy(entity);
    lua_engine_destroy(engine);
    
    test_end("Entity Component Management");
}

// Test entity tags
static void test_entity_tags() {
    test_begin("Entity Tags");
    
    EseLuaEngine *engine = create_test_engine();
    EseEntity *entity = entity_create(engine);
    
    // Test adding a tag
    bool add_result = entity_add_tag(entity, "test_tag");
    TEST_ASSERT(add_result, "Tag should be added successfully");
    TEST_ASSERT(entity_has_tag(entity, "test_tag"), "Entity should have the tag");
    TEST_ASSERT(entity_has_tag(entity, "TEST_TAG"), "Entity should have the tag (case insensitive)");
    
    // Test adding duplicate tag
    add_result = entity_add_tag(entity, "test_tag");
    TEST_ASSERT(!add_result, "Adding duplicate tag should fail");
    
    // Test adding another tag
    add_result = entity_add_tag(entity, "another_tag");
    TEST_ASSERT(add_result, "Second tag should be added successfully");
    
    // Test removing a tag
    bool remove_result = entity_remove_tag(entity, "test_tag");
    TEST_ASSERT(remove_result, "Tag should be removed successfully");
    TEST_ASSERT(!entity_has_tag(entity, "test_tag"), "Entity should not have the removed tag");
    TEST_ASSERT(entity_has_tag(entity, "another_tag"), "Entity should still have the other tag");
    
    // Test removing non-existent tag
    remove_result = entity_remove_tag(entity, "non_existent");
    TEST_ASSERT(!remove_result, "Removing non-existent tag should fail");
    
    // Clean up
    entity_destroy(entity);
    lua_engine_destroy(engine);
    
    test_end("Entity Tags");
}

// Test entity Lua integration
static void test_entity_lua_integration() {
    test_begin("Entity Lua Integration");
    
    const char *script = 
    "function ENTITY:entity_update(delta_time)\n"
    "    self:add_tag('test_tag')\n"
    "end\n";
    
    EseLuaEngine *engine = create_test_engine();
    EseEntity *entity = entity_create(engine);

    // Test getting Lua reference
    int lua_ref = entity_get_lua_ref(entity);
    TEST_ASSERT(lua_ref != LUA_NOREF, "Entity should have a valid Lua reference");
    
    entity_destroy(entity);    
    lua_engine_destroy(engine);
    
    test_end("Entity Lua Integration");
}

// Test NULL pointer aborts
static void test_entity_null_pointer_aborts() {
    test_begin("Entity NULL Pointer Abort Tests");
    
    EseLuaEngine *engine = create_test_engine();
    
    EseEntity *entity = entity_create(engine);
        
    // Test that creation functions abort with NULL pointers
    TEST_ASSERT_ABORT(entity_create(NULL), "entity_create should abort with NULL engine");
    TEST_ASSERT_ABORT(entity_copy(NULL), "entity_copy should abort with NULL entity");
    TEST_ASSERT_ABORT(entity_destroy(NULL), "entity_destroy should abort with NULL entity");
    
    // Test that update functions abort with NULL pointers
    TEST_ASSERT_ABORT(entity_update(NULL, 0.016f), "entity_update should abort with NULL entity");
    TEST_ASSERT_ABORT(entity_run_function_with_args(NULL, "test", 0, NULL), "entity_run_function_with_args should abort with NULL entity");
    
    // Test that collision functions abort with NULL pointers
    TEST_ASSERT_ABORT(entity_check_collision_state(NULL, entity), "entity_check_collision_state should abort with NULL first entity");
    TEST_ASSERT_ABORT(entity_check_collision_state(entity, NULL), "entity_check_collision_state should abort with NULL second entity");
    TEST_ASSERT_ABORT(entity_process_collision_callbacks(NULL, entity, 0), "entity_process_collision_callbacks should abort with NULL first entity");
    TEST_ASSERT_ABORT(entity_process_collision_callbacks(entity, NULL, 0), "entity_process_collision_callbacks should abort with NULL second entity");
    TEST_ASSERT_ABORT(entity_detect_collision_rect(NULL, NULL), "entity_detect_collision_rect should abort with NULL entity");
    
    // Test that draw function aborts with NULL pointers
    TEST_ASSERT_ABORT(entity_draw(NULL, 0.0f, 0.0f, 800.0f, 600.0f, NULL, NULL, NULL), "entity_draw should abort with NULL entity");
    TEST_ASSERT_ABORT(entity_draw(entity, 0.0f, 0.0f, 800.0f, 600.0f, NULL, NULL, NULL), "entity_draw should abort with NULL texture callback");
    TEST_ASSERT_ABORT(entity_draw(entity, 0.0f, 0.0f, 800.0f, 600.0f, (EntityDrawTextureCallback)0x123, NULL, NULL), "entity_draw should abort with NULL rect callback");
    
    // Test that component management functions abort with NULL pointers
    TEST_ASSERT_ABORT(entity_component_add(NULL, NULL), "entity_component_add should abort with NULL entity");
    TEST_ASSERT_ABORT(entity_component_add(entity, NULL), "entity_component_add should abort with NULL component");
    TEST_ASSERT_ABORT(entity_component_remove(NULL, "test"), "entity_component_remove should abort with NULL entity");
    TEST_ASSERT_ABORT(entity_component_remove(entity, NULL), "entity_component_remove should abort with NULL id");
    
    // Test that property functions abort with NULL pointers
    TEST_ASSERT_ABORT(entity_add_prop(NULL, NULL), "entity_add_prop should abort with NULL entity");
    TEST_ASSERT_ABORT(entity_add_prop(entity, NULL), "entity_add_prop should abort with NULL value");
    
    // Test that tag functions abort with NULL pointers
    TEST_ASSERT_ABORT(entity_add_tag(NULL, "test"), "entity_add_tag should abort with NULL entity");
    TEST_ASSERT_ABORT(entity_add_tag(entity, NULL), "entity_add_tag should abort with NULL tag");
    TEST_ASSERT_ABORT(entity_remove_tag(NULL, "test"), "entity_remove_tag should abort with NULL entity");
    TEST_ASSERT_ABORT(entity_remove_tag(entity, NULL), "entity_remove_tag should abort with NULL tag");
    TEST_ASSERT_ABORT(entity_has_tag(NULL, "test"), "entity_has_tag should abort with NULL entity");
    TEST_ASSERT_ABORT(entity_has_tag(entity, NULL), "entity_has_tag should abort with NULL tag");
    
    // Test that collision bounds function aborts with NULL pointer
    TEST_ASSERT_ABORT(entity_get_collision_bounds(NULL, false), "entity_get_collision_bounds should abort with NULL entity");
    
    // Test that Lua reference function aborts with NULL pointer
    TEST_ASSERT_ABORT(entity_get_lua_ref(NULL), "entity_get_lua_ref should abort with NULL entity");
    
    entity_destroy(entity);
    lua_engine_destroy(engine);
    
    test_end("Entity NULL Pointer Abort Tests");
}

// Test entity dispatch functionality
static void test_entity_dispatch() {
    test_begin("Entity Dispatch");
    
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
    TEST_ASSERT(load_resultA, "Script A should load successfully");
    bool load_resultB = lua_engine_load_script_from_string(engine->lua_engine, scriptB, "test_entity_script_b", "ENTITY");
    TEST_ASSERT(load_resultB, "Script B should load successfully");
    
    if (load_resultA && load_resultB) {
        // Add script A to entity1
        EseEntityComponent *lua_compA = entity_component_lua_create(engine->lua_engine, "test_entity_script_a");
        entity_component_add(entity1, lua_compA);
        
        // Add script B to entity2
        EseEntityComponent *lua_compB = entity_component_lua_create(engine->lua_engine, "test_entity_script_b");
        entity_component_add(entity2, lua_compB);
        
        // Update entities to trigger the dispatch
        EseInputState input_state;
        memset(&input_state, 0, sizeof(EseInputState));
        engine_update(engine, 0.016f, &input_state);
        
        // Check that entity2 has the tags added by all dispatch calls
        TEST_ASSERT(entity_has_tag(entity2, "dispatched_tag"), "Entity2 should have the dispatched_tag from colon syntax");
        TEST_ASSERT(entity_has_tag(entity2, "dispatched_tag2"), "Entity2 should have the dispatched_tag2 from dot syntax");
        TEST_ASSERT(entity_has_tag(entity2, "dispatched_tag3_colon_arg"), "Entity2 should have the dispatched_tag3_colon_arg from colon syntax with argument");
        TEST_ASSERT(entity_has_tag(entity2, "dispatched_tag3_dot_arg"), "Entity2 should have the dispatched_tag3_dot_arg from dot syntax with argument");
    }
    
    // Clean up - entities are managed by the engine
    engine_destroy(engine);
    
    test_end("Entity Dispatch");
}
