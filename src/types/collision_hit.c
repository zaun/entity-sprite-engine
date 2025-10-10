#include <stdio.h>
#include <string.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "scripting/lua_engine_private.h"
#include "scripting/lua_value.h"
#include "types/collision_hit.h"
#include "types/map.h"
#include "types/rect.h"
#include "entity/entity.h"
#include "utility/log.h"
#include "utility/profile.h"

// ========================================
// PRIVATE STRUCT DEFINITION
// ========================================

// Internal opaque definition (not exposed in header)
struct EseCollisionHit {
    EseCollisionKind kind;          // CC or CM
    EseEntity *entity;              // hitter, not owned
    EseEntity *target;              // hittee, not owned
    struct {
        // COLLIDER kind data
        EseRect *rect;              // we own
        // MAP kind data
        EseMap *map;                // not owned
        EseLuaValue *cell_x;        // we own
        EseLuaValue *cell_y;        // we own
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

/// Lua: __gc metamethod for EseCollisionHit
static int _ese_collision_hit_lua_gc(lua_State *L);
/// Lua: __index metamethod for EseCollisionHit
static int _ese_collision_hit_lua_index(lua_State *L);
/// Lua: __newindex metamethod for EseCollisionHit
static int _ese_collision_hit_lua_newindex(lua_State *L);
/// Lua: __tostring metamethod for EseCollisionHit
static int _ese_collision_hit_lua_tostring(lua_State *L);
/// Register EseCollisionHit constants table into the global table
static void _register_collision_hit_constants(lua_State *L);

/// Internal creation helper
static EseCollisionHit *_ese_collision_hit_make(void);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

/**
 * @brief Registers constant tables for EseCollisionHit in the active Lua table.
 *
 * Pushes TYPE (COLLIDER, MAP) and STATE (ENTER, STAY, LEAVE) sub-tables.
 */
static void _register_collision_hit_constants(lua_State *L) {
    // EseCollisionHit.TYPE
    lua_newtable(L);
    lua_pushinteger(L, COLLISION_KIND_COLLIDER);
    lua_setfield(L, -2, "COLLIDER");
    lua_pushinteger(L, COLLISION_KIND_MAP);
    lua_setfield(L, -2, "MAP");
    lua_setfield(L, -2, "TYPE");

    // EseCollisionHit.STATE
    lua_newtable(L);
    lua_pushinteger(L, COLLISION_STATE_ENTER);
    lua_setfield(L, -2, "ENTER");
    lua_pushinteger(L, COLLISION_STATE_STAY);
    lua_setfield(L, -2, "STAY");
    lua_pushinteger(L, COLLISION_STATE_LEAVE);
    lua_setfield(L, -2, "LEAVE");
    lua_setfield(L, -2, "STATE");
}

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseCollisionHit
 *
 * Frees the underlying hit when there are no C-side references.
 */
static int _ese_collision_hit_lua_gc(lua_State *L) {
    EseCollisionHit **ud = (EseCollisionHit **)luaL_testudata(L, 1, COLLISION_HIT_META);
    if (!ud) return 0;
    EseCollisionHit *hit = *ud;
    if (hit && hit->lua_ref == LUA_NOREF) {
        ese_collision_hit_destroy(hit);
    }
    return 0;
}

/**
 * @brief Lua __index metamethod for EseCollisionHit property access
 *
 * Provides read access to hit properties (kind, state, entity, target) and
 * type-specific data (rect, map, cell_x, cell_y).
 */
static int _ese_collision_hit_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_COLLISION_HIT_INDEX);
    EseCollisionHit *hit = ese_collision_hit_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!hit || !key) { profile_cancel(PROFILE_LUA_COLLISION_HIT_INDEX); return 0; }

    if (strcmp(key, "kind") == 0) {
        lua_pushinteger(L, hit->kind);
        profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX, "collision_hit_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "state") == 0) {
        lua_pushinteger(L, hit->state);
        profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX, "collision_hit_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "entity") == 0) {
        entity_lua_push(hit->entity);
        profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX, "collision_hit_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "target") == 0) {
        entity_lua_push(hit->target);
        profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX, "collision_hit_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "rect") == 0) {
        if (hit->kind == COLLISION_KIND_COLLIDER && hit->data.rect) {
            ese_rect_lua_push(hit->data.rect);
            profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX, "collision_hit_lua_index (getter)");
            return 1;
        }
        profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX, "collision_hit_lua_index (invalid)");
        return 0;
    } else if (strcmp(key, "map") == 0) {
        if (hit->kind == COLLISION_KIND_MAP && hit->data.map) {
            ese_map_lua_push(hit->data.map);
            profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX, "collision_hit_lua_index (getter)");
            return 1;
        }
        profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX, "collision_hit_lua_index (invalid)");
        return 0;
    } else if (strcmp(key, "cell_x") == 0) {
        if (hit->kind == COLLISION_KIND_MAP && hit->data.cell_x) {
            _lua_engine_push_luavalue(L, hit->data.cell_x);
            profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX, "collision_hit_lua_index (getter)");
            return 1;
        }
        profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX, "collision_hit_lua_index (invalid)");
        return 0;
    } else if (strcmp(key, "cell_y") == 0) {
        if (hit->kind == COLLISION_KIND_MAP && hit->data.cell_y) {
            _lua_engine_push_luavalue(L, hit->data.cell_y);
            profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX, "collision_hit_lua_index (getter)");
            return 1;
        }
        profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX, "collision_hit_lua_index (invalid)");
        return 0;
    }

    profile_stop(PROFILE_LUA_COLLISION_HIT_INDEX, "collision_hit_lua_index (invalid)");
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseCollisionHit (read-only)
 */
