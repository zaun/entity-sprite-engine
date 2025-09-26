#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
#include "utility/log.h"
#include "utility/hashmap.h"
#include "utility/profile.h"
#include "vendor/lua/src/lua.h"
#include "vendor/lua/src/lauxlib.h"
#include "vendor/lua/src/lualib.h"
#include "core/engine.h"
#include "core/console.h"
#include "core/memory_manager.h"
#include "platform/filesystem.h"
#include "platform/time.h"
#include "lua_engine_private.h"
#include "lua_engine.h"

const char _ENGINE_SENTINEL = 0;
const char _ENTITY_LIST_KEY_SENTINEL = 0;
const char _LUA_ENGINE_SENTINEL = 0;

static jmp_buf g_lua_panic_jmp;

static int my_panic(lua_State *L) {
    const char *msg = lua_tostring(L, -1);
    log_error("LUA_ENGINE", "Lua panic: %s", msg ? msg : "unknown");
    longjmp(g_lua_panic_jmp, 1); // jump back, never return
    return 0;
}

EseLuaEngine *lua_engine_create() {
    EseLuaEngine *engine = memory_manager.malloc(sizeof(EseLuaEngine), MMTAG_LUA);
    engine->internal = memory_manager.malloc(sizeof(EseLuaEngineInternal), MMTAG_LUA);

    engine->internal->memory_limit = 1024 * 1024 * 10; // 10MB
    engine->internal->memory_used = 0;

    // Set default limits
    engine->internal->max_execution_time = (clock_t)((10) * CLOCKS_PER_SEC); // 10 second timeout protection
    engine->internal->max_instruction_count = 4000000; // 4M instruction limit for security

    // Initialize Lua runtime
    engine->runtime = lua_newstate(_lua_engine_limited_alloc, engine);
    if (!engine->runtime) {
        log_error("LUA_ENGINE", "Failed to create Lua runtime");
        memory_manager.free(engine);
        return NULL;
    }

    engine->internal->functions = hashmap_create((EseHashMapFreeFn)memory_manager.free);

    
    // Log on lua panic
    lua_atpanic(engine->runtime, my_panic);
    if (setjmp(g_lua_panic_jmp) != 0) {
        log_error("LUA_ENGINE", "Recovered from Lua panic");
        return NULL;
    }

    // Load all standard libraries including JIT
    luaL_openlibs(engine->runtime);
    
    // Enable JIT compiler (it's disabled by default)
    lua_getglobal(engine->runtime, "jit");
    if (lua_istable(engine->runtime, -1)) {
        lua_getfield(engine->runtime, -1, "on");
        if (lua_isfunction(engine->runtime, -1)) {
            // Use pcall to safely enable JIT
            int call_result = lua_pcall(engine->runtime, 0, 0, 0);
            if (call_result == LUA_OK) {
                log_debug("LUA_ENGINE", "JIT compiler enabled");
            } else {
                const char* error_msg = lua_tostring(engine->runtime, -1);
                log_error("LUA_ENGINE", "Failed to enable JIT: %s", error_msg ? error_msg : "unknown error");
                lua_pop(engine->runtime, 1);
            }
        } else {
            log_error("LUA_ENGINE", "JIT on function not available");
            lua_pop(engine->runtime, 1);
        }
    } else {
        log_error("LUA_ENGINE", "JIT table not available");
    }
    lua_pop(engine->runtime, 1);
    
    // Verify JIT is loaded and working
    lua_getglobal(engine->runtime, "jit");
    if (lua_istable(engine->runtime, -1)) {
        log_debug("LUA_ENGINE", "JIT library loaded successfully");
        
        // Check JIT status (simple check - just verify the function exists)
        lua_getfield(engine->runtime, -1, "status");
        if (lua_isfunction(engine->runtime, -1)) {
            log_debug("LUA_ENGINE", "JIT Status: available");
        } else {
            log_debug("LUA_ENGINE", "JIT status field is not a function");
        }
        lua_pop(engine->runtime, 1);
        
        // Check additional JIT info
        lua_getfield(engine->runtime, -1, "version");
        if (lua_isstring(engine->runtime, -1)) {
            log_debug("LUA_ENGINE", "JIT Version: %s", lua_tostring(engine->runtime, -1));
        }
        lua_pop(engine->runtime, 1);
        
        lua_getfield(engine->runtime, -1, "os");
        if (lua_isstring(engine->runtime, -1)) {
            log_debug("LUA_ENGINE", "JIT OS: %s", lua_tostring(engine->runtime, -1));
        }
        lua_pop(engine->runtime, 1);
        
        lua_getfield(engine->runtime, -1, "arch");
        if (lua_isstring(engine->runtime, -1)) {
            log_debug("LUA_ENGINE", "JIT Arch: %s", lua_tostring(engine->runtime, -1));
        }
        lua_pop(engine->runtime, 1);
        
        // Check JIT profiling
        lua_getfield(engine->runtime, -1, "profile");
        if (lua_isfunction(engine->runtime, -1)) {
            log_debug("LUA_ENGINE", "JIT profile function available");
        } else {
            log_debug("LUA_ENGINE", "JIT profile field is not a function");
        }
        lua_pop(engine->runtime, 1);
    } else {
        log_debug("LUA_ENGINE", "JIT library failed to load!");
    }
    lua_pop(engine->runtime, 1);

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
    _lua_copy_field(engine->runtime, g_idx, master_idx, "jit");

    lua_pushvalue(engine->runtime, master_idx);
    lua_setfield(engine->runtime, master_idx, "_G");

    engine->internal->sandbox_master_ref = luaL_ref(engine->runtime, LUA_REGISTRYINDEX);

    lua_gc(engine->runtime, LUA_GCRESTART, 0);

    // Pop _G
    lua_pop(engine->runtime, 1);
    log_debug("LUA_ENGINE", "Lua engine created successfully");

    return engine;
}

