#ifndef ESE_ENTITY_COMPONENT_COLLIDER_H
#define ESE_ENTITY_COMPONENT_COLLIDER_H

#include <string.h>
#include "entity/components/entity_component_private.h" // EseEntityComponent

// Forward declarations
typedef struct EseSprite EseSprite;
typedef struct EseEntity EseEntity;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseRect EseRect;

/**
 * @brief Component that provides collision detection capabilities to an entity.
 * 
 * @details This component manages multiple collision rectangles for complex
 *          collision shapes. It stores an array of collision rectangles,
 *          count, and capacity for dynamic resizing. Each rectangle defines
 *          a collision boundary for the entity.
 */
typedef struct EseEntityComponentCollider {
    EseEntityComponent base;        /**< Base component structure */

    EseRect **rects;                /**< Array of collision rectangles */
    size_t rects_count;             /**< Number of collision rectangles */
    size_t rects_capacity;          /**< Allocated capacity for rectangles array */
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