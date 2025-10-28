/*
 * Project: Entity Sprite Engine
 *
 * Implementation of the job queue system for managing worker threads and
 * asynchronous job execution. Provides a thread-safe queue with worker thread
 * management, job scheduling, cancellation, and callback processing on the main
 * thread.
 *
 * Details:
 * Worker threads pull jobs from a global queue based on target worker
 * constraints. Jobs pass through states: PENDING -> RUNNING -> RESULTS_READY ->
 * RESULTS_PROCESSED
 * -> EXECUTED -> COMPLETED. Main thread processes callbacks for completed jobs.
 * Cancellation marks jobs canceled but allows running jobs to finish without
 * invoking callbacks.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#include "utility/job_queue.h"
#include "core/memory_manager.h"
#include "utility/double_linked_list.h"
#include "utility/int_hashmap.h"
#include "utility/log.h"
#include "utility/thread.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Job state enum representing the state of a job in the job queue.
 */
typedef enum JobState {
  JOB_PENDING = 0,           /** Job is pending assignment to a worker thread */
  JOB_RUNNING = 1,           /** Job is running */
  JOB_RESULTS_READY = 2,     /** Job results are ready */
  JOB_RESULTS_PROCESSED = 3, /** Job results have been processed */
  JOB_EXECUTED = 4,          /** Job has been executed */
  JOB_COMPLETED = 5,         /** Job has been completed */
  JOB_CANCELED = 6,          /** Job has been canceled */
} JobState;

/**
 * @brief Worker structure representing a worker thread in the job queue.
 */
typedef struct Worker {
  EseThread thread;           /** Thread handle */
  ese_worker_id_t id;         /** Worker ID */
  void *thread_data;          /** Thread data created/deinitialized on worker */
  bool shutdown;              /** Shutdown flag */
  struct EseJobQueue *parent; /** Parent job queue */
} Worker;

/**
 * @brief EseJob structure representing a job in the job queue.
 */
typedef struct EseJob {
  ese_job_id_t id;                 /** Job ID */
  ese_worker_id_t target_worker;   /** Worker ID that the job is targeted for */
  ese_worker_id_t executor_worker; /** Worker ID that executed the job */

  worker_thread_job_function fn; /** Function to execute on worker thread */
  main_thread_job_callback
      callback; /** Function to execute on main thread when job completes */
  main_thread_job_cleanup
      cleanup; /** Function to execute on main thread when job destroyed */

  void *user_data;         /** User data to pass to the job function */
  JobResult worker_result; /** Result from worker thread */
  void *main_result;       /** Result from main thread */

  volatile bool canceled;  /** Canceled flag */
  volatile JobState state; /** Job state */
} EseJob;

/**
 * @brief EseJobQueue structure representing a job queue.
 */
typedef struct EseJobQueue {
  EseMutex *global_mutex;            /** Mutex for global queue */
  EseCond *global_cond;              /** Condition variable for global queue */
  EseDoubleLinkedList *global_queue; /** List of EseJob* */

  Worker *workers;      /** Array of worker threads */
  uint32_t num_workers; /** Number of worker threads */

  EseIntHashMap *jobs_by_id; /** HashMap of job IDs to jobs */
  EseMutex *jobs_mutex;      /** Mutex for jobs by ID */

  ese_job_id_t next_job_id; /** Next job ID to assign */
  bool shutting_down;       /** Shutting down flag */

  worker_init_function
      init_fn; /** Function to run on worker thread initialization */
  worker_deinit_function
      deinit_fn; /** Function to run on worker thread deinitialization */
} EseJobQueue;

// ========================================
// PRIVATE FUNCTIONS
// ========================================

/**
 * @brief Free a job value from the hash map.
 *
 * @param value Pointer to the job to free
 */
static void _free_job_value(void *value) {
  EseJob *job = (EseJob *)value;
  if (!job)
    return;
  memory_manager.free(job);
}

/**
 * @brief Create a new job with the specified parameters.
 *
 * @param q Job queue to add the job to
 * @param worker_id Target worker ID or ESE_WORKER_ANY
 * @param fn Job function to execute
 * @param callback Optional callback function
 * @param cleanup Cleanup function
 * @param user_data User data to pass to job
 * @return EseJob* Created job or NULL on failure
 */
