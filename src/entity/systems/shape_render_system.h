/*
 * Project: Entity Sprite Engine
 *
 * Shape rendering system for the Entity Component System. This system handles
 * shape rendering by collecting all shape components and submitting them to the
 * renderer in the LATE phase after all updates and Lua scripts have completed.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef ESE_SHAPE_RENDER_SYSTEM_H
#define ESE_SHAPE_RENDER_SYSTEM_H

typedef struct EseEngine EseEngine;
typedef struct EseSystemManager EseSystemManager;

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Creates and returns a new Shape Render System.
 *
 * @details The shape render system handles shape rendering for all entities
 *          with shape components. It runs in the LATE phase after all updates
 *          and Lua scripts have completed.
 *
 * @return Pointer to the newly created shape render system.
 */
EseSystemManager *shape_render_system_create(void);

/**
 * @brief Registers the shape render system with the engine.
 *
 * @details Convenience function that creates and registers the shape render
 *          system with the engine in one call.
 *
 * @param eng Pointer to the engine.
 */
void engine_register_shape_render_system(EseEngine *eng);

#endif /* ESE_SHAPE_RENDER_SYSTEM_H */
