#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <execinfo.h>
#include <signal.h>
#include "test_utils.h"
#include "../src/scripting/lua_engine.h"
#include "../src/scripting/lua_engine_private.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"
#include "../src/platform/time.h"

// Test function declarations
static void test_benchmark_single_function();
static void test_benchmark_batch_functions();
static void test_benchmark_jit_comparison();

// Benchmark Lua script content
static const char* benchmark_lua_script = 
"function TEST_MODULE:benchmark_function()\n"
"    local sum = 0\n"
"    for i = 1, 10000 do\n"
"        sum = sum + math.sin(i * 0.1) * math.cos(i * 0.1) + math.sqrt(i)\n"
"    end\n"
"    return sum\n"
"end\n"
"\n"
"function TEST_MODULE:benchmark_function_10()\n"
"    local sum = 0\n"
"    for i = 1, 100000 do\n"
"        sum = sum + math.sin(i * 0.1) * math.cos(i * 0.1) + math.sqrt(i)\n"
"    end\n"
"    return sum\n"
"end\n"
"\n"
"function TEST_MODULE:benchmark_function_100()\n"
"    local sum = 0\n"
"    for i = 1, 1000000 do\n"
"        sum = sum + math.sin(i * 0.1) * math.cos(i * 0.1) + math.sqrt(i)\n"
"    end\n"
"    return sum\n"
"end\n"
"\n"
"function TEST_MODULE:benchmark_function_1000()\n"
"    local sum = 0\n"
"    for i = 1, 10000000 do\n"
"        sum = sum + math.sin(i * 0.1) * math.cos(i * 0.1) + math.sqrt(i)\n"
"    end\n"
"    return sum\n"
"end\n"
"\n";

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
    
    test_suite_begin("🚀 Starting Lua Engine Benchmark Tests");
    
    // Initialize required systems
    log_init();
    
    // Run all benchmark tests
    test_benchmark_single_function();
    test_benchmark_batch_functions();
    test_benchmark_jit_comparison();
    
    // Print final summary
    test_suite_end("🎯 Final Benchmark Summary");
    
    return 0;
}

// Benchmark single function execution
static void test_benchmark_single_function() {
    test_begin("Single Function Benchmark");
    
    EseLuaEngine* engine = lua_engine_create();
    
    TEST_ASSERT_NOT_NULL(engine, "Engine should not be NULL");
    if (!engine) {
        test_end("Single Function Benchmark");
        return;
    }
    
    // Load the benchmark script
    bool load_result = lua_engine_load_script_from_string(engine, benchmark_lua_script, "benchmark_script", "TEST_MODULE");
    TEST_ASSERT(load_result, "Benchmark script should load successfully");
    
    if (load_result) {
        // Create an instance of the script
        int instance_ref = lua_engine_instance_script(engine, "benchmark_script");
        TEST_ASSERT(instance_ref > 0, "Script instance should be created successfully");
        
        if (instance_ref > 0) {
            // Create a dummy self object for function calls
            lua_State* L = engine->runtime;
            lua_newtable(L);  // Create empty table as dummy self
            int dummy_self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            
            const int iterations = 1000;
            uint64_t total_time = 0;
            
            printf("Running single function benchmark (%d iterations)...\n", iterations);
            
            // Warm up the JIT
            for (int i = 0; i < 100; i++) {
                lua_engine_run_function(engine, instance_ref, dummy_self_ref, "benchmark_function", 0, NULL, NULL);
            }
            
            // Benchmark single function execution
            for (int i = 0; i < iterations; i++) {
                uint64_t start_time = time_now();
                lua_engine_run_function(engine, instance_ref, dummy_self_ref, "benchmark_function", 0, NULL, NULL);
                uint64_t end_time = time_now();
                total_time += (end_time - start_time);
            }
            
            double avg_time_ms = (double)total_time / iterations / 1000000.0; // Convert ns to ms
            printf("✓ PASS: Average time to run 1 function: %.2fms\n", avg_time_ms);
            
            // Clean up
            luaL_unref(L, LUA_REGISTRYINDEX, dummy_self_ref);
            lua_engine_instance_remove(engine, instance_ref);
        }
    }
    
    lua_engine_destroy(engine);
    
    test_end("Single Function Benchmark");
}

