/*
 * Project: Entity Sprite Engine
 *
 * Map rendering system for the Entity Component System. This system handles map
 * rendering by collecting all map components and submitting them to the
 * renderer in the LATE phase after all updates and Lua scripts have completed.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef ESE_MAP_RENDER_SYSTEM_H
#define ESE_MAP_RENDER_SYSTEM_H

typedef struct EseEngine EseEngine;
typedef struct EseSystemManager EseSystemManager;

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Creates and returns a new Map Render System.
 *
 * @details The map render system handles map rendering for all entities
 *          with map components. It runs in the LATE phase after all updates
 *          and Lua scripts have completed.
 *
 * @return Pointer to the newly created map render system.
 */
EseSystemManager *map_render_system_create(void);

/**
 * @brief Registers the map render system with the engine.
 *
 * @details Convenience function that creates and registers the map render
 *          system with the engine in one call.
 *
 * @param eng Pointer to the engine.
 */
void engine_register_map_render_system(EseEngine *eng);

#endif /* ESE_MAP_RENDER_SYSTEM_H */
