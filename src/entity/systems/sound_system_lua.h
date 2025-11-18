/*
 * Project: Entity Sprite Engine
 *
 * Sound system for the Entity Component System. This system will be
 * responsible for managing sound component playback. The initial
 * implementation provides only a skeleton with an empty update
 * function.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef ESE_SOUND_SYSTEM_LUA_H
#define ESE_SOUND_SYSTEM_LUA_H

typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Initializes the Lua bindings for the Sound global table.
 *
 * Creates a global `Sound` table with a read-only `devices` field that
 * reflects the playback devices detected by the sound system.
 */
void sound_system_lua_init(EseLuaEngine *engine);

#endif /* ESE_SOUND_SYSTEM_LUA_H */
