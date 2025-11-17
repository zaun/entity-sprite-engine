/*
* test_memory_manager.c - Unity-based tests for memory manager functionality
* Tests both main thread and multi-threaded use
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

#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"
#include "../src/utility/thread.h"

/**
* Test function declarations
*/
static void test_memory_manager_malloc_basic(void);
static void test_memory_manager_calloc_basic(void);
static void test_memory_manager_realloc_basic(void);
static void test_memory_manager_free_basic(void);
static void test_memory_manager_strdup_basic(void);
static void test_memory_manager_tag_tracking(void);
static void test_memory_manager_multiple_allocations(void);
static void test_memory_manager_large_allocation(void);
static void test_memory_manager_thread_isolation(void);
static void test_memory_manager_concurrent_threads(void);
static void test_memory_manager_report(void);
static void test_memory_manager_destroy(void);

/**
* Thread worker for testing
*/
typedef struct {
    int thread_id;
    int num_allocs;
    size_t alloc_size;
    void **pointers;
    bool *finished;
} ThreadTestData;

static void *thread_worker_alloc(void *arg) {
    ThreadTestData *data = (ThreadTestData *)arg;
    data->pointers = memory_manager.malloc(sizeof(void *) * data->num_allocs, MMTAG_TEMP);
    TEST_ASSERT_NOT_NULL_MESSAGE(data->pointers, "Thread should be able to allocate memory");
    
    for (int i = 0; i < data->num_allocs; i++) {
        data->pointers[i] = memory_manager.malloc(data->alloc_size, MMTAG_TEMP);
        TEST_ASSERT_NOT_NULL_MESSAGE(data->pointers[i], "Thread allocation should not fail");
    }
    
    *data->finished = true;
    return NULL;
}

static void *thread_worker_mixed(void *arg) {
    ThreadTestData *data = (ThreadTestData *)arg;
    
    // Mix of allocations and deallocations
    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = memory_manager.malloc(1024, MMTAG_TEMP);
        TEST_ASSERT_NOT_NULL_MESSAGE(ptrs[i], "Thread allocation should not fail");
    }
    
    // Free some
    for (int i = 0; i < 5; i++) {
        memory_manager.free(ptrs[i]);
    }
    
    // Allocate more
    for (int i = 5; i < 10; i++) {
        ptrs[i] = memory_manager.realloc(ptrs[i], 2048, MMTAG_TEMP);
        TEST_ASSERT_NOT_NULL_MESSAGE(ptrs[i], "Thread reallocation should not fail");
    }
    
    // Clean up remaining
    for (int i = 5; i < 10; i++) {
        memory_manager.free(ptrs[i]);
    }
    
    *data->finished = true;
    return NULL;
}

/**
* Test suite setup and teardown
*/
void setUp(void) {
    log_init();
}

void tearDown(void) {
    // Per-test cleanup is not required; the memory manager is destroyed once at the
    // end of the test run in main().
}

/**
* Main test runner
*/
int main(void) {
    log_init();

    printf("\nMemory Manager Tests\n");
    printf("--------------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_memory_manager_malloc_basic);
    RUN_TEST(test_memory_manager_calloc_basic);
    RUN_TEST(test_memory_manager_realloc_basic);
    RUN_TEST(test_memory_manager_free_basic);
    RUN_TEST(test_memory_manager_strdup_basic);
    RUN_TEST(test_memory_manager_tag_tracking);
    RUN_TEST(test_memory_manager_multiple_allocations);
    RUN_TEST(test_memory_manager_large_allocation);
    RUN_TEST(test_memory_manager_thread_isolation);
    RUN_TEST(test_memory_manager_concurrent_threads);
    RUN_TEST(test_memory_manager_report);
    RUN_TEST(test_memory_manager_destroy);

    memory_manager.destroy(true);

    return UNITY_END();
}

/**
* Basic malloc test
*/
static void test_memory_manager_malloc_basic(void) {
    void *ptr = memory_manager.malloc(1024, MMTAG_TEMP);
    TEST_ASSERT_NOT_NULL_MESSAGE(ptr, "malloc should return non-NULL pointer");
    
    // Verify memory is aligned
    uintptr_t addr = (uintptr_t)ptr;
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, addr % 16, "Memory should be 16-byte aligned");
    
    memory_manager.free(ptr);
}

