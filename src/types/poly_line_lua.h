#ifndef ESE_POLY_LINE_LUA_H
#define ESE_POLY_LINE_LUA_H

// Forward declarations
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Internal Lua initialization function for EsePolyLine
 * 
 * @details This function is called by the public ese_poly_line_lua_init function
 *          to set up all the private Lua metamethods and methods for EsePolyLine.
 * 
 * @param engine EseLuaEngine pointer where the EsePolyLine type will be registered
 */
void _ese_poly_line_lua_init(EseLuaEngine *engine);

#endif // ESE_POLY_LINE_LUA_H
