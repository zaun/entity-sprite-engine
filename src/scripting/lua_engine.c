#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
#include "utility/log.h"
#include "utility/hashmap.h"
#include "utility/profile.h"
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

    engine->memory_limit = 1024 * 1024 * 10; // 10MB
    engine->memory_used = 0;

    // Set default limits
    engine->max_execution_time = (clock_t)((10) * CLOCKS_PER_SEC); // 10 second timeout protection
    engine->max_instruction_count = 4000000; // 4M instruction limit for security

    // Initialize Lua runtime
    engine->L = lua_newstate(_lua_engine_limited_alloc, engine);
    if (!engine->L) {
        log_error("LUA_ENGINE", "Failed to create Lua runtime");
        memory_manager.free(engine);
        return NULL;
    }

    engine->functions = hashmap_create((EseHashMapFreeFn)memory_manager.free);

    
    // Log on lua panic
    lua_atpanic(engine->L, my_panic);
    if (setjmp(g_lua_panic_jmp) != 0) {
        log_error("LUA_ENGINE", "Recovered from Lua panic");
        return NULL;
    }

    // Load all standard libraries including JIT
    luaL_openlibs(engine->L);
    
    // Enable JIT compiler (it's disabled by default)
    lua_getglobal(engine->L, "jit");
    if (lua_istable(engine->L, -1)) {
        lua_getfield(engine->L, -1, "on");
        if (lua_isfunction(engine->L, -1)) {
            // Use pcall to safely enable JIT
            int call_result = lua_pcall(engine->L, 0, 0, 0);
            if (call_result == LUA_OK) {
                log_debug("LUA_ENGINE", "JIT compiler enabled");
            } else {
                const char* error_msg = lua_tostring(engine->L, -1);
                log_error("LUA_ENGINE", "Failed to enable JIT: %s", error_msg ? error_msg : "unknown error");
                lua_pop(engine->L, 1);
            }
        } else {
            log_error("LUA_ENGINE", "JIT on function not available");
            lua_pop(engine->L, 1);
        }
    } else {
        log_error("LUA_ENGINE", "JIT table not available");
    }
    lua_pop(engine->L, 1);
    
    // Verify JIT is loaded and working
    lua_getglobal(engine->L, "jit");
    if (lua_istable(engine->L, -1)) {
        log_debug("LUA_ENGINE", "JIT library loaded successfully");
        
        // Check JIT status (simple check - just verify the function exists)
        lua_getfield(engine->L, -1, "status");
        if (lua_isfunction(engine->L, -1)) {
            log_debug("LUA_ENGINE", "JIT Status: available");
        } else {
            log_debug("LUA_ENGINE", "JIT status field is not a function");
        }
        lua_pop(engine->L, 1);
        
        // Check additional JIT info
        lua_getfield(engine->L, -1, "version");
        if (lua_isstring(engine->L, -1)) {
            log_debug("LUA_ENGINE", "JIT Version: %s", lua_tostring(engine->L, -1));
        }
        lua_pop(engine->L, 1);
        
        lua_getfield(engine->L, -1, "os");
        if (lua_isstring(engine->L, -1)) {
            log_debug("LUA_ENGINE", "JIT OS: %s", lua_tostring(engine->L, -1));
        }
        lua_pop(engine->L, 1);
        
        lua_getfield(engine->L, -1, "arch");
        if (lua_isstring(engine->L, -1)) {
            log_debug("LUA_ENGINE", "JIT Arch: %s", lua_tostring(engine->L, -1));
        }
        lua_pop(engine->L, 1);
        
        // Check JIT profiling
        lua_getfield(engine->L, -1, "profile");
        if (lua_isfunction(engine->L, -1)) {
            log_debug("LUA_ENGINE", "JIT profile function available");
        } else {
            log_debug("LUA_ENGINE", "JIT profile field is not a function");
        }
        lua_pop(engine->L, 1);
    } else {
        log_debug("LUA_ENGINE", "JIT library failed to load!");
    }
    lua_pop(engine->L, 1);

    // Remove dangerous functions
    lua_pushnil(engine->L);
    lua_setglobal(engine->L, "dofile");
    lua_pushnil(engine->L);
    lua_setglobal(engine->L, "loadfile");
    lua_pushnil(engine->L);
    lua_setglobal(engine->L, "require");

    // Create the entity_list table and store in registry
    lua_newtable(engine->L);

    // Set as global first
    lua_pushvalue(engine->L, -1);  // Copy table
    lua_setglobal(engine->L, "entity_list");

    // Then store the same table in registry
    lua_pushlightuserdata(engine->L, ENTITY_LIST_KEY);
    lua_pushvalue(engine->L, -2);  // Copy table again
    lua_settable(engine->L, LUA_REGISTRYINDEX);  // registry[ENTITY_LIST_KEY] = table

    // Clean up stack
    lua_pop(engine->L, 1);

    // Create script_sandbox_master, copy only explicitly safe keys.
    lua_newtable(engine->L);                 // [master]
    int master_idx = lua_gettop(engine->L);

    lua_getglobal(engine->L, "_G");          // [master, _G]
    int g_idx = lua_gettop(engine->L);

    _lua_copy_field(engine->L, g_idx, master_idx, "assert");
    _lua_copy_field(engine->L, g_idx, master_idx, "pairs");
    _lua_copy_field(engine->L, g_idx, master_idx, "ipairs");
    _lua_copy_field(engine->L, g_idx, master_idx, "next");
    _lua_copy_field(engine->L, g_idx, master_idx, "type");
    _lua_copy_field(engine->L, g_idx, master_idx, "tostring");
    _lua_copy_field(engine->L, g_idx, master_idx, "tonumber");
    _lua_copy_field(engine->L, g_idx, master_idx, "select");
    _lua_copy_field(engine->L, g_idx, master_idx, "pcall");
    _lua_copy_field(engine->L, g_idx, master_idx, "xpcall");
    _lua_copy_field(engine->L, g_idx, master_idx, "math");
    _lua_copy_field(engine->L, g_idx, master_idx, "string");
    _lua_copy_field(engine->L, g_idx, master_idx, "table");
    _lua_copy_field(engine->L, g_idx, master_idx, "print");
    _lua_copy_field(engine->L, g_idx, master_idx, "_VERSION");
    _lua_copy_field(engine->L, g_idx, master_idx, "jit");

    lua_pushvalue(engine->L, master_idx);
    lua_setfield(engine->L, master_idx, "_G");

    engine->sandbox_master_ref = luaL_ref(engine->L, LUA_REGISTRYINDEX);

    lua_gc(engine->L, LUA_GCRESTART, 0);

    // Pop _G
    lua_pop(engine->L, 1);
    log_debug("LUA_ENGINE", "Lua engine created successfully");

    return engine;
}

