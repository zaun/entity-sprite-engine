#include "entity/components/entity_component_listener.h"
#include "core/memory_manager.h"
#include "vendor/json/cJSON.h"
#include "entity/components/entity_component.h"
#include "entity/components/entity_component_private.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "entity/systems/sound_system_private.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <string.h>

// Forward declarations for Lua JSON helpers
static int _entity_component_listener_tojson_lua(lua_State *L);
static int _entity_component_listener_fromjson_lua(lua_State *L);

// VTable wrapper functions
static EseEntityComponent *_listener_vtable_copy(EseEntityComponent *component) {
    return _entity_component_listener_copy((EseEntityComponentListener *)component->data);
}

static void _listener_vtable_destroy(EseEntityComponent *component) {
    _entity_component_listener_destroy((EseEntityComponentListener *)component->data);
}

static bool _listener_vtable_run_function(EseEntityComponent *component, EseEntity *entity,
                                          const char *func_name, int argc, void *argv[]) {
    (void)component;
    (void)entity;
    (void)func_name;
    (void)argc;
    (void)argv;
    // Listener components don't support function execution
    return false;
}

static void _listener_vtable_collides_component(EseEntityComponent *a, EseEntityComponent *b,
                                                EseArray *out_hits) {
    (void)a;
    (void)b;
    (void)out_hits;
}

static void _listener_vtable_ref(EseEntityComponent *component) {
    EseEntityComponentListener *listener = (EseEntityComponentListener *)component->data;
    log_assert("ENTITY_COMP", listener, "listener vtable ref called with NULL");
    if (listener->base.lua_ref == LUA_NOREF) {
        EseEntityComponentListener **ud = (EseEntityComponentListener **)lua_newuserdata(
            listener->base.lua->runtime, sizeof(EseEntityComponentListener *));
        *ud = listener;
        luaL_getmetatable(listener->base.lua->runtime, ENTITY_COMPONENT_LISTENER_PROXY_META);
        lua_setmetatable(listener->base.lua->runtime, -2);
        listener->base.lua_ref = luaL_ref(listener->base.lua->runtime, LUA_REGISTRYINDEX);
        listener->base.lua_ref_count = 1;
    } else {
        listener->base.lua_ref_count++;
    }
}

static void _listener_vtable_unref(EseEntityComponent *component) {
    EseEntityComponentListener *listener = (EseEntityComponentListener *)component->data;
    if (!listener) {
        return;
    }
    if (listener->base.lua_ref != LUA_NOREF && listener->base.lua_ref_count > 0) {
        listener->base.lua_ref_count--;
        if (listener->base.lua_ref_count == 0) {
            luaL_unref(listener->base.lua->runtime, LUA_REGISTRYINDEX, listener->base.lua_ref);
            listener->base.lua_ref = LUA_NOREF;
        }
    }
}

// Static vtable instance for listener components
static const ComponentVTable listener_vtable = {.copy = _listener_vtable_copy,
                                                .destroy = _listener_vtable_destroy,
                                                .run_function = _listener_vtable_run_function,
                                                .collides = _listener_vtable_collides_component,
                                                .ref = _listener_vtable_ref,
                                                .unref = _listener_vtable_unref};

static EseEntityComponent *_entity_component_listener_make(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "_entity_component_listener_make called with NULL engine");

    EseEntityComponentListener *component =
        memory_manager.malloc(sizeof(EseEntityComponentListener), MMTAG_ENTITY);
    component->base.data = component;
    component->base.active = true;
    component->base.id = ese_uuid_create(engine);
    component->base.lua = engine;
    component->base.lua_ref = LUA_NOREF;
    component->base.lua_ref_count = 0;
    component->base.type = ENTITY_COMPONENT_LISTENER;
    component->base.vtable = &listener_vtable;

    // Default listener values (runtime defaults may differ from older tests/docs).
    component->volume = 100.0f;        // volume in [0, 100]
    component->spatial = true;         // spatial audio enabled by default
    component->max_distance = 10000.0f;

    // New distance attenuation controls.
    // These are chosen so the default behavior matches the previous linear
    // attenuation model: full attenuation with a linear rolloff.
    component->attenuation = 1.0f;     // 0 = no distance attenuation, 1 = full
    component->rolloff = 1.0f;         // 1 = linear, >1 faster drop, <1 slower

    profile_count_add("entity_comp_listener_make_count");
    return &component->base;
}

