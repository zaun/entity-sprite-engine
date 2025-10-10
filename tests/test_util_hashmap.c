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
#include "../src/utility/hashmap.h"
#include "../src/utility/log.h"

/**
* C API Test Functions Declarations
*/
static void test_hashmap_create_and_free(void);
static void test_hashmap_set_get_basic(void);
static void test_hashmap_update_existing_key(void);
static void test_hashmap_remove_and_return_value(void);
static void test_hashmap_clear(void);
static void test_hashmap_size_and_empty_cases(void);
static void test_hashmap_iterate_all_entries(void);
static void test_hashmap_resize_rehashing(void);
static void test_hashmap_null_arguments(void);
static void test_hashmap_value_free_function_on_clear_and_free(void);
static void test_hashmap_remove_does_not_free_value(void);
static void test_hashmap_keys_are_copied_and_independent(void);
static void test_hashmap_get_unknown_key_returns_null(void);
static void test_hashmap_iter_handles_empty_and_progression(void);

/**
* Helpers
*/
typedef struct FreeTracker {
    int freed_count;
} FreeTracker;

static FreeTracker g_tracker;

static void tracked_free(void *value) {
    if (value) {
        g_tracker.freed_count++;
        memory_manager.free(value);
    }
}

static char *dupstr(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = memory_manager.malloc(n, MMTAG_TEMP);
    memcpy(p, s, n);
    return p;
}

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

/**
* Main test runner
*/
int main(void) {
    log_init();

    printf("\nEseHashMap Tests\n");
    printf("-----------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_hashmap_create_and_free);
    RUN_TEST(test_hashmap_set_get_basic);
    RUN_TEST(test_hashmap_update_existing_key);
    RUN_TEST(test_hashmap_remove_and_return_value);
    RUN_TEST(test_hashmap_clear);
    RUN_TEST(test_hashmap_size_and_empty_cases);
    RUN_TEST(test_hashmap_iterate_all_entries);
    RUN_TEST(test_hashmap_resize_rehashing);
    RUN_TEST(test_hashmap_null_arguments);
    RUN_TEST(test_hashmap_value_free_function_on_clear_and_free);
    RUN_TEST(test_hashmap_remove_does_not_free_value);
    RUN_TEST(test_hashmap_keys_are_copied_and_independent);
    RUN_TEST(test_hashmap_get_unknown_key_returns_null);
    RUN_TEST(test_hashmap_iter_handles_empty_and_progression);

    memory_manager.destroy();

    return UNITY_END();
}

/**
* Tests
*/

static void test_hashmap_create_and_free(void) {
    EseHashMap *map = hashmap_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(map, "hashmap_create should return a map");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, hashmap_size(map), "new map size should be 0");
    hashmap_destroy(map);
}

static void test_hashmap_set_get_basic(void) {
    EseHashMap *map = hashmap_create(NULL);

    int *a = memory_manager.malloc(sizeof(int), MMTAG_TEMP);
    int *b = memory_manager.malloc(sizeof(int), MMTAG_TEMP);
    *a = 42; *b = 7;

    hashmap_set(map, "alpha", a);
    hashmap_set(map, "beta", b);

    int *ga = (int *)hashmap_get(map, "alpha");
    int *gb = (int *)hashmap_get(map, "beta");

    TEST_ASSERT_NOT_NULL(ga);
    TEST_ASSERT_NOT_NULL(gb);
    TEST_ASSERT_EQUAL_INT(42, *ga);
    TEST_ASSERT_EQUAL_INT(7, *gb);
    TEST_ASSERT_EQUAL_UINT(2, hashmap_size(map));

    // cleanup (values owned by test since free_fn is NULL)
    hashmap_clear(map);
    memory_manager.free(a);
    memory_manager.free(b);
    hashmap_destroy(map);
}

static void test_hashmap_update_existing_key(void) {
    EseHashMap *map = hashmap_create(NULL);

    int *a = memory_manager.malloc(sizeof(int), MMTAG_TEMP);
    int *b = memory_manager.malloc(sizeof(int), MMTAG_TEMP);
    *a = 1; *b = 2;

    hashmap_set(map, "k", a);
    TEST_ASSERT_EQUAL_UINT(1, hashmap_size(map));
    TEST_ASSERT_EQUAL_INT(1, *(int*)hashmap_get(map, "k"));

    // update same key with new pointer; old value not auto-freed when free_fn is NULL
    hashmap_set(map, "k", b);
    TEST_ASSERT_EQUAL_UINT(1, hashmap_size(map));
    TEST_ASSERT_EQUAL_INT(2, *(int*)hashmap_get(map, "k"));

    // cleanup
    hashmap_clear(map);
    memory_manager.free(a);
    memory_manager.free(b);
    hashmap_destroy(map);
}

