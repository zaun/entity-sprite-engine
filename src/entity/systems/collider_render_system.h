/*
 * Project: Entity Sprite Engine
 *
 * Collider rendering system for the Entity Component System. This system
 * handles collider rendering by collecting all collider components and
 * submitting them to the renderer in the LATE phase after all updates and Lua
 * scripts have completed.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef ESE_COLLIDER_RENDER_SYSTEM_H
#define ESE_COLLIDER_RENDER_SYSTEM_H

// ========================================
// Defines and Structs
// ========================================

typedef struct EseEngine EseEngine;
typedef struct EseSystemManager EseSystemManager;

// ========================================
// Public Functions
// ========================================

/**
 * @brief Creates and returns a new Collider Render System.
 *
 * @details The collider render system handles collider rendering for all
 * entities with collider components. It runs in the LATE phase after all
 * updates and Lua scripts have completed.
 *
 * @return Pointer to the newly created collider render system.
 */
EseSystemManager *collider_render_system_create(void);

/**
 * @brief Registers the collider render system with the engine.
 *
 * @details Convenience function that creates and registers the collider render
 *          system with the engine in one call.
 *
 * @param eng Pointer to the engine.
 */
void engine_register_collider_render_system(EseEngine *eng);

#endif /* ESE_COLLIDER_RENDER_SYSTEM_H */
