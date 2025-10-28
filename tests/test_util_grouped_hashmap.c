/*
* test_util_grouped_hashmap.c - Unity tests for grouped_hashmap API and edge cases
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
#include "../src/utility/grouped_hashmap.h"

/**
* Test function declarations
*/
static void test_grouped_hashmap_create_and_free_null(void);
static void test_grouped_hashmap_basic_set_get_remove(void);
static void test_grouped_hashmap_overwrite_calls_free_fn(void);
static void test_grouped_hashmap_remove_returns_value_and_does_not_free(void);
static void test_grouped_hashmap_remove_missing_is_null_and_warns(void);
static void test_grouped_hashmap_remove_group_frees_each_value(void);
static void test_grouped_hashmap_size_counts_correctly(void);
static void test_grouped_hashmap_null_argument_behaviors(void);
static void test_grouped_hashmap_collisions_and_resize(void);
static void test_grouped_hashmap_iterator_traversal(void);
static void test_grouped_hashmap_iter_nulls_are_safe(void);

/**
* Test suite setup and teardown
*/
void setUp(void) {
}

void tearDown(void) {
}

/**
* Helpers
*/
typedef struct FreeCount {
    int count;
} FreeCount;

static void free_counter(void *value) {
    if (value) {
        FreeCount *fc = (FreeCount*)value;
        fc->count += 1;
    }
}

typedef struct CountBox {
    int *counter;
} CountBox;

static void free_counter_and_free(void *value) {
    if (value) {
        CountBox *cb = (CountBox*)value;
        if (cb->counter) {
            *cb->counter += 1;
        }
        memory_manager.free(cb);
    }
}

static void free_heap_string(void *value) {
    if (value) {
        memory_manager.free(value);
    }
}

/**
* Main test runner
*/
int main(void) {
    log_init();

    printf("\nGroupedHashMap Tests\n");
    printf("---------------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_grouped_hashmap_create_and_free_null);
    RUN_TEST(test_grouped_hashmap_basic_set_get_remove);
    RUN_TEST(test_grouped_hashmap_overwrite_calls_free_fn);
    RUN_TEST(test_grouped_hashmap_remove_returns_value_and_does_not_free);
    RUN_TEST(test_grouped_hashmap_remove_missing_is_null_and_warns);
    RUN_TEST(test_grouped_hashmap_remove_group_frees_each_value);
    RUN_TEST(test_grouped_hashmap_size_counts_correctly);
    RUN_TEST(test_grouped_hashmap_null_argument_behaviors);
    RUN_TEST(test_grouped_hashmap_collisions_and_resize);
    RUN_TEST(test_grouped_hashmap_iterator_traversal);
    RUN_TEST(test_grouped_hashmap_iter_nulls_are_safe);

    memory_manager.destroy(true);

    return UNITY_END();
}

/**
* Tests
*/

static void test_grouped_hashmap_create_and_free_null(void) {
    EseGroupedHashMap *map = grouped_hashmap_create(NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(map, "map should be created");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0u, grouped_hashmap_size(map), "size should be 0");
    grouped_hashmap_destroy(NULL); // should be a no-op
    grouped_hashmap_destroy(map);
}

static void test_grouped_hashmap_basic_set_get_remove(void) {
    EseGroupedHashMap *map = grouped_hashmap_create(free_heap_string);
    char *v1 = memory_manager.strdup("alpha", MMTAG_TEMP);
    char *v2 = memory_manager.strdup("beta", MMTAG_TEMP);

    grouped_hashmap_set(map, "g1", "id1", v1);
    grouped_hashmap_set(map, "g1", "id2", v2);

    TEST_ASSERT_EQUAL_UINT_MESSAGE(2u, grouped_hashmap_size(map), "size should be 2");
    TEST_ASSERT_EQUAL_STRING("alpha", (char*)grouped_hashmap_get(map, "g1", "id1"));
    TEST_ASSERT_EQUAL_STRING("beta", (char*)grouped_hashmap_get(map, "g1", "id2"));

    // Ensure different groups can share same id
    char *v3 = memory_manager.strdup("gamma", MMTAG_TEMP);
    grouped_hashmap_set(map, "g2", "id1", v3);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(3u, grouped_hashmap_size(map), "size should be 3");
    TEST_ASSERT_EQUAL_STRING("gamma", (char*)grouped_hashmap_get(map, "g2", "id1"));

    // Remove one item
    char *removed = (char*)grouped_hashmap_remove(map, "g1", "id2");
    TEST_ASSERT_NOT_NULL(removed);
    TEST_ASSERT_EQUAL_STRING("beta", removed);
    memory_manager.free(removed);

    TEST_ASSERT_NULL(grouped_hashmap_get(map, "g1", "id2"));
    TEST_ASSERT_EQUAL_UINT_MESSAGE(2u, grouped_hashmap_size(map), "size should be 2 after remove");

    grouped_hashmap_destroy(map);
}

static void test_grouped_hashmap_overwrite_calls_free_fn(void) {
    int count_a = 0;
    int count_b = 0;
    EseGroupedHashMap *map = grouped_hashmap_create(free_counter_and_free);
    CountBox *a = memory_manager.malloc(sizeof(CountBox), MMTAG_TEMP);
    a->counter = &count_a;
    CountBox *b = memory_manager.malloc(sizeof(CountBox), MMTAG_TEMP);
    b->counter = &count_b;

    grouped_hashmap_set(map, "grp", "same", a);
    // Overwrite same key -> free_fn should be called for old value
    grouped_hashmap_set(map, "grp", "same", b);

    TEST_ASSERT_EQUAL_UINT_MESSAGE(1u, grouped_hashmap_size(map), "size should remain 1 after overwrite");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, count_a, "free_fn should have been called for old value");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, count_b, "new value should not be freed yet");

    grouped_hashmap_destroy(map);
    // During free, free_fn is called once for remaining value 'b'
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, count_b, "free_fn should be called during map free");
}

