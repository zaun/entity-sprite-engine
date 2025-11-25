/*
 * Project: Entity Sprite Engine
 *
 * Lua bindings for entity components.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */

#include "entity/bindings/collider.h"
#include "core/memory_manager.h"
#include "entity/components/entity_component.h"
#include "entity/components/collider.h"
#include "scripting/lua_engine.h"
#include "types/types.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"
#include <math.h>
#include <string.h>

#define COLLIDER_RECTS_PROXY_META "ColliderRectsProxyMeta"

// ========================================
// PRIVATE FUNCTIONS
// ========================================

/**
 * @brief Helper function to get the collider component from a rects proxy
 * table.
 *
 * @details Extracts the collider component from the rects proxy userdata.
 *
 * @param L   Lua state pointer.
 * @param idx Stack index of the rects proxy userdata.
 * @return Pointer to the collider component, or NULL if extraction fails.
 */
static void *_entity_component_collider_rects_get_component(lua_State *L, int idx);
static EseEntityComponentCollider *_entity_component_collider_get(lua_State *L, int idx);

static int _entity_component_collider_new(lua_State *L);
static int _entity_component_collider_rects_add(lua_State *L);
static int _entity_component_collider_rects_remove(lua_State *L);
static int _entity_component_collider_rects_insert(lua_State *L);
static int _entity_component_collider_rects_pop(lua_State *L);
static int _entity_component_collider_rects_shift(lua_State *L);
static int _entity_component_collider_index(lua_State *L);
static int _entity_component_collider_newindex(lua_State *L);
static int _entity_component_collider_rects_rects_index(lua_State *L);
static int _entity_component_collider_gc(lua_State *L);
static int _entity_component_collider_tostring(lua_State *L);
static int _entity_component_collider_tojson_lua(lua_State *L);
static int _entity_component_collider_fromjson_lua(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS (IMPLEMENTATION)
// ========================================

static void *_entity_component_collider_rects_get_component(lua_State *L, int idx) {
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }

    EseEntityComponentCollider **ud =
        (EseEntityComponentCollider **)luaL_testudata(L, idx, COLLIDER_RECTS_PROXY_META);
    if (!ud) {
        return NULL;
    }

    return *ud;
}

static int _entity_component_collider_new(lua_State *L) {
    EseRect *rect = NULL;

    int n_args = lua_gettop(L);
    if (n_args == 1) {
        rect = ese_rect_lua_get(L, 1);
        if (rect == NULL) {
            luaL_argerror(L, 1,
                          "EntityComponentCollider.new() or "
                          "EntityComponentCollider.new(Rect)");
            return 0;
        }
    } else if (n_args > 1) {
        luaL_argerror(L, 1,
                      "EntityComponentCollider.new() or "
                      "EntityComponentCollider.new(Rect)");
        return 0;
    }

    EseLuaEngine *lua = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);

    EseEntityComponent *component = entity_component_collider_make(lua);
    component->lua = lua;

    EseEntityComponentCollider **ud =
        (EseEntityComponentCollider **)lua_newuserdata(L, sizeof(EseEntityComponentCollider *));
    *ud = (EseEntityComponentCollider *)component->data;

    luaL_getmetatable(L, ENTITY_COMPONENT_COLLIDER_PROXY_META);
    lua_setmetatable(L, -2);

    if (rect) {
        entity_component_collider_rects_add((EseEntityComponentCollider *)component->data, rect);
    }

    return 1;
}

static int _entity_component_collider_rects_add(lua_State *L) {
    EseEntityComponentCollider *collider =
        (EseEntityComponentCollider *)lua_engine_instance_method_normalize(
            L, _entity_component_collider_rects_get_component, "ColliderRectsProxy");

    int n_args = lua_gettop(L);
    EseRect *rect = NULL;
    if (n_args == 1) {
        rect = ese_rect_lua_get(L, 1);
    } else {
        return luaL_argerror(L, 1, "Expected a Rect argument.");
    }

    if (rect == NULL) {
        return luaL_argerror(L, 1, "Expected a Rect argument.");
    }

    entity_component_collider_rects_add(collider, rect);

    return 0;
}

