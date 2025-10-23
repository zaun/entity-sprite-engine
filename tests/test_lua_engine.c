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

#include "../src/scripting/lua_engine.h"
#include "../src/scripting/lua_engine_private.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

/**
* Test function declarations
*/
static void test_engine_creation(void);
static void test_jit_functionality(void);
static void test_jit_script_loading(void);
static void test_basic_lua_functionality(void);
static void test_memory_management(void);
static void test_error_handling(void);
static void test_function_references(void);
static void test_script_instances(void);
static void test_lua_value_arguments(void);
static void test_timeout_and_limits(void);
static void test_sandbox_environment(void);
static void test_null_pointer_aborts(void);

/**
* Test suite setup and teardown
*/
static EseLuaEngine *g_engine = NULL;

void setUp(void) {
    g_engine = create_test_engine();
}

void tearDown(void) {
    lua_engine_destroy(g_engine);
}

// Test Lua script content for JIT testing
static const char* test_lua_script = 
"function TEST_MODULE:fibonacci(n)\n"
"    if n == nil or n <= 1 then\n"
"        return n or 0\n"
"    end\n"
"    return TEST_MODULE:fibonacci(n-1) + TEST_MODULE:fibonacci(n-2)\n"
"end\n"
"\n"
"function TEST_MODULE:test_math()\n"
"    local sum = 0\n"
"    for i = 1, 1000 do\n"
"        sum = sum + math.sin(i) * math.cos(i)\n"
"    end\n"
"    return sum\n"
"end\n"
"\n"
"function TEST_MODULE:test_loops()\n"
"    local result = 0\n"
"    for i = 1, 10000 do\n"
"        result = result + i\n"
"    end\n"
"    return result\n"
"end\n"
"\n";

/**
* Main test runner
*/
int main(void) {
    log_init();

    printf("\nEseLuaEngine Tests\n");
    printf("------------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_engine_creation);
    RUN_TEST(test_jit_functionality);
    RUN_TEST(test_jit_script_loading);
    RUN_TEST(test_basic_lua_functionality);
    RUN_TEST(test_memory_management);
    RUN_TEST(test_error_handling);
    RUN_TEST(test_function_references);
    RUN_TEST(test_script_instances);
    RUN_TEST(test_lua_value_arguments);
    RUN_TEST(test_timeout_and_limits);
    RUN_TEST(test_sandbox_environment);
    RUN_TEST(test_null_pointer_aborts);

    memory_manager.destroy();

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_engine_creation(void) {
    // Test engine creation
    EseLuaEngine* engine = lua_engine_create();
    
    TEST_ASSERT_NOT_NULL_MESSAGE(engine, "Engine should not be NULL");
    if (!engine) {
        return;
    }
    
    TEST_ASSERT_NOT_NULL_MESSAGE(engine->runtime, "Engine runtime should not be NULL");
    TEST_ASSERT_NOT_NULL_MESSAGE(engine->internal, "Engine internal should not be NULL");
    
    // Test engine destruction
    lua_engine_destroy(engine);
}

static void test_jit_functionality(void) {
    EseLuaEngine* engine = lua_engine_create();
    
    TEST_ASSERT_NOT_NULL_MESSAGE(engine, "Engine should not be NULL");
    if (!engine) {
        return;
    }
    
    // Check if JIT library is available
    lua_State* L = engine->runtime;
    
    // Test JIT library presence
    lua_getglobal(L, "jit");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "JIT library should be available as a table");
    
    // Test JIT version (safe to access)
    lua_getfield(L, -1, "version");
    TEST_ASSERT_TRUE_MESSAGE(lua_isstring(L, -1), "JIT version should be available");
    if (lua_isstring(L, -1)) {
        const char* version = lua_tostring(L, -1);
        TEST_ASSERT_NOT_NULL_MESSAGE(version, "JIT version string should not be NULL");
        // Log the version for debugging
        printf("✓ JIT Version: %s\n", version);
    }
    lua_pop(L, 1);
    
    // Test JIT OS (safe to access)
    lua_getfield(L, -1, "os");
    TEST_ASSERT_TRUE_MESSAGE(lua_isstring(L, -1), "JIT OS should be available");
    if (lua_isstring(L, -1)) {
        const char* os = lua_tostring(L, -1);
        TEST_ASSERT_NOT_NULL_MESSAGE(os, "JIT OS string should not be NULL");
        // Log the OS for debugging
        printf("✓ JIT OS: %s\n", os);
    }
    lua_pop(L, 1);
    
    // Test JIT architecture (safe to access)
    lua_getfield(L, -1, "arch");
    TEST_ASSERT_TRUE_MESSAGE(lua_isstring(L, -1), "JIT architecture should be available");
    if (lua_isstring(L, -1)) {
        const char* arch = lua_tostring(L, -1);
        TEST_ASSERT_NOT_NULL_MESSAGE(arch, "JIT architecture string should not be NULL");
        // Log the architecture for debugging
        printf("✓ JIT Architecture: %s\n", arch);
    }
    lua_pop(L, 1);
    
    // Test JIT status function (with proper error handling)
    lua_getfield(L, -1, "status");
    if (lua_isfunction(L, -1)) {
        int call_result = lua_pcall(L, 0, 1, 0);
        TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, call_result, "JIT status function should execute successfully");
        if (call_result == LUA_OK) {
            if (lua_isstring(L, -1)) {
                const char* status = lua_tostring(L, -1);
                TEST_ASSERT_NOT_NULL_MESSAGE(status, "JIT status string should not be NULL");
                printf("✓ JIT Status: %s\n", status);
                
                // Check if JIT compiled any traces
                if (strstr(status, "TRACE") || strstr(status, "trace")) {
                    printf("✓ JIT compilation detected in status\n");
                }
            } else if (lua_isboolean(L, -1)) {
                bool status_bool = lua_toboolean(L, -1);
                printf("✓ JIT Status: %s (boolean)\n", status_bool ? "true" : "false");
                
                if (status_bool) {
                    printf("✓ JIT is enabled and active\n");
                } else {
                    printf("ℹ JIT is disabled or inactive\n");
                }
            } else {
                printf("ℹ JIT Status returned unexpected type: %s\n", lua_typename(L, lua_type(L, -1)));
            }
        } else {
            const char* error_msg = lua_tostring(L, -1);
            printf("ℹ JIT Status function call failed: %s\n", error_msg ? error_msg : "unknown error");
        }
        lua_pop(L, 1);
    } else {
        printf("ℹ JIT status field is not a function (type: %s)\n", lua_typename(L, lua_type(L, -1)));
    }
    lua_pop(L, 1); // Pop status field
    
    lua_pop(L, 1); // Pop JIT table
    
    lua_engine_destroy(engine);
}

