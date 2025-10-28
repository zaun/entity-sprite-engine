#include "entity/components/entity_component_collider.h"
#include "core/asset_manager.h"
#include "core/collision_resolver.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "entity/components/entity_component.h"
#include "entity/components/entity_component_lua.h"
#include "entity/components/entity_component_private.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "scripting/lua_engine.h"
#include "types/rect.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <math.h>
#include <string.h>

#define COLLIDER_RECT_CAPACITY 5

static void _entity_component_collider_rect_changed(EseRect *rect,
                                                    void *userdata);

bool _entity_component_collider_collides_component(
    EseEntityComponentCollider *colliderA,
    EseEntityComponentCollider *colliderB, EseArray *out_hits);

/**
 * @brief Helper function to get the collider component from a rects proxy
 * table.
 *
 * @details Extracts the collider component from the __component field of a
 * rects proxy table.
 *
 * @param L Lua state pointer
 * @param idx Stack index of the rects proxy table
 * @return Pointer to the collider component, or NULL if extraction fails
 */
static EseEntityComponentCollider *
_entity_component_collider_rects_get_component(lua_State *L, int idx) {
  // Check if it's userdata
  if (!lua_isuserdata(L, idx)) {
    return NULL;
  }

  // Get the userdata and check metatable
  EseEntityComponentCollider **ud =
      (EseEntityComponentCollider **)luaL_testudata(L, idx,
                                                    "ColliderRectsProxyMeta");
  if (!ud) {
    return NULL; // Wrong metatable or not userdata
  }

  return *ud;
}

// VTable wrapper functions
static EseEntityComponent *
_collider_vtable_copy(EseEntityComponent *component) {
  return _entity_component_collider_copy(
      (EseEntityComponentCollider *)component->data);
}

static void _collider_vtable_destroy(EseEntityComponent *component) {
  _entity_component_collider_destroy(
      (EseEntityComponentCollider *)component->data);
}

static void _collider_vtable_update(EseEntityComponent *component,
                                    EseEntity *entity, float delta_time) {
  // Collider update only updates world bounds
  entity_component_collider_update_world_bounds_only(
      (EseEntityComponentCollider *)component->data);
}

static void _collider_vtable_draw(EseEntityComponent *component, int screen_x,
                                  int screen_y, void *callbacks,
                                  void *user_data) {
  // Collider rendering is now handled by the collider render system
  (void)component;
  (void)screen_x;
  (void)screen_y;
  (void)callbacks;
  (void)user_data;
}

static bool _collider_vtable_run_function(EseEntityComponent *component,
                                          EseEntity *entity,
                                          const char *func_name, int argc,
                                          void *argv[]) {
  // Colliders don't support function execution
  return false;
}

static void _collider_vtable_collides_component(EseEntityComponent *a,
                                                EseEntityComponent *b,
                                                EseArray *out_hits) {
  _entity_component_collider_collides_component(
      (EseEntityComponentCollider *)a->data,
      (EseEntityComponentCollider *)b->data, out_hits);
}

static void _collider_vtable_ref(EseEntityComponent *component) {
  entity_component_collider_ref((EseEntityComponentCollider *)component->data);
}

static void _collider_vtable_unref(EseEntityComponent *component) {
  entity_component_collider_unref(
      (EseEntityComponentCollider *)component->data);
}

// Static vtable instance for collider components
static const ComponentVTable collider_vtable = {
    .copy = _collider_vtable_copy,
    .destroy = _collider_vtable_destroy,
    .update = _collider_vtable_update,
    .draw = _collider_vtable_draw,
    .run_function = _collider_vtable_run_function,
    .collides = _collider_vtable_collides_component,
    .ref = _collider_vtable_ref,
    .unref = _collider_vtable_unref};

