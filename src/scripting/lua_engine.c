#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utility/log.h"
#include "utility/hashmap.h"
#include "vendor/lua/src/lua.h"
#include "vendor/lua/src/lauxlib.h"
#include "vendor/lua/src/lualib.h"
#include "core/memory_manager.h"
#include "platform/filesystem.h"
#include "lua_engine_private.h"
#include "lua_engine.h"

const char _ENGINE_SENTINEL = 0;
const char _ENTITY_LIST_KEY_SENTINEL = 0;
const char _LUA_ENGINE_SENTINEL = 0;

EseLuaEngine *lua_engine_create() {
    EseLuaEngine *engine = memory_manager.malloc(sizeof(EseLuaEngine), MMTAG_LUA);
    engine->internal = memory_manager.malloc(sizeof(EseLuaEngineInternal), MMTAG_LUA);

    engine->internal->memory_limit = 1024 * 1024 * 10; // 10MB
    engine->internal->max_execution_time = (clock_t)((10) * CLOCKS_PER_SEC); // 10 second
    engine->internal->max_instruction_count = 4000000; // 4M

    // Initialize Lua runtime
    engine->runtime = lua_newstate(_lua_engine_limited_alloc, engine);
    if (!engine->runtime) {
        log_error("LUA_ENGINE", "Failed to create Lua runtime");
        memory_manager.free(engine);
        return NULL;
    }

    engine->internal->functions = hashmap_create();

    // Stop auto GC
    lua_gc(engine->runtime, LUA_GCSTOP, 0); 

    // Load Libs
    luaL_requiref(engine->runtime, "_G", luaopen_base, 1);
    luaL_requiref(engine->runtime, "table", luaopen_table, 1);
    luaL_requiref(engine->runtime, "string", luaopen_string, 1);
    luaL_requiref(engine->runtime, "math", luaopen_math, 1);
    lua_pop(engine->runtime, 4);  // Remove libs from stack

    // Remove dangerous functions
    lua_pushnil(engine->runtime);
    lua_setglobal(engine->runtime, "dofile");
    lua_pushnil(engine->runtime);
    lua_setglobal(engine->runtime, "loadfile");
    lua_pushnil(engine->runtime);
    lua_setglobal(engine->runtime, "require");

    // Create the entity_list table and store in registry
    lua_newtable(engine->runtime);

    // Set as global first
    lua_pushvalue(engine->runtime, -1);  // Copy table
    lua_setglobal(engine->runtime, "entity_list");

    // Then store the same table in registry
    lua_pushlightuserdata(engine->runtime, ENTITY_LIST_KEY);
    lua_pushvalue(engine->runtime, -2);  // Copy table again
    lua_settable(engine->runtime, LUA_REGISTRYINDEX);  // registry[ENTITY_LIST_KEY] = table

    // Clean up stack
    lua_pop(engine->runtime, 1);

    // Create script_sandbox_master, copy only explicitly safe keys.
    lua_newtable(engine->runtime);                 // [master]
    int master_idx = lua_gettop(engine->runtime);

    lua_getglobal(engine->runtime, "_G");          // [master, _G]
    int g_idx = lua_gettop(engine->runtime);

    _lua_copy_field(engine->runtime, g_idx, master_idx, "assert");
    _lua_copy_field(engine->runtime, g_idx, master_idx, "pairs");
    _lua_copy_field(engine->runtime, g_idx, master_idx, "ipairs");
    _lua_copy_field(engine->runtime, g_idx, master_idx, "next");
    _lua_copy_field(engine->runtime, g_idx, master_idx, "type");
    _lua_copy_field(engine->runtime, g_idx, master_idx, "tostring");
    _lua_copy_field(engine->runtime, g_idx, master_idx, "tonumber");
    _lua_copy_field(engine->runtime, g_idx, master_idx, "select");
    _lua_copy_field(engine->runtime, g_idx, master_idx, "pcall");
    _lua_copy_field(engine->runtime, g_idx, master_idx, "xpcall");
    _lua_copy_field(engine->runtime, g_idx, master_idx, "math");
    _lua_copy_field(engine->runtime, g_idx, master_idx, "string");
    _lua_copy_field(engine->runtime, g_idx, master_idx, "table");
    _lua_copy_field(engine->runtime, g_idx, master_idx, "print");
    _lua_copy_field(engine->runtime, g_idx, master_idx, "_VERSION");

    lua_pushvalue(engine->runtime, master_idx);
    lua_setfield(engine->runtime, master_idx, "_G");

    engine->internal->sandbox_master_ref = luaL_ref(engine->runtime, LUA_REGISTRYINDEX);

    // Pop _G
    lua_pop(engine->runtime, 1);

    return engine;
}

