#include <stdbool.h>
#include "utility/log.h"
#include "vendor/lua/src/lua.h"
#include "vendor/lua/src/lauxlib.h"
#include "vendor/lua/src/lualib.h"
#include "core/engine_private.h"
#include "core/engine.h"
#include "platform/renderer.h"

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
        if (n1 < 0) n1 = 0;
        offset += (size_t)n1 < sizeof(buffer) - offset ? (size_t)n1 : sizeof(buffer) - offset - 1;
        int n2 = snprintf(buffer + offset, sizeof(buffer) - offset, "%s", s);
        if (n2 < 0) n2 = 0;
        offset += (size_t)n2 < sizeof(buffer) - offset ? (size_t)n2 : sizeof(buffer) - offset - 1;
        lua_pop(L, 1);
        if (offset >= sizeof(buffer) - 1) break;
    }
    buffer[sizeof(buffer) - 1] = '\0';
    log_debug("LUA_SCRIPT", "%s", buffer);
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
    bool status = lua_engine_load_script(engine, script);

    log_debug("ENGINE", "Loading script %s has %s.", script, status ? "completed" : "failed");
    
    lua_pushboolean(L, status);
    return 1;
}

int _lua_asset_load_atlas(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 2) {
        log_warn("ENGINE", "asset_load_atlast(String group, String stlas) takes 2 string arguments");
        lua_pushboolean(L, false);
        return 1;
    }
    
    if (!lua_isstring(L, 1) || !lua_isstring(L, 2)) {
        log_warn("ENGINE", "asset_load_atlast(String group, String stlas) takes 2 string arguments");
        lua_pushboolean(L, false);
        return 1;
    }
    
    const char *group = lua_tostring(L, 1);
    const char *atlas = lua_tostring(L, 2);

    // Get engine reference
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    bool status = asset_manager_load_sprite_atlas(engine->asset_manager, atlas, group);

    log_debug("ENGINE", "Loading atlas %s has %s.", atlas, status ? "completed" : "failed");
    
    lua_pushboolean(L, status);
    return 1;
}

int _lua_asset_load_shader(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 2) {
        log_warn("ENGINE", "asset_load_shader(String group, String shader_filename) takes 2 string arguments");
        lua_pushboolean(L, false);
        return 1;
    }
    
    if (!lua_isstring(L, 1) || !lua_isstring(L, 2)) {
        log_warn("ENGINE", "asset_load_shader(String group, String shader_filename) takes 2 string arguments");
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

int _lua_set_pipeline(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 2) {
        log_warn("ENGINE", "_lua_set_pipeline(String vertexShader, String fragmentShader) takes 2 string arguments");
        lua_pushboolean(L, false);
        return 1;
    }
    
    if (!lua_isstring(L, 1) || !lua_isstring(L, 2)) {
        log_warn("ENGINE", "_lua_set_pipeline(String vertexShader, String fragmentShader) takes 2 string arguments");
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
