/*
 * Project: Entity Sprite Engine
 *
 * Map update system for the Entity Component System. This system is
 * responsible for running map component update logic (world-bounds
 * maintenance and map script callbacks) each frame so that map
 * components themselves can remain POD + Lua bindings.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef ESE_MAP_SYSTEM_H
#define ESE_MAP_SYSTEM_H

// ========================================
// Forward declarations
// ========================================

typedef struct EseEngine EseEngine;
typedef struct EseSystemManager EseSystemManager;

// ========================================
// Public Functions
// ========================================

/**
 * @brief Creates and returns a new Map System.
 *
 * @details The map system tracks all map components and runs their update
 *          behavior each frame in the LUA phase.
 *
 * @return Pointer to the newly created map system.
 */
EseSystemManager *map_system_create(void);

/**
 * @brief Registers the map system with the engine.
 *
 * @details Convenience function that creates and registers the map system
 *          with the engine in one call.
 *
 * @param eng Pointer to the engine.
 */
void engine_register_map_system(EseEngine *eng);

#endif /* ESE_MAP_SYSTEM_H */
