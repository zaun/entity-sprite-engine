#ifndef ESE_LUA_SYSTEM_H
#define ESE_LUA_SYSTEM_H

typedef struct EseEngine EseEngine;
typedef struct EseSystemManager EseSystemManager;

/**
 * @brief Creates and returns a new Lua System.
 *
 * @details The Lua system updates all active Lua components during the
 *          SYS_PHASE_LUA phase, executing script init on first run and update
 *          each frame with delta time.
 */
EseSystemManager *lua_system_create(void);

/**
 * @brief Registers the Lua system with the engine.
 *
 * @param eng Pointer to the engine.
 */
void engine_register_lua_system(EseEngine *eng);

#endif /* ESE_LUA_SYSTEM_H */
