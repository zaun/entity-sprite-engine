#include "types/collision_hit.h"
#include "core/memory_manager.h"
#include "entity/entity.h"
#include "scripting/lua_engine.h"
#include "scripting/lua_engine_private.h"
#include "scripting/lua_value.h"
#include "types/collision_hit_lua.h"
#include "types/map.h"
#include "types/rect.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <stdio.h>
#include <string.h>

// ========================================
// PRIVATE STRUCT DEFINITION
// ========================================

// Internal opaque definition (not exposed in header)
struct EseCollisionHit {
  EseCollisionKind kind; // CC or CM
  EseEntity *entity;     // hitter, not owned
  EseEntity *target;     // hittee, not owned
  struct {
    // COLLIDER kind data
    EseRect *rect; // we own
    // MAP kind data
    EseMap *map;         // not owned
    EseLuaValue *cell_x; // we own
    EseLuaValue *cell_y; // we own
  } data;
  EseCollisionState state;

  // Lua integration
  lua_State *state_ptr;
  int lua_ref;
  int lua_ref_count;
};

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

/// Internal creation helper
static EseCollisionHit *_ese_collision_hit_make(void);

/// Private setters
static void _ese_collision_hit_set_lua_ref(EseCollisionHit *hit, int lua_ref);
static void _ese_collision_hit_set_lua_ref_count(EseCollisionHit *hit,
                                                 int lua_ref_count);
static void _ese_collision_hit_set_state_ptr(EseCollisionHit *hit,
                                             lua_State *state_ptr);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Internal helpers
/**
 * @brief Creates a new EseCollisionHit with default values.
 */