void lua_engine_destroy(EseLuaEngine *engine) {
    log_assert("LUA_ENGINE", engine, "engine_destroy called with NULL engine");

    // Need to iterate and memory_manager.free all script references
    hashmap_free(engine->internal->functions);

    if (engine->internal->sandbox_master_ref != LUA_NOREF) {
        luaL_unref(engine->runtime, LUA_REGISTRYINDEX, engine->internal->sandbox_master_ref);
    }

    lua_close(engine->runtime);

    memory_manager.free(engine->internal);
    memory_manager.free(engine);
}

void lua_engine_global_lock(EseLuaEngine *engine) {
    log_assert("LUA_ENGINE", engine, "lua_eng_global_lock called with NULL engine");
    
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
    log_assert("LUA_ENGINE", engine, "lua_eng_gc called with NULL engine");
    
    lua_gc(engine->runtime, LUA_GCSTEP, 0);
}

void lua_engine_add_registry_key(lua_State *L, const void *key, void *ptr) {
    log_assert("LUA_ENGINE", L, "lua_eng_add_registry_key called with NULL L");
    lua_pushlightuserdata(L, (void *)key);  // push registry key
    lua_pushlightuserdata(L, ptr);          // push the pointer value
    lua_settable(L, LUA_REGISTRYINDEX);     // registry[key] = ptr
}

void *lua_engine_get_registry_key(lua_State *L, const void *key) {
    log_assert("LUA_ENGINE", L, "lua_eng_get_registry_key called with NULL L");
    void *result;
    lua_pushlightuserdata(L, (void *)key);  // push registry key
    lua_gettable(L, LUA_REGISTRYINDEX);     // pushes registry[key]
    result = lua_touserdata(L, -1);         // get lightuserdata
    lua_pop(L, 1);                          // pop the value
    return result;
}

void lua_engine_remove_registry_key(lua_State *L, const void *key) {
    log_assert("LUA_ENGINE", L, "lua_eng_add_registry_key called with NULL L");
    lua_pushlightuserdata(L, (void *)key);
    lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);
}

void lua_engine_add_function(EseLuaEngine *engine, const char *function_name, lua_CFunction func) {
    log_assert("LUA_ENGINE", engine, "lua_eng_add_function called with NULL engine");
    log_assert("LUA_ENGINE", function_name, "lua_eng_add_function called with NULL function_name");
    log_assert("LUA_ENGINE", func, "lua_eng_add_function called with NULL func");
    log_assert("LUA_ENGINE", engine->internal->sandbox_master_ref != LUA_NOREF, "lua_eng_add_function engine->internal->sandbox_master_ref is LUA_NOREF");

    // Add to script_sandbox_master
    lua_rawgeti(engine->runtime, LUA_REGISTRYINDEX, engine->internal->sandbox_master_ref); // master
    lua_pushcfunction(engine->runtime, func);
    lua_setfield(engine->runtime, -2, function_name);
    lua_pop(engine->runtime, 1); // master

    log_debug("LUA_ENGINE", "Added C function '%s' to Lua.", function_name);
}

void lua_engine_add_global(EseLuaEngine *engine, const char *global_name, int lua_ref) {
    log_assert("LUA_ENGINE", engine, "lua_eng_add_global called with NULL engine");
    log_assert("LUA_ENGINE", global_name, "lua_eng_add_global called with NULL global_name");
    log_assert("LUA_ENGINE", lua_ref != LUA_NOREF, "lua_eng_add_global called with LUA_NOREF lua_ref");
    log_assert("LUA_ENGINE", engine->internal->sandbox_master_ref != LUA_NOREF, "lua_eng_add_global engine->internal->sandbox_master_ref is LUA_NOREF");

    // Add to script_sandbox_master
    lua_rawgeti(engine->runtime, LUA_REGISTRYINDEX, engine->internal->sandbox_master_ref); // master
    lua_rawgeti(engine->runtime, LUA_REGISTRYINDEX, lua_ref);
    lua_setfield(engine->runtime, -2, global_name);
    lua_pop(engine->runtime, 1); // master
}

