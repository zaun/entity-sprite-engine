/*
 * Project: Entity Sprite Engine
 *
 * Implementation of the Sound System. This system will manage sound
 * playback for entities that have sound components. For now, the
 * update function is intentionally empty and no behavior is applied
 * to tracked components.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "core/system_manager.h"
#include "core/system_manager_private.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component_sound.h"
#include "entity/components/entity_component_listener.h"
#include "entity/systems/sound_system.h"
#include "entity/systems/sound_system_lua.h"
#include "entity/systems/sound_system_private.h"
#include "entity/entity_private.h"
#include "utility/log.h"
#include "vendor/miniaud/miniaudio.h"
#include "audio/pcm.h"
#include <string.h>

SoundSystemData *g_sound_system_data = NULL;

// ========================================
// PRIVATE FUNCTIONS
// ========================================

static void sound_sys_data_callback(ma_device *device, void *output, const void *input,
                                    ma_uint32 frame_count) {
    (void)input;

    if (!device || !output || frame_count == 0) {
        return;
    }

    SoundSystemData *data = (SoundSystemData *)device->pUserData;

    // Fallback: if we don't have system data or the engine yet, just output silence.
    ma_uint32 bytes_per_frame =
        ma_get_bytes_per_frame(device->playback.format, device->playback.channels);
    if (!data || !data->engine || bytes_per_frame == 0 || device->playback.channels == 0 ||
        device->playback.format != ma_format_f32) {
        if (bytes_per_frame > 0) {
            memset(output, 0, (size_t)frame_count * bytes_per_frame);
        }
        return;
    }

    EseEngine *eng = data->engine;

    float *out_samples = (float *)output;
    ma_uint32 channels = device->playback.channels;
    ma_uint32 total_frames = frame_count;
    ma_uint32 total_samples = total_frames * channels;

    // Start with silence in the output buffer.
    memset(out_samples, 0, (size_t)total_samples * sizeof(float));

    // Lock while we read/update shared sound component state.
    EseMutex *mtx = data->mutex;
    if (mtx) {
        ese_mutex_lock(mtx);
    }

    if (!eng || !data->sounds || data->sound_count == 0) {
        if (mtx) {
            ese_mutex_unlock(mtx);
        }
        return; // nothing to mix
    }
    // Mix all active & playing sound components into the output buffer.
    for (size_t i = 0; i < data->sound_count; i++) {
        EseEntityComponentSound *sound = data->sounds[i];
        if (!sound) {
            continue;
        }

        // Only mix active, playing sounds with a valid asset id.
        if (!sound->base.active || !sound->playing || !sound->sound_name) {
            continue;
        }

        EsePcm *pcm = engine_get_sound(eng, sound->sound_name);
        if (!pcm) {
            continue;
        }
        const float *pcm_samples = pcm_get_samples(pcm);
        uint32_t pcm_frames = pcm_get_frame_count(pcm);
        uint32_t pcm_channels = pcm_get_channels(pcm);

        if (!pcm_samples || pcm_frames == 0 || pcm_channels == 0) {
            continue;
        }

        // Keep the component's frame_count in sync with the underlying asset.
        sound->frame_count = pcm_frames;

        uint32_t frame_pos = sound->current_frame;

        for (ma_uint32 f = 0; f < total_frames; f++) {
            if (!sound->playing) {
                break;
            }

            if (frame_pos >= pcm_frames) {
                if (sound->repeat && pcm_frames > 0) {
                    frame_pos = 0; // loop
                } else {
                    // Sound finished: stop playback and rewind so a future play() starts from the beginning.
                    sound->playing = false;
                    frame_pos = 0;
                    break;
                }
            }

            if (!sound->playing) {
                break;
            }

            // Mix this frame for each output channel.
            for (ma_uint32 ch = 0; ch < channels; ch++) {
                float sample = 0.0f;

                if (pcm_channels == 1) {
                    // Mono source: duplicate into all output channels.
                    sample = pcm_samples[frame_pos];
                } else {
                    // Use the matching channel if available, otherwise clamp to the last.
                    uint32_t src_ch = (ch < pcm_channels) ? ch : (pcm_channels - 1);
                    sample = pcm_samples[frame_pos * pcm_channels + src_ch];
                }

                out_samples[f * channels + ch] += sample;
            }

            frame_pos++;
        }

        // Persist playback position back onto the component.
        sound->current_frame = frame_pos;
    }

    // Simple hard clipping to keep samples in [-1, 1] after mixing.
    for (ma_uint32 i = 0; i < total_samples; i++) {
        if (out_samples[i] > 1.0f) {
            out_samples[i] = 1.0f;
        } else if (out_samples[i] < -1.0f) {
            out_samples[i] = -1.0f;
        }
    }

    if (mtx) {
        ese_mutex_unlock(mtx);
    }
}

/**
 * @brief Checks if the system accepts this component type.
 */
