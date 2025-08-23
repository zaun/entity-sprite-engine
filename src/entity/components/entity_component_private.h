#ifndef ESE_ENTITY_COMPONENTS_PRIVATE_H
#define ESE_ENTITY_COMPONENTS_PRIVATE_H

#include <stdbool.h>
#include "entity/entity.h"

// Forward declarations
typedef struct EseEntity EseEntity;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseUUID EseUUID;

typedef enum {
    ENTITY_COMPONENT_COLLIDER,
    ENTITY_COMPONENT_LUA,
    ENTITY_COMPONENT_MAP,
    ENTITY_COMPONENT_SPRITE,
} EntityComponentType;

typedef struct EseEntityComponent {
    EseUUID *id;
    bool active;
    EntityComponentType type;
    void *data;

    EseEntity *entity;  /**< EseEntity this component belongs to */
    EseLuaEngine *lua; /**< EseLuaEngine this component belongs to */
    int lua_ref;
} EseEntityComponent;


#endif // ESE_ENTITY_COMPONENTS_PRIVATE_H
