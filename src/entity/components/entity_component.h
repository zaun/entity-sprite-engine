#ifndef ESE_ENTITY_COMPONENTS_H
#define ESE_ENTITY_COMPONENTS_H

#include "entity/entity.h" // EntityDrawTextureCallback, EntityDrawRectCallback
#include "utility/array.h"

// Forward declarations
typedef struct EseEntity EseEntity;
typedef struct EseEntityComponent EseEntityComponent;
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Cached Lua function reference for performance optimization.
 *
 * @details Stores a cached reference to a Lua function to avoid repeated
 * lookups. The function_ref is a Lua registry reference that can be used
 * directly.
 */
typedef struct {
    int function_ref; /** Lua registry reference to the function */
    bool exists;      /** true if function exists, false if LUA_NOREF */
} CachedLuaFunction;

void entity_component_lua_init(EseLuaEngine *engine);

EseEntityComponent *entity_component_copy(EseEntityComponent *component);

void entity_component_destroy(EseEntityComponent *component);

void entity_component_push(EseEntityComponent *component);

void entity_component_detect_collision_with_component(EseEntityComponent *a, EseEntityComponent *b,
                                                      EseArray *out_hits);

bool entity_component_detect_collision_rect(EseEntityComponent *a, EseRect *rect);

/**
 * @brief Runs a function on a component using component-specific logic.
 *
 * @details This is the new entry point for running functions on components.
 *          It delegates to component-specific run functions for better
 * performance.
 *
 * @param component Pointer to the component.
 * @param entity Pointer to the entity (for getting the correct Lua self
 * reference).
 * @param func_name Name of the function to execute.
 * @param argc Number of arguments to pass.
 * @param argv Array of arguments to pass to the function.
 *
 * @return true if the function executed successfully, false otherwise.
 */
bool entity_component_run_function(EseEntityComponent *component, EseEntity *entity,
                                   const char *func_name, int argc, EseLuaValue *argv[]);

void *entity_component_get_data(EseEntityComponent *component);

EseEntityComponent *entity_component_get(lua_State *L);

#endif // ESE_ENTITY_COMPONENTS_H