static EseCollisionHit *_ese_collision_hit_make(void) {
  EseCollisionHit *hit = (EseCollisionHit *)memory_manager.malloc(
      sizeof(EseCollisionHit), MMTAG_COLLISION_INDEX);
  hit->kind = COLLISION_KIND_COLLIDER;
  hit->entity = NULL;
  hit->target = NULL;
  // Initialize all union members to NULL to avoid invalid frees when switching
  // kinds
  hit->data.rect = NULL;
  hit->data.map = NULL;
  hit->data.cell_x = NULL;
  hit->data.cell_y = NULL;
  hit->state = COLLISION_STATE_ENTER;
  _ese_collision_hit_set_state_ptr(hit, NULL);
  _ese_collision_hit_set_lua_ref(hit, LUA_NOREF);
  _ese_collision_hit_set_lua_ref_count(hit, 0);
  return hit;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseCollisionHit *ese_collision_hit_create(EseLuaEngine *engine) {
  log_assert("COLLISION_HIT", engine,
             "ese_collision_hit_create called with NULL engine");
  EseCollisionHit *hit = _ese_collision_hit_make();
  _ese_collision_hit_set_state_ptr(hit, engine->runtime);
  return hit;
}

EseCollisionHit *ese_collision_hit_copy(const EseCollisionHit *src) {
  log_assert("COLLISION_HIT", src,
             "ese_collision_hit_copy called with NULL src");

  // Recover engine from Lua state and create a fresh instance
  EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(
      ese_collision_hit_get_state_ptr(src), LUA_ENGINE_KEY);
  log_assert("COLLISION_HIT", engine,
             "ese_collision_hit_copy could not resolve engine from Lua state");

  EseCollisionHit *copy = ese_collision_hit_create(engine);

  // Copy simple fields
  ese_collision_hit_set_kind(copy, ese_collision_hit_get_kind(src));
  ese_collision_hit_set_state(copy, ese_collision_hit_get_state(src));
  ese_collision_hit_set_entity(copy, ese_collision_hit_get_entity(src));
  ese_collision_hit_set_target(copy, ese_collision_hit_get_target(src));

  // Copy kind-specific data
  if (ese_collision_hit_get_kind(src) == COLLISION_KIND_COLLIDER) {
    if (ese_collision_hit_get_rect(src)) {
      ese_collision_hit_set_rect(
          copy,
          ese_collision_hit_get_rect(src)); // performs deep copy internally
    }
  } else if (ese_collision_hit_get_kind(src) == COLLISION_KIND_MAP) {
    ese_collision_hit_set_map(copy, ese_collision_hit_get_map(src));
    int cx = ese_collision_hit_get_cell_x(src);
    int cy = ese_collision_hit_get_cell_y(src);
    ese_collision_hit_set_cell_x(copy, cx);
    ese_collision_hit_set_cell_y(copy, cy);
  }

  return copy;
}

void ese_collision_hit_destroy(EseCollisionHit *hit) {
  if (!hit)
    return;
  if (ese_collision_hit_get_lua_ref(hit) == LUA_NOREF) {
    // Free owned resources depending on kind
    if (ese_collision_hit_get_kind(hit) == COLLISION_KIND_COLLIDER) {
      if (ese_collision_hit_get_rect(hit)) {
        ese_rect_destroy(ese_collision_hit_get_rect(hit));
      }
    } else if (ese_collision_hit_get_kind(hit) == COLLISION_KIND_MAP) {
      if (hit->data.cell_x) {
        lua_value_destroy(hit->data.cell_x);
        hit->data.cell_x = NULL;
      }
      if (hit->data.cell_y) {
        lua_value_destroy(hit->data.cell_y);
        hit->data.cell_y = NULL;
      }
      // Map pointer is not owned
    }

    memory_manager.free(hit);
  } else {
    ese_collision_hit_unref(hit);
  }
}

// Lua integration
void ese_collision_hit_lua_init(EseLuaEngine *engine) {
  log_assert("COLLISION_HIT", engine,
             "ese_collision_hit_lua_init called with NULL engine");

  _ese_collision_hit_lua_init(engine);
}

void ese_collision_hit_lua_push(EseCollisionHit *hit) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_lua_push called with NULL hit");
  if (ese_collision_hit_get_lua_ref(hit) == LUA_NOREF) {
    EseCollisionHit **ud = (EseCollisionHit **)lua_newuserdata(
        ese_collision_hit_get_state_ptr(hit), sizeof(EseCollisionHit *));
    *ud = hit;
    luaL_getmetatable(ese_collision_hit_get_state_ptr(hit), COLLISION_HIT_META);
    lua_setmetatable(ese_collision_hit_get_state_ptr(hit), -2);
  } else {
    lua_rawgeti(ese_collision_hit_get_state_ptr(hit), LUA_REGISTRYINDEX,
                ese_collision_hit_get_lua_ref(hit));
  }
}

EseCollisionHit *ese_collision_hit_lua_get(lua_State *L, int idx) {
  log_assert("COLLISION_HIT", L,
             "ese_collision_hit_lua_get called with NULL Lua state");
  if (!lua_isuserdata(L, idx))
    return NULL;
  EseCollisionHit **ud =
      (EseCollisionHit **)luaL_testudata(L, idx, COLLISION_HIT_META);
  if (!ud)
    return NULL;
  return *ud;
}

void ese_collision_hit_ref(EseCollisionHit *hit) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_ref called with NULL hit");
  if (ese_collision_hit_get_lua_ref(hit) == LUA_NOREF) {
    EseCollisionHit **ud = (EseCollisionHit **)lua_newuserdata(
        ese_collision_hit_get_state_ptr(hit), sizeof(EseCollisionHit *));
    *ud = hit;
    luaL_getmetatable(ese_collision_hit_get_state_ptr(hit), COLLISION_HIT_META);
    lua_setmetatable(ese_collision_hit_get_state_ptr(hit), -2);
    int ref = luaL_ref(ese_collision_hit_get_state_ptr(hit), LUA_REGISTRYINDEX);
    _ese_collision_hit_set_lua_ref(hit, ref);
    _ese_collision_hit_set_lua_ref_count(hit, 1);
  } else {
    _ese_collision_hit_set_lua_ref_count(
        hit, ese_collision_hit_get_lua_ref_count(hit) + 1);
  }
}

