#ifndef ENTITY_COMPONENT_LUA_PRIVATE_H
#define ENTITY_COMPONENT_LUA_PRIVATE_H

#include <string.h>
#include "entity/components/entity_component_private.h" // EseEntityComponent

#define LUA_PROXY_META "EntityComponentLuaProxyMeta"

// Forward declarations
typedef struct EseEntity EseEntity;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseLuaValue EseLuaValue;
typedef struct lua_State lua_State;

/**
 * @brief Component that provides Lua scripting capabilities to an entity.
 * 
 * @details This component manages Lua script execution, instance data, and
 *          property storage. It stores the script filename, engine reference,
 *          instance registry reference, argument values, and dynamic properties.
 *          The engine reference is not owned and should not be freed.
 */
typedef struct EseEntityComponentLua {
    EseEntityComponent base;        /**< Base component structure */

    char *script;                   /**< Filename of the Lua script to execute */
    EseLuaEngine *engine;           /**< Reference to Lua engine (not owned) */
    int instance_ref;               /**< Lua registry reference to instance table */

    EseLuaValue *arg;               /**< Argument value passed to script functions */
    EseLuaValue **props;            /**< Array of dynamic properties */
    size_t props_count;             /**< Number of properties in the array */
} EseEntityComponentLua;

EseEntityComponent *_entity_component_lua_copy(const EseEntityComponentLua *src);

void _entity_component_lua_destroy(EseEntityComponentLua *component);

void _entity_component_lua_update(EseEntityComponentLua *component, EseEntity *entity, float delta_time);

EseEntityComponentLua *_entity_component_lua_get(lua_State *L, int idx);

void _entity_component_lua_init(EseLuaEngine *engine);

EseEntityComponent *entity_component_lua_create(EseLuaEngine *engine, const char *script);

#endif // ENTITY_COMPONENT_LUA_PRIVATE_H