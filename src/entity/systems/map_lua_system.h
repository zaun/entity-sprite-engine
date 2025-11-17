/*
 * Project: Entity Sprite Engine
 *
 * Map Lua system for the Entity Component System. This system is responsible
 * for running Lua-driven behavior for map components (map_init / map_update
 * and related script hooks) in the LUA phase, keeping the components
 * themselves as POD + Lua bindings.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef ESE_MAP_LUA_SYSTEM_H
#define ESE_MAP_LUA_SYSTEM_H

// ========================================
// Forward declarations
// ========================================

typedef struct EseEngine EseEngine;
typedef struct EseSystemManager EseSystemManager;

// ========================================
// Public Functions
// ========================================

/**
 * @brief Creates and returns a new Map Lua System.
 *
 * @details The map Lua system tracks all map components and runs their Lua
 *          update behavior each frame in the LUA phase.
 */
EseSystemManager *map_lua_system_create(void);

/**
 * @brief Registers the map Lua system with the engine.
 *
 * @details Convenience function that creates and registers the map Lua system
 *          with the engine in one call.
 *
 * @param eng Pointer to the engine.
 */
void engine_register_map_lua_system(EseEngine *eng);

#endif /* ESE_MAP_LUA_SYSTEM_H */
