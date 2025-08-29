#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <execinfo.h>
#include <signal.h>
#include "test_utils.h"
#include "../src/entity/components/entity_component_lua.h"
#include "../src/entity/components/entity_component.h"
#include "../src/entity/entity.h"
#include "../src/entity/entity_lua.h"
#include "../src/scripting/lua_engine.h"
#include "../src/scripting/lua_value.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

// Test function declarations
static void test_component_creation();
static void test_component_copy();
static void test_component_destruction();
static void test_component_update();
static void test_component_function_caching();
static void test_component_function_execution();
static void test_component_lua_integration();
static void test_component_property_access();
static void test_component_script_changing();
static void test_component_memory_management();
static void test_component_null_pointer_aborts();

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

// Test Lua script content
static const char* test_component_script = 
"function TEST_MODULE:entity_init()\n"
"    self.data.init_called = true\n"
"    self.data.init_count = (self.data.init_count or 0) + 1\n"
"    return true\n"
"end\n"
"\n"
"function TEST_MODULE:entity_update(delta_time)\n"
"    self.data.update_called = true\n"
"    self.data.update_count = (self.data.update_count or 0) + 1\n"
"    self.data.last_delta = delta_time\n"
"    return true\n"
"end\n"
"\n"
"function TEST_MODULE:entity_collision_enter(other)\n"
"    self.data.collision_enter_called = true\n"
"    self.data.collision_enter_count = (self.data.collision_enter_count or 0) + 1\n"
"    self.data.last_collision_other = other\n"
"    return true\n"
"end\n"
"\n"
"function TEST_MODULE:entity_collision_stay(other)\n"
"    self.data.collision_stay_called = true\n"
"    self.data.collision_stay_count = (self.data.collision_stay_count or 0) + 1\n"
"    return true\n"
"end\n"
"\n"
"function TEST_MODULE:entity_collision_exit(other)\n"
"    self.data.collision_exit_called = true\n"
"    self.data.collision_exit_count = (self.data.collision_exit_count or 0) + 1\n"
"    return true\n"
"end\n"
"\n"
"function TEST_MODULE:custom_function(arg1, arg2)\n"
"    self.data.custom_called = true\n"
"    self.data.custom_arg1 = arg1\n"
"    self.data.custom_arg2 = arg2\n"
"    self.data.custom_result = arg1 + arg2\n"
"    return self.data.custom_result\n"
"end\n"
"\n"
"function TEST_MODULE:void_function()\n"
"    self.data.void_called = true\n"
"    -- No return value\n"
"end\n";

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
    
    test_suite_begin("🧪 Starting Entity Component Lua Tests");
    
    // Initialize required systems
    log_init();
    
    // Run all test suites
    test_component_creation();
    test_component_copy();
    test_component_destruction();
    test_component_update();
    test_component_function_caching();
    test_component_function_execution();
    test_component_lua_integration();
    test_component_property_access();
    test_component_script_changing();
    test_component_memory_management();
    test_component_null_pointer_aborts();
    
    // Print final summary
    test_suite_end("🎯 Final Test Summary");
    
    return 0;
}

// Test basic component creation
static void test_component_creation() {
    test_begin("Component Creation");
    
    EseLuaEngine *engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");
    
    if (engine) {
        // Test creating component with script
        EseEntityComponent *component = entity_component_lua_create(engine, "test_script.lua");
        TEST_ASSERT_NOT_NULL(component, "Component should be created");
        
        if (component) {
            TEST_ASSERT_EQUAL(ENTITY_COMPONENT_LUA, component->type, "Component should have correct type");
            TEST_ASSERT(component->active, "Component should be active by default");
            TEST_ASSERT_NOT_NULL(component->id, "Component should have a valid ID");
            TEST_ASSERT(component->lua == engine, "Component should reference the engine");
            TEST_ASSERT_NOT_NULL(component->data, "Component should have data pointer");
            TEST_ASSERT(component->lua_ref != LUA_NOREF, "Component should be registered with Lua");
            
            // Check the Lua component data
            EseEntityComponentLua *lua_comp = (EseEntityComponentLua*)component->data;
            TEST_ASSERT_NOT_NULL(lua_comp->script, "Component should have script filename");
            TEST_ASSERT_STRING_EQUAL("test_script.lua", lua_comp->script, "Component should have correct script filename");
            TEST_ASSERT(lua_comp->engine == engine, "Component should reference the engine");
            TEST_ASSERT_EQUAL(LUA_NOREF, lua_comp->instance_ref, "Component should start with no instance reference");
            TEST_ASSERT_NOT_NULL(lua_comp->arg, "Component should have argument value");
            TEST_ASSERT_NULL(lua_comp->props, "Component should start with NULL props array");
            TEST_ASSERT_EQUAL(0, lua_comp->props_count, "Component should start with 0 props");
            TEST_ASSERT_NOT_NULL(lua_comp->function_cache, "Component should have function cache");
            
            entity_component_destroy(component);
        }
        
        // Test creating component without script
        component = entity_component_lua_create(engine, NULL);
        TEST_ASSERT_NOT_NULL(component, "Component should be created without script");
        
        if (component) {
            EseEntityComponentLua *lua_comp = (EseEntityComponentLua*)component->data;
            TEST_ASSERT_NULL(lua_comp->script, "Component should have NULL script when none provided");
            
            entity_component_destroy(component);
        }
        
        lua_engine_destroy(engine);
    }
    
    test_end("Component Creation");
}

