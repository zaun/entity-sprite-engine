/*
 * project: Entity Sprite Engine
 *
 * Job queue system for managing worker threads and asynchronous job execution.
 *
 * Job queue design / implementation:
 *
 * On queue creation, the worker_thread_init_function is run for each worker thread. The rational
 * is to allow the user to create worker threads for a specific purpose, such as a LUA thread.
 * There is no failure path for this function. If the function returns NULL, then the thread_data
 * will be NULL.
 *
 * On queue destruction, the worker_thread_deinit_function is run for each worker thread. The
 * rational is to allow the user to cleanup worker threads for a specific purpose, such as a
 * LUA thread.
 *
 * 1. Jobs that are created are pushed to a single global queue protected by a single global lock.
 * 2. While holding the global lock, a worker inspects and updates queue structures: it removes a
 *    chosen job from the global queue and appends it to that worker's queue.
 * 3. If the front job targets a different worker (i.e. != ESE_WORKER_ANY and != my id), the worker
 *    continues scanning the global queue (under the global lock) for the first eligible job.
 * 4. After the job is moved to the worker's queue, the worker releases the global lock and executes
 *    the job on its thread without holding any queue locks.
 * 5. When a job completes, the worker, while holding the completed-queue lock, removes the job from
 *    its worker queue and appends it to the completed queue, then releases the completed-queue lock.
 *    Workers must not invoke callbacks.
 * 6. The main thread processes the completed queue. While holding the completed-queue lock, the main
 *    thread moves all jobs from the completed queue to a local finished list, then releases the lock.
 * 7. The main thread calls the callback for each job in the finished list that is not canceled with
 *    no lock held. After callback invocation, the cleanup function is called and the job is removed
 *    from the finished list and freed, user_data and result are the caller's responsibility. For
 *    canceled jobs the main thread calls the cleanup function and frees the job without invoking a
 *    callback.
 * 8. Cancellation uses a lock-free atomic `canceled` bool on each job. If cancel runs while the job
 *    is still in the global queue, cancel acquires the global lock, removes the job from the global
 *    queue, sets `canceled = true`, appends the job to the completed queue, releases the lock, and
 *    returns. If the job has already been moved to a worker queue, cancel atomically sets
 *    `canceled = true` and returns immediately. The worker may or may not look at the `canceled`
 *    flag to determine if the job has been canceled.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */

#ifndef ESE_JOB_QUEUE_H
#define ESE_JOB_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "utility/thread.h"

// ========================================
// Defines and Structs
// ========================================

typedef struct EseJobQueue EseJobQueue;

typedef int32_t ese_job_id_t;
typedef uint32_t ese_worker_id_t;

#define ESE_WORKER_ANY ((ese_worker_id_t)UINT32_MAX)
#define ESE_JOB_NOT_QUEUED ((ese_job_id_t)-3)
#define ESE_JOB_NOT_FOUND ((int32_t)-1)
#define ESE_JOB_TIMEOUT ((int32_t)-2)
#define ESE_JOB_NOT_COMPLETED ((int32_t)0)
#define ESE_JOB_COMPLETED ((int32_t)1)
#define ESE_JOB_CANCELED ((int32_t)2)

// Called once for each worker thread at queue creation, returns thread data
typedef void *(*worker_thread_init_function)(ese_worker_id_t worker_id);
// Called once for each worker thread at queue destruction, cleans up and freed thread data
typedef void (*worker_thread_deinit_function)(ese_worker_id_t worker_id, void *thread_data);
// Called by each worker thread to execute a job, returns result
typedef void *(*worker_thread_job_function)(void *thread_data, void *user_data);
// Called by the main thread to invoke the callback for a completed job
typedef void (*main_thread_job_callback)(ese_job_id_t job_id, void *result);
// Called by the main thread to cleanup a completed job's data
typedef void (*main_thread_job_cleanup)(ese_job_id_t job_id, void *user_data, void *result);

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
EseJobQueue *ese_job_queue_create(
    uint32_t num_workers,
    worker_thread_init_function init_fn,
    worker_thread_deinit_function deinit_fn);

