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
    ENTITY_COMPONENT_SHAPE,
    ENTITY_COMPONENT_SPRITE,
    ENTITY_COMPONENT_TEXT,
} EntityComponentType;

/**
 * @brief Virtual function table for component operations.
 * 
 * @details This structure contains function pointers for all component operations,
 *          allowing polymorphic behavior without large switch statements.
 */
typedef struct ComponentVTable {
    EseEntityComponent* (*copy)(EseEntityComponent* component);
    void (*destroy)(EseEntityComponent* component);
    void (*update)(EseEntityComponent* component, EseEntity* entity, float delta_time);
    void (*draw)(EseEntityComponent* component, int screen_x, int screen_y, void* callbacks, void* user_data);
    bool (*run_function)(EseEntityComponent* component, EseEntity* entity, const char* func_name, int argc, void* argv[]);
    void (*ref)(EseEntityComponent* component);
    void (*unref)(EseEntityComponent* component);
} ComponentVTable;

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
    const ComponentVTable *vtable;  /**< Virtual function table for polymorphic operations */

    EseEntity *entity;              /**< EseEntity this component belongs to */
    EseLuaEngine *lua;              /**< EseLuaEngine this component belongs to */
    int lua_ref;                    /**< Lua registry reference to its own userdata */
    int lua_ref_count;              /**< Reference count for Lua userdata */
} EseEntityComponent;


#endif // ESE_ENTITY_COMPONENTS_PRIVATE_H
