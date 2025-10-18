/*
* test_job_queue.c - Unity-based tests for utility/job_queue
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "testing.h"

#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"
#include "../src/utility/thread.h"
#include "../src/utility/job_queue.h"

/* =====================
 * Helpers & Fixtures
 * ===================== */

/* Forward declarations */
static void drain_callbacks(EseJobQueue *q);

typedef struct WorkerInitData {
    ese_worker_id_t worker_id;
} WorkerInitData;

typedef struct JobBlocker {
    EseMutex *mutex;
    EseCond *cond;
    bool go;
    bool started;
} JobBlocker;

typedef struct IntBox {
    int value;
} IntBox;

static int g_callback_count = 0;
static int g_cleanup_count = 0;
static int g_last_callback_result = -999999;

static void reset_globals(void) {
    g_callback_count = 0;
    g_cleanup_count = 0;
    g_last_callback_result = -999999;
}

static void *worker_init(ese_worker_id_t worker_id) {
    WorkerInitData *d = (WorkerInitData *)memory_manager.malloc(sizeof(WorkerInitData), MMTAG_THREAD);
    d->worker_id = worker_id;
    return d;
}

static void worker_deinit(ese_worker_id_t worker_id, void *thread_data) {
    (void)worker_id;
    if (thread_data) {
        memory_manager.free(thread_data);
    }
}

/* Job that returns user_data->value + worker_id */
static void *job_add_worker_id(void *thread_data, void *user_data) {
    WorkerInitData *wd = (WorkerInitData *)thread_data;
    IntBox *in = (IntBox *)user_data;
    IntBox *out = (IntBox *)memory_manager.malloc(sizeof(IntBox), MMTAG_TEMP);
    int wid = wd ? (int)wd->worker_id : 0;
    out->value = (in ? in->value : 0) + wid;
    return out;
}

/* Job that blocks until JobBlocker.go is set */
static void *job_block_until_go(void *thread_data, void *user_data) {
    (void)thread_data;
    JobBlocker *blk = (JobBlocker *)user_data;
    ese_mutex_lock(blk->mutex);
    blk->started = true;
    while (!blk->go) {
        ese_cond_wait(blk->cond, blk->mutex);
    }
    ese_mutex_unlock(blk->mutex);
    IntBox *out = (IntBox *)memory_manager.malloc(sizeof(IntBox), MMTAG_TEMP);
    out->value = 12345;
    return out;
}

/* Simple immediate job: returns user_data->value * 2 */
static void *job_double(void *thread_data, void *user_data) {
    (void)thread_data;
    IntBox *in = (IntBox *)user_data;
    IntBox *out = (IntBox *)memory_manager.malloc(sizeof(IntBox), MMTAG_TEMP);
    out->value = in ? (in->value * 2) : 0;
    return out;
}

static void main_callback(ese_job_id_t job_id, void *result) {
    (void)job_id;
    IntBox *out = (IntBox *)result;
    g_callback_count++;
    g_last_callback_result = out ? out->value : -111111;
}

/* Cleanup for IntBox jobs */
static void cleanup_intbox(ese_job_id_t job_id, void *user_data, void *result) {
    (void)job_id;
    if (result) memory_manager.free(result);
    if (user_data) memory_manager.free(user_data);
    g_cleanup_count++;
}

/* Cleanup for JobBlocker jobs: destroy sync primitives and free */
static void cleanup_jobblocker(ese_job_id_t job_id, void *user_data, void *result) {
    (void)job_id;
    if (result) memory_manager.free(result);
    if (user_data) {
        JobBlocker *blk = (JobBlocker *)user_data;
        if (blk->mutex) ese_mutex_destroy(blk->mutex);
        if (blk->cond) ese_cond_destroy(blk->cond);
        memory_manager.free(blk);
    }
    g_cleanup_count++;
}

/* =====================
 * Test Declarations
 * ===================== */
static void test_create_and_destroy(void);
static void test_push_any_and_poll_callbacks(void);
static void test_push_on_specific_worker(void);
static void test_status_and_wait_paths(void);
static void test_cancel_before_start(void);
static void test_cancel_during_run(void);
static void test_wait_timeout_then_complete(void);
static void test_invalid_worker_id_and_null_callback_cleanup(void);
static void test_status_wait_unknown_job_id(void);
static void test_cancel_after_completion_and_double_cancel(void);
static void test_poll_when_empty_returns_zero(void);
static void test_cancel_lock_order_no_deadlock(void);
static void test_thread_detach_no_use_after_free(void);

