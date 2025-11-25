#ifndef ESE_ENTITY_COMPONENT_MUSIC_H
#define ESE_ENTITY_COMPONENT_MUSIC_H

#include "entity/components/entity_component_private.h" // EseEntityComponent
#include "vendor/json/cJSON.h"
#include "vendor/lua/src/lua.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define ENTITY_COMPONENT_MUSIC_PROXY_META "EntityComponentMusicProxyMeta"

// Forward declarations
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EsePcm EsePcm;

/**
 * @brief Component that provides music playback with a playlist.
 *
 * @details This component stores a list of music asset identifiers and
 *          simple playback state (track index, frame counters). The actual
 *          audio data is managed by the engine's audio backend.
 */
typedef struct EseEntityComponentMusic {
    EseEntityComponent base; /** Base component structure */

    char **tracks;          /** Dynamic array of music asset IDs */
    size_t track_count;     /** Number of entries in tracks */
    size_t track_capacity;  /** Allocated capacity of tracks array */

    uint32_t current_track;   /** Zero-based index of the current track in tracks */

    EsePcm *current_pcm;      /** Cached decoded PCM for the current track */
    uint32_t frame_count;     /** Total number of frames in the current track */
    uint32_t current_frame;   /** Current playback frame within the current track */

    bool playing;             /** True if the playlist is currently playing */
    bool repeat;              /** Repeat the playlist when it reaches the end */

    bool spatial;             /** Whether this music should be spatialized (default: true) */

    /**
     * Duration of crossfade between tracks, in seconds.
     *
     * Note: the initial implementation stores this value and exposes it to
     * Lua, but the mixer currently performs simple track-to-track playback
     * without overlapping crossfades. This can be extended in the future to
     * implement true crossfading.
     */
    float xfade_time;
} EseEntityComponentMusic;

EseEntityComponent *_entity_component_music_copy(const EseEntityComponentMusic *src);

void _entity_component_music_destroy(EseEntityComponentMusic *component);

cJSON *entity_component_music_serialize(const EseEntityComponentMusic *component);
EseEntityComponent *entity_component_music_deserialize(EseLuaEngine *engine,
                                                       const cJSON *data);

EseEntityComponentMusic *_entity_component_music_get(lua_State *L, int idx);

void _entity_component_music_init(EseLuaEngine *engine);

EseEntityComponent *entity_component_music_create(EseLuaEngine *engine);

#endif // ESE_ENTITY_COMPONENT_MUSIC_H