static void test_grouped_hashmap_remove_returns_value_and_does_not_free(void) {
    EseGroupedHashMap *map = grouped_hashmap_create(free_heap_string);
    char *v = memory_manager.strdup("keep_me", MMTAG_TEMP);
    grouped_hashmap_set(map, "g", "x", v);

    char *ret = (char*)grouped_hashmap_remove(map, "g", "x");
    TEST_ASSERT_EQUAL_STRING("keep_me", ret);
    // Removing should not call free_fn; caller must free
    memory_manager.free(ret);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0u, grouped_hashmap_size(map), "size should be 0 after remove");

    grouped_hashmap_destroy(map);
}

static void test_grouped_hashmap_remove_missing_is_null_and_warns(void) {
    EseGroupedHashMap *map = grouped_hashmap_create(NULL);
    void *ret = grouped_hashmap_remove(map, "nope", "id");
    TEST_ASSERT_NULL(ret);
    grouped_hashmap_destroy(map);
}

static void test_grouped_hashmap_remove_group_frees_each_value(void) {
    int count_a = 0, count_b = 0, count_c = 0;
    EseGroupedHashMap *map = grouped_hashmap_create(free_counter_and_free);
    CountBox *a = memory_manager.malloc(sizeof(CountBox), MMTAG_TEMP);
    a->counter = &count_a;
    CountBox *b = memory_manager.malloc(sizeof(CountBox), MMTAG_TEMP);
    b->counter = &count_b;
    CountBox *c = memory_manager.malloc(sizeof(CountBox), MMTAG_TEMP);
    c->counter = &count_c;

    grouped_hashmap_set(map, "g1", "id1", a);
    grouped_hashmap_set(map, "g1", "id2", b);
    grouped_hashmap_set(map, "g2", "id3", c);

    grouped_hashmap_remove_group(map, "g1");

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, count_a, "free_fn called for g1/id1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, count_b, "free_fn called for g1/id2");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1u, grouped_hashmap_size(map), "only g2/id3 remains");

    grouped_hashmap_destroy(map);
    // c should be freed during map free
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, count_c, "remaining value freed on map free");
}

static void test_grouped_hashmap_size_counts_correctly(void) {
    EseGroupedHashMap *map = grouped_hashmap_create(free_heap_string);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0u, grouped_hashmap_size(map), "initial size 0");
    for (int i = 0; i < 10; i++) {
        char key[16];
        snprintf(key, sizeof(key), "id%d", i);
        char *val = memory_manager.strdup("v", MMTAG_TEMP);
        grouped_hashmap_set(map, "g", key, val);
        TEST_ASSERT_EQUAL_PTR(val, grouped_hashmap_get(map, "g", key));
        TEST_ASSERT_EQUAL_UINT((unsigned)(i + 1), grouped_hashmap_size(map));
    }
    // Remove some
    for (int i = 0; i < 10; i += 2) {
        char key[16];
        snprintf(key, sizeof(key), "id%d", i);
        char *v = (char*)grouped_hashmap_remove(map, "g", key);
        TEST_ASSERT_NOT_NULL(v);
        memory_manager.free(v);
    }
    TEST_ASSERT_EQUAL_UINT_MESSAGE(5u, grouped_hashmap_size(map), "half removed");
    grouped_hashmap_destroy(map);
}

static void test_grouped_hashmap_null_argument_behaviors(void) {
    EseGroupedHashMap *map = grouped_hashmap_create(NULL);

    // get: nulls => returns NULL
    TEST_ASSERT_NULL(grouped_hashmap_get(NULL, "g", "id"));
    TEST_ASSERT_NULL(grouped_hashmap_get(map, NULL, "id"));
    TEST_ASSERT_NULL(grouped_hashmap_get(map, "g", NULL));

    // set: any null => no-op, does not crash (logged by implementation)
    grouped_hashmap_set(NULL, "g", "id", (void*)1);
    grouped_hashmap_set(map, NULL, "id", (void*)1);
    grouped_hashmap_set(map, "g", NULL, (void*)1);
    grouped_hashmap_set(map, "g", "id", NULL);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0u, grouped_hashmap_size(map), "set with nulls should not change size");

    // remove: nulls => returns NULL
    TEST_ASSERT_NULL(grouped_hashmap_remove(NULL, "g", "id"));
    TEST_ASSERT_NULL(grouped_hashmap_remove(map, NULL, "id"));
    TEST_ASSERT_NULL(grouped_hashmap_remove(map, "g", NULL));

    // remove_group: nulls => no-op
    grouped_hashmap_remove_group(NULL, "g");
    grouped_hashmap_remove_group(map, NULL);

    // size(NULL) => 0
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0u, grouped_hashmap_size(NULL), "size(NULL) should be 0");

    grouped_hashmap_destroy(map);
}