/* =====================
 * Unity setUp/tearDown
 * ===================== */
void setUp(void) {}
void tearDown(void) {}

/* =====================
 * Test Runner
 * ===================== */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    log_init();

    printf("\nJobQueue Tests\n");
    printf("--------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_create_and_destroy);
    RUN_TEST(test_push_any_and_poll_callbacks);
    RUN_TEST(test_push_on_specific_worker);
    RUN_TEST(test_status_and_wait_paths);
    RUN_TEST(test_cancel_before_start);
    RUN_TEST(test_cancel_during_run);
    RUN_TEST(test_wait_timeout_then_complete);
    RUN_TEST(test_invalid_worker_id_and_null_callback_cleanup);
    RUN_TEST(test_status_wait_unknown_job_id);
    RUN_TEST(test_cancel_after_completion_and_double_cancel);
    RUN_TEST(test_poll_when_empty_returns_zero);
    RUN_TEST(test_cancel_lock_order_no_deadlock);
    RUN_TEST(test_thread_detach_no_use_after_free);

    memory_manager.destroy();

    return UNITY_END();
}

/* =====================
 * Tests
 * ===================== */

static void test_create_and_destroy(void) {
    reset_globals();
    EseJobQueue *q = ese_job_queue_create(3, worker_init, worker_deinit);
    TEST_ASSERT_NOT_NULL(q);
    ese_job_queue_destroy(q);
}

static void test_invalid_worker_id_and_null_callback_cleanup(void) {
    reset_globals();
    EseJobQueue *q = ese_job_queue_create(2, worker_init, worker_deinit);

    /* invalid worker id -> not queued */
    IntBox *in1 = (IntBox *)memory_manager.malloc(sizeof(IntBox), MMTAG_TEMP);
    in1->value = 1;
    ese_job_id_t bad = ese_job_queue_push_on_worker(q, 99, job_double, NULL, cleanup_intbox, in1);
    TEST_ASSERT_EQUAL_INT(ESE_JOB_NOT_QUEUED, bad);
    /* We allocated in1; since not queued, cleanup wasn't called. Free it now. */
    memory_manager.free(in1);

    /* null callback is allowed; cleanup must run */
    IntBox *in2 = (IntBox *)memory_manager.malloc(sizeof(IntBox), MMTAG_TEMP);
    in2->value = 3;
    ese_job_id_t id = ese_job_queue_push(q, job_double, NULL, cleanup_intbox, in2);
    TEST_ASSERT_TRUE(id > 0);
    int rc = ese_job_queue_wait_for_completion(q, id, 500);
    TEST_ASSERT_EQUAL_INT(ESE_JOB_COMPLETED, rc);
    /* poll should remove job and call cleanup; callback_count remains unchanged */
    int polled = ese_job_queue_poll_callbacks(q);
    TEST_ASSERT_EQUAL_INT(0, polled);
    TEST_ASSERT_EQUAL_INT(0, g_callback_count);
    TEST_ASSERT_EQUAL_INT(1, g_cleanup_count);

    ese_job_queue_destroy(q);
}

static void test_status_wait_unknown_job_id(void) {
    reset_globals();
    EseJobQueue *q = ese_job_queue_create(1, worker_init, worker_deinit);
    TEST_ASSERT_EQUAL_INT(ESE_JOB_NOT_FOUND, ese_job_queue_status(q, 123456));
    TEST_ASSERT_EQUAL_INT(ESE_JOB_NOT_FOUND, ese_job_queue_wait_for_completion(q, 123456, 10));
    ese_job_queue_destroy(q);
}