bool lua_engine_load_script(EseLuaEngine *engine, const char* filename, const char* module_name) {
    log_assert("LUA_ENGINE", engine, "lua_eng_load_script called with NULL engine");
    log_assert("LUA_ENGINE", filename, "lua_eng_load_script called with NULL filename");
    log_assert("LUA_ENGINE", module_name, "lua_eng_load_script called with NULL module_name");
    log_assert("LUA_ENGINE", engine->internal->sandbox_master_ref != LUA_NOREF, "lua_eng_load_script engine->internal->sandbox_master_ref is LUA_NOREF");

    profile_start(PROFILE_LUA_ENGINE_LOAD_SCRIPT);

    if (!filesystem_check_file(filename, ".lua")) {
        log_error("LUA_ENGINE", "Error: invalid %s", filename);
        profile_cancel(PROFILE_LUA_ENGINE_LOAD_SCRIPT);
        profile_count_add("lua_eng_load_script_failed");
        return false;
    }

    int *func = hashmap_get(engine->internal->functions, filename);
    if (func) {
        profile_stop(PROFILE_LUA_ENGINE_LOAD_SCRIPT, "lua_eng_load_script");
        profile_count_add("lua_eng_load_script_already_loaded");
        return true; // already loaded
    }

    char *full_path = filesystem_get_resource(filename);
    if (!full_path) {
        log_error("LUA_ENGINE", "Error: filesystem_get_resource failed for %s", filename);
        profile_cancel(PROFILE_LUA_ENGINE_LOAD_SCRIPT);
        profile_count_add("lua_eng_load_script_failed");
        return false;
    }

    FILE* file = fopen(full_path, "r");
    if (!file) {
        log_error("LUA_ENGINE", "Error: Failed to open Lua script file '%s'", full_path);
        memory_manager.free(full_path);
        profile_cancel(PROFILE_LUA_ENGINE_LOAD_SCRIPT);
        profile_count_add("lua_eng_load_script_failed");
        return false;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        log_error("LUA_ENGINE", "Error: Failed to get file size for '%s'", full_path);
        memory_manager.free(full_path);
        fclose(file);
        profile_cancel(PROFILE_LUA_ENGINE_LOAD_SCRIPT);
        profile_count_add("lua_eng_load_script_failed");
        return false;
    }

    char* script = memory_manager.malloc(file_size + 1, MMTAG_LUA);
    size_t bytes_read = fread(script, 1, file_size, file);
    fclose(file);

    if (bytes_read != (size_t)file_size) {
        log_error("LUA_ENGINE", "Error: Failed to read complete Lua script from '%s'", full_path);
        memory_manager.free(script);
        memory_manager.free(full_path);
        profile_cancel(PROFILE_LUA_ENGINE_LOAD_SCRIPT);
        profile_count_add("lua_eng_load_script_failed");
        return false;
    }
    script[file_size] = '\0';

    bool status = lua_engine_load_script_from_string(engine, script, filename, module_name);
    memory_manager.free(script);
    memory_manager.free(full_path);
    
    if (status) {
        profile_stop(PROFILE_LUA_ENGINE_LOAD_SCRIPT, "lua_eng_load_script");
        profile_count_add("lua_eng_load_script_success");
    } else {
        profile_cancel(PROFILE_LUA_ENGINE_LOAD_SCRIPT);
        profile_count_add("lua_eng_load_script_failed");
    }
    
    return status;
}


bool lua_engine_load_script_from_string(EseLuaEngine *engine, const char* script, const char* name, const char* module_name) {
    log_assert("LUA_ENGINE", engine, "lua_eng_load_script_frm_str called with NULL engine");
    log_assert("LUA_ENGINE", script, "lua_eng_load_script_frm_str called with NULL script");
    log_assert("LUA_ENGINE", name, "lua_eng_load_script_frm_str called with NULL name");
    log_assert("LUA_ENGINE", module_name, "lua_eng_load_script_frm_str called with NULL module_name");

    profile_start(PROFILE_LUA_ENGINE_LOAD_SCRIPT_STRING);

    // Build environment
    _lua_engine_build_env_from_master(engine->runtime, engine->internal->sandbox_master_ref);
    int env_idx = lua_gettop(engine->runtime);

    // Create module table (STARTUP)
    lua_newtable(engine->runtime);
    lua_setfield(engine->runtime, env_idx, module_name);

    // Lock environment
    lua_newtable(engine->runtime);
    lua_pushcfunction(engine->runtime, _lua_global_write_error);
    lua_setfield(engine->runtime, -2, "__newindex");
    lua_pushstring(engine->runtime, "locked");
    lua_setfield(engine->runtime, -2, "__metatable");
    lua_setmetatable(engine->runtime, env_idx);

    // run the pre-processor
    char *processed_script = _replace_colon_calls(module_name, script);

    // Wrap script: LuaJIT-safe, returns module table
    const char *prologue_fmt = "local %s = ...\n";
    const char *epilogue_fmt = "\nreturn %s\n";
    size_t new_len = strlen(prologue_fmt) + strlen(module_name)
                   + strlen(processed_script)
                   + strlen(epilogue_fmt) + strlen(module_name) + 1;

    char *wrapped = memory_manager.malloc(new_len, MMTAG_LUA);
    snprintf(wrapped, new_len, prologue_fmt, module_name);
    strcat(wrapped, processed_script);
    char epilogue[256];
    snprintf(epilogue, sizeof(epilogue), epilogue_fmt, module_name);
    strcat(wrapped, epilogue);
    memory_manager.free(processed_script);

    // Load chunk
    char chunkname[512];
    snprintf(chunkname, sizeof(chunkname), "@%s", name ? name : "unnamed");

    if (luaL_loadbuffer(engine->runtime, wrapped, strlen(wrapped), chunkname) == LUA_OK) {
        // // Push the per-script environment table as the chunk's environment
        // lua_pushvalue(engine->runtime, env_idx);
        // lua_setfenv(engine->runtime, -2);
    
        // Push module table as argument
        lua_getfield(engine->runtime, env_idx, module_name);

        if (lua_pcall(engine->runtime, 1, 1, 0) == LUA_OK) {
            if (lua_istable(engine->runtime, -1)) {
                int script_ref = luaL_ref(engine->runtime, LUA_REGISTRYINDEX);
                int *ref = memory_manager.malloc(sizeof(int), MMTAG_LUA);
                *ref = script_ref;
                hashmap_set(engine->internal->functions, name, ref);

                memory_manager.free(wrapped);
                profile_stop(PROFILE_LUA_ENGINE_LOAD_SCRIPT_STRING, "lua_eng_load_script_string");
                profile_count_add("lua_eng_load_script_string_success");
                return true;
            } else {
                log_error("LUA_ENGINE", "Script '%s' did not return a table", name);
                lua_pop(engine->runtime, 1);
            }
        } else {
            log_error("LUA_ENGINE", "Error executing script '%s': %s",
                name, lua_tostring(engine->runtime, -1));
            lua_pop(engine->runtime, 1);
        }
    } else {
        log_error("LUA_ENGINE", "Error loading script '%s': %s",
            name, lua_tostring(engine->runtime, -1));
        lua_pop(engine->runtime, 1);
    }

    lua_pop(engine->runtime, 1); // pop env
    memory_manager.free(wrapped);
    profile_cancel(PROFILE_LUA_ENGINE_LOAD_SCRIPT_STRING);
    profile_count_add("lua_eng_load_script_string_failed");
    return false;
}

