#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <execinfo.h>
#include <signal.h>
#include <time.h>
#include "test_utils.h"

// Direct Lua includes - no src structures
#include "../src/vendor/lua/src/lua.h"
#include "../src/vendor/lua/src/lauxlib.h"
#include "../src/vendor/lua/src/lualib.h"
#include "../src/platform/time.h"

// Hook-related constants (copied from lua_engine_private.h)
static const char hook_key_sentinel = 0;
#define LUA_HOOK_KEY ((void*)&hook_key_sentinel)
#define LUA_HOOK_FRQ 1000

// Hook structure (copied from lua_engine_private.h)
typedef struct LuaFunctionHook {
    clock_t start_time;
    clock_t max_execution_time;
    size_t max_instruction_count;
    size_t instruction_count;
    size_t call_count;
} LuaFunctionHook;

// Global flag to enable/disable hooks
static bool g_include_hook = false;
static bool g_include_lookup = false;
static bool g_include_luavalue = false;

// Test function declarations
static void test_lua_error_handling();
static void test_lua_panic_behavior();
static void test_direct_lua_benchmarks();

// Helper function declarations
static int function_that_calls_luaL_error(lua_State *L);
static int panic_function(lua_State *L);

// Hook function (copied from lua_engine_private.c)
static void _lua_engine_function_hook(lua_State *L, lua_Debug *ar) {
    if (!g_include_hook) return;
    
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_HOOK_KEY);
    LuaFunctionHook *hook = (LuaFunctionHook *)lua_touserdata(L, -1);
    lua_pop(L, 1);

    if (!hook) {
        luaL_error(L, "Internal error: hook data missing");
    }

    clock_t current = clock();
    hook->instruction_count += LUA_HOOK_FRQ;
    hook->call_count++;

    if (hook->instruction_count > hook->max_instruction_count) {
        luaL_error(L, "Instruction count limit exceeded");
    }

    if (current - hook->start_time > hook->max_execution_time) {
        luaL_error(L, "Script execution timeout");
    }
}

// Function lookup simulation (copied from lua_engine_private.c)
static bool _simulate_engine_lookup(lua_State *L, int instance_ref, const char *func_name) {
    if (!g_include_lookup) return true;
    
    // Save the current stack top to restore later
    int stack_top = lua_gettop(L);
    
    // Push the instance table onto the stack
    lua_rawgeti(L, LUA_REGISTRYINDEX, instance_ref);

    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return false;
    }

    // Try to get the function from the instance table
    lua_getfield(L, -1, func_name);

    if (lua_isfunction(L, -1)) {
        lua_pop(L, 2); // pop function and instance table
        // Stack restored to original state
        return true;
    }

    // Not found in instance, try metatable's __index
    lua_pop(L, 1); // pop nil

    if (!lua_getmetatable(L, -1)) { // push metatable
        lua_pop(L, 1); // pop instance table
        return false;
    }

    lua_getfield(L, -1, "__index"); // push __index

    if (!lua_istable(L, -1)) {
        lua_pop(L, 2); // pop __index, metatable
        lua_pop(L, 1); // pop instance table
        return false;
    }

    lua_getfield(L, -1, func_name); // push method from __index

    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 3); // pop nil, __index, metatable
        lua_pop(L, 1); // pop instance table
        return false;
    }

    // Success: Stack is [instance][metatable][__index][function]
    // Pop everything to restore original stack state
    lua_pop(L, 4); // pop function, __index, metatable, and instance

    // Stack restored to original state
    return true;
}

// EseLuaValue simulation (simplified version)
typedef struct {
    enum { LUA_VAL_NIL, LUA_VAL_BOOL, LUA_VAL_NUMBER, LUA_VAL_STRING } type;
    union {
        bool boolean;
        double number;
        char *string;
    } value;
} SimulatedLuaValue;

