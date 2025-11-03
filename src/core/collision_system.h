/*
 * Project: Entity Sprite Engine
 *
 * Public API for the collision detection system. Handles parallel collision
 * detection between entities using spatial indexing and worker threads.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef ESE_COLLISION_SYSTEM_H
#define ESE_COLLISION_SYSTEM_H

#include <stddef.h>
#include "core/system_manager.h"
#include "utility/array.h"

// ========================================
// Defines and Structs
// ========================================

typedef struct SpatialIndex SpatialIndex;
typedef struct EseCollisionHit EseCollisionHit;
typedef struct EseSystemManager EseSystemManager;
typedef struct EseEngine EseEngine;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Register the collision system with the engine.
 *
 * @details Creates and registers a collision detection system that uses the
 *          engine's spatial index for broad-phase collision detection. The
 *          system will use parallel workers if a job queue is available.
 *
 * @param eng Engine pointer to register the system with.
 * @param worker_count Number of worker threads to use (0 defaults to 1).
 */
void engine_register_collision_system(EseEngine *eng, size_t worker_count);

#endif // ESE_COLLISION_SYSTEM_H