bool _entity_component_collider_collides_component(
    EseEntityComponentCollider *colliderA,
    EseEntityComponentCollider *colliderB, EseArray *out_hits) {
  log_assert("ENTITY_COMP", colliderA,
             "_entity_component_collider_collides_component called with NULL "
             "collider");
  log_assert("ENTITY_COMP", colliderB,
             "_entity_component_collider_collides_component called with NULL "
             "collider");

  profile_start(PROFILE_ENTITY_COMP_COLLIDER_COLLIDES);

  float pos_a_x = ese_point_get_x(colliderA->base.entity->position);
  float pos_a_y = ese_point_get_y(colliderA->base.entity->position);
  float pos_b_x = ese_point_get_x(colliderB->base.entity->position);
  float pos_b_y = ese_point_get_y(colliderB->base.entity->position);

  for (size_t i = 0; i < colliderA->rects_count; i++) {
    EseRect *rect_a = colliderA->rects[i];

    // Create world-space rect for A
    EseRect *world_rect_a = ese_rect_create(colliderA->base.lua);
    ese_rect_set_x(world_rect_a, ese_rect_get_x(rect_a) +
                                     ese_point_get_x(colliderA->offset) +
                                     pos_a_x);
    ese_rect_set_y(world_rect_a, ese_rect_get_y(rect_a) +
                                     ese_point_get_y(colliderA->offset) +
                                     pos_a_y);
    ese_rect_set_width(world_rect_a, ese_rect_get_width(rect_a));
    ese_rect_set_height(world_rect_a, ese_rect_get_height(rect_a));
    ese_rect_set_rotation(world_rect_a, ese_rect_get_rotation(rect_a));

    for (size_t j = 0; j < colliderB->rects_count; j++) {
      EseRect *rect_b = colliderB->rects[j];

      // Create world-space rect for B
      EseRect *world_rect_b = ese_rect_create(colliderB->base.lua);
      ese_rect_set_x(world_rect_b, ese_rect_get_x(rect_b) +
                                       ese_point_get_x(colliderB->offset) +
                                       pos_b_x);
      ese_rect_set_y(world_rect_b, ese_rect_get_y(rect_b) +
                                       ese_point_get_y(colliderB->offset) +
                                       pos_b_y);
      ese_rect_set_width(world_rect_b, ese_rect_get_width(rect_b));
      ese_rect_set_height(world_rect_b, ese_rect_get_height(rect_b));
      ese_rect_set_rotation(world_rect_b, ese_rect_get_rotation(rect_b));

      // Use proper rotated rectangle intersection test
      if (ese_rect_intersects(world_rect_a, world_rect_b)) {
        profile_count_add("collider_pair_rect_tests_hit");
        EseCollisionHit *hit =
            ese_collision_hit_create(colliderA->base.entity->lua);
        ese_collision_hit_set_kind(hit, COLLISION_KIND_COLLIDER);
        ese_collision_hit_set_entity(hit, colliderA->base.entity);
        ese_collision_hit_set_target(hit, colliderB->base.entity);
        ese_collision_hit_set_state(hit, COLLISION_STATE_STAY);
        ese_collision_hit_set_rect(hit, colliderB->rects[j]);
        array_push(out_hits, hit);
        ese_rect_destroy(world_rect_b);
        ese_rect_destroy(world_rect_a);
        profile_stop(PROFILE_ENTITY_COMP_COLLIDER_COLLIDES,
                     "entity_comp_collider_collides_comp");
        return true;
      }
      ese_rect_destroy(world_rect_b);
      profile_count_add("collider_pair_rect_tests_miss");
    }

    ese_rect_destroy(world_rect_a);
  }

  profile_stop(PROFILE_ENTITY_COMP_COLLIDER_COLLIDES,
               "entity_comp_collider_collides_comp");
  return false;
}

void entity_component_collider_ref(EseEntityComponentCollider *component) {
  log_assert("ENTITY_COMP", component,
             "entity_component_collider_ref called with NULL component");

  if (component->base.lua_ref == LUA_NOREF) {
    // First time referencing - create userdata and store reference
    EseEntityComponentCollider **ud =
        (EseEntityComponentCollider **)lua_newuserdata(
            component->base.lua->runtime, sizeof(EseEntityComponentCollider *));
    *ud = component;

    // Attach metatable
    luaL_getmetatable(component->base.lua->runtime,
                      ENTITY_COMPONENT_COLLIDER_PROXY_META);
    lua_setmetatable(component->base.lua->runtime, -2);

    // Store hard reference to prevent garbage collection
    component->base.lua_ref =
        luaL_ref(component->base.lua->runtime, LUA_REGISTRYINDEX);
    component->base.lua_ref_count = 1;
  } else {
    // Already referenced - just increment count
    component->base.lua_ref_count++;
  }

  profile_count_add("entity_comp_collider_ref_count");
}

void entity_component_collider_unref(EseEntityComponentCollider *component) {
  if (!component)
    return;

  if (component->base.lua_ref != LUA_NOREF &&
      component->base.lua_ref_count > 0) {
    component->base.lua_ref_count--;

    if (component->base.lua_ref_count == 0) {
      // No more references - remove from registry
      luaL_unref(component->base.lua->runtime, LUA_REGISTRYINDEX,
                 component->base.lua_ref);
      component->base.lua_ref = LUA_NOREF;
    }
  }

  profile_count_add("entity_comp_collider_unref_count");
}

static EseEntityComponent *
_entity_component_collider_make(EseLuaEngine *engine) {
  EseEntityComponentCollider *component =
      memory_manager.malloc(sizeof(EseEntityComponentCollider), MMTAG_ENTITY);
  component->base.data = component;
  component->base.active = true;
  component->base.id = ese_uuid_create(engine);
  component->base.lua = engine;
  component->base.lua_ref = LUA_NOREF;
  component->base.lua_ref_count = 0;
  component->base.type = ENTITY_COMPONENT_COLLIDER;
  component->base.vtable = &collider_vtable;

  component->offset = ese_point_create(engine);
  ese_point_ref(component->offset);
  component->rects = memory_manager.malloc(
      sizeof(EseRect *) * COLLIDER_RECT_CAPACITY, MMTAG_ENTITY);
  component->rects_capacity = COLLIDER_RECT_CAPACITY;
  component->rects_count = 0;
  component->draw_debug = false;
  component->map_interaction = false;

  return &component->base;
}

