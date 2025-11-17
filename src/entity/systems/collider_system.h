/*
 * Project: Entity Sprite Engine
 *
 * Collider update system for the Entity Component System. This system is
 * responsible for updating collider world bounds each frame based on the
 * entity's current position, allowing collider components themselves to
 * remain POD + Lua bindings.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef ESE_COLLIDER_SYSTEM_H
#define ESE_COLLIDER_SYSTEM_H

// ========================================
// Forward declarations
// ========================================

typedef struct EseEngine EseEngine;
typedef struct EseSystemManager EseSystemManager;

// ========================================
// Public Functions
// ========================================

/**
 * @brief Creates and returns a new Collider System.
 *
 * @details The collider system tracks all collider components and updates their
 *          world bounds each frame in an EARLY or LATE phase (depending on how
 *          it is registered with the engine).
 *
 * @return Pointer to the newly created collider system.
 */
EseSystemManager *collider_system_create(void);

/**
 * @brief Registers the collider system with the engine.
 *
 * @details Convenience function that creates and registers the collider system
 *          with the engine in one call.
 *
 * @param eng Pointer to the engine.
 */
void engine_register_collider_system(EseEngine *eng);

#endif /* ESE_COLLIDER_SYSTEM_H */
