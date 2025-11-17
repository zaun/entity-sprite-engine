/*
 * Project: Entity Sprite Engine
 *
 * Implementation of the ECS System architecture. Manages the lifecycle of
 * systems, their execution in phases, and parallel execution when enabled.
 *
 * Details:
 * Systems are organized into three phases: EARLY (parallel before Lua), LUA
 * (single-threaded), and LATE (parallel after Lua). Systems can be executed
 * sequentially or in parallel using the job queue. Component add/remove events
 * notify interested systems based on their acceptance filters. Each system has
 * optional callbacks for initialization, update, component tracking, and
 * shutdown.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#include "core/system_manager.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "core/system_manager_private.h"
#include "utility/job_queue.h"
#include "utility/log.h"
#include <string.h>

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief User data structure for system job execution.
 */
typedef struct {
    EseSystemManager *sys;
    EseEngine *eng;
    float dt;
} SystemJobData;

// ========================================
// PRIVATE FUNCTIONS
// ========================================

/**
 * @brief Worker thread function for parallel system execution.
 *
 * @param thread_data Thread data (unused)
 * @param user_data Pointer to SystemJobData
 * @param canceled Cancel flag (unused)
 * @return JobResult Optional job result published by the system
 */
static JobResult _system_job_worker(void *thread_data, const void *user_data,
                                    volatile bool *canceled) {
    (void)thread_data;
    (void)canceled;

    SystemJobData *job_data = (SystemJobData *)user_data;
    if (job_data && job_data->sys && job_data->sys->vt && job_data->sys->vt->update) {
        // Delegate to the system's update callback, which returns a
        // JobResult-compatible payload. An all-zero result means
        // "no work to apply on the main thread".
        return job_data->sys->vt->update(job_data->sys, job_data->eng, job_data->dt);
    }

    JobResult res = {.result = NULL, .size = 0, .copy_fn = NULL, .free_fn = NULL};
    return res;
}

/**
 * @brief Main-thread callback for system jobs.
 *
 * @param job_id Job ID (unused)
 * @param user_data Pointer to SystemJobData
 * @param result Result pointer (main-thread copy) from JobResult
 */
static void _system_job_callback(ese_job_id_t job_id, void *user_data, void *result) {
    (void)job_id;
    SystemJobData *job_data = (SystemJobData *)user_data;
    if (!job_data || !job_data->sys || !job_data->eng || !result) {
        return;
    }

    EseSystemManager *sys = job_data->sys;
    if (sys->vt && sys->vt->apply_result) {
        sys->vt->apply_result(sys, job_data->eng, result);
    }
}

/**
 * @brief Cleanup function for system jobs.
 *
 * @param job_id Job ID (unused)
 * @param user_data Pointer to SystemJobData to free
 * @param result Result pointer (main-thread copy)
 *
 * @note The lifetime of the result payload is managed by the system's
 *       apply_result callback (for parallel phases) or by engine_run_phase
 *       (for sequential phases). This function is only responsible for freeing
 *       the SystemJobData wrapper.
 */