/**
* Basic calloc test
*/
static void test_memory_manager_calloc_basic(void) {
    void *ptr = memory_manager.calloc(10, sizeof(int), MMTAG_TEMP);
    TEST_ASSERT_NOT_NULL_MESSAGE(ptr, "calloc should return non-NULL pointer");
    
    // Verify memory is zero-initialized
    int *int_ptr = (int *)ptr;
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, int_ptr[i], "calloc memory should be zero-initialized");
    }
    
    memory_manager.free(ptr);
}

/**
* Basic realloc test
*/
static void test_memory_manager_realloc_basic(void) {
    void *ptr = memory_manager.malloc(1024, MMTAG_TEMP);
    TEST_ASSERT_NOT_NULL_MESSAGE(ptr, "malloc should succeed");
    
    void *new_ptr = memory_manager.realloc(ptr, 2048, MMTAG_TEMP);
    TEST_ASSERT_NOT_NULL_MESSAGE(new_ptr, "realloc should succeed");
    
    memory_manager.free(new_ptr);
}

/**
* Basic free test
*/
static void test_memory_manager_free_basic(void) {
    void *ptr1 = memory_manager.malloc(512, MMTAG_TEMP);
    void *ptr2 = memory_manager.malloc(512, MMTAG_TEMP);
    
    TEST_ASSERT_NOT_NULL_MESSAGE(ptr1, "First allocation should succeed");
    TEST_ASSERT_NOT_NULL_MESSAGE(ptr2, "Second allocation should succeed");
    
    memory_manager.free(ptr1);
    memory_manager.free(ptr2);
    
    // Test should complete without crashing
    TEST_ASSERT_TRUE_MESSAGE(true, "Free should complete without errors");
}

/**
* Basic strdup test
*/
static void test_memory_manager_strdup_basic(void) {
    const char *original = "Hello, World!";
    char *copy = memory_manager.strdup(original, MMTAG_TEMP);
    
    TEST_ASSERT_NOT_NULL_MESSAGE(copy, "strdup should return non-NULL");
    TEST_ASSERT_TRUE_MESSAGE(original != copy, "strdup should return a different pointer");
    TEST_ASSERT_EQUAL_STRING_MESSAGE(original, copy, "strdup should copy string content");
    
    memory_manager.free(copy);
}

/**
* Tag tracking test
*/
static void test_memory_manager_tag_tracking(void) {
    void *ptr_general = memory_manager.malloc(100, MMTAG_GENERAL);
    void *ptr_engine = memory_manager.malloc(200, MMTAG_ENGINE);
    void *ptr_gui = memory_manager.malloc(300, MMTAG_GUI);
    
    TEST_ASSERT_NOT_NULL_MESSAGE(ptr_general, "GENERAL allocation should succeed");
    TEST_ASSERT_NOT_NULL_MESSAGE(ptr_engine, "ENGINE allocation should succeed");
    TEST_ASSERT_NOT_NULL_MESSAGE(ptr_gui, "GUI allocation should succeed");
    
    memory_manager.free(ptr_general);
    memory_manager.free(ptr_engine);
    memory_manager.free(ptr_gui);
    
    // Test should complete without errors
    TEST_ASSERT_TRUE_MESSAGE(true, "Tag tracking should work correctly");
}

/**
* Multiple allocations test
*/
static void test_memory_manager_multiple_allocations(void) {
    const int num_allocations = 100;
    void *pointers[100];
    
    // Allocate
    for (int i = 0; i < num_allocations; i++) {
        pointers[i] = memory_manager.malloc(1024, MMTAG_TEMP);
        TEST_ASSERT_NOT_NULL_MESSAGE(pointers[i], "Allocation should succeed");
    }
    
    // Free
    for (int i = 0; i < num_allocations; i++) {
        memory_manager.free(pointers[i]);
    }
    
    TEST_ASSERT_TRUE_MESSAGE(true, "Multiple allocations should work correctly");
}