static EseJob *_create_job(EseJobQueue *q, ese_worker_id_t worker_id,
                           worker_thread_job_function fn,
                           main_thread_job_callback callback,
                           main_thread_job_cleanup cleanup, void *user_data) {
  log_assert("JOBQ", q, "_create_job null q");
  log_assert("JOBQ", fn, "_create_job null fn");
  log_assert("JOBQ", cleanup, "_create_job null cleanup");

  EseJob *job = memory_manager.calloc(1, sizeof(EseJob), MMTAG_THREAD);
  job->id = q->next_job_id++;
  job->target_worker = worker_id;
  job->executor_worker = ESE_WORKER_ANY;
  job->fn = fn;
  job->callback = callback;
  job->cleanup = cleanup;
  job->user_data = user_data;
  job->worker_result.result = NULL;
  job->worker_result.size = 0;
  job->main_result = NULL;
  job->canceled = false;
  job->state = JOB_PENDING;

  int_hashmap_set(q->jobs_by_id, (uint64_t)job->id, job);
  return job;
}

/**
 * @brief Main worker thread function.
 *
 * @details Processes jobs from the global queue and executes them. Handles
 * shutdown gracefully and calls init/deinit functions.
 *
 * @param ud Worker thread user data
 * @return void* Always returns NULL
 */
