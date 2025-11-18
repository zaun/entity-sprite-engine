#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "platform/renderer.h"
#include "types/types.h"
#include "utility/log.h"
#include "vendor/lua/src/lauxlib.h"
#include "vendor/lua/src/lua.h"
#include "vendor/lua/src/lualib.h"
#include <stdbool.h>
#include <string.h>

int _lua_print(lua_State *L) {
    log_assert("LUA", L, "_lua_print called with NULL L");

    int n = lua_gettop(L);
    lua_getglobal(L, "tostring");
    char buffer[1024];
    size_t offset = 0;
    buffer[0] = '\0';

    for (int i = 1; i <= n; i++) {
        lua_pushvalue(L, -1);
        lua_pushvalue(L, i);
        if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
            const char *err = lua_tostring(L, -1);
            log_error("LUA", "print tostring error: %s", err ? err : "(unknown)");
            lua_pop(L, 1);
            continue;
        }
        const char *s = lua_tostring(L, -1);
        if (!s) {
            lua_pop(L, 1);
            continue;
        }
        const char *sep = (i > 1) ? "\t" : "";
        int n1 = snprintf(buffer + offset, sizeof(buffer) - offset, "%s", sep);
        if (n1 < 0)
            n1 = 0;
        offset += (size_t)n1 < sizeof(buffer) - offset ? (size_t)n1 : sizeof(buffer) - offset - 1;
        int n2 = snprintf(buffer + offset, sizeof(buffer) - offset, "%s", s);
        if (n2 < 0)
            n2 = 0;
        offset += (size_t)n2 < sizeof(buffer) - offset ? (size_t)n2 : sizeof(buffer) - offset - 1;
        lua_pop(L, 1);
        if (offset >= sizeof(buffer) - 1)
            break;
    }
    buffer[sizeof(buffer) - 1] = '\0';
    log_debug("LUA_SCRIPT", "%s", buffer);

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    engine_add_to_console(engine, ESE_CONSOLE_NORMAL, "LUA", buffer);
    return 0;
}

int _lua_asset_load_script(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 1) {
        log_warn("ENGINE", "asset_load_script(String script) takes 1 string argument");
        lua_pushboolean(L, false);
        return 1;
    }

    if (!lua_isstring(L, 1)) {
        log_warn("ENGINE", "asset_load_script(String script) takes 1 string argument");
        lua_pushboolean(L, false);
        return 1;
    }

    const char *script = lua_tostring(L, 1);

    // Get engine reference
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    bool status = lua_engine_load_script(engine, script, "ENTITY");

    log_debug("ENGINE", "Loading script %s has %s.", script, status ? "completed" : "failed");

    lua_pushboolean(L, status);
    return 1;
}

int _lua_asset_load_atlas(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args < 2 || n_args > 3) {
        log_warn("ENGINE", "asset_load_atlas(String group, String stlas, [Boolean "
                           "indexed]) takes 2 arguments and 1 optional argument");
        lua_pushboolean(L, false);
        return 1;
    }

    if (!lua_isstring(L, 1) || !lua_isstring(L, 2)) {
        log_warn("ENGINE", "asset_load_atlas(String group, String stlas, [Boolean "
                           "indexed]) takes 2 arguments and 1 optional argument");
        lua_pushboolean(L, false);
        return 1;
    }

    if (n_args == 3 && !lua_isboolean(L, 3)) {
        log_warn("ENGINE", "asset_load_atlas(String group, String stlas, [Boolean "
                           "indexed]) takes 2 arguments and 1 optional argument");
        lua_pushboolean(L, false);
        return 1;
    }

    const char *group = lua_tostring(L, 1);
    const char *atlas = lua_tostring(L, 2);
    bool indexed = n_args == 3 ? lua_toboolean(L, 3) : false;

    // Get engine reference
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    bool status = asset_manager_load_sprite_atlas(engine->asset_manager, atlas, group, indexed);

    log_debug("ENGINE", "Loading atlas %s has %s.", atlas, status ? "completed" : "failed");

    lua_pushboolean(L, status);
    return 1;
}

int _lua_asset_load_sound(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 3) {
        log_warn("ENGINE", "asset_load_sound(String group, String id, String filename) takes 3 string arguments");
        lua_pushboolean(L, false);
        return 1;
    }

    if (!lua_isstring(L, 1) || !lua_isstring(L, 2) || !lua_isstring(L, 3)) {
        log_warn("ENGINE", "asset_load_sound(String group, String id, String filename) takes 3 string arguments");
        lua_pushboolean(L, false);
        return 1;
    }

    const char *group = lua_tostring(L, 1);
    const char *id = lua_tostring(L, 2);
    const char *filename = lua_tostring(L, 3);

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    bool status = asset_manager_load_sound(engine->asset_manager, filename, id, group);

    log_debug("ENGINE", "Loading sound %s (group=%s, id=%s) has %s.", filename, group, id,
              status ? "completed" : "failed");

    lua_pushboolean(L, status);
    return 1;
}

int _lua_asset_load_music(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 3) {
        log_warn("ENGINE", "asset_load_music(String group, String id, String filename) takes 3 string arguments");
        lua_pushboolean(L, false);
        return 1;
    }

    if (!lua_isstring(L, 1) || !lua_isstring(L, 2) || !lua_isstring(L, 3)) {
        log_warn("ENGINE", "asset_load_music(String group, String id, String filename) takes 3 string arguments");
        lua_pushboolean(L, false);
        return 1;
    }

    const char *group = lua_tostring(L, 1);
    const char *id = lua_tostring(L, 2);
    const char *filename = lua_tostring(L, 3);

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    bool status = asset_manager_load_music(engine->asset_manager, filename, id, group);

    log_debug("ENGINE", "Loading music %s (group=%s, id=%s) has %s.", filename, group, id,
              status ? "completed" : "failed");

    lua_pushboolean(L, status);
    return 1;
}

