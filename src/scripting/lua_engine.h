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

typedef struct EseLuaEngine {
    lua_State *runtime;
    EseLuaEngineInternal *internal;
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
 * 
 * @return true if the script was successfully loaded and compiled, false on any error.
 * 
 * @warning Script files are read entirely into memory. Large files may cause issues.
 * @warning The script must return a table or loading will fail.
 */
bool lua_engine_load_script(EseLuaEngine *engine, const char* filename);

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
 * @brief Executes a method on a Lua script instance with no additional arguments.
 * 
 * @details Locates the specified function in the instance's class table hierarchy,
 *          pushes the proxy instance as the 'self' parameter, sets up execution
 *          limits and timeout hooks, calls the function with instruction counting
 *          and time monitoring, and cleans up the execution environment.
 * 
 * @param engine Pointer to the EseLuaEngine instance.
 * @param instance_ref Registry reference ID of the instance.
 * @param instance_ref Registry reference ID of the function self.
 * @param func_name Name of the method to execute.
 * 
 * @return true if the function executed successfully without errors, false on error
 *         or if function was not found.
 * 
 * @warning Function execution is subject to time and instruction count limits.
 * @warning Lua errors in the called function will be logged but not propagated.
 */
bool lua_engine_instance_run_function(
    EseLuaEngine *engine,
    int instance_ref,
    int self_ref,
    const char *func_name
);

/**
 * @brief Executes a method on a Lua script instance with provided arguments.
 * 
 * @details Locates the function, pushes the proxy instance as 'self', converts
 *          and pushes each EseLuaValue argument to the Lua stack, sets up execution
 *          monitoring with timeout and instruction counting, calls the function,
 *          and handles any execution errors or timeouts.
 * 
 * @param engine Pointer to the EseLuaEngine instance.
 * @param instance_ref Registry reference ID of the instance.
 * @param self_ref Registry reference ID of the function self.
 * @param func_name Name of the method to execute.
 * @param argc Number of arguments to pass (length of argv array).
 * @param argv Array of EseLuaValue structures to pass as function arguments.
 * 
 * @return true if the function executed successfully, false on error or timeout.
 * 
 * @warning Arguments are converted to Lua types; complex nested structures may cause stack overflow.
 * @warning Function execution is limited by engine timeout and instruction count settings.
 */
bool lua_engine_instance_run_function_with_args(
    EseLuaEngine *engine,
    int instance_ref,
    int self_ref,
    const char *func_name,
    int argc,
    EseLuaValue *argv
);

#endif // LUA_ENGINE_H