static void _system_job_cleanup(ese_job_id_t job_id, void *user_data, void *result) {
    (void)job_id;
    (void)result;

    if (user_data) {
        memory_manager.free(user_data);
    }
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Create a system manager instance.
 *
 * @param vt Virtual table defining system behavior
 * @param phase Execution phase for this system
 * @param user_data Optional user data pointer
 * @return EseSystemManager* Created system manager
 */
EseSystemManager *system_manager_create(const EseSystemManagerVTable *vt, EseSystemPhase phase,
                                        void *user_data) {
    log_assert("SYSTEM_MANAGER", vt, "system_manager_create called with NULL vtable");

    EseSystemManager *s = memory_manager.calloc(1, sizeof(EseSystemManager), MMTAG_ENGINE);
    s->vt = vt;
    s->phase = phase;
    s->data = user_data;
    s->active = true;
    return s;
}

/**
 * @brief Destroy a system manager instance.
 *
 * @param sys System manager to destroy
 * @param eng Engine pointer
 */
void system_manager_destroy(EseSystemManager *sys, EseEngine *eng) {
    log_assert("SYSTEM_MANAGER", sys, "system_manager_destroy called with NULL system");
    log_assert("SYSTEM_MANAGER", eng, "system_manager_destroy called with NULL engine");

    if (sys->vt && sys->vt->shutdown) {
        sys->vt->shutdown(sys, eng);
    }

    memory_manager.free(sys);
}

/**
 * @brief Add a system to the engine.
 *
 * @param eng Engine pointer
 * @param sys System to add
 */
void engine_add_system(EseEngine *eng, EseSystemManager *sys) {
    log_assert("SYSTEM_MANAGER", eng, "engine_add_system called with NULL engine");
    log_assert("SYSTEM_MANAGER", sys, "engine_add_system called with NULL system");

    if (eng->sys_count == eng->sys_cap) {
        size_t nc = eng->sys_cap ? eng->sys_cap * 2 : 4;
        eng->systems =
            memory_manager.realloc(eng->systems, sizeof(EseSystemManager *) * nc, MMTAG_ENGINE);
        eng->sys_cap = nc;
    }

    eng->systems[eng->sys_count++] = sys;

    if (sys->vt && sys->vt->init) {
        sys->vt->init(sys, eng);
    }
}

/**
 * @brief Run all systems in a specific phase.
 *
 * @param eng Engine pointer
 * @param phase Phase to run
 * @param dt Delta time
 * @param parallel Whether to run in parallel using job queue
 */
void engine_run_phase(EseEngine *eng, EseSystemPhase phase, float dt, bool parallel) {
    log_assert("SYSTEM_MANAGER", eng, "engine_run_phase called with NULL engine");

    // For parallel execution, we need to track job IDs to wait for them
    ese_job_id_t *job_ids = NULL;
    size_t job_count = 0;

    if (parallel && eng->job_queue) {
        // Allocate space for job IDs (worst case: all systems in this phase)
        job_ids = memory_manager.malloc(sizeof(ese_job_id_t) * eng->sys_count, MMTAG_ENGINE);
    }

    for (size_t i = 0; i < eng->sys_count; i++) {
        EseSystemManager *s = eng->systems[i];
        if (!s->active || s->phase != phase) {
            continue;
        }

        if (parallel && eng->job_queue) {
            // Allocate job data for this system
            SystemJobData *job_data = memory_manager.malloc(sizeof(SystemJobData), MMTAG_ENGINE);
            job_data->sys = s;
            job_data->eng = eng;
            job_data->dt = dt;

            // Push the job to any available worker
            ese_job_id_t job_id = ese_job_queue_push(eng->job_queue, _system_job_worker,
                                                     _system_job_callback,
                                                     _system_job_cleanup, job_data);

            if (job_id != ESE_JOB_NOT_QUEUED) {
                job_ids[job_count++] = job_id;
            }
        } else {
            if (s->vt && s->vt->update) {
                // Sequential execution: run the system update directly on this
                // thread. If the system returns a non-empty JobResult and
                // defines apply_result, invoke it here and then clean up the
                // worker-result payload.
                JobResult r = s->vt->update(s, eng, dt);

                if (r.result) {
                    if (s->vt->apply_result) {
                        // In the sequential path we are already on the main
                        // thread, so we can treat the worker result as the
                        // main-thread payload and skip any extra copy step.
                        s->vt->apply_result(s, eng, r.result);
                    }

                    if (r.free_fn) {
                        r.free_fn(r.result);
                    } else {
                        memory_manager.free(r.result);
                    }
                }
            }
        }
    }

    // Wait for all parallel jobs to complete
    if (parallel && eng->job_queue && job_ids) {
        for (size_t i = 0; i < job_count; i++) {
            ese_job_queue_wait_for_completion(eng->job_queue, job_ids[i], 0);
        }
        memory_manager.free(job_ids);
    }
}

/**
 * @brief Notify all systems that a component has been added.
 *
 * @param eng Engine pointer
 * @param c Component that was added
 */
void engine_notify_comp_add(EseEngine *eng, EseEntityComponent *c) {
    log_assert("SYSTEM_MANAGER", eng, "engine_notify_comp_add called with NULL engine");
    log_assert("SYSTEM_MANAGER", c, "engine_notify_comp_add called with NULL component");

    log_verbose("SYSTEM_MANAGER", "Notifying systems of component add");

    for (size_t i = 0; i < eng->sys_count; i++) {
        EseSystemManager *s = eng->systems[i];
        if (!s->active) {
            continue;
        }

        if (s->vt->accepts && s->vt->accepts(s, c)) {
            log_verbose("SYSTEM_MANAGER", "System %zu accepts component", i);
            if (s->vt->on_component_added) {
                s->vt->on_component_added(s, eng, c);
            }
        }
    }
}

/**
 * @brief Notify all systems that a component is about to be removed.
 *
 * @param eng Engine pointer
 * @param c Component that will be removed
 */
void engine_notify_comp_rem(EseEngine *eng, EseEntityComponent *c) {
    log_assert("SYSTEM_MANAGER", eng, "engine_notify_comp_rem called with NULL engine");
    log_assert("SYSTEM_MANAGER", c, "engine_notify_comp_rem called with NULL component");

    for (size_t i = 0; i < eng->sys_count; i++) {
        EseSystemManager *s = eng->systems[i];
        if (!s->active) {
            continue;
        }

        if (s->vt->accepts && s->vt->accepts(s, c)) {
            if (s->vt->on_component_removed) {
                s->vt->on_component_removed(s, eng, c);
            }
        }
    }
}
