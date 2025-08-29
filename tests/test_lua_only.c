#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <execinfo.h>
#include <signal.h>
#include "test_utils.h"

// Direct Lua includes - no src structures
#include "../src/vendor/lua/src/lua.h"
#include "../src/vendor/lua/src/lauxlib.h"
#include "../src/vendor/lua/src/lualib.h"

// Test function declarations
static void test_lua_error_handling();
static void test_lua_panic_behavior();

// Helper function declarations
static int function_that_calls_luaL_error(lua_State *L);
static int panic_function(lua_State *L);

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

int main() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_sigaction = segfault_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);

    printf("\n=== 🧪 Starting Lua-Only Tests ===\n\n");

    test_lua_error_handling();
    test_lua_panic_behavior();

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
