#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
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
    engine->internal->max_execution_time = (clock_t)((10) * CLOCKS_PER_SEC); // 10 second
    engine->internal->max_instruction_count = 4000000; // 4M

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
        exit(1);
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
                log_debug("LUA_ENGINE", "Failed to enable JIT: %s", error_msg ? error_msg : "unknown error");
                lua_pop(engine->runtime, 1);
            }
        } else {
            log_debug("LUA_ENGINE", "JIT on function not available");
            lua_pop(engine->runtime, 1);
        }
    } else {
        log_debug("LUA_ENGINE", "JIT table not available");
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
        return true; // already loaded
    }

    char *full_path = filesystem_get_resource(filename);
    if (!full_path) {
        log_error("LUA_ENGINE", "Error: filesystem_get_resource failed for %s", filename);
        return false;
    }

    FILE* file = fopen(full_path, "r");
    if (!file) {
        log_error("LUA_ENGINE", "Error: Failed to open Lua script file '%s'", full_path);
        memory_manager.free(full_path);
        return false;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        log_error("LUA_ENGINE", "Error: Failed to get file size for '%s'", full_path);
        memory_manager.free(full_path);
        fclose(file);
        return false;
    }

    char* script = memory_manager.malloc(file_size + 1, MMTAG_LUA);
    size_t bytes_read = fread(script, 1, file_size, file);
    fclose(file);

    if (bytes_read != (size_t)file_size) {
        log_error("LUA_ENGINE", "Error: Failed to read complete Lua script from '%s'", full_path);
        memory_manager.free(script);
        memory_manager.free(full_path);
        return false;
    }
    script[file_size] = '\0';

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

    // Wrap script: LuaJIT-safe, returns module table
    const char *prologue_fmt = "local %s = ...\n";
    const char *epilogue_fmt = "\nreturn %s\n";

    size_t new_len = strlen(prologue_fmt) + strlen(module_name)
                   + strlen(script)
                   + strlen(epilogue_fmt) + strlen(module_name) + 1;

    char *wrapped = memory_manager.malloc(new_len, MMTAG_LUA);
    snprintf(wrapped, new_len, prologue_fmt, module_name);
    strcat(wrapped, script);
    char epilogue[256];
    snprintf(epilogue, sizeof(epilogue), epilogue_fmt, module_name);
    strcat(wrapped, epilogue);

    // Load chunk
    char chunkname[512];
    snprintf(chunkname, sizeof(chunkname), "@%s", filename ? filename : "unnamed");

    if (luaL_loadbuffer(engine->runtime, wrapped, strlen(wrapped), chunkname) == LUA_OK) {
        // Push module table as argument
        lua_getfield(engine->runtime, env_idx, module_name);

        if (lua_pcall(engine->runtime, 1, 1, 0) == LUA_OK) {
            if (lua_istable(engine->runtime, -1)) {
                int script_ref = luaL_ref(engine->runtime, LUA_REGISTRYINDEX);
                int *ref = memory_manager.malloc(sizeof(int), MMTAG_LUA);
                *ref = script_ref;
                hashmap_set(engine->internal->functions, filename, ref);

                memory_manager.free(full_path);
                memory_manager.free(script);
                memory_manager.free(wrapped);
                return true;
            } else {
                log_error("LUA_ENGINE", "Script '%s' did not return a table", filename);
                lua_pop(engine->runtime, 1);
            }
        } else {
            log_error("LUA_ENGINE", "Error executing script '%s': %s",
                      filename, lua_tostring(engine->runtime, -1));
            lua_pop(engine->runtime, 1);
        }
    } else {
        log_error("LUA_ENGINE", "Error loading script '%s': %s",
                  filename, lua_tostring(engine->runtime, -1));
        lua_pop(engine->runtime, 1);
    }

    lua_pop(engine->runtime, 1); // pop env
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
        log_error("LUA_ENGINE", "Script '%s' not found", filename);
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

bool lua_engine_run_function_ref(EseLuaEngine *engine, int function_ref, int self_ref, int argc, EseLuaValue *argv) {
    log_assert("LUA_ENGINE", engine, "lua_engine_run_function_ref called with NULL engine");

    // Check if function reference is valid
    if (function_ref == LUA_NOREF) {
        return false;
    }

    lua_State *L = engine->runtime;

    // Push function by reference (fast - no lookup needed)
    lua_rawgeti(L, LUA_REGISTRYINDEX, function_ref);
    
    // Stack: [function]

    // Push the entity proxy as the "self" argument
    lua_rawgeti(L, LUA_REGISTRYINDEX, self_ref); // push entity proxy as self
    int n_args = 1; // self

    // Add args to stack
    for (int i = 0; i < argc; ++i) {
        _lua_engine_push_luavalue(L, &argv[i]);
        n_args++;
    }

    // Setup timeout (security feature - kept from original)
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
        log_error("LUA_ENGINE", "Error running function by reference: %s", lua_tostring(L, -1));
        lua_pop(L, 1); // error message
        ok = false;
    }

    // Remove the hook
    lua_sethook(L, NULL, 0, 0);

    // log_debug("LUA_ENGINE", "Stats: call count = %d  instruction count = %zu", timeout.call_count, timeout.instruction_count);

    // No need to pop anything else; stack is clean
    return ok;
}

bool lua_engine_run_function(EseLuaEngine *engine, int instance_ref, int self_ref, const char *func_name, int argc, EseLuaValue *argv) {
    log_assert("LUA_ENGINE", engine, "lua_engine_run_function called with NULL engine");
    log_assert("LUA_ENGINE", func_name, "lua_engine_run_function called with NULL func_name");

    lua_State *L = engine->runtime;

    // Find the function in the script instance
    if (!_lua_engine_instance_get_function(L, instance_ref, func_name)) {
        // Function not found, just return false
        log_debug("LUA_ENGINE", "Function '%s' not found in instance %d", func_name, instance_ref);
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

    // Setup timeout (security feature - kept from original)
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
        log_error("LUA_ENGINE", "Error running function '%s': %s", func_name, lua_tostring(L, -1));
        lua_pop(L, 1); // error message
        ok = false;
    }

    // Remove the hook
    lua_sethook(L, NULL, 0, 0);

    // log_debug("LUA_ENGINE", "Stats: call count = %d  instruction count = %zu", timeout.call_count, timeout.instruction_count);

    // No need to pop anything else; stack is clean
    return ok;
}


int lua_isinteger_lj(lua_State *L, int idx) {
    if (!lua_isnumber(L, idx)) return 0;
    lua_Number n = lua_tonumber(L, idx);
    lua_Integer i = (lua_Integer)n;
    return (n == (lua_Number)i);
}

// Unique key for registry storage
static const char *LUA_EXTRASPACE_KEY = "lua_extraspace_lj";
void *lua_getextraspace_lj(lua_State *L) {
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