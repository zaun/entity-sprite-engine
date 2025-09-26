#ifndef ESE_ENTITY_LUA_H
#define ESE_ENTITY_LUA_H

#include "scripting/lua_engine_private.h"

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseEntity EseEntity;

/**
 * @brief Initializes the EseEntity userdata type in the Lua state.
 * 
 * @param engine EseLuaEngine pointer where the EseEntity type will be registered
 */
void entity_lua_init(EseLuaEngine *engine);

/**
 * @brief Increment the Lua registry ref-count for an entity and create userdata if needed.
 */
void entity_ref(EseEntity *entity);

void entity_lua_push(EseEntity *entity);

/**
 * @brief Extracts an EseEntity pointer from a Lua userdata object with type safety.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseEntity object
 * @return Pointer to the EseEntity object, or NULL if extraction fails
 */
EseEntity *entity_lua_get(lua_State *L, int idx);

bool _entity_lua_to_data(EseEntity *entity, EseLuaValue *value);

#endif // ESE_ENTITY_LUA_H
