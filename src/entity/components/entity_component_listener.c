#include "entity/components/entity_component_listener.h"
#include "core/memory_manager.h"
#include "entity/components/entity_component.h"
#include "entity/components/entity_component_private.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "entity/systems/sound_system_private.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <string.h>

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
    const char *keys[] = {"new"};
    lua_CFunction functions[] = {_entity_component_listener_new};
    lua_engine_new_object(engine, "EntityComponentListener", 1, keys, functions);

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