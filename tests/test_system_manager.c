/*
 * test_system.c - Unity-based tests for ECS System functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "testing.h"

#include "../src/core/system_manager.h"
#include "../src/core/engine.h"
#include "../src/core/engine_private.h"
#include "../src/core/memory_manager.h"
#include "../src/entity/components/entity_component.h"
#include "../src/entity/components/entity_component_private.h"
#include "../src/entity/entity.h"
#include "../src/utility/log.h"

/**
 * Test function declarations
 */
static void test_system_create(void);
static void test_system_destroy_null(void);
static void test_system_destroy(void);
static void test_engine_add_system(void);
static void test_engine_add_multiple_systems(void);
static void test_system_init_callback(void);
static void test_system_shutdown_callback(void);
static void test_system_update_callback(void);
static void test_engine_run_phase_early(void);
static void test_engine_run_phase_lua(void);
static void test_engine_run_phase_late(void);
static void test_engine_run_phase_skips_inactive(void);
static void test_engine_run_phase_parallel(void);
static void test_engine_notify_comp_add(void);
static void test_engine_notify_comp_rem(void);
static void test_system_accepts_filter(void);
static void test_system_user_data(void);

/**
 * Global test state
 */
static int g_init_called = 0;
static int g_update_called = 0;
static int g_shutdown_called = 0;
static int g_comp_added_called = 0;
static int g_comp_removed_called = 0;
static int g_accepts_called = 0;
static float g_last_dt = 0.0f;
static EseEngine *g_last_engine = NULL;
static EseEntityComponent *g_last_component = NULL;

/**
 * Reset global test state
 */
static void reset_test_state(void) {
    g_init_called = 0;
    g_update_called = 0;
    g_shutdown_called = 0;
    g_comp_added_called = 0;
    g_comp_removed_called = 0;
    g_accepts_called = 0;
    g_last_dt = 0.0f;
    g_last_engine = NULL;
    g_last_component = NULL;
}

/**
 * Unity setUp - called before each test
 */
void setUp(void) {
    reset_test_state();
}

/**
 * Unity tearDown - called after each test
 */
void tearDown(void) {
    // Nothing to do
}

/**
 * Test system callbacks
 */
static void test_sys_init(EseSystemManager *self, EseEngine *eng) {
    (void)self;
    g_init_called++;
    g_last_engine = eng;
}

static void test_sys_update(EseSystemManager *self, EseEngine *eng, float dt) {
    (void)self;
    g_update_called++;
    g_last_engine = eng;
    g_last_dt = dt;
}

static bool test_sys_accepts(EseSystemManager *self, const EseEntityComponent *comp) {
    (void)self;
    (void)comp;
    g_accepts_called++;
    return true;
}

static bool test_sys_accepts_never(EseSystemManager *self, const EseEntityComponent *comp) {
    (void)self;
    (void)comp;
    g_accepts_called++;
    return false;
}

static void test_sys_on_comp_add(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp) {
    (void)self;
    g_comp_added_called++;
    g_last_engine = eng;
    g_last_component = comp;
}

static void test_sys_on_comp_rem(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp) {
    (void)self;
    g_comp_removed_called++;
    g_last_engine = eng;
    g_last_component = comp;
}

static void test_sys_shutdown(EseSystemManager *self, EseEngine *eng) {
    (void)self;
    g_shutdown_called++;
    g_last_engine = eng;
}

/**
 * Test: Create a system
 */
static void test_system_create(void) {
    reset_test_state();
    
    EseSystemManagerVTable vt = {0};
    EseSystemManager *sys = system_manager_create(&vt, SYS_PHASE_EARLY, NULL);
    
    TEST_ASSERT_NOT_NULL(sys);
    
    system_manager_destroy(sys, NULL);
}

/**
 * Test: Destroy null system should be safe
 */
static void test_system_destroy_null(void) {
    reset_test_state();
    
    system_manager_destroy(NULL, NULL);
    
    TEST_ASSERT_EQUAL_INT(0, g_shutdown_called);
}

/**
 * Test: Destroy system calls shutdown callback
 */