static void test_jit_script_loading(void) {
    EseLuaEngine* engine = lua_engine_create();
    
    TEST_ASSERT_NOT_NULL_MESSAGE(engine, "Engine should not be NULL");
    if (!engine) {
        return;
    }
    
    // Load the script
    bool load_result = lua_engine_load_script_from_string(engine, test_lua_script, "test_lua_script", "TEST_MODULE");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Script should load successfully");
    
    if (load_result) {
        // Create an instance of the script
        int instance_ref = lua_engine_instance_script(engine, "test_lua_script");
        TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, instance_ref, "Script instance should be created successfully");
        
        if (instance_ref > 0) {
            // Create a dummy self object for function calls
            lua_State* L = engine->runtime;
            lua_newtable(L);  // Create empty table as dummy self
            int dummy_self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            
            // Call test_loops function (this should trigger JIT compilation)
            bool exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "test_loops", 0, NULL, NULL);
            TEST_ASSERT_TRUE_MESSAGE(exec_result, "test_loops function should execute successfully");
            
            // Test calling a function to trigger JIT compilation
            EseLuaValue* fibonacci_arg = lua_value_create_number("n", 10.0);
            EseLuaValue fibonacci_args[] = { *fibonacci_arg };
            
            // Call fibonacci function (this should also trigger JIT compilation)
            exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "fibonacci", 1, fibonacci_args, NULL);
            TEST_ASSERT_TRUE_MESSAGE(exec_result, "fibonacci function should execute successfully");
            
            // Call test_math function
            exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "test_math", 0, NULL, NULL);
            TEST_ASSERT_TRUE_MESSAGE(exec_result, "test_math function should execute successfully");
            
            // Check JIT compilation status after running functions
            lua_getglobal(L, "jit");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "status");
                if (lua_isfunction(L, -1)) {
                    int call_result = lua_pcall(L, 0, 1, 0);
                    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, call_result, "JIT status should execute successfully");
                    if (call_result == LUA_OK) {
                        const char* status = lua_tostring(L, -1);
                        if (status) {
                            printf("✓ JIT Status after script execution: %s\n", status);
                            
                            // Check if JIT compiled any traces
                            if (strstr(status, "TRACE") || strstr(status, "trace")) {
                                printf("✓ JIT compilation detected in status\n");
                            } else {
                                printf("ℹ JIT status shows: %s\n", status);
                            }
                        } else {
                            printf("ℹ JIT Status returned non-string value\n");
                        }
                    }
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
            
            // Clean up dummy self, lua value, and instance
            lua_value_destroy(fibonacci_arg);
            luaL_unref(L, LUA_REGISTRYINDEX, dummy_self_ref);
            lua_engine_instance_remove(engine, instance_ref);
        }
    }
    
    // Clean up
    lua_engine_destroy(engine);
}