void lua_engine_destroy(EseLuaEngine *engine) {
    log_assert("LUA_ENGINE", engine, "engine_destroy called with NULL engine");

    // Need to iterate and memory_manager.free all script references
    EseHashMapIter *iter = hashmap_iter_create(engine->internal->functions);
    void* value;
    while (hashmap_iter_next(iter, NULL, &value) == 1) {
        int *ref = (int*)value;
        luaL_unref(engine->runtime, LUA_REGISTRYINDEX, *ref);
        memory_manager.free(ref);
    }
    hashmap_iter_free(iter);

    hashmap_free(engine->internal->functions);

    if (engine->internal->sandbox_master_ref != LUA_NOREF) {
        luaL_unref(engine->runtime, LUA_REGISTRYINDEX, engine->internal->sandbox_master_ref);
    }

    lua_close(engine->runtime);

    memory_manager.free(engine->internal);
    memory_manager.free(engine);
}

void lua_engine_global_lock(EseLuaEngine *engine) {
    lua_State *L = engine->runtime;

    // Prevent global writes (lock global)
    lua_getglobal(L, "_G");
    lua_newtable(L);
    lua_pushcfunction(L, _lua_global_write_error);
    lua_setfield(L, -2, "__newindex");
    lua_pushstring(L, "locked");
    lua_setfield(L, -2, "__metatable");
    lua_setmetatable(L, -2);
    lua_pop(L, 1);

    // Lock the sandbox
    if (engine->internal->sandbox_master_ref != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, engine->internal->sandbox_master_ref);
        lua_newtable(L);
        lua_pushstring(L, "locked");
        lua_setfield(L, -2, "__metatable");
        lua_setmetatable(L, -2);
        lua_pop(L, 1);
    }
}

void lua_engine_gc(EseLuaEngine *engine) {
    lua_gc(engine->runtime, LUA_GCSTEP, 0);
}

void lua_engine_add_registry_key(lua_State *L, const void *key, void *ptr) {
    log_assert("LUA_ENGINE", L, "lua_engine_add_registry_key called with NULL L");
    lua_pushlightuserdata(L, (void *)key);  // push registry key
    lua_pushlightuserdata(L, ptr);          // push the pointer value
    lua_settable(L, LUA_REGISTRYINDEX);     // registry[key] = ptr
}

void *lua_engine_get_registry_key(lua_State *L, const void *key) {
    log_assert("LUA_ENGINE", L, "lua_engine_get_registry_key called with NULL L");
    void *result;
    lua_pushlightuserdata(L, (void *)key);  // push registry key
    lua_gettable(L, LUA_REGISTRYINDEX);     // pushes registry[key]
    result = lua_touserdata(L, -1);         // get lightuserdata
    lua_pop(L, 1);                          // pop the value
    return result;
}

void lua_engine_remove_registry_key(lua_State *L, const void *key) {
    log_assert("LUA_ENGINE", L, "lua_engine_add_registry_key called with NULL L");
    lua_pushlightuserdata(L, (void *)key);
    lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);
}

