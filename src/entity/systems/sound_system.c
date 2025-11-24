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
#include "types/point.h"
#include "utility/log.h"
#include "vendor/miniaud/miniaudio.h"
#include "audio/pcm.h"
#include <math.h>
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

    // Find the first active listener (component and entity) if any.
    EseEntityComponentListener *listener = NULL;
    EsePoint *listener_pos = NULL;
    float listener_x = 0.0f;
    float listener_y = 0.0f;
    float listener_volume = 1.0f; // normalized [0,1]
    float listener_max_distance = 0.0f;
    bool listener_spatial = false;

    // New listener distance attenuation controls.
    float listener_attenuation = 1.0f; // [0,1], 1 = full attenuation, 0 = none
    float listener_rolloff = 1.0f;     // exponent shaping curve

    if (data->listeners && data->listener_count > 0) {
        for (size_t li = 0; li < data->listener_count; li++) {
            EseEntityComponentListener *lc = data->listeners[li];
            if (!lc || !lc->base.active) {
                continue;
            }

            EseEntity *ent = lc->base.entity;
            if (!ent || !ent->active || ent->destroyed) {
                continue;
            }

            listener = lc;
            listener_spatial = lc->spatial;

            // Clamp and normalize listener volume from [0,100] -> [0,1].
            float vol = lc->volume;
            if (vol < 0.0f) {
                vol = 0.0f;
            } else if (vol > 100.0f) {
                vol = 100.0f;
            }
            listener_volume = vol / 100.0f;

            listener_max_distance = lc->max_distance;
            if (listener_max_distance < 0.0f) {
                listener_max_distance = 0.0f;
            }

            // Cache listener attenuation/rolloff with sane clamps so the
            // callback uses only validated values.
            listener_attenuation = lc->attenuation;
            if (listener_attenuation < 0.0f) {
                listener_attenuation = 0.0f;
            } else if (listener_attenuation > 1.0f) {
                listener_attenuation = 1.0f;
            }

            listener_rolloff = lc->rolloff;
            if (listener_rolloff < 0.1f) {
                listener_rolloff = 0.1f;
            } else if (listener_rolloff > 8.0f) {
                listener_rolloff = 8.0f;
            }

            listener_pos = ent->position;
            if (listener_pos) {
                listener_x = ese_point_get_x(listener_pos);
                listener_y = ese_point_get_y(listener_pos);
            }

            break; // Use the first active listener only.
        }
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

        EseEntity *sound_entity = sound->base.entity;
        if (!sound_entity || !sound_entity->active || sound_entity->destroyed) {
            continue;
        }

        // Use the cached PCM pointer resolved on the main thread.
        EsePcm *pcm = sound->pcm;
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

        // Compute per-sound gain and optional stereo panning based on
        // listener/sound positions and spatial flags.
        float base_gain = 1.0f;
        float left_gain = 1.0f;
        float right_gain = 1.0f;
        bool apply_spatial_pan = false;

        EsePoint *sound_pos = sound_entity->position;

        // Apply listener master volume if we have a listener.
        if (listener) {
            base_gain = listener_volume;
        }

        // Spatialization only when:
        //  - the sound is marked spatial,
        //  - we have an active listener that wants spatial audio,
        //  - both entities have positions and max_distance > 0.
        if (sound->spatial && listener && listener_spatial && sound_pos && listener_pos &&
            listener_max_distance > 0.0f) {
            float sx = ese_point_get_x(sound_pos);
            float sy = ese_point_get_y(sound_pos);

            float dx = sx - listener_x;
            float dy = sy - listener_y;
            float distance = sqrtf(dx * dx + dy * dy);

            if (distance >= listener_max_distance) {
                base_gain = 0.0f;
            } else {
                // Normalized distance [0, 1) within the audible radius.
                float norm = distance / listener_max_distance;
                if (norm < 0.0f) {
                    norm = 0.0f;
                } else if (norm > 1.0f) {
                    norm = 1.0f;
                }

                // Base distance falloff curve using the listener's rolloff
                // exponent. rolloff == 1 -> linear, >1 -> faster drop, <1 -> slower.
                float full_att = powf(1.0f - norm, listener_rolloff);
                if (full_att < 0.0f) {
                    full_att = 0.0f;
                } else if (full_att > 1.0f) {
                    full_att = 1.0f;
                }

                // Blend between no attenuation (1.0) and the full curve based
                // on the listener's attenuation factor.
                float distance_gain = (1.0f - listener_attenuation) + listener_attenuation * full_att;
                if (distance_gain < 0.0f) {
                    distance_gain = 0.0f;
                } else if (distance_gain > 1.0f) {
                    distance_gain = 1.0f;
                }

                base_gain *= distance_gain;

                // Left/right pan from relative x offset.
                //
                // Previously we normalized by max_distance directly, which made
                // panning very subtle when the listener's audible radius was
                // large (e.g., max_distance = 1000 and the sound only 100
                // units away). Instead, use a fraction of max_distance as the
                // reference for panning so sounds near the listener produce a
                // strong stereo effect while still clamping to [-1, 1].
                float pan = 0.0f;
                float pan_ref = listener_max_distance * 0.25f; // quarter of radius
                if (pan_ref < 1.0f) {
                    pan_ref = 1.0f;
                }
                // Negate dx so that when the listener moves to the right of
                // the sound, the perceived audio moves toward the right ear,
                // matching typical stereo expectations.
                pan = -dx / pan_ref;
                if (pan < -1.0f) {
                    pan = -1.0f;
                } else if (pan > 1.0f) {
                    pan = 1.0f;
                }

                if (channels >= 2) {
                    float l = 1.0f;
                    float r = 1.0f;
                    if (pan >= 0.0f) {
                        // Sound is to the right: reduce left channel.
                        l = 1.0f - pan;
                        r = 1.0f;
                    } else {
                        // Sound is to the left: reduce right channel.
                        l = 1.0f;
                        r = 1.0f + pan; // pan is negative
                    }
                    left_gain = base_gain * l;
                    right_gain = base_gain * r;
                    apply_spatial_pan = true;
                }
            }
        }

        if (!apply_spatial_pan) {
            left_gain = base_gain;
            right_gain = base_gain;
        }

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

                float gain = base_gain;
                if (channels >= 2) {
                    if (apply_spatial_pan) {
                        if (ch == 0) {
                            gain = left_gain;
                        } else if (ch == 1) {
                            gain = right_gain;
                        }
                    } else {
                        gain = base_gain;
                    }
                }

                out_samples[f * channels + ch] += sample * gain;
            }

            frame_pos++;
        }

        // Persist playback position back onto the component.
        sound->current_frame = frame_pos;
    }

    // Mix all active & playing music components (non-spatial) into the output buffer.
    if (data->music && data->music_count > 0) {
        for (size_t i = 0; i < data->music_count; i++) {
            EseEntityComponentMusic *music = data->music[i];
            if (!music) {
                continue;
            }

            // Only mix active, playing music components with a non-empty playlist.
            if (!music->base.active || !music->playing || music->track_count == 0) {
                continue;
            }

            EseEntity *music_entity = music->base.entity;
            if (!music_entity || !music_entity->active || music_entity->destroyed) {
                continue;
            }

            // Ensure current_track is in range.
            if (music->current_track >= music->track_count) {
                music->current_track = 0;
                music->current_frame = 0;
                music->current_pcm = NULL;
                music->frame_count = 0;
            }

            // Lazily resolve PCM for the current track.
            if (!music->current_pcm) {
                const char *track_name = music->tracks[music->current_track];
                if (!track_name) {
                    continue;
                }

                EsePcm *pcm = engine_get_music(eng, track_name);
                if (!pcm) {
                    continue;
                }

                music->current_pcm = pcm;
                uint32_t frames = pcm_get_frame_count(pcm);
                music->frame_count = frames;
                if (music->current_frame > frames) {
                    music->current_frame = frames;
                }
            }

            EsePcm *pcm = music->current_pcm;
            if (!pcm) {
                continue;
            }

            const float *pcm_samples = pcm_get_samples(pcm);
            uint32_t pcm_frames = pcm_get_frame_count(pcm);
            uint32_t pcm_channels = pcm_get_channels(pcm);

            if (!pcm_samples || pcm_frames == 0 || pcm_channels == 0) {
                continue;
            }

            uint32_t frame_pos = music->current_frame;

            // Music is affected by listener master volume but not spatialization.
            float gain = 1.0f;
            if (listener) {
                gain = listener_volume;
            }

            for (ma_uint32 f = 0; f < total_frames; f++) {
                if (!music->playing) {
                    break;
                }

                if (frame_pos >= pcm_frames) {
                    // Move to the next track in the playlist or stop.
                    if (music->track_count == 0) {
                        music->playing = false;
                        frame_pos = 0;
                        break;
                    }

                    uint32_t next_index = music->current_track + 1;
                    bool has_next = false;
                    if (next_index < music->track_count) {
                        has_next = true;
                    } else if (music->repeat && music->track_count > 0) {
                        next_index = 0;
                        has_next = true;
                    }

                    if (!has_next) {
                        music->playing = false;
                        frame_pos = 0;
                        music->current_pcm = NULL;
                        music->frame_count = 0;
                        break;
                    }

                    music->current_track = next_index;
                    music->current_frame = 0;
                    music->current_pcm = NULL;
                    music->frame_count = 0;

                    const char *track_name = music->tracks[music->current_track];
                    if (!track_name) {
                        music->playing = false;
                        frame_pos = 0;
                        break;
                    }

                    EsePcm *next_pcm = engine_get_music(eng, track_name);
                    if (!next_pcm) {
                        music->playing = false;
                        frame_pos = 0;
                        break;
                    }

                    music->current_pcm = next_pcm;
                    pcm = next_pcm;
                    pcm_samples = pcm_get_samples(pcm);
                    pcm_frames = pcm_get_frame_count(pcm);
                    pcm_channels = pcm_get_channels(pcm);

                    if (!pcm_samples || pcm_frames == 0 || pcm_channels == 0) {
                        music->playing = false;
                        frame_pos = 0;
                        music->current_pcm = NULL;
                        music->frame_count = 0;
                        break;
                    }

                    music->frame_count = pcm_frames;
                    frame_pos = music->current_frame;
                }

                if (!music->playing) {
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

                    out_samples[f * channels + ch] += sample * gain;
                }

                frame_pos++;
            }

            music->current_frame = frame_pos;
        }
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
    return comp->type == ENTITY_COMPONENT_SOUND || comp->type == ENTITY_COMPONENT_MUSIC ||
           comp->type == ENTITY_COMPONENT_LISTENER;
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
            g_sound_system_data->sound_capacity = g_sound_system_data->sound_capacity
                                                      ? g_sound_system_data->sound_capacity * 2
                                                      : 64;
            g_sound_system_data->sounds = memory_manager.realloc(
                g_sound_system_data->sounds,
                sizeof(EseEntityComponentSound *) * g_sound_system_data->sound_capacity,
                MMTAG_S_SPRITE); // reuse sprite system tag for now
        }

        g_sound_system_data->sounds[g_sound_system_data->sound_count++] =
            (EseEntityComponentSound *)comp->data;
    } else if (comp->type == ENTITY_COMPONENT_MUSIC) {
        if (g_sound_system_data->music_count == g_sound_system_data->music_capacity) {
            g_sound_system_data->music_capacity = g_sound_system_data->music_capacity
                                                      ? g_sound_system_data->music_capacity * 2
                                                      : 8;
            g_sound_system_data->music = memory_manager.realloc(
                g_sound_system_data->music,
                sizeof(EseEntityComponentMusic *) * g_sound_system_data->music_capacity,
                MMTAG_S_SPRITE); // reuse sprite system tag for now
        }

        g_sound_system_data->music[g_sound_system_data->music_count++] =
            (EseEntityComponentMusic *)comp->data;
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
                g_sound_system_data->sounds[i] =
                    g_sound_system_data->sounds[--g_sound_system_data->sound_count];
                if (mtx) {
                    ese_mutex_unlock(mtx);
                }
                return;
            }
        }
    } else if (comp->type == ENTITY_COMPONENT_MUSIC) {
        if (g_sound_system_data->music_count == 0) {
            if (mtx) {
                ese_mutex_unlock(mtx);
            }
            return;
        }

        EseEntityComponentMusic *mc = (EseEntityComponentMusic *)comp->data;

        for (size_t i = 0; i < g_sound_system_data->music_count; i++) {
            if (g_sound_system_data->music[i] == mc) {
                g_sound_system_data->music[i] =
                    g_sound_system_data->music[--g_sound_system_data->music_count];
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
 * Resolves cached PCM pointers for sound components on the main thread so the
 * audio callback can mix using only pre-resolved EsePcm pointers without
 * calling back into the engine or asset manager.
 */
static EseSystemJobResult sound_sys_update(EseSystemManager *self, EseEngine *eng, float dt) {
    (void)self;
    (void)dt;

    EseSystemJobResult res = {0};

    if (!g_sound_system_data || !eng) {
        return res;
    }

    EseMutex *mtx = g_sound_system_data->mutex;
    if (mtx) {
        ese_mutex_lock(mtx);
    }

    for (size_t i = 0; i < g_sound_system_data->sound_count; i++) {
        EseEntityComponentSound *sound = g_sound_system_data->sounds[i];
        if (!sound) {
            continue;
        }

        // If there is no sound assigned, clear any cached PCM and playback state.
        if (!sound->sound_name) {
            sound->pcm = NULL;
            sound->frame_count = 0;
            sound->current_frame = 0;
            continue;
        }

        // Already have a cached asset, nothing to do.
        if (sound->pcm) {
            continue;
        }

        EsePcm *pcm = engine_get_sound(eng, sound->sound_name);
        sound->pcm = pcm;
        if (pcm) {
            uint32_t frames = pcm_get_frame_count(pcm);
            sound->frame_count = frames;
            if (sound->current_frame > frames) {
                sound->current_frame = frames;
            }
        } else {
            sound->frame_count = 0;
            sound->current_frame = 0;
        }
    }

    if (mtx) {
        ese_mutex_unlock(mtx);
    }

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
    EseMutex *mtx = d->mutex;
    bool had_device = false;
    if (mtx) {
        ese_mutex_lock(mtx);
        had_device = d->device_initialized;
        ese_mutex_unlock(mtx);
    } else {
        had_device = d->device_initialized;
    }

    if (had_device) {
        ma_device_stop(&d->output_device);
        ma_device_uninit(&d->output_device);

        if (mtx) {
            ese_mutex_lock(mtx);
            d->device_initialized = false;
            ese_mutex_unlock(mtx);
        } else {
            d->device_initialized = false;
        }
    }

    if (d->sounds) {
        memory_manager.free(d->sounds);
    }
    if (d->music) {
        memory_manager.free(d->music);
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

    EseMutex *mtx = g_sound_system_data->mutex;
    if (mtx) {
        ese_mutex_lock(mtx);
    }

    if (!g_sound_system_data->device_infos ||
        index >= g_sound_system_data->device_info_count) {
        if (mtx) {
            ese_mutex_unlock(mtx);
        }
        log_error("SOUND_SYSTEM", "Cannot select device: index %u out of range", index);
        return false;
    }

    bool had_previous_device = g_sound_system_data->device_initialized;

    if (mtx) {
        ese_mutex_unlock(mtx);
    }

    // Stop and uninit previous device if needed.
    if (had_previous_device) {
        ma_device_stop(&g_sound_system_data->output_device);
        ma_device_uninit(&g_sound_system_data->output_device);

        if (mtx) {
            ese_mutex_lock(mtx);
            g_sound_system_data->device_initialized = false;
            ese_mutex_unlock(mtx);
        } else {
            g_sound_system_data->device_initialized = false;
        }
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

    if (mtx) {
        ese_mutex_lock(mtx);
    }

    g_sound_system_data->output_device_id = g_sound_system_data->device_infos[index].id;
    g_sound_system_data->device_initialized = true;

    if (mtx) {
        ese_mutex_unlock(mtx);
    }

    log_debug("SOUND_SYSTEM", "Selected playback device %u: %s", index,
              g_sound_system_data->device_infos[index].name);

    return true;
}

const char *sound_system_selected_device_name(void) {
    if (!g_sound_system_data || !g_sound_system_data->ready ||
        !g_sound_system_data->device_infos) {
        return NULL;
    }

    const char *name = NULL;
    EseMutex *mtx = g_sound_system_data->mutex;
    if (mtx) {
        ese_mutex_lock(mtx);
    }

    // No device currently initialized; nothing to report.
    if (!g_sound_system_data->device_initialized) {
        if (mtx) {
            ese_mutex_unlock(mtx);
        }
        return NULL;
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

    // The sound system uses a global backing store (g_sound_system_data) which
    // can only safely be associated with one engine at a time. However, tests
    // (and some tools) may create additional transient engines in the same
    // process that do not require audio. For those cases, silently skip
    // registering another sound system instead of aborting the process.
    if (g_sound_system_data != NULL) {
        log_error("SOUND_SYSTEM", "Only one sound system permitted; skipping registration");
        return;
    }

    engine_add_system(eng, system_manager_create(&SoundSystemVTable, SYS_PHASE_EARLY, NULL));

    // Initialize Lua bindings for the Sound global once the system is registered
    sound_system_lua_init(eng->lua_engine);
}
