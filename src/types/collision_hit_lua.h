#ifndef ESE_COLLISION_HIT_LUA_H
#define ESE_COLLISION_HIT_LUA_H

// Forward declarations
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Internal Lua initialization function for EseCollisionHit
 * 
 * @details This function is called by the public ese_collision_hit_lua_init function
 *          to set up all the private Lua metamethods and methods for EseCollisionHit.
 * 
 * @param engine EseLuaEngine pointer where the EseCollisionHit type will be registered
 */
void _ese_collision_hit_lua_init(EseLuaEngine *engine);

#endif // ESE_COLLISION_HIT_LUA_H