void lua_engine_destroy(EseLuaEngine *engine) {
    log_assert("LUA_ENGINE", engine, "engine_destroy called with NULL engine");

    // Need to iterate and memory_manager.free all script references
    hashmap_free(engine->functions);

    if (engine->sandbox_master_ref != LUA_NOREF) {
        luaL_unref(engine->L, LUA_REGISTRYINDEX, engine->sandbox_master_ref);
    }

    lua_close(engine->L);

    memory_manager.free(engine);
    memory_manager.free(engine);
}

void lua_engine_global_lock(EseLuaEngine *engine) {
    log_assert("LUA_ENGINE", engine, "lua_eng_global_lock called with NULL engine");
    
    lua_State *L = engine->L;

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
    if (engine->sandbox_master_ref != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, engine->sandbox_master_ref);
        lua_newtable(L);
        lua_pushstring(L, "locked");
        lua_setfield(L, -2, "__metatable");
        lua_setmetatable(L, -2);
        lua_pop(L, 1);
    }
}

void lua_engine_gc(EseLuaEngine *engine) {
    log_assert("LUA_ENGINE", engine, "lua_eng_gc called with NULL engine");
    
    lua_gc(engine->L, LUA_GCSTEP, 0);
}

void lua_engine_add_registry_key(EseLuaEngine *engine, const void *key, void *ptr) {
    log_assert("LUA_ENGINE", engine->L, "lua_eng_add_registry_key called with invalid engine");
    lua_pushlightuserdata(engine->L, (void *)key);  // push registry key
    lua_pushlightuserdata(engine->L, ptr);          // push the pointer value
    lua_settable(engine->L, LUA_REGISTRYINDEX);     // registry[key] = ptr
}

void *lua_engine_get_registry_key(EseLuaEngine *engine, const void *key) {
    log_assert("LUA_ENGINE", engine->L, "lua_eng_get_registry_key called with invalid engine");
    void *result;
    lua_pushlightuserdata(engine->L, (void *)key);  // push registry key
    lua_gettable(engine->L, LUA_REGISTRYINDEX);     // pushes registry[key]
    result = lua_touserdata(engine->L, -1);         // get lightuserdata
    lua_pop(engine->L, 1);                          // pop the value
    return result;
}