// Benchmark batch function execution
static void test_benchmark_batch_functions() {
    test_begin("Batch Function Benchmark");
    
    EseLuaEngine* engine = lua_engine_create();
    
    TEST_ASSERT_NOT_NULL(engine, "Engine should not be NULL");
    if (!engine) {
        test_end("Batch Function Benchmark");
        return;
    }
    
    // Load the benchmark script
    bool load_result = lua_engine_load_script_from_string(engine, benchmark_lua_script, "benchmark_script", "TEST_MODULE");
    TEST_ASSERT(load_result, "Benchmark script should load successfully");
    
    if (load_result) {
        // Create an instance of the script
        int instance_ref = lua_engine_instance_script(engine, "benchmark_script");
        TEST_ASSERT(instance_ref > 0, "Script instance should be created successfully");
        
        if (instance_ref > 0) {
            // Create a dummy self object for function calls
            lua_State* L = engine->runtime;
            lua_newtable(L);  // Create empty table as dummy self
            int dummy_self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            
            const int iterations = 100;
            
            printf("Running batch function benchmarks (%d iterations each)...\n", iterations);
            
            // Warm up the JIT
            for (int i = 0; i < 50; i++) {
                lua_engine_run_function(engine, instance_ref, dummy_self_ref, "benchmark_function", 0, NULL, NULL);
            }
            
            // Benchmark 10 functions in a row
            uint64_t total_time_10 = 0;
            for (int iter = 0; iter < iterations; iter++) {
                uint64_t start_time = time_now();
                for (int i = 0; i < 10; i++) {
                    lua_engine_run_function(engine, instance_ref, dummy_self_ref, "benchmark_function", 0, NULL, NULL);
                }
                uint64_t end_time = time_now();
                total_time_10 += (end_time - start_time);
            }
            double avg_time_10_ms = (double)total_time_10 / iterations / 1000000.0;
            printf("✓ PASS: Average time to run 10 functions: %.2fms\n", avg_time_10_ms);
            
            // Benchmark 100 functions in a row
            uint64_t total_time_100 = 0;
            for (int iter = 0; iter < iterations; iter++) {
                uint64_t start_time = time_now();
                for (int i = 0; i < 100; i++) {
                    lua_engine_run_function(engine, instance_ref, dummy_self_ref, "benchmark_function", 0, NULL, NULL);
                }
                uint64_t end_time = time_now();
                total_time_100 += (end_time - start_time);
            }
            double avg_time_100_ms = (double)total_time_100 / iterations / 1000000.0;
            printf("✓ PASS: Average time to run 100 functions: %.2fms\n", avg_time_100_ms);
            
            // Benchmark 1000 functions in a row
            uint64_t total_time_1000 = 0;
            for (int iter = 0; iter < iterations; iter++) {
                uint64_t start_time = time_now();
                for (int i = 0; i < 1000; i++) {
                    lua_engine_run_function(engine, instance_ref, dummy_self_ref, "benchmark_function", 0, NULL, NULL);
                }
                uint64_t end_time = time_now();
                total_time_1000 += (end_time - start_time);
            }
            double avg_time_1000_ms = (double)total_time_1000 / iterations / 1000000.0;
            printf("✓ PASS: Average time to run 1000 functions: %.2fms\n", avg_time_1000_ms);
            
            // Clean up
            luaL_unref(L, LUA_REGISTRYINDEX, dummy_self_ref);
            lua_engine_instance_remove(engine, instance_ref);
        }
    }
    
    lua_engine_destroy(engine);
    
    test_end("Batch Function Benchmark");
}

