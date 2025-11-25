#include "entity/bindings/listener.h"
#include "core/memory_manager.h"
#include "entity/components/entity_component.h"
#include "entity/components/listener.h"
#include "entity/systems/sound_system_private.h"
#include "scripting/lua_engine.h"
#include "types/types.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"
#include <string.h>

// ========================================
// PRIVATE FUNCTIONS
// ========================================

/**
 * @brief Retrieve an `EseEntityComponentListener` from a Lua userdata.
 *
 * @param L   Lua state pointer.
 * @param idx Stack index where the listener userdata is expected.
 * @return Pointer to the listener component, or NULL if extraction fails.
 */
static EseEntityComponentListener *_entity_component_listener_get(lua_State *L, int idx);

/**
 * @brief Lua __index metamethod for EseEntityComponentListener objects (getter).
 */
static int _entity_component_listener_index(lua_State *L);

/**
 * @brief Lua __newindex metamethod for EseEntityComponentListener objects (setter).
 */
static int _entity_component_listener_newindex(lua_State *L);

/**
 * @brief Lua __gc metamethod for EseEntityComponentListener objects.
 */
static int _entity_component_listener_gc(lua_State *L);

/**
 * @brief Lua __tostring metamethod for EseEntityComponentListener objects.
 */
static int _entity_component_listener_tostring(lua_State *L);

/**
 * @brief Lua function to create a new EseEntityComponentListener object.
 *
 * @details Callable from Lua as EntityComponentListener.new().
 */
static int _entity_component_listener_new(lua_State *L);

/**
 * @brief Lua instance method implementation for `EntityComponentListener:toJSON()`.
 */
static int _entity_component_listener_tojson_lua(lua_State *L);

/**
 * @brief Lua class method implementation for `EntityComponentListener.fromJSON()`.
 */
static int _entity_component_listener_fromjson_lua(lua_State *L);

// ========================================
// PRIVATE IMPLEMENTATION
// ========================================

static EseEntityComponentListener *_entity_component_listener_get(lua_State *L, int idx) {
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

static int _entity_component_listener_new(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 0) {
        log_debug("ENTITY_COMP", "EntityComponentListener.new() takes no arguments");
    }

    // Set engine reference
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);

    // Create EseEntityComponent wrapper
    EseEntityComponent *component = entity_component_listener_make(engine);

    // For Lua-created components, create userdata without storing a persistent ref
    EseEntityComponentListener **ud =
        (EseEntityComponentListener **)lua_newuserdata(L, sizeof(EseEntityComponentListener *));
    *ud = (EseEntityComponentListener *)component->data;
    luaL_getmetatable(L, ENTITY_COMPONENT_LISTENER_PROXY_META);
    lua_setmetatable(L, -2);

    profile_count_add("entity_comp_listener_new_count");
    return 1;
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

// ========================================
// PUBLIC FUNCTIONS
// ========================================

void entity_component_listener_init(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "entity_component_listener_init called with NULL engine");

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