static void test_basic_lua_functionality(void) {
    EseLuaEngine* engine = lua_engine_create();
    
    TEST_ASSERT_NOT_NULL_MESSAGE(engine, "Engine should not be NULL");
    if (!engine) {
        return;
    }
    
    lua_State* L = engine->runtime;
    
    // Test basic math operations
    lua_getglobal(L, "math");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "Math library should be available as a table");
    
    lua_getfield(L, -1, "sin");
    TEST_ASSERT_TRUE_MESSAGE(lua_isfunction(L, -1), "math.sin function should be available");
    
    lua_pushnumber(L, 0.0);
    int result = lua_pcall(L, 1, 1, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, result, "Basic math operation should succeed");
    
    if (result == LUA_OK) {
        double value = lua_tonumber(L, -1);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0, value, "sin(0) should equal 0");
        lua_pop(L, 1);
    }
    
    lua_pop(L, 1); // pop math table
    
    // Test string operations
    lua_getglobal(L, "string");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(L, -1), "String library should be available as a table");
    
    lua_getfield(L, -1, "upper");
    TEST_ASSERT_TRUE_MESSAGE(lua_isfunction(L, -1), "string.upper function should be available");
    
    lua_pushstring(L, "hello");
    int string_result = lua_pcall(L, 1, 1, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, string_result, "String operation should succeed");
    
    if (string_result == LUA_OK) {
        const char* value = lua_tostring(L, -1);
        TEST_ASSERT_EQUAL_STRING_MESSAGE("HELLO", value, "String upper should work");
        lua_pop(L, 1);
    }
    
    lua_pop(L, 1); // pop string table
    
    lua_engine_destroy(engine);
}

static void test_memory_management(void) {
    EseLuaEngine* engine = lua_engine_create();
    
    TEST_ASSERT_NOT_NULL_MESSAGE(engine, "Engine should not be NULL");
    if (!engine) {
        return;
    }
    
    // Test garbage collection
    lua_engine_gc(engine);
    
    // Test global lock
    lua_engine_global_lock(engine);
    
    lua_engine_destroy(engine);
}

static void test_error_handling(void) {
    EseLuaEngine* engine = lua_engine_create();
    
    TEST_ASSERT_NOT_NULL_MESSAGE(engine, "Engine should not be NULL");
    if (!engine) {
        return;
    }
    
    lua_State* L = engine->runtime;
    
    // Test invalid Lua code
    int result = luaL_loadstring(L, "invalid lua code here");
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, result, "Invalid Lua code should fail to load");
    
    if (result != LUA_OK) {
        const char* error_msg = lua_tostring(L, -1);
        TEST_ASSERT_NOT_NULL_MESSAGE(error_msg, "Error message should be available");
        lua_pop(L, 1);
    }
    
    // Test calling non-existent function
    lua_getglobal(L, "nonexistent_function");
    TEST_ASSERT_TRUE_MESSAGE(lua_isnil(L, -1), "Non-existent function should return nil");
    lua_pop(L, 1);
    
    lua_engine_destroy(engine);
}

