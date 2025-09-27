/*
* test_util_int_hashmap.c - Unity-based tests for utility/int_hashmap
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "testing.h"

#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"
#include "../src/utility/int_hashmap.h"

/**
* Test Functions Declarations
*/
static void test_int_hashmap_create_and_free(void);
static void test_int_hashmap_null_inputs(void);
static void test_int_hashmap_set_get_single(void);
static void test_int_hashmap_set_null_value_is_noop(void);
static void test_int_hashmap_update_existing_key_does_not_change_size(void);
static void test_int_hashmap_remove_existing_and_nonexisting(void);
static void test_int_hashmap_clear_resets_size_and_removes_entries(void);
static void test_int_hashmap_iter_empty_and_basic(void);
static void test_int_hashmap_iter_all_entries_no_duplicates(void);
static void test_int_hashmap_resize_many_entries_integrity(void);
static void test_int_hashmap_free_fn_called_on_clear_and_free_not_on_remove_or_overwrite(void);
static void test_int_hashmap_iter_allows_null_out_params(void);
static void test_int_hashmap_keys_zero_and_uint64_max(void);
static void test_int_hashmap_set_on_null_map_is_noop(void);

/**
* Unity setUp/tearDown (required symbols)
*/
void setUp(void) {}
void tearDown(void) {}

/**
* Free function tracking
*/
static int g_free_count = 0;
static void tracked_free(void *value) {
    if (value) {
        g_free_count++;
        memory_manager.free(value);
    }
}

static int *alloc_int(int v) {
    int *p = memory_manager.malloc(sizeof(int), MMTAG_TEMP);
    *p = v;
    return p;
}

/**
* Main test runner
*/
int main(void) {
    log_init();

    printf("\nIntHashMap Tests\n");
    printf("-----------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_int_hashmap_create_and_free);
    RUN_TEST(test_int_hashmap_null_inputs);
    RUN_TEST(test_int_hashmap_set_get_single);
    RUN_TEST(test_int_hashmap_set_null_value_is_noop);
    RUN_TEST(test_int_hashmap_update_existing_key_does_not_change_size);
    RUN_TEST(test_int_hashmap_remove_existing_and_nonexisting);
    RUN_TEST(test_int_hashmap_clear_resets_size_and_removes_entries);
    RUN_TEST(test_int_hashmap_iter_empty_and_basic);
    RUN_TEST(test_int_hashmap_iter_all_entries_no_duplicates);
    RUN_TEST(test_int_hashmap_resize_many_entries_integrity);
    RUN_TEST(test_int_hashmap_free_fn_called_on_clear_and_free_not_on_remove_or_overwrite);
    RUN_TEST(test_int_hashmap_iter_allows_null_out_params);
    RUN_TEST(test_int_hashmap_keys_zero_and_uint64_max);
    RUN_TEST(test_int_hashmap_set_on_null_map_is_noop);

    memory_manager.destroy();

    return UNITY_END();
}

static void test_int_hashmap_create_and_free(void) {
    EseIntHashMap *map = int_hashmap_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(map, "create should return a map");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, int_hashmap_size(map), "new map size should be 0");
    int_hashmap_free(map);

    /* free(NULL) should be safe */
    int_hashmap_free(NULL);
}

static void test_int_hashmap_null_inputs(void) {
    /* size(NULL) returns 0 */
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, int_hashmap_size(NULL), "size(NULL) should be 0");

    /* get/remove on NULL map return NULL */
    TEST_ASSERT_NULL(int_hashmap_get(NULL, 123));
    TEST_ASSERT_NULL(int_hashmap_remove(NULL, 123));

    /* clear(NULL) is safe */
    int_hashmap_clear(NULL);

    /* iterator create with NULL returns NULL, iter_next/iter_free handle NULL */
    EseIntHashMapIter *iter = int_hashmap_iter_create(NULL);
    TEST_ASSERT_NULL(iter);
    TEST_ASSERT_EQUAL_INT(0, int_hashmap_iter_next(NULL, NULL, NULL));
    int_hashmap_iter_free(NULL);
}

static void test_int_hashmap_set_get_single(void) {
    EseIntHashMap *map = int_hashmap_create(tracked_free);
    int *val = alloc_int(42);

    int_hashmap_set(map, 7ULL, val);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1, int_hashmap_size(map), "size should be 1 after add");
    int *got = (int *)int_hashmap_get(map, 7ULL);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL_INT(42, *got);

    /* get of missing key returns NULL */
    TEST_ASSERT_NULL(int_hashmap_get(map, 8ULL));

    int_hashmap_free(map);
}

