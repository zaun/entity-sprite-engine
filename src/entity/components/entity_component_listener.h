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
 *          It stores volume, spatialization flag, and maximum distance
 *          for audible sounds.
 */
typedef struct EseEntityComponentListener {
    EseEntityComponent base; /** Base component structure */

    float volume;       /** Listener volume in range [0, 100] */
    bool spatial;       /** Whether listener uses spatialized audio */
    float max_distance; /** Maximum audible distance for spatial sounds */
} EseEntityComponentListener;

EseEntityComponent *_entity_component_listener_copy(const EseEntityComponentListener *src);

void _entity_component_listener_destroy(EseEntityComponentListener *component);

EseEntityComponentListener *_entity_component_listener_get(lua_State *L, int idx);

void _entity_component_listener_init(EseLuaEngine *engine);

EseEntityComponent *entity_component_listener_create(EseLuaEngine *engine);

#endif // ESE_ENTITY_COMPONENT_LISTENER_H