static int _ese_collision_hit_lua_newindex(lua_State *L) {
    return luaL_error(L, "EseCollisionHit is read-only");
}

/**
 * @brief Lua __tostring metamethod for EseCollisionHit string representation
 */
static int _ese_collision_hit_lua_tostring(lua_State *L) {
    EseCollisionHit *hit = ese_collision_hit_lua_get(L, 1);
    if (!hit) { lua_pushstring(L, "EseCollisionHit: (invalid)"); return 1; }
    char buf[160];
    snprintf(buf, sizeof(buf), "EseCollisionHit: %p (kind=%d, state=%d)", (void*)hit, (int)hit->kind, (int)hit->state);
    lua_pushstring(L, buf);
    return 1;
}

// Internal helpers
/**
 * @brief Creates a new EseCollisionHit with default values.
 */
static EseCollisionHit *_ese_collision_hit_make(void) {
    EseCollisionHit *hit = (EseCollisionHit *)memory_manager.malloc(sizeof(EseCollisionHit), MMTAG_COLLISION_INDEX);
    hit->kind = COLLISION_KIND_COLLIDER;
    hit->entity = NULL;
    hit->target = NULL;
    // Initialize all union members to NULL to avoid invalid frees when switching kinds
    hit->data.rect = NULL;
    hit->data.map = NULL;
    hit->data.cell_x = NULL;
    hit->data.cell_y = NULL;
    hit->state = COLLISION_STATE_ENTER;
    hit->state_ptr = NULL;
    hit->lua_ref = LUA_NOREF;
    hit->lua_ref_count = 0;
    return hit;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseCollisionHit *ese_collision_hit_create(EseLuaEngine *engine) {
    log_assert("COLLISION_HIT", engine, "ese_collision_hit_create called with NULL engine");
    EseCollisionHit *hit = _ese_collision_hit_make();
    hit->state_ptr = engine->runtime;
    return hit;
}

EseCollisionHit *ese_collision_hit_copy(const EseCollisionHit *src) {
    log_assert("COLLISION_HIT", src, "ese_collision_hit_copy called with NULL src");

    // Recover engine from Lua state and create a fresh instance
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(src->state_ptr, LUA_ENGINE_KEY);
    log_assert("COLLISION_HIT", engine, "ese_collision_hit_copy could not resolve engine from Lua state");

    EseCollisionHit *copy = ese_collision_hit_create(engine);

    // Copy simple fields
    ese_collision_hit_set_kind(copy, src->kind);
    ese_collision_hit_set_state(copy, src->state);
    ese_collision_hit_set_entity(copy, src->entity);
    ese_collision_hit_set_target(copy, src->target);

    // Copy kind-specific data
    if (src->kind == COLLISION_KIND_COLLIDER) {
        if (src->data.rect) {
            ese_collision_hit_set_rect(copy, src->data.rect); // performs deep copy internally
        }
    } else if (src->kind == COLLISION_KIND_MAP) {
        ese_collision_hit_set_map(copy, src->data.map);
        int cx = src->data.cell_x ? (int)lua_value_get_number(src->data.cell_x) : 0;
        int cy = src->data.cell_y ? (int)lua_value_get_number(src->data.cell_y) : 0;
        ese_collision_hit_set_cell_x(copy, cx);
        ese_collision_hit_set_cell_y(copy, cy);
    }

    return copy;
}

void ese_collision_hit_destroy(EseCollisionHit *hit) {
    if (!hit) return;
    if (hit->lua_ref == LUA_NOREF) {
        // Free owned resources depending on kind
        if (hit->kind == COLLISION_KIND_COLLIDER) {
            if (hit->data.rect) {
                ese_rect_destroy(hit->data.rect);
                hit->data.rect = NULL;
            }
        } else if (hit->kind == COLLISION_KIND_MAP) {
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
    log_assert("COLLISION_HIT", engine, "ese_collision_hit_lua_init called with NULL engine");

    // Create metatable
    lua_State *L = engine->runtime;
    if (luaL_newmetatable(L, COLLISION_HIT_META)) {
        lua_pushstring(L, COLLISION_HIT_META);
        lua_setfield(L, -2, "__name");
        lua_pushcfunction(L, _ese_collision_hit_lua_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, _ese_collision_hit_lua_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, _ese_collision_hit_lua_gc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, _ese_collision_hit_lua_tostring);
        lua_setfield(L, -2, "__tostring");
        lua_pushstring(L, "locked");
        lua_setfield(L, -2, "__metatable");
    }
    lua_pop(L, 1);

    // Create global EseCollisionHit table with only constants
    lua_getglobal(L, "EseCollisionHit");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        _register_collision_hit_constants(L);
        lua_setglobal(L, "EseCollisionHit");
    } else {
        // augment existing table
        _register_collision_hit_constants(L);
        lua_pop(L, 1);
    }
}

void ese_collision_hit_lua_push(EseCollisionHit *hit) {
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_lua_push called with NULL hit");
    if (hit->lua_ref == LUA_NOREF) {
        EseCollisionHit **ud = (EseCollisionHit **)lua_newuserdata(hit->state_ptr, sizeof(EseCollisionHit *));
        *ud = hit;
        luaL_getmetatable(hit->state_ptr, COLLISION_HIT_META);
        lua_setmetatable(hit->state_ptr, -2);
    } else {
        lua_rawgeti(hit->state_ptr, LUA_REGISTRYINDEX, hit->lua_ref);
    }
}

EseCollisionHit *ese_collision_hit_lua_get(lua_State *L, int idx) {
    log_assert("COLLISION_HIT", L, "ese_collision_hit_lua_get called with NULL Lua state");
    if (!lua_isuserdata(L, idx)) return NULL;
    EseCollisionHit **ud = (EseCollisionHit **)luaL_testudata(L, idx, COLLISION_HIT_META);
    if (!ud) return NULL;
    return *ud;
}

void ese_collision_hit_ref(EseCollisionHit *hit) {
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_ref called with NULL hit");
    if (hit->lua_ref == LUA_NOREF) {
        EseCollisionHit **ud = (EseCollisionHit **)lua_newuserdata(hit->state_ptr, sizeof(EseCollisionHit *));
        *ud = hit;
        luaL_getmetatable(hit->state_ptr, COLLISION_HIT_META);
        lua_setmetatable(hit->state_ptr, -2);
        hit->lua_ref = luaL_ref(hit->state_ptr, LUA_REGISTRYINDEX);
        hit->lua_ref_count = 1;
    } else {
        hit->lua_ref_count++;
    }
}

void ese_collision_hit_unref(EseCollisionHit *hit) {
    if (!hit) return;
    if (hit->lua_ref != LUA_NOREF && hit->lua_ref_count > 0) {
        hit->lua_ref_count--;
        if (hit->lua_ref_count == 0) {
            luaL_unref(hit->state_ptr, LUA_REGISTRYINDEX, hit->lua_ref);
            hit->lua_ref = LUA_NOREF;
        }
    }
}

// Property access
EseCollisionKind ese_collision_hit_get_kind(const EseCollisionHit *hit) {
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_get_kind called with NULL hit");
    return hit->kind;
}

/// Sets the collision kind and clears non-matching data.
void ese_collision_hit_set_kind(EseCollisionHit *hit, EseCollisionKind kind) {
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_set_kind called with NULL hit");

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
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_get_state called with NULL hit");

    return hit->state;
}

void ese_collision_hit_set_state(EseCollisionHit *hit, EseCollisionState state) {
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_set_state called with NULL hit");

    hit->state = state;
}

EseEntity *ese_collision_hit_get_entity(const EseCollisionHit *hit) {
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_get_entity called with NULL hit");

    return hit->entity;
}

void ese_collision_hit_set_entity(EseCollisionHit *hit, EseEntity *entity) {
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_set_entity called with NULL hit");

    hit->entity = entity;
}

EseEntity *ese_collision_hit_get_target(const EseCollisionHit *hit) {
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_get_target called with NULL hit");

    return hit->target;
}

void ese_collision_hit_set_target(EseCollisionHit *hit, EseEntity *target) {
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_set_target called with NULL hit");

    hit->target = target;
}

void ese_collision_hit_set_rect(EseCollisionHit *hit, const EseRect *rect) {
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_set_rect called with NULL hit");
    log_assert("COLLISION_HIT", hit->kind == COLLISION_KIND_COLLIDER, "ese_collision_hit_set_rect called with non-collider hit");

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
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_get_rect called with NULL hit");
    log_assert("COLLISION_HIT", hit->kind == COLLISION_KIND_COLLIDER, "ese_collision_hit_get_rect called with non-collider hit");
    
    return hit->data.rect;
}

void ese_collision_hit_set_map(EseCollisionHit *hit, EseMap *map) {
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_set_map called with NULL hit");
    log_assert("COLLISION_HIT", hit->kind == COLLISION_KIND_MAP, "ese_collision_hit_set_map called with non-map hit");

    hit->data.map = map;
}

EseMap *ese_collision_hit_get_map(const EseCollisionHit *hit) {
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_get_map called with NULL hit");
    log_assert("COLLISION_HIT", hit->kind == COLLISION_KIND_MAP, "ese_collision_hit_get_map called with non-map hit");

    return hit->data.map;
}

void ese_collision_hit_set_cell_x(EseCollisionHit *hit, int cell_x) {
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_set_cell_x called with NULL hit");
    log_assert("COLLISION_HIT", hit->kind == COLLISION_KIND_MAP, "ese_collision_hit_set_cell_x called with non-map hit");

    // Replace owned value safely
    if (hit->data.cell_x) {
        lua_value_destroy(hit->data.cell_x);
        hit->data.cell_x = NULL;
    }
    hit->data.cell_x = lua_value_create_number("cell_x", (double)cell_x);
}

int ese_collision_hit_get_cell_x(const EseCollisionHit *hit) {
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_get_cell_x called with NULL hit");
    log_assert("COLLISION_HIT", hit->kind == COLLISION_KIND_MAP, "ese_collision_hit_get_cell_x called with non-map hit");

    return hit->data.cell_x ? (int)lua_value_get_number(hit->data.cell_x) : 0;
}

void ese_collision_hit_set_cell_y(EseCollisionHit *hit, int cell_y) {
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_set_cell_y called with NULL hit");
    log_assert("COLLISION_HIT", hit->kind == COLLISION_KIND_MAP, "ese_collision_hit_set_cell_y called with non-map hit");

    // Replace owned value safely
    if (hit->data.cell_y) {
        lua_value_destroy(hit->data.cell_y);
        hit->data.cell_y = NULL;
    }
    hit->data.cell_y = lua_value_create_number("cell_y", (double)cell_y);
}

int ese_collision_hit_get_cell_y(const EseCollisionHit *hit) {
    log_assert("COLLISION_HIT", hit, "ese_collision_hit_get_cell_y called with NULL hit");
    log_assert("COLLISION_HIT", hit->kind == COLLISION_KIND_MAP, "ese_collision_hit_get_cell_y called with non-map hit");

    return hit->data.cell_y ? (int)lua_value_get_number(hit->data.cell_y) : 0;
}