EseEntityComponent *_entity_component_listener_copy(const EseEntityComponentListener *src) {
    log_assert("ENTITY_COMP", src, "_entity_component_listener_copy called with NULL src");

    EseEntityComponent *copy = _entity_component_listener_make(src->base.lua);
    EseEntityComponentListener *listener_copy = (EseEntityComponentListener *)copy->data;

    listener_copy->volume = src->volume;
    listener_copy->spatial = src->spatial;
    listener_copy->max_distance = src->max_distance;
    listener_copy->attenuation = src->attenuation;
    listener_copy->rolloff = src->rolloff;

    profile_count_add("entity_comp_listener_copy_count");
    return copy;
}

static void _entity_component_listener_cleanup(EseEntityComponentListener *component) {
    ese_uuid_destroy(component->base.id);
    memory_manager.free(component);
    profile_count_add("entity_comp_listener_destroy_count");
}

void _entity_component_listener_destroy(EseEntityComponentListener *component) {
    log_assert("ENTITY_COMP", component,
               "_entity_component_listener_destroy called with NULL component");

    // Respect Lua registry ref-count; only free when no refs remain
    if (component->base.lua_ref != LUA_NOREF && component->base.lua_ref_count > 0) {
        component->base.lua_ref_count--;
        if (component->base.lua_ref_count == 0) {
            luaL_unref(component->base.lua->runtime, LUA_REGISTRYINDEX, component->base.lua_ref);
            component->base.lua_ref = LUA_NOREF;
            _entity_component_listener_cleanup(component);
        } else {
            // We don't own the component yet, still referenced from Lua
            return;
        }
    } else if (component->base.lua_ref == LUA_NOREF) {
        _entity_component_listener_cleanup(component);
    }
}

cJSON *entity_component_listener_serialize(const EseEntityComponentListener *component) {
    log_assert("ENTITY_COMP", component,
               "entity_component_listener_serialize called with NULL component");

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        log_error("ENTITY_COMP", "Listener serialize: failed to create JSON object");
        return NULL;
    }

    if (!cJSON_AddStringToObject(json, "type", "ENTITY_COMPONENT_LISTENER") ||
        !cJSON_AddBoolToObject(json, "active", component->base.active) ||
        !cJSON_AddNumberToObject(json, "volume", (double)component->volume) ||
        !cJSON_AddBoolToObject(json, "spatial", component->spatial) ||
        !cJSON_AddNumberToObject(json, "max_distance", (double)component->max_distance) ||
        !cJSON_AddNumberToObject(json, "attenuation", (double)component->attenuation) ||
        !cJSON_AddNumberToObject(json, "rolloff", (double)component->rolloff)) {
        log_error("ENTITY_COMP", "Listener serialize: failed to add fields");
        cJSON_Delete(json);
        return NULL;
    }

    return json;
}

