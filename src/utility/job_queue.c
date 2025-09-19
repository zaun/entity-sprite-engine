#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "utility/log.h"
#include "core/memory_manager.h"
#include "utility/job_queue.h"

/* ----------------------
 * Structures (internal)
 * ---------------------- */
struct EseJob_ts {
    EseJobFn fn;
    void *data;
    bool wait_for_completion;
    bool completed;
    bool success;
    EseMutex *completion_mutex;
    EseCond *completion_cond;
    struct EseJob_ts *next;
};

struct EseJobQueue_ts {
    EseJob *head;
    EseJob *tail;
    EseMutex *queue_mutex;
    EseCond *queue_cond;
    bool shutdown;
    size_t pending_jobs;
    EseMutex *pending_mutex;
    EseCond *pending_cond; /* signaled when pending_jobs reaches zero */
};

/* ----------------------
 * Job operations
 * ---------------------- */

EseJob *ese_job_create(EseJobFn fn, void *data, bool wait_for_completion) {
    if (!fn) return NULL;

    EseJob *job = memory_manager.malloc(sizeof(EseJob), MMTAG_THREAD);
    if (!job) return NULL;

    memset(job, 0, sizeof(EseJob));
    job->fn = fn;
    job->data = data;
    job->wait_for_completion = wait_for_completion;
    job->completed = false;
    job->success = false;

    if (wait_for_completion) {
        job->completion_mutex = ese_mutex_create();
        job->completion_cond = ese_cond_create();
        if (!job->completion_mutex || !job->completion_cond) {
            if (job->completion_mutex) ese_mutex_destroy(job->completion_mutex);
            if (job->completion_cond) ese_cond_destroy(job->completion_cond);
            memory_manager.free(job);
            return NULL;
        }
    }

    return job;
}

void ese_job_destroy(EseJob *job) {
    if (!job) return;

    if (job->completion_mutex) {
        ese_mutex_destroy(job->completion_mutex);
    }
    if (job->completion_cond) {
        ese_cond_destroy(job->completion_cond);
    }

    memory_manager.free(job);
}

void ese_job_execute(EseJob *job) {
    if (!job) return;

    log_assert("JOB_QUEUE", job->fn, "ese_job_execute called with NULL function");

    /* Execute the job function */
    job->fn(job->data);
    job->success = true;
    job->completed = true;

    /* Signal per-job waiter if any */
    if (job->wait_for_completion && job->completion_mutex && job->completion_cond) {
        ese_mutex_lock(job->completion_mutex);
        ese_cond_signal(job->completion_cond);
        ese_mutex_unlock(job->completion_mutex);
    }
}

bool ese_job_is_completed(const EseJob *job) {
    if (!job) return false;
    return job->completed;
}

/* Wait for job completion; timeout_ms <= 0 means wait forever.
 * Returns true if job completed successfully, false on timeout or failure.
 */
bool ese_job_wait_for_completion(EseJob *job, int timeout_ms) {
    if (!job || !job->wait_for_completion) return false;
    if (!job->completion_mutex || !job->completion_cond) return false;

    ese_mutex_lock(job->completion_mutex);
    while (!job->completed) {
        int result;
        if (timeout_ms > 0) {
            result = ese_cond_wait_timeout(job->completion_cond, job->completion_mutex, timeout_ms);
            if (result == 1) { /* timeout */
                ese_mutex_unlock(job->completion_mutex);
                return false;
            }
            /* if result == -1 treat as error and continue loop */
        } else {
            ese_cond_wait(job->completion_cond, job->completion_mutex);
        }
    }
    bool success = job->success;
    ese_mutex_unlock(job->completion_mutex);
    return success;
}

/* ----------------------
 * Job queue operations
 * ---------------------- */

