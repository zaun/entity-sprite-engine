#ifndef ESE_POINT_LUA_H
#define ESE_POINT_LUA_H

// Forward declarations
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Internal Lua initialization function for EsePoint
 * 
 * @details This function is called by the public ese_point_lua_init function
 *          to set up all the private Lua metamethods and methods for EsePoint.
 * 
 * @param engine EseLuaEngine pointer where the EsePoint type will be registered
 */
void _ese_point_lua_init(EseLuaEngine *engine);

#endif // ESE_POINT_LUA_H