// Simulate engine's argument conversion overhead
static void _simulate_luavalue_overhead(lua_State *L, int argc) {
    if (!g_include_luavalue) return;
    
    // Simulate creating EseLuaValue structures for arguments
    for (int i = 0; i < argc; i++) {
        // Simulate memory allocation for EseLuaValue
        SimulatedLuaValue *arg = malloc(sizeof(SimulatedLuaValue));
        arg->type = LUA_VAL_NUMBER;
        arg->value.number = (double)i;
        
        // Simulate the overhead without actually pushing to stack
        // This simulates the memory allocation/deallocation cost
        // and the type checking overhead of the real engine
        
        // Simulate cleanup
        free(arg);
    }
}

// Benchmark Lua script content (modified to work without self parameter and accept arguments)
static const char* benchmark_lua_script = 
"function TEST_MODULE.benchmark_function(num)\n"
"    local sum = 0\n"
"    for i = 1, 100 do\n"
"        sum = sum + i + (num or 0)\n"
"    end\n"
"    return sum\n"
"end\n"
"\n"
"function TEST_MODULE.benchmark_function_10(num)\n"
"    local sum = 0\n"
"    for i = 1, 1000 do\n"
"        sum = sum + i + (num or 0)\n"
"    end\n"
"    return sum\n"
"end\n"
"\n"
"function TEST_MODULE.benchmark_function_100(num)\n"
"    local sum = 0\n"
"    for i = 1, 10000 do\n"
"        sum = sum + i + (num or 0)\n"
"    end\n"
"    return sum\n"
"end\n"
"\n"
"function TEST_MODULE.benchmark_function_1000(num)\n"
"    local sum = 0\n"
"    for i = 1, 100000 do\n"
"        sum = sum + i + (num or 0)\n"
"    end\n"
"    return sum\n"
"end\n"
"\n";

// Mock segfault handler for debugging
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

int main(int argc, char *argv[]) {
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--include-hook") == 0) {
            g_include_hook = true;
            printf("🔧 Hook system ENABLED - will test with execution hooks\n");
        }
        if (strcmp(argv[i], "--include-lookup") == 0) {
            g_include_lookup = true;
            printf("🔧 Function Lookup System ENABLED - will simulate engine lookup overhead\n");
        }
        if (strcmp(argv[i], "--include-luavalue") == 0) {
            g_include_luavalue = true;
            printf("🔧 EseLuaValue System ENABLED - will simulate engine argument conversion overhead\n");
        }
    }
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_sigaction = segfault_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);

    printf("\n=== 🧪 Starting Lua-Only Tests ===\n");
    printf("Hook System: %s\n", g_include_hook ? "ENABLED" : "DISABLED");
    printf("Function Lookup System: %s\n", g_include_lookup ? "ENABLED" : "DISABLED");
    printf("EseLuaValue System: %s\n", g_include_luavalue ? "ENABLED" : "DISABLED");
    printf("\n");

    test_lua_error_handling();
    test_lua_panic_behavior();
    test_direct_lua_benchmarks();

    printf("\n=== 🧪 Lua-Only Tests Complete ===\n");
    printf("  Passed: %d\n", test_passed);
    printf("  Failed: %d\n", test_failed);
    printf("  Skipped: %d\n", test_skipped);
    printf("  Success rate: %.1f%%\n", 
           test_count > 0 ? (test_passed * 100.0) / test_count : 0.0);

    return test_failed > 0 ? 1 : 0;
}

