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
#include "entity/components/entity_component_music.h"
#include "utility/thread.h"
#include "vendor/miniaud/miniaudio.h"

// Forward declarations
typedef struct EseEngine EseEngine;

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
    ma_device_info *device_infos;
    ma_uint32 device_info_count;

    bool device_initialized;                /** Whether output_device has been initialized */
    ma_device_id output_device_id;
    ma_device output_device;

    EseEngine *engine;                      /** Back-pointer to the owning engine */

    EseEntityComponentSound **sounds;       /** Array of sound component pointers */
    size_t sound_count;                     /** Current number of tracked sounds */
    size_t sound_capacity;                  /** Allocated capacity of the sounds array */

    EseEntityComponentMusic **music;        /** Array of music component pointers */
    size_t music_count;                     /** Current number of tracked music components */
    size_t music_capacity;                  /** Allocated capacity of the music array */

    EseEntityComponentListener **listeners; /** Array of listener component pointers */
    size_t listener_count;                  /** Current number of tracked listeners */
    size_t listener_capacity;               /** Allocated capacity of the listeners array */

    EseMutex *mutex;                        /** Protects access to this struct from multiple threads */
} SoundSystemData;

/**
 * @brief Global pointer to the active sound system data.
 *
 * Used by Lua bindings to expose read-only device information via the
 * `Sound` global table.
 */
extern SoundSystemData *g_sound_system_data;

/**
 * @brief Select the playback device by index (0-based).
 *
 * Stops and uninitializes any previously selected device, then initializes
 * and starts the new one.
 *
 * @param index Zero-based index into device_infos.
 * @return true on success, false on failure.
 */
bool sound_system_select_device_index(ma_uint32 index);

/**
 * @brief Get the name of the currently selected playback device.
 *
 * @return Pointer to the device name string, or NULL if no device is selected.
 */
const char *sound_system_selected_device_name(void);

#endif /* ESE_SOUND_SYSTEM_PRIVATE_H */
