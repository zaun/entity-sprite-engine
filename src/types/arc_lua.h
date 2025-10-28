#ifndef ESE_ARC_LUA_H
#define ESE_ARC_LUA_H

// Forward declarations
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Internal Lua initialization function for EseArc
 *
 * @details This function is called by the public ese_arc_lua_init function
 *          to set up all the private Lua metamethods and methods for EseArc.
 *
 * @param engine EseLuaEngine pointer where the EseArc type will be registered
 */
void _ese_arc_lua_init(EseLuaEngine *engine);

#endif // ESE_ARC_LUA_H
