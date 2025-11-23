#ifndef ESE_ENTITY_COMPONENT_LISTENER_H
#define ESE_ENTITY_COMPONENT_LISTENER_H

#include "entity/components/entity_component_private.h" // EseEntityComponent
#include "vendor/lua/src/lua.h"
#include <stdbool.h>

#define ENTITY_COMPONENT_LISTENER_PROXY_META "EntityComponentListenerProxyMeta"

// Forward declarations
typedef struct EseEntity EseEntity;
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Component that represents an audio listener in the scene.
 *
 * @details This component is used by the sound system to determine how
 *          sounds should be heard from a given entity's perspective.
 *          It stores volume, spatialization flags, distance attenuation,
 *          rolloff factor, and maximum distance for audible sounds.
 */
typedef struct EseEntityComponentListener {
    EseEntityComponent base; /** Base component structure */

    float volume;       /** Listener volume in range [0, 100] */
    bool spatial;       /** Whether listener uses spatialized audio */
    float max_distance; /** Maximum audible distance for spatial sounds */

    /**
     * Listener distance attenuation strength in range [0, 1].
     *  - 0   : no distance-based attenuation (only panning applies).
     *  - 1   : full attenuation according to the rolloff curve.
     *  - 0-1 : blend between no attenuation and full attenuation.
     */
    float attenuation;

    /**
     * Rolloff factor that shapes the distance attenuation curve.
     *  - 1.0 produces a linear falloff.
     *  - >1.0 makes volume drop off more quickly with distance.
     *  - <1.0 (but >0) makes the drop-off more gradual.
     */
    float rolloff;
} EseEntityComponentListener;

EseEntityComponent *_entity_component_listener_copy(const EseEntityComponentListener *src);

void _entity_component_listener_destroy(EseEntityComponentListener *component);

EseEntityComponentListener *_entity_component_listener_get(lua_State *L, int idx);

void _entity_component_listener_init(EseLuaEngine *engine);

EseEntityComponent *entity_component_listener_create(EseLuaEngine *engine);

#endif // ESE_ENTITY_COMPONENT_LISTENER_H