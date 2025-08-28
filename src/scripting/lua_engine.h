#ifndef ESE_LUA_ENGINE_H
#define ESE_LUA_ENGINE_H

#include <stdbool.h>

// Required for anyone useing the lua_engine
#include "../vendor/lua/src/lua.h"
#include "../vendor/lua/src/lauxlib.h"
#include "../vendor/lua/src/lualib.h"
#include "lua_value.h"

// Forward declarations
typedef struct EseLuaValue EseLuaValue;
typedef struct lua_State lua_State;
typedef struct EseLuaEngineInternal EseLuaEngineInternal;

/**
 * @brief Main interface for the Lua scripting engine.
 * 
 * @details This structure provides the public interface for Lua scripting
 *          functionality. It contains a Lua state for script execution
 *          and internal state for configuration and security management.
 *          The engine enforces memory limits, execution timeouts, and
 *          provides a sandboxed environment for safe script execution.
 */
typedef struct EseLuaEngine {
    lua_State *runtime;             /**< Lua state for script execution */
    EseLuaEngineInternal *internal; /**< Internal state and configuration */
} EseLuaEngine;

extern const char _ENGINE_SENTINEL;
#define ENGINE_KEY ((void *)&_ENGINE_SENTINEL)

extern const char _LUA_ENGINE_SENTINEL;
#define LUA_ENGINE_KEY ((void *)&_LUA_ENGINE_SENTINEL)

extern const char _ENTITY_LIST_KEY_SENTINEL;
#define ENTITY_LIST_KEY ((void *)&_ENTITY_LIST_KEY_SENTINEL)

typedef int (*lua_CFunction) (lua_State *L);

/**
 * @brief Creates and initializes a new EseLuaEngine instance with security restrictions.
 * 
 * @details Allocates a new EseLuaEngine, creates a Lua state with custom memory allocator,
 *          loads standard libraries (base, table, string, math), removes dangerous 
 *          functions (dofile, loadfile, require), replaces print with custom logging
 *          version, and sets memory/execution limits. The engine enforces a 10MB 
 *          memory limit, 10-second execution timeout, and 100,000 instruction limit.
 * 
 * @return Pointer to newly created EseLuaEngine instance on success, NULL on failure.
 * 
 * @warning The returned engine must be freed with lua_engine_destroy() to prevent memory leaks.
 */
EseLuaEngine *lua_engine_create();

/**
 * @brief Frees a EseLuaEngine instance and all associated resources.
 * 
 * @details Iterates through all loaded script references, unreferences them from
 *          the Lua registry, frees the EseHashMap containing function references,
 *          closes the Lua state, and frees the engine structure.
 * 
 * @param engine Pointer to the EseLuaEngine instance to free. Safe to pass NULL.
 */
void lua_engine_destroy(EseLuaEngine *engine);


void lua_engine_global_lock(EseLuaEngine *engine);

void lua_engine_gc(EseLuaEngine *engine);

/**
  * @brief Stores a Lua value (already on top of the stack) under a
  *        C-only key in the Lua registry.
  *
  * @param L      lua_State pointer
  * @param key    A unique key (e.g. address of a static sentinel).
  * @param ptr    Pointer to store
  */
 void lua_engine_add_registry_key(lua_State *L, const void *key, void *ptr);

 /**
  * @brief Retrieves the Lua value stored under the given key in the
  *        registry and pushes it onto the stack.
  *
  * @param L        lua_State pointer
  * @param key      The same key you used to add it.
  * @return         Stored pointer or NULL if none
  */
 void *lua_engine_get_registry_key(lua_State *L, const void *key);

 /**
  * @brief Removes the entry for the given key from the registry.
  *
  * @param L        lua_State pointer
  * @param key      The key whose value you want to delete.
  */
 void lua_engine_remove_registry_key(lua_State *L, const void *key);

void lua_engine_add_function(EseLuaEngine *engine, const char *function_name, lua_CFunction func);

void lua_engine_add_global(EseLuaEngine *engine, const char *global_name, int lua_ref);

/**
 * @brief Loads and compiles a Lua script file into the engine's function registry.
 * 
 * @details Validates the filename has .lua extension, resolves the full path,
 *          reads the entire file into memory, compiles it using luaL_loadstring,
 *          executes it expecting a table return value, and stores a registry
 *          reference in the engine's EseHashMap. The script is expected to return
 *          a class table that can be instantiated later.
 * 
 * @param engine Pointer to the EseLuaEngine instance.
 * @param filename Name of the Lua script file to load (must have .lua extension).
 * @param module_name Name of the module to load).
 * 
 * @return true if the script was successfully loaded and compiled, false on any error.
 * 
 * @warning Script files are read entirely into memory. Large files may cause issues.
 * @warning The script must return a table or loading will fail.
 */
