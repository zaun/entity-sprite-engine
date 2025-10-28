#include "entity/components/entity_component_sprite.h"
#include "core/asset_manager.h"
#include "core/engine.h"
#include "core/memory_manager.h"
#include "entity/components/entity_component.h"
#include "entity/components/entity_component_lua.h"
#include "entity/components/entity_component_private.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "graphics/sprite.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <string.h>

// VTable wrapper functions
static EseEntityComponent *_sprite_vtable_copy(EseEntityComponent *component) {
  return _entity_component_sprite_copy(
      (EseEntityComponentSprite *)component->data);
}

static void _sprite_vtable_destroy(EseEntityComponent *component) {
  _entity_component_sprite_destroy((EseEntityComponentSprite *)component->data);
}

static void _sprite_vtable_update(EseEntityComponent *component,
                                  EseEntity *entity, float delta_time) {
  (void)component;
  (void)entity;
  (void)delta_time;
}

static void _sprite_vtable_draw(EseEntityComponent *component, int screen_x,
                                int screen_y, void *callbacks,
                                void *user_data) {
  (void)component;
  (void)screen_x;
  (void)screen_y;
  (void)callbacks;
  (void)user_data;
}

static bool _sprite_vtable_run_function(EseEntityComponent *component,
                                        EseEntity *entity,
                                        const char *func_name, int argc,
                                        void *argv[]) {
  // Sprite components don't support function execution
  return false;
}

static void _sprite_vtable_collides_component(EseEntityComponent *a,
                                              EseEntityComponent *b,
                                              EseArray *out_hits) {
  (void)a;
  (void)b;
  (void)out_hits;
}

static void _sprite_vtable_ref(EseEntityComponent *component) {
  EseEntityComponentSprite *sprite =
      (EseEntityComponentSprite *)component->data;
  log_assert("ENTITY_COMP", sprite, "sprite vtable ref called with NULL");
  if (sprite->base.lua_ref == LUA_NOREF) {
    EseEntityComponentSprite **ud =
        (EseEntityComponentSprite **)lua_newuserdata(
            sprite->base.lua->runtime, sizeof(EseEntityComponentSprite *));
    *ud = sprite;
    luaL_getmetatable(sprite->base.lua->runtime,
                      ENTITY_COMPONENT_SPRITE_PROXY_META);
    lua_setmetatable(sprite->base.lua->runtime, -2);
    sprite->base.lua_ref =
        luaL_ref(sprite->base.lua->runtime, LUA_REGISTRYINDEX);
    sprite->base.lua_ref_count = 1;
  } else {
    sprite->base.lua_ref_count++;
  }
}

static void _sprite_vtable_unref(EseEntityComponent *component) {
  EseEntityComponentSprite *sprite =
      (EseEntityComponentSprite *)component->data;
  if (!sprite)
    return;
  if (sprite->base.lua_ref != LUA_NOREF && sprite->base.lua_ref_count > 0) {
    sprite->base.lua_ref_count--;
    if (sprite->base.lua_ref_count == 0) {
      luaL_unref(sprite->base.lua->runtime, LUA_REGISTRYINDEX,
                 sprite->base.lua_ref);
      sprite->base.lua_ref = LUA_NOREF;
    }
  }
}

// Static vtable instance for sprite components
static const ComponentVTable sprite_vtable = {
    .copy = _sprite_vtable_copy,
    .destroy = _sprite_vtable_destroy,
    .update = _sprite_vtable_update,
    .draw = _sprite_vtable_draw,
    .run_function = _sprite_vtable_run_function,
    .collides = _sprite_vtable_collides_component,
    .ref = _sprite_vtable_ref,
    .unref = _sprite_vtable_unref};

// (legacy table-proxy registration removed; using userdata + ref counting)

