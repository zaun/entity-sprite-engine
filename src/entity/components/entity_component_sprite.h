

#ifndef ESE_ENTITY_COMPONENT_SPRITE_H
#define ESE_ENTITY_COMPONENT_SPRITE_H

#include "entity/components/entity_component_private.h" // EseEntityComponent
#include "vendor/json/cJSON.h"
#include <string.h>

#define ENTITY_COMPONENT_SPRITE_PROXY_META "EntityComponentSpriteProxyMeta"

// Forward declarations
typedef struct EseEntity EseEntity;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseSprite EseSprite;

/**
 * @brief Component that provides sprite rendering capabilities to an entity.
 *
 * @details This component manages sprite animation, frame timing, and visual
 *          representation. It stores the sprite name, current frame index, and
 *          elapsed time for animation control. The actual sprite object is
 * looked up by name when needed for rendering.
 */
typedef struct EseEntityComponentSprite {
    EseEntityComponent base; /** Base component structure */

    char *sprite_name;         /** Name/ID of the sprite to display */
    size_t current_frame;      /** Current animation frame index */
    float sprite_ellapse_time; /** Elapsed time for frame timing control */
} EseEntityComponentSprite;

EseEntityComponent *_entity_component_sprite_copy(const EseEntityComponentSprite *src);

void _entity_component_sprite_destroy(EseEntityComponentSprite *component);

cJSON *entity_component_sprite_serialize(const EseEntityComponentSprite *component);
EseEntityComponent *entity_component_sprite_deserialize(EseLuaEngine *engine,
                                                        const cJSON *data);

EseEntityComponentSprite *_entity_component_sprite_get(lua_State *L, int idx);

void _entity_component_sprite_init(EseLuaEngine *engine);

void _entity_component_sprite_draw(EseEntityComponentSprite *component, float screen_x,
                                   float screen_y, EntityDrawTextureCallback texCallback,
                                   void *callback_user_data);

EseEntityComponent *entity_component_sprite_create(EseLuaEngine *engine, const char *sprite_name);

#endif // ESE_ENTITY_COMPONENT_SPRITE_H