int lua_engine_instance_script(EseLuaEngine *engine, const char *name) {
    log_assert("LUA_ENGINE", engine, "lua_eng_inst_script called with NULL engine");
    log_assert("LUA_ENGINE", name, "lua_eng_inst_script called with NULL name");

    profile_start(PROFILE_LUA_ENGINE_INSTANCE_SCRIPT);

    int *script_ref = hashmap_get(engine->internal->functions, name);
    if (!script_ref) {
        log_error("LUA_ENGINE", "Script '%s' not found", name);
        profile_cancel(PROFILE_LUA_ENGINE_INSTANCE_SCRIPT);
        profile_count_add("lua_eng_inst_script_not_found");
        return -1;
    }

    lua_State *L = engine->runtime;

    // Push the class table (returned by the script) onto the stack
    lua_rawgeti(L, LUA_REGISTRYINDEX, *script_ref);

    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        log_error("LUA_ENGINE", "Script did not return a table");
        profile_cancel(PROFILE_LUA_ENGINE_INSTANCE_SCRIPT);
        profile_count_add("lua_eng_inst_script_not_table");
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

    profile_stop(PROFILE_LUA_ENGINE_INSTANCE_SCRIPT, "lua_eng_inst_script");
    profile_count_add("lua_eng_inst_script_success");
    return instance_ref;
}

void lua_engine_instance_remove(EseLuaEngine *engine, int instance_ref) {
    log_assert("LUA_ENGINE", engine, "lua_eng_inst_remove called with NULL engine");

    lua_State *L = engine->runtime;

    luaL_unref(L, LUA_REGISTRYINDEX, instance_ref);
}

/**
 * @brief Converts a Lua value on the stack to an EseLuaValue structure.
 * 
 * @details This function converts the Lua value at the specified stack index
 *          to an EseLuaValue structure. It handles all basic Lua types including
 *          nil, boolean, number, string, table, and userdata.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua value to convert
 * @param out_result Pointer to EseLuaValue to store the converted result
 * 
 * @warning The caller is responsible for freeing the out_result when done.
 */
static void _lua_engine_convert_stack_to_luavalue(lua_State *L, int idx, EseLuaValue *out_result) {
    if (!out_result) return;
    
    // Reset the output value
    lua_value_set_nil(out_result);
    
    // Handle negative indices by converting to positive
    int abs_idx = idx;
    if (idx < 0) {
        abs_idx = lua_gettop(L) + idx + 1;
    }
    
    if (lua_isnil(L, abs_idx)) {
        lua_value_set_nil(out_result);
    } else if (lua_isboolean(L, abs_idx)) {
        lua_value_set_bool(out_result, lua_toboolean(L, abs_idx));
    } else if (lua_isnumber(L, abs_idx)) {
        lua_value_set_number(out_result, lua_tonumber(L, abs_idx));
    } else if (lua_isstring(L, abs_idx)) {
        lua_value_set_string(out_result, lua_tostring(L, abs_idx));
    } else if (lua_istable(L, abs_idx)) {
        lua_value_set_table(out_result);
        
        // Get table size using lua_objlen (Lua 5.1) or lua_rawlen (Lua 5.2+)
        size_t table_size = 0;
        #if LUA_VERSION_NUM >= 502
            table_size = lua_rawlen(L, abs_idx);
        #else
            table_size = lua_objlen(L, abs_idx);
        #endif
        
        // Iterate through table and add items
        lua_pushnil(L); // First key
        while (lua_next(L, abs_idx) != 0) {
            // Key is at index -2, value at index -1
            EseLuaValue *item = lua_value_create_nil(NULL);
            _lua_engine_convert_stack_to_luavalue(L, -1, item);
            
            // Add to table
            lua_value_push(out_result, item, false); // false = take ownership
            
            lua_pop(L, 1); // Remove value, keep key for next iteration
        }
    } else if (lua_isuserdata(L, abs_idx)) {
        lua_value_set_userdata(out_result, lua_touserdata(L, abs_idx));
    } else {
        // For other types (function, thread, etc.), just set as nil
        lua_value_set_nil(out_result);
    }
}

