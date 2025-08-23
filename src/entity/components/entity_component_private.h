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

/**
 * @brief Base structure for all entity components in the ECS system.
 * 
 * @details This structure serves as the foundation for all component types.
 *          Each component has a unique ID, active state, type classification,
 *          and optional data pointer. Components are integrated with Lua
 *          via proxy tables for scripting access.
 */
typedef struct EseEntityComponent {
    EseUUID *id;                    /**< Unique component identifier */
    bool active;                    /**< Whether component is active and should be processed */
    EntityComponentType type;       /**< Type classification for component processing */
    void *data;                     /**< Component-specific data (cast to specific type) */

    EseEntity *entity;              /**< EseEntity this component belongs to */
    EseLuaEngine *lua;             /**< EseLuaEngine this component belongs to */
    int lua_ref;                    /**< Lua registry reference to its own proxy table */
} EseEntityComponent;


#endif // ESE_ENTITY_COMPONENTS_PRIVATE_H
