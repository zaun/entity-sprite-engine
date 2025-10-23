// Job queue implementation
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "utility/job_queue.h"
#include "utility/thread.h"
#include "utility/double_linked_list.h"
#include "utility/int_hashmap.h"
#include "utility/log.h"
#include "core/memory_manager.h"

typedef struct EseJob EseJob;

typedef struct Worker {
    EseThread thread;
    ese_worker_id_t id;
    void *thread_data;
    struct EseJobQueue *parent;
} Worker;

struct EseJobQueue {
    EseMutex *global_mutex; // protects global_queue and next_job_id
	EseCond *global_cond;   // signals when new work is available
	EseDoubleLinkedList *global_queue; // list of EseJob*
	EseMutex *completed_mutex; // protects completed_queue
	EseCond *completed_cond;
	EseDoubleLinkedList *completed_queue; // list of EseJob*
	Worker *workers;
	uint32_t num_workers;
	EseIntHashMap *jobs_by_id; // job_id -> EseJob*
	ese_job_id_t next_job_id;
    EseMutex *jobs_mutex; // protects jobs_by_id and next_job_id
    bool shutting_down;
	worker_thread_init_function init_fn;
	worker_thread_deinit_function deinit_fn;
};

typedef enum JobState {
	JOB_PENDING = 0,
	JOB_RUNNING = 1,
	JOB_COMPLETED = 2,
	JOB_CANCELED = 3
} JobState;

struct EseJob {
	ese_job_id_t id;
	ese_worker_id_t target_worker;
	worker_thread_job_function fn;
	main_thread_job_callback callback;
	main_thread_job_cleanup cleanup;
	void *user_data;
	void *result;
	volatile bool canceled;
	volatile JobState state;
};

static void _free_job_value(void *value) {
	EseJob *job = (EseJob*)value;
	if (!job) return;
	// user_data and result are owned by caller; only free job struct
	memory_manager.free(job);
}

static void *_worker_thread_main(void *ud) {
    Worker *worker = (Worker*)ud;
    EseJobQueue *q = worker->parent;
    log_verbose("JOBQ", "worker %u start q=%p", (unsigned)worker->id, (void*)q);
    for (;;) {
        // Find an eligible job under the global lock
        ese_mutex_lock(q->global_mutex);
        while (!q->shutting_down) {
            EseDListIter *it = dlist_iter_create(q->global_queue);
            void *val = NULL;
            EseJob *picked = NULL;
            while (dlist_iter_next(it, &val)) {
                EseJob *j = (EseJob*)val;
                if (j->target_worker == ESE_WORKER_ANY || j->target_worker == worker->id) { picked = j; break; }
            }
            dlist_iter_free(it);
            if (picked) {
                dlist_remove_by_value(q->global_queue, picked);
                ese_mutex_unlock(q->global_mutex);
                // Execute picked job
                ese_mutex_lock(q->jobs_mutex);
                picked->state = JOB_RUNNING;
                bool is_canceled = picked->canceled;
                ese_mutex_unlock(q->jobs_mutex);
                if (!is_canceled && picked->fn) {
                    picked->result = picked->fn(worker->thread_data, picked->user_data, &is_canceled);
                }
                ese_mutex_lock(q->jobs_mutex);
                picked->state = picked->canceled ? JOB_CANCELED : JOB_COMPLETED;
                ese_mutex_unlock(q->jobs_mutex);
                ese_mutex_lock(q->completed_mutex);
                dlist_append(q->completed_queue, picked);
                ese_cond_signal(q->completed_cond);
                ese_mutex_unlock(q->completed_mutex);
                // Loop back to take next job
                goto next_iteration;
            }
            // No eligible job; wait with timeout to avoid missed signals
            (void)ese_cond_wait_timeout(q->global_cond, q->global_mutex, 50);
        }
        // shutting down
        ese_mutex_unlock(q->global_mutex);
        break;
    next_iteration:
        ;
    }
    log_verbose("JOBQ", "worker %u stopping", (unsigned)worker->id);
    return NULL;
}

