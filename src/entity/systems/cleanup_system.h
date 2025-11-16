/*
 * Project: Entity Sprite Engine
 *
 * Component cleanup system for the Entity Component System. This system handles
 * deferred component removal to prevent race conditions with parallel systems.
 * Components are queued for removal and processed in the CLEANUP phase after
 * all other systems have completed.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef ESE_CLEANUP_SYSTEM_H
#define ESE_CLEANUP_SYSTEM_H

typedef struct EseEngine EseEngine;
typedef struct EseSystemManager EseSystemManager;
typedef struct EseEntity EseEntity;
typedef struct EseEntityComponent EseEntityComponent;

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Creates and returns a new Component Cleanup System.
 *
 * @details The cleanup system handles deferred component removal to prevent
 *          race conditions with parallel systems. It runs in the CLEANUP phase
 *          after all other systems have completed.
 *
 * @return Pointer to the newly created cleanup system.
 */
EseSystemManager *cleanup_system_create(void);

/**
 * @brief Registers the cleanup system with the engine.
 *
 * @details Convenience function that creates and registers the cleanup system
 *          with the engine in one call.
 *
 * @param eng Pointer to the engine.
 */
void engine_register_cleanup_system(EseEngine *eng);

#endif /* ESE_CLEANUP_SYSTEM_H */