bool lua_engine_run_function_ref(EseLuaEngine *engine, int function_ref, int self_ref, int argc, EseLuaValue *argv[], EseLuaValue *out_result) {
    log_assert("LUA_ENGINE", engine, "lua_eng_run_func_ref called with NULL engine");

    profile_start(PROFILE_LUA_ENGINE_RUN_FUNCTION_REF);

    // Check if function reference is valid
    if (function_ref == LUA_NOREF) {
        profile_cancel(PROFILE_LUA_ENGINE_RUN_FUNCTION_REF);
        profile_count_add("lua_eng_run_func_ref_invalid_ref");
        return false;
    }

    lua_State *L = engine->runtime;

    // Function lookup timing
    profile_start(PROFILE_LUA_ENGINE_FUNCTION_LOOKUP);
    lua_rawgeti(L, LUA_REGISTRYINDEX, function_ref);
    profile_stop(PROFILE_LUA_ENGINE_FUNCTION_LOOKUP, "lua_eng_run_func_ref_lookup");
    
    // Stack: [function]

    // Entity proxy timing
    profile_start(PROFILE_LUA_ENGINE_FUNCTION_LOOKUP);
    lua_rawgeti(L, LUA_REGISTRYINDEX, self_ref); // push entity proxy as self
    profile_stop(PROFILE_LUA_ENGINE_FUNCTION_LOOKUP, "lua_eng_run_func_ref_entity_proxy");
    
    int n_args = 1; // self

    // Argument conversion timing
    profile_start(PROFILE_LUA_ENGINE_ARG_CONVERSION);
    for (int i = 0; i < argc; ++i) {
        _lua_engine_push_luavalue(L, argv[i]);
        n_args++;
    }
    profile_stop(PROFILE_LUA_ENGINE_ARG_CONVERSION, "lua_eng_run_func_ref_arg_conversion");

    profile_start(PROFILE_LUA_ENGINE_HOOK_SETUP);
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
    profile_stop(PROFILE_LUA_ENGINE_HOOK_SETUP, "lua_eng_run_func_ref_hook_setup");

    // Lua execution timing
    profile_start(PROFILE_LUA_ENGINE_LUA_EXECUTION);
    bool ok = true;
    int n_results = out_result ? 1 : 0; // Expect 1 result if out_result is provided
    if (lua_pcall(L, n_args, n_results, 0) != LUA_OK) {
        // Grab the error message
        const char *error_message = lua_tostring(L, -1);
        lua_pop(L, 1); // error message
        
        // Log the error first
        log_error("LUA_ENGINE", "(lua_engine_run_function_ref) Error running function: %s", error_message);
        
        // Try to add to console if engine is available
        lua_pushlightuserdata(L, (void*)ENGINE_KEY);
        lua_gettable(L, LUA_REGISTRYINDEX);
        if (lua_islightuserdata(L, -1)) {
            EseEngine *engine = (EseEngine *)lua_touserdata(L, -1);
            lua_pop(L, 1);
            
            if (engine) {
                engine_add_to_console(engine, ESE_CONSOLE_ERROR, "LUA", error_message);
                engine_show_console(engine, true);
            }
        } else {
            lua_pop(L, 1);
        }
        ok = false;
    } else if (out_result) {
        // Result conversion timing
        profile_start(PROFILE_LUA_ENGINE_RESULT_CONVERSION);
        // Convert the result to EseLuaValue
        _lua_engine_convert_stack_to_luavalue(L, -1, out_result);
        profile_stop(PROFILE_LUA_ENGINE_RESULT_CONVERSION, "lua_eng_run_func_ref_result_conversion");
        lua_pop(L, 1); // pop the result
    }
    profile_stop(PROFILE_LUA_ENGINE_LUA_EXECUTION, "lua_eng_run_func_ref_execution");

    // Hook cleanup timing - DISABLED FOR TESTING
    profile_start(PROFILE_LUA_ENGINE_HOOK_CLEANUP);
    lua_sethook(L, NULL, 0, 0);
    profile_stop(PROFILE_LUA_ENGINE_HOOK_CLEANUP, "lua_eng_run_func_ref_hook_cleanup");

    if (ok) {
        profile_stop(PROFILE_LUA_ENGINE_RUN_FUNCTION_REF, "lua_eng_run_func_ref");
        profile_count_add("lua_eng_run_func_ref_success");
    } else {
        profile_cancel(PROFILE_LUA_ENGINE_RUN_FUNCTION_REF);
        profile_count_add("lua_eng_run_func_ref_failed");
    }

    // No need to pop anything else; stack is clean
    return ok;
}