static int _entity_component_collider_rects_remove(lua_State *L) {
    EseEntityComponentCollider *collider = _entity_component_collider_rects_get_component(L, 1);
    if (!collider) {
        return luaL_error(L, "Invalid collider object.");
    }

    EseRect *rect_to_remove = ese_rect_lua_get(L, 2);
    if (rect_to_remove == NULL) {
        return luaL_argerror(L, 2, "Expected a Rect object.");
    }

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

    ese_rect_remove_watcher(rect_to_remove, entity_component_collider_rect_changed, collider);
    ese_rect_unref(rect_to_remove);

    for (size_t i = idx; i < collider->rects_count - 1; ++i) {
        collider->rects[i] = collider->rects[i + 1];
    }

    collider->rects_count--;
    collider->rects[collider->rects_count] = NULL;

    entity_component_collider_update_bounds(collider);

    lua_pushboolean(L, true);
    return 1;
}

static int _entity_component_collider_rects_insert(lua_State *L) {
    EseEntityComponentCollider *collider = _entity_component_collider_rects_get_component(L, 1);
    if (!collider) {
        return luaL_error(L, "Invalid collider object.");
    }

    EseRect *rect = ese_rect_lua_get(L, 2);
    if (rect == NULL) {
        return luaL_argerror(L, 2, "Expected a rect object.");
    }

    int index = (int)luaL_checkinteger(L, 3) - 1;

    if (index < 0 || index > (int)collider->rects_count) {
        return luaL_error(L, "Index out of bounds.");
    }

    if (collider->rects_count == collider->rects_capacity) {
        size_t new_capacity = collider->rects_capacity * 2;
        EseRect **new_rects =
            memory_manager.realloc(collider->rects, sizeof(EseRect *) * new_capacity, MMTAG_ENTITY);
        collider->rects = new_rects;
        collider->rects_capacity = new_capacity;
    }

    for (size_t i = collider->rects_count; i > (size_t)index; --i) {
        collider->rects[i] = collider->rects[i - 1];
    }

    collider->rects[index] = rect;
    collider->rects_count++;
    ese_rect_ref(rect);

    ese_rect_add_watcher(rect, entity_component_collider_rect_changed, collider);

    entity_component_collider_update_bounds(collider);

    return 0;
}

static int _entity_component_collider_rects_pop(lua_State *L) {
    EseEntityComponentCollider *collider = _entity_component_collider_rects_get_component(L, 1);
    if (!collider) {
        return luaL_error(L, "Invalid collider object.");
    }

    if (collider->rects_count == 0) {
        lua_pushnil(L);
        return 1;
    }

    EseRect *rect = collider->rects[collider->rects_count - 1];

    ese_rect_remove_watcher(rect, entity_component_collider_rect_changed, collider);
    ese_rect_unref(rect);

    collider->rects[collider->rects_count - 1] = NULL;
    collider->rects_count--;

    entity_component_collider_update_bounds(collider);

    ese_rect_lua_push(rect);

    return 1;
}

static int _entity_component_collider_rects_shift(lua_State *L) {
    EseEntityComponentCollider *collider = _entity_component_collider_rects_get_component(L, 1);
    if (!collider) {
        return luaL_error(L, "Invalid collider object.");
    }

    if (collider->rects_count == 0) {
        lua_pushnil(L);
        return 1;
    }

    EseRect *rect = collider->rects[0];

    ese_rect_remove_watcher(rect, entity_component_collider_rect_changed, collider);
    ese_rect_unref(rect);

    for (size_t i = 0; i < collider->rects_count - 1; ++i) {
        collider->rects[i] = collider->rects[i + 1];
    }

    collider->rects_count--;
    collider->rects[collider->rects_count] = NULL;

    entity_component_collider_update_bounds(collider);

    ese_rect_lua_push(rect);

    return 1;
}

static int _entity_component_collider_index(lua_State *L) {
    EseEntityComponentCollider *component = _entity_component_collider_get(L, 1);
    const char *key = lua_tostring(L, 2);

    if (!component) {
        lua_pushnil(L);
        return 1;
    }

    if (!key) {
        return 0;
    }

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
    } else if (strcmp(key, "toJSON") == 0) {
        lua_pushcfunction(L, _entity_component_collider_tojson_lua);
        return 1;
    } else if (strcmp(key, "offset") == 0) {
        ese_point_lua_push(component->offset);
        return 1;
    } else if (strcmp(key, "rects") == 0) {
        EseEntityComponentCollider **ud =
            (EseEntityComponentCollider **)lua_newuserdata(L, sizeof(EseEntityComponentCollider *));
        *ud = component;

        luaL_getmetatable(L, COLLIDER_RECTS_PROXY_META);
        lua_setmetatable(L, -2);
        return 1;
    }

    return 0;
}