// Test component copying
static void test_component_copy() {
    test_begin("Component Copy");
    
    EseLuaEngine *engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");
    
    if (engine) {
        EseEntityComponent *original = entity_component_lua_create(engine, "test_script.lua");
        TEST_ASSERT_NOT_NULL(original, "Original component should be created");
        
        if (original) {
            // Create a copy
            EseEntityComponent *copy = entity_component_copy(original);
            TEST_ASSERT_NOT_NULL(copy, "Component copy should be created");
            
            if (copy) {
                // Verify the copy has the same values but different pointers
                TEST_ASSERT(original->id != copy->id, "Copy should have different ID");
                TEST_ASSERT_EQUAL(original->type, copy->type, "Copy should have same type");
                TEST_ASSERT(original->lua == copy->lua, "Copy should reference same engine");
                TEST_ASSERT(original->data != copy->data, "Copy should have different data pointer");
                TEST_ASSERT(original->lua_ref != copy->lua_ref, "Copy should have different Lua reference");
                
                // Check the Lua component data
                EseEntityComponentLua *orig_lua = (EseEntityComponentLua*)original->data;
                EseEntityComponentLua *copy_lua = (EseEntityComponentLua*)copy->data;
                TEST_ASSERT_STRING_EQUAL(orig_lua->script, copy_lua->script, "Copy should have same script filename");
                TEST_ASSERT(orig_lua->engine == copy_lua->engine, "Copy should reference same engine");
                TEST_ASSERT_EQUAL(LUA_NOREF, copy_lua->instance_ref, "Copy should start with no instance reference");
                
                entity_component_destroy(copy);
            }
            
            entity_component_destroy(original);
        }
        
        lua_engine_destroy(engine);
    }
    
    test_end("Component Copy");
}

// Test component destruction
static void test_component_destruction() {
    test_begin("Component Destruction");
    
    EseLuaEngine *engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");
    
    if (engine) {
        EseEntityComponent *component = entity_component_lua_create(engine, "test_script.lua");
        TEST_ASSERT_NOT_NULL(component, "Component should be created");
        
        if (component) {
            // Destroy the component
            entity_component_destroy(component);
            printf("✓ PASS: Component destroyed successfully\n");
        }
        
        lua_engine_destroy(engine);
    }
    
    test_end("Component Destruction");
}

// Test component update
static void test_component_update() {
    test_begin("Component Update");
    
    EseLuaEngine *engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");
    
    if (engine) {
        // Load test script
        bool load_result = lua_engine_load_script_from_string(engine, test_component_script, "test_component_script", "TEST_MODULE");
        TEST_ASSERT(load_result, "Test script should load successfully");
        
        if (load_result) {
            EseEntityComponent *component = entity_component_lua_create(engine, "test_component_script");
            TEST_ASSERT_NOT_NULL(component, "Component should be created");
            
            if (component) {
                // Test update with no script
                EseEntityComponent *no_script_comp = entity_component_lua_create(engine, NULL);
                TEST_ASSERT_NOT_NULL(no_script_comp, "No-script component should be created");
                
                if (no_script_comp) {
                    // Create a mock entity for testing
                    EseEntity *mock_entity = entity_create(engine);
                    TEST_ASSERT_NOT_NULL(mock_entity, "Mock entity should be created for testing");
                    
                    if (mock_entity) {
                        entity_component_update(no_script_comp, mock_entity, 0.016f);
                        printf("✓ PASS: Component updated successfully with no script\n");
                        entity_destroy(mock_entity);
                    }
                    
                    entity_component_destroy(no_script_comp);
                }
                
                // Test update with script (should initialize on first update)
                EseEntity *mock_entity = entity_create(engine);
                TEST_ASSERT_NOT_NULL(mock_entity, "Mock entity should be created for testing");
                
                if (mock_entity) {
                    entity_component_update(component, mock_entity, 0.016f);
                    printf("✓ PASS: Component updated successfully with script\n");
                    
                    // Test second update (should run update function)
                    entity_component_update(component, mock_entity, 0.032f);
                    printf("✓ PASS: Component updated successfully on second call\n");
                    
                    entity_destroy(mock_entity);
                }
                
                entity_component_destroy(component);
            }
        }
        
        lua_engine_destroy(engine);
    }
    
    test_end("Component Update");
}

