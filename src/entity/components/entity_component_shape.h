#ifndef ESE_ENTITY_COMPONENT_SHAPE_H
#define ESE_ENTITY_COMPONENT_SHAPE_H

#include <string.h>
#include "entity/components/entity_component_private.h" // EseEntityComponent
#include "types/poly_line.h" // EsePolyLine

#define ENTITY_COMPONENT_SHAPE_PROXY_META "EntityComponentShapeProxyMeta"

// Forward declarations
typedef struct EseEntity EseEntity;
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Component that provides shape rendering capabilities to an entity.
 * 
 * @details This component manages shape display with a configurable polyline property.
 *          It stores the shape polyline for rendering. The draw function is
 *          left blank as requested.
 */
typedef struct EseEntityComponentShape {
    EseEntityComponent base;        /** Base component structure */
    EsePolyLine *polyline;          /** Polyline for the shape */
    float rotation;                 /** Rotation of the shape */
} EseEntityComponentShape;

EseEntityComponent *_entity_component_shape_copy(const EseEntityComponentShape *src);

void _entity_component_shape_destroy(EseEntityComponentShape *component);

EseEntityComponentShape *_entity_component_shape_get(lua_State *L, int idx);

void _entity_component_shape_init(EseLuaEngine *engine);

void _entity_component_shape_draw(EseEntityComponentShape *component, float screen_x, float screen_y, EntityDrawCallbacks *callbacks, void *callback_user_data);

EseEntityComponent *entity_component_shape_create(EseLuaEngine *engine);

#endif // ESE_ENTITY_COMPONENT_SHAPE_H