static bool sound_sys_accepts(EseSystemManager *self, const EseEntityComponent *comp) {
    (void)self;
    if (!comp) {
        return false;
    }
    return comp->type == ENTITY_COMPONENT_SOUND || comp->type == ENTITY_COMPONENT_LISTENER;
}

/**
 * @brief Called when a sound component is added to an entity.
 */
static void sound_sys_on_add(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp) {
    (void)eng;

    if (!comp || !comp->data || !g_sound_system_data) {
        return;
    }

    EseMutex *mtx = g_sound_system_data->mutex;
    if (mtx) {
        ese_mutex_lock(mtx);
    }

    if (comp->type == ENTITY_COMPONENT_SOUND) {
        if (g_sound_system_data->sound_count == g_sound_system_data->sound_capacity) {
            g_sound_system_data->sound_capacity = g_sound_system_data->sound_capacity ? g_sound_system_data->sound_capacity * 2 : 64;
            g_sound_system_data->sounds = memory_manager.realloc(
                g_sound_system_data->sounds, sizeof(EseEntityComponentSound *) * g_sound_system_data->sound_capacity,
                MMTAG_S_SPRITE); // reuse sprite system tag for now
        }

        g_sound_system_data->sounds[g_sound_system_data->sound_count++] = (EseEntityComponentSound *)comp->data;
    } else if (comp->type == ENTITY_COMPONENT_LISTENER) {
        if (g_sound_system_data->listener_count == g_sound_system_data->listener_capacity) {
            g_sound_system_data->listener_capacity = g_sound_system_data->listener_capacity ? g_sound_system_data->listener_capacity * 2 : 4;
            g_sound_system_data->listeners = memory_manager.realloc(
                g_sound_system_data->listeners, sizeof(EseEntityComponentListener *) * g_sound_system_data->listener_capacity,
                MMTAG_S_SPRITE); // reuse sprite system tag for now
        }

        g_sound_system_data->listeners[g_sound_system_data->listener_count++] = (EseEntityComponentListener *)comp->data;
    }

    if (mtx) {
        ese_mutex_unlock(mtx);
    }
}

/**
 * @brief Called when a sound component is removed from an entity.
 */
static void sound_sys_on_remove(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp) {
    (void)eng;

    if (!comp || !comp->data || !g_sound_system_data) {
        return;
    }

    EseMutex *mtx = g_sound_system_data->mutex;
    if (mtx) {
        ese_mutex_lock(mtx);
    }

    if (comp->type == ENTITY_COMPONENT_SOUND) {
        if (g_sound_system_data->sound_count == 0) {
            if (mtx) {
                ese_mutex_unlock(mtx);
            }
            return;
        }

        EseEntityComponentSound *sc = (EseEntityComponentSound *)comp->data;

        for (size_t i = 0; i < g_sound_system_data->sound_count; i++) {
            if (g_sound_system_data->sounds[i] == sc) {
                g_sound_system_data->sounds[i] = g_sound_system_data->sounds[--g_sound_system_data->sound_count];
                if (mtx) {
                    ese_mutex_unlock(mtx);
                }
                return;
            }
        }
    } else if (comp->type == ENTITY_COMPONENT_LISTENER) {
        if (g_sound_system_data->listener_count == 0) {
            if (mtx) {
                ese_mutex_unlock(mtx);
            }
            return;
        }

        EseEntityComponentListener *lc = (EseEntityComponentListener *)comp->data;

        for (size_t i = 0; i < g_sound_system_data->listener_count; i++) {
            if (g_sound_system_data->listeners[i] == lc) {
                g_sound_system_data->listeners[i] = g_sound_system_data->listeners[--g_sound_system_data->listener_count];
                if (mtx) {
                    ese_mutex_unlock(mtx);
                }
                return;
            }
        }
    }

    if (mtx) {
        ese_mutex_unlock(mtx);
    }
}

