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

bool entity_component_test_collision(EseEntityComponent *a, EseEntityComponent *b);

void entity_component_draw(
    EseEntityComponent *component,
    float camera_x, float camera_y,
    float view_width, float view_height,
    EntityDrawTextureCallback texCallback,
    EntityDrawRectCallback rectCallback,
    void *callback_user_data
);

void entity_component_run_function_with_args(
    EseEntityComponent *component,
    const char *func_name,
    int argc,
    EseLuaValue *argv
);

EseEntityComponent *entity_component_get(lua_State *L);

#endif // ESE_ENTITY_COMPONENTS_H
