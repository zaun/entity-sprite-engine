/*
 * Test file for engine collision detection functionality
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

// Test function declarations
static void test_engine_collision_detection();

// Helper function to create and initialize engine
static EseLuaEngine* create_test_engine() {
    EseLuaEngine *engine = lua_engine_create();
    if (engine) {
        // Initialize required systems for testing
        log_init();
    }
    return engine;
}

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
    
    test_suite_begin("🧪 Starting Engine Collision Tests");
    
    // Initialize required systems
    log_init();
    
    // Run all test suites
    test_engine_collision_detection();
    
    // Print final summary
    test_suite_end("🎯 Final Test Summary");
    
    return 0;
}

// Test engine collision detection
static void test_engine_collision_detection() {
    test_begin("Engine Collision Detection");
    
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
    
    EseEngine *engine = engine_create("dummy_startup.lua");
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");
    
    EseLuaEngine *lua_engine = engine->lua_engine;
    TEST_ASSERT_NOT_NULL(lua_engine, "Lua engine should be created");
    
    bool load_result = lua_engine_load_script_from_string(lua_engine, script, "test_entity_script", "ENTITY");
    TEST_ASSERT(load_result, "Test script should load successfully");
    
    if (load_result) {
        EseEntity *entity1 = entity_create(lua_engine);
        EseEntity *entity2 = entity_create(lua_engine);

        EseEntityComponent *lua_comp1 = entity_component_lua_create(lua_engine, "test_entity_script");
        EseEntityComponent *lua_comp2 = entity_component_lua_create(lua_engine, "test_entity_script");

        entity_component_add(entity1, lua_comp1);
        entity_component_add(entity2, lua_comp2);

        EseEntityComponent *collider1 = entity_component_collider_create(lua_engine);
        EseEntityComponent *collider2 = entity_component_collider_create(lua_engine);

        entity_component_add(entity1, collider1);
        entity_component_add(entity2, collider2);

        EseRect *rect1 = rect_create(lua_engine);
        EseRect *rect2 = rect_create(lua_engine);

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

        // Add entities to engine
        engine_add_entity(engine, entity1);
        engine_add_entity(engine, entity2);

        // Create input state for engine_update
        EseInputState *input_state = input_state_create(lua_engine);
        TEST_ASSERT_NOT_NULL(input_state, "Input state should be created");

        // Test 1: No collisions - entities far apart
        entity_set_position(entity1, 0, 0);
        entity_set_position(entity2, 300, 0);
        
        engine_update(engine, 0.016f, input_state);
        
        TEST_ASSERT(!entity_has_tag(entity1, "enter"), "Entity should not have the enter tag after no collisions");
        TEST_ASSERT(!entity_has_tag(entity2, "enter"), "Entity should not have the enter tag after no collisions");
        TEST_ASSERT(!entity_has_tag(entity1, "stay"), "Entity should not have the stay tag after no collisions");
        TEST_ASSERT(!entity_has_tag(entity2, "stay"), "Entity should not have the stay tag after no collisions");
        TEST_ASSERT(!entity_has_tag(entity1, "exit"), "Entity should not have the exit tag after no collisions");
        TEST_ASSERT(!entity_has_tag(entity2, "exit"), "Entity should not have the exit tag after no collisions");

        // Test 2: Collision enter - entities overlapping
        entity_set_position(entity1, 150, 0);
        entity_set_position(entity2, 200, 0);
        
        engine_update(engine, 0.016f, input_state);
        
        TEST_ASSERT(entity_has_tag(entity1, "enter"), "Entity should have the enter tag after collision enter");
        TEST_ASSERT(entity_has_tag(entity2, "enter"), "Entity should have the enter tag after collision enter");
        TEST_ASSERT(!entity_has_tag(entity1, "stay"), "Entity should not have the stay tag after collision enter");
        TEST_ASSERT(!entity_has_tag(entity2, "stay"), "Entity should not have the stay tag after collision enter");
        TEST_ASSERT(!entity_has_tag(entity1, "exit"), "Entity should not have the exit tag after collision enter");
        TEST_ASSERT(!entity_has_tag(entity2, "exit"), "Entity should not have the exit tag after collision enter");

        // Test 3: Collision stay - entities still overlapping
        entity_set_position(entity1, 200, 0);
        entity_set_position(entity2, 200, 0);
        
        engine_update(engine, 0.016f, input_state);
        
        TEST_ASSERT(entity_has_tag(entity1, "enter"), "Entity should have the enter tag after collision stay");
        TEST_ASSERT(entity_has_tag(entity2, "enter"), "Entity should have the enter tag after collision stay");
        TEST_ASSERT(entity_has_tag(entity1, "stay"), "Entity should have the stay tag after collision stay");
        TEST_ASSERT(entity_has_tag(entity2, "stay"), "Entity should have the stay tag after collision stay");
        TEST_ASSERT(!entity_has_tag(entity1, "exit"), "Entity should not have the exit tag after collision stay");
        TEST_ASSERT(!entity_has_tag(entity2, "exit"), "Entity should not have the exit tag after collision stay");

        // Test 4: Collision exit - entities separated again
        entity_set_position(entity1, 301, 0);
        entity_set_position(entity2, 200, 0);
        
        engine_update(engine, 0.016f, input_state);
        
        TEST_ASSERT(entity_has_tag(entity1, "enter"), "Entity should have the enter tag after collision exit");
        TEST_ASSERT(entity_has_tag(entity2, "enter"), "Entity should have the enter tag after collision exit");
        TEST_ASSERT(entity_has_tag(entity1, "stay"), "Entity should have the stay tag after collision exit");
        TEST_ASSERT(entity_has_tag(entity2, "stay"), "Entity should have the stay tag after collision exit");
        TEST_ASSERT(entity_has_tag(entity1, "exit"), "Entity should have the exit tag after collision exit");
        TEST_ASSERT(entity_has_tag(entity2, "exit"), "Entity should have the exit tag after collision exit");

        // Clean up
        input_state_destroy(input_state);
    }
    
    engine_destroy(engine);
    
    test_end("Engine Collision Detection");
}