static void test_hashmap_remove_and_return_value(void) {
    EseHashMap *map = hashmap_create(NULL);

    int *a = memory_manager.malloc(sizeof(int), MMTAG_TEMP);
    *a = 11;

    hashmap_set(map, "k", a);
    TEST_ASSERT_NOT_NULL(hashmap_get(map, "k"));

    void *removed = hashmap_remove(map, "k");
    TEST_ASSERT_EQUAL_PTR(a, removed);
    TEST_ASSERT_EQUAL_UINT(0, hashmap_size(map));
    TEST_ASSERT_NULL(hashmap_get(map, "k"));

    memory_manager.free(removed);
    hashmap_destroy(map);
}

static void test_hashmap_clear(void) {
    EseHashMap *map = hashmap_create(NULL);

    int *a = memory_manager.malloc(sizeof(int), MMTAG_TEMP);
    int *b = memory_manager.malloc(sizeof(int), MMTAG_TEMP);
    *a = 3; *b = 4;

    hashmap_set(map, "a", a);
    hashmap_set(map, "b", b);
    TEST_ASSERT_EQUAL_UINT(2, hashmap_size(map));

    hashmap_clear(map);
    TEST_ASSERT_EQUAL_UINT(0, hashmap_size(map));
    TEST_ASSERT_NULL(hashmap_get(map, "a"));
    TEST_ASSERT_NULL(hashmap_get(map, "b"));

    // values are not freed when free_fn is NULL
    memory_manager.free(a);
    memory_manager.free(b);
    hashmap_destroy(map);
}

static void test_hashmap_size_and_empty_cases(void) {
    EseHashMap *map = hashmap_create(NULL);
    TEST_ASSERT_EQUAL_UINT(0, hashmap_size(map));

    // unknown key
    TEST_ASSERT_NULL(hashmap_get(map, "missing"));

    // remove unknown
    TEST_ASSERT_NULL(hashmap_remove(map, "missing"));

    hashmap_destroy(map);
}

static void test_hashmap_iterate_all_entries(void) {
    EseHashMap *map = hashmap_create(NULL);

    // insert enough to ensure chaining happens likely and multiple buckets
    const int N = 100;
    int **values = memory_manager.malloc(sizeof(int*) * N, MMTAG_TEMP);
    for (int i = 0; i < N; i++) {
        values[i] = memory_manager.malloc(sizeof(int), MMTAG_TEMP);
        *values[i] = i;
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        hashmap_set(map, key, values[i]);
    }

    size_t count = 0;
    EseHashMapIter *iter = hashmap_iter_create(map);
    TEST_ASSERT_NOT_NULL(iter);
    const char *k;
    void *v;
    while (hashmap_iter_next(iter, &k, &v)) {
        TEST_ASSERT_NOT_NULL(k);
        TEST_ASSERT_NOT_NULL(v);
        count++;
    }
    TEST_ASSERT_EQUAL_UINT(N, count);
    hashmap_iter_free(iter);

    // cleanup
    for (int i = 0; i < N; i++) memory_manager.free(values[i]);
    memory_manager.free(values);
    hashmap_destroy(map);
}

static void test_hashmap_resize_rehashing(void) {
    EseHashMap *map = hashmap_create(NULL);

    // Insert many to trigger resize
    const int N = 200;
    int **values = memory_manager.malloc(sizeof(int*) * N, MMTAG_TEMP);
    for (int i = 0; i < N; i++) {
        values[i] = memory_manager.malloc(sizeof(int), MMTAG_TEMP);
        *values[i] = i * 2 + 1;
        char key[32];
        snprintf(key, sizeof(key), "k%03d", i);
        hashmap_set(map, key, values[i]);
    }

    // Verify after presumed resizes, all are still retrievable
    for (int i = 0; i < N; i++) {
        char key[32];
        snprintf(key, sizeof(key), "k%03d", i);
        int *v = (int*)hashmap_get(map, key);
        TEST_ASSERT_NOT_NULL(v);
        TEST_ASSERT_EQUAL_INT(i * 2 + 1, *v);
    }

    TEST_ASSERT_EQUAL_UINT(N, hashmap_size(map));

    // cleanup
    for (int i = 0; i < N; i++) memory_manager.free(values[i]);
    memory_manager.free(values);
    hashmap_destroy(map);
}