void ese_collision_hit_unref(EseCollisionHit *hit) {
  if (!hit)
    return;
  if (ese_collision_hit_get_lua_ref(hit) != LUA_NOREF &&
      ese_collision_hit_get_lua_ref_count(hit) > 0) {
    _ese_collision_hit_set_lua_ref_count(
        hit, ese_collision_hit_get_lua_ref_count(hit) - 1);
    if (ese_collision_hit_get_lua_ref_count(hit) == 0) {
      luaL_unref(ese_collision_hit_get_state_ptr(hit), LUA_REGISTRYINDEX,
                 ese_collision_hit_get_lua_ref(hit));
      _ese_collision_hit_set_lua_ref(hit, LUA_NOREF);
    }
  }
}

// Property access
EseCollisionKind ese_collision_hit_get_kind(const EseCollisionHit *hit) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_get_kind called with NULL hit");
  return hit->kind;
}

/// Sets the collision kind and clears non-matching data.
void ese_collision_hit_set_kind(EseCollisionHit *hit, EseCollisionKind kind) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_set_kind called with NULL hit");

  // Clear non-matching union data when switching kinds
  if (kind == COLLISION_KIND_COLLIDER) {
    // We own cell_x/cell_y; destroy if present
    if (hit->data.cell_x) {
      lua_value_destroy(hit->data.cell_x);
      hit->data.cell_x = NULL;
    }
    if (hit->data.cell_y) {
      lua_value_destroy(hit->data.cell_y);
      hit->data.cell_y = NULL;
    }
    // Map is not owned
    hit->data.map = NULL;
  } else if (kind == COLLISION_KIND_MAP) {
    if (hit->data.rect) {
      ese_rect_destroy(hit->data.rect);
      hit->data.rect = NULL;
    }
  }

  hit->kind = kind;
}

EseCollisionState ese_collision_hit_get_state(const EseCollisionHit *hit) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_get_state called with NULL hit");

  return hit->state;
}

void ese_collision_hit_set_state(EseCollisionHit *hit,
                                 EseCollisionState state) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_set_state called with NULL hit");

  hit->state = state;
}

EseEntity *ese_collision_hit_get_entity(const EseCollisionHit *hit) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_get_entity called with NULL hit");

  return hit->entity;
}

void ese_collision_hit_set_entity(EseCollisionHit *hit, EseEntity *entity) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_set_entity called with NULL hit");

  hit->entity = entity;
}

EseEntity *ese_collision_hit_get_target(const EseCollisionHit *hit) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_get_target called with NULL hit");

  return hit->target;
}

void ese_collision_hit_set_target(EseCollisionHit *hit, EseEntity *target) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_set_target called with NULL hit");

  hit->target = target;
}

void ese_collision_hit_set_rect(EseCollisionHit *hit, const EseRect *rect) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_set_rect called with NULL hit");
  log_assert("COLLISION_HIT", hit->kind == COLLISION_KIND_COLLIDER,
             "ese_collision_hit_set_rect called with non-collider hit");

  if (rect == NULL) {
    if (hit->data.rect) {
      ese_rect_destroy(hit->data.rect);
      hit->data.rect = NULL;
    }
    return;
  }

  if (hit->data.rect) {
    ese_rect_destroy(hit->data.rect);
    hit->data.rect = NULL;
  }

  hit->data.rect = ese_rect_copy(rect);
}

EseRect *ese_collision_hit_get_rect(const EseCollisionHit *hit) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_get_rect called with NULL hit");
  log_assert("COLLISION_HIT", hit->kind == COLLISION_KIND_COLLIDER,
             "ese_collision_hit_get_rect called with non-collider hit");

  return hit->data.rect;
}

void ese_collision_hit_set_map(EseCollisionHit *hit, EseMap *map) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_set_map called with NULL hit");
  log_assert("COLLISION_HIT", hit->kind == COLLISION_KIND_MAP,
             "ese_collision_hit_set_map called with non-map hit");

  hit->data.map = map;
}

EseMap *ese_collision_hit_get_map(const EseCollisionHit *hit) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_get_map called with NULL hit");
  log_assert("COLLISION_HIT", hit->kind == COLLISION_KIND_MAP,
             "ese_collision_hit_get_map called with non-map hit");

  return hit->data.map;
}