void lua_engine_remove_registry_key(EseLuaEngine *engine, const void *key) {
    log_assert("LUA_ENGINE", engine->L, "lua_eng_add_registry_key called with invalid engine");
    lua_pushlightuserdata(engine->L, (void *)key);
    lua_pushnil(engine->L);
    lua_settable(engine->L, LUA_REGISTRYINDEX);
}


void lua_engine_add_function(EseLuaEngine *engine, const char *function_name, EseLuaCFunction func) {
    log_assert("LUA_ENGINE", engine, "lua_eng_add_function called with NULL engine");
    log_assert("LUA_ENGINE", function_name, "lua_eng_add_function called with NULL function_name");
    log_assert("LUA_ENGINE", func, "lua_eng_add_function called with NULL func");
    log_assert("LUA_ENGINE", engine->sandbox_master_ref != LUA_NOREF, "lua_eng_add_function engine->sandbox_master_ref is LUA_NOREF");

    // Add wrapper function to script_sandbox_master with user function as upvalue
    lua_rawgeti(engine->L, LUA_REGISTRYINDEX, engine->sandbox_master_ref); // master
    lua_pushlightuserdata(engine->L, (void*)func); // push user function as upvalue
    lua_pushcclosure(engine->L, _lua_engine_wrapper, 1); // create closure with 1 upvalue
    lua_setfield(engine->L, -2, function_name);
    lua_pop(engine->L, 1); // master

    log_debug("LUA_ENGINE", "Added C function '%s' to Lua.", function_name);
}

void lua_engine_add_metatable(EseLuaEngine *engine, const char *name, EseLuaCFunction index_func, EseLuaCFunction newindex_func, EseLuaCFunction gc_func, EseLuaCFunction tostring_func) {
    log_assert("LUA_ENGINE", engine, "lua_engine_add_metatable called with NULL engine");
    log_assert("LUA_ENGINE", name, "lua_engine_add_metatable called with NULL name");
    log_assert("LUA_ENGINE", index_func, "lua_engine_add_metatable called with NULL index_func");
    log_assert("LUA_ENGINE", newindex_func, "lua_engine_add_metatable called with NULL newindex_func");
    log_assert("LUA_ENGINE", gc_func, "lua_engine_add_metatable called with NULL gc_func");
    log_assert("LUA_ENGINE", tostring_func, "lua_engine_add_metatable called with NULL tostring_func");

    if (luaL_newmetatable(engine->L, name)) {
        log_debug("LUA", "Adding metatable '%s' to engine", name);
        
        // Set __name
        lua_pushstring(engine->L, name);
        lua_setfield(engine->L, -2, "__name");
        
        // Set __index
        lua_pushlightuserdata(engine->L, (void*)index_func);
        lua_pushcclosure(engine->L, _lua_engine_wrapper, 1);
        lua_setfield(engine->L, -2, "__index");
        
        // Set __newindex
        lua_pushlightuserdata(engine->L, (void*)newindex_func);
        lua_pushcclosure(engine->L, _lua_engine_wrapper, 1);
        lua_setfield(engine->L, -2, "__newindex");
        
        // Set __gc
        lua_pushlightuserdata(engine->L, (void*)gc_func);
        lua_pushcclosure(engine->L, _lua_engine_wrapper, 1);
        lua_setfield(engine->L, -2, "__gc");
        
        // Set __tostring
        lua_pushlightuserdata(engine->L, (void*)tostring_func);
        lua_pushcclosure(engine->L, _lua_engine_wrapper, 1);
        lua_setfield(engine->L, -2, "__tostring");
        
        // Set __metatable to "locked"
        lua_pushstring(engine->L, "locked");
        lua_setfield(engine->L, -2, "__metatable");
    }
    lua_pop(engine->L, 1);
    
    log_debug("LUA_ENGINE", "Added metatable '%s' to Lua.", name);
}

