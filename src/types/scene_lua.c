#include "types/scene_lua.h"

#include "core/engine.h"
#include "core/engine_lua.h"
#include "scripting/lua_engine.h"
#include "types/scene.h"
#include "utility/log.h"
#include "vendor/lua/src/lauxlib.h"
#include "vendor/lua/src/lua.h"
#include "vendor/lua/src/lualib.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// ========================================
// FORWARD DECLARATIONS
// ========================================

static EseScene *ese_scene_lua_get(lua_State *L, int idx);
static void ese_scene_lua_push(lua_State *L, EseScene *scene);

// Metamethods
static int _ese_scene_lua_gc(lua_State *L);
static int _ese_scene_lua_index(lua_State *L);
static int _ese_scene_lua_newindex(lua_State *L);
static int _ese_scene_lua_tostring(lua_State *L);

// Class methods
static int _ese_scene_lua_create(lua_State *L);
static int _ese_scene_lua_class_clear(lua_State *L);
static int _ese_scene_lua_class_reset(lua_State *L);

// Instance methods
static int _ese_scene_lua_run(lua_State *L);
static int _ese_scene_lua_entity_count(lua_State *L);

// ========================================
// HELPERS
// ========================================

static EseScene *ese_scene_lua_get(lua_State *L, int idx) {
    EseScene **ud = (EseScene **)luaL_testudata(L, idx, SCENE_PROXY_META);
    if (!ud) {
        return NULL;
    }
    return *ud;
}

static void ese_scene_lua_push(lua_State *L, EseScene *scene) {
    log_assert("SCENE", scene, "ese_scene_lua_push called with NULL scene");

    EseScene **ud = (EseScene **)lua_newuserdata(L, sizeof(EseScene *));
    *ud = scene;

    luaL_getmetatable(L, SCENE_PROXY_META);
    lua_setmetatable(L, -2);
}

// ========================================
// METAMETHODS
// ========================================

static int _ese_scene_lua_gc(lua_State *L) {
    EseScene **ud = (EseScene **)luaL_testudata(L, 1, SCENE_PROXY_META);
    if (!ud) {
        return 0;
    }

    if (*ud) {
        ese_scene_destroy(*ud);
        *ud = NULL;
    }

    return 0;
}

static int _ese_scene_lua_index(lua_State *L) {
    EseScene *scene = ese_scene_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);

    if (!scene || !key) {
        return 0;
    }

    if (strcmp(key, "run") == 0) {
        lua_pushcfunction(L, _ese_scene_lua_run);
        return 1;
    } else if (strcmp(key, "entity_count") == 0) {
        lua_pushcfunction(L, _ese_scene_lua_entity_count);
        return 1;
    }

    return 0;
}

static int _ese_scene_lua_newindex(lua_State *L) {
    (void)L;
    return luaL_error(L, "Scene instances are read-only");
}

static int _ese_scene_lua_tostring(lua_State *L) {
    EseScene *scene = ese_scene_lua_get(L, 1);
    if (!scene) {
        lua_pushstring(L, "Scene(invalid)");
        return 1;
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "Scene(entity_count=%zu)", ese_scene_entity_count(scene));
    lua_pushstring(L, buf);
    return 1;
}

// ========================================
// CLASS METHODS
// ========================================

static int _ese_scene_lua_create(lua_State *L) {
    int argc = lua_gettop(L);
    bool include_persistent = false;

    if (argc > 1) {
        return luaL_error(L, "Scene.create([include_persistent:boolean]) takes at most 1 argument");
    }

    if (argc == 1) {
        if (!lua_isboolean(L, 1)) {
            return luaL_error(L, "Scene.create argument must be a boolean");
        }
        include_persistent = lua_toboolean(L, 1) != 0;
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    if (!engine) {
        return luaL_error(L, "Scene.create: no engine available");
    }

    EseScene *scene = ese_scene_create_from_engine(engine, include_persistent);
    if (!scene) {
        return luaL_error(L, "Scene.create: failed to create scene from engine");
    }

    ese_scene_lua_push(L, scene);
    return 1;
}

static int _ese_scene_lua_class_clear(lua_State *L) {
    int argc = lua_gettop(L);
    if (argc != 0) {
        return luaL_error(L, "Scene.clear() takes 0 arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    if (!engine) {
        return luaL_error(L, "Scene.clear: no engine available");
    }

    engine_clear_entities(engine, false);
    lua_pushboolean(L, 1);
    return 1;
}

static int _ese_scene_lua_class_reset(lua_State *L) {
    int argc = lua_gettop(L);
    if (argc != 0) {
        return luaL_error(L, "Scene.reset() takes 0 arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    if (!engine) {
        return luaL_error(L, "Scene.reset: no engine available");
    }

    engine_clear_entities(engine, true);
    lua_pushboolean(L, 1);
    return 1;
}

// ========================================
// INSTANCE METHODS
// ========================================

static int _ese_scene_lua_run(lua_State *L) {
    int argc = lua_gettop(L);
    if (argc != 1) {
        return luaL_error(L, "scene:run() takes 0 arguments");
    }

    EseScene *scene = ese_scene_lua_get(L, 1);
    if (!scene) {
        return luaL_error(L, "scene:run() called on invalid Scene");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    if (!engine) {
        return luaL_error(L, "scene:run(): no engine available");
    }

    ese_scene_run(scene, engine);
    return 0;
}

static int _ese_scene_lua_entity_count(lua_State *L) {
    int argc = lua_gettop(L);
    if (argc != 1) {
        return luaL_error(L, "scene:entity_count() takes 0 arguments");
    }

    EseScene *scene = ese_scene_lua_get(L, 1);
    if (!scene) {
        return luaL_error(L, "scene:entity_count() called on invalid Scene");
    }

    lua_pushinteger(L, (lua_Integer)ese_scene_entity_count(scene));
    return 1;
}

// ========================================
// PUBLIC INIT
// ========================================

void _ese_scene_lua_init(EseLuaEngine *engine) {
    log_assert("SCENE", engine, "_ese_scene_lua_init called with NULL engine");

    // Create metatable for Scene userdata
    lua_engine_new_object_meta(engine, SCENE_PROXY_META, _ese_scene_lua_index, _ese_scene_lua_newindex,
                               _ese_scene_lua_gc, _ese_scene_lua_tostring);

    // Create global Scene table with class methods
    const char *keys[] = {"create", "clear", "reset"};
    lua_CFunction functions[] = {_ese_scene_lua_create, _ese_scene_lua_class_clear,
                                 _ese_scene_lua_class_reset};
    lua_engine_new_object(engine, "Scene", 3, keys, functions);
}
