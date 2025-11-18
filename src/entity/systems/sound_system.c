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

SoundSystemData *g_sound_system_data = NULL;

// ========================================
// PRIVATE FUNCTIONS
// ========================================

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

    if (!comp || !comp->data) {
        return;
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
}

/**
 * @brief Called when a sound component is removed from an entity.
 */
static void sound_sys_on_remove(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp) {
    (void)eng;

    if (!comp || !comp->data) {
        return;
    }

    if (comp->type == ENTITY_COMPONENT_SOUND) {
        if (g_sound_system_data->sound_count == 0) {
            return;
        }

        EseEntityComponentSound *sc = (EseEntityComponentSound *)comp->data;

        for (size_t i = 0; i < g_sound_system_data->sound_count; i++) {
            if (g_sound_system_data->sounds[i] == sc) {
                g_sound_system_data->sounds[i] = g_sound_system_data->sounds[--g_sound_system_data->sound_count];
                return;
            }
        }
    } else if (comp->type == ENTITY_COMPONENT_LISTENER) {
        if (g_sound_system_data->listener_count == 0) {
            return;
        }

        EseEntityComponentListener *lc = (EseEntityComponentListener *)comp->data;

        for (size_t i = 0; i < g_sound_system_data->listener_count; i++) {
            if (g_sound_system_data->listeners[i] == lc) {
                g_sound_system_data->listeners[i] = g_sound_system_data->listeners[--g_sound_system_data->listener_count];
                return;
            }
        }
    }
}

/**
 * @brief Initialize the sound system.
 */
static void sound_sys_init(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    g_sound_system_data = memory_manager.calloc(1, sizeof(SoundSystemData), MMTAG_S_SPRITE);

    // Initilize audio
    ma_result result = ma_context_init(NULL, 0, NULL, &g_sound_system_data->context);
    if (result != MA_SUCCESS) {
        log_error("SOUND_SYSTEM", "Failed to initilize audio: %s", ma_result_description(result));
        g_sound_system_data->ready = false;
        return;
    }

    // Get devices
    result = ma_context_get_devices(&g_sound_system_data->context, &g_sound_system_data->device_infos, &g_sound_system_data->device_info_count, NULL, 0);
    if (result != MA_SUCCESS) {
        log_error("SOUND_SYSTEM", "Failed to get devices: %s", ma_result_description(result));
        ma_context_uninit(&g_sound_system_data->context);

        g_sound_system_data->ready = false;
        g_sound_system_data->device_info_count = 0;
        g_sound_system_data->device_infos = NULL;
        return;
    }
    g_sound_system_data->ready = true;

    // log devices
    log_verbose("SOUND_SYSTEM", "Playback devices:");
    for (ma_uint32 i = 0; i < g_sound_system_data->device_info_count; i++) {
        log_verbose("SOUND_SYSTEM", "  %s %u: %s",
            g_sound_system_data->device_infos[i].isDefault ? "**" : "  ",
            i, g_sound_system_data->device_infos[i].name,
            g_sound_system_data->device_infos[i].id);
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
    if (d) {
        if (g_sound_system_data->sounds) {
            memory_manager.free(g_sound_system_data->sounds);
        }
        if (g_sound_system_data->listeners) {
            memory_manager.free(g_sound_system_data->listeners);
        }

        if (g_sound_system_data->ready) {
            ma_context_uninit(&g_sound_system_data->context);
        }

        memory_manager.free(d);
        if (g_sound_system_data == d) {
            g_sound_system_data = NULL;
        }
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
// PUBLIC FUNCTIONS
// ========================================

void engine_register_sound_system(EseEngine *eng) {
    log_assert("SOUND_SYS", eng, "engine_register_sound_system called with NULL engine");
    log_assert("SOUND_SYSTEM", g_sound_system_data == NULL, "Only one sound system permitted");

    engine_add_system(eng, system_manager_create(&SoundSystemVTable, SYS_PHASE_EARLY, NULL));

    // Initialize Lua bindings for the Sound global once the system is registered
    sound_system_lua_init(eng->lua_engine);
}