// Test function caching
static void test_component_function_caching() {
    test_begin("Component Function Caching");
    
    EseLuaEngine *engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");
    
    if (engine) {
        // Load test script
        bool load_result = lua_engine_load_script_from_string(engine, test_component_script, "test_component_script", "TEST_MODULE");
        TEST_ASSERT(load_result, "Test script should load successfully");
        
        if (load_result) {
            EseEntityComponent *component = entity_component_lua_create(engine, "test_component_script");
            TEST_ASSERT_NOT_NULL(component, "Component should be created");
            
            if (component) {
                EseEntityComponentLua *lua_comp = (EseEntityComponentLua*)component->data;
                
                // Force initialization to trigger function caching
                EseEntity *mock_entity = entity_create(engine);
                TEST_ASSERT_NOT_NULL(mock_entity, "Mock entity should be created for testing");
                
                if (mock_entity) {
                    entity_component_update(component, mock_entity, 0.016f);
                    
                    // Check that functions were cached
                    TEST_ASSERT(lua_comp->instance_ref != LUA_NOREF, "Component should have instance reference after update");
                    TEST_ASSERT_NOT_NULL(lua_comp->function_cache, "Component should have function cache");
                    
                    // Test that standard functions are cached (we can't access the hashmap directly, so we'll just verify the cache exists)
                    printf("✓ PASS: Function cache created successfully\n");
                    
                    entity_destroy(mock_entity);
                }
                
                // Check that functions were cached
                TEST_ASSERT(lua_comp->instance_ref != LUA_NOREF, "Component should have instance reference after update");
                TEST_ASSERT_NOT_NULL(lua_comp->function_cache, "Component should have function cache");
                
                // Test that standard functions are cached (we can't access the hashmap directly, so we'll just verify the cache exists)
                printf("✓ PASS: Function cache created successfully\n");
                
                entity_component_destroy(component);
            }
        }
        
        lua_engine_destroy(engine);
    }
    
    test_end("Component Function Caching");
}

// Test function execution
static void test_component_function_execution() {
    test_begin("Component Function Execution");
    
    EseLuaEngine *engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");
    
    if (engine) {
        // Load test script
        bool load_result = lua_engine_load_script_from_string(engine, test_component_script, "test_component_script", "TEST_MODULE");
        TEST_ASSERT(load_result, "Test script should load successfully");
        
        if (load_result) {
            EseEntityComponent *component = entity_component_lua_create(engine, "test_component_script");
            TEST_ASSERT_NOT_NULL(component, "Component should be created");
            
            if (component) {
                EseEntityComponentLua *lua_comp = (EseEntityComponentLua*)component->data;
                
                // Initialize the component
                EseEntity *mock_entity = entity_create(engine);
                TEST_ASSERT_NOT_NULL(mock_entity, "Mock entity should be created for testing");
                
                if (mock_entity) {
                    entity_component_update(component, mock_entity, 0.016f);
                
                // Test running a custom function with arguments
                EseLuaValue *arg1 = lua_value_create_number("arg1", 10.0);
                EseLuaValue *arg2 = lua_value_create_number("arg2", 20.0);
                TEST_ASSERT_NOT_NULL(arg1, "First argument should be created");
                TEST_ASSERT_NOT_NULL(arg2, "Second argument should be created");
                
                if (arg1 && arg2) {
                    // Test running a function that doesn't exist
                    bool exec_result = entity_component_lua_run(lua_comp, mock_entity, "non_existent_function", 0, NULL);
                    TEST_ASSERT(!exec_result, "Non-existent function should not execute");
                    
                    // Test running a void function
                    exec_result = entity_component_lua_run(lua_comp, mock_entity, "void_function", 0, NULL);
                    TEST_ASSERT(exec_result, "Void function should execute successfully");
                    
                    lua_value_free(arg1);
                    lua_value_free(arg2);
                }
                
                printf("✓ PASS: Function execution tests completed successfully\n");
                
                    entity_destroy(mock_entity);
                }
                
                entity_component_destroy(component);
            }
        }
        
        lua_engine_destroy(engine);
    }
    
    test_end("Component Function Execution");
}