EseEntityComponent *entity_component_listener_deserialize(EseLuaEngine *engine,
                                                          const cJSON *data) {
    log_assert("ENTITY_COMP", engine,
               "entity_component_listener_deserialize called with NULL engine");
    log_assert("ENTITY_COMP", data,
               "entity_component_listener_deserialize called with NULL data");

    if (!cJSON_IsObject(data)) {
        log_error("ENTITY_COMP", "Listener deserialize: data is not an object");
        return NULL;
    }

    const cJSON *type_item = cJSON_GetObjectItemCaseSensitive(data, "type");
    if (!cJSON_IsString(type_item) ||
        strcmp(type_item->valuestring, "ENTITY_COMPONENT_LISTENER") != 0) {
        log_error("ENTITY_COMP", "Listener deserialize: invalid or missing type");
        return NULL;
    }

    const cJSON *active_item = cJSON_GetObjectItemCaseSensitive(data, "active");
    const cJSON *vol_item = cJSON_GetObjectItemCaseSensitive(data, "volume");
    const cJSON *spatial_item = cJSON_GetObjectItemCaseSensitive(data, "spatial");
    const cJSON *max_item = cJSON_GetObjectItemCaseSensitive(data, "max_distance");
    const cJSON *att_item = cJSON_GetObjectItemCaseSensitive(data, "attenuation");
    const cJSON *roll_item = cJSON_GetObjectItemCaseSensitive(data, "rolloff");

    EseEntityComponent *base = entity_component_listener_create(engine);
    if (!base) {
        log_error("ENTITY_COMP", "Listener deserialize: failed to create component");
        return NULL;
    }

    EseEntityComponentListener *comp = (EseEntityComponentListener *)base->data;
    if (cJSON_IsBool(active_item)) {
        comp->base.active = cJSON_IsTrue(active_item);
    }
    if (cJSON_IsNumber(vol_item)) {
        comp->volume = (float)vol_item->valuedouble;
    }
    if (cJSON_IsBool(spatial_item)) {
        comp->spatial = cJSON_IsTrue(spatial_item);
    }
    if (cJSON_IsNumber(max_item)) {
        comp->max_distance = (float)max_item->valuedouble;
    }
    if (cJSON_IsNumber(att_item)) {
        comp->attenuation = (float)att_item->valuedouble;
    }
    if (cJSON_IsNumber(roll_item)) {
        comp->rolloff = (float)roll_item->valuedouble;
    }

    return base;
}

/**
 * @brief Lua __index metamethod for EseEntityComponentListener objects (getter).
 */
static int _entity_component_listener_index(lua_State *L) {
    EseEntityComponentListener *component = _entity_component_listener_get(L, 1);
    const char *key = lua_tostring(L, 2);

    // SAFETY: Return nil for freed components
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
    } else if (strcmp(key, "volume") == 0) {
        lua_pushnumber(L, (lua_Number)component->volume);
        return 1;
    } else if (strcmp(key, "spatial") == 0) {
        lua_pushboolean(L, component->spatial);
        return 1;
    } else if (strcmp(key, "max_distance") == 0) {
        lua_pushnumber(L, (lua_Number)component->max_distance);
        return 1;
    } else if (strcmp(key, "attenuation") == 0) {
        lua_pushnumber(L, (lua_Number)component->attenuation);
        return 1;
    } else if (strcmp(key, "rolloff") == 0) {
        lua_pushnumber(L, (lua_Number)component->rolloff);
        return 1;
    } else if (strcmp(key, "toJSON") == 0) {
        lua_pushcfunction(L, _entity_component_listener_tojson_lua);
        return 1;
    }

    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseEntityComponentListener objects (setter).
 */