static EseEntityComponent *
_entity_component_sprite_make(EseLuaEngine *engine, const char *sprite_name) {
  EseEntityComponentSprite *component =
      memory_manager.malloc(sizeof(EseEntityComponentSprite), MMTAG_ENTITY);
  component->base.data = component;
  component->base.active = true;
  component->base.id = ese_uuid_create(engine);
  component->base.lua = engine;
  component->base.lua_ref = LUA_NOREF;
  component->base.lua_ref_count = 0;
  component->base.type = ENTITY_COMPONENT_SPRITE;
  component->base.vtable = &sprite_vtable;

  component->current_frame = 0;
  component->sprite_ellapse_time = 0;

  if (sprite_name != NULL) {
    component->sprite_name = memory_manager.strdup(sprite_name, MMTAG_ENTITY);
    log_debug("ENTITY_COMP", "Sprite component created with name '%s'",
              sprite_name);
  } else {
    component->sprite_name = NULL;
  }

  return &component->base;
}

EseEntityComponent *
_entity_component_sprite_copy(const EseEntityComponentSprite *src) {
  log_assert("ENTITY_COMP", src,
             "_entity_component_sprite_copy called with NULL src");

  EseEntityComponent *copy =
      _entity_component_sprite_make(src->base.lua, src->sprite_name);

  return copy;
}

void _entity_component_ese_sprite_cleanup(EseEntityComponentSprite *component) {
  memory_manager.free(component->sprite_name);
  ese_uuid_destroy(component->base.id);
  memory_manager.free(component);
  profile_count_add("entity_comp_sprite_destroy_count");
}

void _entity_component_sprite_destroy(EseEntityComponentSprite *component) {
  log_assert("ENTITY_COMP", component,
             "_entity_component_sprite_destroy called with NULL src");

  // Respect Lua registry ref-count; only free when no refs remain
  if (component->base.lua_ref != LUA_NOREF &&
      component->base.lua_ref_count > 0) {
    component->base.lua_ref_count--;
    if (component->base.lua_ref_count == 0) {
      luaL_unref(component->base.lua->runtime, LUA_REGISTRYINDEX,
                 component->base.lua_ref);
      component->base.lua_ref = LUA_NOREF;
      _entity_component_ese_sprite_cleanup(component);
    } else {
      // We dont "own" the sprite so dont free it}
      return;
    }
  } else if (component->base.lua_ref == LUA_NOREF) {
    _entity_component_ese_sprite_cleanup(component);
  }
}

/**
 * @brief Lua function to create a new EseEntityComponentSprite object.
 *
 * @details Callable from Lua as EseEntityComponentSprite.new().
 *
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new point object)
 *
 * @warning Items created in Lua are owned by Lua
 */
static int _entity_component_sprite_new(lua_State *L) {
  const char *sprite_name = NULL;

  int n_args = lua_gettop(L);
  if (n_args == 1 && lua_isstring(L, 1)) {
    sprite_name = lua_tostring(L, 1);
  } else if (n_args == 1 && !lua_isstring(L, 1)) {
    log_debug("ENTITY_COMP", "Script must be a string, ignored");
  } else if (n_args != 0) {
    log_debug("ENTITY_COMP",
              "EntityComponentLua.new() or EseEntityComponentLua.new(String)");
  }

  // Set engine reference
  EseLuaEngine *lua =
      (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);

  // Create EseEntityComponent wrapper
  EseEntityComponent *component =
      _entity_component_sprite_make(lua, sprite_name);

  // For Lua-created components, create userdata without storing a persistent
  // ref
  EseEntityComponentSprite **ud = (EseEntityComponentSprite **)lua_newuserdata(
      L, sizeof(EseEntityComponentSprite *));
  *ud = (EseEntityComponentSprite *)component->data;
  luaL_getmetatable(L, ENTITY_COMPONENT_SPRITE_PROXY_META);
  lua_setmetatable(L, -2);

  return 1;
}

EseEntityComponentSprite *_entity_component_sprite_get(lua_State *L, int idx) {
  // Check if it's userdata
  if (!lua_isuserdata(L, idx)) {
    return NULL;
  }

  // Get the userdata and check metatable
  EseEntityComponentSprite **ud = (EseEntityComponentSprite **)luaL_testudata(
      L, idx, ENTITY_COMPONENT_SPRITE_PROXY_META);
  if (!ud) {
    return NULL; // Wrong metatable or not userdata
  }

  return *ud;
}