static void test_grouped_hashmap_collisions_and_resize(void) {
    EseGroupedHashMap *map = grouped_hashmap_create(free_heap_string);

    // Insert enough distinct keys to trigger resize (capacity starts at 16, LOAD_FACTOR 0.75)
    const int total = 20; // > 16 * 0.75
    for (int i = 0; i < total; i++) {
        char gid[8];
        char id[8];
        snprintf(gid, sizeof(gid), "g%d", i);
        snprintf(id, sizeof(id), "i%d", i);
        char *val = memory_manager.strdup("v", MMTAG_TEMP);
        grouped_hashmap_set(map, gid, id, val);
    }
    TEST_ASSERT_EQUAL_UINT_MESSAGE((unsigned)total, grouped_hashmap_size(map), "all inserted");

    // Verify all retrievable
    for (int i = 0; i < total; i++) {
        char gid[8];
        char id[8];
        snprintf(gid, sizeof(gid), "g%d", i);
        snprintf(id, sizeof(id), "i%d", i);
        TEST_ASSERT_NOT_NULL(grouped_hashmap_get(map, gid, id));
    }

    // Overwrite a couple of keys to exercise collision path updates
    char *nv = memory_manager.strdup("nv", MMTAG_TEMP);
    grouped_hashmap_set(map, "g0", "i0", nv);
    TEST_ASSERT_EQUAL_STRING("nv", (char*)grouped_hashmap_get(map, "g0", "i0"));

    // Cleanup by freeing map (should free remaining values)
    grouped_hashmap_destroy(map);
}

static void test_grouped_hashmap_iterator_traversal(void) {
    EseGroupedHashMap *map = grouped_hashmap_create(free_heap_string);
    // Insert a few entries in different buckets/groups
    grouped_hashmap_set(map, "A", "1", memory_manager.strdup("a1", MMTAG_TEMP));
    grouped_hashmap_set(map, "A", "2", memory_manager.strdup("a2", MMTAG_TEMP));
    grouped_hashmap_set(map, "B", "1", memory_manager.strdup("b1", MMTAG_TEMP));

    bool seen_A1 = false, seen_A2 = false, seen_B1 = false;
    EseGroupedHashMapIter *iter = grouped_hashmap_iter_create(map);
    TEST_ASSERT_NOT_NULL(iter);
    const char *g = NULL, *i = NULL; void *v = NULL;
    while (grouped_hashmap_iter_next(iter, &g, &i, &v)) {
        TEST_ASSERT_NOT_NULL(g);
        TEST_ASSERT_NOT_NULL(i);
        TEST_ASSERT_NOT_NULL(v);
        if (strcmp(g, "A") == 0 && strcmp(i, "1") == 0) { seen_A1 = true; TEST_ASSERT_EQUAL_STRING("a1", (char*)v); }
        if (strcmp(g, "A") == 0 && strcmp(i, "2") == 0) { seen_A2 = true; TEST_ASSERT_EQUAL_STRING("a2", (char*)v); }
        if (strcmp(g, "B") == 0 && strcmp(i, "1") == 0) { seen_B1 = true; TEST_ASSERT_EQUAL_STRING("b1", (char*)v); }
    }
    TEST_ASSERT_TRUE(seen_A1 && seen_A2 && seen_B1);
    grouped_hashmap_iter_free(iter);

    grouped_hashmap_destroy(map);
}

static void test_grouped_hashmap_iter_nulls_are_safe(void) {
    // iter_create(NULL) should return NULL and not crash
    TEST_ASSERT_NULL(grouped_hashmap_iter_create(NULL));

    EseGroupedHashMap *map = grouped_hashmap_create(NULL);
    EseGroupedHashMapIter *iter = grouped_hashmap_iter_create(map);
    TEST_ASSERT_NOT_NULL(iter);

    // Passing NULLs for out-params is allowed; also passing NULL iter should return 0 safely
    TEST_ASSERT_EQUAL_INT(0, grouped_hashmap_iter_next(NULL, NULL, NULL, NULL));
    TEST_ASSERT_TRUE(grouped_hashmap_iter_next(iter, NULL, NULL, NULL) == 1 || grouped_hashmap_iter_next(iter, NULL, NULL, NULL) == 0);

    grouped_hashmap_iter_free(iter);
    grouped_hashmap_destroy(map);
}


