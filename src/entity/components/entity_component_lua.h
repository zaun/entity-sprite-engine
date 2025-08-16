#ifndef ENTITY_COMPONENT_LUA_PRIVATE_H
#define ENTITY_COMPONENT_LUA_PRIVATE_H

#include <string.h>
#include "entity/components/entity_component_private.h" // EseEntityComponent

// Forward declarations
typedef struct EseEntity EseEntity;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseLuaValue EseLuaValue;
typedef struct lua_State lua_State;

typedef struct EseEntityComponentLua {
    EseEntityComponent base;

    char *script;
    EseLuaEngine *engine; // we dont own this
    int instance_ref;

    EseLuaValue *arg;
    EseLuaValue **props;
    size_t props_count;
} EseEntityComponentLua;

EseEntityComponent *_entity_component_lua_copy(const EseEntityComponentLua *src);

void _entity_component_lua_destroy(EseEntityComponentLua *component);

void _entity_component_lua_update(EseEntityComponentLua *component, EseEntity *entity, float delta_time);

EseEntityComponentLua *_entity_component_lua_get(lua_State *L, int idx);

void _entity_component_lua_init(EseLuaEngine *engine);

EseEntityComponent *entity_component_lua_create(EseLuaEngine *engine, const char *script);

#endif // ENTITY_COMPONENT_LUA_PRIVATE_H