EseEntityComponent *
_entity_component_collider_copy(const EseEntityComponentCollider *src) {
  log_assert("ENTITY_COMP", src,
             "_entity_component_collider_copy called with NULL src");

  EseEntityComponentCollider *copy =
      memory_manager.malloc(sizeof(EseEntityComponentCollider), MMTAG_ENTITY);
  copy->base.data = copy;
  copy->base.active = true;
  copy->base.id = ese_uuid_create(src->base.lua);
  copy->base.lua = src->base.lua;
  copy->base.lua_ref = LUA_NOREF;
  copy->base.lua_ref_count = 0;
  copy->base.type = ENTITY_COMPONENT_COLLIDER;
  copy->base.vtable = &collider_vtable;

  copy->offset = ese_point_copy(src->offset);
  ese_point_ref(copy->offset);

  // Copy rects
  copy->rects = memory_manager.malloc(sizeof(EseRect *) * src->rects_capacity,
                                      MMTAG_ENTITY);
  copy->rects_capacity = src->rects_capacity;
  copy->rects_count = src->rects_count;
  copy->draw_debug = src->draw_debug;
  copy->map_interaction = src->map_interaction;

  for (size_t i = 0; i < copy->rects_count; ++i) {
    EseRect *src_comp = src->rects[i];
    EseRect *dst_comp = ese_rect_copy(src_comp);
    copy->rects[i] = dst_comp;
  }

  return &copy->base;
}

void _entity_component_collider_cleanup(EseEntityComponentCollider *component) {
  for (size_t i = 0; i < component->rects_count; ++i) {
    // Remove watcher before destroying rect
    ese_rect_remove_watcher(component->rects[i],
                            _entity_component_collider_rect_changed, component);
    ese_rect_unref(component->rects[i]);
    ese_rect_destroy(component->rects[i]);
  }
  memory_manager.free(component->rects);

  ese_point_unref(component->offset);
  ese_point_destroy(component->offset);

  // Clean up collision bounds from entity if this component created them
  if (component->base.entity) {
    if (component->base.entity->collision_bounds) {
      ese_rect_unref(component->base.entity->collision_bounds);
      ese_rect_destroy(component->base.entity->collision_bounds);
      component->base.entity->collision_bounds = NULL;
    }
    if (component->base.entity->collision_world_bounds) {
      ese_rect_unref(component->base.entity->collision_world_bounds);
      ese_rect_destroy(component->base.entity->collision_world_bounds);
      component->base.entity->collision_world_bounds = NULL;
    }
  }
  ese_uuid_destroy(component->base.id);
  memory_manager.free(component);
  profile_count_add("entity_comp_collider_destroy_count");
}

void _entity_component_collider_destroy(EseEntityComponentCollider *component) {
  log_assert("ENTITY_COMP", component,
             "_entity_component_collider_destroy called with NULL src");

  // Respect Lua registry ref-count; only free when no refs remain
  if (component->base.lua_ref != LUA_NOREF &&
      component->base.lua_ref_count > 0) {
    component->base.lua_ref_count--;
    if (component->base.lua_ref_count == 0) {
      luaL_unref(component->base.lua->runtime, LUA_REGISTRYINDEX,
                 component->base.lua_ref);
      component->base.lua_ref = LUA_NOREF;
      _entity_component_collider_cleanup(component);
    } else {
      // We dont "own" the collider so dont free it
      return;
    }
  } else if (component->base.lua_ref == LUA_NOREF) {
    _entity_component_collider_cleanup(component);
  }
}

/**
 * @brief Lua function to create a new EseEntityComponentCollider object.
 *
 * @details Callable from Lua as EseEntityComponentCollider.new().
 *
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new point object)
 *
 * @warning Items created in Lua are owned by Lua
 */
static int _entity_component_collider_new(lua_State *L) {
  EseRect *rect = NULL;

  int n_args = lua_gettop(L);
  if (n_args == 1) {
    // The rect parameter is at index 1 (first argument to the function)
    rect = ese_rect_lua_get(L, 1);
    if (rect == NULL) {
      luaL_argerror(
          L, 1,
          "EntityComponentCollider.new() or EntityComponentCollider.new(Rect)");
      return 0;
    }
  } else if (n_args > 1) {
    luaL_argerror(
        L, 1,
        "EntityComponentCollider.new() or EntityComponentCollider.new(Rect)");
    return 0;
  }

  // Set engine reference
  EseLuaEngine *lua =
      (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);

  // Create EseEntityComponent wrapper
  EseEntityComponent *component = _entity_component_collider_make(lua);
  component->lua = lua;

  // For Lua-created components, don't create a hard reference - let Lua manage
  // the lifecycle Create userdata directly without storing a persistent
  // reference
  EseEntityComponentCollider **ud =
      (EseEntityComponentCollider **)lua_newuserdata(
          L, sizeof(EseEntityComponentCollider *));
  *ud = (EseEntityComponentCollider *)component->data;

  // Attach metatable
  luaL_getmetatable(L, ENTITY_COMPONENT_COLLIDER_PROXY_META);
  lua_setmetatable(L, -2);

  if (rect) {
    entity_component_collider_rects_add(
        (EseEntityComponentCollider *)component->data, rect);
  }

  return 1;
}

