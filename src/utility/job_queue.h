/*
 * Project: Entity Sprite Engine
 *
 * Implementation of the job queue system for managing worker threads and
 * asynchronous job execution. Provides a thread-safe queue with worker thread
 * management, job scheduling, cancellation, and callback processing on the main
 * thread.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef ESE_JOB_QUEUE_H
#define ESE_JOB_QUEUE_H

#include "utility/thread.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Sentinel worker id indicating any worker can handle the job.
 *
 * Used by push APIs to indicate the job may be executed by any available
 * worker.
 */
#define ESE_WORKER_ANY ((ese_worker_id_t)UINT32_MAX)

#define ESE_JOB_NOT_QUEUED ((ese_job_id_t) - 3)
#define ESE_JOB_NOT_FOUND ((int32_t)-1)
#define ESE_JOB_TIMEOUT ((int32_t)-2)
#define ESE_JOB_NOT_COMPLETED ((int32_t)0)
#define ESE_JOB_COMPLETED ((int32_t)1)
#define ESE_JOB_CANCELED ((int32_t)2)

/**
 * @typedef EseJobQueue
 * @brief Opaque handle for a job queue instance.
 *
 * The internal representation is private to the implementation. Users interact
 * with the queue via the public API functions declared in this header.
 */
typedef struct EseJobQueue EseJobQueue;

/**
 * @typedef ese_job_id_t
 * @brief Type for job IDs.
 */
typedef int32_t ese_job_id_t;

/**
 * @typedef ese_worker_id_t
 * @brief Type for worker IDs.
 */
typedef uint32_t ese_worker_id_t;

/**
 * @typedef job_result_copy_function
 * @brief Type for a function that copies a job result.
 */
typedef void *(*job_result_copy_function)(const void *worker_result, size_t worker_size,
                                          size_t *out_size);

/**
 * @typedef job_result_free_function
 * @brief Type for a function that frees a job result.
 */
typedef void (*job_result_free_function)(void *worker_result);

/**
 * @typedef worker_init_function
 * @brief Type for a function that initializes a worker thread.
 */
typedef void *(*worker_init_function)(ese_worker_id_t worker_id);

/**
 * @typedef worker_deinit_function
 * @brief Type for a function that deinitializes a worker thread.
 */
typedef void (*worker_deinit_function)(ese_worker_id_t worker_id, void *thread_data);

/**
 * @typedef JobResult
 * @brief Type for a job result.
 */
typedef struct JobResult {
    void *result;                     /** Worker thread result */
    size_t size;                      /** Result size in bytes */
    job_result_copy_function copy_fn; /** Optional deep-copy function */
    job_result_free_function free_fn; /** Optional worker-side destructor */
} JobResult;

/**
 * @typedef worker_thread_job_function
 * @brief Type for a function that executes a job on a worker thread.
 */
typedef JobResult (*worker_thread_job_function)(void *thread_data, const void *user_data,
                                                volatile bool *canceled);

/**
 * @typedef main_thread_job_callback
 * @brief Type for a function that executes a job on the main thread.
 */
typedef void (*main_thread_job_callback)(ese_job_id_t job_id, void *user_data, void *result);

/**
 * @typedef main_thread_job_cleanup
 * @brief Type for a function that cleans up a job on the main thread.
 */
typedef void (*main_thread_job_cleanup)(ese_job_id_t job_id, void *user_data, void *result);

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @section thread_safety Job Queue Thread Safety
 *
 * Public API functions are designed to be thread-safe for concurrent pushes
 * (ese_job_queue_push and ese_job_queue_push_on_worker). Destroying a queue and
 * processing callbacks should be coordinated with worker activity to avoid
 * races. See per-function docs for details.
 */

/**
 * @brief Create a job queue with the specified number of worker threads.
 *
 * @param num_workers Number of worker threads to create
 * @param init_fn Optional initialization function for worker threads
 * @param deinit_fn Optional cleanup function for worker threads
 * @return EseJobQueue* New job queue instance
 */
EseJobQueue *ese_job_queue_create(uint32_t num_workers, worker_init_function init_fn,
                                  worker_deinit_function deinit_fn);

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
 * @details Non-blocking operation. Returns job id on success or
 * ESE_JOB_NOT_QUEUED on failure. Queue owns job metadata. Caller owns user_data
 * and result.
 *
 * @param queue Job queue to push to
 * @param fn Required function to execute on worker thread
 * @param callback Optional callback to execute on main thread when job
 * completes
 * @param cleanup Required cleanup function to execute on main thread when job
 * destroyed
 * @param user_data User data to pass to the job function
 * @return ese_job_id_t Job ID or ESE_JOB_NOT_QUEUED on failure
 */
ese_job_id_t ese_job_queue_push(EseJobQueue *queue, worker_thread_job_function fn,
                                main_thread_job_callback callback, main_thread_job_cleanup cleanup,
                                void *user_data);

/**
 * @brief Push a job to a specific worker thread.
 *
 * @details Non-blocking operation. worker_id must be in range [0,
 * num_workers-1] or ESE_WORKER_ANY.
 *
 * @param queue Job queue to push to
 * @param worker_id Specific worker thread ID or ESE_WORKER_ANY
 * @param fn Required function to execute on worker thread
 * @param callback Optional callback to execute on main thread when job
 * completes
 * @param cleanup Required cleanup function to execute on main thread when job
 * destroyed
 * @param user_data User data to pass to the job function
 * @return ese_job_id_t Job ID or ESE_JOB_NOT_QUEUED on failure
 */
ese_job_id_t ese_job_queue_push_on_worker(EseJobQueue *queue, ese_worker_id_t worker_id,
                                          worker_thread_job_function fn,
                                          main_thread_job_callback callback,
                                          main_thread_job_cleanup cleanup, void *user_data);

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
 * @details timeout_ms of 0 means wait forever. Returns one of the ESE_JOB_*
 * codes or ESE_JOB_TIMEOUT or ESE_JOB_NOT_FOUND.
 *
 * Note: This call blocks the current thread and is safe to use from any thread.
 *
 * @param queue Job queue containing the job
 * @param job_id ID of the job to wait for
 * @param timeout_ms Timeout in milliseconds (0 for infinite wait)
 * @return int32_t Job status code
 */
int32_t ese_job_queue_wait_for_completion(EseJobQueue *queue, ese_job_id_t job_id,
                                          size_t timeout_ms);

/**
 * @brief Cancel a pending or running job.
 *
 * @details If job not started, it will not execute and callback will not be
 * called. If job is already running it will finish execution, the callback will
 * not be called. If job is already completed, it will not be canceled as the
 * callback has already been called. Returns ESE_JOB_NOT_FOUND, ESE_JOB_CANCELED
 * or ESE_JOB_COMPLETED.
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
 * @return bool True if there are more jobs to process, false otherwise
 */
bool ese_job_queue_process(EseJobQueue *queue);

#endif /* ESE_JOB_QUEUE_H */