static int _entity_component_collider_newindex(lua_State *L) {
    EseEntityComponentCollider *component = _entity_component_collider_get(L, 1);
    const char *key = lua_tostring(L, 2);

    if (!component) {
        return 0;
    }

    if (!key) {
        return 0;
    }

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
        ese_point_set_x(component->offset, ese_point_get_x(new_position_point));
        ese_point_set_y(component->offset, ese_point_get_y(new_position_point));
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

static int _entity_component_collider_rects_rects_index(lua_State *L) {
    EseEntityComponentCollider *component = _entity_component_collider_rects_get_component(L, 1);
    if (!component) {
        lua_pushnil(L);
        return 1;
    }

    if (lua_isnumber(L, 2)) {
        int index = (int)lua_tointeger(L, 2) - 1;
        if (index >= 0 && index < (int)component->rects_count) {
            EseRect *rect = component->rects[index];
            ese_rect_lua_push(rect);
            return 1;
        }

        lua_pushnil(L);
        return 1;
    }

    const char *key = lua_tostring(L, 2);
    if (!key) {
        return 0;
    }

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

static int _entity_component_collider_gc(lua_State *L) {
    EseEntityComponentCollider **ud =
        (EseEntityComponentCollider **)luaL_testudata(L, 1, ENTITY_COMPONENT_COLLIDER_PROXY_META);
    if (!ud) {
        return 0;
    }

    EseEntityComponentCollider *component = *ud;
    if (component) {
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
    snprintf(buf, sizeof(buf), "EntityComponentCollider: %p (id=%s active=%s draw_debug=%s)",
             (void *)component, ese_uuid_get_value(component->base.id),
             component->base.active ? "true" : "false", component->draw_debug ? "true" : "false");
    lua_pushstring(L, buf);

    return 1;
}

static int _entity_component_collider_tojson_lua(lua_State *L) {
    EseEntityComponentCollider *self = _entity_component_collider_get(L, 1);
    if (!self) {
        return luaL_error(L, "EntityComponentCollider:toJSON() called on invalid component");
    }
    if (lua_gettop(L) != 1) {
        return luaL_error(L, "EntityComponentCollider:toJSON() takes 0 arguments");
    }
    cJSON *json = entity_component_collider_serialize(self);
    if (!json) {
        return luaL_error(L, "EntityComponentCollider:toJSON() failed to serialize");
    }
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!json_str) {
        return luaL_error(L, "EntityComponentCollider:toJSON() failed to stringify");
    }
    lua_pushstring(L, json_str);
    free(json_str);
    return 1;
}

static int _entity_component_collider_fromjson_lua(lua_State *L) {
    const char *json_str = luaL_checkstring(L, 1);
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (!engine) {
        return luaL_error(L, "EntityComponentCollider.fromJSON() could not get engine");
    }

    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        return luaL_error(L, "EntityComponentCollider.fromJSON() failed to parse JSON");
    }

    EseEntityComponent *base = entity_component_collider_deserialize(engine, json);
    cJSON_Delete(json);
    if (!base) {
        return luaL_error(L, "EntityComponentCollider.fromJSON() failed to deserialize");
    }

    EseEntityComponentCollider *comp = (EseEntityComponentCollider *)base->data;

    EseEntityComponentCollider **ud =
        (EseEntityComponentCollider **)lua_newuserdata(L, sizeof(EseEntityComponentCollider *));
    *ud = comp;
    luaL_getmetatable(L, ENTITY_COMPONENT_COLLIDER_PROXY_META);
    lua_setmetatable(L, -2);

    return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

static EseEntityComponentCollider *_entity_component_collider_get(lua_State *L, int idx) {
    log_assert("ENTITY_COMP", L, "_entity_component_collider_get called with NULL Lua state");

    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }

    EseEntityComponentCollider **ud =
        (EseEntityComponentCollider **)luaL_testudata(L, idx, ENTITY_COMPONENT_COLLIDER_PROXY_META);
    if (!ud) {
        return NULL;
    }

    return *ud;
}

void entity_component_collider_init(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "entity_component_collider_init called with NULL engine");

    lua_engine_new_object_meta(engine, ENTITY_COMPONENT_COLLIDER_PROXY_META,
                               _entity_component_collider_index,
                               _entity_component_collider_newindex, _entity_component_collider_gc,
                               _entity_component_collider_tostring);

    const char *keys[] = {"new", "fromJSON"};
    lua_CFunction functions[] = {_entity_component_collider_new, _entity_component_collider_fromjson_lua};
    lua_engine_new_object(engine, "EntityComponentCollider", 2, keys, functions);

    lua_engine_new_object_meta(engine, COLLIDER_RECTS_PROXY_META,
                               _entity_component_collider_rects_rects_index, NULL, NULL, NULL);

    profile_count_add("entity_comp_collider_init_count");
}