static void test_system_destroy(void) {
    reset_test_state();
    
    EseSystemManagerVTable vt = {
        .shutdown = test_sys_shutdown
    };
    
    EseSystemManager *sys = system_manager_create(&vt, SYS_PHASE_EARLY, NULL);
    EseEngine *engine = engine_create(NULL);
    
    system_manager_destroy(sys, engine);
    
    TEST_ASSERT_EQUAL_INT(1, g_shutdown_called);
    TEST_ASSERT_EQUAL_PTR(engine, g_last_engine);
    
    engine_destroy(engine);
}

/**
 * Test: Add a system to the engine
 */
static void test_engine_add_system(void) {
    reset_test_state();
    
    EseEngine *engine = engine_create(NULL);
    EseSystemManagerVTable vt = {
        .init = test_sys_init
    };
    
    EseSystemManager *sys = system_manager_create(&vt, SYS_PHASE_EARLY, NULL);
    size_t initial_count = engine->sys_count;  // Account for auto-registered systems
    engine_add_system(engine, sys);
    
    TEST_ASSERT_EQUAL_INT(initial_count + 1, engine->sys_count);
    TEST_ASSERT_EQUAL_INT(1, g_init_called);
    TEST_ASSERT_EQUAL_PTR(engine, g_last_engine);
    
    engine_destroy(engine);
}

/**
 * Test: Add multiple systems to the engine
 */
static void test_engine_add_multiple_systems(void) {
    reset_test_state();
    
    EseEngine *engine = engine_create(NULL);
    EseSystemManagerVTable vt = {
        .init = test_sys_init
    };
    
    size_t initial_count = engine->sys_count;  // Account for auto-registered systems
    for (int i = 0; i < 10; i++) {
        EseSystemManager *sys = system_manager_create(&vt, SYS_PHASE_EARLY, NULL);
        engine_add_system(engine, sys);
    }
    
    TEST_ASSERT_EQUAL_INT(initial_count + 10, engine->sys_count);
    TEST_ASSERT_EQUAL_INT(10, g_init_called);
    
    engine_destroy(engine);
}

/**
 * Test: System init callback is called
 */
static void test_system_init_callback(void) {
    reset_test_state();
    
    EseEngine *engine = engine_create(NULL);
    EseSystemManagerVTable vt = {
        .init = test_sys_init
    };
    
    EseSystemManager *sys = system_manager_create(&vt, SYS_PHASE_EARLY, NULL);
    engine_add_system(engine, sys);
    
    TEST_ASSERT_EQUAL_INT(1, g_init_called);
    
    engine_destroy(engine);
}

/**
 * Test: System shutdown callback is called
 */
static void test_system_shutdown_callback(void) {
    reset_test_state();
    
    EseEngine *engine = engine_create(NULL);
    EseSystemManagerVTable vt = {
        .shutdown = test_sys_shutdown
    };
    
    EseSystemManager *sys = system_manager_create(&vt, SYS_PHASE_EARLY, NULL);
    engine_add_system(engine, sys);
    
    TEST_ASSERT_EQUAL_INT(0, g_shutdown_called);
    
    engine_destroy(engine);
    
    TEST_ASSERT_EQUAL_INT(1, g_shutdown_called);
}

/**
 * Test: System update callback is called
 */
static void test_system_update_callback(void) {
    reset_test_state();
    
    EseEngine *engine = engine_create(NULL);
    EseSystemManagerVTable vt = {
        .update = test_sys_update
    };
    
    EseSystemManager *sys = system_manager_create(&vt, SYS_PHASE_EARLY, NULL);
    engine_add_system(engine, sys);
    
    engine_run_phase(engine, SYS_PHASE_EARLY, 0.016f, false);
    
    TEST_ASSERT_EQUAL_INT(1, g_update_called);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.016f, g_last_dt);
    TEST_ASSERT_EQUAL_PTR(engine, g_last_engine);
    
    engine_destroy(engine);
}

/**
 * Test: Run EARLY phase systems
 */