static void test_cancel_after_completion_and_double_cancel(void) {
    reset_globals();
    EseJobQueue *q = ese_job_queue_create(1, worker_init, worker_deinit);
    IntBox *in = (IntBox *)memory_manager.malloc(sizeof(IntBox), MMTAG_TEMP);
    in->value = 2;
    ese_job_id_t id = ese_job_queue_push(q, job_double, main_callback, cleanup_intbox, in);
    TEST_ASSERT_TRUE(id > 0);
    int rc = ese_job_queue_wait_for_completion(q, id, 500);
    TEST_ASSERT_EQUAL_INT(ESE_JOB_COMPLETED, rc);

    /* cancel after complete should report completed */
    int cr1 = ese_job_queue_cancel_callback(q, id);
    TEST_ASSERT_EQUAL_INT(ESE_JOB_COMPLETED, cr1);
    /* double-cancel also completed */
    int cr2 = ese_job_queue_cancel_callback(q, id);
    TEST_ASSERT_EQUAL_INT(ESE_JOB_COMPLETED, cr2);

    /* poll and assert one callback + one cleanup */
    drain_callbacks(q);
    TEST_ASSERT_EQUAL_INT(1, g_callback_count);
    TEST_ASSERT_EQUAL_INT(1, g_cleanup_count);
    ese_job_queue_destroy(q);
}

static void test_poll_when_empty_returns_zero(void) {
    reset_globals();
    EseJobQueue *q = ese_job_queue_create(1, worker_init, worker_deinit);
    /* Immediately empty */
    TEST_ASSERT_EQUAL_INT(0, ese_job_queue_poll_callbacks(q));
    /* Queue and complete one to ensure poll works normally */
    IntBox *in = (IntBox *)memory_manager.malloc(sizeof(IntBox), MMTAG_TEMP);
    in->value = 5;
    ese_job_id_t id = ese_job_queue_push(q, job_double, main_callback, cleanup_intbox, in);
    TEST_ASSERT_TRUE(id > 0);
    int rc = ese_job_queue_wait_for_completion(q, id, 500);
    TEST_ASSERT_EQUAL_INT(ESE_JOB_COMPLETED, rc);
    TEST_ASSERT_EQUAL_INT(1, ese_job_queue_poll_callbacks(q));
    TEST_ASSERT_EQUAL_INT(1, g_cleanup_count);
    ese_job_queue_destroy(q);
}

/* This test attempts to trigger cancel while a worker is scanning under global lock.
 * It won't deterministically deadlock, but running repeatedly will catch lock-order bugs. */
static void *job_yield_then_return(void *thread_data, void *user_data) {
    (void)thread_data; (void)user_data;
    // brief work
    return NULL;
}

static void test_cancel_lock_order_no_deadlock(void) {
    reset_globals();
    EseJobQueue *q = ese_job_queue_create(2, worker_init, worker_deinit);
    // enqueue several jobs, then cancel mid-flight
    ese_job_id_t ids[20];
    for (int i = 0; i < 20; i++) {
        ids[i] = ese_job_queue_push(q, job_yield_then_return, NULL, cleanup_intbox, NULL);
        TEST_ASSERT_TRUE(ids[i] > 0);
    }
    // cancel half of them while workers are active
    for (int i = 0; i < 20; i += 2) {
        (void)ese_job_queue_cancel_callback(q, ids[i]);
    }
    // wait for all to be done or canceled
    for (int i = 0; i < 20; i++) {
        (void)ese_job_queue_wait_for_completion(q, ids[i], 500);
    }
    // no deadlock occurred if we reached here
    ese_job_queue_destroy(q);
}

#include "../src/utility/thread.h"
static void *tiny_thread_fn(void *ud) {
    int *p = (int *)ud; if (p) *p = 42; return (void *)0xdeadbeef;
}

static void test_thread_detach_no_use_after_free(void) {
    // Create a thread and detach it; ensure no crash/use-after-free when it exits.
    int v = 0;
    EseThread t = ese_thread_create(tiny_thread_fn, &v);
    TEST_ASSERT_NOT_NULL(t);
    ese_thread_detach(t);
    // Give it a moment to run
    // (no join available; if use-after-free existed, this would often crash)
    // Best-effort sleep-free: just spin briefly
    for (volatile int i = 0; i < 1000000; i++) { /* spin */ }
    TEST_ASSERT_EQUAL_INT(42, v);
}

static void drain_callbacks(EseJobQueue *q) {
    /* poll until no more callbacks processed in a pass */
    for (int i = 0; i < 100; i++) {
        int n = ese_job_queue_poll_callbacks(q);
        if (n == 0) break;
    }
}