static int _entity_component_listener_newindex(lua_State *L) {
    EseEntityComponentListener *component = _entity_component_listener_get(L, 1);
    const char *key = lua_tostring(L, 2);

    // SAFETY: Silently ignore writes to freed components
    if (!component) {
        return 0;
    }

    if (!key) {
        return 0;
    }

    EseMutex *mtx = (g_sound_system_data ? g_sound_system_data->mutex : NULL);
    if (mtx) {
        ese_mutex_lock(mtx);
    }

    if (strcmp(key, "active") == 0) {
        if (!lua_isboolean(L, 3)) {
            if (mtx) {
                ese_mutex_unlock(mtx);
            }
            return luaL_error(L, "active must be a boolean");
        }
        component->base.active = lua_toboolean(L, 3);
        if (mtx) {
            ese_mutex_unlock(mtx);
        }
        return 0;
    } else if (strcmp(key, "id") == 0) {
        if (mtx) {
            ese_mutex_unlock(mtx);
        }
        return luaL_error(L, "id is read-only");
    } else if (strcmp(key, "volume") == 0) {
        if (!lua_isnumber(L, 3)) {
            if (mtx) {
                ese_mutex_unlock(mtx);
            }
            return luaL_error(L, "volume must be a number");
        }
        float v = (float)lua_tonumber(L, 3);
        if (v < 0.0f) {
            v = 0.0f;
        } else if (v > 100.0f) {
            v = 100.0f;
        }
        component->volume = v;
        if (mtx) {
            ese_mutex_unlock(mtx);
        }
        return 0;
    } else if (strcmp(key, "spatial") == 0) {
        if (!lua_isboolean(L, 3)) {
            if (mtx) {
                ese_mutex_unlock(mtx);
            }
            return luaL_error(L, "spatial must be a boolean");
        }
        component->spatial = lua_toboolean(L, 3);
        if (mtx) {
            ese_mutex_unlock(mtx);
        }
        return 0;
    } else if (strcmp(key, "max_distance") == 0) {
        if (!lua_isnumber(L, 3)) {
            if (mtx) {
                ese_mutex_unlock(mtx);
            }
            return luaL_error(L, "max_distance must be a number");
        }
        component->max_distance = (float)lua_tonumber(L, 3);
        if (mtx) {
            ese_mutex_unlock(mtx);
        }
        return 0;
    } else if (strcmp(key, "attenuation") == 0) {
        if (!lua_isnumber(L, 3)) {
            if (mtx) {
                ese_mutex_unlock(mtx);
            }
            return luaL_error(L, "attenuation must be a number");
        }
        float a = (float)lua_tonumber(L, 3);
        if (a < 0.0f) {
            a = 0.0f;
        } else if (a > 1.0f) {
            a = 1.0f;
        }
        component->attenuation = a;
        if (mtx) {
            ese_mutex_unlock(mtx);
        }
        return 0;
    } else if (strcmp(key, "rolloff") == 0) {
        if (!lua_isnumber(L, 3)) {
            if (mtx) {
                ese_mutex_unlock(mtx);
            }
            return luaL_error(L, "rolloff must be a number");
        }
        float r = (float)lua_tonumber(L, 3);
        // Clamp to a sensible range: avoid 0 (no curve) and absurdly large exponents.
        if (r < 0.1f) {
            r = 0.1f;
        } else if (r > 8.0f) {
            r = 8.0f;
        }
        component->rolloff = r;
        if (mtx) {
            ese_mutex_unlock(mtx);
        }
        return 0;
    }

    if (mtx) {
        ese_mutex_unlock(mtx);
    }

    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __gc metamethod for EseEntityComponentListener objects.
 */
static int _entity_component_listener_gc(lua_State *L) {
    // Get from userdata
    EseEntityComponentListener **ud = (EseEntityComponentListener **)luaL_testudata(
        L, 1, ENTITY_COMPONENT_LISTENER_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }

    EseEntityComponentListener *component = *ud;
    if (component) {
        if (component->base.lua_ref == LUA_NOREF) {
            _entity_component_listener_destroy(component);
            *ud = NULL;
        }
    }

    return 0;
}

static int _entity_component_listener_tostring(lua_State *L) {
    EseEntityComponentListener *component = _entity_component_listener_get(L, 1);

    if (!component) {
        lua_pushstring(L, "EntityComponentListener: (invalid)");
        return 1;
    }

    char buf[256];
    snprintf(buf, sizeof(buf),
             "EntityComponentListener: %p (id=%s active=%s volume=%.2f spatial=%s max_distance=%.2f attenuation=%.2f rolloff=%.2f)",
             (void *)component, ese_uuid_get_value(component->base.id),
             component->base.active ? "true" : "false", component->volume,
             component->spatial ? "true" : "false", component->max_distance,
             component->attenuation, component->rolloff);
    lua_pushstring(L, buf);

    return 1;
}

/**
 * @brief Lua function to create a new EseEntityComponentListener object.
 *
 * @details Callable from Lua as EntityComponentListener.new().
 */
static int _entity_component_listener_new(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 0) {
        log_debug("ENTITY_COMP", "EntityComponentListener.new() takes no arguments");
    }

    // Set engine reference
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);

    // Create EseEntityComponent wrapper
    EseEntityComponent *component = _entity_component_listener_make(engine);

    // For Lua-created components, create userdata without storing a persistent ref
    EseEntityComponentListener **ud =
        (EseEntityComponentListener **)lua_newuserdata(L, sizeof(EseEntityComponentListener *));
    *ud = (EseEntityComponentListener *)component->data;
    luaL_getmetatable(L, ENTITY_COMPONENT_LISTENER_PROXY_META);
    lua_setmetatable(L, -2);

    profile_count_add("entity_comp_listener_new_count");
    return 1;
}

