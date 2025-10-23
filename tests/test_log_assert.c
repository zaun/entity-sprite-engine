/*
* test_log_assert.c - Unity-based tests for log_assert functionality
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

#include "../src/utility/log.h"

/**
* C API Test Functions Declarations
*/
static void test_log_assert_aborts_on_false(void);
static void test_log_assert_passes_on_true(void);
static void test_log_assert_categories(void);
static void test_log_assert_message_formatting(void);
static void test_log_assert_edge_cases(void);

/**
* Test suite setup and teardown
*/
void setUp(void) {
    // Initialize logging system
    log_init();
}

void tearDown(void) {
    // No cleanup needed for log_assert tests
}

/**
* Main test runner
*/
int main(void) {
    log_init();

    printf("\nEseLogAssert Tests\n");
    printf("------------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_log_assert_aborts_on_false);
    RUN_TEST(test_log_assert_passes_on_true);
    RUN_TEST(test_log_assert_categories);
    RUN_TEST(test_log_assert_message_formatting);
    RUN_TEST(test_log_assert_edge_cases);

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_log_assert_aborts_on_false(void) {
    // Test that log_assert aborts when condition is false
    TEST_ASSERT_DEATH(
        log_assert("TEST", false, "this should abort"),
        "log_assert should abort when condition is false"
    );
}

static void test_log_assert_passes_on_true(void) {
    // Test that log_assert does NOT abort when condition is true
    // This should pass (no abort) - we can't use TEST_ASSERT_DEATH for this
    // since we expect it NOT to abort
    log_assert("TEST", true, "this should not abort");
    // If we reach this point, the test passed
    TEST_ASSERT_TRUE_MESSAGE(true, "log_assert should not abort when condition is true");
}

static void test_log_assert_categories(void) {
    // Test with different categories - all should abort
    TEST_ASSERT_DEATH(
        log_assert("MEMORY", false, "invalid pointer"),
        "log_assert should abort with MEMORY category"
    );
    
    TEST_ASSERT_DEATH(
        log_assert("GRAPHICS", false, "texture not found"),
        "log_assert should abort with GRAPHICS category"
    );
    
    TEST_ASSERT_DEATH(
        log_assert("LUA_ENGINE", false, "invalid lua state"),
        "log_assert should abort with LUA_ENGINE category"
    );
    
    TEST_ASSERT_DEATH(
        log_assert("ENTITY", false, "entity not found"),
        "log_assert should abort with ENTITY category"
    );
}

static void test_log_assert_message_formatting(void) {
    // Test different message formats
    TEST_ASSERT_DEATH(
        log_assert("TEST", false, "simple message"),
        "log_assert should abort with simple message"
    );
    
    TEST_ASSERT_DEATH(
        log_assert("TEST", false, "message with %s", "formatting"),
        "log_assert should abort with formatted message"
    );
    
    TEST_ASSERT_DEATH(
        log_assert("TEST", false, "message with number %d", 42),
        "log_assert should abort with number formatting"
    );
    
    TEST_ASSERT_DEATH(
        log_assert("TEST", false, "message with float %.2f", 3.14159),
        "log_assert should abort with float formatting"
    );
}

static void test_log_assert_edge_cases(void) {
    // Test edge cases
    TEST_ASSERT_DEATH(
        log_assert("", false, "empty category"),
        "log_assert should abort with empty category"
    );
    
    TEST_ASSERT_DEATH(
        log_assert("TEST", false, ""),
        "log_assert should abort with empty message"
    );
    
    TEST_ASSERT_DEATH(
        log_assert("TEST", false, "very long message that might test buffer limits and formatting capabilities of the logging system"),
        "log_assert should abort with long message"
    );
    
    // Test with special characters
    TEST_ASSERT_DEATH(
        log_assert("TEST", false, "message with special chars: !@#$%%^&*()"),
        "log_assert should abort with special characters"
    );
}