static void test_function_references(void) {
    EseLuaEngine* engine = lua_engine_create();
    
    TEST_ASSERT_NOT_NULL_MESSAGE(engine, "Engine should not be NULL");
    if (!engine) {
        return;
    }
    
    // Load a simple script
    const char* simple_script = 
        "function TEST_MODULE:add(a)\n"
        "    return a + 5\n"
        "end\n"
        "function TEST_MODULE:multiply(a)\n"
        "    return a * 3\n"
        "end\n";
    
    bool load_result = lua_engine_load_script_from_string(engine, simple_script, "simple_script", "TEST_MODULE");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Simple script should load successfully");
    
    if (load_result) {
        // Create an instance
        int instance_ref = lua_engine_instance_script(engine, "simple_script");
        TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, instance_ref, "Script instance should be created successfully");
        
        if (instance_ref > 0) {
            lua_State* L = engine->runtime;
            lua_newtable(L);
            int dummy_self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            
            // Test add function with single argument
            EseLuaValue* arg1 = lua_value_create_number("a", 5.0);
            EseLuaValue args1[] = { *arg1 };  // Create array of values, not pointers
            bool exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "add", 1, args1, NULL);
            TEST_ASSERT_TRUE_MESSAGE(exec_result, "add function should execute successfully");
            
            // Test multiply function with single argument
            EseLuaValue* arg2 = lua_value_create_number("a", 3.0); // Changed to "a" to match function signature
            EseLuaValue args2[] = { *arg2 };  // Create array of values, not pointers
            exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "multiply", 1, args2, NULL);
            TEST_ASSERT_TRUE_MESSAGE(exec_result, "multiply function should execute successfully");
            
            // Clean up AFTER all function calls complete
            lua_value_destroy(arg1);
            lua_value_destroy(arg2);
            luaL_unref(L, LUA_REGISTRYINDEX, dummy_self_ref);
            lua_engine_instance_remove(engine, instance_ref);
        }
    }
    
    lua_engine_destroy(engine);
}

static void test_script_instances(void) {
    EseLuaEngine* engine = lua_engine_create();
    
    TEST_ASSERT_NOT_NULL_MESSAGE(engine, "Engine should not be NULL");
    if (!engine) {
        return;
    }
    
    // Load a script
    const char* instance_script = 
        "function TEST_MODULE:get_id()\n"
        "    return 'instance_script'\n"
        "end\n";
    
    bool load_result = lua_engine_load_script_from_string(engine, instance_script, "instance_script", "TEST_MODULE");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Instance script should load successfully");
    
    if (load_result) {
        // Create multiple instances
        int instance1 = lua_engine_instance_script(engine, "instance_script");
        int instance2 = lua_engine_instance_script(engine, "instance_script");
        int instance3 = lua_engine_instance_script(engine, "instance_script");
        
        TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, instance1, "First instance should be created successfully");
        TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, instance2, "Second instance should be created successfully");
        TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, instance3, "Third instance should be created successfully");
        TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(instance1, instance2, "Instances should have different references");
        TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(instance2, instance3, "Instances should have different references");
        TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(instance1, instance3, "Instances should have different references");
        
        // Test that all instances work
        lua_State* L = engine->runtime;
        lua_newtable(L);
        int dummy_self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        
        bool exec_result = lua_engine_run_function(engine, instance1, dummy_self_ref, "get_id", 0, NULL, NULL);
        TEST_ASSERT_TRUE_MESSAGE(exec_result, "First instance should execute successfully");
        
        exec_result = lua_engine_run_function(engine, instance2, dummy_self_ref, "get_id", 0, NULL, NULL);
        TEST_ASSERT_TRUE_MESSAGE(exec_result, "Second instance should execute successfully");
        
        exec_result = lua_engine_run_function(engine, instance3, dummy_self_ref, "get_id", 0, NULL, NULL);
        TEST_ASSERT_TRUE_MESSAGE(exec_result, "Third instance should execute successfully");
        
        // Remove instances in reverse order
        lua_engine_instance_remove(engine, instance3);
        lua_engine_instance_remove(engine, instance2);
        lua_engine_instance_remove(engine, instance1);
        
        luaL_unref(L, LUA_REGISTRYINDEX, dummy_self_ref);
    }
    
    lua_engine_destroy(engine);
}