EseJobQueue *ese_job_queue_create(void) {
    EseJobQueue *queue = memory_manager.malloc(sizeof(EseJobQueue), MMTAG_THREAD);
    if (!queue) return NULL;

    memset(queue, 0, sizeof(EseJobQueue));
    queue->queue_mutex = ese_mutex_create();
    queue->queue_cond = ese_cond_create();
    queue->pending_mutex = ese_mutex_create();
    queue->pending_cond = ese_cond_create();

    if (!queue->queue_mutex || !queue->queue_cond || !queue->pending_mutex || !queue->pending_cond) {
        if (queue->queue_mutex) ese_mutex_destroy(queue->queue_mutex);
        if (queue->queue_cond) ese_cond_destroy(queue->queue_cond);
        if (queue->pending_mutex) ese_mutex_destroy(queue->pending_mutex);
        if (queue->pending_cond) ese_cond_destroy(queue->pending_cond);
        memory_manager.free(queue);
        return NULL;
    }

    queue->head = queue->tail = NULL;
    queue->shutdown = false;
    queue->pending_jobs = 0;

    return queue;
}

void ese_job_queue_destroy(EseJobQueue *queue) {
    if (!queue) return;

    /* Drain remaining jobs (caller should shutdown first to stop workers) */
    ese_mutex_lock(queue->queue_mutex);
    EseJob *job = queue->head;
    while (job) {
        EseJob *next = job->next;
        ese_job_destroy(job);
        job = next;
    }
    queue->head = queue->tail = NULL;
    ese_mutex_unlock(queue->queue_mutex);

    if (queue->queue_mutex) ese_mutex_destroy(queue->queue_mutex);
    if (queue->queue_cond) ese_cond_destroy(queue->queue_cond);
    if (queue->pending_mutex) ese_mutex_destroy(queue->pending_mutex);
    if (queue->pending_cond) ese_cond_destroy(queue->pending_cond);

    memory_manager.free(queue);
}

/* Internal helper: increment pending count (caller must hold queue_mutex) */
static void _pending_increment(EseJobQueue *queue) {
    ese_mutex_lock(queue->pending_mutex);
    queue->pending_jobs++;
    ese_mutex_unlock(queue->pending_mutex);
}

/* Internal helper: decrement pending count and signal if zero */
static void _pending_decrement_and_signal(EseJobQueue *queue) {
    ese_mutex_lock(queue->pending_mutex);
    if (queue->pending_jobs > 0) queue->pending_jobs--;
    if (queue->pending_jobs == 0) {
        ese_cond_signal(queue->pending_cond);
    }
    ese_mutex_unlock(queue->pending_mutex);
}

/* Push a job onto the queue. If wait_for_completion is true, caller must
 * wait on the job (job->completion_cond) or use push_and_wait convenience.
 */
bool ese_job_queue_push(EseJobQueue *queue, EseJobFn fn, void *data, bool wait_for_completion) {
    if (!queue || !fn) return false;

    EseJob *job = ese_job_create(fn, data, wait_for_completion);
    if (!job) return false;

    ese_mutex_lock(queue->queue_mutex);

    if (queue->shutdown) {
        ese_mutex_unlock(queue->queue_mutex);
        ese_job_destroy(job);
        return false;
    }

    /* Add job to tail */
    if (queue->tail) queue->tail->next = job;
    else queue->head = job;
    queue->tail = job;

    /* If caller wants completion, increase pending count */
    if (wait_for_completion) {
        _pending_increment(queue);
    }

    /* signal worker(s) */
    ese_cond_signal(queue->queue_cond);
    ese_mutex_unlock(queue->queue_mutex);
    return true;
}

/* Convenience: push a job and wait for its completion.
 * timeout_ms <= 0 means wait forever.
 * Returns true if job completed and reported success; false on timeout/failure.
 */