static void *_worker_thread_main(void *ud) {
  Worker *worker = (Worker *)ud;
  EseJobQueue *q = worker->parent;

  log_verbose("JOBQ", "worker %u start q=%p", (unsigned)worker->id, (void *)q);

  // Worker-thread init
  if (q->init_fn) {
    log_verbose("JOBQ", "worker %u initing", (unsigned)worker->id);
    worker->thread_data = q->init_fn(worker->id);
  }

  for (;;) {
    EseJob *picked = NULL;
    bool freed_or_advanced = false;
    bool shutdown_mode = false;
    bool potential_future_work = false;

    ese_mutex_lock(q->global_mutex);
    shutdown_mode = q->shutting_down;

    EseDListIter *it = dlist_iter_create(q->global_queue);
    void *val = NULL;

    while (dlist_iter_next(it, &val)) {
      EseJob *j = (EseJob *)val;

      ese_mutex_lock(q->jobs_mutex);

      // Advance PROCESSED -> EXECUTED on this worker
      bool is_processed = (j->state == JOB_RESULTS_PROCESSED);
      bool is_mine = (j->executor_worker == worker->id);
      if (is_processed && is_mine) {
        if (j->worker_result.result) {
          if (j->worker_result.free_fn) {
            j->worker_result.free_fn(j->worker_result.result);
          } else {
            memory_manager.free(j->worker_result.result);
          }
        }
        j->worker_result.result = NULL;
        j->worker_result.size = 0;
        j->state = JOB_EXECUTED;
        freed_or_advanced = true;
      }

      // During shutdown, see if our jobs still need us
      if (!potential_future_work && j->executor_worker == worker->id) {
        if (j->state == JOB_RESULTS_READY ||
            j->state == JOB_RESULTS_PROCESSED) {
          potential_future_work = true;
        }
      }

      // Pick eligible job if not shutting down
      if (!shutdown_mode && !picked) {
        bool eligible =
            (j->state == JOB_PENDING) && (j->target_worker == ESE_WORKER_ANY ||
                                          j->target_worker == worker->id);
        if (eligible) {
          j->state = JOB_RUNNING;
          j->executor_worker = worker->id;
          picked = j;
        }
      }
      ese_mutex_unlock(q->jobs_mutex);

      if (picked)
        break;
    }
    dlist_iter_free(it);

    if (!picked && !freed_or_advanced) {
      if (shutdown_mode) {
        if (potential_future_work) {
          (void)ese_cond_wait_timeout(q->global_cond, q->global_mutex, 50);
          ese_mutex_unlock(q->global_mutex);
          continue;
        }
        bool any_nonfinal = false;
        it = dlist_iter_create(q->global_queue);
        while (dlist_iter_next(it, &val)) {
          EseJob *j = (EseJob *)val;
          ese_mutex_lock(q->jobs_mutex);
          JobState s = j->state;
          ese_mutex_unlock(q->jobs_mutex);
          if (s != JOB_EXECUTED && s != JOB_CANCELED) {
            any_nonfinal = true;
            break;
          }
        }
        dlist_iter_free(it);
        if (any_nonfinal) {
          (void)ese_cond_wait_timeout(q->global_cond, q->global_mutex, 50);
          ese_mutex_unlock(q->global_mutex);
          continue;
        }

        ese_mutex_unlock(q->global_mutex);
        break; // exit loop
      }

      (void)ese_cond_wait_timeout(q->global_cond, q->global_mutex, 50);
      ese_mutex_unlock(q->global_mutex);
      continue;
    }

    if (freed_or_advanced) {
      ese_cond_broadcast(q->global_cond);
    }
    ese_mutex_unlock(q->global_mutex);

    if (!picked)
      continue;

    // Execute picked job outside locks
    bool is_canceled_local = false;
    ese_mutex_lock(q->jobs_mutex);
    is_canceled_local = picked->canceled;
    ese_mutex_unlock(q->jobs_mutex);

    if (!is_canceled_local && picked->fn) {
      picked->worker_result = picked->fn(worker->thread_data, picked->user_data,
                                         &is_canceled_local);
    }

    ese_mutex_lock(q->jobs_mutex);
    if (picked->canceled || is_canceled_local) {
      picked->canceled = true;
      picked->state = JOB_CANCELED;
    } else {
      picked->state = JOB_RESULTS_READY;
    }
    ese_mutex_unlock(q->jobs_mutex);

    ese_mutex_lock(q->global_mutex);
    ese_cond_broadcast(q->global_cond);
    ese_mutex_unlock(q->global_mutex);
  }

  log_verbose("JOBQ", "worker %u stopping", (unsigned)worker->id);

  // Worker-thread deinit then per-thread MM destroy
  if (q->deinit_fn) {
    log_verbose("JOBQ", "worker %u deiniting", (unsigned)worker->id);
    q->deinit_fn(worker->id, worker->thread_data);
    worker->thread_data = NULL;
  }
  if (!worker->shutdown) {
    log_verbose("JOBQ", "worker %u destroying", (unsigned)worker->id);
    worker->shutdown = true;
    memory_manager.destroy(false);
  }
  return NULL;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Create a job queue with the specified number of worker threads.
 *
 * @param num_workers Number of worker threads to create
 * @param init_fn Optional initialization function for worker threads
 * @param deinit_fn Optional cleanup function for worker threads
 * @return EseJobQueue* New job queue instance
 */
EseJobQueue *ese_job_queue_create(uint32_t num_workers,
                                  worker_init_function init_fn,
                                  worker_deinit_function deinit_fn) {
  log_assert("JOBQ", num_workers > 0, "num_workers must be > 0");
  EseJobQueue *q = memory_manager.calloc(1, sizeof(EseJobQueue), MMTAG_THREAD);
  log_verbose("JOBQ", "create queue %p with %u workers", (void *)q,
              (unsigned)num_workers);

  q->global_mutex = ese_mutex_create();
  q->global_cond = ese_cond_create();
  q->global_queue = dlist_create(NULL);
  q->num_workers = num_workers;
  q->workers = memory_manager.calloc(num_workers, sizeof(Worker), MMTAG_THREAD);
  q->jobs_by_id = int_hashmap_create(NULL);
  q->next_job_id = 1;
  q->jobs_mutex = ese_mutex_create();
  q->init_fn = init_fn;
  q->deinit_fn = deinit_fn;
  q->shutting_down = false;

  for (uint32_t i = 0; i < num_workers; i++) {
    Worker *w = &q->workers[i];
    w->shutdown = false;
    w->id = (ese_worker_id_t)i;
    w->parent = q;
    w->thread_data = NULL; // init in worker
    w->thread = ese_thread_create(_worker_thread_main, w);
  }

  return q;
}

/**
 * @brief Push a job to any available worker thread.
 *
 * @param q Job queue to push to
 * @param fn Required function to execute on worker thread
 * @param callback Optional callback to execute on main thread when job
 * completes
 * @param cleanup Required cleanup function to execute on main thread when job
 * destroyed
 * @param user_data User data to pass to the job function
 * @return ese_job_id_t Job ID or ESE_JOB_NOT_QUEUED on failure
 */
ese_job_id_t ese_job_queue_push(EseJobQueue *q, worker_thread_job_function fn,
                                main_thread_job_callback callback,
                                main_thread_job_cleanup cleanup,
                                void *user_data) {
  return ese_job_queue_push_on_worker(q, ESE_WORKER_ANY, fn, callback, cleanup,
                                      user_data);
}

/**
 * @brief Push a job to a specific worker thread.
 *
 * @param q Job queue to push to
 * @param worker_id Specific worker thread ID or ESE_WORKER_ANY
 * @param fn Required function to execute on worker thread
 * @param callback Optional callback to execute on main thread when job
 * completes
 * @param cleanup Required cleanup function to execute on main thread when job
 * destroyed
 * @param user_data User data to pass to the job function
 * @return ese_job_id_t Job ID or ESE_JOB_NOT_QUEUED on failure
 */
ese_job_id_t ese_job_queue_push_on_worker(EseJobQueue *q,
                                          ese_worker_id_t worker_id,
                                          worker_thread_job_function fn,
                                          main_thread_job_callback callback,
                                          main_thread_job_cleanup cleanup,
                                          void *user_data) {
  log_assert("JOBQ", q, "push_on_worker null q");
  log_assert("JOBQ", fn, "push_on_worker null fn");
  log_assert("JOBQ", cleanup, "push_on_worker null cleanup");

  if (!(worker_id == ESE_WORKER_ANY || worker_id < q->num_workers)) {
    return ESE_JOB_NOT_QUEUED;
  }

  ese_mutex_lock(q->global_mutex);
  if (q->shutting_down) {
    ese_mutex_unlock(q->global_mutex);
    return ESE_JOB_NOT_QUEUED;
  }

  ese_mutex_lock(q->jobs_mutex);
  EseJob *job = _create_job(q, worker_id, fn, callback, cleanup, user_data);
  if (!job) {
    ese_mutex_unlock(q->jobs_mutex);
    ese_mutex_unlock(q->global_mutex);
    return ESE_JOB_NOT_QUEUED;
  }
  dlist_append(q->global_queue, job);
  ese_cond_broadcast(q->global_cond);
  ese_mutex_unlock(q->jobs_mutex);
  ese_mutex_unlock(q->global_mutex);
  return job->id;
}

/**
 * @brief Query the status of a job.
 *
 * @param q Job queue containing the job
 * @param job_id ID of the job to query
 * @return int32_t Job status code
 */
int32_t ese_job_queue_status(EseJobQueue *q, ese_job_id_t job_id) {
  log_assert("JOBQ", q, "status null q");
  ese_mutex_lock(q->jobs_mutex);
  EseJob *job = (EseJob *)int_hashmap_get(q->jobs_by_id, (uint64_t)job_id);
  if (!job) {
    ese_mutex_unlock(q->jobs_mutex);
    return ESE_JOB_NOT_FOUND;
  }
  JobState state = job->state;
  ese_mutex_unlock(q->jobs_mutex);

  if (state == JOB_RESULTS_READY || state == JOB_RESULTS_PROCESSED ||
      state == JOB_EXECUTED || state == JOB_COMPLETED) {
    return ESE_JOB_COMPLETED;
  }
  if (state == JOB_CANCELED) {
    return ESE_JOB_CANCELED;
  }
  return ESE_JOB_NOT_COMPLETED;
}

/**
 * @brief Wait for a job to complete.
 *
 * @param q Job queue containing the job
 * @param job_id ID of the job to wait for
 * @param timeout_ms Timeout in milliseconds (0 for infinite wait)
 * @return int32_t Job status code
 */
int32_t ese_job_queue_wait_for_completion(EseJobQueue *q, ese_job_id_t job_id,
                                          size_t timeout_ms) {
  log_assert("JOBQ", q, "wait null q");

  ese_mutex_lock(q->global_mutex);

  ese_mutex_lock(q->jobs_mutex);
  EseJob *job = (EseJob *)int_hashmap_get(q->jobs_by_id, (uint64_t)job_id);
  if (!job) {
    ese_mutex_unlock(q->jobs_mutex);
    ese_mutex_unlock(q->global_mutex);
    return ESE_JOB_NOT_FOUND;
  }
  JobState state_snapshot = job->state;
  ese_mutex_unlock(q->jobs_mutex);

  int rc = 0;
  while (state_snapshot < JOB_RESULTS_READY && state_snapshot != JOB_CANCELED) {
    if (timeout_ms == 0) {
      ese_cond_wait(q->global_cond, q->global_mutex);
    } else {
      rc = ese_cond_wait_timeout(q->global_cond, q->global_mutex,
                                 (int)timeout_ms);
      if (rc == 1) {
        ese_mutex_unlock(q->global_mutex);
        return ESE_JOB_TIMEOUT;
      }
      if (rc != 0) {
        ese_mutex_unlock(q->global_mutex);
        return ESE_JOB_NOT_FOUND;
      }
    }
    ese_mutex_lock(q->jobs_mutex);
    job = (EseJob *)int_hashmap_get(q->jobs_by_id, (uint64_t)job_id);
    if (!job) {
      ese_mutex_unlock(q->jobs_mutex);
      ese_mutex_unlock(q->global_mutex);
      return ESE_JOB_COMPLETED;
    }
    state_snapshot = job->state;
    ese_mutex_unlock(q->jobs_mutex);
  }
  ese_mutex_unlock(q->global_mutex);

  if (state_snapshot == JOB_CANCELED)
    return ESE_JOB_CANCELED;
  return ESE_JOB_COMPLETED;
}

/**
 * @brief Cancel a pending or running job.
 *
 * @param q Job queue containing the job
 * @param job_id ID of the job to cancel
 * @return int32_t Job status code
 */
int32_t ese_job_queue_cancel_callback(EseJobQueue *q, ese_job_id_t job_id) {
  log_assert("JOBQ", q, "cancel null q");

  ese_mutex_lock(q->global_mutex);
  ese_mutex_lock(q->jobs_mutex);

  EseJob *job = (EseJob *)int_hashmap_get(q->jobs_by_id, (uint64_t)job_id);
  if (!job) {
    ese_mutex_unlock(q->jobs_mutex);
    ese_mutex_unlock(q->global_mutex);
    return ESE_JOB_NOT_FOUND;
  }

  if (job->state == JOB_RESULTS_READY || job->state == JOB_RESULTS_PROCESSED ||
      job->state == JOB_EXECUTED || job->state == JOB_COMPLETED) {
    ese_mutex_unlock(q->jobs_mutex);
    ese_mutex_unlock(q->global_mutex);
    return ESE_JOB_COMPLETED;
  }

  job->canceled = true;
  if (job->state == JOB_PENDING) {
    job->state = JOB_CANCELED;
  }
  ese_cond_broadcast(q->global_cond);

  ese_mutex_unlock(q->jobs_mutex);
  ese_mutex_unlock(q->global_mutex);
  return ESE_JOB_CANCELED;
}

/**
 * @brief Process completed job callbacks on the main thread.
 *
 * @param q Job queue to process callbacks for
 * @return bool True if there are more jobs to process, false otherwise
 */
bool ese_job_queue_process(EseJobQueue *q) {
  log_assert("JOBQ", q, "process null q");

  bool progressed_to_processed = false;

  ese_mutex_lock(q->global_mutex);
  EseDListIter *it = dlist_iter_create(q->global_queue);
  void *val = NULL;
  while (dlist_iter_next(it, &val)) {
    EseJob *job = (EseJob *)val;

    ese_mutex_lock(q->jobs_mutex);
    JobState state = job->state;
    ese_mutex_unlock(q->jobs_mutex);

    if (state == JOB_RESULTS_READY) {
      ese_mutex_lock(q->jobs_mutex);
      if (job->worker_result.result && job->worker_result.size > 0) {
        // Need to copy the result to the main thread
        if (job->worker_result.copy_fn && job->worker_result.free_fn) {
          size_t out_size = 0;
          void *main_copy = job->worker_result.copy_fn(
              job->worker_result.result, job->worker_result.size, &out_size);
          if (main_copy) {
            job->main_result = main_copy;
          } else {
            job->main_result = NULL;
          }
        } else {
          void *main_copy =
              memory_manager.malloc(job->worker_result.size, MMTAG_TEMP);
          memcpy(main_copy, job->worker_result.result, job->worker_result.size);
          job->main_result = main_copy;
        }
      }
      job->state = JOB_RESULTS_PROCESSED;
      ese_mutex_unlock(q->jobs_mutex);
      progressed_to_processed = true;
    }
  }
  dlist_iter_free(it);

  if (progressed_to_processed) {
    ese_cond_broadcast(q->global_cond);
    (void)ese_cond_wait_timeout(q->global_cond, q->global_mutex, 1);
  }
  ese_mutex_unlock(q->global_mutex);

  EseDoubleLinkedList *local = dlist_create(NULL);

  ese_mutex_lock(q->global_mutex);
  it = dlist_iter_create(q->global_queue);
  while (dlist_iter_next(it, &val)) {
    EseJob *job = (EseJob *)val;
    ese_mutex_lock(q->jobs_mutex);
    bool take = (job->state == JOB_EXECUTED || job->state == JOB_CANCELED);
    ese_mutex_unlock(q->jobs_mutex);
    if (take) {
      dlist_append(local, job);
    }
  }
  dlist_iter_free(it);
  ese_mutex_unlock(q->global_mutex);

  while ((val = dlist_pop_front(local)) != NULL) {
    EseJob *job = (EseJob *)val;

    bool canceled = false;
    ese_mutex_lock(q->jobs_mutex);
    canceled = job->canceled || (job->state == JOB_CANCELED);
    ese_mutex_unlock(q->jobs_mutex);

    if (!q->shutting_down && !canceled && job->callback) {
      job->callback(job->id, job->user_data, job->main_result);
    }

    if (job->cleanup) {
      job->cleanup(job->id, job->user_data, job->main_result);
    }

    ese_mutex_lock(q->global_mutex);
    dlist_remove_by_value(q->global_queue, job);
    ese_mutex_unlock(q->global_mutex);

    ese_mutex_lock(q->jobs_mutex);
    int_hashmap_remove(q->jobs_by_id, (uint64_t)job->id);
    ese_mutex_unlock(q->jobs_mutex);

    _free_job_value(job);
  }
  dlist_free(local);

  bool has_more = false;
  ese_mutex_lock(q->global_mutex);
  it = dlist_iter_create(q->global_queue);
  if (dlist_iter_next(it, &val)) {
    has_more = true;
  }
  dlist_iter_free(it);
  ese_mutex_unlock(q->global_mutex);

  return has_more;
}

/**
 * @brief Destroy the job queue and cleanup resources.
 *
 * @param q Job queue to destroy
 */
void ese_job_queue_destroy(EseJobQueue *q) {
  log_assert("JOBQ", q, "destroy called with NULL");

  ese_mutex_lock(q->global_mutex);
  ese_mutex_lock(q->jobs_mutex);

  EseDListIter *it = dlist_iter_create(q->global_queue);
  void *val = NULL;
  while (dlist_iter_next(it, &val)) {
    EseJob *job = (EseJob *)val;
    if (job->state == JOB_PENDING) {
      job->canceled = true;
      job->state = JOB_CANCELED;
    } else if (job->state == JOB_RUNNING) {
      job->canceled = true;
    }
  }
  dlist_iter_free(it);

  q->shutting_down = true;
  ese_cond_broadcast(q->global_cond);

  ese_mutex_unlock(q->jobs_mutex);
  ese_mutex_unlock(q->global_mutex);

  for (;;) {
    bool more = ese_job_queue_process(q);

    bool need_workers = false;
    ese_mutex_lock(q->global_mutex);
    ese_mutex_lock(q->jobs_mutex);
    it = dlist_iter_create(q->global_queue);
    while (dlist_iter_next(it, &val)) {
      EseJob *job = (EseJob *)val;
      JobState s = job->state;
      if (s == JOB_RESULTS_READY || s == JOB_RESULTS_PROCESSED ||
          s == JOB_RUNNING) {
        need_workers = true;
        break;
      }
    }
    dlist_iter_free(it);
    ese_mutex_unlock(q->jobs_mutex);

    if (!more && !need_workers) {
      ese_mutex_unlock(q->global_mutex);
      break;
    }

    ese_cond_broadcast(q->global_cond);
    (void)ese_cond_wait_timeout(q->global_cond, q->global_mutex, 10);
    ese_mutex_unlock(q->global_mutex);
  }

  for (uint32_t i = 0; i < q->num_workers; i++) {
    if (q->workers[i].thread) {
      ese_thread_join(q->workers[i].thread);
    }
  }

  // deinit_fn already called in worker thread; do not call here

  ese_mutex_lock(q->global_mutex);
  void *v = NULL;
  while ((v = dlist_pop_front(q->global_queue)) != NULL) {
    EseJob *job = (EseJob *)v;
    if (job->cleanup) {
      job->cleanup(job->id, job->user_data, job->main_result);
    }
    ese_mutex_lock(q->jobs_mutex);
    int_hashmap_remove(q->jobs_by_id, (uint64_t)job->id);
    ese_mutex_unlock(q->jobs_mutex);
    _free_job_value(job);
  }
  ese_mutex_unlock(q->global_mutex);

  memory_manager.free(q->workers);
  int_hashmap_destroy(q->jobs_by_id);
  dlist_free(q->global_queue);

  ese_cond_destroy(q->global_cond);
  ese_mutex_destroy(q->global_mutex);
  ese_mutex_destroy(q->jobs_mutex);

  memory_manager.free(q);
}
