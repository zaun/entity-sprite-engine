/*
 * Project: Entity Sprite Engine
 *
 * Sprite rendering system for the Entity Component System. This system handles
 * sprite rendering by collecting all sprite components and submitting them to
 * the renderer in the LATE phase after all updates and Lua scripts have
 * completed.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef ESE_SPRITE_RENDER_SYSTEM_H
#define ESE_SPRITE_RENDER_SYSTEM_H

typedef struct EseEngine EseEngine;
typedef struct EseSystemManager EseSystemManager;

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Creates and returns a new Sprite Render System.
 *
 * @details The sprite render system handles sprite rendering for all entities
 *          with sprite components. It runs in the LATE phase after all updates
 *          and Lua scripts have completed.
 *
 * @return Pointer to the newly created sprite render system.
 */
EseSystemManager *sprite_render_system_create(void);

/**
 * @brief Registers the sprite render system with the engine.
 *
 * @details Convenience function that creates and registers the sprite render
 *          system with the engine in one call.
 *
 * @param eng Pointer to the engine.
 */
void engine_register_sprite_render_system(EseEngine *eng);

#endif /* ESE_SPRITE_RENDER_SYSTEM_H */