void ese_collision_hit_set_cell_x(EseCollisionHit *hit, int cell_x) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_set_cell_x called with NULL hit");
  log_assert("COLLISION_HIT", hit->kind == COLLISION_KIND_MAP,
             "ese_collision_hit_set_cell_x called with non-map hit");

  // Replace owned value safely
  if (hit->data.cell_x) {
    lua_value_destroy(hit->data.cell_x);
    hit->data.cell_x = NULL;
  }
  hit->data.cell_x = lua_value_create_number("cell_x", (double)cell_x);
}

int ese_collision_hit_get_cell_x(const EseCollisionHit *hit) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_get_cell_x called with NULL hit");
  log_assert("COLLISION_HIT", hit->kind == COLLISION_KIND_MAP,
             "ese_collision_hit_get_cell_x called with non-map hit");

  return hit->data.cell_x ? (int)lua_value_get_number(hit->data.cell_x) : 0;
}

void ese_collision_hit_set_cell_y(EseCollisionHit *hit, int cell_y) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_set_cell_y called with NULL hit");
  log_assert("COLLISION_HIT", hit->kind == COLLISION_KIND_MAP,
             "ese_collision_hit_set_cell_y called with non-map hit");

  // Replace owned value safely
  if (hit->data.cell_y) {
    lua_value_destroy(hit->data.cell_y);
    hit->data.cell_y = NULL;
  }
  hit->data.cell_y = lua_value_create_number("cell_y", (double)cell_y);
}

int ese_collision_hit_get_cell_y(const EseCollisionHit *hit) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_get_cell_y called with NULL hit");
  log_assert("COLLISION_HIT", hit->kind == COLLISION_KIND_MAP,
             "ese_collision_hit_get_cell_y called with non-map hit");

  return hit->data.cell_y ? (int)lua_value_get_number(hit->data.cell_y) : 0;
}

lua_State *ese_collision_hit_get_state_ptr(const EseCollisionHit *hit) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_get_state_ptr called with NULL hit");
  return hit->state_ptr;
}

int ese_collision_hit_get_lua_ref(const EseCollisionHit *hit) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_get_lua_ref called with NULL hit");
  return hit->lua_ref;
}

int ese_collision_hit_get_lua_ref_count(const EseCollisionHit *hit) {
  log_assert("COLLISION_HIT", hit,
             "ese_collision_hit_get_lua_ref_count called with NULL hit");
  return hit->lua_ref_count;
}

/**
 * @brief Sets the Lua registry reference for the collision hit (private)
 *
 * @param hit Pointer to the EseCollisionHit object
 * @param lua_ref The new Lua registry reference value
 */
static void _ese_collision_hit_set_lua_ref(EseCollisionHit *hit, int lua_ref) {
  log_assert("COLLISION_HIT", hit != NULL,
             "_ese_collision_hit_set_lua_ref: hit cannot be NULL");
  hit->lua_ref = lua_ref;
}

/**
 * @brief Sets the Lua reference count for the collision hit (private)
 *
 * @param hit Pointer to the EseCollisionHit object
 * @param lua_ref_count The new Lua reference count value
 */
static void _ese_collision_hit_set_lua_ref_count(EseCollisionHit *hit,
                                                 int lua_ref_count) {
  log_assert("COLLISION_HIT", hit != NULL,
             "_ese_collision_hit_set_lua_ref_count: hit cannot be NULL");
  hit->lua_ref_count = lua_ref_count;
}

/**
 * @brief Sets the Lua state associated with the collision hit (private)
 *
 * @param hit Pointer to the EseCollisionHit object
 * @param state_ptr The new Lua state value
 */
static void _ese_collision_hit_set_state_ptr(EseCollisionHit *hit,
                                             lua_State *state_ptr) {
  log_assert("COLLISION_HIT", hit != NULL,
             "_ese_collision_hit_set_state_ptr: hit cannot be NULL");
  hit->state_ptr = state_ptr;
}