/**
 * @brief Lua __index metamethod for EseEntityComponentSprite objects (getter).
 *
 * @details Handles property access for EseEntityComponentSprite objects from
 * Lua.
 *
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack (1 for valid properties,
 * 0 otherwise)
 */
static int _entity_component_sprite_index(lua_State *L) {
  EseEntityComponentSprite *component = _entity_component_sprite_get(L, 1);
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
  } else if (strcmp(key, "sprite") == 0) {
    lua_pushstring(L, component->sprite_name);
    return 1;
  }

  return 0;
}

/**
 * @brief Lua __newindex metamethod for EseEntityComponentSprite objects
 * (setter).
 *
 * @details Handles property assignment for EseEntityComponentSprite objects
 * from Lua.
 *
 * @param L Lua state pointer
 * @return Always returns 0 (no return values) or throws Lua error for invalid
 * operations
 */
static int _entity_component_sprite_newindex(lua_State *L) {
  EseEntityComponentSprite *component = _entity_component_sprite_get(L, 1);
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
    return 0;
  } else if (strcmp(key, "id") == 0) {
    return luaL_error(L, "id is read-only");
  } else if (strcmp(key, "sprite") == 0) {
    if (!lua_isstring(L, 3) && !lua_isnil(L, 3)) {
      return luaL_error(L, "sprite must be a string or nil");
    }

    if (component->sprite_name != NULL) {
      memory_manager.free(component->sprite_name);
      component->sprite_name = NULL;
    }

    if (lua_isstring(L, 3)) {
      const char *sprite_name = lua_tostring(L, 3);
      component->current_frame = 0;
      component->sprite_name = memory_manager.strdup(sprite_name, MMTAG_ENTITY);
      return 0;
    }
    // nil path handled: sprite name cleared above
    return 0;
  }

  return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __gc metamethod for EseEntityComponentSprite objects.
 *
 * @param L Lua state pointer
 * @return Always 0 (no return values)
 */
static int _entity_component_sprite_gc(lua_State *L) {
  // Get from userdata
  EseEntityComponentSprite **ud = (EseEntityComponentSprite **)luaL_testudata(
      L, 1, ENTITY_COMPONENT_SPRITE_PROXY_META);
  if (!ud) {
    return 0; // Not our userdata
  }

  EseEntityComponentSprite *component = *ud;
  if (component) {
    if (component->base.lua_ref == LUA_NOREF) {
      _entity_component_sprite_destroy(component);
      *ud = NULL;
    }
  }

  return 0;
}

static int _entity_component_sprite_tostring(lua_State *L) {
  EseEntityComponentSprite *component = _entity_component_sprite_get(L, 1);

  if (!component) {
    lua_pushstring(L, "EntityComponentSprite: (invalid)");
    return 1;
  }

  char buf[128];
  snprintf(buf, sizeof(buf),
           "EntityComponentSprite: id=%s active=%s sprite_name=%s",
           ese_uuid_get_value(component->base.id),
           component->base.active ? "true" : "false",
           component->sprite_name ? component->sprite_name : "nil");
  lua_pushstring(L, buf);

  return 1;
}

void _entity_component_sprite_init(EseLuaEngine *engine) {
  log_assert("ENTITY_COMP", engine,
             "_entity_component_sprite_init called with NULL engine");

  // Create metatable
  lua_engine_new_object_meta(
      engine, ENTITY_COMPONENT_SPRITE_PROXY_META,
      _entity_component_sprite_index, _entity_component_sprite_newindex,
      _entity_component_sprite_gc, _entity_component_sprite_tostring);

  // Create global EntityComponentSprite table with functions
  const char *keys[] = {"new"};
  lua_CFunction functions[] = {_entity_component_sprite_new};
  lua_engine_new_object(engine, "EntityComponentSprite", 1, keys, functions);
}

EseEntityComponent *entity_component_sprite_create(EseLuaEngine *engine,
                                                   const char *sprite_name) {
  log_assert("ENTITY_COMP", engine,
             "entity_component_sprite_create called with NULL engine");

  EseEntityComponent *component =
      _entity_component_sprite_make(engine, sprite_name);

  // Register with Lua using ref system
  component->vtable->ref(component);

  return component;
}