void lua_engine_add_globaltable(EseLuaEngine *engine, const char *name, size_t argc, const char **function_names, EseLuaCFunction *functions) {
    log_assert("LUA_ENGINE", engine, "lua_engine_add_globaltable called with NULL engine");
    log_assert("LUA_ENGINE", name, "lua_engine_add_globaltable called with NULL name");
    log_assert("LUA_ENGINE", function_names, "lua_engine_add_globaltable called with NULL function_names");
    log_assert("LUA_ENGINE", functions, "lua_engine_add_globaltable called with NULL functions");

    // Check if the global table already exists
    lua_getglobal(engine->L, name);
    if (lua_isnil(engine->L, -1)) {
        lua_pop(engine->L, 1); // pop the nil
        log_debug("LUA", "Creating global table '%s'", name);
        
        // Create new table
        lua_newtable(engine->L);
        
        // Add all functions to the table
        for (size_t i = 0; i < argc; i++) {
            if (function_names[i] && functions[i]) {
                // Push the function as an upvalue and create a closure with the wrapper
                lua_pushlightuserdata(engine->L, (void*)functions[i]);
                lua_pushcclosure(engine->L, _lua_engine_wrapper, 1);
                lua_setfield(engine->L, -2, function_names[i]);
            }
        }
        
        // Set as global
        lua_setglobal(engine->L, name);
        
        log_debug("LUA_ENGINE", "Added global table '%s' with %zu functions to Lua.", name, argc);
    } else {
        lua_pop(engine->L, 1); // pop the existing table
        log_debug("LUA_ENGINE", "Global table '%s' already exists, skipping creation.", name);
    }
}

void* lua_engine_create_userdata(EseLuaEngine *engine, const char *metatable_name, size_t size) {
    log_assert("LUA_ENGINE", engine, "lua_engine_create_userdata called with NULL engine");
    log_assert("LUA_ENGINE", metatable_name, "lua_engine_create_userdata called with NULL metatable_name");
    log_assert("LUA_ENGINE", size > 0, "lua_engine_create_userdata called with size 0");

    // Create userdata with the specified size
    void *userdata = lua_newuserdata(engine->L, size);
    
    // Attach the metatable
    luaL_getmetatable(engine->L, metatable_name);
    lua_setmetatable(engine->L, -2);
    
    log_debug("LUA_ENGINE", "Created userdata with metatable '%s' and size %zu", metatable_name, size);
    
    return userdata;
}

void lua_engine_get_registry_value(EseLuaEngine *engine, int ref) {
    log_assert("LUA_ENGINE", engine, "lua_engine_get_registry_value called with NULL engine");
    log_assert("LUA_ENGINE", ref != LUA_NOREF, "lua_engine_get_registry_value called with LUA_NOREF ref");

    lua_rawgeti(engine->L, LUA_REGISTRYINDEX, ref);
    
    log_debug("LUA_ENGINE", "Retrieved registry value with ref %d", ref);
}

int lua_engine_get_reference(EseLuaEngine *engine) {
    log_assert("LUA_ENGINE", engine, "lua_engine_get_reference called with NULL engine");
    
    int ref = luaL_ref(engine->L, LUA_REGISTRYINDEX);
    
    log_debug("LUA_ENGINE", "Created reference %d", ref);
    return ref;
}

bool lua_engine_is_userdata(EseLuaEngine *engine, int idx) {
    log_assert("LUA_ENGINE", engine, "lua_engine_is_userdata called with NULL engine");
    
    return lua_isuserdata(engine->L, idx);
}

void* lua_engine_test_userdata(EseLuaEngine *engine, int idx, const char *metatable_name) {
    log_assert("LUA_ENGINE", engine, "lua_engine_test_userdata called with NULL engine");
    log_assert("LUA_ENGINE", metatable_name, "lua_engine_test_userdata called with NULL metatable_name");
    
    return luaL_testudata(engine->L, idx, metatable_name);
}

void lua_engine_add_global(EseLuaEngine *engine, const char *global_name, int lua_ref) {
    log_assert("LUA_ENGINE", engine, "lua_eng_add_global called with NULL engine");
    log_assert("LUA_ENGINE", global_name, "lua_eng_add_global called with NULL global_name");
    log_assert("LUA_ENGINE", lua_ref != LUA_NOREF, "lua_eng_add_global called with LUA_NOREF lua_ref");
    log_assert("LUA_ENGINE", engine->sandbox_master_ref != LUA_NOREF, "lua_eng_add_global engine->sandbox_master_ref is LUA_NOREF");

    // Add to script_sandbox_master
    lua_rawgeti(engine->L, LUA_REGISTRYINDEX, engine->sandbox_master_ref); // master
    lua_rawgeti(engine->L, LUA_REGISTRYINDEX, lua_ref);
    lua_setfield(engine->L, -2, global_name);
    lua_pop(engine->L, 1); // master
}