EseJobQueue *ese_job_queue_create(
	uint32_t num_workers,
	worker_thread_init_function init_fn,
	worker_thread_deinit_function deinit_fn) {
	log_assert("JOBQ", num_workers > 0, "num_workers must be > 0");
    EseJobQueue *q = memory_manager.calloc(1, sizeof(EseJobQueue), MMTAG_THREAD);
    log_verbose("JOBQ", "create queue %p with %u workers", (void*)q, (unsigned)num_workers);
    q->global_mutex = ese_mutex_create();
    q->global_cond = ese_cond_create();
	q->completed_mutex = ese_mutex_create();
	q->completed_cond = ese_cond_create();
	q->global_queue = dlist_create(NULL);
	q->completed_queue = dlist_create(NULL);
	q->num_workers = num_workers;
	q->workers = memory_manager.calloc(num_workers, sizeof(Worker), MMTAG_THREAD);
    q->jobs_by_id = int_hashmap_create(NULL);
    q->next_job_id = 1;
    q->jobs_mutex = ese_mutex_create();
    q->init_fn = init_fn;
	q->deinit_fn = deinit_fn;
    q->shutting_down = false;

	// Create workers
	for (uint32_t i = 0; i < num_workers; i++) {
		Worker *w = &q->workers[i];
        w->id = (ese_worker_id_t)i;
		w->parent = q;
		if (q->init_fn) {
			w->thread_data = q->init_fn(w->id);
		}
		w->thread = ese_thread_create(_worker_thread_main, w);
	}
	return q;
}

void ese_job_queue_destroy(EseJobQueue *q) {
	log_assert("JOBQ", q, "destroy called with NULL");
    // Signal workers to stop
    ese_mutex_lock(q->global_mutex);
    q->shutting_down = true;
    ese_cond_broadcast(q->global_cond);
    ese_mutex_unlock(q->global_mutex);

    // Join all workers
	for (uint32_t i = 0; i < q->num_workers; i++) {
		if (q->workers[i].thread) {
			ese_thread_join(q->workers[i].thread);
		}
	}

    // Run deinit
	if (q->deinit_fn) {
		for (uint32_t i = 0; i < q->num_workers; i++) {
			q->deinit_fn(q->workers[i].id, q->workers[i].thread_data);
		}
	}

    // Cancel remaining jobs in global and worker queues and move them to completed for cleanup
	void *v;
	while ((v = dlist_pop_front(q->global_queue)) != NULL) {
		EseJob *job = (EseJob*)v;
		job->canceled = true;
		dlist_append(q->completed_queue, job);
	}

    // No per-worker queues in this design; nothing else to drain here
	// Cleanup completed queue without callbacks
	while ((v = dlist_pop_front(q->completed_queue)) != NULL) {
		EseJob *job = (EseJob*)v;
		if (job->cleanup) job->cleanup(job->id, job->user_data, job->result);
		int_hashmap_remove(q->jobs_by_id, (uint64_t)job->id);
		_free_job_value(job);
	}

    // Destroy structures
	dlist_free(q->global_queue);
	dlist_free(q->completed_queue);

    // free workers array
	memory_manager.free(q->workers);
    int_hashmap_destroy(q->jobs_by_id);
    ese_cond_destroy(q->global_cond);
    ese_mutex_destroy(q->global_mutex);
	ese_cond_destroy(q->completed_cond);
    ese_mutex_destroy(q->completed_mutex);
    ese_mutex_destroy(q->jobs_mutex);
	memory_manager.free(q);
}

