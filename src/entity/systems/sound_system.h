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
#ifndef ESE_SOUND_SYSTEM_H
#define ESE_SOUND_SYSTEM_H

// ========================================
// Forward declarations
// ========================================

typedef struct EseEngine EseEngine;
typedef struct EseSystemManager EseSystemManager;

// ========================================
// Public Functions
// ========================================

/**
 * @brief Registers the sound system with the engine.
 *
 * @details Convenience function that creates and registers the sound
 *          system with the engine in one call.
 *
 * @param eng Pointer to the engine.
 */
void engine_register_sound_system(EseEngine *eng);

#endif /* ESE_SOUND_SYSTEM_H */