int _lua_asset_load_shader(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 2) {
        log_warn("ENGINE", "asset_load_shader(String group, String "
                           "shader_filename) takes 2 string arguments");
        lua_pushboolean(L, false);
        return 1;
    }

    if (!lua_isstring(L, 1) || !lua_isstring(L, 2)) {
        log_warn("ENGINE", "asset_load_shader(String group, String "
                           "shader_filename) takes 2 string arguments");
        lua_pushboolean(L, false);
        return 1;
    }

    const char *group_name = lua_tostring(L, 1);
    const char *file_name = lua_tostring(L, 2);

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    bool status = renderer_shader_compile(engine->renderer, group_name, file_name);

    lua_pushboolean(L, status);
    return 1;
}

int _lua_asset_load_map(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 2) {
        log_warn("ENGINE", "asset_load_map(String group, String map) takes 2 "
                           "string arguments");
        lua_pushboolean(L, false);
        return 1;
    }

    if (!lua_isstring(L, 1) || !lua_isstring(L, 2)) {
        log_warn("ENGINE", "asset_load_map(String group, String map) takes 2 "
                           "string arguments");
        lua_pushboolean(L, false);
        return 1;
    }

    const char *group = lua_tostring(L, 1);
    const char *map = lua_tostring(L, 2);

    // Get engine reference
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    bool status = asset_manager_load_map(engine->asset_manager, engine->lua_engine, map, group);

    log_debug("ENGINE", "Loading map %s has %s.", map, status ? "completed" : "failed");

    lua_pushboolean(L, status);
    return 1;
}

int _lua_asset_get_map(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 1) {
        log_warn("ENGINE", "asset_load_map(String ese_map_id) takes 2 string arguments");
        lua_pushboolean(L, false);
        return 1;
    }

    if (!lua_isstring(L, 1)) {
        log_warn("ENGINE", "asset_load_map(String ese_map_id) takes 1 string argument");
        lua_pushboolean(L, false);
        return 1;
    }

    const char *ese_map_id = lua_tostring(L, 1);

    // Get engine reference
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseMap *found = asset_manager_get_map(engine->asset_manager, ese_map_id);

    if (found) {
        ese_map_lua_push(found);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int _lua_set_pipeline(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 2) {
        log_warn("ENGINE", "_lua_set_pipeline(String vertexShader, String "
                           "fragmentShader) takes 2 string arguments");
        lua_pushboolean(L, false);
        return 1;
    }

    if (!lua_isstring(L, 1) || !lua_isstring(L, 2)) {
        log_warn("ENGINE", "_lua_set_pipeline(String vertexShader, String "
                           "fragmentShader) takes 2 string arguments");
        lua_pushboolean(L, false);
        return 1;
    }

    const char *vertexShader = lua_tostring(L, 1);
    const char *fragmentShader = lua_tostring(L, 2);

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    bool status = renderer_create_pipeline_state(engine->renderer, vertexShader, fragmentShader);

    lua_pushboolean(L, status);
    return 1;
}

int _lua_detect_collision(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 2) {
        log_warn("ENGINE", "detect_collision(rect, number max_results) takes 2 argument");
        lua_newtable(L);
        return 1;
    }

    if (!lua_isinteger_lj(L, 2)) {
        log_warn("ENGINE", "detect_collision(rect, number max_results) 2nd "
                           "argumant to be an integer");
        lua_newtable(L);
        return 1;
    }

    // 1st argument must be a table
    if (!lua_getmetatable(L, 1)) {
        log_warn("ENGINE", "detect_collision(rect) expected rect");
        lua_newtable(L);
        return 1;
    }

    // table nust have a __name
    lua_getfield(L, -1, "__name"); // get metatable.__name
    const char *tname = lua_tostring(L, -1);
    if (!tname) {
        log_warn("ENGINE", "detect_collision(rect) expected rect");
        lua_newtable(L);
        return 1;
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    int max_results = (int)lua_tointeger(L, 2);

    // If the table is a Rect
    if (strcmp(tname, "RectProxyMeta") == 0) {
        EseRect *rect = ese_rect_lua_get(L, 1);
        EseEntity **entities = engine_detect_collision_rect(engine, rect, max_results);

        // build Lua table
        lua_newtable(L);
        int idx = 1;
        for (int i = 0; entities[i] != NULL; i++) {
            lua_pushinteger(L, idx++);
            entity_lua_push(entities[i]);
            lua_settable(L, -3);
        }

        // free the list, we dont own the entites.
        memory_manager.free(entities);
        return 1;
    }

    log_warn("ENGINE", "detect_collision(rect) expected rect");
    lua_newtable(L);
    return 1;
}

int _lua_scene_clear(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 0) {
        log_warn("ENGINE", "scene_clear() takes 0 arguments");
        lua_pushboolean(L, false);
        return 1;
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    engine_clear_entities(engine, false);

    lua_pushboolean(L, true);
    return 1;
}

int _lua_scene_reset(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 0) {
        log_warn("ENGINE", "scene_reset() takes 0 arguments");
        lua_pushboolean(L, false);
        return 1;
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    engine_clear_entities(engine, true);

    lua_pushboolean(L, true);
    return 1;
}
