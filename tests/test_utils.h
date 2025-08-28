#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Test skip flag
static bool test_skip = false;
static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;
static int test_skipped = 0;

// Test macros
#define TEST_ASSERT(condition, message) do { \
    test_count++; \
    if (test_skip) { \
        printf("ℹ INFO: Skipping test due to test_skip flag\n"); \
        test_skipped++; \
        return; \
    } \
    if (condition) { \
        test_passed++; \
        printf("✓ PASS: %s\n", message); \
    } else { \
        test_failed++; \
        printf("✗ FAIL: %s\n", message); \
    } \
} while(0)

#define TEST_ASSERT_EQUAL(expected, actual, message) do { \
    test_count++; \
    if (test_skip) { \
        printf("ℹ INFO: Skipping test due to test_skip flag\n"); \
        test_skipped++; \
        return; \
    } \
    if ((expected) == (actual)) { \
        test_passed++; \
        printf("✓ PASS: %s (expected: %g, got: %g)\n", message, (double)(expected), (double)(actual)); \
    } else { \
        test_failed++; \
        printf("✗ FAIL: %s (expected: %g, got: %g)\n", message, (double)(expected), (double)(actual)); \
    } \
} while(0)

#define TEST_ASSERT_POINTER_EQUAL(expected, actual, message) do { \
    test_count++; \
    if (test_skip) { \
        printf("ℹ INFO: Skipping test due to test_skip flag\n"); \
        test_skipped++; \
        return; \
    } \
    if ((expected) == (actual)) { \
        test_passed++; \
        printf("✓ PASS: %s (expected: %p, got: %p)\n", message, (void*)(expected), (void*)(actual)); \
    } else { \
        test_failed++; \
        printf("✗ FAIL: %s (expected: %p, got: %p)\n", message, (void*)(expected), (void*)(actual)); \
    } \
} while(0)

#define TEST_ASSERT_FLOAT_EQUAL(expected, actual, tolerance, message) do { \
    test_count++; \
    if (test_skip) { \
        printf("ℹ INFO: Skipping test due to test_skip flag\n"); \
        test_skipped++; \
        return; \
    } \
    float diff = (expected) - (actual); \
    if (diff < 0) diff = -diff; \
    if (diff <= (tolerance)) { \
        test_passed++; \
        printf("✓ PASS: %s (expected: %g, got: %g, diff: %g)\n", message, (double)(expected), (double)(actual), (double)diff); \
    } else { \
        test_failed++; \
        printf("✗ FAIL: %s (expected: %g, got: %g, diff: %g)\n", message, (double)(expected), (double)(actual), (double)diff); \
    } \
} while(0)

#define TEST_ASSERT_STRING_EQUAL(expected, actual, message) do { \
    test_count++; \
    if (test_skip) { \
        printf("ℹ INFO: Skipping test due to test_skip flag\n"); \
        test_skipped++; \
        return; \
    } \
    if (strcmp((expected), (actual)) == 0) { \
        test_passed++; \
        printf("✓ PASS: %s (expected: \"%s\", got: \"%s\")\n", message, (expected), (actual)); \
    } else { \
        test_failed++; \
        printf("✗ FAIL: %s (expected: \"%s\", got: \"%s\")\n", message, (expected), (actual)); \
    } \
} while(0)

#define TEST_ASSERT_NOT_NULL(ptr, message) do { \
    test_count++; \
    if (test_skip) { \
        printf("ℹ INFO: Skipping test due to test_skip flag\n"); \
        test_skipped++; \
        return; \
    } \
    if ((ptr) != NULL) { \
        test_passed++; \
        printf("✓ PASS: %s (pointer is not NULL)\n", message); \
    } else { \
        test_failed++; \
        printf("✗ FAIL: %s (pointer is NULL)\n", message); \
    } \
} while(0)

#define TEST_ASSERT_NULL(ptr, message) do { \
    test_count++; \
    if (test_skip) { \
        printf("ℹ INFO: Skipping test due to test_skip flag\n"); \
        test_skipped++; \
        return; \
    } \
    if ((ptr) == NULL) { \
        test_passed++; \
        printf("✓ PASS: %s (pointer is NULL)\n", message); \
    } else { \
        test_failed++; \
        printf("✗ FAIL: %s (pointer is not NULL)\n", message); \
    } \
} while(0)

// Test suite functions
static void test_suite_begin(const char *suite_name) {
    printf("\n=== %s ===\n", suite_name);
    test_count = 0;
    test_passed = 0;
    test_failed = 0;
}

static void test_suite_end(const char *suite_name) {
    printf("\n--- %s Results ---\n", suite_name);
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passed);
    printf("Failed: %d\n", test_failed);
    printf("Skipped: %d\n", test_skipped);
    printf("Success rate: %.1f%%\n", test_count > 0 ? (float)test_passed / test_count * 100.0f : 0.0f);
    
    if (test_failed == 0) {
        printf("🎉 All tests passed!\n");
    } else {
        printf("❌ Some tests failed!\n");
    }
}

// Mock Lua engine for testing
typedef struct MockLuaEngine {
    void *runtime;
} MockLuaEngine;

static MockLuaEngine* mock_lua_engine_create() {
    MockLuaEngine *engine = malloc(sizeof(MockLuaEngine));
    engine->runtime = NULL; // Mock runtime
    return engine;
}

static void mock_lua_engine_destroy(MockLuaEngine *engine) {
    if (engine) {
        free(engine);
    }
}

// Mock memory manager functions
static void mock_memory_manager_init() {
    // Mock implementation - do nothing
}

#endif // TEST_UTILS_H