static void test_int_hashmap_set_null_value_is_noop(void) {
    EseIntHashMap *map = int_hashmap_create(tracked_free);

    /* Setting NULL value should do nothing */
    int_hashmap_set(map, 1ULL, NULL);
    TEST_ASSERT_EQUAL_size_t(0, int_hashmap_size(map));
    TEST_ASSERT_NULL(int_hashmap_get(map, 1ULL));

    /* If a key exists, setting NULL should not alter it */
    int *v = alloc_int(5);
    int_hashmap_set(map, 1ULL, v);
    TEST_ASSERT_EQUAL_size_t(1, int_hashmap_size(map));
    int *got = (int *)int_hashmap_get(map, 1ULL);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL_INT(5, *got);

    int_hashmap_set(map, 1ULL, NULL);
    got = (int *)int_hashmap_get(map, 1ULL);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL_INT(5, *got);

    int_hashmap_free(map);
}

static void test_int_hashmap_update_existing_key_does_not_change_size(void) {
    EseIntHashMap *map = int_hashmap_create(tracked_free);
    int *v1 = alloc_int(10);
    int *v2 = alloc_int(20);

    int_hashmap_set(map, 100ULL, v1);
    TEST_ASSERT_EQUAL_size_t(1, int_hashmap_size(map));
    TEST_ASSERT_EQUAL_INT(10, *(int *)int_hashmap_get(map, 100ULL));

    /* overwrite with a new pointer keeps size the same */
    int_hashmap_set(map, 100ULL, v2);
    TEST_ASSERT_EQUAL_size_t(1, int_hashmap_size(map));
    TEST_ASSERT_EQUAL_INT(20, *(int *)int_hashmap_get(map, 100ULL));

    /* caller is responsible for v1 (was overwritten and not freed by map) */
    memory_manager.free(v1);

    int_hashmap_free(map);
}

static void test_int_hashmap_remove_existing_and_nonexisting(void) {
    EseIntHashMap *map = int_hashmap_create(NULL);
    int *v = alloc_int(77);
    int_hashmap_set(map, 7ULL, v);

    /* remove existing returns the pointer and decreases size */
    int *removed = (int *)int_hashmap_remove(map, 7ULL);
    TEST_ASSERT_NOT_NULL(removed);
    TEST_ASSERT_EQUAL_INT(77, *removed);
    TEST_ASSERT_EQUAL_size_t(0, int_hashmap_size(map));
    memory_manager.free(removed);

    /* remove non-existent returns NULL and keeps size */
    TEST_ASSERT_NULL(int_hashmap_remove(map, 7ULL));
    TEST_ASSERT_EQUAL_size_t(0, int_hashmap_size(map));

    int_hashmap_free(map);
}

static void test_int_hashmap_clear_resets_size_and_removes_entries(void) {
    EseIntHashMap *map = int_hashmap_create(tracked_free);
    for (uint64_t i = 0; i < 10; i++) {
        int_hashmap_set(map, i, alloc_int((int)i));
    }
    TEST_ASSERT_EQUAL_size_t(10, int_hashmap_size(map));

    /* clear removes all entries and with free_fn frees values */
    int_hashmap_clear(map);
    TEST_ASSERT_EQUAL_size_t(0, int_hashmap_size(map));
    for (uint64_t i = 0; i < 10; i++) {
        TEST_ASSERT_NULL(int_hashmap_get(map, i));
    }

    int_hashmap_free(map);
}

static void test_int_hashmap_iter_empty_and_basic(void) {
    EseIntHashMap *map = int_hashmap_create(tracked_free);
    EseIntHashMapIter *iter = int_hashmap_iter_create(map);
    TEST_ASSERT_NOT_NULL(iter);
    uint64_t key;
    void *val;
    TEST_ASSERT_EQUAL_INT(0, int_hashmap_iter_next(iter, &key, &val));
    int_hashmap_iter_free(iter);

    /* add single item and iterate */
    int *v = alloc_int(9);
    int_hashmap_set(map, 999ULL, v);
    iter = int_hashmap_iter_create(map);
    int seen = 0;
    while (int_hashmap_iter_next(iter, &key, &val)) {
        seen++;
        TEST_ASSERT_EQUAL_UINT64(999ULL, key);
        TEST_ASSERT_EQUAL_INT(9, *(int *)val);
    }
    TEST_ASSERT_EQUAL_INT(1, seen);
    int_hashmap_iter_free(iter);

    int_hashmap_free(map);
}

static void test_int_hashmap_iter_all_entries_no_duplicates(void) {
    EseIntHashMap *map = int_hashmap_create(NULL);
    enum { N = 50 };
    bool found[N];
    memset(found, 0, sizeof(found));
    for (int i = 0; i < N; i++) {
        int_hashmap_set(map, (uint64_t)i, alloc_int(1000 + i));
    }
    TEST_ASSERT_EQUAL_size_t(N, int_hashmap_size(map));

    EseIntHashMapIter *iter = int_hashmap_iter_create(map);
    int count = 0;
    uint64_t key;
    void *val;
    while (int_hashmap_iter_next(iter, &key, &val)) {
        TEST_ASSERT_TRUE(key < (uint64_t)N);
        TEST_ASSERT_FALSE(found[key]);
        found[key] = true;
        TEST_ASSERT_EQUAL_INT(1000 + (int)key, *(int *)val);
        count++;
    }
    TEST_ASSERT_EQUAL_INT(N, count);
    int_hashmap_iter_free(iter);

    /* cleanup: remove and free values (remove does not call free_fn) */
    for (int i = 0; i < N; i++) {
        int *p = (int *)int_hashmap_remove(map, (uint64_t)i);
        memory_manager.free(p);
    }

    int_hashmap_free(map);
}