bool lua_engine_run_function(EseLuaEngine *engine, int instance_ref, int self_ref, const char *func_name, int argc, EseLuaValue *argv, EseLuaValue *out_result) {
    log_assert("LUA_ENGINE", engine, "lua_eng_run_function called with NULL engine");
    log_assert("LUA_ENGINE", func_name, "lua_eng_run_function called with NULL func_name");

    profile_start(PROFILE_LUA_ENGINE_RUN_FUNCTION);

    lua_State *L = engine->runtime;
    
    // OPTIMIZATION: Function caching - check if we have a cached function reference
    int cached_func_ref = -1;
    char cache_key[256];
    snprintf(cache_key, sizeof(cache_key), "func_cache_%d_%s", instance_ref, func_name);
    
    // Try to get cached function from registry
    lua_pushstring(L, cache_key);
    lua_gettable(L, LUA_REGISTRYINDEX);
    if (lua_isfunction(L, -1)) {
        // Found cached function!
        cached_func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        log_debug("LUA_ENGINE", "Using cached function for '%s'", func_name);
    } else {
        lua_pop(L, 1); // pop nil or non-function
        log_debug("LUA_ENGINE", "No cached function found for '%s'", func_name);
    }
    
    // Validate stack state before starting
    int initial_stack_size = lua_gettop(L);
    log_debug("LUA_ENGINE", "Stack size before function call: %d", initial_stack_size);
    
    // CRITICAL FIX: Ensure stack is clean before starting
    if (initial_stack_size != 0) {
        log_warn("LUA_ENGINE", "Stack not clean! Found %d items on stack before function call", initial_stack_size);
        log_debug("LUA_ENGINE", "Cleaning up stack items:");
        for (int i = 1; i <= initial_stack_size; i++) {
            log_debug("LUA_ENGINE", "  Removing Stack[%d]: %s", i, lua_typename(L, lua_type(L, i)));
        }
        lua_settop(L, 0); // Clean the stack
        initial_stack_size = 0;
        log_debug("LUA_ENGINE", "Stack cleaned. New stack size: %d", lua_gettop(L));
    }

    // Function lookup timing
    profile_start(PROFILE_LUA_ENGINE_FUNCTION_LOOKUP);
    
    if (cached_func_ref == -1) {
        // Need to look up the function
        if (!_lua_engine_instance_get_function(L, instance_ref, func_name)) {
            log_debug("LUA_ENGINE", "Function '%s' not found in instance %d", func_name, instance_ref);
            profile_cancel(PROFILE_LUA_ENGINE_FUNCTION_LOOKUP);
            profile_cancel(PROFILE_LUA_ENGINE_RUN_FUNCTION);
            profile_count_add("lua_eng_run_func_function_not_found");
            return false;
        }
        
        // Cache the function for future calls
        lua_pushstring(L, cache_key);
        lua_pushvalue(L, -2); // duplicate function
        lua_settable(L, LUA_REGISTRYINDEX);
        
        // Store the function reference for fast access
        // The function is now at the top of the stack
        cached_func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        
        log_debug("LUA_ENGINE", "Cached function '%s' for future calls (ref: %d)", func_name, cached_func_ref);
        
        // Verify the cached function is actually a function
        lua_rawgeti(L, LUA_REGISTRYINDEX, cached_func_ref);
        if (lua_isfunction(L, -1)) {
            log_debug("LUA_ENGINE", "Verified cached function is a function");
            // The function is now at the top of the stack and will be used for the call
        } else {
            log_error("LUA_ENGINE", "Cached value is not a function! Type: %s", lua_typename(L, lua_type(L, -1)));
            lua_pop(L, 1); // Only pop if verification failed
        }
    } else {
        // Use cached function
        log_debug("LUA_ENGINE", "Retrieving cached function (ref: %d)", cached_func_ref);
        lua_rawgeti(L, LUA_REGISTRYINDEX, cached_func_ref);
        
        // Verify what we actually got
        if (lua_isfunction(L, -1)) {
            log_debug("LUA_ENGINE", "Successfully retrieved cached function");
            // The function is now at the top of the stack and will be used for the call
        } else {
            log_error("LUA_ENGINE", "Failed to retrieve cached function! Got type: %s", lua_typename(L, lua_type(L, -1)));
            // Fall back to standard lookup
            lua_pop(L, 1); // pop the wrong value
            if (!_lua_engine_instance_get_function(L, instance_ref, func_name)) {
                log_debug("LUA_ENGINE", "Function '%s' not found in instance %d", func_name, instance_ref);
                profile_cancel(PROFILE_LUA_ENGINE_FUNCTION_LOOKUP);
                profile_cancel(PROFILE_LUA_ENGINE_RUN_FUNCTION);
                profile_count_add("lua_eng_run_func_function_not_found");
                return false;
            }
        }
    }
    
    profile_stop(PROFILE_LUA_ENGINE_FUNCTION_LOOKUP, "lua_eng_run_func_lookup");

    // Stack: [function]
    log_debug("LUA_ENGINE", "Stack after function lookup: %d", lua_gettop(L));

    // Argument setup timing
    profile_start(PROFILE_LUA_ENGINE_ARG_CONVERSION);
    
    // The function is currently at the top of the stack, so we need to reorder:
    // 1. Move function to bottom (Stack[1])
    // 2. Push arguments above it
    
    // DEBUG: Show stack before reordering
    log_debug("LUA_ENGINE", "Stack before reordering:");
    for (int i = 1; i <= lua_gettop(L); i++) {
        log_debug("LUA_ENGINE", "  Stack[%d]: %s", i, lua_typename(L, lua_type(L, i)));
    }
    
    // First, move the function to the bottom of the stack
    lua_insert(L, 1);
    
    // DEBUG: Show stack after reordering (function should be at Stack[1]):
    log_debug("LUA_ENGINE", "Stack after reordering (function should be at Stack[1]):");
    for (int i = 1; i <= lua_gettop(L); i++) {
        log_debug("LUA_ENGINE", "  Stack[%d]: %s", i, lua_typename(L, lua_type(L, i)));
    }
    
    // Now push the entity proxy as the "self" argument above the function
    lua_rawgeti(L, LUA_REGISTRYINDEX, self_ref); // push entity proxy as self
    int n_args = 1; // self

    // Add args to stack above the self argument
    for (int i = 0; i < argc; ++i) {
        _lua_engine_push_luavalue(L, &argv[i]);
        n_args++;
    }
    
    profile_stop(PROFILE_LUA_ENGINE_ARG_CONVERSION, "lua_eng_run_func_arg_setup");

    log_debug("LUA_ENGINE", "Stack before pcall: %d (function + %d args)", lua_gettop(L), n_args);

    // Hook setup timing
    profile_start(PROFILE_LUA_ENGINE_HOOK_SETUP);
    
    // Setup timeout (security feature - always enabled for safety)
    LuaFunctionHook timeout;
    timeout.start_time = clock();
    timeout.call_count = 0;
    timeout.instruction_count = 0;
    timeout.max_execution_time = engine->internal->max_execution_time;
    timeout.max_instruction_count = engine->internal->max_instruction_count;

    // Set the hook with the timeout as upvalue (security requirement)
    lua_pushlightuserdata(L, &timeout);
    lua_setfield(L, LUA_REGISTRYINDEX, LUA_HOOK_KEY);
    lua_sethook(L, _lua_engine_function_hook, LUA_MASKCOUNT, LUA_HOOK_FRQ);
    
    profile_stop(PROFILE_LUA_ENGINE_HOOK_SETUP, "lua_eng_run_func_hook_setup");

    bool ok = true;
    int n_results = out_result ? 1 : 0; // Expect 1 result if out_result is provided
    
    // CRITICAL: Add detailed stack debugging around pcall
    log_debug("LUA_ENGINE", "About to call lua_pcall with %d args, expecting %d results", n_args, n_results);
    log_debug("LUA_ENGINE", "Stack contents before pcall:");
    for (int i = 1; i <= lua_gettop(L); i++) {
        log_debug("LUA_ENGINE", "  Stack[%d]: %s", i, lua_typename(L, lua_type(L, i)));
    }
    
    // Lua execution timing
    profile_start(PROFILE_LUA_ENGINE_LUA_EXECUTION);
    
    // OPTIMIZATION: Direct function call bypass for maximum performance
    // Skip the engine wrapper and call the function directly
    int pcall_result = LUA_OK; // Declare here for both paths
    
    if (cached_func_ref != -1 && argc == 0 && !out_result) {
        // Fast path: direct call with minimal overhead
        log_debug("LUA_ENGINE", "Using fast path: direct function call");
        
        // DEBUG: Show stack before reordering in fast path
        log_debug("LUA_ENGINE", "Fast path stack before reordering:");
        for (int i = 1; i <= lua_gettop(L); i++) {
            log_debug("LUA_ENGINE", "  Stack[%d]: %s", i, lua_typename(L, lua_type(L, i)));
        }
        
        // CRITICAL FIX: Function is already at Stack[1], no need to reorder
        // The function is correctly positioned for lua_pcall
        
        // DEBUG: Show stack in fast path (no reordering needed)
        log_debug("LUA_ENGINE", "Fast path stack (function already at Stack[1]):");
        for (int i = 1; i <= lua_gettop(L); i++) {
            log_debug("LUA_ENGINE", "  Stack[%d]: %s", i, lua_typename(L, lua_type(L, i)));
        }
        
        // Push self argument above the function
        lua_rawgeti(L, LUA_REGISTRYINDEX, self_ref);
        
        // CRITICAL FIX: Calculate actual argument count for fast path
        // We have: Stack[1] = function, Stack[2] = self, Stack[3+] = additional args
        int fast_path_args = lua_gettop(L) - 1;
        log_debug("LUA_ENGINE", "Fast path calling with %d arguments", fast_path_args);
        
        // Call function directly with correct argument count
        pcall_result = lua_pcall(L, fast_path_args, 0, 0);
        
        if (pcall_result != LUA_OK) {
            // Grab the error message
            const char *error_message = lua_tostring(L, -1);
            lua_pop(L, 1); // error message
            
            // Log the error first
            log_error("LUA_ENGINE", "Fast path error: %s", error_message);
            
            // Try to add to console if engine is available
            lua_pushlightuserdata(L, (void*)ENGINE_KEY);
            lua_gettable(L, LUA_REGISTRYINDEX);
            if (lua_islightuserdata(L, -1)) {
                EseEngine *engine = (EseEngine *)lua_touserdata(L, -1);
                lua_pop(L, 1);
                
                if (engine) {
                    engine_add_to_console(engine, ESE_CONSOLE_ERROR, "LUA", error_message);
                    engine_show_console(engine, true);
                }
            } else {
                lua_pop(L, 1);
            }
            
            profile_cancel(PROFILE_LUA_ENGINE_RUN_FUNCTION);
            profile_count_add("lua_eng_run_func_failed");
            ok = false;
            return ok;
        }
    } else {
        // Standard path: use engine wrapper
        log_debug("LUA_ENGINE", "Using standard path: engine wrapper");
        
        // CRITICAL FIX: Use the n_args variable calculated during argument setup
        // n_args already includes both self and actual arguments, which is what lua_pcall expects
        pcall_result = lua_pcall(L, n_args, n_results, 0);
        
        if (pcall_result != LUA_OK) {
            // Grab the error message
            const char *error_message = lua_tostring(L, -1);
            lua_pop(L, 1); // error message
            
            // Log the error first
            log_error("LUA_ENGINE", "(lua_engine_run_function) Standard path, Error running function '%s': %s", func_name, error_message);
            
            // Try to add to console if engine is available
            lua_pushlightuserdata(L, (void*)ENGINE_KEY);
            lua_gettable(L, LUA_REGISTRYINDEX);
            if (lua_islightuserdata(L, -1)) {
                EseEngine *engine = (EseEngine *)lua_touserdata(L, -1);
                lua_pop(L, 1);
                
                if (engine) {
                    engine_add_to_console(engine, ESE_CONSOLE_ERROR, "LUA", error_message);
                    engine_show_console(engine, true);
                }
            } else {
                lua_pop(L, 1);
            }
            
            ok = false;
        } else if (out_result) {
            // Convert the result to EseLuaValue
            _lua_engine_convert_stack_to_luavalue(L, -1, out_result);
            lua_pop(L, 1); // pop the result
        } else {
            // No out_result provided, but function may have returned values
            // We need to pop any return values to keep the stack clean
            // The stack should be back to initial_stack_size after popping return values
            int current_stack_size = lua_gettop(L);
            int expected_stack_size = initial_stack_size;
            int return_values = current_stack_size - expected_stack_size;
            
            log_debug("LUA_ENGINE", "Stack cleanup: current=%d, expected=%d, return_values=%d", 
                      current_stack_size, expected_stack_size, return_values);
            
            if (return_values > 0) {
                log_debug("LUA_ENGINE", "Function returned %d values, popping them", return_values);
                lua_pop(L, return_values);
                
                // Verify stack is now clean
                int final_stack_size = lua_gettop(L);
                log_debug("LUA_ENGINE", "After popping return values, stack size: %d", final_stack_size);
            } else if (return_values < 0) {
                log_warn("LUA_ENGINE", "Stack corruption detected! Expected %d, got %d", expected_stack_size, current_stack_size);
            }
        }
    }
    
    profile_stop(PROFILE_LUA_ENGINE_LUA_EXECUTION, "lua_eng_run_func_execution");
    
    log_debug("LUA_ENGINE", "lua_pcall returned: %d", pcall_result);
    log_debug("LUA_ENGINE", "Stack size after pcall: %d", lua_gettop(L));
    
    if (pcall_result != LUA_OK) {
        // Grab the error message
        const char *error_message = lua_tostring(L, -1);
        lua_pop(L, 1); // error message
        
        // Log the error first
        log_error("LUA_ENGINE", "(lua_engine_run_function) Fast path Error running function '%s': %s", func_name, error_message);
        
        // Try to add to console if engine is available
        lua_pushlightuserdata(L, (void*)ENGINE_KEY);
        lua_gettable(L, LUA_REGISTRYINDEX);
        if (lua_islightuserdata(L, -1)) {
            EseEngine *engine = (EseEngine *)lua_touserdata(L, -1);
            lua_pop(L, 1);
            
            if (engine) {
                engine_add_to_console(engine, ESE_CONSOLE_ERROR, "LUA", error_message);
                engine_show_console(engine, true);
            }
        } else {
            lua_pop(L, 1);
        }
        
        ok = false;
    } else if (out_result) {
        // Convert the result to EseLuaValue
        _lua_engine_convert_stack_to_luavalue(L, -1, out_result);
        lua_pop(L, 1); // pop the result
    } else {
        // No out_result provided, but function may have returned values
        // We need to pop any return values to keep the stack clean
        // The stack should be back to initial_stack_size after popping return values
        int current_stack_size = lua_gettop(L);
        int expected_stack_size = initial_stack_size;
        int return_values = current_stack_size - expected_stack_size;
        
        log_debug("LUA_ENGINE", "Stack cleanup: current=%d, expected=%d, return_values=%d", 
                  current_stack_size, expected_stack_size, return_values);
        
        if (return_values > 0) {
            log_debug("LUA_ENGINE", "Function returned %d values, popping them", return_values);
            lua_pop(L, return_values);
            
            // Verify stack is now clean
            int final_stack_size = lua_gettop(L);
            log_debug("LUA_ENGINE", "After popping return values, stack size: %d", final_stack_size);
        } else if (return_values < 0) {
            log_warn("LUA_ENGINE", "Stack corruption detected! Expected %d, got %d", expected_stack_size, current_stack_size);
        }
    }

    // Hook cleanup timing 
    profile_start(PROFILE_LUA_ENGINE_HOOK_CLEANUP);
    
    // Remove the hook only if we set one
    if (true) { // Always remove hooks for security
        lua_sethook(L, NULL, 0, 0);
    }
    profile_stop(PROFILE_LUA_ENGINE_HOOK_CLEANUP, "lua_eng_run_func_hook_cleanup");

    // log_debug("LUA_ENGINE", "Stats: call count = %d  instruction count = %zu", timeout.call_count, timeout.call_count);

    // Ensure stack is clean after function call
    int final_stack_size = lua_gettop(L);
    log_debug("LUA_ENGINE", "Stack size after function call: %d", final_stack_size);
    
    if (final_stack_size != 0) {
        log_warn("LUA_ENGINE", "Stack not clean after function call! Cleaning up...");
        lua_settop(L, 0);
    }
    
    // Validate final stack state
    int cleaned_stack_size = lua_gettop(L);
    if (cleaned_stack_size != 0) {
        log_error("LUA_ENGINE", "Failed to clean stack! Final size: %d", cleaned_stack_size);
    }

    if (ok) {
        profile_stop(PROFILE_LUA_ENGINE_RUN_FUNCTION, "lua_eng_run_func");
        profile_count_add("lua_eng_run_func_success");
    } else {
        profile_cancel(PROFILE_LUA_ENGINE_RUN_FUNCTION);
        profile_count_add("lua_eng_run_func_failed");
    }

    return ok;
}


