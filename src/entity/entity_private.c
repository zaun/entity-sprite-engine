#include "entity/entity_private.h"
#include "core/memory_manager.h"
#include "entity/components/entity_component_collider.h"
#include "entity/components/entity_component_private.h"
#include "entity/entity.h"
#include "entity/entity_lua.h"
#include "scripting/lua_value.h"
#include "types/types.h"
#include "utility/double_linked_list.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/lua/src/lauxlib.h"
#include "vendor/lua/src/lua.h"
#include "vendor/lua/src/lualib.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ENTITY_INITIAL_CAPACITY 10

/**
 * @brief Callback function called when entity position changes
 *
 * @param point The position point that changed
 * @param user_data The entity that owns the position point
 */
static void _entity_position_changed(EsePoint *point, void *user_data) {
  EseEntity *entity = (EseEntity *)user_data;

  // Update collision bounds for all collider components
  for (size_t i = 0; i < entity->component_count; i++) {
    EseEntityComponent *comp = entity->components[i];
    if (comp->active && comp->type == ENTITY_COMPONENT_COLLIDER) {
      entity_component_collider_position_changed(
          (EseEntityComponentCollider *)comp->data);
    }
  }
}

EseEntity *_entity_make(EseLuaEngine *engine) {
  profile_start(PROFILE_ENTITY_CREATE);

  EseEntity *entity = memory_manager.malloc(sizeof(EseEntity), MMTAG_ENTITY);
  entity->position = ese_point_create(engine);
  ese_point_ref(entity->position);

  // Register a watcher to update collision bounds when position changes
  ese_point_add_watcher(entity->position, _entity_position_changed, entity);
  entity->id = ese_uuid_create(engine);
  ese_uuid_ref(entity->id);
  entity->active = true;
  entity->visible = true;
  entity->persistent = false;
  entity->destroyed = false;
  ese_point_set_x(entity->position, 0.0f);
  ese_point_set_y(entity->position, 0.0f);
  entity->draw_order = 0;

  // Not storing any values, so no free function needed
  entity->current_collisions = hashmap_create(NULL);
  entity->previous_collisions = hashmap_create(NULL);
  entity->collision_bounds = NULL;
  entity->collision_world_bounds = NULL;

  // Lazily allocate components array on first insert to reduce baseline
  // allocations
  entity->components = NULL;
  entity->component_capacity = 0;
  entity->component_count = 0;

  entity->lua = engine;
  entity->lua_ref = LUA_NOREF;
  entity->lua_ref_count = 0;
  entity->lua_val_ref = lua_value_create_nil("entity self ref");

  // Lazily create default_props when first property is added
  entity->default_props = NULL;

  // Initialize tag system
  entity->tags = NULL;
  entity->tag_count = 0;
  entity->tag_capacity = 0;

  // Lazily create pub/sub subscriptions list on first subscribe
  entity->subscriptions = NULL;

  profile_stop(PROFILE_ENTITY_CREATE, "entity_make");
  profile_count_add("entity_make_count");
  return entity;
}

int _entity_component_find_index(EseEntity *entity, const char *id) {
  log_assert("ENTITY", entity,
             "_entity_component_find_index called with NULL entity");

  for (size_t i = 0; i < entity->component_count; ++i) {
    EseEntityComponent *comp = entity->components[i];
    if (strcmp(ese_uuid_get_value(comp->id), id) == 0) {
      return (int)i;
    }
  }
  return -1;
}

/**
 * @brief Free function for entity subscription tracking.
 */
void _entity_subscription_free(void *value) {
  if (value == NULL)
    return;

  EseEntitySubscription *sub = (EseEntitySubscription *)value;
  memory_manager.free(sub->topic_name);
  memory_manager.free(sub->function_name);
  memory_manager.free(sub);
}