void lua_engine_add_function(EseLuaEngine *engine, const char *function_name, lua_CFunction func) {
    log_assert("LUA_ENGINE", engine, "lua_engine_add_function called with NULL engine");
    log_assert("LUA_ENGINE", function_name, "lua_engine_add_function called with NULL function_name");
    log_assert("LUA_ENGINE", func, "lua_engine_add_function called with NULL func");
    log_assert("LUA_ENGINE", engine->internal->sandbox_master_ref != LUA_NOREF, "lua_engine_add_function engine->internal->sandbox_master_ref is LUA_NOREF");

    // Add to script_sandbox_master
    lua_rawgeti(engine->runtime, LUA_REGISTRYINDEX, engine->internal->sandbox_master_ref); // master
    lua_pushcfunction(engine->runtime, func);
    lua_setfield(engine->runtime, -2, function_name);
    lua_pop(engine->runtime, 1); // master

    log_debug("LUA_ENGINE", "Added C function '%s' to Lua.", function_name);
}

void lua_engine_add_global(EseLuaEngine *engine, const char *global_name, int lua_ref) {
    log_assert("LUA_ENGINE", engine, "lua_engine_add_global called with NULL engine");
    log_assert("LUA_ENGINE", global_name, "lua_engine_add_global called with NULL global_name");
    log_assert("LUA_ENGINE", lua_ref != LUA_NOREF, "lua_engine_add_global called with LUA_NOREF lua_ref");
    log_assert("LUA_ENGINE", engine->internal->sandbox_master_ref != LUA_NOREF, "lua_engine_add_global engine->internal->sandbox_master_ref is LUA_NOREF");

    // Add to script_sandbox_master
    lua_rawgeti(engine->runtime, LUA_REGISTRYINDEX, engine->internal->sandbox_master_ref); // master
    lua_rawgeti(engine->runtime, LUA_REGISTRYINDEX, lua_ref);
    lua_setfield(engine->runtime, -2, global_name);
    lua_pop(engine->runtime, 1); // master
}