// Test Lua error handling with luaL_error
static void test_lua_error_handling() {
    test_begin("Lua Error Handling");
    
    // Create a fresh Lua state
    lua_State *L = luaL_newstate();
    TEST_ASSERT_NOT_NULL(L, "Lua state should be created");
    
    if (!L) {
        test_end("Lua Error Handling");
        return;
    }
    
    // Open standard libraries
    luaL_openlibs(L);
    
    // Test 1: Basic luaL_error call
    printf("Testing basic luaL_error call...\n");
    
    // Push a function that will call luaL_error
    lua_pushcfunction(L, function_that_calls_luaL_error);
    
    // Call the function - this should NOT panic, just throw a Lua error
    int result = lua_pcall(L, 0, 0, 0);
    
    if (result == LUA_ERRRUN) {
        // This is expected - luaL_error should throw a catchable error
        const char *error_msg = lua_tostring(L, -1);
        TEST_ASSERT_NOT_NULL(error_msg, "Error message should be available");
        printf("✓ luaL_error threw catchable error: %s\n", error_msg);
        lua_pop(L, 1); // pop error message
    } else {
        TEST_ASSERT(false, "luaL_error should have thrown LUA_ERRRUN");
    }
    
    // Test 2: Test luaL_error in a protected call
    printf("Testing luaL_error in protected call...\n");
    
    // Create a protected call
    lua_pushcfunction(L, function_that_calls_luaL_error);
    result = lua_pcall(L, 0, 0, 0);
    
    if (result == LUA_ERRRUN) {
        const char *error_msg = lua_tostring(L, -1);
        TEST_ASSERT_NOT_NULL(error_msg, "Protected call error message should be available");
        printf("✓ Protected call caught luaL_error: %s\n", error_msg);
        lua_pop(L, 1);
    } else {
        TEST_ASSERT(false, "Protected call should have caught LUA_ERRRUN");
    }
    
    // Test 3: Test that we can continue using Lua after luaL_error
    printf("Testing Lua state recovery after luaL_error...\n");
    
    // Try to do something simple
    lua_pushstring(L, "test");
    const char *str = lua_tostring(L, -1);
    TEST_ASSERT_NOT_NULL(str, "Lua state should still be functional");
    TEST_ASSERT(strcmp(str, "test") == 0, "String should match");
    lua_pop(L, 1);
    
    printf("✓ Lua state recovered successfully after luaL_error\n");
    
    // Clean up
    lua_close(L);
    
    test_end("Lua Error Handling");
}

// Test Lua panic behavior
static void test_lua_panic_behavior() {
    test_begin("Lua Panic Behavior");
    
    // Create a fresh Lua state
    lua_State *L = luaL_newstate();
    TEST_ASSERT_NOT_NULL(L, "Lua state should be created");
    
    if (!L) {
        test_end("Lua Panic Behavior");
        return;
    }
    
    // Open standard libraries
    luaL_openlibs(L);
    
    // Test: Set a panic function and see if luaL_error triggers it
    printf("Testing if luaL_error triggers panic function...\n");
    
    // Set a panic function
    lua_atpanic(L, panic_function);
    
    // Try to call luaL_error
    lua_pushcfunction(L, function_that_calls_luaL_error);
    
    // This should NOT trigger the panic function
    int result = lua_pcall(L, 0, 0, 0);
    
    if (result == LUA_ERRRUN) {
        printf("✓ luaL_error did NOT trigger panic function (correct behavior)\n");
        const char *error_msg = lua_tostring(L, -1);
        TEST_ASSERT_NOT_NULL(error_msg, "Error message should be available");
        lua_pop(L, 1);
    } else {
        TEST_ASSERT(false, "luaL_error should have thrown LUA_ERRRUN");
    }
    
    // Clean up
    lua_close(L);
    
    test_end("Lua Panic Behavior");
}

// Function that calls luaL_error
static int function_that_calls_luaL_error(lua_State *L) {
    luaL_error(L, "This is a test error from luaL_error");
    return 0; // This should never be reached
}

// Panic function to detect if luaL_error triggers panics
static int panic_function(lua_State *L) {
    printf("🚨 PANIC FUNCTION CALLED! This should NOT happen with luaL_error!\n");
    printf("Panic message: %s\n", lua_tostring(L, -1));
    return 0;
}

