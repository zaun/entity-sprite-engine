/*
 * Project: Entity Sprite Engine
 *
 * Public API for the ECS System architecture. Defines the base System
 * infrastructure for the Entity Component System. Systems operate on
 * collections of components across all relevant entities, providing better
 * performance through cache locality and enabling parallelism.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef ESE_SYSTEM_MANAGER_H
#define ESE_SYSTEM_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

typedef struct EseEngine EseEngine;
typedef struct EseEntityComponent EseEntityComponent;

/**
 * @brief Phase bucket for coarse scheduling of system execution.
 *
 * @details Systems are organized into phases to ensure proper ordering of
 * operations. EARLY phase runs in parallel before Lua, LUA phase is
 * single-threaded, and LATE phase runs in parallel after Lua but before
 * rendering.
 */
typedef enum {
    SYS_PHASE_EARLY, /** Parallel execution before Lua scripts */
    SYS_PHASE_LUA,   /** Single-threaded execution for Lua components */
    SYS_PHASE_LATE   /** Parallel execution after Lua, before render */
} EseSystemPhase;

/**
 * @brief Opaque handle to a System instance.
 */
typedef struct EseSystemManager EseSystemManager;

/**
 * @brief Virtual table defining the behavior of a System.
 *
 * @details All callback functions are optional (can be NULL).
 *          Systems should implement only the callbacks they need.
 */
typedef struct EseSystemManagerVTable {
    /**
     * @brief Called once when the system is registered with the engine.
     *
     * @param self Pointer to the system instance.
     * @param eng Pointer to the engine.
     */
    void (*init)(EseSystemManager *self, EseEngine *eng);

    /**
     * @brief Called every frame to update the system.
     *
     * @param self Pointer to the system instance.
     * @param eng Pointer to the engine.
     * @param dt Delta time in seconds since the last frame.
     */
    void (*update)(EseSystemManager *self, EseEngine *eng, float dt);

    /**
     * @brief Determines whether this system is interested in a component.
     *
     * @param self Pointer to the system instance.
     * @param comp Pointer to the component to check.
     * @return true if the system wants to track this component, false
     * otherwise.
     */
    bool (*accepts)(EseSystemManager *self, const EseEntityComponent *comp);

    /**
     * @brief Notification that a component has been added to an entity.
     *
     * @param self Pointer to the system instance.
     * @param eng Pointer to the engine.
     * @param comp Pointer to the component that was added.
     */
    void (*on_component_added)(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp);

    /**
     * @brief Notification that a component is about to be removed from an
     * entity.
     *
     * @param self Pointer to the system instance.
     * @param eng Pointer to the engine.
     * @param comp Pointer to the component that will be removed.
     */
    void (*on_component_removed)(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp);

    /**
     * @brief Called when the system is being destroyed.
     *
     * @param self Pointer to the system instance.
     * @param eng Pointer to the engine.
     */
    void (*shutdown)(EseSystemManager *self, EseEngine *eng);
} EseSystemManagerVTable;

/**
 * @brief Creates a new System instance.
 *
 * @param vt Pointer to the virtual table defining the system's behavior (must
 * not be NULL).
 * @param phase The execution phase for this system.
 * @param user_data Optional user data pointer to store system-specific state.
 * @return Pointer to the newly created system.
 */
EseSystemManager *system_manager_create(const EseSystemManagerVTable *vt, EseSystemPhase phase,
                                        void *user_data);

/**
 * @brief Destroys a System instance and frees its resources.
 *
 * @details Calls the shutdown callback if defined, then frees the system.
 *          Ignores NULL inputs per project convention.
 *
 * @param sys Pointer to the system to destroy.
 * @param eng Pointer to the engine.
 */
void system_manager_destroy(EseSystemManager *sys, EseEngine *eng);

/**
 * @brief Registers a system with the engine.
 *
 * @details Adds the system to the engine's system list and calls its init
 * callback. The engine takes ownership of the system pointer.
 *
 * @param eng Pointer to the engine.
 * @param sys Pointer to the system to register.
 */
void engine_add_system(EseEngine *eng, EseSystemManager *sys);

/**
 * @brief Runs all systems in a specific phase.
 *
 * @details Iterates through all registered systems and updates those in the
 * specified phase. If parallel is true, systems are dispatched to the job
 * queue; otherwise they run sequentially on the main thread.
 *
 * @param eng Pointer to the engine.
 * @param phase The phase to execute.
 * @param dt Delta time in seconds.
 * @param parallel Whether to run systems in parallel using the job queue.
 */
void engine_run_phase(EseEngine *eng, EseSystemPhase phase, float dt, bool parallel);

/**
 * @brief Notifies all systems that a component has been added.
 *
 * @details Called internally by entity_component_add. Systems that accept the
 * component will have their on_component_added callback invoked.
 *
 * @param eng Pointer to the engine.
 * @param c Pointer to the component that was added.
 */
void engine_notify_comp_add(EseEngine *eng, EseEntityComponent *c);

/**
 * @brief Notifies all systems that a component is about to be removed.
 *
 * @details Called internally by entity_component_remove. Systems that accept
 * the component will have their on_component_removed callback invoked.
 *
 * @param eng Pointer to the engine.
 * @param c Pointer to the component that will be removed.
 */
void engine_notify_comp_rem(EseEngine *eng, EseEntityComponent *c);

#endif /* ESE_SYSTEM_MANAGER_H */