static void test_push_any_and_poll_callbacks(void) {
    reset_globals();
    EseJobQueue *q = ese_job_queue_create(2, worker_init, worker_deinit);

    ese_job_id_t ids[5];
    for (int i = 0; i < 5; i++) {
        IntBox *in = (IntBox *)memory_manager.malloc(sizeof(IntBox), MMTAG_TEMP);
        in->value = i + 1;
        ids[i] = ese_job_queue_push(q, job_double, main_callback, cleanup_intbox, in);
        TEST_ASSERT_TRUE(ids[i] > 0);
    }

    /* Wait for all to complete */
    for (int i = 0; i < 5; i++) {
        int rc = ese_job_queue_wait_for_completion(q, ids[i], 1000);
        TEST_ASSERT_TRUE_MESSAGE(rc == ESE_JOB_COMPLETED || rc == ESE_JOB_CANCELED, "job did not complete in time");
    }

    /* Poll until all 5 callbacks observed or retries exhausted */
    for (int tries = 0; tries < 200 && g_callback_count < 5; tries++) {
        ese_job_queue_poll_callbacks(q);
    }

    TEST_ASSERT_EQUAL_INT(5, g_callback_count);
    TEST_ASSERT_EQUAL_INT(5, g_cleanup_count);
    ese_job_queue_destroy(q);
}

static void test_push_on_specific_worker(void) {
    reset_globals();
    EseJobQueue *q = ese_job_queue_create(3, worker_init, worker_deinit);

    /* target worker 2 explicitly */
    IntBox *in = (IntBox *)memory_manager.malloc(sizeof(IntBox), MMTAG_TEMP);
    in->value = 10;
    ese_job_id_t id = ese_job_queue_push_on_worker(q, 2, job_add_worker_id, main_callback, cleanup_intbox, in);
    TEST_ASSERT_TRUE(id > 0);

    /* Wait */
    int rc = ese_job_queue_wait_for_completion(q, id, 0);
    TEST_ASSERT_EQUAL_INT(ESE_JOB_COMPLETED, rc);

    drain_callbacks(q);

    /* Expect 10 + worker_id(2) */
    TEST_ASSERT_EQUAL_INT(1, g_callback_count);
    TEST_ASSERT_EQUAL_INT(1, g_cleanup_count);
    TEST_ASSERT_EQUAL_INT(12, g_last_callback_result);

    ese_job_queue_destroy(q);
}

static void test_status_and_wait_paths(void) {
    reset_globals();
    EseJobQueue *q = ese_job_queue_create(1, worker_init, worker_deinit);

    IntBox *in = (IntBox *)memory_manager.malloc(sizeof(IntBox), MMTAG_TEMP);
    in->value = 7;
    ese_job_id_t id = ese_job_queue_push(q, job_double, main_callback, cleanup_intbox, in);
    TEST_ASSERT_TRUE(id > 0);

    /* Immediately should be not completed */
    int st = ese_job_queue_status(q, id);
    TEST_ASSERT_EQUAL_INT(ESE_JOB_NOT_COMPLETED, st);

    int rc = ese_job_queue_wait_for_completion(q, id, 0);
    TEST_ASSERT_EQUAL_INT(ESE_JOB_COMPLETED, rc);

    /* After completion but before poll, status should report completed */
    st = ese_job_queue_status(q, id);
    TEST_ASSERT_EQUAL_INT(ESE_JOB_COMPLETED, st);

    /* Poll, which will free job metadata */
    drain_callbacks(q);

    /* After poll, status should be NOT_FOUND */
    st = ese_job_queue_status(q, id);
    TEST_ASSERT_EQUAL_INT(ESE_JOB_NOT_FOUND, st);

    ese_job_queue_destroy(q);
}