static EseJob *_create_job(EseJobQueue *q, ese_worker_id_t worker_id, worker_thread_job_function fn, main_thread_job_callback callback, main_thread_job_cleanup cleanup, void *user_data) {
	log_assert("JOBQ", q, "_create_job null q");
	log_assert("JOBQ", fn, "_create_job null fn");
	log_assert("JOBQ", cleanup, "_create_job null cleanup");
    EseJob *job = memory_manager.calloc(1, sizeof(EseJob), MMTAG_THREAD);
    // protect id and map insert
    ese_mutex_lock(q->jobs_mutex);
    job->id = q->next_job_id++;
	job->target_worker = worker_id;
	job->fn = fn;
	job->callback = callback;
	job->cleanup = cleanup;
	job->user_data = user_data;
	job->result = NULL;
	job->canceled = false;
	job->state = JOB_PENDING;
    int_hashmap_set(q->jobs_by_id, (uint64_t)job->id, job);
    ese_mutex_unlock(q->jobs_mutex);
	return job;
}

ese_job_id_t ese_job_queue_push(EseJobQueue *q, worker_thread_job_function fn, main_thread_job_callback callback, main_thread_job_cleanup cleanup, void *user_data) {
	return ese_job_queue_push_on_worker(q, ESE_WORKER_ANY, fn, callback, cleanup, user_data);
}

ese_job_id_t ese_job_queue_push_on_worker(EseJobQueue *q, ese_worker_id_t worker_id, worker_thread_job_function fn, main_thread_job_callback callback, main_thread_job_cleanup cleanup, void *user_data) {
	log_assert("JOBQ", q, "push_on_worker null q");
	log_assert("JOBQ", fn, "push_on_worker null fn");
	log_assert("JOBQ", cleanup, "push_on_worker null cleanup");
	if (!(worker_id == ESE_WORKER_ANY || worker_id < q->num_workers)) {
		return ESE_JOB_NOT_QUEUED;
	}
	EseJob *job = _create_job(q, worker_id, fn, callback, cleanup, user_data);
    ese_mutex_lock(q->global_mutex);
    dlist_append(q->global_queue, job);
    // signal global condition so any waiting worker wakes
    ese_cond_broadcast(q->global_cond);
    ese_mutex_unlock(q->global_mutex);
	return job->id;
}

int32_t ese_job_queue_status(EseJobQueue *q, ese_job_id_t job_id) {
	log_assert("JOBQ", q, "status null q");
    ese_mutex_lock(q->jobs_mutex);
    EseJob *job = (EseJob*)int_hashmap_get(q->jobs_by_id, (uint64_t)job_id);
    if (!job) { ese_mutex_unlock(q->jobs_mutex); return ESE_JOB_NOT_FOUND; }
    int32_t rc = (job->state == JOB_COMPLETED) ? ESE_JOB_COMPLETED :
                 (job->state == JOB_CANCELED)  ? ESE_JOB_CANCELED  : ESE_JOB_NOT_COMPLETED;
    ese_mutex_unlock(q->jobs_mutex);
    return rc;
}

int32_t ese_job_queue_wait_for_completion(EseJobQueue *q, ese_job_id_t job_id, size_t timeout_ms) {
	log_assert("JOBQ", q, "wait null q");
    ese_mutex_lock(q->jobs_mutex);
    EseJob *job = (EseJob*)int_hashmap_get(q->jobs_by_id, (uint64_t)job_id);
    if (!job) { ese_mutex_unlock(q->jobs_mutex); return ESE_JOB_NOT_FOUND; }
    int32_t state_snapshot = job->state;
    ese_mutex_unlock(q->jobs_mutex);
    ese_mutex_lock(q->completed_mutex);
    int rc = 0;
    while (!(state_snapshot == JOB_COMPLETED || state_snapshot == JOB_CANCELED)) {
        if (timeout_ms == 0) {
            ese_cond_wait(q->completed_cond, q->completed_mutex);
        } else {
            rc = ese_cond_wait_timeout(q->completed_cond, q->completed_mutex, (int)timeout_ms);
            if (rc == 1) { ese_mutex_unlock(q->completed_mutex); return ESE_JOB_TIMEOUT; }
            if (rc != 0) { ese_mutex_unlock(q->completed_mutex); return ESE_JOB_NOT_FOUND; }
        }
        // re-check under jobs lock, re-lookup pointer in case it was freed by poll
        ese_mutex_lock(q->jobs_mutex);
        job = (EseJob*)int_hashmap_get(q->jobs_by_id, (uint64_t)job_id);
        state_snapshot = job ? job->state : JOB_COMPLETED; // if removed, consider completed
        ese_mutex_unlock(q->jobs_mutex);
    }
    ese_mutex_unlock(q->completed_mutex);
    return (state_snapshot == JOB_COMPLETED) ? ESE_JOB_COMPLETED : ESE_JOB_CANCELED;
}