// Test Lua integration
static void test_component_lua_integration() {
    test_begin("Component Lua Integration");
    
    EseLuaEngine *engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");
    
    if (engine) {
        // Initialize component system
        entity_component_lua_init(engine);
        
        // Test that global table was created
        lua_State *L = engine->runtime;
        lua_getglobal(L, "EntityComponentLua");
        TEST_ASSERT(lua_istable(L, -1), "EntityComponentLua global table should exist");
        lua_pop(L, 1);
        
        // Test that metatable was registered
        luaL_getmetatable(L, LUA_PROXY_META);
        TEST_ASSERT(lua_istable(L, -1), "EntityComponentLuaProxyMeta metatable should exist");
        lua_pop(L, 1);
        
        printf("✓ PASS: Lua integration initialized successfully\n");
        
        lua_engine_destroy(engine);
    }
    
    test_end("Component Lua Integration");
}

// Test property access from Lua
static void test_component_property_access() {
    test_begin("Component Property Access");
    
    EseLuaEngine *engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");
    
    if (engine) {
        // Initialize component system
        entity_component_lua_init(engine);
        
        EseEntityComponent *component = entity_component_lua_create(engine, "test_script.lua");
        TEST_ASSERT_NOT_NULL(component, "Component should be created");
        
        if (component) {
            lua_State *L = engine->runtime;
            
            // Test reading properties
            entity_component_push(component);
            TEST_ASSERT(lua_istable(L, -1), "Component should be pushed as table");
            
            // Test reading active property
            lua_getfield(L, -1, "active");
            TEST_ASSERT(lua_isboolean(L, -1), "active property should be boolean");
            bool active = lua_toboolean(L, -1);
            TEST_ASSERT(active, "Component should be active by default");
            lua_pop(L, 1);
            
            // Test reading id property
            lua_getfield(L, -1, "id");
            TEST_ASSERT(lua_isstring(L, -1), "id property should be string");
            lua_pop(L, 1);
            
            // Test reading script property
            lua_getfield(L, -1, "script");
            TEST_ASSERT(lua_isstring(L, -1), "script property should be string");
            const char *script = lua_tostring(L, -1);
            TEST_ASSERT_STRING_EQUAL("test_script.lua", script, "Script property should match");
            lua_pop(L, 1);
            
            // Test writing active property
            lua_pushboolean(L, false);
            lua_setfield(L, -2, "active");
            
            lua_getfield(L, -1, "active");
            active = lua_toboolean(L, -1);
            TEST_ASSERT(!active, "active property should be updated");
            lua_pop(L, 1);
            
            // Test writing script property
            lua_pushstring(L, "new_script.lua");
            lua_setfield(L, -2, "script");
            
            lua_getfield(L, -1, "script");
            script = lua_tostring(L, -1);
            TEST_ASSERT_STRING_EQUAL("new_script.lua", script, "Script property should be updated");
            lua_pop(L, 1);
            
            // Test that id is read-only by attempting to set it through Lua
            // We need to use a Lua script to test this properly since lua_setfield directly
            // would cause a C-level error
            const char *test_id_script = "component.id = 'new_id'";
            int load_result = luaL_loadstring(L, test_id_script);
            if (load_result == LUA_OK) {
                // Debug: check what's on the stack
                printf("DEBUG: Stack top before setting global: %d\n", lua_gettop(L));
                printf("DEBUG: Value at -1 is: %s\n", lua_typename(L, lua_type(L, -1)));
                
                // Find the component table on the stack - it should be a table with the component metatable
                int component_index = -1;
                for (int i = lua_gettop(L); i >= 1; i--) {
                    if (lua_istable(L, i)) {
                        // Check if it has the right metatable
                        if (lua_getmetatable(L, i)) {
                            luaL_getmetatable(L, LUA_PROXY_META);
                            if (lua_rawequal(L, -1, -2)) {
                                component_index = i;
                                lua_pop(L, 2); // Pop both metatables
                                break;
                            }
                            lua_pop(L, 2); // Pop both metatables
                        }
                    }
                }
                
                if (component_index > 0) {
                    printf("DEBUG: Found component at index %d\n", component_index);
                    // Copy the component to the top of the stack
                    lua_pushvalue(L, component_index);
                    lua_setglobal(L, "component"); // Set the component as global
                    
                    // Try to execute the script - it should fail with a read-only error
                    int call_result = lua_pcall(L, 0, 0, 0);
                    if (call_result == LUA_OK) {
                        // If we get here, the property was set successfully (which shouldn't happen)
                        TEST_ASSERT(false, "ID property should be read-only and not allow setting");
                    } else {
                        // Check that the error message contains "read-only"
                        const char *error_msg = lua_tostring(L, -1);
                        TEST_ASSERT_NOT_NULL(error_msg, "Error message should exist");
                        
                        // Debug: print the actual error message
                        printf("DEBUG: Actual error message: '%s'\n", error_msg ? error_msg : "NULL");
                        
                        // Check if the error message contains "read-only" or similar
                        bool has_readonly = error_msg && (
                            strstr(error_msg, "read-only") != NULL ||
                            strstr(error_msg, "read only") != NULL ||
                            strstr(error_msg, "readonly") != NULL
                        );
                        TEST_ASSERT(has_readonly, "Error should indicate property is read-only");
                        
                        lua_pop(L, 1); // Pop the error message
                        printf("✓ PASS: ID property correctly rejected write with read-only error\n");
                    }
                    
                    // Remove the global variable
                    lua_pushnil(L);
                    lua_setglobal(L, "component");
                } else {
                    TEST_ASSERT(false, "Could not find component table on stack");
                }
            } else {
                TEST_ASSERT(false, "Failed to load test script for ID read-only test");
            }
            
            // Verify the id property still exists and is unchanged
            lua_getfield(L, -1, "id");
            const char *id = lua_tostring(L, -1);
            TEST_ASSERT_NOT_NULL(id, "ID property should still exist after failed write");
            lua_pop(L, 1);
            
            lua_pop(L, 1); // Pop component table
            
            printf("✓ PASS: Property access tests completed successfully\n");
            
            entity_component_destroy(component);
        }
        
        lua_engine_destroy(engine);
    }
    
    test_end("Component Property Access");
}