static void test_lua_value_arguments(void) {
    EseLuaEngine* engine = lua_engine_create();
    
    TEST_ASSERT_NOT_NULL_MESSAGE(engine, "Engine should not be NULL");
    if (!engine) {
        return;
    }
    
    // Load a script that tests different argument types
    const char* arg_test_script = 
        "function TEST_MODULE:test_args(arg)\n"
        "    if arg == nil then\n"
        "        return true\n"
        "    elseif type(arg) == 'boolean' and arg == true then\n"
        "        return true\n"
        "    elseif type(arg) == 'number' and arg == 42.5 then\n"
        "        return true\n"
        "    elseif type(arg) == 'string' and arg == 'hello' then\n"
        "        return true\n"
        "    elseif type(arg) == 'table' then\n"
        "        return true\n"
        "    else\n"
        "        return false\n"
        "    end\n"
        "end\n";
    
    bool load_result = lua_engine_load_script_from_string(engine, arg_test_script, "arg_test_script", "TEST_MODULE");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Argument test script should load successfully");
    
    if (load_result) {
        int instance_ref = lua_engine_instance_script(engine, "arg_test_script");
        TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, instance_ref, "Script instance should be created successfully");
        
        if (instance_ref > 0) {
            lua_State* L = engine->runtime;
            lua_newtable(L);
            int dummy_self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            
            // Create different types of arguments
            EseLuaValue* nil_arg = lua_value_create_nil("nil_val");
            EseLuaValue* bool_arg = lua_value_create_bool("bool_val", true);
            EseLuaValue* num_arg = lua_value_create_number("num_val", 42.5);
            EseLuaValue* str_arg = lua_value_create_string("str_val", "hello");
            EseLuaValue* table_arg = lua_value_create_table("table_val");
            
            // Test function execution with single arguments
            EseLuaValue nil_args[] = { *nil_arg };
            bool exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "test_args", 1, nil_args, NULL);
            TEST_ASSERT_TRUE_MESSAGE(exec_result, "Function with nil argument should execute successfully");
            
            EseLuaValue bool_args[] = { *bool_arg };
            exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "test_args", 1, bool_args, NULL);
            TEST_ASSERT_TRUE_MESSAGE(exec_result, "Function with bool argument should execute successfully");
            
            EseLuaValue num_args[] = { *num_arg };
            exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "test_args", 1, num_args, NULL);
            TEST_ASSERT_TRUE_MESSAGE(exec_result, "Function with number argument should execute successfully");
            
            EseLuaValue str_args[] = { *str_arg };
            exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "test_args", 1, str_args, NULL);
            TEST_ASSERT_TRUE_MESSAGE(exec_result, "Function with string argument should execute successfully");
            
            EseLuaValue table_args[] = { *table_arg };
            exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "test_args", 1, table_args, NULL);
            TEST_ASSERT_TRUE_MESSAGE(exec_result, "Function with table argument should execute successfully");
            
            // Clean up
            lua_value_destroy(nil_arg);
            lua_value_destroy(bool_arg);
            lua_value_destroy(num_arg);
            lua_value_destroy(str_arg);
            lua_value_destroy(table_arg);
            
            luaL_unref(L, LUA_REGISTRYINDEX, dummy_self_ref);
            lua_engine_instance_remove(engine, instance_ref);
        }
    }
    
    lua_engine_destroy(engine);
}

