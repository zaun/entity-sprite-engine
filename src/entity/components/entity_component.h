#ifndef ESE_ENTITY_COMPONENTS_H
#define ESE_ENTITY_COMPONENTS_H

#include "entity/entity.h" // EntityDrawTextureCallback, EntityDrawRectCallback

// Forward declarations
typedef struct EseEntity EseEntity;
typedef struct EseEntityComponent EseEntityComponent;
typedef struct EseLuaEngine EseLuaEngine;

void entity_component_lua_init(EseLuaEngine *engine);

EseEntityComponent *entity_component_copy(EseEntityComponent* component);

void entity_component_destroy(EseEntityComponent* component);

void entity_component_push(EseEntityComponent *component);

void entity_component_update(EseEntityComponent *component, EseEntity *entity, float delta_time);

bool entity_component_detect_collision_component(EseEntityComponent *a, EseEntityComponent *b);

bool entity_component_detect_collision_rect(EseEntityComponent *a, EseRect *rect);

void entity_component_draw(
    EseEntityComponent *component,
    float camera_x, float camera_y,
    float view_width, float view_height,
    EntityDrawCallbacks *callbacks,
    void *callback_user_data
);



/**
 * @brief Runs a function on a component using component-specific logic.
 * 
 * @details This is the new entry point for running functions on components.
 *          It delegates to component-specific run functions for better performance.
 * 
 * @param component Pointer to the component.
 * @param entity Pointer to the entity (for getting the correct Lua self reference).
 * @param func_name Name of the function to execute.
 * @param argc Number of arguments to pass.
 * @param argv Array of arguments to pass to the function.
 * 
 * @return true if the function executed successfully, false otherwise.
 */
bool entity_component_run_function(
    EseEntityComponent *component,
    EseEntity *entity,
    const char *func_name,
    int argc,
    EseLuaValue *argv[]
);

void *entity_component_get_data(EseEntityComponent *component);

EseEntityComponent *entity_component_get(lua_State *L);

#endif // ESE_ENTITY_COMPONENTS_H