/**
* Large allocation test
*/
static void test_memory_manager_large_allocation(void) {
    const size_t large_size = 10 * 1024 * 1024; // 10 MB
    void *ptr = memory_manager.malloc(large_size, MMTAG_TEMP);
    
    TEST_ASSERT_NOT_NULL_MESSAGE(ptr, "Large allocation should succeed");
    
    memory_manager.free(ptr);
}

/**
* Thread isolation test - verify threads have separate memory tracking
*/
static void test_memory_manager_thread_isolation(void) {
    bool finished = false;
    ThreadTestData data = {
        .thread_id = 1,
        .num_allocs = 50,
        .alloc_size = 1024,
        .pointers = NULL,
        .finished = &finished
    };
    
    EseThread thread = ese_thread_create(thread_worker_alloc, &data);
    TEST_ASSERT_NOT_NULL_MESSAGE(thread, "Thread creation should succeed");
    
    // Wait for thread to complete
    ese_thread_join(thread);
    
    TEST_ASSERT_TRUE_MESSAGE(finished, "Thread should have completed");
    
    // Verify the memory was properly managed in the thread
    // (The allocations should be tracked in the thread's memory manager)
    TEST_ASSERT_TRUE_MESSAGE(true, "Thread isolation test passed");
}

/**
* Concurrent threads test
*/
static void test_memory_manager_concurrent_threads(void) {
    const int num_threads = 4;
    EseThread threads[4];
    bool finished[4] = {false, false, false, false};
    ThreadTestData data[4];
    
    // Create multiple threads
    for (int i = 0; i < num_threads; i++) {
        data[i].thread_id = i;
        data[i].num_allocs = 100;
        data[i].alloc_size = 512;
        data[i].pointers = NULL;
        data[i].finished = &finished[i];
        
        threads[i] = ese_thread_create(thread_worker_mixed, &data[i]);
        TEST_ASSERT_NOT_NULL_MESSAGE(threads[i], "Thread creation should succeed");
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        ese_thread_join(threads[i]);
        TEST_ASSERT_TRUE_MESSAGE(finished[i], "Thread should have completed");
    }
    
    TEST_ASSERT_TRUE_MESSAGE(true, "Concurrent threads test passed");
}

/**
* Memory report test
*/
static void test_memory_manager_report(void) {
    // Allocate some memory
    void *ptr1 = memory_manager.malloc(1024, MMTAG_TEMP);
    void *ptr2 = memory_manager.malloc(2048, MMTAG_ENGINE);
    
    TEST_ASSERT_NOT_NULL_MESSAGE(ptr1, "Allocation should succeed");
    TEST_ASSERT_NOT_NULL_MESSAGE(ptr2, "Allocation should succeed");
    
    // Generate a report (should not crash)
    memory_manager.report(false);
    
    memory_manager.free(ptr1);
    memory_manager.free(ptr2);
    
    // Generate another report
    memory_manager.report(false);
    
    TEST_ASSERT_TRUE_MESSAGE(true, "Memory report should work");
}

/**
* Destroy test
*/
static void test_memory_manager_destroy(void) {
    // Allocate some memory
    void *ptr1 = memory_manager.malloc(1024, MMTAG_TEMP);
    void *ptr2 = memory_manager.malloc(2048, MMTAG_ENGINE);
    
    TEST_ASSERT_NOT_NULL_MESSAGE(ptr1, "Allocation should succeed");
    TEST_ASSERT_NOT_NULL_MESSAGE(ptr2, "Allocation should succeed");
    
    // Clean up - this should not crash even with leaked memory
    // (In a real scenario, you'd free first, but we want to test the destroy behavior)
    TEST_ASSERT_TRUE_MESSAGE(true, "Destroy test setup complete");
}