EseEntityComponentCollider *_entity_component_collider_get(lua_State *L,
                                                           int idx) {
  log_assert("ENTITY_COMP", L,
             "_entity_component_collider_get called with NULL Lua state");

  // Check if the value at idx is userdata
  if (!lua_isuserdata(L, idx)) {
    return NULL;
  }

  // Get the userdata and check metatable
  EseEntityComponentCollider **ud =
      (EseEntityComponentCollider **)luaL_testudata(
          L, idx, ENTITY_COMPONENT_COLLIDER_PROXY_META);
  if (!ud) {
    return NULL; // Wrong metatable or not userdata
  }

  return *ud;
}

/**
 * @brief Add the passed component to the entity.
 */
static int _entity_component_collider_rects_add(lua_State *L) {
  // Get the collider component from the upvalue
  EseEntityComponentCollider *collider =
      (EseEntityComponentCollider *)lua_touserdata(L, lua_upvalueindex(1));

  if (!collider) {
    return luaL_error(L, "Invalid collider component in upvalue.");
  }

  int n_args = lua_gettop(L);
  EseRect *rect = NULL;
  if (n_args == 2) {
    // Called as: c.rects:add(rect) -> [self, rect]
    rect = ese_rect_lua_get(L, 2);
  } else if (n_args == 1) {
    // Called as: c.rects.add(rect) -> [rect]
    rect = ese_rect_lua_get(L, 1);
  } else {
    return luaL_argerror(L, 1, "Expected a Rect argument.");
  }

  if (rect == NULL) {
    return luaL_argerror(L, (n_args == 2 ? 2 : 1), "Expected a Rect argument.");
  }

  // Add the rect to the collider
  entity_component_collider_rects_add(collider, rect);

  return 0;
}

/**
 * @brief Lua function to remove a component from an entity.
 */
static int _entity_component_collider_rects_remove(lua_State *L) {
  EseEntityComponentCollider *collider =
      _entity_component_collider_rects_get_component(L, 1);
  if (!collider) {
    return luaL_error(L, "Invalid collider object.");
  }

  EseRect *rect_to_remove = ese_rect_lua_get(L, 2);
  if (rect_to_remove == NULL) {
    return luaL_argerror(L, 2, "Expected a Rect object.");
  }

  // Find the rect in the collider's rects array
  int idx = -1;
  for (size_t i = 0; i < collider->rects_count; ++i) {
    if (collider->rects[i] == rect_to_remove) {
      idx = (int)i;
      break;
    }
  }

  if (idx < 0) {
    lua_pushboolean(L, false);
    return 1;
  }

  // Remove the watcher before removing the rect
  ese_rect_remove_watcher(rect_to_remove,
                          _entity_component_collider_rect_changed, collider);
  ese_rect_unref(rect_to_remove);

  // Shift elements to remove the component
  for (size_t i = idx; i < collider->rects_count - 1; ++i) {
    collider->rects[i] = collider->rects[i + 1];
  }

  collider->rects_count--;
  collider->rects[collider->rects_count] = NULL;

  // Update entity's collision bounds after removing rect
  entity_component_collider_update_bounds(collider);

  lua_pushboolean(L, true);
  return 1;
}

/**
 * @brief Lua function to insert a component at a specific index.
 */
static int _entity_component_collider_rects_insert(lua_State *L) {
  EseEntityComponentCollider *collider =
      _entity_component_collider_rects_get_component(L, 1);
  if (!collider) {
    return luaL_error(L, "Invalid collider object.");
  }

  EseRect *rect = ese_rect_lua_get(L, 2);
  if (rect == NULL) {
    return luaL_argerror(L, 2, "Expected a rect object.");
  }

  int index = (int)luaL_checkinteger(L, 3) - 1; // Lua is 1-based

  if (index < 0 || index > (int)collider->rects_count) {
    return luaL_error(L, "Index out of bounds.");
  }

  // Resize array if necessary
  if (collider->rects_count == collider->rects_capacity) {
    size_t new_capacity = collider->rects_capacity * 2;
    EseRect **new_rects = memory_manager.realloc(
        collider->rects, sizeof(EseRect *) * new_capacity, MMTAG_ENTITY);
    collider->rects = new_rects;
    collider->rects_capacity = new_capacity;
  }

  // Shift elements to make space for the new component
  for (size_t i = collider->rects_count; i > index; --i) {
    collider->rects[i] = collider->rects[i - 1];
  }

  collider->rects[index] = rect;
  collider->rects_count++;
  ese_rect_ref(rect);

  // Register a watcher to automatically update bounds when rect properties
  // change
  ese_rect_add_watcher(rect, _entity_component_collider_rect_changed, collider);

  // Update entity's collision bounds after inserting rect
  entity_component_collider_update_bounds(collider);

  return 0;
}