/**
 * @brief Destroy the job queue and cleanup resources.
 *
 * @details Blocks until current jobs complete, cancels pending jobs, and frees
 *          queue and job structures. Cleanup functions are called for each job.
 *          Callback functions are not called for canceled jobs.
 *
 * @param queue Job queue to destroy
 */
void ese_job_queue_destroy(EseJobQueue *queue);

/**
 * @brief Push a job to any available worker thread.
 *
 * @details Non-blocking operation. Returns job id on success or ESE_JOB_NOT_QUEUED
 *          on failure. Queue owns job metadata. Caller owns user_data and result.
 *
 * @param queue Job queue to push to
 * @param fn Required function to execute on worker thread
 * @param callback Optional callback to execute on main thread when job completes
 * @param cleanup Required cleanup function to execute on main thread when job destroyed
 * @param user_data User data to pass to the job function
 * @return ese_job_id_t Job ID or ESE_JOB_NOT_QUEUED on failure
 */
ese_job_id_t ese_job_queue_push(
    EseJobQueue *queue,
    worker_thread_job_function fn,
    main_thread_job_callback callback,
    main_thread_job_cleanup cleanup,
    void *user_data);

/**
 * @brief Push a job to a specific worker thread.
 *
 * @details Non-blocking operation. worker_id must be in range [0, num_workers-1]
 *          or ESE_WORKER_ANY.
 *
 * @param queue Job queue to push to
 * @param worker_id Specific worker thread ID or ESE_WORKER_ANY
 * @param fn Required function to execute on worker thread
 * @param callback Optional callback to execute on main thread when job completes
 * @param cleanup Required cleanup function to execute on main thread when job destroyed
 * @param user_data User data to pass to the job function
 * @return ese_job_id_t Job ID or ESE_JOB_NOT_QUEUED on failure
 */
ese_job_id_t ese_job_queue_push_on_worker(
    EseJobQueue *queue,
    ese_worker_id_t worker_id,
    worker_thread_job_function fn,
    main_thread_job_callback callback,
    main_thread_job_cleanup cleanup,
    void *user_data);

/**
 * @brief Query the status of a job.
 *
 * @details Non-blocking operation. Returns one of the ESE_JOB_* codes or
 *          ESE_JOB_NOT_FOUND.
 *
 * @param queue Job queue containing the job
 * @param job_id ID of the job to query
 * @return int32_t Job status code
 */
int32_t ese_job_queue_status(EseJobQueue *queue, ese_job_id_t job_id);

/**
 * @brief Wait for a job to complete.
 *
 * @details timeout_ms of 0 means wait forever. Returns one of the ESE_JOB_* codes
 *          or ESE_JOB_TIMEOUT or ESE_JOB_NOT_FOUND.
 *
 * @param queue Job queue containing the job
 * @param job_id ID of the job to wait for
 * @param timeout_ms Timeout in milliseconds (0 for infinite wait)
 * @return int32_t Job status code
 */
int32_t ese_job_queue_wait_for_completion(EseJobQueue *queue, ese_job_id_t job_id, size_t timeout_ms);

/**
 * @brief Cancel a pending or running job.
 *
 * @details If job not started, it will not execute and callback will not be called.
 *          If job is already running it will finish execution, the callback will not be called.
 *          If job is already completed, it will not be canceled as the callback has already
 *          been called.
 *          Returns ESE_JOB_NOT_FOUND, ESE_JOB_CANCELED or ESE_JOB_COMPLETED.
 *
 * @param queue Job queue containing the job
 * @param job_id ID of the job to cancel
 * @return int32_t Job status code
 */
int32_t ese_job_queue_cancel_callback(EseJobQueue *queue, ese_job_id_t job_id);

/**
 * @brief Process completed job callbacks on the main thread.
 *
 * @details This function should be called periodically by the main thread to
 *          invoke callbacks for completed jobs. It processes all completed
 *          jobs and invokes their callbacks. After callback invocation, jobs
 *          are removed from the completed queue and freed. user_data and result
 *          are the caller's responsibility to free.
 *
 * @param queue Job queue to process callbacks for
 * @return int Number of callbacks processed
 */
int ese_job_queue_poll_callbacks(EseJobQueue *queue);

#endif /* ESE_JOB_QUEUE_H */