bool lua_engine_load_script(EseLuaEngine *engine, const char* filename, const char* module_name) {
    log_assert("LUA_ENGINE", engine, "lua_engine_load_script called with NULL engine");
    log_assert("LUA_ENGINE", filename, "lua_engine_load_script called with NULL filename");
    log_assert("LUA_ENGINE", engine->internal->sandbox_master_ref != LUA_NOREF, "lua_engine_load_script engine->internal->sandbox_master_ref is LUA_NOREF");

    if (!filesystem_check_file(filename, ".lua")) {
        log_error("LUA_ENGINE", "Error: invalid %s", filename);
        return false;
    }

    int *func = hashmap_get(engine->internal->functions, filename);
    if (func) {
        return true;
    }

    char *full_path = filesystem_get_resource(filename);
    if (!full_path) {
        log_error("LUA_ENGINE", "Error: filesystem_get_resource failed for %s", filename);
        return false;
    }

    // Open the file
    FILE* file = fopen(full_path, "r");
    if (!file) {
        log_error("LUA_ENGINE", "Error: Failed to open Lua script file '%s'", full_path);
        memory_manager.free(full_path);
        return false;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        log_error("LUA_ENGINE", "Error: Failed to get file size for '%s'", full_path);
        memory_manager.free(full_path);
        fclose(file);
        return false;
    }

    // Allocate buffer for script content
    char* script = memory_manager.malloc(file_size + 1, MMTAG_LUA);
    size_t bytes_read = fread(script, 1, file_size, file);
    if (bytes_read != (size_t)file_size) {
        log_error("LUA_ENGINE", "Error: Failed to read complete Lua script from '%s'", full_path);
        memory_manager.free(script);
        memory_manager.free(full_path);
        fclose(file);
        return false;
    }
    script[file_size] = '\0';

    fclose(file);

    _lua_engine_build_env_from_master(engine->runtime, engine->internal->sandbox_master_ref);
    int env_idx = lua_gettop(engine->runtime);

    // Create M table and add to environment
    lua_newtable(engine->runtime);  // Create module
    lua_setfield(engine->runtime, env_idx, module_name); 

    // Lock environment against modifications
    lua_newtable(engine->runtime);  // metatable
    lua_pushcfunction(engine->runtime, _lua_global_write_error);
    lua_setfield(engine->runtime, -2, "__newindex");
    lua_pushstring(engine->runtime, "locked");
    lua_setfield(engine->runtime, -2, "__metatable");
    lua_setmetatable(engine->runtime, env_idx);

    const char *prologue = "local _ENV = ...; (function() ";
    size_t epilogue_len = strlen(" end)(); return ") + strlen(module_name) + 1;
    char *epilogue = memory_manager.malloc(epilogue_len, MMTAG_LUA);
    snprintf(epilogue, epilogue_len, " end)(); return %s", module_name);

    char *processed_script = _replace_colon_calls(module_name, script);
    size_t new_len = strlen(prologue) + strlen(processed_script) + strlen(epilogue);
    char *wrapped = memory_manager.malloc(new_len + 1, MMTAG_LUA);
    snprintf(wrapped, new_len + 1, "%s%s%s", prologue, processed_script, epilogue);
    memory_manager.free(processed_script);
    memory_manager.free(epilogue);

    // Load the script
    if (luaL_loadstring(engine->runtime, wrapped) == LUA_OK) {
        lua_pushvalue(engine->runtime, env_idx);
        if (lua_pcall(engine->runtime, 1, 1, 0) == LUA_OK) {
            int script_ref = luaL_ref(engine->runtime, LUA_REGISTRYINDEX);

            int *ref = memory_manager.malloc(sizeof(int), MMTAG_LUA);
            *ref = script_ref;
            hashmap_set(engine->internal->functions, filename, ref);
            memory_manager.free(full_path);
            memory_manager.free(script); 
            memory_manager.free(wrapped);
            return true;
        } else {
            log_error("LUA_ENGINE", "Error executing script '%s': %s", filename, lua_tostring(engine->runtime, -1));
            lua_pop(engine->runtime, 1);
        }
    } else {
        log_error("LUA_ENGINE", "Error loading script '%s': %s", filename, lua_tostring(engine->runtime, -1));
        lua_pop(engine->runtime, 1);
    }

    lua_pop(engine->runtime, 1); // env

    log_debug("LUA_ENGINE", "New script %s", filename);
    memory_manager.free(full_path);
    memory_manager.free(script);
    memory_manager.free(wrapped);
    return false;
}

int lua_engine_instance_script(EseLuaEngine *engine, const char *filename) {
    log_assert("LUA_ENGINE", engine, "lua_engine_instance_script called with NULL engine");
    log_assert("LUA_ENGINE", filename, "lua_engine_instance_script called with NULL filename");

    int *script_ref = hashmap_get(engine->internal->functions, filename);
    if (!script_ref) {
        log_error("LUA_ENGINE", "Script not found");
        return -1;
    }

    lua_State *L = engine->runtime;

    // Push the class table (returned by the script) onto the stack
    lua_rawgeti(L, LUA_REGISTRYINDEX, *script_ref);

    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        log_error("LUA_ENGINE", "Script did not return a table");
        return -1;
    }

    // Create a new instance table
    lua_newtable(L); // instance table

    // Set metatable for instance with __index = class table
    lua_newtable(L);                // metatable
    lua_pushvalue(L, -3);           // push class table
    lua_setfield(L, -2, "__index"); // metatable.__index = class table
    lua_setmetatable(L, -2);        // set metatable for instance

    // Remove class table, leave instance table on top
    lua_remove(L, -2);

    // Reference the instance table in the registry
    int instance_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    log_debug("LUA_ENGINE", "New instance %d", instance_ref);
    return instance_ref;
}

void lua_engine_instance_remove(EseLuaEngine *engine, int instance_ref) {
    log_assert("LUA_ENGINE", engine, "lua_engine_instance_remove called with NULL engine");

    lua_State *L = engine->runtime;

    luaL_unref(L, LUA_REGISTRYINDEX, instance_ref);
}

