#include <stdbool.h>
#include <string.h>
#include "utility/log.h"
#include "vendor/lua/src/lua.h"
#include "vendor/lua/src/lauxlib.h"
#include "vendor/lua/src/lualib.h"
#include "core/engine_private.h"
#include "core/engine.h"
#include "core/memory_manager.h"
#include "platform/renderer.h"
#include "types/types.h"
#include "scripting/lua_value.h"
#include "scripting/lua_engine.h"

EseLuaValue* _lua_print(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    log_assert("LUA", engine, "_lua_print called with NULL engine");
    
    char buffer[1024];
    size_t offset = 0;
    buffer[0] = '\0';

    for (size_t i = 0; i < argc; i++) {
        const char *sep = (i > 0) ? "\t" : "";
        int n1 = snprintf(buffer + offset, sizeof(buffer) - offset, "%s", sep);
        if (n1 < 0) n1 = 0;
        offset += (size_t)n1 < sizeof(buffer) - offset ? (size_t)n1 : sizeof(buffer) - offset - 1;
        
        // Convert EseLuaValue to string representation
        char value_str[256];
        if (lua_value_is_string(argv[i])) {
            snprintf(value_str, sizeof(value_str), "%s", lua_value_get_string(argv[i]));
        } else if (lua_value_is_number(argv[i])) {
            snprintf(value_str, sizeof(value_str), "%g", lua_value_get_number(argv[i]));
        } else if (lua_value_is_bool(argv[i])) {
            snprintf(value_str, sizeof(value_str), "%s", lua_value_get_bool(argv[i]) ? "true" : "false");
        } else if (lua_value_is_nil(argv[i])) {
            snprintf(value_str, sizeof(value_str), "nil");
        } else {
            snprintf(value_str, sizeof(value_str), "<%s>", "unknown");
        }
        
        int n2 = snprintf(buffer + offset, sizeof(buffer) - offset, "%s", value_str);
        if (n2 < 0) n2 = 0;
        offset += (size_t)n2 < sizeof(buffer) - offset ? (size_t)n2 : sizeof(buffer) - offset - 1;
        if (offset >= sizeof(buffer) - 1) break;
    }
    buffer[sizeof(buffer) - 1] = '\0';
    log_debug("LUA_SCRIPT", "%s", buffer);

    // Get the EseEngine from the EseLuaEngine
    EseEngine *ese_engine = (EseEngine *)lua_engine_get_registry_key(engine, ENGINE_KEY);
    engine_add_to_console(ese_engine, ESE_CONSOLE_NORMAL, "LUA", buffer);
    
    return NULL; // print doesn't return a value
}

EseLuaValue * _lua_asset_load_script(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        log_warn("ENGINE", "asset_load_script(String script) takes 1 string argument");
        return lua_value_create_bool("result", false);
    }
    
    if (!lua_value_is_string(argv[0])) {
        log_warn("ENGINE", "asset_load_script(String script) takes 1 string argument");
        return lua_value_create_bool("result", false);
    }
    
    const char *script = lua_value_get_string(argv[0]);

    bool status = lua_engine_load_script(engine, script, "ENTITY");

    log_debug("ENGINE", "Loading script %s has %s.", script, status ? "completed" : "failed");
    
    return lua_value_create_bool("result", status);
}

EseLuaValue* _lua_asset_load_atlas(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc < 2 || argc > 3) {
        log_warn("ENGINE", "asset_load_atlas(String group, String stlas, [Boolean indexed]) takes 2 arguments and 1 optional argument");
        return lua_value_create_bool("result", false);
    }
    
    if (!lua_value_is_string(argv[0]) || !lua_value_is_string(argv[1])) {
        log_warn("ENGINE", "asset_load_atlas(String group, String stlas, [Boolean indexed]) takes 2 arguments and 1 optional argument");
        return lua_value_create_bool("result", false);
    }

    if (argc == 3 && !lua_value_is_bool(argv[2])) {
        log_warn("ENGINE", "asset_load_atlas(String group, String stlas, [Boolean indexed]) takes 2 arguments and 1 optional argument");
        return lua_value_create_bool("result", false);
    }
    
    const char *group = lua_value_get_string(argv[0]);
    const char *atlas = lua_value_get_string(argv[1]);
    bool indexed = argc == 3 ? lua_value_get_bool(argv[2]) : false;

    // Get engine reference
    EseEngine *ese_engine = (EseEngine *)lua_engine_get_registry_key(engine, ENGINE_KEY);
    bool status = asset_manager_load_sprite_atlas(ese_engine->asset_manager, atlas, group, indexed);

    log_debug("ENGINE", "Loading atlas %s has %s.", atlas, status ? "completed" : "failed");
    
    return lua_value_create_bool("result", status);
}

EseLuaValue* _lua_asset_load_shader(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 2) {
        log_warn("ENGINE", "asset_load_shader(String group, String shader_filename) takes 2 string arguments");
        return lua_value_create_bool("result", false);
    }
    
    if (!lua_value_is_string(argv[0]) || !lua_value_is_string(argv[1])) {
        log_warn("ENGINE", "asset_load_shader(String group, String shader_filename) takes 2 string arguments");
        return lua_value_create_bool("result", false);
    }
    
    const char *group_name = lua_value_get_string(argv[0]);
    const char *file_name = lua_value_get_string(argv[1]);

    EseEngine *ese_engine = (EseEngine *)lua_engine_get_registry_key(engine, ENGINE_KEY);
    bool status = renderer_shader_compile(ese_engine->renderer, group_name, file_name);

    return lua_value_create_bool("result", status);
}

