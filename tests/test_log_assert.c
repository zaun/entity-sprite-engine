#include "test_utils.h"
#include "../src/utility/log.h"

// Test function that demonstrates log_assert testing
static void test_log_assert_aborts(void) {
    test_begin("Testing log_assert aborts");
    
    // Test that log_assert aborts when condition is false
    TEST_ASSERT_ABORT(
        log_assert("TEST", false, "this should abort"),
        "log_assert should abort when condition is false"
    );
    
    // Test that log_assert does NOT abort when condition is true
    // This should pass (no abort)
    log_assert("TEST", true, "this should not abort");
    printf("✓ PASS: log_assert did not abort when condition was true\n");
    
    test_end("Testing log_assert aborts");
}

// Test function that demonstrates different categories
static void test_log_assert_categories(void) {
    test_begin("Testing log_assert categories");
    
    // Test with MEMORY category
    TEST_ASSERT_ABORT(
        log_assert("MEMORY", false, "invalid pointer"),
        "log_assert should abort with MEMORY category"
    );
    
    // Test with GRAPHICS category
    TEST_ASSERT_ABORT(
        log_assert("GRAPHICS", false, "texture not found"),
        "log_assert should abort with GRAPHICS category"
    );
    
    test_end("Testing log_assert categories");
}

// Main test function
int main(void) {
    test_suite_begin("log_assert Abort Testing Suite");
    
    // Run tests
    test_log_assert_aborts();
    test_log_assert_categories();
    
    test_suite_end("log_assert Abort Testing Suite");
    
    return (test_suite_failed == 0) ? 0 : 1;
}