static void test_timeout_and_limits(void) {
    EseLuaEngine* engine = lua_engine_create();
    
    TEST_ASSERT_NOT_NULL_MESSAGE(engine, "Engine should not be NULL");
    if (!engine) {
        return;
    }
    
    // Load a script that tests limits safely
    const char* limit_test_script = 
        "function TEST_MODULE:simple_function()\n"
        "    return 42\n"
        "end\n"
        "function TEST_MODULE:loop_function()\n"
        "    local sum = 0\n"
        "    for i = 1, 1000 do\n"
        "        sum = sum + i\n"
        "    end\n"
        "    return sum\n"
        "end\n"
        "function TEST_MODULE:recursive_function(n)\n"
        "    if n == nil or n <= 1 then\n"
        "        return 1\n"
        "    end\n"
        "    return n + TEST_MODULE:recursive_function(n - 1)\n"
        "end\n";
    
    bool load_result = lua_engine_load_script_from_string(engine, limit_test_script, "limit_test_script", "TEST_MODULE");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Limit test script should load successfully");
    
    if (load_result) {
        int instance_ref = lua_engine_instance_script(engine, "limit_test_script");
        TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, instance_ref, "Script instance should be created successfully");
        
        if (instance_ref > 0) {
            lua_State* L = engine->runtime;
            lua_newtable(L);
            int dummy_self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            
            // Test simple function (should work)
            bool exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "simple_function", 0, NULL, NULL);
            TEST_ASSERT_TRUE_MESSAGE(exec_result, "Simple function should execute successfully");
            
            // Test loop function (should work and be reasonably fast)
            exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "loop_function", 0, NULL, NULL);
            TEST_ASSERT_TRUE_MESSAGE(exec_result, "Loop function should execute successfully");
            
            // Test recursive function with reasonable depth (should work)
            EseLuaValue* arg = lua_value_create_number("n", 10.0);
            EseLuaValue recursive_args[] = { *arg };
            exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "recursive_function", 1, recursive_args, NULL);
            TEST_ASSERT_TRUE_MESSAGE(exec_result, "Recursive function should execute successfully");
            
            // Clean up
            lua_value_destroy(arg);
            luaL_unref(L, LUA_REGISTRYINDEX, dummy_self_ref);
            lua_engine_instance_remove(engine, instance_ref);
        }
    }
    
    lua_engine_destroy(engine);
}

static void test_sandbox_environment(void) {
    EseLuaEngine* engine = lua_engine_create();
    
    TEST_ASSERT_NOT_NULL_MESSAGE(engine, "Engine should not be NULL");
    if (!engine) {
        return;
    }
    
    // Load a script that tries to access restricted functionality
    const char* sandbox_test_script = 
        "function TEST_MODULE:test_sandbox()\n"
        "    -- Try to access os.execute (should be restricted)\n"
        "    if os and os.execute then\n"
        "        return 'os.execute available'\n"
        "    else\n"
        "        return 'os.execute restricted'\n"
        "    end\n"
        "end\n"
        "function TEST_MODULE:test_globals()\n"
        "    -- Check what globals are available\n"
        "    local count = 0\n"
        "    for k,v in pairs(_G) do\n"
        "        count = count + 1\n"
        "    end\n"
        "    return count\n"
        "end\n";
    
    bool load_result = lua_engine_load_script_from_string(engine, sandbox_test_script, "sandbox_test_script", "TEST_MODULE");
    TEST_ASSERT_TRUE_MESSAGE(load_result, "Sandbox test script should load successfully");
    
    if (load_result) {
        int instance_ref = lua_engine_instance_script(engine, "sandbox_test_script");
        TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, instance_ref, "Script instance should be created successfully");
        
        if (instance_ref > 0) {
            lua_State* L = engine->runtime;
            lua_newtable(L);
            int dummy_self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            
            // Test sandbox restrictions
            bool exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "test_sandbox", 0, NULL, NULL);
            TEST_ASSERT_TRUE_MESSAGE(exec_result, "Sandbox test function should execute successfully");
            
            // Test global access
            exec_result = lua_engine_run_function(engine, instance_ref, dummy_self_ref, "test_globals", 0, NULL, NULL);
            TEST_ASSERT_TRUE_MESSAGE(exec_result, "Global test function should execute successfully");
            
            luaL_unref(L, LUA_REGISTRYINDEX, dummy_self_ref);
            lua_engine_instance_remove(engine, instance_ref);
        }
    }
    
    lua_engine_destroy(engine);
}