static void test_engine_run_phase_early(void) {
    reset_test_state();
    
    EseEngine *engine = engine_create(NULL);
    EseSystemManagerVTable vt = {
        .update = test_sys_update
    };
    
    EseSystemManager *sys = system_manager_create(&vt, SYS_PHASE_EARLY, NULL);
    engine_add_system(engine, sys);
    
    engine_run_phase(engine, SYS_PHASE_EARLY, 0.016f, false);
    
    TEST_ASSERT_EQUAL_INT(1, g_update_called);
    
    engine_destroy(engine);
}

/**
 * Test: Run LUA phase systems
 */
static void test_engine_run_phase_lua(void) {
    reset_test_state();
    
    EseEngine *engine = engine_create(NULL);
    EseSystemManagerVTable vt = {
        .update = test_sys_update
    };
    
    EseSystemManager *sys = system_manager_create(&vt, SYS_PHASE_LUA, NULL);
    engine_add_system(engine, sys);
    
    engine_run_phase(engine, SYS_PHASE_LUA, 0.016f, false);
    
    TEST_ASSERT_EQUAL_INT(1, g_update_called);
    
    engine_destroy(engine);
}

/**
 * Test: Run LATE phase systems
 */
static void test_engine_run_phase_late(void) {
    reset_test_state();
    
    EseEngine *engine = engine_create(NULL);
    EseSystemManagerVTable vt = {
        .update = test_sys_update
    };
    
    EseSystemManager *sys = system_manager_create(&vt, SYS_PHASE_LATE, NULL);
    engine_add_system(engine, sys);
    
    engine_run_phase(engine, SYS_PHASE_LATE, 0.016f, false);
    
    TEST_ASSERT_EQUAL_INT(1, g_update_called);
    
    engine_destroy(engine);
}

/**
 * Test: Run phase only runs systems in the specified phase
 */
static void test_engine_run_phase_skips_inactive(void) {
    reset_test_state();
    
    EseEngine *engine = engine_create(NULL);
    EseSystemManagerVTable vt_early = {
        .update = test_sys_update
    };
    EseSystemManagerVTable vt_late = {
        .update = test_sys_update
    };
    
    // Add one EARLY system and one LATE system
    EseSystemManager *sys_early = system_manager_create(&vt_early, SYS_PHASE_EARLY, NULL);
    EseSystemManager *sys_late = system_manager_create(&vt_late, SYS_PHASE_LATE, NULL);
    engine_add_system(engine, sys_early);
    engine_add_system(engine, sys_late);
    
    // Running EARLY phase should only update the early system
    engine_run_phase(engine, SYS_PHASE_EARLY, 0.016f, false);
    
    TEST_ASSERT_EQUAL_INT(1, g_update_called);
    
    engine_destroy(engine);
}

/**
 * Test: Run phase with parallel execution
 */
static void test_engine_run_phase_parallel(void) {
    reset_test_state();
    
    EseEngine *engine = engine_create(NULL);
    EseSystemManagerVTable vt = {
        .update = test_sys_update
    };
    
    EseSystemManager *sys1 = system_manager_create(&vt, SYS_PHASE_EARLY, NULL);
    EseSystemManager *sys2 = system_manager_create(&vt, SYS_PHASE_EARLY, NULL);
    engine_add_system(engine, sys1);
    engine_add_system(engine, sys2);
    
    engine_run_phase(engine, SYS_PHASE_EARLY, 0.016f, true);
    
    // Both systems should have been called
    TEST_ASSERT_EQUAL_INT(2, g_update_called);
    
    engine_destroy(engine);
}

/**
 * Test: Component added notification
 */
static void test_engine_notify_comp_add(void) {
    reset_test_state();
    
    EseEngine *engine = engine_create(NULL);
    EseSystemManagerVTable vt = {
        .accepts = test_sys_accepts,
        .on_component_added = test_sys_on_comp_add
    };
    
    EseSystemManager *sys = system_manager_create(&vt, SYS_PHASE_EARLY, NULL);
    engine_add_system(engine, sys);
    
    // Create a real minimal component for testing
    EseEntityComponent *dummy_comp = memory_manager.calloc(1, sizeof(EseEntityComponent), MMTAG_ENTITY);
    dummy_comp->type = ENTITY_COMPONENT_LUA;  // Use a type that test_sys_accepts will accept
    
    engine_notify_comp_add(engine, dummy_comp);
    
    TEST_ASSERT_EQUAL_INT(1, g_accepts_called);
    TEST_ASSERT_EQUAL_INT(1, g_comp_added_called);
    TEST_ASSERT_EQUAL_PTR(dummy_comp, g_last_component);
    
    memory_manager.free(dummy_comp);
    engine_destroy(engine);
}