/**
 * @brief Lua function to remove and return the last component.
 */
static int _entity_component_collider_rects_pop(lua_State *L) {
  EseEntityComponentCollider *collider =
      _entity_component_collider_rects_get_component(L, 1);
  if (!collider) {
    return luaL_error(L, "Invalid collider object.");
  }

  if (collider->rects_count == 0) {
    lua_pushnil(L);
    return 1;
  }

  EseRect *rect = collider->rects[collider->rects_count - 1];

  // Remove the watcher before removing the rect
  ese_rect_remove_watcher(rect, _entity_component_collider_rect_changed,
                          collider);
  ese_rect_unref(rect);

  collider->rects[collider->rects_count - 1] = NULL;
  collider->rects_count--;

  // Update entity's collision bounds after removing rect
  entity_component_collider_update_bounds(collider);

  // Push the rect to Lua using the proper API
  ese_rect_lua_push(rect);

  return 1;
}

/**
 * @brief Lua function to remove and return the first component.
 */
static int _entity_component_collider_rects_shift(lua_State *L) {
  EseEntityComponentCollider *collider =
      _entity_component_collider_rects_get_component(L, 1);
  if (!collider) {
    return luaL_error(L, "Invalid collider object.");
  }

  if (collider->rects_count == 0) {
    lua_pushnil(L);
    return 1;
  }

  EseRect *rect = collider->rects[0];

  // Remove the watcher before removing the rect
  ese_rect_remove_watcher(rect, _entity_component_collider_rect_changed,
                          collider);
  ese_rect_unref(rect);

  // Shift all elements
  for (size_t i = 0; i < collider->rects_count - 1; ++i) {
    collider->rects[i] = collider->rects[i + 1];
  }

  collider->rects_count--;
  collider->rects[collider->rects_count] = NULL;

  // Update entity's collision bounds after removing rect
  entity_component_collider_update_bounds(collider);

  // Push the rect to Lua using the proper API
  ese_rect_lua_push(rect);

  return 1;
}

/**
 * @brief Lua __index metamethod for EseEntityComponentCollider objects
 * (getter).
 *
 * @details Handles property access for EseEntityComponentCollider objects from
 * Lua.
 *
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack (1 for valid properties,
 * 0 otherwise)
 */
static int _entity_component_collider_index(lua_State *L) {
  EseEntityComponentCollider *component = _entity_component_collider_get(L, 1);
  const char *key = lua_tostring(L, 2);

  // SAFETY: Return nil for freed components
  if (!component) {
    lua_pushnil(L);
    return 1;
  }

  if (!key)
    return 0;

  if (strcmp(key, "active") == 0) {
    lua_pushboolean(L, component->base.active);
    return 1;
  } else if (strcmp(key, "id") == 0) {
    lua_pushstring(L, ese_uuid_get_value(component->base.id));
    return 1;
  } else if (strcmp(key, "draw_debug") == 0) {
    lua_pushboolean(L, component->draw_debug);
    return 1;
  } else if (strcmp(key, "map_interaction") == 0) {
    lua_pushboolean(L, component->map_interaction);
    return 1;
  } else if (strcmp(key, "offset") == 0) {
    ese_point_lua_push(component->offset);
    return 1;
  } else if (strcmp(key, "rects") == 0) {
    // Create rects proxy userdata
    EseEntityComponentCollider **ud =
        (EseEntityComponentCollider **)lua_newuserdata(
            L, sizeof(EseEntityComponentCollider *));
    *ud = component;

    // Attach metatable
    luaL_getmetatable(L, "ColliderRectsProxyMeta");
    lua_setmetatable(L, -2);
    return 1;
  }

  return 0;
}

/**
 * @brief Lua __newindex metamethod for EseEntityComponentCollider objects
 * (setter).
 *
 * @details Handles property assignment for EseEntityComponentCollider objects
 * from Lua.
 *
 * @param L Lua state pointer
 * @return Always returns 0 (no return values) or throws Lua error for invalid
 * operations
 */