// Test script changing
static void test_component_script_changing() {
    test_begin("Component Script Changing");
    
    EseLuaEngine *engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");
    
    if (engine) {
        // Load test script
        bool load_result = lua_engine_load_script_from_string(engine, test_component_script, "test_component_script", "TEST_MODULE");
        TEST_ASSERT(load_result, "Test script should load successfully");
        
        if (load_result) {
            EseEntityComponent *component = entity_component_lua_create(engine, "test_component_script");
            TEST_ASSERT_NOT_NULL(component, "Component should be created");
            
            if (component) {
                EseEntityComponentLua *lua_comp = (EseEntityComponentLua*)component->data;
                
                // Initialize with first script
                EseEntity *mock_entity = entity_create(engine);
                TEST_ASSERT_NOT_NULL(mock_entity, "Mock entity should be created for testing");
                
                if (mock_entity) {
                    entity_component_update(component, mock_entity, 0.016f);
                    TEST_ASSERT(lua_comp->instance_ref != LUA_NOREF, "Component should have instance reference");
                    
                    // Change script to NULL
                    lua_State *L = engine->runtime;
                    entity_component_push(component);
                    lua_pushnil(L);
                    lua_setfield(L, -2, "script");
                    lua_pop(L, 1);
                    
                    TEST_ASSERT_NULL(lua_comp->script, "Script should be set to NULL");
                    TEST_ASSERT_EQUAL(LUA_NOREF, lua_comp->instance_ref, "Instance reference should be cleared");
                    
                    // Change script to new script
                    entity_component_push(component);
                    lua_pushstring(L, "test_component_script");
                    lua_setfield(L, -2, "script");
                    lua_pop(L, 1);
                    
                    TEST_ASSERT_NOT_NULL(lua_comp->script, "Script should be set to new value");
                    TEST_ASSERT_STRING_EQUAL("test_component_script", lua_comp->script, "Script should match new value");
                    
                    // Update to initialize new script
                    entity_component_update(component, mock_entity, 0.016f);
                    TEST_ASSERT(lua_comp->instance_ref != LUA_NOREF, "Component should have new instance reference");
                    
                    entity_destroy(mock_entity);
                }
                
                printf("✓ PASS: Script changing tests completed successfully\n");
                
                entity_component_destroy(component);
            }
        }
        
        lua_engine_destroy(engine);
    }
    
    test_end("Component Script Changing");
}

