#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "testing.h"

#include "../src/core/engine.h"
#include "../src/core/engine_private.h"
#include "../src/core/memory_manager.h"
#include "../src/entity/entity.h"
#include "../src/entity/entity_private.h"
#include "../src/types/scene.h"
#include "../src/utility/double_linked_list.h"
#include "../src/utility/log.h"

static EseEngine *g_engine = NULL;

void setUp(void) {
    g_engine = engine_create(NULL);
}

void tearDown(void) {
    engine_destroy(g_engine);
    g_engine = NULL;
}

static void test_scene_snapshot_and_run_core_fields(void) {
    TEST_ASSERT_NOT_NULL(g_engine);

    // Create a persistent entity
    EseEntity *persistent = entity_create(g_engine->lua_engine);
    TEST_ASSERT_NOT_NULL(persistent);
    entity_set_position(persistent, 10.0f, 20.0f);
    entity_set_persistent(persistent, true);
    TEST_ASSERT_TRUE(entity_add_tag(persistent, "PERSISTENT"));
    engine_add_entity(g_engine, persistent);

    // Create a non-persistent entity
    EseEntity *temp = entity_create(g_engine->lua_engine);
    TEST_ASSERT_NOT_NULL(temp);
    entity_set_position(temp, -5.0f, -7.0f);
    entity_set_persistent(temp, false);
    TEST_ASSERT_TRUE(entity_add_tag(temp, "TEMP"));
    engine_add_entity(g_engine, temp);

    // Snapshot only non-persistent entities
    EseScene *scene = ese_scene_create_from_engine(g_engine, false);
    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_EQUAL_size_t(1, ese_scene_entity_count(scene));

    // Clear non-persistent entities and run engine update once to process deletions
    engine_clear_entities(g_engine, false);
    engine_update(g_engine, 0.0f, g_engine->input_state);

    // After clear/update, only the persistent entity should remain
    size_t persistent_count = 0;
    size_t temp_count = 0;
    EseDListIter *iter = dlist_iter_create(g_engine->entities);
    void *value = NULL;
    while (dlist_iter_next(iter, &value)) {
        EseEntity *e = (EseEntity *)value;
        if (entity_get_persistent(e)) {
            persistent_count++;
        }
        if (entity_has_tag(e, "TEMP")) {
            temp_count++;
        }
    }
    dlist_iter_free(iter);

    TEST_ASSERT_EQUAL_size_t(1, persistent_count);
    TEST_ASSERT_EQUAL_size_t(0, temp_count);

    // Re-instantiate the snapshot; this should add a new non-persistent TEMP entity
    ese_scene_run(scene, g_engine);

    persistent_count = 0;
    temp_count = 0;
    iter = dlist_iter_create(g_engine->entities);
    value = NULL;
    while (dlist_iter_next(iter, &value)) {
        EseEntity *e = (EseEntity *)value;
        if (entity_get_persistent(e)) {
            persistent_count++;
        }
        if (entity_has_tag(e, "TEMP")) {
            temp_count++;
        }
    }
    dlist_iter_free(iter);

    TEST_ASSERT_EQUAL_size_t(1, persistent_count);
    TEST_ASSERT_EQUAL_size_t(1, temp_count);

    ese_scene_destroy(scene);
}

int main(void) {
    log_init();

    printf("\nEseScene Tests\n");
    printf("-------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_scene_snapshot_and_run_core_fields);

    memory_manager.destroy(true);

    return UNITY_END();
}
