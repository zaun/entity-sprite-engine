#ifndef ESE_ENTITY_COMPONENT_SOUND_H
#define ESE_ENTITY_COMPONENT_SOUND_H

#include "entity/components/entity_component_private.h" // EseEntityComponent
#include "vendor/lua/src/lua.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define ENTITY_COMPONENT_SOUND_PROXY_META "EntityComponentSoundProxyMeta"

// Forward declarations
typedef struct EseEntity EseEntity;
typedef struct EseLuaEngine EseLuaEngine;
/**
 * @brief Component that provides sound playback capabilities to an entity.
 *
 * @details This component stores a sound asset identifier and simple playback
 *          state (frame counters). The actual audio data is managed by the
 *          engine's audio backend.
 */
typedef struct EseEntityComponentSound {
    EseEntityComponent base; /** Base component structure */

    char *sound_name;       /** Name/ID of the sound to play */
    uint32_t frame_count;   /** Total number of audio frames (read-only to Lua) */
    uint32_t current_frame; /** Current playback frame (read-only to Lua) */
    bool playing;           /** True if the sound is currently playing */
    bool repeat;            /** True if the sound should repeat when it reaches the end */
} EseEntityComponentSound;

EseEntityComponent *_entity_component_sound_copy(const EseEntityComponentSound *src);

void _entity_component_sound_destroy(EseEntityComponentSound *component);

EseEntityComponentSound *_entity_component_sound_get(lua_State *L, int idx);

void _entity_component_sound_init(EseLuaEngine *engine);

EseEntityComponent *entity_component_sound_create(EseLuaEngine *engine, const char *sound_name);

#endif // ESE_ENTITY_COMPONENT_SOUND_H
