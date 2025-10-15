#ifndef ESE_COLOR_LUA_H
#define ESE_COLOR_LUA_H

// Forward declarations
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Internal Lua initialization function for EseColor
 * 
 * @details This function is called by the public ese_color_lua_init function
 *          to set up all the private Lua metamethods and methods for EseColor.
 * 
 * @param engine EseLuaEngine pointer where the EseColor type will be registered
 */
void _ese_color_lua_init(EseLuaEngine *engine);

#endif // ESE_COLOR_LUA_H