// Benchmark with JIT on vs off comparison
static void test_benchmark_jit_comparison() {
    test_begin("JIT Comparison Benchmark");
    
    printf("Running JIT comparison benchmarks...\n");
    
    // Test with JIT on (default)
    EseLuaEngine* engine_jit_on = lua_engine_create();
    
    TEST_ASSERT_NOT_NULL(engine_jit_on, "JIT on engine should not be NULL");
    if (!engine_jit_on) {
        test_end("JIT Comparison Benchmark");
        return;
    }
    
    // Load the benchmark script
    bool load_result = lua_engine_load_script_from_string(engine_jit_on, benchmark_lua_script, "benchmark_script", "TEST_MODULE");
    TEST_ASSERT(load_result, "Benchmark script should load successfully");
    
    if (load_result) {
        // Create an instance of the script
        int instance_ref = lua_engine_instance_script(engine_jit_on, "benchmark_script");
        TEST_ASSERT(instance_ref > 0, "Script instance should be created successfully");
        
        if (instance_ref > 0) {
            // Create a dummy self object for function calls
            lua_State* L = engine_jit_on->runtime;
            lua_newtable(L);  // Create empty table as dummy self
            int dummy_self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            
            const int iterations = 100;
            
            printf("\nWith JIT on:\n");
            
            // Warm up the JIT
            for (int i = 0; i < 100; i++) {
                lua_engine_run_function(engine_jit_on, instance_ref, dummy_self_ref, "benchmark_function", 0, NULL, NULL);
            }
            
            // Benchmark single function
            uint64_t total_time_1 = 0;
            for (int iter = 0; iter < iterations; iter++) {
                uint64_t start_time = time_now();
                lua_engine_run_function(engine_jit_on, instance_ref, dummy_self_ref, "benchmark_function", 0, NULL, NULL);
                uint64_t end_time = time_now();
                total_time_1 += (end_time - start_time);
            }
            double avg_time_1_ms = (double)total_time_1 / iterations / 1000000.0;
            printf("Average time to run 1 function: %.2fms\n", avg_time_1_ms);
            
            // Benchmark 10 functions
            uint64_t total_time_10 = 0;
            for (int iter = 0; iter < iterations; iter++) {
                uint64_t start_time = time_now();
                for (int i = 0; i < 10; i++) {
                    lua_engine_run_function(engine_jit_on, instance_ref, dummy_self_ref, "benchmark_function", 0, NULL, NULL);
                }
                uint64_t end_time = time_now();
                total_time_10 += (end_time - start_time);
            }
            double avg_time_10_ms = (double)total_time_10 / iterations / 1000000.0;
            printf("Average time to run 10 functions: %.2fms\n", avg_time_10_ms);
            
            // Benchmark 100 functions
            uint64_t total_time_100 = 0;
            for (int iter = 0; iter < iterations; iter++) {
                uint64_t start_time = time_now();
                for (int i = 0; i < 100; i++) {
                    lua_engine_run_function(engine_jit_on, instance_ref, dummy_self_ref, "benchmark_function", 0, NULL, NULL);
                }
                uint64_t end_time = time_now();
                total_time_100 += (end_time - start_time);
            }
            double avg_time_100_ms = (double)total_time_100 / iterations / 1000000.0;
            printf("Average time to run 100 functions: %.2fms\n", avg_time_100_ms);
            
            // Benchmark 1000 functions
            uint64_t total_time_1000 = 0;
            for (int iter = 0; iter < iterations; iter++) {
                uint64_t start_time = time_now();
                for (int i = 0; i < 1000; i++) {
                    lua_engine_run_function(engine_jit_on, instance_ref, dummy_self_ref, "benchmark_function", 0, NULL, NULL);
                }
                uint64_t end_time = time_now();
                total_time_1000 += (end_time - start_time);
            }
            double avg_time_1000_ms = (double)total_time_1000 / iterations / 1000000.0;
            printf("Average time to run 1000 functions: %.2fms\n", avg_time_1000_ms);
            
            // Clean up
            luaL_unref(L, LUA_REGISTRYINDEX, dummy_self_ref);
            lua_engine_instance_remove(engine_jit_on, instance_ref);
        }
    }
    
    lua_engine_destroy(engine_jit_on);
    
    // Test with JIT off
    EseLuaEngine* engine_jit_off = lua_engine_create();
    
    TEST_ASSERT_NOT_NULL(engine_jit_off, "JIT off engine should not be NULL");
    if (!engine_jit_off) {
        test_end("JIT Comparison Benchmark");
        return;
    }
    
    // Disable JIT
    lua_State* L = engine_jit_off->runtime;
    lua_getglobal(L, "jit");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "off");
        if (lua_isfunction(L, -1)) {
            lua_pushboolean(L, 1); // Enable JIT off
            int result = lua_pcall(L, 1, 0, 0);
            if (result == LUA_OK) {
                printf("\nJIT disabled successfully\n");
            } else {
                printf("\nWarning: Failed to disable JIT\n");
            }
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    
    // Load the benchmark script
    load_result = lua_engine_load_script_from_string(engine_jit_off, benchmark_lua_script, "benchmark_script", "TEST_MODULE");
    TEST_ASSERT(load_result, "Benchmark script should load successfully");
    
    if (load_result) {
        // Create an instance of the script
        int instance_ref = lua_engine_instance_script(engine_jit_off, "benchmark_script");
        TEST_ASSERT(instance_ref > 0, "Script instance should be created successfully");
        
        if (instance_ref > 0) {
            // Create a dummy self object for function calls
            lua_newtable(L);  // Create empty table as dummy self
            int dummy_self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            
            const int iterations = 100;
            
            printf("\nWith JIT off:\n");
            
            // Benchmark single function
            uint64_t total_time_1 = 0;
            for (int iter = 0; iter < iterations; iter++) {
                uint64_t start_time = time_now();
                lua_engine_run_function(engine_jit_off, instance_ref, dummy_self_ref, "benchmark_function", 0, NULL, NULL);
                uint64_t end_time = time_now();
                total_time_1 += (end_time - start_time);
            }
            double avg_time_1_ms = (double)total_time_1 / iterations / 1000000.0;
            printf("Average time to run 1 function: %.2fms\n", avg_time_1_ms);
            
            // Benchmark 10 functions
            uint64_t total_time_10 = 0;
            for (int iter = 0; iter < iterations; iter++) {
                uint64_t start_time = time_now();
                for (int i = 0; i < 10; i++) {
                    lua_engine_run_function(engine_jit_off, instance_ref, dummy_self_ref, "benchmark_function", 0, NULL, NULL);
                }
                uint64_t end_time = time_now();
                total_time_10 += (end_time - start_time);
            }
            double avg_time_10_ms = (double)total_time_10 / iterations / 1000000.0;
            printf("Average time to run 10 functions: %.2fms\n", avg_time_10_ms);
            
            // Benchmark 100 functions
            uint64_t total_time_100 = 0;
            for (int iter = 0; iter < iterations; iter++) {
                uint64_t start_time = time_now();
                for (int i = 0; i < 100; i++) {
                    lua_engine_run_function(engine_jit_off, instance_ref, dummy_self_ref, "benchmark_function", 0, NULL, NULL);
                }
                uint64_t end_time = time_now();
                total_time_100 += (end_time - start_time);
            }
            double avg_time_100_ms = (double)total_time_100 / iterations / 1000000.0;
            printf("Average time to run 100 functions: %.2fms\n", avg_time_100_ms);
            
            // Benchmark 1000 functions
            uint64_t total_time_1000 = 0;
            for (int iter = 0; iter < iterations; iter++) {
                uint64_t start_time = time_now();
                for (int i = 0; i < 1000; i++) {
                    lua_engine_run_function(engine_jit_off, instance_ref, dummy_self_ref, "benchmark_function", 0, NULL, NULL);
                }
                uint64_t end_time = time_now();
                total_time_1000 += (end_time - start_time);
            }
            double avg_time_1000_ms = (double)total_time_1000 / iterations / 1000000.0;
            printf("Average time to run 1000 functions: %.2fms\n", avg_time_1000_ms);
            
            // Clean up
            luaL_unref(L, LUA_REGISTRYINDEX, dummy_self_ref);
            lua_engine_instance_remove(engine_jit_off, instance_ref);
        }
    }
    
    lua_engine_destroy(engine_jit_off);
    
    printf("\n✓ PASS: JIT comparison benchmark completed\n");
    
    test_end("JIT Comparison Benchmark");
}