int32_t ese_job_queue_cancel_callback(EseJobQueue *q, ese_job_id_t job_id) {
	log_assert("JOBQ", q, "cancel null q");
    // Lock order: always global_mutex then jobs_mutex to avoid deadlocks
    ese_mutex_lock(q->global_mutex);
    ese_mutex_lock(q->jobs_mutex);
    EseJob *job = (EseJob*)int_hashmap_get(q->jobs_by_id, (uint64_t)job_id);
    if (!job) { ese_mutex_unlock(q->jobs_mutex); ese_mutex_unlock(q->global_mutex); return ESE_JOB_NOT_FOUND; }
	// Scan global and remove if present
	EseDListIter *it = dlist_iter_create(q->global_queue);
	void *val = NULL;
	bool found_in_global = false;
	while (dlist_iter_next(it, &val)) {
		if (val == job) { found_in_global = true; break; }
	}
	dlist_iter_free(it);
    if (found_in_global) {
        dlist_remove_by_value(q->global_queue, job);
        job->canceled = true;
        job->state = JOB_CANCELED;
        ese_mutex_unlock(q->jobs_mutex);
        ese_mutex_unlock(q->global_mutex);
        ese_mutex_lock(q->completed_mutex);
        dlist_append(q->completed_queue, job);
        ese_cond_signal(q->completed_cond);
        ese_mutex_unlock(q->completed_mutex);
        return ESE_JOB_CANCELED;
    }
    ese_mutex_unlock(q->jobs_mutex);
    ese_mutex_unlock(q->global_mutex);
    // If not in global queue, mark canceled and return; worker will handle state
    if (job->state == JOB_COMPLETED) {
        return ESE_JOB_COMPLETED;
    }
    ese_mutex_lock(q->jobs_mutex);
    job->canceled = true;
    int32_t rc = (job->state == JOB_COMPLETED) ? ESE_JOB_COMPLETED : ESE_JOB_CANCELED;
    ese_mutex_unlock(q->jobs_mutex);
    return rc;
}

int ese_job_queue_poll_callbacks(EseJobQueue *q) {
	log_assert("JOBQ", q, "poll null q");
	int processed = 0;
	// Move all completed to local list
	EseDoubleLinkedList *local = dlist_create(NULL);
	ese_mutex_lock(q->completed_mutex);
	void *v;
	while ((v = dlist_pop_front(q->completed_queue)) != NULL) {
		dlist_append(local, v);
	}
	ese_mutex_unlock(q->completed_mutex);
    // Process without holding locks
    while ((v = dlist_pop_front(local)) != NULL) {
        EseJob *job = (EseJob*)v;
        ese_mutex_lock(q->jobs_mutex);
        bool canceled = job->canceled;
        ese_mutex_unlock(q->jobs_mutex);
        if (!q->shutting_down && !canceled && job->callback) {
            job->callback(job->id, job->result);
            processed++;
        }
        if (job->cleanup) job->cleanup(job->id, job->user_data, job->result);
        ese_mutex_lock(q->jobs_mutex);
        int_hashmap_remove(q->jobs_by_id, (uint64_t)job->id);
        ese_mutex_unlock(q->jobs_mutex);
        _free_job_value(job);
    }
	dlist_free(local);
	return processed;
}


