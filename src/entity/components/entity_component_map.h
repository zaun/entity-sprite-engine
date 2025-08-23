#ifndef ESE_ENTITY_COMPONENT_MAP_H
#define ESE_ENTITY_COMPONENT_MAP_H

#include <string.h>
#include "entity/components/entity_component_private.h" // EseEntityComponent

typedef struct EseMap EseMap;
typedef struct EsePoint EsePoint;
typedef struct lua_State lua_State;

typedef struct EseEntityComponentMap
{
    EseEntityComponent base;
    EseMap *map;       /**< map to render */
    EsePoint *map_pos; /**< which map‐cell to center */
    int size;
    uint32_t seed;

    int *sprite_frames;
} EseEntityComponentMap;

EseEntityComponent *_entity_component_map_copy(const EseEntityComponentMap *src);

void _entity_component_map_destroy(EseEntityComponentMap *component);

void _entity_component_map_update(EseEntityComponentMap *component, EseEntity *entity, float delta_time);

EseEntityComponentMap *_entity_component_map_get(lua_State *L, int idx);

void _entity_component_map_init(EseLuaEngine *engine);

void _entity_component_map_draw(EseEntityComponentMap *component, float screen_x, float screen_y, EntityDrawTextureCallback texCallback, void *callback_user_data);

EseEntityComponent *entity_component_map_create(EseLuaEngine *engine);

#endif // ESE_ENTITY_COMPONENT_MAP_H