static void test_int_hashmap_resize_many_entries_integrity(void) {
    EseIntHashMap *map = int_hashmap_create(NULL);
    /* Insert enough to trigger multiple resizes; verify gets and size */
    enum { N = 1000 };
    for (int i = 0; i < N; i++) {
        int_hashmap_set(map, (uint64_t)(i * 37ULL), alloc_int(i));
    }
    TEST_ASSERT_EQUAL_size_t(N, int_hashmap_size(map));

    for (int i = 0; i < N; i++) {
        int *p = (int *)int_hashmap_get(map, (uint64_t)(i * 37ULL));
        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_EQUAL_INT(i, *p);
    }

    /* Clean up values */
    for (int i = 0; i < N; i++) {
        int *p = (int *)int_hashmap_remove(map, (uint64_t)(i * 37ULL));
        memory_manager.free(p);
    }
    TEST_ASSERT_EQUAL_size_t(0, int_hashmap_size(map));

    int_hashmap_free(map);
}

static void test_int_hashmap_free_fn_called_on_clear_and_free_not_on_remove_or_overwrite(void) {
    g_free_count = 0;
    EseIntHashMap *map = int_hashmap_create(tracked_free);

    int *a = alloc_int(1);
    int *b = alloc_int(2);
    int *c = alloc_int(3);
    int *d = alloc_int(4);

    int_hashmap_set(map, 1ULL, a);
    int_hashmap_set(map, 2ULL, b);
    int_hashmap_set(map, 3ULL, c);
    TEST_ASSERT_EQUAL_size_t(3, int_hashmap_size(map));

    /* Overwrite: free_fn should NOT be called for the old value */
    int prev_free_count = g_free_count;
    int *b2 = alloc_int(22);
    int_hashmap_set(map, 2ULL, b2);
    TEST_ASSERT_EQUAL_size_t(3, int_hashmap_size(map));
    TEST_ASSERT_EQUAL_INT(prev_free_count, g_free_count);
    /* caller must free the overwritten value */
    memory_manager.free(b);

    /* Remove: free_fn should NOT be called, caller must free */
    prev_free_count = g_free_count;
    int *removed = (int *)int_hashmap_remove(map, 1ULL);
    TEST_ASSERT_NOT_NULL(removed);
    TEST_ASSERT_EQUAL_INT(1, *removed);
    TEST_ASSERT_EQUAL_INT(prev_free_count, g_free_count);
    memory_manager.free(removed);

    /* Clear: free_fn should be called for remaining entries */
    prev_free_count = g_free_count;
    int_hashmap_clear(map);
    TEST_ASSERT_EQUAL_size_t(0, int_hashmap_size(map));
    /* Remaining entries were keys 2 and 3 -> two frees */
    TEST_ASSERT_EQUAL_INT(prev_free_count + 2, g_free_count);

    /* Set again and free map: free_fn should be called for each */
    int_hashmap_set(map, 10ULL, d);
    int *e = alloc_int(5);
    int_hashmap_set(map, 11ULL, e);
    prev_free_count = g_free_count;
    int_hashmap_free(map);
    TEST_ASSERT_EQUAL_INT(prev_free_count + 2, g_free_count);
}

/* Additional edge-case tests */
static void test_int_hashmap_iter_allows_null_out_params(void) {
    EseIntHashMap *map = int_hashmap_create(tracked_free);
    int_hashmap_set(map, 1ULL, alloc_int(10));
    EseIntHashMapIter *iter = int_hashmap_iter_create(map);
    /* key and value can be NULL */
    int advanced = int_hashmap_iter_next(iter, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(1, advanced);
    /* next should end */
    TEST_ASSERT_EQUAL_INT(0, int_hashmap_iter_next(iter, NULL, NULL));
    int_hashmap_iter_free(iter);
    int_hashmap_free(map);
}

static void test_int_hashmap_keys_zero_and_uint64_max(void) {
    EseIntHashMap *map = int_hashmap_create(tracked_free);
    int_hashmap_set(map, 0ULL, alloc_int(1));
    int_hashmap_set(map, UINT64_MAX, alloc_int(2));
    TEST_ASSERT_EQUAL_INT(1, *(int *)int_hashmap_get(map, 0ULL));
    TEST_ASSERT_EQUAL_INT(2, *(int *)int_hashmap_get(map, UINT64_MAX));
    int *a = (int *)int_hashmap_remove(map, 0ULL);
    int *b = (int *)int_hashmap_remove(map, UINT64_MAX);
    memory_manager.free(a);
    memory_manager.free(b);
    int_hashmap_free(map);
}

static void test_int_hashmap_set_on_null_map_is_noop(void) {
    /* Should not crash */
    int_hashmap_set(NULL, 123ULL, (void *)0x1);
}


