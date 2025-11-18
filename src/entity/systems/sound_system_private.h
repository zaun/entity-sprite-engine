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
#ifndef ESE_SOUND_SYSTEM_PRIVATE_H
#define ESE_SOUND_SYSTEM_PRIVATE_H

#include "entity/components/entity_component_sound.h"
#include "entity/components/entity_component_listener.h"
#include "vendor/miniaud/miniaudio.h"

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Internal data for the sound system.
 *
 * Maintains a dynamically-sized array of sound component pointers for
 * future per-frame updates.
 */
typedef struct {
    bool ready;
    ma_context context;
    ma_device_info* device_infos;
    ma_uint32 device_info_count;

    EseEntityComponentSound **sounds;      /** Array of sound component pointers */
    size_t sound_count;                    /** Current number of tracked sounds */
    size_t sound_capacity;                 /** Allocated capacity of the sounds array */

    EseEntityComponentListener **listeners; /** Array of listener component pointers */
    size_t listener_count;                  /** Current number of tracked listeners */
    size_t listener_capacity;               /** Allocated capacity of the listeners array */
} SoundSystemData;

/**
 * @brief Global pointer to the active sound system data.
 *
 * Used by Lua bindings to expose read-only device information via the
 * `Sound` global table.
 */
extern SoundSystemData *g_sound_system_data;

#endif /* ESE_SOUND_SYSTEM_PRIVATE_H */
