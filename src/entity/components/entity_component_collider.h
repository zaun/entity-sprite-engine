#ifndef ESE_ENTITY_COMPONENT_COLLIDER_H
#define ESE_ENTITY_COMPONENT_COLLIDER_H

#include <string.h>
#include "entity/components/entity_component_private.h" // EseEntityComponent

// Forward declarations
typedef struct EseSprite EseSprite;
typedef struct EseEntity EseEntity;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseRect EseRect;

typedef struct EseEntityComponentCollider {
    EseEntityComponent base;
    EseRect **rects;
    size_t rects_count;
    size_t rects_capacity;
} EseEntityComponentCollider;

EseEntityComponent *_entity_component_collider_copy(const EseEntityComponentCollider *src);

void _entity_component_collider_destroy(EseEntityComponentCollider *component);

void _entity_component_collider_update(EseEntityComponentCollider *component, EseEntity *entity, float delta_time);

EseEntityComponentCollider *_entity_component_collider_get(lua_State *L, int idx);

void _entity_component_collider_init(EseLuaEngine *engine);

void _entity_component_collider_draw(EseEntityComponentCollider *collider, float screen_x, float screen_y, EntityDrawRectCallback rectCallback, void *callback_user_data);

EseEntityComponent *entity_component_collider_create(EseLuaEngine *engine);

void entity_component_collider_rects_add(EseEntityComponentCollider *collider, EseRect *rect);

#endif // ESE_ENTITY_COMPONENT_COLLIDER_H