#ifndef ESE_SCENE_LUA_H
#define ESE_SCENE_LUA_H

typedef struct EseLuaEngine EseLuaEngine;

// Internal Lua initialization for Scene; called by ese_scene_lua_init.
void _ese_scene_lua_init(EseLuaEngine *engine);

#endif // ESE_SCENE_LUA_H