bool lua_engine_load_script(EseLuaEngine *engine, const char* filename, const char* module_name) {
    log_assert("LUA_ENGINE", engine, "lua_eng_load_script called with NULL engine");
    log_assert("LUA_ENGINE", filename, "lua_eng_load_script called with NULL filename");
    log_assert("LUA_ENGINE", module_name, "lua_eng_load_script called with NULL module_name");
    log_assert("LUA_ENGINE", engine->sandbox_master_ref != LUA_NOREF, "lua_eng_load_script engine->sandbox_master_ref is LUA_NOREF");

    profile_start(PROFILE_LUA_ENGINE_LOAD_SCRIPT);

    if (!filesystem_check_file(filename, ".lua")) {
        log_error("LUA_ENGINE", "Error: invalid %s", filename);
        profile_cancel(PROFILE_LUA_ENGINE_LOAD_SCRIPT);
        profile_count_add("lua_eng_load_script_failed");
        return false;
    }

    int *func = hashmap_get(engine->functions, filename);
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
    _lua_engine_build_env_from_master(engine->L, engine->sandbox_master_ref);
    int env_idx = lua_gettop(engine->L);

    // Create module table (STARTUP)
    lua_newtable(engine->L);
    lua_setfield(engine->L, env_idx, module_name);

    // Lock environment
    lua_newtable(engine->L);
    lua_pushcfunction(engine->L, _lua_global_write_error);
    lua_setfield(engine->L, -2, "__newindex");
    lua_pushstring(engine->L, "locked");
    lua_setfield(engine->L, -2, "__metatable");
    lua_setmetatable(engine->L, env_idx);

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

    if (luaL_loadbuffer(engine->L, wrapped, strlen(wrapped), chunkname) == LUA_OK) {
        // Push module table as argument
        lua_getfield(engine->L, env_idx, module_name);

        if (lua_pcall(engine->L, 1, 1, 0) == LUA_OK) {
            if (lua_istable(engine->L, -1)) {
                int script_ref = luaL_ref(engine->L, LUA_REGISTRYINDEX);
                int *ref = memory_manager.malloc(sizeof(int), MMTAG_LUA);
                *ref = script_ref;
                hashmap_set(engine->functions, name, ref);

                memory_manager.free(wrapped);
                profile_stop(PROFILE_LUA_ENGINE_LOAD_SCRIPT_STRING, "lua_eng_load_script_string");
                profile_count_add("lua_eng_load_script_string_success");
                return true;
            } else {
                log_error("LUA_ENGINE", "Script '%s' did not return a table", name);
                lua_pop(engine->L, 1);
            }
        } else {
            log_error("LUA_ENGINE", "Error executing script '%s': %s",
                name, lua_tostring(engine->L, -1));
            lua_pop(engine->L, 1);
        }
    } else {
        log_error("LUA_ENGINE", "Error loading script '%s': %s",
            name, lua_tostring(engine->L, -1));
        lua_pop(engine->L, 1);
    }

    lua_pop(engine->L, 1); // pop env
    memory_manager.free(wrapped);
    profile_cancel(PROFILE_LUA_ENGINE_LOAD_SCRIPT_STRING);
    profile_count_add("lua_eng_load_script_string_failed");
    return false;
}

int lua_engine_instance_script(EseLuaEngine *engine, const char *name) {
    log_assert("LUA_ENGINE", engine, "lua_eng_inst_script called with NULL engine");
    log_assert("LUA_ENGINE", name, "lua_eng_inst_script called with NULL name");

    profile_start(PROFILE_LUA_ENGINE_INSTANCE_SCRIPT);

    int *script_ref = hashmap_get(engine->functions, name);
    if (!script_ref) {
        log_error("LUA_ENGINE", "Script '%s' not found", name);
        profile_cancel(PROFILE_LUA_ENGINE_INSTANCE_SCRIPT);
        profile_count_add("lua_eng_inst_script_not_found");
        return -1;
    }

    lua_State *L = engine->L;

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

    lua_State *L = engine->L;

    luaL_unref(L, LUA_REGISTRYINDEX, instance_ref);
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

    lua_State *L = engine->L;

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
    timeout.max_execution_time = engine->max_execution_time;
    timeout.max_instruction_count = engine->max_instruction_count;

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

    lua_State *L = engine->L;
    
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
    timeout.max_execution_time = engine->max_execution_time;
    timeout.max_instruction_count = engine->max_instruction_count;

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