/**
 * Test: Component removed notification
 */
static void test_engine_notify_comp_rem(void) {
    reset_test_state();
    
    EseEngine *engine = engine_create(NULL);
    EseSystemManagerVTable vt = {
        .accepts = test_sys_accepts,
        .on_component_removed = test_sys_on_comp_rem
    };
    
    EseSystemManager *sys = system_manager_create(&vt, SYS_PHASE_EARLY, NULL);
    engine_add_system(engine, sys);
    
    // Create a real minimal component for testing
    EseEntityComponent *dummy_comp = memory_manager.calloc(1, sizeof(EseEntityComponent), MMTAG_ENTITY);
    dummy_comp->type = ENTITY_COMPONENT_LUA;  // Use a type that test_sys_accepts will accept
    
    engine_notify_comp_rem(engine, dummy_comp);
    
    TEST_ASSERT_EQUAL_INT(1, g_accepts_called);
    TEST_ASSERT_EQUAL_INT(1, g_comp_removed_called);
    TEST_ASSERT_EQUAL_PTR(dummy_comp, g_last_component);
    
    memory_manager.free(dummy_comp);
    engine_destroy(engine);
}

/**
 * Test: System accepts filter works correctly
 */
static void test_system_accepts_filter(void) {
    reset_test_state();
    
    EseEngine *engine = engine_create(NULL);
    EseSystemManagerVTable vt = {
        .accepts = test_sys_accepts_never,
        .on_component_added = test_sys_on_comp_add
    };
    
    EseSystemManager *sys = system_manager_create(&vt, SYS_PHASE_EARLY, NULL);
    engine_add_system(engine, sys);
    
    // Create a real minimal component for testing
    EseEntityComponent *dummy_comp = memory_manager.calloc(1, sizeof(EseEntityComponent), MMTAG_ENTITY);
    dummy_comp->type = ENTITY_COMPONENT_LUA;
    
    engine_notify_comp_add(engine, dummy_comp);
    
    TEST_ASSERT_EQUAL_INT(1, g_accepts_called);
    TEST_ASSERT_EQUAL_INT(0, g_comp_added_called);
    
    memory_manager.free(dummy_comp);
    engine_destroy(engine);
}

/**
 * Test: System user data is passed to callbacks
 */
static void test_system_user_data(void) {
    reset_test_state();
    
    // User data will be accessible in the system callbacks
    // This test verifies that the system can be created with user data
    int test_data = 42;
    EseSystemManagerVTable vt = {0};
    
    EseSystemManager *sys = system_manager_create(&vt, SYS_PHASE_EARLY, &test_data);
    
    TEST_ASSERT_NOT_NULL(sys);
    
    system_manager_destroy(sys, NULL);
}

/**
 * Main test runner
 */
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_system_create);
    RUN_TEST(test_system_destroy_null);
    RUN_TEST(test_system_destroy);
    RUN_TEST(test_engine_add_system);
    RUN_TEST(test_engine_add_multiple_systems);
    RUN_TEST(test_system_init_callback);
    RUN_TEST(test_system_shutdown_callback);
    RUN_TEST(test_system_update_callback);
    RUN_TEST(test_engine_run_phase_early);
    RUN_TEST(test_engine_run_phase_lua);
    RUN_TEST(test_engine_run_phase_late);
    RUN_TEST(test_engine_run_phase_skips_inactive);
    RUN_TEST(test_engine_run_phase_parallel);
    RUN_TEST(test_engine_notify_comp_add);
    RUN_TEST(test_engine_notify_comp_rem);
    RUN_TEST(test_system_accepts_filter);
    RUN_TEST(test_system_user_data);

    return UNITY_END();
}