bool lua_engine_load_script(EseLuaEngine *engine, const char* filename, const char* module_name);

/**
 * @brief Loads and compiles a Lua script from a string into the engine's function registry.
 * 
 * @details Validates the script string is not empty, compiles it using luaL_loadstring,
 *          executes it expecting a table return value, and stores a registry
 *          reference in the engine's EseHashMap. The script is expected to return
 *          a class table that can be instantiated later.
 * 
 * @param engine Pointer to the EseLuaEngine instance.
 * @param script String containing the Lua script to load.
 * @param name Name of the script (used for error messages and lookup).
 * @param module_name Name of the module to load).
 * 
 * @return true if the script was successfully loaded and compiled, false on any error.
 * 
 * @warning The script must return a table or loading will fail.
 */
bool lua_engine_load_script_from_string(EseLuaEngine *engine, const char* script, const char* name, const char* module_name);

/**
 * @brief Creates a new proxy-wrapped instance from a loaded Lua script class.
 * 
 * @details Retrieves the script's class table from registry, creates a real instance
 *          table with metatable pointing to the class (__index = class), creates
 *          a readonly_keys tracking table, wraps the real instance in a proxy table
 *          with custom __index/__newindex metamethods for read-only property support,
 *          and returns a registry reference to the proxy.
 * 
 * @param engine Pointer to the EseLuaEngine instance.
 * @param filename Name of the previously loaded Lua script to instantiate.
 * 
 * @return Registry reference ID (positive integer) to the new proxy instance on success,
 *         -1 on failure.
 * 
 * @warning The returned reference must be removed with lua_engine_instance_remove()
 *          to prevent registry leaks.
 * @warning The script must have been loaded first with lua_engine_load_script().
 */
int lua_engine_instance_script(EseLuaEngine *engine, const char *filename);

/**
 * @brief Removes a Lua script instance from the registry.
 * 
 * @details Uses luaL_unref to remove the instance reference from the Lua registry,
 *          allowing the proxy table and all associated data to be garbage collected.
 * 
 * @param engine Pointer to the EseLuaEngine instance. Safe to pass NULL.
 * @param instance_ref Registry reference ID of the instance to remove.
 * 
 * @warning Using the instance_ref after calling this function will result in undefined behavior.
 */
void lua_engine_instance_remove(EseLuaEngine *engine, int instance_ref);

/**
 * @brief Executes a Lua function by its registry reference.
 * 
 * @details Executes a Lua function using its registry reference. This is faster than
 *          the old instance_run_function functions as it avoids repeated function lookups.
 *          Hook setup is still performed for security.
 * 
 * @param engine Pointer to the EseLuaEngine instance.
 * @param function_ref Registry reference ID of the function to execute.
 * @param self_ref Registry reference ID of the function self.
 * @param argc Number of arguments to pass (length of argv array).
 * @param argv Array of EseLuaValue structures to pass as function arguments.
 * 
 * @return true if the function executed successfully, false on error or timeout.
 * 
 * @warning Function execution is subject to time and instruction count limits.
 * @warning Lua errors in the called function will be logged but not propagated.
 */
bool lua_engine_run_function_ref(
    EseLuaEngine *engine,
    int function_ref,
    int self_ref,
    int argc,
    EseLuaValue *argv
);

/**
 * @brief Executes a Lua function by name from a script instance.
 * 
 * @details Executes a Lua function by looking it up by name in the script instance.
 *          This is slower than lua_engine_run_function_ref as it requires function lookup,
 *          but more convenient when you don't have a cached function reference.
 * 
 * @param engine Pointer to the EseLuaEngine instance.
 * @param instance_ref Registry reference ID of the script instance.
 * @param self_ref Registry reference ID of the function self.
 * @param func_name Name of the function to execute.
 * @param argc Number of arguments to pass (length of argv array).
 * @param argv Array of EseLuaValue structures to pass as function arguments.
 * 
 * @return true if the function executed successfully, false on error or timeout.
 * 
 * @warning Function execution is subject to time and instruction count limits.
 * @warning Lua errors in the called function will be logged but not propagated.
 */
bool lua_engine_run_function(
    EseLuaEngine *engine,
    int instance_ref,
    int self_ref,
    const char *func_name,
    int argc,
    EseLuaValue *argv
);

int lua_isinteger_lj(lua_State *L, int idx);
void *lua_getextraspace_lj(lua_State *L);

#endif // ESE_LUA_ENGINE_H