bool lua_engine_instance_run_function(EseLuaEngine *engine, int instance_ref, int self_ref, const char *func_name) {
    log_assert("LUA_ENGINE", engine, "lua_engine_instance_run_function called with NULL engine");
    log_assert("LUA_ENGINE", func_name, "lua_engine_instance_run_function called with NULL func_name");

    lua_State *L = engine->runtime;

    if (!_lua_engine_instance_get_function(L, instance_ref, func_name)) {
        return false;
    }

    // Stack: [function]
    lua_rawgeti(L, LUA_REGISTRYINDEX, self_ref); // push entity proxy as self
    int n_args = 1; // se
    // Stack: [function][instance]

    // Setup timeout
    LuaFunctionHook timeout;
    timeout.start_time = clock();
    timeout.call_count = 0;
    timeout.instruction_count = 0;
    timeout.max_execution_time = engine->internal->max_execution_time;
    timeout.max_instruction_count = engine->internal->max_instruction_count;

    // Set the hook with the timeout as upvalue
    lua_pushlightuserdata(L, &timeout);
    lua_setfield(L, LUA_REGISTRYINDEX, LUA_HOOK_KEY);
    lua_sethook(L, _lua_engine_function_hook, LUA_MASKCOUNT, LUA_HOOK_FRQ);

    bool ok = true;
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        log_error("LUA_ENGINE", "Error running '%s': %s", func_name, lua_tostring(L, -1));
        lua_pop(L, 1); // error message
        ok = false;
    }

    // Remove the hook
    lua_sethook(L, NULL, 0, 0);

    // log_debug("LUA_ENGINE", "Stats: call count = %d  instruction count = %zu", timeout.call_count, timeout.instruction_count);

    // No need to pop anything else; stack is clean
    return true;
}

bool lua_engine_instance_run_function_with_args(EseLuaEngine *engine, int instance_ref, int self_ref, const char *func_name, int argc, EseLuaValue *argv) {
    log_assert("LUA_ENGINE", engine, "lua_engine_instance_run_function called with NULL engine");
    log_assert("LUA_ENGINE", func_name, "lua_engine_instance_run_function called with NULL func_name");

    lua_State *L = engine->runtime;

    // Find the function in the script instance
    if (!_lua_engine_instance_get_function(L, instance_ref, func_name)) {
        // just move on, not all scripts have all functions
        return false;
    }

    // Stack: [function]

    // Push the entity proxy as the "self" argument
    lua_rawgeti(L, LUA_REGISTRYINDEX, self_ref); // push entity proxy as self
    int n_args = 1; // self

    // Add args to stack
    for (int i = 0; i < argc; ++i) {
        _lua_engine_push_luavalue(L, &argv[i]);
        n_args++;
    }

    // Setup timeout
    LuaFunctionHook timeout;
    timeout.start_time = clock();
    timeout.call_count = 0;
    timeout.instruction_count = 0;
    timeout.max_execution_time = engine->internal->max_execution_time;
    timeout.max_instruction_count = engine->internal->max_instruction_count;

    // Set the hook with the timeout as upvalue
    lua_pushlightuserdata(L, &timeout);
    lua_setfield(L, LUA_REGISTRYINDEX, LUA_HOOK_KEY);
    lua_sethook(L, _lua_engine_function_hook, LUA_MASKCOUNT, LUA_HOOK_FRQ);

    bool ok = true;
    if (lua_pcall(L, n_args, 0, 0) != LUA_OK) {
        log_error("LUA_ENGINE", "Error running '%s': %s", func_name, lua_tostring(L, -1));
        lua_pop(L, 1); // error message
        ok = false;
    }

    // Remove the hook
    lua_sethook(L, NULL, 0, 0);

    // log_debug("LUA_ENGINE", "Stats: call count = %d  instruction count = %zu", timeout.call_count, timeout.instruction_count);

    // No need to pop anything else; stack is clean
    return ok;
}