static void test_cancel_before_start(void) {
    reset_globals();
    EseJobQueue *q = ese_job_queue_create(1, worker_init, worker_deinit);

    /* Create blocker so job will be pending in global queue briefly */
    JobBlocker *blk = (JobBlocker *)memory_manager.malloc(sizeof(JobBlocker), MMTAG_TEMP);
    blk->mutex = ese_mutex_create();
    blk->cond = ese_cond_create();
    blk->go = false;
    blk->started = false;

    ese_job_id_t id = ese_job_queue_push(q, job_block_until_go, main_callback, cleanup_jobblocker, blk);
    TEST_ASSERT_TRUE(id > 0);

    /* Cancel right away */
    int cr = ese_job_queue_cancel_callback(q, id);
    TEST_ASSERT_EQUAL_INT(ESE_JOB_CANCELED, cr);

    /* Wait for completion (canceled) */
    int wr = ese_job_queue_wait_for_completion(q, id, 100);
    TEST_ASSERT_TRUE(wr == ESE_JOB_CANCELED || wr == ESE_JOB_COMPLETED);

    /* Poll callbacks; callback should NOT be called for canceled job */
    drain_callbacks(q);
    TEST_ASSERT_EQUAL_INT(0, g_callback_count);
    TEST_ASSERT_EQUAL_INT(1, g_cleanup_count);

    /* blk and its sync objects freed by cleanup */

    ese_job_queue_destroy(q);
}

static void test_cancel_during_run(void) {
    reset_globals();
    EseJobQueue *q = ese_job_queue_create(1, worker_init, worker_deinit);

    JobBlocker *blk = (JobBlocker *)memory_manager.malloc(sizeof(JobBlocker), MMTAG_TEMP);
    blk->mutex = ese_mutex_create();
    blk->cond = ese_cond_create();
    blk->go = false;
    blk->started = false;

    ese_job_id_t id = ese_job_queue_push(q, job_block_until_go, main_callback, cleanup_jobblocker, blk);
    TEST_ASSERT_TRUE(id > 0);

    /* Wait until the job actually starts running */
    for (int i = 0; i < 1000; i++) {
        ese_mutex_lock(blk->mutex);
        bool started = blk->started;
        ese_mutex_unlock(blk->mutex);
        if (started) break;
    }

    /* Cancel while running */
    int cr = ese_job_queue_cancel_callback(q, id);
    TEST_ASSERT_TRUE(cr == ESE_JOB_CANCELED || cr == ESE_JOB_COMPLETED);

    /* Allow the job to proceed and finish */
    ese_mutex_lock(blk->mutex);
    blk->go = true;
    ese_cond_broadcast(blk->cond);
    ese_mutex_unlock(blk->mutex);

    int wr = ese_job_queue_wait_for_completion(q, id, 0);
    TEST_ASSERT_TRUE(wr == ESE_JOB_CANCELED || wr == ESE_JOB_COMPLETED);

    drain_callbacks(q);

    /* Callback should be suppressed if canceled took effect, but cleanup must run */
    TEST_ASSERT_TRUE(g_cleanup_count >= 1);

    /* blk and its sync objects freed by cleanup */

    ese_job_queue_destroy(q);
}

static void test_wait_timeout_then_complete(void) {
    reset_globals();
    EseJobQueue *q = ese_job_queue_create(1, worker_init, worker_deinit);

    JobBlocker *blk = (JobBlocker *)memory_manager.malloc(sizeof(JobBlocker), MMTAG_TEMP);
    blk->mutex = ese_mutex_create();
    blk->cond = ese_cond_create();
    blk->go = false;
    blk->started = false;

    ese_job_id_t id = ese_job_queue_push(q, job_block_until_go, main_callback, cleanup_jobblocker, blk);
    TEST_ASSERT_TRUE(id > 0);

    /* Expect timeout while the job is blocked */
    int wr = ese_job_queue_wait_for_completion(q, id, 10);
    TEST_ASSERT_EQUAL_INT(ESE_JOB_TIMEOUT, wr);

    /* Unblock to finish */
    ese_mutex_lock(blk->mutex);
    blk->go = true;
    ese_cond_broadcast(blk->cond);
    ese_mutex_unlock(blk->mutex);

    int wr2 = ese_job_queue_wait_for_completion(q, id, 0);
    TEST_ASSERT_TRUE(wr2 == ESE_JOB_COMPLETED || wr2 == ESE_JOB_CANCELED);

    drain_callbacks(q);
    TEST_ASSERT_EQUAL_INT(1, g_cleanup_count);

    /* blk and its sync objects freed by cleanup */

    ese_job_queue_destroy(q);
}


