#ifndef ESE_ENTITY_PRIVATE_H
#define ESE_ENTITY_PRIVATE_H

#include <stdint.h>
#include "utility/double_linked_list.h"
#include "utility/hashmap.h"
#include "entity/components/entity_component.h"
#include "types/types.h"
#include "entity.h"

/**
 * @brief Internal entity structure.
 */
struct EseEntity {
    EseUUID *id;                           /**< Unique entity identifier */
    bool active;                        /**< Whether entity is active */
    int draw_order;                     /**< Drawing order (z-index) */
    
    EsePoint *position;                    /**< EseEntity position */
    
    EseEntityComponent **components;       /**< Array of components */
    size_t component_count;             /**< Number of components */
    size_t component_capacity;          /**< Capacity of component array */

    EseHashMap* current_collisions;

    EseLuaEngine *lua;                     /**< Lua engine reference */
    int lua_ref;                        /**< Lua registry reference */
    EseDoubleLinkedList *default_props;    /**< Lua default props added to self.data */
};

/**
 * @brief Internal function to create an entity.
 * 
 * @param engine Pointer to EseLuaEngine
 * @return Pointer to newly created EseEntity
 */
EseEntity *_entity_make(EseLuaEngine *engine);

/**
 * @brief Internal function to find component index.
 * 
 * @param entity Pointer to EseEntity
 * @param id Component ID
 * @return Component index or -1 if not found
 */
int _entity_component_find_index(EseEntity *entity, const char *id);

/**
 * @brief Generates a unique, order-independent key for a collision pair.
 *
 * @param uuid1 The first entity's UUID.
 * @param uuid2 The second entity's UUID.
 * 
 * @return const char* A pointer to a static buffer containing the string key.
 */
const char* _get_collision_key(EseUUID* uuid1, EseUUID* uuid2);

bool _entity_test_collision(EseEntity *a, EseEntity *b);

#endif // ESE_ENTITY_PRIVATE_H
