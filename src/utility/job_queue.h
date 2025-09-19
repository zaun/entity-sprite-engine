#ifndef ESE_JOB_QUEUE_H
#define ESE_JOB_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "thread.h"

/* Job queue for thread-safe communication */
typedef struct EseJobQueue_ts EseJobQueue;
typedef struct EseJob_ts EseJob;

/* Job function signature */
typedef void (*EseJobFn)(void *data);

/* Job queue operations */
EseJobQueue *ese_job_queue_create(void);
void ese_job_queue_destroy(EseJobQueue *queue);
bool ese_job_queue_push(EseJobQueue *queue, EseJobFn fn, void *data, bool wait_for_completion);
/* Convenience: push a job and wait for its completion (timeout_ms <= 0 = wait forever) */
bool ese_job_queue_push_and_wait(EseJobQueue *queue, EseJobFn fn, void *data, int timeout_ms);
void ese_job_queue_wait_for_completion(EseJobQueue *queue);
void ese_job_queue_shutdown(EseJobQueue *queue);

/* Job operations */
EseJob *ese_job_create(EseJobFn fn, void *data, bool wait_for_completion);
void ese_job_destroy(EseJob *job);
void ese_job_execute(EseJob *job);
bool ese_job_is_completed(const EseJob *job);
bool ese_job_wait_for_completion(EseJob *job, int timeout_ms);

/* Worker helpers */
EseJob *ese_job_queue_pop(EseJobQueue *queue);
void ese_job_queue_mark_completed(EseJobQueue *queue, EseJob *job);

/* Worker thread function (provided for convenience) */
void *ese_job_queue_worker_thread(void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* ESE_JOB_QUEUE_H */