static void test_hashmap_null_arguments(void) {
    // Map NULL
    TEST_ASSERT_NULL(hashmap_get(NULL, "x"));
    TEST_ASSERT_NULL(hashmap_remove(NULL, "x"));
    TEST_ASSERT_EQUAL_UINT(0, hashmap_size(NULL));

    EseHashMap *map = hashmap_create(NULL);

    // NULL key/value behaviors
    // set ignores NULLs
    hashmap_set(map, NULL, (void*)1);
    hashmap_set(map, "x", NULL);

    // get/remove with NULL key return NULL
    TEST_ASSERT_NULL(hashmap_get(map, NULL));
    TEST_ASSERT_NULL(hashmap_remove(map, NULL));

    // iterator on NULL
    TEST_ASSERT_NULL(hashmap_iter_create(NULL));

    hashmap_destroy(map);
}

static void test_hashmap_value_free_function_on_clear_and_free(void) {
    EseHashMap *map = hashmap_create(tracked_free);
    g_tracker.freed_count = 0;

    char *v1 = dupstr("v1");
    char *v2 = dupstr("v2");
    hashmap_set(map, "a", v1);
    hashmap_set(map, "b", v2);

    TEST_ASSERT_EQUAL_UINT(2, hashmap_size(map));

    // clear should free both values via tracked_free
    hashmap_clear(map);
    TEST_ASSERT_EQUAL_UINT(0, hashmap_size(map));
    TEST_ASSERT_EQUAL_INT(2, g_tracker.freed_count);

    // set again then free(map) should free remaining values
    g_tracker.freed_count = 0;
    char *v3 = dupstr("v3");
    char *v4 = dupstr("v4");
    hashmap_set(map, "c", v3);
    hashmap_set(map, "d", v4);

    hashmap_destroy(map);
    TEST_ASSERT_EQUAL_INT(2, g_tracker.freed_count);
}

static void test_hashmap_remove_does_not_free_value(void) {
    EseHashMap *map = hashmap_create(tracked_free);
    g_tracker.freed_count = 0;

    char *v = dupstr("hello");
    hashmap_set(map, "k", v);

    void *removed = hashmap_remove(map, "k");
    TEST_ASSERT_EQUAL_PTR(v, removed);
    TEST_ASSERT_EQUAL_INT(0, g_tracker.freed_count);
    memory_manager.free(removed);

    hashmap_destroy(map);
}

static void test_hashmap_keys_are_copied_and_independent(void) {
    EseHashMap *map = hashmap_create(NULL);

    char keybuf[16];
    strcpy(keybuf, "temp");

    int *val = memory_manager.malloc(sizeof(int), MMTAG_TEMP);
    *val = 9;
    hashmap_set(map, keybuf, val);

    // mutate original buffer after insertion
    strcpy(keybuf, "other");

    int *got = (int*)hashmap_get(map, "temp");
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL_INT(9, *got);

    // cleanup
    hashmap_clear(map);
    memory_manager.free(val);
    hashmap_destroy(map);
}

static void test_hashmap_get_unknown_key_returns_null(void) {
    EseHashMap *map = hashmap_create(NULL);
    TEST_ASSERT_NULL(hashmap_get(map, "nope"));
    hashmap_destroy(map);
}

static void test_hashmap_iter_handles_empty_and_progression(void) {
    EseHashMap *map = hashmap_create(NULL);

    EseHashMapIter *iter = hashmap_iter_create(map);
    TEST_ASSERT_NOT_NULL(iter);
    const char *k = NULL; void *v = NULL;
    TEST_ASSERT_EQUAL_INT(0, hashmap_iter_next(iter, &k, &v));
    hashmap_iter_free(iter);

    // add a few and iterate
    int *a = memory_manager.malloc(sizeof(int), MMTAG_TEMP); *a = 1;
    int *b = memory_manager.malloc(sizeof(int), MMTAG_TEMP); *b = 2;
    int *c = memory_manager.malloc(sizeof(int), MMTAG_TEMP); *c = 3;

    hashmap_set(map, "a", a);
    hashmap_set(map, "b", b);
    hashmap_set(map, "c", c);

    iter = hashmap_iter_create(map);
    int seen = 0;
    while (hashmap_iter_next(iter, NULL, NULL)) {
        seen++;
    }
    TEST_ASSERT_EQUAL_INT(3, seen);
    hashmap_iter_free(iter);

    // cleanup
    hashmap_clear(map);
    memory_manager.free(a);
    memory_manager.free(b);
    memory_manager.free(c);
    hashmap_destroy(map);
}
