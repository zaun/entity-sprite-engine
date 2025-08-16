


#ifndef ESE_ENTITY_COMPONENT_SPRITE_H
#define ESE_ENTITY_COMPONENT_SPRITE_H

#include <string.h>
#include "entity/components/entity_component_private.h" // EseEntityComponent

// Forward declarations
typedef struct EseEntity EseEntity;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseSprite EseSprite;

typedef struct EseEntityComponentSprite {
    EseEntityComponent base;

    char *sprite_name;
    EseSprite *sprite; // Not owned by EseEntityComponentSprite
    size_t current_frame;
    float sprite_ellapse_time;
} EseEntityComponentSprite;

EseEntityComponent *_entity_component_sprite_copy(const EseEntityComponentSprite *src);

void _entity_component_sprite_destroy(EseEntityComponentSprite *component);

void _entity_component_sprite_update(EseEntityComponentSprite *component, EseEntity *entity, float delta_time);

EseEntityComponentSprite *_entity_component_sprite_get(lua_State *L, int idx);

void _entity_component_sprite_init(EseLuaEngine *engine);

void _entity_component_sprite_draw(EseEntityComponentSprite *component, float screen_x, float screen_y, EntityDrawTextureCallback texCallback, void *callback_user_data);

EseEntityComponent *entity_component_sprite_create(EseLuaEngine *engine, const char *sprite_name);

#endif // ESE_ENTITY_COMPONENT_SPRITE_H