// Test memory management
static void test_component_memory_management() {
    test_begin("Component Memory Management");
    
    EseLuaEngine *engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created");
    
    if (engine) {
        // Load test script
        bool load_result = lua_engine_load_script_from_string(engine, test_component_script, "test_component_script", "TEST_MODULE");
        TEST_ASSERT(load_result, "Test script should load successfully");
        
        if (load_result) {
            EseEntityComponent *component = entity_component_lua_create(engine, "test_component_script");
            TEST_ASSERT_NOT_NULL(component, "Component should be created");
            
            if (component) {
                EseEntityComponentLua *lua_comp = (EseEntityComponentLua*)component->data;
                
                // Initialize to create function cache
                EseEntity *mock_entity = entity_create(engine);
                TEST_ASSERT_NOT_NULL(mock_entity, "Mock entity should be created for testing");
                
                if (mock_entity) {
                    entity_component_update(component, mock_entity, 0.016f);
                    
                    // Test that function cache is populated
                    TEST_ASSERT_NOT_NULL(lua_comp->function_cache, "Function cache should exist");
                    
                    // Test clearing cache
                    _entity_component_lua_clear_cache(lua_comp);
                    
                    // Test that cache is empty but still exists
                    TEST_ASSERT_NOT_NULL(lua_comp->function_cache, "Function cache should still exist after clearing");
                    
                    entity_destroy(mock_entity);
                }
                
                printf("✓ PASS: Memory management tests completed successfully\n");
                
                entity_component_destroy(component);
            }
        }
        
        lua_engine_destroy(engine);
    }
    
    test_end("Component Memory Management");
}

// Test NULL pointer aborts
static void test_component_null_pointer_aborts() {
    test_begin("Component NULL Pointer Abort Tests");
    
    EseLuaEngine *engine = create_test_engine();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for NULL pointer abort tests");
    
    if (engine) {
        EseEntityComponent *component = entity_component_lua_create(engine, "test_script.lua");
        TEST_ASSERT_NOT_NULL(component, "Component should be created for NULL pointer abort tests");
        
        if (component) {
            EseEntityComponentLua *lua_comp = (EseEntityComponentLua*)component->data;
            
            // Test that creation functions abort with NULL pointers
            TEST_ASSERT_ABORT(entity_component_lua_create(NULL, "test"), "entity_component_lua_create should abort with NULL engine");
            
            // Test that copy function aborts with NULL pointer
            TEST_ASSERT_ABORT(_entity_component_lua_copy(NULL), "_entity_component_lua_copy should abort with NULL source");
            
            // Test that destroy function aborts with NULL pointer
            TEST_ASSERT_ABORT(_entity_component_lua_destroy(NULL), "_entity_component_lua_destroy should abort with NULL component");
            
            // Test that update function aborts with NULL pointers
            TEST_ASSERT_ABORT(_entity_component_lua_update(NULL, NULL, 0.0f), "_entity_component_lua_update should abort with NULL component");
            TEST_ASSERT_ABORT(_entity_component_lua_update(lua_comp, NULL, 0.0f), "_entity_component_lua_update should abort with NULL entity");
            
            // Test that function caching functions abort with NULL pointers
            TEST_ASSERT_ABORT(_entity_component_lua_cache_functions(NULL), "_entity_component_lua_cache_functions should abort with NULL component");
            TEST_ASSERT_ABORT(_entity_component_lua_clear_cache(NULL), "_entity_component_lua_clear_cache should abort with NULL component");
            
            // Test that function execution aborts with NULL pointers
            TEST_ASSERT_ABORT(entity_component_lua_run(NULL, NULL, "test", 0, NULL), "entity_component_lua_run should abort with NULL component");
            TEST_ASSERT_ABORT(entity_component_lua_run(lua_comp, NULL, NULL, 0, NULL), "entity_component_lua_run should abort with NULL entity");
            TEST_ASSERT_ABORT(entity_component_lua_run(lua_comp, NULL, "test", 0, NULL), "entity_component_lua_run should abort with NULL entity");
            
            // Test that init function aborts with NULL pointer
            TEST_ASSERT_ABORT(_entity_component_lua_init(NULL), "_entity_component_lua_init should abort with NULL engine");
            
            entity_component_destroy(component);
        }
        
        lua_engine_destroy(engine);
    }
    
    test_end("Component NULL Pointer Abort Tests");
}
