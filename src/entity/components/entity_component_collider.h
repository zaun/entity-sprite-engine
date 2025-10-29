#ifndef ESE_ENTITY_COMPONENT_COLLIDER_H
#define ESE_ENTITY_COMPONENT_COLLIDER_H

#include "entity/components/entity_component_private.h" // EseEntityComponent
#include <stdbool.h>
#include <string.h>

#define ENTITY_COMPONENT_COLLIDER_PROXY_META "EntityComponentColliderProxyMeta"

// Forward declarations
typedef struct EseSprite EseSprite;
typedef struct EseEntity EseEntity;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseRect EseRect;
typedef struct EsePoint EsePoint;

/**
 * @brief Component that provides collision detection capabilities to an entity.
 *
 * @details This component manages multiple collision rectangles for complex
 *          collision shapes. It stores an array of collision rectangles,
 *          count, and capacity for dynamic resizing. Each rectangle defines
 *          a collision boundary for the entity.
 */
typedef struct EseEntityComponentCollider {
    EseEntityComponent base; /** Base component structure */

    EsePoint *offset;      /** Offset of the collider */
    EseRect **rects;       /** Array of collision rectangles */
    size_t rects_count;    /** Number of collision rectangles */
    size_t rects_capacity; /** Allocated capacity for rectangles array */
    bool draw_debug;       /** Whether to draw debug visualization of colliders */
    bool map_interaction;  /** Whether to interact with the map */
} EseEntityComponentCollider;

EseEntityComponent *_entity_component_collider_copy(const EseEntityComponentCollider *src);

void _entity_component_collider_destroy(EseEntityComponentCollider *component);

EseEntityComponentCollider *_entity_component_collider_get(lua_State *L, int idx);

void _entity_component_collider_init(EseLuaEngine *engine);

void _entity_component_collider_draw(EseEntityComponentCollider *collider, float screen_x,
                                     float screen_y, EntityDrawRectCallback rectCallback,
                                     void *callback_user_data);

EseEntityComponent *entity_component_collider_create(EseLuaEngine *engine);

void entity_component_collider_rects_add(EseEntityComponentCollider *collider, EseRect *rect);

void entity_component_collider_update_bounds(EseEntityComponentCollider *collider);

void entity_component_collider_rect_updated(EseEntityComponentCollider *collider);

void entity_component_collider_position_changed(EseEntityComponentCollider *collider);

void entity_component_collider_update_world_bounds_only(EseEntityComponentCollider *collider);

bool entity_component_collider_get_draw_debug(EseEntityComponentCollider *collider);

void entity_component_collider_set_draw_debug(EseEntityComponentCollider *collider,
                                              bool draw_debug);

bool entity_component_collider_get_map_interaction(EseEntityComponentCollider *collider);

void entity_component_collider_set_map_interaction(EseEntityComponentCollider *collider,
                                                   bool enabled);

void entity_component_collider_ref(EseEntityComponentCollider *component);

void entity_component_collider_unref(EseEntityComponentCollider *component);

#endif // ESE_ENTITY_COMPONENT_COLLIDER_H