/**
 * @brief Initialize the sound system.
 */
static void sound_sys_init(EseSystemManager *self, EseEngine *eng) {
    g_sound_system_data = memory_manager.calloc(1, sizeof(SoundSystemData), MMTAG_S_SPRITE);
    if (!g_sound_system_data) {
        log_error("SOUND_SYSTEM", "Failed to allocate SoundSystemData");
        return;
    }

    g_sound_system_data->mutex = ese_mutex_create();
    if (!g_sound_system_data->mutex) {
        log_error("SOUND_SYSTEM", "Failed to create sound system mutex");
        memory_manager.free(g_sound_system_data);
        g_sound_system_data = NULL;
        return;
    }

    // Store a back-pointer to the engine for use in the audio callback.
    g_sound_system_data->engine = eng;

    // Attach system data so shutdown can clean up correctly.
    self->data = g_sound_system_data;

    // Initialize audio context.
    ma_result result = ma_context_init(NULL, 0, NULL, &g_sound_system_data->context);
    if (result != MA_SUCCESS) {
        log_error("SOUND_SYSTEM", "Failed to initialize audio context: %s",
                  ma_result_description(result));
        g_sound_system_data->ready = false;
        return;
    }

    // Get playback devices.
    result = ma_context_get_devices(&g_sound_system_data->context,
                                    &g_sound_system_data->device_infos,
                                    &g_sound_system_data->device_info_count,
                                    NULL, 0);
    if (result != MA_SUCCESS) {
        log_error("SOUND_SYSTEM", "Failed to get playback devices: %s",
                  ma_result_description(result));
        ma_context_uninit(&g_sound_system_data->context);

        g_sound_system_data->ready = false;
        g_sound_system_data->device_info_count = 0;
        g_sound_system_data->device_infos = NULL;
        return;
    }

    g_sound_system_data->ready = true;

    // Log devices.
    log_verbose("SOUND_SYSTEM", "Playback devices:");
    for (ma_uint32 i = 0; i < g_sound_system_data->device_info_count; i++) {
        log_verbose("SOUND_SYSTEM", "  %s %u: %s",
                    g_sound_system_data->device_infos[i].isDefault ? "**" : "  ",
                    i, g_sound_system_data->device_infos[i].name);
    }

    // Auto-select and start the default playback device on startup.
    if (g_sound_system_data->device_info_count > 0) {
        ma_uint32 default_index = 0;
        for (ma_uint32 i = 0; i < g_sound_system_data->device_info_count; i++) {
            if (g_sound_system_data->device_infos[i].isDefault) {
                default_index = i;
                break;
            }
        }

        if (!sound_system_select_device_index(default_index)) {
            log_error("SOUND_SYSTEM", "Failed to initialize default playback device (index %u)",
                      default_index);
        }
    }
}

/**
 * @brief Update all sound components.
 *
 * The initial skeleton implementation intentionally performs no work
 * and simply returns an empty job result.
 */
static EseSystemJobResult sound_sys_update(EseSystemManager *self, EseEngine *eng, float dt) {
    (void)self;
    (void)eng;
    (void)dt;

    EseSystemJobResult res = {0};
    return res;
}

/**
 * @brief Clean up the sound system.
 */
static void sound_sys_shutdown(EseSystemManager *self, EseEngine *eng) {
    (void)eng;

    SoundSystemData *d = (SoundSystemData *)self->data;
    if (!d) {
        return;
    }

    // Stop and uninitialize any active playback device. This will also stop
    // the audio callback from firing before we tear down shared state.
    if (d->device_initialized) {
        ma_device_stop(&d->output_device);
        ma_device_uninit(&d->output_device);
        d->device_initialized = false;
    }

    if (d->sounds) {
        memory_manager.free(d->sounds);
    }
    if (d->listeners) {
        memory_manager.free(d->listeners);
    }

    if (d->ready) {
        ma_context_uninit(&d->context);
        d->ready = false;
    }

    if (d->mutex) {
        EseMutex *mtx = d->mutex;
        d->mutex = NULL;
        ese_mutex_destroy(mtx);
    }

    memory_manager.free(d);
    self->data = NULL;

    if (g_sound_system_data == d) {
        g_sound_system_data = NULL;
    }
}