bool ese_job_queue_push_and_wait(EseJobQueue *queue, EseJobFn fn, void *data, int timeout_ms) {
    if (!queue || !fn) return false;

    EseJob *job = ese_job_create(fn, data, true);
    if (!job) return false;

    ese_mutex_lock(queue->queue_mutex);
    if (queue->shutdown) {
        ese_mutex_unlock(queue->queue_mutex);
        ese_job_destroy(job);
        return false;
    }
    /* enqueue */
    if (queue->tail) queue->tail->next = job;
    else queue->head = job;
    queue->tail = job;
    /* increment pending */
    _pending_increment(queue);
    ese_cond_signal(queue->queue_cond);
    ese_mutex_unlock(queue->queue_mutex);

    /* wait for job to be completed (per-job condvar) */
    bool ok = ese_job_wait_for_completion(job, timeout_ms);

    /* mark job as completed in queue (decrement pending and maybe signal) */
    _pending_decrement_and_signal(queue);

    /* destroy job */
    ese_job_destroy(job);
    return ok;
}

/* Wait until all pending jobs that requested completion are done.
 * This uses a condvar to avoid busy-waiting.
 */
void ese_job_queue_wait_for_completion(EseJobQueue *queue) {
    if (!queue) return;

    ese_mutex_lock(queue->pending_mutex);
    while (queue->pending_jobs > 0) {
        /* Wait until signaled that pending_jobs reached zero */
        ese_cond_wait(queue->pending_cond, queue->pending_mutex);
    }
    ese_mutex_unlock(queue->pending_mutex);
}

/* Signal shutdown: wakes workers so they can exit. */
void ese_job_queue_shutdown(EseJobQueue *queue) {
    if (!queue) return;

    ese_mutex_lock(queue->queue_mutex);
    queue->shutdown = true;
    ese_cond_broadcast(queue->queue_cond);
    ese_mutex_unlock(queue->queue_mutex);
}

/* ----------------------
 * Job queue worker functions
 * ---------------------- */

/* Pop next job (blocks until available or shutdown). Caller should call
 * ese_job_execute(job) and then use esa_job_queue_mark_completed to decrement
 * pending count when appropriate; finally ese_job_destroy(job) to free per-job resources.
 */
EseJob *ese_job_queue_pop(EseJobQueue *queue) {
    if (!queue) return NULL;

    ese_mutex_lock(queue->queue_mutex);

    while (!queue->head && !queue->shutdown) {
        ese_cond_wait(queue->queue_cond, queue->queue_mutex);
    }

    if (queue->shutdown && !queue->head) {
        ese_mutex_unlock(queue->queue_mutex);
        return NULL;
    }

    EseJob *job = queue->head;
    if (job) {
        queue->head = job->next;
        if (!queue->head) queue->tail = NULL;
        job->next = NULL;
    }

    ese_mutex_unlock(queue->queue_mutex);
    return job;
}

/* Called by worker to atomically note that a job that requested completion
 * has finished; decrements pending_jobs and signals if zero.
 */
void ese_job_queue_mark_completed(EseJobQueue *queue, EseJob *job) {
    (void)job;
    if (!queue) return;
    _pending_decrement_and_signal(queue);
}

/* Worker thread entry point: pop jobs and execute them until shutdown.
 * This is a convenience function you can use as the worker thread main.
 */
void *ese_job_queue_worker_thread(void *user_data) {
    EseJobQueue *queue = (EseJobQueue *)user_data;
    log_assert("JOB_QUEUE", queue, "ese_job_queue_worker_thread called with NULL queue");

    while (true) {
        EseJob *job = ese_job_queue_pop(queue);
        if (!job) {
            /* queue is shutting down and empty */
            break;
        }

        /* execute job */
        ese_job_execute(job);

        /* if job requested completion, mark it completed in the queue */
        if (job->wait_for_completion) {
            ese_job_queue_mark_completed(queue, job);
        }

        /* destroy job; if caller needs job->ok they should have used push_and_wait or
         * provided their own per-job waiting logic */
        ese_job_destroy(job);
    }
    return NULL;
}