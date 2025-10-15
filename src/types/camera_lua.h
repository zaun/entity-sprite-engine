#ifndef ESE_CAMERA_LUA_H
#define ESE_CAMERA_LUA_H

// Forward declarations
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Internal Lua initialization function for EseCamera
 * 
 * @details This function is called by the public ese_camera_lua_init function
 *          to set up all the private Lua metamethods and methods for EseCamera.
 * 
 * @param engine EseLuaEngine pointer where the EseCamera type will be registered
 */
void _ese_camera_lua_init(EseLuaEngine *engine);

#endif // ESE_CAMERA_LUA_H