/**
 * @brief Virtual table for the sound system.
 */
static const EseSystemManagerVTable SoundSystemVTable = {
    .init = sound_sys_init,
    .update = sound_sys_update,
    .accepts = sound_sys_accepts,
    .on_component_added = sound_sys_on_add,
    .on_component_removed = sound_sys_on_remove,
    .shutdown = sound_sys_shutdown};

// ========================================
// PUBLIC/INTERNAL FUNCTIONS
// ========================================

bool sound_system_select_device_index(ma_uint32 index) {
    if (!g_sound_system_data || !g_sound_system_data->ready) {
        log_error("SOUND_SYSTEM", "Cannot select device: sound system not ready");
        return false;
    }

    if (!g_sound_system_data->device_infos ||
        index >= g_sound_system_data->device_info_count) {
        log_error("SOUND_SYSTEM", "Cannot select device: index %u out of range", index);
        return false;
    }

    // Stop and uninit previous device if needed.
    if (g_sound_system_data->device_initialized) {
        ma_device_stop(&g_sound_system_data->output_device);
        ma_device_uninit(&g_sound_system_data->output_device);
        g_sound_system_data->device_initialized = false;
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.pDeviceID = &g_sound_system_data->device_infos[index].id;
    // Use device's native sample rate if 0; format/channels are chosen here.
    config.sampleRate = 0;
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.dataCallback = sound_sys_data_callback;
    config.pUserData = g_sound_system_data;

    ma_result result = ma_device_init(&g_sound_system_data->context, &config,
                                      &g_sound_system_data->output_device);
    if (result != MA_SUCCESS) {
        log_error("SOUND_SYSTEM", "Failed to init playback device %u (%s): %s", index,
                  g_sound_system_data->device_infos[index].name,
                  ma_result_description(result));
        return false;
    }

    result = ma_device_start(&g_sound_system_data->output_device);
    if (result != MA_SUCCESS) {
        log_error("SOUND_SYSTEM", "Failed to start playback device %u (%s): %s", index,
                  g_sound_system_data->device_infos[index].name,
                  ma_result_description(result));
        ma_device_uninit(&g_sound_system_data->output_device);
        return false;
    }

    g_sound_system_data->output_device_id = g_sound_system_data->device_infos[index].id;
    g_sound_system_data->device_initialized = true;

    log_debug("SOUND_SYSTEM", "Selected playback device %u: %s", index,
              g_sound_system_data->device_infos[index].name);

    return true;
}

const char *sound_system_selected_device_name(void) {
    if (!g_sound_system_data || !g_sound_system_data->ready ||
        !g_sound_system_data->device_infos ||
        !g_sound_system_data->device_initialized) {
        return NULL;
    }

    const char *name = NULL;
    EseMutex *mtx = g_sound_system_data->mutex;
    if (mtx) {
        ese_mutex_lock(mtx);
    }

    // Find the device whose ID matches the currently active output_device_id.
    for (ma_uint32 i = 0; i < g_sound_system_data->device_info_count; i++) {
        if (ma_device_id_equal(&g_sound_system_data->device_infos[i].id,
                               &g_sound_system_data->output_device_id)) {
            name = g_sound_system_data->device_infos[i].name;
            break;
        }
    }

    if (mtx) {
        ese_mutex_unlock(mtx);
    }

    return name;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

void engine_register_sound_system(EseEngine *eng) {
    log_assert("SOUND_SYS", eng, "engine_register_sound_system called with NULL engine");
    log_assert("SOUND_SYSTEM", g_sound_system_data == NULL, "Only one sound system permitted");

    engine_add_system(eng, system_manager_create(&SoundSystemVTable, SYS_PHASE_EARLY, NULL));

    // Initialize Lua bindings for the Sound global once the system is registered
    sound_system_lua_init(eng->lua_engine);
}
