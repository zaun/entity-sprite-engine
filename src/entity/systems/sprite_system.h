/*
 * Project: Entity Sprite Engine
 *
 * Sprite animation system for the Entity Component System. This system manages
 * sprite animation by advancing animation frames for all active sprite
 * components. It operates on sprite component data without modifying the
 * component's core structure.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef ESE_SPRITE_SYSTEM_H
#define ESE_SPRITE_SYSTEM_H

typedef struct EseEngine EseEngine;
typedef struct EseSystemManager EseSystemManager;

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Creates and returns a new Sprite Animation System.
 *
 * @details The sprite system handles sprite animation frame advancement
 *          for all entities with sprite components. It runs in the EARLY
 *          phase before Lua scripts execute.
 *
 * @return Pointer to the newly created sprite system.
 */
EseSystemManager *sprite_system_create(void);

/**
 * @brief Registers the sprite system with the engine.
 *
 * @details Convenience function that creates and registers the sprite
 *          system with the engine in one call.
 *
 * @param eng Pointer to the engine.
 */
void engine_register_sprite_system(EseEngine *eng);

#endif /* ESE_SPRITE_SYSTEM_H */