EseEntityComponentListener *_entity_component_listener_get(lua_State *L, int idx) {
    // Check if it's userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }

    // Get the userdata and check metatable
    EseEntityComponentListener **ud = (EseEntityComponentListener **)luaL_testudata(
        L, idx, ENTITY_COMPONENT_LISTENER_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }

    return *ud;
}

void _entity_component_listener_init(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "_entity_component_listener_init called with NULL engine");

    // Create metatable
    lua_engine_new_object_meta(engine, ENTITY_COMPONENT_LISTENER_PROXY_META,
                               _entity_component_listener_index, _entity_component_listener_newindex,
                               _entity_component_listener_gc, _entity_component_listener_tostring);

    // Create global EntityComponentListener table with functions
    const char *keys[] = {"new", "fromJSON"};
    lua_CFunction functions[] = {_entity_component_listener_new, _entity_component_listener_fromjson_lua};
    lua_engine_new_object(engine, "EntityComponentListener", 2, keys, functions);

    profile_count_add("entity_comp_listener_init_count");
}

EseEntityComponent *entity_component_listener_create(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine,
               "entity_component_listener_create called with NULL engine");

    EseEntityComponent *component = _entity_component_listener_make(engine);

    // Register with Lua using ref system
    component->vtable->ref(component);

    profile_count_add("entity_comp_listener_create_count");
    return component;
}

static int _entity_component_listener_tojson_lua(lua_State *L) {
    EseEntityComponentListener *self = _entity_component_listener_get(L, 1);
    if (!self) {
        return luaL_error(L, "EntityComponentListener:toJSON() called on invalid component");
    }
    if (lua_gettop(L) != 1) {
        return luaL_error(L, "EntityComponentListener:toJSON() takes 0 arguments");
    }
    cJSON *json = entity_component_listener_serialize(self);
    if (!json) {
        return luaL_error(L, "EntityComponentListener:toJSON() failed to serialize");
    }
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!json_str) {
        return luaL_error(L, "EntityComponentListener:toJSON() failed to stringify");
    }
    lua_pushstring(L, json_str);
    free(json_str);
    return 1;
}

static int _entity_component_listener_fromjson_lua(lua_State *L) {
    const char *json_str = luaL_checkstring(L, 1);
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (!engine) {
        return luaL_error(L, "EntityComponentListener.fromJSON() could not get engine");
    }

    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        return luaL_error(L, "EntityComponentListener.fromJSON() failed to parse JSON");
    }

    EseEntityComponent *base = entity_component_listener_deserialize(engine, json);
    cJSON_Delete(json);
    if (!base) {
        return luaL_error(L, "EntityComponentListener.fromJSON() failed to deserialize");
    }

    EseEntityComponentListener *comp = (EseEntityComponentListener *)base->data;

    // Create userdata proxy and attach metatable
    EseEntityComponentListener **ud =
        (EseEntityComponentListener **)lua_newuserdata(L, sizeof(EseEntityComponentListener *));
    *ud = comp;
    luaL_getmetatable(L, ENTITY_COMPONENT_LISTENER_PROXY_META);
    lua_setmetatable(L, -2);

    return 1;
}
