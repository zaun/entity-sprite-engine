#ifndef ESE_MAP_LUA_H
#define ESE_MAP_LUA_H

// Forward declarations
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Internal Lua initialization function for EseMap
 *
 * @details This function is called by the public ese_map_lua_init function
 *          to set up all the private Lua metamethods and methods for EseMap.
 *
 * @param engine EseLuaEngine pointer where the EseMap type will be registered
 */
void _ese_map_lua_init(EseLuaEngine *engine);

#endif // ESE_MAP_LUA_H
