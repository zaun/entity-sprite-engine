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
#include "entity/systems/sound_system.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "core/system_manager.h"
#include "core/system_manager_private.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component_sound.h"
#include "entity/entity_private.h"
#include "utility/log.h"

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
    EseEntityComponentSound **sounds; /** Array of sound component pointers */
    size_t count;                     /** Current number of tracked sounds */
    size_t capacity;                  /** Allocated capacity of the array */
} SoundSystemData;

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
    return comp->type == ENTITY_COMPONENT_SOUND;
}

/**
 * @brief Called when a sound component is added to an entity.
 */
static void sound_sys_on_add(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp) {
    (void)eng;
    SoundSystemData *d = (SoundSystemData *)self->data;

    if (!comp || !comp->data) {
        return;
    }

    if (d->count == d->capacity) {
        d->capacity = d->capacity ? d->capacity * 2 : 64;
        d->sounds = memory_manager.realloc(d->sounds, sizeof(EseEntityComponentSound *) * d->capacity,
                                           MMTAG_S_SPRITE); // reuse sprite system tag for now
    }

    d->sounds[d->count++] = (EseEntityComponentSound *)comp->data;
}

/**
 * @brief Called when a sound component is removed from an entity.
 */
static void sound_sys_on_remove(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp) {
    (void)eng;
    SoundSystemData *d = (SoundSystemData *)self->data;

    if (!comp || !comp->data || d->count == 0) {
        return;
    }

    EseEntityComponentSound *sc = (EseEntityComponentSound *)comp->data;

    for (size_t i = 0; i < d->count; i++) {
        if (d->sounds[i] == sc) {
            d->sounds[i] = d->sounds[--d->count];
            return;
        }
    }
}

/**
 * @brief Initialize the sound system.
 */
static void sound_sys_init(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    SoundSystemData *d = memory_manager.calloc(1, sizeof(SoundSystemData), MMTAG_S_SPRITE);
    self->data = d;
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
        if (d->sounds) {
            memory_manager.free(d->sounds);
        }
        memory_manager.free(d);
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

EseSystemManager *sound_system_create(void) {
    return system_manager_create(&SoundSystemVTable, SYS_PHASE_EARLY, NULL);
}

void engine_register_sound_system(EseEngine *eng) {
    log_assert("SOUND_SYS", eng, "engine_register_sound_system called with NULL engine");

    EseSystemManager *sys = sound_system_create();
    engine_add_system(eng, sys);
}
