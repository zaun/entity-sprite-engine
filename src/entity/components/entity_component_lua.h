#ifndef ENTITY_COMPONENT_LUA_PRIVATE_H
#define ENTITY_COMPONENT_LUA_PRIVATE_H

#include "entity/components/entity_component_private.h" // EseEntityComponent
#include <string.h>

#define ENTITY_COMPONENT_LUA_PROXY_META "EntityComponentLuaProxyMeta"

// Forward declarations
typedef struct EseEntity EseEntity;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseLuaValue EseLuaValue;
typedef struct EseHashMap EseHashMap;
typedef struct lua_State lua_State;

/**
 * @brief Component that provides Lua scripting capabilities to an entity.
 *
 * @details This component manages Lua script execution, instance data, and
 *          property storage. It stores the script filename, engine reference,
 *          instance registry reference, argument values, and dynamic
 * properties. The engine reference is not owned and should not be freed.
 */
typedef struct EseEntityComponentLua {
  EseEntityComponent base; /** Base component structure */

  char *script;         /** Filename of the Lua script to execute */
  EseLuaEngine *engine; /** Reference to Lua engine (not owned) */
  int instance_ref;     /** Lua registry reference to instance table */

  EseLuaValue *arg;    /** Argument value passed to script functions */
  EseLuaValue **props; /** Array of dynamic properties */
  size_t props_count;  /** Number of properties in the array */

  EseHashMap
      *function_cache; /** Cache of function references for performance */
} EseEntityComponentLua;

EseEntityComponent *
_entity_component_lua_copy(const EseEntityComponentLua *src);

void _entity_component_lua_destroy(EseEntityComponentLua *component);

void _entity_component_lua_update(EseEntityComponentLua *component,
                                  EseEntity *entity, double delta_time);

EseEntityComponentLua *_entity_component_lua_get(lua_State *L, int idx);

void _entity_component_lua_init(EseLuaEngine *engine);

EseEntityComponent *entity_component_lua_create(EseLuaEngine *engine,
                                                const char *script);

/**
 * @brief Runs a Lua function using cached function references for performance.
 *
 * @details Executes a Lua function using the component's function cache. If the
 * function is not cached, it will be looked up and cached for future use.
 * Functions that don't exist are ignored.
 *
 * @param component Pointer to the EntityComponentLua component.
 * @param entity Pointer to the entity (for getting the correct Lua self
 * reference).
 * @param func_name Name of the function to execute.
 * @param argc Number of arguments to pass.
 * @param argv Array of arguments to pass to the function.
 *
 * @return true if the function executed successfully, false if function doesn't
 * exist or on error.
 */
bool entity_component_lua_run(EseEntityComponentLua *component,
                              EseEntity *entity, const char *func_name,
                              int argc, EseLuaValue *argv[]);

/**
 * @brief Populates the function cache with standard entity lifecycle functions.
 *
 * @details Caches the standard entity functions (entity_init, entity_update,
 *          entity_collision_enter, entity_collision_stay,
 * entity_collision_exit). Functions that don't exist are cached as LUA_NOREF.
 *
 * @param component Pointer to the EntityComponentLua component.
 */
void _entity_component_lua_cache_functions(EseEntityComponentLua *component);

/**
 * @brief Clears all cached function references.
 *
 * @details Removes all cached function references from the registry and clears
 * the cache.
 *
 * @param component Pointer to the EntityComponentLua component.
 */
void _entity_component_lua_clear_cache(EseEntityComponentLua *component);

/**
 * @brief Increments the reference count for a Lua component.
 *
 * @details Creates a Lua userdata proxy for the component and stores it in the
 * registry. If the component is already referenced, just increments the
 * reference count.
 *
 * @param component Pointer to the EntityComponentLua component.
 */
void entity_component_lua_ref(EseEntityComponentLua *component);

/**
 * @brief Decrements the reference count for a Lua component.
 *
 * @details Decrements the reference count and removes the Lua reference if it
 * reaches zero. Does not free the component memory - that's handled by the
 * destroy function.
 *
 * @param component Pointer to the EntityComponentLua component.
 */
void entity_component_lua_unref(EseEntityComponentLua *component);

#endif // ENTITY_COMPONENT_LUA_PRIVATE_H