EseLuaValue* _lua_asset_load_map(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 2) {
        log_warn("ENGINE", "asset_load_map(String group, String map) takes 2 string arguments");
        return lua_value_create_bool("result", false);
    }
    
    if (!lua_value_is_string(argv[0]) || !lua_value_is_string(argv[1])) {
        log_warn("ENGINE", "asset_load_map(String group, String map) takes 2 string arguments");
        return lua_value_create_bool("result", false);
    }
    
    const char *group = lua_value_get_string(argv[0]);
    const char *map = lua_value_get_string(argv[1]);

    // Get engine reference
    EseEngine *ese_engine = (EseEngine *)lua_engine_get_registry_key(engine, ENGINE_KEY);
    bool status = asset_manager_load_map(ese_engine->asset_manager, engine, map, group);

    log_debug("ENGINE", "Loading map %s has %s.", map, status ? "completed" : "failed");
    
    return lua_value_create_bool("result", status);
}

EseLuaValue* _lua_asset_get_map(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        log_warn("ENGINE", "asset_get_map(String ese_map_id) takes 1 string argument");
        return lua_value_create_bool("result", false);
    }
    
    if (!lua_value_is_string(argv[0])) {
        log_warn("ENGINE", "asset_get_map(String ese_map_id) takes 1 string argument");
        return lua_value_create_bool("result", false);
    }
    
    const char *ese_map_id = lua_value_get_string(argv[0]);

    // Get engine reference
    EseEngine *ese_engine = (EseEngine *)lua_engine_get_registry_key(engine, ENGINE_KEY);
    EseMap *found = asset_manager_get_map(ese_engine->asset_manager, ese_map_id);
    
    if (found) {
        return lua_value_create_map("result", found);
    } else {
        return lua_value_create_nil("result");
    }
}

EseLuaValue* _lua_set_pipeline(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 2) {
        log_warn("ENGINE", "_lua_set_pipeline(String vertexShader, String fragmentShader) takes 2 string arguments");
        return lua_value_create_bool("result", false);
    }
    
    if (!lua_value_is_string(argv[0]) || !lua_value_is_string(argv[1])) {
        log_warn("ENGINE", "_lua_set_pipeline(String vertexShader, String fragmentShader) takes 2 string arguments");
        return lua_value_create_bool("result", false);
    }
    
    const char *vertexShader = lua_value_get_string(argv[0]);
    const char *fragmentShader = lua_value_get_string(argv[1]);

    EseEngine *ese_engine = (EseEngine *)lua_engine_get_registry_key(engine, ENGINE_KEY);
    bool status = renderer_create_pipeline_state(ese_engine->renderer, vertexShader, fragmentShader);

    return lua_value_create_bool("result", status);
}

EseLuaValue* _lua_detect_collision(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 2) {
        log_warn("ENGINE", "detect_collision(rect, number max_results) takes 2 argument");
        return lua_value_create_table("result");
    }

    if (!lua_value_is_number(argv[1])) {
        log_warn("ENGINE", "detect_collision(rect, number max_results) 2nd argument to be a number");
        return lua_value_create_table("result");
    }

    if (!lua_value_is_rect(argv[0])) {
        log_warn("ENGINE", "detect_collision(rect) expected rect");
        return lua_value_create_table("result");
    }

    EseEngine *ese_engine = (EseEngine *)lua_engine_get_registry_key(engine, ENGINE_KEY);
    int max_results = (int)lua_value_get_number(argv[1]);

    EseRect *rect = lua_value_get_rect(argv[0]);
    EseEntity **entities = engine_detect_collision_rect(ese_engine, rect, max_results);

    // Create result table
    EseLuaValue *result = lua_value_create_table("result");
    
    if (entities) {
        int idx = 1;
        for (int i = 0; entities[i] != NULL; i++) {
            // Create a table entry for each entity
            EseLuaValue *entity_entry = lua_value_create_table(NULL);
            lua_value_push(entity_entry, lua_value_create_number("index", idx++), false);
            lua_value_push(entity_entry, lua_value_create_userdata("entity", entities[i]), false);
            lua_value_push(result, entity_entry, false);
        }
        
        // free the list, we dont own the entities
        memory_manager.free(entities);
    }

    return result;
}

EseLuaValue* _lua_scene_clear(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 0) {
        log_warn("ENGINE", "scene_clear() takes 0 arguments");
        return lua_value_create_bool("result", false);
    }

    EseEngine *ese_engine = (EseEngine *)lua_engine_get_registry_key(engine, ENGINE_KEY);
    engine_clear_entities(ese_engine, false);

    return lua_value_create_bool("result", true);
}

EseLuaValue* _lua_scene_reset(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 0) {
        log_warn("ENGINE", "scene_reset() takes 0 arguments");
        return lua_value_create_bool("result", false);
    }

    EseEngine *ese_engine = (EseEngine *)lua_engine_get_registry_key(engine, ENGINE_KEY);
    engine_clear_entities(ese_engine, true);

    return lua_value_create_bool("result", true);
}