static void test_null_pointer_aborts(void) {
    EseLuaEngine *engine = lua_engine_create();
    
    TEST_ASSERT_NOT_NULL_MESSAGE(engine, "Engine should be created for NULL pointer abort tests");
    
    if (engine) {
        // Test that engine destruction aborts with NULL
        TEST_ASSERT_DEATH(lua_engine_destroy(NULL), "lua_engine_destroy should abort with NULL engine");
        
        // Test that global lock aborts with NULL
        TEST_ASSERT_DEATH(lua_engine_global_lock(NULL), "lua_engine_global_lock should abort with NULL engine");
        
        // Test that garbage collection aborts with NULL
        TEST_ASSERT_DEATH(lua_engine_gc(NULL), "lua_engine_gc should abort with NULL engine");
        
        // Test that registry key functions abort with NULL Lua state
        TEST_ASSERT_DEATH(lua_engine_add_registry_key(NULL, (void*)0x123, (void*)0x456), "lua_engine_add_registry_key should abort with NULL Lua state");
        TEST_ASSERT_DEATH(lua_engine_get_registry_key(NULL, (void*)0x123), "lua_engine_get_registry_key should abort with NULL Lua state");
        TEST_ASSERT_DEATH(lua_engine_remove_registry_key(NULL, (void*)0x123), "lua_engine_remove_registry_key should abort with NULL Lua state");
        
        // Test that add function aborts with NULL parameters
        TEST_ASSERT_DEATH(lua_engine_add_function(NULL, "test", NULL), "lua_engine_add_function should abort with NULL engine");
        TEST_ASSERT_DEATH(lua_engine_add_function(engine, NULL, NULL), "lua_engine_add_function should abort with NULL function_name");
        TEST_ASSERT_DEATH(lua_engine_add_function(engine, "test", NULL), "lua_engine_add_function should abort with NULL func");
        
        // Test that add global aborts with NULL parameters
        TEST_ASSERT_DEATH(lua_engine_add_global(NULL, "test", 1), "lua_engine_add_global should abort with NULL engine");
        TEST_ASSERT_DEATH(lua_engine_add_global(engine, NULL, 1), "lua_engine_add_global should abort with NULL global_name");
        
        // Test that load script aborts with NULL parameters
        TEST_ASSERT_DEATH(lua_engine_load_script(NULL, "test.lua", "TEST"), "lua_engine_load_script should abort with NULL engine");
        TEST_ASSERT_DEATH(lua_engine_load_script(engine, NULL, "TEST"), "lua_engine_load_script should abort with NULL filename");
        TEST_ASSERT_DEATH(lua_engine_load_script(engine, "test.lua", NULL), "lua_engine_load_script should abort with NULL module_name");
        
        // Test that load script from string aborts with NULL parameters
        TEST_ASSERT_DEATH(lua_engine_load_script_from_string(NULL, "test", "test", "TEST"), "lua_engine_load_script_from_string should abort with NULL engine");
        TEST_ASSERT_DEATH(lua_engine_load_script_from_string(engine, NULL, "test", "TEST"), "lua_engine_load_script_from_string should abort with NULL script");
        TEST_ASSERT_DEATH(lua_engine_load_script_from_string(engine, "test", NULL, "TEST"), "lua_engine_load_script_from_string should abort with NULL name");
        TEST_ASSERT_DEATH(lua_engine_load_script_from_string(engine, "test", "test", NULL), "lua_engine_load_script_from_string should abort with NULL module_name");
        
        // Test that instance script aborts with NULL parameters
        TEST_ASSERT_DEATH(lua_engine_instance_script(NULL, "test"), "lua_engine_instance_script should abort with NULL engine");
        TEST_ASSERT_DEATH(lua_engine_instance_script(engine, NULL), "lua_engine_instance_script should abort with NULL name");
        
        // Test that instance remove aborts with NULL engine
        TEST_ASSERT_DEATH(lua_engine_instance_remove(NULL, 1), "lua_engine_instance_remove should abort with NULL engine");
        
        // Test that run function ref aborts with NULL engine
        TEST_ASSERT_DEATH(lua_engine_run_function_ref(NULL, 1, 1, 0, NULL, NULL), "lua_engine_run_function_ref should abort with NULL engine");
        
        // Test that run function aborts with NULL parameters
        TEST_ASSERT_DEATH(lua_engine_run_function(NULL, 1, 1, NULL, 0, NULL, NULL), "lua_engine_run_function should abort with NULL engine");
        TEST_ASSERT_DEATH(lua_engine_run_function(engine, 1, 1, NULL, 0, NULL, NULL), "lua_engine_run_function should abort with NULL func_name");
        
        // Test that lua_isinteger_lj aborts with NULL Lua state
        TEST_ASSERT_DEATH(lua_isinteger_lj(NULL, 1), "lua_isinteger_lj should abort with NULL Lua state");
        
        // Test that lua_getextraspace_lj aborts with NULL Lua state
        TEST_ASSERT_DEATH(lua_getextraspace_lj(NULL), "lua_getextraspace_lj should abort with NULL Lua state");
        
        lua_engine_destroy(engine);
    }
}
