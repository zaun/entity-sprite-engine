/*
 * Project: Entity Sprite Engine
 *
 * Entity Collider Component.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef ESE_ENTITY_COMPONENT_COLLIDER_H
#define ESE_ENTITY_COMPONENT_COLLIDER_H

#include "entity/components/entity_component_private.h" // EseEntityComponent
#include "vendor/json/cJSON.h"
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
 * @details This component manages one or more collision rectangles for complex
 *          collision shapes. It stores an array of collision rectangles plus an
 *          offset, and maintains flags for debug drawing and map interaction.
 */
typedef struct EseEntityComponentCollider {
    EseEntityComponent base; /** Base component structure */

    EsePoint *offset;      /** Offset of the collider relative to the entity position */
    EseRect **rects;       /** Array of collision rectangles */
    size_t rects_count;    /** Number of collision rectangles */
    size_t rects_capacity; /** Allocated capacity for rectangles array */
    bool draw_debug;       /** Whether to draw debug visualization of colliders */
    bool map_interaction;  /** Whether to interact with the map */
} EseEntityComponentCollider;

/**
 * @brief Allocate and initialize a collider component without registering it with Lua.
 *
 * @param engine Lua engine used for allocations and UUID generation.
 * @return Pointer to the base component, or NULL on allocation failure.
 */
EseEntityComponent *entity_component_collider_make(EseLuaEngine *engine);

/**
 * @brief Create a collider component and register it with Lua.
 *
 * @param engine Lua engine used for allocations and UUID generation.
 * @return Pointer to the base component, or NULL on allocation failure.
 */
EseEntityComponent *entity_component_collider_create(EseLuaEngine *engine);

/**
 * @brief Create a deep copy of an existing collider component.
 *
 * @param src Source collider to copy.
 * @return Pointer to the new base component, or NULL on allocation failure.
 */
EseEntityComponent *entity_component_collider_copy(const EseEntityComponentCollider *src);

/**
 * @brief Destroy a collider component, respecting Lua reference counting.
 *
 * @param component Collider component to destroy.
 */
void entity_component_collider_destroy(EseEntityComponentCollider *component);

/**
 * @brief Increment the Lua registry reference count for this collider component.
 *
 * @param component Collider component to reference.
 */
void entity_component_collider_ref(EseEntityComponentCollider *component);

/**
 * @brief Decrement the Lua registry reference count for this collider component.
 *
 * When the reference count reaches zero, the component and its resources are freed.
 *
 * @param component Collider component to unreference.
 */
void entity_component_collider_unref(EseEntityComponentCollider *component);

/**
 * @brief Serialize collider state to a JSON object.
 *
 * The JSON currently includes type, active flag, debug draw flag, map interaction
 * flag, and collider offset.
 *
 * @param component Collider component to serialize.
 * @return Newly allocated cJSON object, or NULL on error.
 */
cJSON *entity_component_collider_serialize(const EseEntityComponentCollider *component);

/**
 * @brief Deserialize collider state from a JSON object.
 *
 * A new collider component is created and configured from the JSON fields.
 *
 * @param engine Lua engine used for allocations and UUID generation.
 * @param data   JSON object describing the collider.
 * @return Pointer to the new base component, or NULL on error.
 */
EseEntityComponent *entity_component_collider_deserialize(EseLuaEngine *engine,
                                                          const cJSON *data);

/**
 * @brief Add a collision rectangle to the collider and register change watchers.
 *
 * This takes a reference to the rect, attaches a watcher so that future rect
 * changes update bounds, and recomputes entity collision bounds.
 *
 * @param collider Collider component to modify.
 * @param rect     Rectangle to add to the collider.
 */
void entity_component_collider_rects_add(EseEntityComponentCollider *collider, EseRect *rect);

/**
 * @brief Watcher callback invoked when a collider rect changes.
 *
 * This is registered with each rect so that collider bounds stay up to date.
 *
 * @param rect     Rectangle that changed.
 * @param userdata User data pointer, expected to be an EseEntityComponentCollider*.
 */
void entity_component_collider_rect_changed(EseRect *rect, void *userdata);

/**
 * @brief Recompute entity-relative and world-space collision bounds.
 *
 * When there are no rects, this clears existing bounds. Otherwise, it computes
 * an axis-aligned bounding box that encloses all collider rects.
 *
 * @param collider Collider component whose bounds should be updated.
 */
void entity_component_collider_update_bounds(EseEntityComponentCollider *collider);

/**
 * @brief Notify the collider that one or more of its rects has changed.
 *
 * This is a convenience wrapper that simply recomputes collision bounds.
 *
 * @param collider Collider component to update.
 */
void entity_component_collider_rect_updated(EseEntityComponentCollider *collider);

/**
 * @brief Notify the collider that its owning entity's position has changed.
 *
 * This recomputes world-space collision bounds using the new entity position.
 *
 * @param collider Collider component whose entity moved.
 */
void entity_component_collider_position_changed(EseEntityComponentCollider *collider);

/**
 * @brief Get the debug-draw flag for this collider.
 *
 * @param collider Collider component to query.
 * @return true if collider debug drawing is enabled, false otherwise.
 */
bool entity_component_collider_get_draw_debug(EseEntityComponentCollider *collider);

/**
 * @brief Set the debug-draw flag for this collider.
 *
 * @param collider   Collider component to modify.
 * @param draw_debug Whether debug drawing should be enabled.
 */
void entity_component_collider_set_draw_debug(EseEntityComponentCollider *collider,
                                              bool draw_debug);

/**
 * @brief Get whether this collider participates in map interactions.
 *
 * @param collider Collider component to query.
 * @return true if map interaction is enabled, false otherwise.
 */
bool entity_component_collider_get_map_interaction(EseEntityComponentCollider *collider);

/**
 * @brief Enable or disable map interaction for this collider.
 *
 * @param collider Collider component to modify.
 * @param enabled  Whether map interaction should be enabled.
 */
void entity_component_collider_set_map_interaction(EseEntityComponentCollider *collider,
                                                   bool enabled);

#endif // ESE_ENTITY_COMPONENT_COLLIDER_H