static int _entity_component_collider_newindex(lua_State *L) {
  EseEntityComponentCollider *component = _entity_component_collider_get(L, 1);
  const char *key = lua_tostring(L, 2);

  // SAFETY: Silently ignore writes to freed components
  if (!component) {
    return 0;
  }

  if (!key)
    return 0;

  if (strcmp(key, "active") == 0) {
    if (!lua_isboolean(L, 3)) {
      return luaL_error(L, "active must be a boolean");
    }
    component->base.active = lua_toboolean(L, 3);
    lua_pushboolean(L, component->base.active);
    return 1;
  } else if (strcmp(key, "id") == 0) {
    return luaL_error(L, "id is read-only");
  } else if (strcmp(key, "offset") == 0) {
    EsePoint *new_position_point = ese_point_lua_get(L, 3);
    if (!new_position_point) {
      return luaL_error(L, "Collider offset must be a EsePoint object");
    }
    // Copy values, don't copy reference (ownership safety)
    ese_point_set_x(component->offset, ese_point_get_x(new_position_point));
    ese_point_set_y(component->offset, ese_point_get_y(new_position_point));
    // Pop the point off the stack
    lua_pop(L, 1);
    return 0;
  } else if (strcmp(key, "draw_debug") == 0) {
    if (!lua_isboolean(L, 3)) {
      return luaL_error(L, "draw_debug must be a boolean");
    }
    component->draw_debug = lua_toboolean(L, 3);
    lua_pushboolean(L, component->draw_debug);
    return 1;
  } else if (strcmp(key, "map_interaction") == 0) {
    if (!lua_isboolean(L, 3)) {
      return luaL_error(L, "map_interaction must be a boolean");
    }
    component->map_interaction = lua_toboolean(L, 3);
    lua_pushboolean(L, component->map_interaction);
    return 1;
  } else if (strcmp(key, "rects") == 0) {
    return luaL_error(L, "rects is not assignable");
  }

  return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __index metamethod for EseEntityComponentCollider rects collection
 * (getter).
 *
 * @details Handles property access for EseEntityComponentCollider rects
 * collection from Lua.
 *
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack (1 for valid properties,
 * 0 otherwise)
 */
static int _entity_component_collider_rects_rects_index(lua_State *L) {
  EseEntityComponentCollider *component =
      _entity_component_collider_rects_get_component(L, 1);
  if (!component) {
    lua_pushnil(L);
    return 1;
  }

  // Check if it's a number (array access)
  if (lua_isnumber(L, 2)) {
    int index = (int)lua_tointeger(L, 2) - 1; // Convert to 0-based
    if (index >= 0 && index < (int)component->rects_count) {
      EseRect *rect = component->rects[index];

      // Push the rect using the proper API
      ese_rect_lua_push(rect);
      return 1;
    } else {
      lua_pushnil(L);
      return 1;
    }
  }

  // Check for method names
  const char *key = lua_tostring(L, 2);
  if (!key)
    return 0;

  if (strcmp(key, "count") == 0) {
    lua_pushinteger(L, component->rects_count);
    return 1;
  } else if (strcmp(key, "add") == 0) {
    lua_pushlightuserdata(L, component);
    lua_pushcclosure(L, _entity_component_collider_rects_add, 1);
    return 1;
  } else if (strcmp(key, "remove") == 0) {
    lua_pushcfunction(L, _entity_component_collider_rects_remove);
    return 1;
  } else if (strcmp(key, "insert") == 0) {
    lua_pushcfunction(L, _entity_component_collider_rects_insert);
    return 1;
  } else if (strcmp(key, "pop") == 0) {
    lua_pushcfunction(L, _entity_component_collider_rects_pop);
    return 1;
  } else if (strcmp(key, "shift") == 0) {
    lua_pushcfunction(L, _entity_component_collider_rects_shift);
    return 1;
  }

  return 0;
}

/**
 * @brief Lua __gc metamethod for EseEntityComponentCollider objects.
 *
 * @details For userdata-based components, we don't need to check ownership
 * flags. The component will be destroyed when its reference count reaches zero.
 *
 * @param L Lua state pointer
 * @return Always 0 (no return values)
 */
static int _entity_component_collider_gc(lua_State *L) {
  // Get from userdata
  EseEntityComponentCollider **ud =
      (EseEntityComponentCollider **)luaL_testudata(
          L, 1, ENTITY_COMPONENT_COLLIDER_PROXY_META);
  if (!ud) {
    return 0; // Not our userdata
  }

  EseEntityComponentCollider *component = *ud;
  if (component) {
    // If lua_ref == LUA_NOREF, there are no more references to this component,
    // so we can free it.
    // If lua_ref != LUA_NOREF, this component was referenced from C and should
    // not be freed.
    if (component->base.lua_ref == LUA_NOREF) {
      _entity_component_collider_destroy(component);
    }
  }

  return 0;
}

static int _entity_component_collider_tostring(lua_State *L) {
  EseEntityComponentCollider *component = _entity_component_collider_get(L, 1);

  if (!component) {
    lua_pushstring(L, "EntityComponentCollider: (invalid)");
    return 1;
  }

  char buf[128];
  snprintf(buf, sizeof(buf),
           "EntityComponentCollider: %p (id=%s active=%s draw_debug=%s)",
           (void *)component, ese_uuid_get_value(component->base.id),
           component->base.active ? "true" : "false",
           component->draw_debug ? "true" : "false");
  lua_pushstring(L, buf);

  return 1;
}

void _entity_component_collider_init(EseLuaEngine *engine) {
  log_assert("ENTITY_COMP", engine,
             "_entity_component_collider_init called with NULL engine");

  // Create main metatable
  lua_engine_new_object_meta(
      engine, ENTITY_COMPONENT_COLLIDER_PROXY_META,
      _entity_component_collider_index, _entity_component_collider_newindex,
      _entity_component_collider_gc, _entity_component_collider_tostring);

  // Create global EntityComponentCollider table with functions
  const char *keys[] = {"new"};
  lua_CFunction functions[] = {_entity_component_collider_new};
  lua_engine_new_object(engine, "EntityComponentCollider", 1, keys, functions);

  // Create ColliderRectsProxyMeta metatable
  lua_engine_new_object_meta(engine, "ColliderRectsProxyMeta",
                             _entity_component_collider_rects_rects_index, NULL,
                             NULL, NULL);
}

EseEntityComponent *entity_component_collider_create(EseLuaEngine *engine) {
  log_assert("ENTITY_COMP", engine,
             "entity_component_collider_create called with NULL engine");

  EseEntityComponent *component = _entity_component_collider_make(engine);

  // Register with Lua using ref system
  entity_component_collider_ref((EseEntityComponentCollider *)component->data);

  return component;
}

static void _entity_component_collider_rect_changed(EseRect *rect,
                                                    void *userdata) {
  EseEntityComponentCollider *collider = (EseEntityComponentCollider *)userdata;
  if (collider) {
    entity_component_collider_update_bounds(collider);
  }
}

void entity_component_collider_rects_add(EseEntityComponentCollider *collider,
                                         EseRect *rect) {
  log_assert("ENTITY", collider,
             "entity_component_collider_rects_add called with NULL collider");
  log_assert("ENTITY", rect,
             "entity_component_collider_rects_add called with NULL rect");

  if (collider->rects_count == collider->rects_capacity) {
    size_t new_capacity = collider->rects_capacity * 2;
    EseRect **new_rects = memory_manager.realloc(
        collider->rects, sizeof(EseRect *) * new_capacity, MMTAG_ENTITY);
    collider->rects = new_rects;
    collider->rects_capacity = new_capacity;
  }

  collider->rects[collider->rects_count++] = rect;
  ese_rect_ref(rect);

  // Register a watcher to automatically update bounds when rect properties
  // change
  ese_rect_add_watcher(rect, _entity_component_collider_rect_changed, collider);

  // Update entity's collision bounds after adding rect
  entity_component_collider_update_bounds(collider);
}

void entity_component_collider_update_bounds(
    EseEntityComponentCollider *collider) {
  log_assert(
      "ENTITY", collider,
      "entity_component_collider_update_bounds called with NULL collider");

  // If component isn't attached to an entity yet, skip bounds update
  if (!collider->base.entity) {
    return;
  }

  if (collider->rects_count == 0) {
    // No rects, clear both collision bounds
    if (collider->base.entity->collision_bounds) {
      ese_rect_destroy(collider->base.entity->collision_bounds);
      collider->base.entity->collision_bounds = NULL;
    }
    if (collider->base.entity->collision_world_bounds) {
      ese_rect_destroy(collider->base.entity->collision_world_bounds);
      collider->base.entity->collision_world_bounds = NULL;
    }
    return;
  }

  // Compute bounds from all rects in this collider (relative to entity)
  float min_x = INFINITY, min_y = INFINITY, max_x = -INFINITY,
        max_y = -INFINITY;

  for (size_t i = 0; i < collider->rects_count; i++) {
    EseRect *r = collider->rects[i];
    if (!r)
      continue;

    // Get rect properties
    float rx = ese_rect_get_x(r) + ese_point_get_x(collider->offset);
    float ry = ese_rect_get_y(r) + ese_point_get_y(collider->offset);
    float rw = ese_rect_get_width(r);
    float rh = ese_rect_get_height(r);
    float rotation = ese_rect_get_rotation(r);

    // Calculate the bounding box of the rotated rectangle
    if (fabsf(rotation) < 1e-6f) {
      // No rotation - simple AABB
      min_x = fminf(min_x, rx);
      min_y = fminf(min_y, ry);
      max_x = fmaxf(max_x, rx + rw);
      max_y = fmaxf(max_y, ry + rh);
    } else {
      // Rotated rectangle - calculate all 4 corners and find bounding box
      float cx = rx + rw * 0.5f; // Center X
      float cy = ry + rh * 0.5f; // Center Y
      float half_w = rw * 0.5f;
      float half_h = rh * 0.5f;

      float cos_r = cosf(rotation);
      float sin_r = sinf(rotation);

      // Calculate the 4 corners of the rotated rectangle
      float corners_x[4], corners_y[4];

      // Corner 1: (-half_w, -half_h)
      corners_x[0] = cx + cos_r * (-half_w) - sin_r * (-half_h);
      corners_y[0] = cy + sin_r * (-half_w) + cos_r * (-half_h);

      // Corner 2: (+half_w, -half_h)
      corners_x[1] = cx + cos_r * (half_w)-sin_r * (-half_h);
      corners_y[1] = cy + sin_r * (half_w) + cos_r * (-half_h);

      // Corner 3: (+half_w, +half_h)
      corners_x[2] = cx + cos_r * (half_w)-sin_r * (half_h);
      corners_y[2] = cy + sin_r * (half_w) + cos_r * (half_h);

      // Corner 4: (-half_w, +half_h)
      corners_x[3] = cx + cos_r * (-half_w) - sin_r * (half_h);
      corners_y[3] = cy + sin_r * (-half_w) + cos_r * (half_h);

      // Find the bounding box of all corners
      for (int j = 0; j < 4; j++) {
        min_x = fminf(min_x, corners_x[j]);
        min_y = fminf(min_y, corners_y[j]);
        max_x = fmaxf(max_x, corners_x[j]);
        max_y = fmaxf(max_y, corners_y[j]);
      }
    }
  }

  // Create or update the collision bounds (relative to entity)
  if (!collider->base.entity->collision_bounds) {
    collider->base.entity->collision_bounds =
        ese_rect_create(collider->base.lua);
    ese_rect_ref(collider->base.entity->collision_bounds);
  }

  EseRect *bounds = collider->base.entity->collision_bounds;
  ese_rect_set_x(bounds, min_x);
  ese_rect_set_y(bounds, min_y);
  ese_rect_set_width(bounds, max_x - min_x);
  ese_rect_set_height(bounds, max_y - min_y);
  ese_rect_set_rotation(bounds, 0.0f); // Collision bounds are axis-aligned

  // Create or update the collision world bounds
  if (!collider->base.entity->collision_world_bounds) {
    collider->base.entity->collision_world_bounds =
        ese_rect_create(collider->base.lua);
    ese_rect_ref(collider->base.entity->collision_world_bounds);
  }

  EseRect *world_bounds = collider->base.entity->collision_world_bounds;
  ese_rect_set_x(world_bounds,
                 min_x + ese_point_get_x(collider->base.entity->position));
  ese_rect_set_y(world_bounds,
                 min_y + ese_point_get_y(collider->base.entity->position));
  ese_rect_set_width(world_bounds, max_x - min_x);
  ese_rect_set_height(world_bounds, max_y - min_y);
  ese_rect_set_rotation(world_bounds,
                        0.0f); // Collision bounds are axis-aligned
}

void entity_component_collider_rect_updated(
    EseEntityComponentCollider *collider) {
  log_assert(
      "ENTITY", collider,
      "entity_component_collider_rect_updated called with NULL collider");

  // Simply call the bounds update function
  entity_component_collider_update_bounds(collider);
}

void entity_component_collider_position_changed(
    EseEntityComponentCollider *collider) {
  log_assert(
      "ENTITY", collider,
      "entity_component_collider_position_changed called with NULL collider");

  // Update bounds since entity position affects all rect world positions
  entity_component_collider_update_bounds(collider);
}

void entity_component_collider_update_world_bounds_only(
    EseEntityComponentCollider *collider) {
  log_assert("ENTITY", collider,
             "entity_component_collider_update_world_bounds_only called with "
             "NULL collider");

  // If component isn't attached to an entity yet, skip bounds update
  if (!collider->base.entity) {
    return;
  }

  // If no entity bounds exist, can't update world bounds
  if (!collider->base.entity->collision_bounds) {
    return;
  }

  // Update ONLY the world bounds based on current entity position and entity
  // bounds
  if (!collider->base.entity->collision_world_bounds) {
    collider->base.entity->collision_world_bounds =
        ese_rect_create(collider->base.lua);
  }

  EseRect *entity_bounds = collider->base.entity->collision_bounds;
  EseRect *world_bounds = collider->base.entity->collision_world_bounds;

  // Copy entity bounds to world bounds and add entity position offset
  ese_rect_set_x(world_bounds,
                 ese_rect_get_x(entity_bounds) +
                     ese_point_get_x(collider->base.entity->position));
  ese_rect_set_y(world_bounds,
                 ese_rect_get_y(entity_bounds) +
                     ese_point_get_y(collider->base.entity->position));
  ese_rect_set_width(world_bounds, ese_rect_get_width(entity_bounds));
  ese_rect_set_height(world_bounds, ese_rect_get_height(entity_bounds));
  ese_rect_set_rotation(world_bounds, ese_rect_get_rotation(entity_bounds));
}

bool entity_component_collider_get_draw_debug(
    EseEntityComponentCollider *collider) {
  log_assert(
      "ENTITY_COMP", collider,
      "entity_component_collider_get_draw_debug called with NULL collider");

  return collider->draw_debug;
}

void entity_component_collider_set_draw_debug(
    EseEntityComponentCollider *collider, bool draw_debug) {
  log_assert(
      "ENTITY_COMP", collider,
      "entity_component_collider_set_draw_debug called with NULL collider");

  collider->draw_debug = draw_debug;
}

bool entity_component_collider_get_map_interaction(
    EseEntityComponentCollider *collider) {
  log_assert("ENTITY_COMP", collider,
             "entity_component_collider_get_map_interaction called with NULL "
             "collider");
  return collider->map_interaction;
}

void entity_component_collider_set_map_interaction(
    EseEntityComponentCollider *collider, bool enabled) {
  log_assert("ENTITY_COMP", collider,
             "entity_component_collider_set_map_interaction called with NULL "
             "collider");
  collider->map_interaction = enabled;
}