// Direct Lua benchmark tests 
static void test_direct_lua_benchmarks() {
    test_begin("Direct Lua Benchmarks");
    
    printf("=== Direct LuaJIT Benchmark Tests ===\n");
    
    // Create a fresh Lua state
    lua_State *L = luaL_newstate();
    TEST_ASSERT_NOT_NULL(L, "Lua state should be created");
    
    if (!L) {
        test_end("Direct Lua Benchmarks");
        return;
    }
    
    // Open standard libraries (including JIT)
    luaL_openlibs(L);
    
    // Check JIT status (with error handling)
    printf("Checking JIT status...\n");
    lua_getglobal(L, "jit");
    if (lua_istable(L, -1)) {
        printf("JIT table found\n");
        lua_getfield(L, -1, "status");
        if (lua_isfunction(L, -1)) {
            printf("JIT status function found\n");
            int call_result = lua_pcall(L, 0, 1, 0);
            if (call_result == LUA_OK) {
                const char* status = lua_tostring(L, -1);
                printf("JIT Status: %s\n", status ? status : "unknown");
            } else {
                printf("JIT status call failed: %s\n", lua_tostring(L, -1));
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        } else {
            printf("JIT status is not a function (type: %s)\n", lua_typename(L, lua_type(L, -1)));
            lua_pop(L, 1);
        }
        
        lua_getfield(L, -1, "version");
        if (lua_isstring(L, -1)) {
            const char* version = lua_tostring(L, -1);
            printf("JIT Version: %s\n", version ? version : "unknown");
        } else {
            printf("JIT version is not a string (type: %s)\n", lua_typename(L, lua_type(L, -1)));
        }
        lua_pop(L, 1);
    } else {
        printf("JIT table not found (type: %s)\n", lua_typename(L, lua_type(L, -1)));
    }
    lua_pop(L, 1);
    
    // Load the benchmark script directly
    printf("\n--- Loading Benchmark Script ---\n");
    
    // First, create the TEST_MODULE table
    lua_newtable(L);
    lua_setglobal(L, "TEST_MODULE");
    
    // Now load and execute the script
    int load_result = luaL_loadstring(L, benchmark_lua_script);
    TEST_ASSERT(load_result == LUA_OK, "Script should load successfully");
    
    if (load_result == LUA_OK) {
        // Execute the script to populate the TEST_MODULE table
        int exec_result = lua_pcall(L, 0, 0, 0);
        TEST_ASSERT(exec_result == LUA_OK, "Script should execute successfully");
        
        if (exec_result == LUA_OK) {
            // Get the TEST_MODULE table
            lua_getglobal(L, "TEST_MODULE");
            TEST_ASSERT(lua_istable(L, -1), "TEST_MODULE should be a table");
            
            if (lua_istable(L, -1)) {
                // Test 1: Single function benchmark
                printf("\n--- Single Function Benchmark ---\n");
                lua_getfield(L, -1, "benchmark_function");
                TEST_ASSERT(lua_isfunction(L, -1), "benchmark_function should exist");
                
                if (lua_isfunction(L, -1)) {
                    const int iterations = 100;
                    printf("Running single function benchmark (%d iterations)...\n", iterations);
                    
                    // Warm up
                    uint64_t warmup_start = time_now();
                    for (int i = 0; i < 10; i++) {
                        lua_pushvalue(L, -1); // Copy function
                        
                        // Simulate engine lookup overhead if enabled
                        if (g_include_lookup) {
                            // Get the TEST_MODULE table reference for lookup simulation
                            lua_getglobal(L, "TEST_MODULE");
                            int module_ref = luaL_ref(L, LUA_REGISTRYINDEX);
                            _simulate_engine_lookup(L, module_ref, "benchmark_function");
                            luaL_unref(L, LUA_REGISTRYINDEX, module_ref);
                            // The lookup function may have modified the stack, so we need to ensure the function is still available
                            // Since we're calling lua_pushvalue(L, -1) right before this, the function should be at -1
                        }
                        
                        // Simulate EseLuaValue overhead if enabled
                        if (g_include_luavalue) {
                            _simulate_luavalue_overhead(L, 1); // Simulate one argument
                        }
                        
                        // Setup hook if enabled
                        if (g_include_hook) {
                            LuaFunctionHook timeout;
                            timeout.start_time = clock();
                            timeout.call_count = 0;
                            timeout.instruction_count = 0;
                            timeout.max_execution_time = (clock_t)((10) * CLOCKS_PER_SEC); // 10 seconds
                            timeout.max_instruction_count = 4000000; // 4M
                            
                            lua_pushlightuserdata(L, &timeout);
                            lua_setfield(L, LUA_REGISTRYINDEX, LUA_HOOK_KEY);
                            lua_sethook(L, _lua_engine_function_hook, LUA_MASKCOUNT, LUA_HOOK_FRQ);
                        }
                        
                        // Push argument for function call
                        lua_pushnumber(L, (double)i);
                        
                        int call_result = lua_pcall(L, 1, 1, 0); // Pass 1 argument, expect 1 result
                        
                        // Remove hook if enabled
                        if (g_include_hook) {
                            lua_sethook(L, NULL, 0, 0);
                        }
                        
                        if (call_result != LUA_OK) {
                            printf("✗ FAIL: Warm-up call %d failed: %s\n", i, lua_tostring(L, -1));
                            lua_pop(L, 1);
                            break;
                        }
                        lua_pop(L, 1); // Pop result
                    }
                    uint64_t warmup_end = time_now();
                    double warmup_time_ms = (double)(warmup_end - warmup_start) / 1000000.0;
                    printf("Warm-up time: %.2fms\n", warmup_time_ms);
                    
                    // Benchmark
                    uint64_t benchmark_start = time_now();
                    for (int i = 0; i < iterations; i++) {
                        lua_pushvalue(L, -1); // Copy function
                        
                        // Simulate engine lookup overhead if enabled
                        if (g_include_lookup) {
                            // Get the TEST_MODULE table reference for lookup simulation
                            lua_getglobal(L, "TEST_MODULE");
                            int module_ref = luaL_ref(L, LUA_REGISTRYINDEX);
                            _simulate_engine_lookup(L, module_ref, "benchmark_function");
                            luaL_unref(L, LUA_REGISTRYINDEX, module_ref);
                            // The lookup function may have modified the stack, so we need to ensure the function is still available
                            // Since we're calling lua_pushvalue(L, -1) right before this, the function should be still at -1
                        }
                        
                        // Simulate EseLuaValue overhead if enabled
                        if (g_include_luavalue) {
                            _simulate_luavalue_overhead(L, 1); // Simulate one argument
                        }
                        
                        // Setup hook if enabled
                        if (g_include_hook) {
                            LuaFunctionHook timeout;
                            timeout.start_time = clock();
                            timeout.call_count = 0;
                            timeout.instruction_count = 0;
                            timeout.max_execution_time = (clock_t)((10) * CLOCKS_PER_SEC); // 10 seconds
                            timeout.max_instruction_count = 4000000; // 4M
                            
                            lua_pushlightuserdata(L, &timeout);
                            lua_setfield(L, LUA_REGISTRYINDEX, LUA_HOOK_KEY);
                            lua_sethook(L, _lua_engine_function_hook, LUA_MASKCOUNT, LUA_HOOK_FRQ);
                        }
                        
                        // Push argument for function call
                        lua_pushnumber(L, (double)i);
                        
                        int call_result = lua_pcall(L, 1, 1, 0); // Pass 1 argument, expect 1 result
                        
                        // Remove hook if enabled
                        if (g_include_hook) {
                            lua_sethook(L, NULL, 0, 0);
                        }
                        
                        if (call_result != LUA_OK) {
                            printf("✗ FAIL: Benchmark call %d failed: %s\n", i, lua_tostring(L, -1));
                            lua_pop(L, 1);
                            break;
                        }
                        lua_pop(L, 1); // Pop result
                    }
                    uint64_t benchmark_end = time_now();
                    double benchmark_time_ms = (double)(benchmark_end - benchmark_start) / 1000000.0;
                    double avg_time_us = (double)(benchmark_end - benchmark_start) / iterations / 1000.0;
                    printf("✓ PASS: Single function benchmark completed in %.2fms (avg: %.2fμs per call)\n", benchmark_time_ms, avg_time_us);
                }
                lua_pop(L, 1); // Pop function
                
                // Test 2: Batch function benchmark
                printf("\n--- Batch Function Benchmark ---\n");
                lua_getfield(L, -1, "benchmark_function");
                if (lua_isfunction(L, -1)) {
                    const int iterations = 20;
                    printf("Running batch function benchmarks (%d iterations each)...\n", iterations);
                    
                    // Test 10 functions in a row
                    uint64_t batch10_start = time_now();
                    for (int iter = 0; iter < iterations; iter++) {
                        for (int i = 0; i < 10; i++) {
                            lua_pushvalue(L, -1); // Copy function
                            int call_result = lua_pcall(L, 0, 1, 0);
                            if (call_result != LUA_OK) {
                                printf("✗ FAIL: Batch 10 call failed: %s\n", lua_tostring(L, -1));
                                lua_pop(L, 1);
                                goto batch_10_done;
                            }
                            lua_pop(L, 1); // Pop result
                        }
                    }
                    uint64_t batch10_end = time_now();
                    double batch10_time_ms = (double)(batch10_end - batch10_start) / 1000000.0;
                    double avg_batch10_us = (double)(batch10_end - batch10_start) / (iterations * 10) / 1000.0;
                    printf("✓ PASS: 10 functions batch completed in %.2fms (avg: %.2fμs per call)\n", batch10_time_ms, avg_batch10_us);
                    
                batch_10_done:
                    // Test 50 functions in a row
                    {
                        uint64_t batch50_start = time_now();
                    for (int iter = 0; iter < iterations; iter++) {
                        for (int i = 0; i < 50; i++) {
                            lua_pushvalue(L, -1); // Copy function
                            int call_result = lua_pcall(L, 0, 1, 0);
                            if (call_result != LUA_OK) {
                                printf("✗ FAIL: Batch 50 call failed: %s\n", lua_tostring(L, -1));
                                lua_pop(L, 1);
                                goto batch_50_done;
                            }
                            lua_pop(L, 1); // Pop result
                        }
                    }
                    uint64_t batch50_end = time_now();
                    double batch50_time_ms = (double)(batch50_end - batch50_start) / 1000000.0;
                    double avg_batch50_us = (double)(batch50_end - batch50_start) / (iterations * 50) / 1000.0;
                        printf("✓ PASS: 50 functions batch completed in %.2fms (avg: %.2fμs per call)\n", batch50_time_ms, avg_batch50_us);
                    }
                    
                batch_50_done:
                    // Test 100 functions in a row
                    {
                        uint64_t batch100_start = time_now();
                    for (int iter = 0; iter < iterations; iter++) {
                        for (int i = 0; i < 100; i++) {
                            lua_pushvalue(L, -1); // Copy function
                            int call_result = lua_pcall(L, 0, 1, 0);
                            if (call_result != LUA_OK) {
                                printf("✗ FAIL: Batch 100 call failed: %s\n", lua_tostring(L, -1));
                                lua_pop(L, 1);
                                goto batch_100_done;
                            }
                            lua_pop(L, 1); // Pop result
                        }
                    }
                    uint64_t batch100_end = time_now();
                    double batch100_time_ms = (double)(batch100_end - batch100_start) / 1000000.0;
                    double avg_batch100_us = (double)(batch100_end - batch100_start) / (iterations * 100) / 1000.0;
                        printf("✓ PASS: 100 functions batch completed in %.2fms (avg: %.2fμs per call)\n", batch100_time_ms, avg_batch100_us);
                    }
                    
                batch_100_done:
                    printf("✓ PASS: Batch function benchmarks completed\n");
                }
                lua_pop(L, 1); // Pop function
                
                // Test 3: JIT stress test (10,000 calls)
                printf("\n--- JIT Stress Test (10,000 calls) ---\n");
                lua_getfield(L, -1, "benchmark_function");
                if (lua_isfunction(L, -1)) {
                    const int total_calls = 10000;
                    printf("Testing JIT compilation with %d repeated function calls...\n", total_calls);
                    
                    uint64_t stress_start = time_now();
                    for (int i = 0; i < total_calls; i++) {
                        if (i % 1000 == 0) {
                            printf("Progress: %d/%d calls...\n", i, total_calls);
                        }
                        
                        lua_pushvalue(L, -1); // Copy function
                        
                        // Simulate engine lookup overhead if enabled
                        if (g_include_lookup) {
                            // Get the TEST_MODULE table reference for lookup simulation
                            lua_getglobal(L, "TEST_MODULE");
                            int module_ref = luaL_ref(L, LUA_REGISTRYINDEX);
                            _simulate_engine_lookup(L, module_ref, "benchmark_function");
                            luaL_unref(L, LUA_REGISTRYINDEX, module_ref);
                            // The lookup function may have modified the stack, so we need to ensure the function is still available
                            // Since we're calling lua_pushvalue(L, -1) right before this, the function should be still at -1
                        }
                        
                        // Simulate EseLuaValue overhead if enabled
                        if (g_include_luavalue) {
                            _simulate_luavalue_overhead(L, 1); // Simulate one argument
                        }
                        
                        // Setup hook if enabled
                        if (g_include_hook) {
                            LuaFunctionHook timeout;
                            timeout.start_time = clock();
                            timeout.call_count = 0;
                            timeout.instruction_count = 0;
                            timeout.max_execution_time = (clock_t)((10) * CLOCKS_PER_SEC); // 10 seconds
                            timeout.max_instruction_count = 4000000; // 4M
                            
                            lua_pushlightuserdata(L, &timeout);
                            lua_setfield(L, LUA_REGISTRYINDEX, LUA_HOOK_KEY);
                            lua_sethook(L, _lua_engine_function_hook, LUA_MASKCOUNT, LUA_HOOK_FRQ);
                        }
                        
                        // Push argument for function call
                        lua_pushnumber(L, (double)i);
                        
                        int call_result = lua_pcall(L, 1, 1, 0); // Pass 1 argument, expect 1 result
                        
                        // Remove hook if enabled
                        if (g_include_hook) {
                            lua_sethook(L, NULL, 0, 0);
                        }
                        
                        if (call_result != LUA_OK) {
                            printf("✗ FAIL: JIT stress call %d failed: %s\n", i, lua_tostring(L, -1));
                            lua_pop(L, 1);
                            break;
                        }
                        lua_pop(L, 1); // Pop result
                    }
                    uint64_t stress_end = time_now();
                    double stress_time_ms = (double)(stress_end - stress_start) / 1000000.0;
                    double avg_stress_us = (double)(stress_end - stress_start) / total_calls / 1000.0;
                    printf("✓ PASS: JIT stress test (10K) completed in %.2fms (avg: %.2fμs per call)\n", stress_time_ms, avg_stress_us);
                }
                lua_pop(L, 1); // Pop function
                
                // Test 4: JIT stress test (100,000 calls)
                printf("\n--- JIT Stress Test (100,000 calls) ---\n");
                lua_getfield(L, -1, "benchmark_function");
                if (lua_isfunction(L, -1)) {
                    const int total_calls = 100000;
                    printf("Testing JIT compilation with %d repeated function calls...\n", total_calls);
                    
                    uint64_t stress100k_start = time_now();
                    for (int i = 0; i < total_calls; i++) {
                        if (i % 10000 == 0) {
                            printf("Progress: %d/%d calls...\n", i, total_calls);
                        }
                        
                        lua_pushvalue(L, -1); // Copy function
                        
                        // Simulate engine lookup overhead if enabled
                        if (g_include_lookup) {
                            // Get the TEST_MODULE table reference for lookup simulation
                            lua_getglobal(L, "TEST_MODULE");
                            int module_ref = luaL_ref(L, LUA_REGISTRYINDEX);
                            _simulate_engine_lookup(L, module_ref, "benchmark_function");
                            luaL_unref(L, LUA_REGISTRYINDEX, module_ref);
                            // The lookup function may have modified the stack, so we need to ensure the function is still available
                            // Since we're calling lua_pushvalue(L, -1) right before this, the function should be still at -1
                        }
                        
                        // Simulate EseLuaValue overhead if enabled
                        if (g_include_luavalue) {
                            _simulate_luavalue_overhead(L, 1); // Simulate one argument
                        }
                        
                        // Push argument for function call
                        lua_pushnumber(L, (double)i);
                        
                        int call_result = lua_pcall(L, 1, 1, 0); // Pass 1 argument, expect 1 result
                        if (call_result != LUA_OK) {
                            printf("✗ FAIL: JIT stress 100K call %d failed: %s\n", i, lua_tostring(L, -1));
                            lua_pop(L, 1);
                            break;
                        }
                        lua_pop(L, 1); // Pop result
                    }
                    uint64_t stress100k_end = time_now();
                    double stress100k_time_ms = (double)(stress100k_end - stress100k_start) / 1000000.0;
                    double avg_stress100k_us = (double)(stress100k_end - stress100k_start) / total_calls / 1000.0;
                    printf("✓ PASS: JIT stress test (100K) completed in %.2fms (avg: %.2fμs per call)\n", stress100k_time_ms, avg_stress100k_us);
                }
                lua_pop(L, 1); // Pop function
                
                // Test 5: JIT stress test (1,000,000 calls)
                printf("\n--- JIT Stress Test (1,000,000 calls) ---\n");
                lua_getfield(L, -1, "benchmark_function");
                if (lua_isfunction(L, -1)) {
                    const int total_calls = 1000000;
                    printf("Testing JIT compilation with %d repeated function calls...\n", total_calls);
                    
                    uint64_t stress1m_start = time_now();
                    for (int i = 0; i < total_calls; i++) {
                        if (i % 100000 == 0) {
                            printf("Progress: %d/%d calls...\n", i, total_calls);
                        }
                        
                        lua_pushvalue(L, -1); // Copy function
                        
                        // Simulate engine lookup overhead if enabled
                        if (g_include_lookup) {
                            // Get the TEST_MODULE table reference for lookup simulation
                            lua_getglobal(L, "TEST_MODULE");
                            int module_ref = luaL_ref(L, LUA_REGISTRYINDEX);
                            _simulate_engine_lookup(L, module_ref, "benchmark_function");
                            luaL_unref(L, LUA_REGISTRYINDEX, module_ref);
                            // The lookup function may have modified the stack, so we need to ensure the function is still available
                            // Since we're calling lua_pushvalue(L, -1) right before this, the function should be still at -1
                        }
                        
                        // Simulate EseLuaValue overhead if enabled
                        if (g_include_luavalue) {
                            _simulate_luavalue_overhead(L, 1); // Simulate one argument
                        }
                        
                        // Push argument for function call
                        lua_pushnumber(L, (double)i);
                        
                        int call_result = lua_pcall(L, 1, 1, 0); // Pass 1 argument, expect 1 result
                        if (call_result != LUA_OK) {
                            printf("✗ FAIL: JIT stress 1M call %d failed: %s\n", i, lua_tostring(L, -1));
                            lua_pop(L, 1);
                            break;
                        }
                        lua_pop(L, 1); // Pop result
                    }
                    uint64_t stress1m_end = time_now();
                    double stress1m_time_ms = (double)(stress1m_end - stress1m_start) / 1000000.0;
                    double avg_stress1m_us = (double)(stress1m_end - stress1m_start) / total_calls / 1000.0;
                    printf("✓ PASS: JIT stress test (1M) completed in %.2fms (avg: %.2fμs per call)\n", stress1m_time_ms, avg_stress1m_us);
                }
                lua_pop(L, 1); // Pop function
            }
            lua_pop(L, 1); // Pop TEST_MODULE table
        }
    }
    
    // Clean up
    lua_close(L);
    
    printf("\n✓ PASS: Direct Lua benchmarks completed\n");
    test_end("Direct Lua Benchmarks");
}
