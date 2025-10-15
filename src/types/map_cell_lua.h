#ifndef ESE_MAP_CELL_LUA_H
#define ESE_MAP_CELL_LUA_H

// Forward declarations
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Internal Lua initialization function for EseMapCell
 * 
 * @details This function is called by the public ese_map_cell_lua_init function
 *          to set up all the private Lua metamethods and methods for EseMapCell.
 * 
 * @param engine EseLuaEngine pointer where the EseMapCell type will be registered
 */
void _ese_map_cell_lua_init(EseLuaEngine *engine);

#endif // ESE_MAP_CELL_LUA_H