int lua_isinteger_lj(lua_State *L, int idx) {
    log_assert("LUA_ENGINE", L, "lua_isinteger_lj called with NULL Lua state");
    
    if (!lua_isnumber(L, idx)) return 0;
    lua_Number n = lua_tonumber(L, idx);
    lua_Integer i = (lua_Integer)n;
    return (n == (lua_Number)i);
}

// Unique key for registry storage
static const char *LUA_EXTRASPACE_KEY = "lua_extraspace_lj";
void *lua_getextraspace_lj(lua_State *L) {
    log_assert("LUA_ENGINE", L, "lua_getextraspace_lj called with NULL Lua state");
    
    void *p;

    // Check if already allocated
    lua_pushlightuserdata(L, (void*)LUA_EXTRASPACE_KEY);
    lua_gettable(L, LUA_REGISTRYINDEX);
    p = lua_touserdata(L, -1);
    lua_pop(L, 1);

    if (!p) {
        // Allocate space (just sizeof(void*), like Lua 5.3 guarantees)
        p = lua_newuserdata(L, sizeof(void*));
        *(void**)p = NULL; // initialize to NULL

        // Store in registry
        lua_pushlightuserdata(L, (void*)LUA_EXTRASPACE_KEY);
        lua_pushvalue(L, -2); // copy userdata
        lua_settable(L, LUA_REGISTRYINDEX);

        lua_pop(L, 1); // pop userdata
    }

    return p;
}