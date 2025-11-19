#include "entity/components/entity_component_sound.h"
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
static EseEntityComponent *_sound_vtable_copy(EseEntityComponent *component) {
    return _entity_component_sound_copy((EseEntityComponentSound *)component->data);
}

static void _sound_vtable_destroy(EseEntityComponent *component) {
    _entity_component_sound_destroy((EseEntityComponentSound *)component->data);
}

static bool _sound_vtable_run_function(EseEntityComponent *component, EseEntity *entity,
                                       const char *func_name, int argc, void *argv[]) {
    (void)component;
    (void)entity;
    (void)func_name;
    (void)argc;
    (void)argv;
    // Sound components don't support function execution (yet)
    return false;
}

static void _sound_vtable_collides_component(EseEntityComponent *a, EseEntityComponent *b,
                                             EseArray *out_hits) {
    (void)a;
    (void)b;
    (void)out_hits;
}

static void _sound_vtable_ref(EseEntityComponent *component) {
    EseEntityComponentSound *sound = (EseEntityComponentSound *)component->data;
    log_assert("ENTITY_COMP", sound, "sound vtable ref called with NULL");
    if (sound->base.lua_ref == LUA_NOREF) {
        EseEntityComponentSound **ud = (EseEntityComponentSound **)lua_newuserdata(
            sound->base.lua->runtime, sizeof(EseEntityComponentSound *));
        *ud = sound;
        luaL_getmetatable(sound->base.lua->runtime, ENTITY_COMPONENT_SOUND_PROXY_META);
        lua_setmetatable(sound->base.lua->runtime, -2);
        sound->base.lua_ref = luaL_ref(sound->base.lua->runtime, LUA_REGISTRYINDEX);
        sound->base.lua_ref_count = 1;
    } else {
        sound->base.lua_ref_count++;
    }
}

static void _sound_vtable_unref(EseEntityComponent *component) {
    EseEntityComponentSound *sound = (EseEntityComponentSound *)component->data;
    if (!sound)
        return;
    if (sound->base.lua_ref != LUA_NOREF && sound->base.lua_ref_count > 0) {
        sound->base.lua_ref_count--;
        if (sound->base.lua_ref_count == 0) {
            luaL_unref(sound->base.lua->runtime, LUA_REGISTRYINDEX, sound->base.lua_ref);
            sound->base.lua_ref = LUA_NOREF;
        }
    }
}

// Static vtable instance for sound components
static const ComponentVTable sound_vtable = {.copy = _sound_vtable_copy,
                                             .destroy = _sound_vtable_destroy,
                                             .run_function = _sound_vtable_run_function,
                                             .collides = _sound_vtable_collides_component,
                                             .ref = _sound_vtable_ref,
                                             .unref = _sound_vtable_unref};

static EseEntityComponent *_entity_component_sound_make(EseLuaEngine *engine, const char *sound_name) {
    log_assert("ENTITY_COMP", engine, "_entity_component_sound_make called with NULL engine");

    EseEntityComponentSound *component =
        memory_manager.malloc(sizeof(EseEntityComponentSound), MMTAG_ENTITY);
    component->base.data = component;
    component->base.active = true;
    component->base.id = ese_uuid_create(engine);
    component->base.lua = engine;
    component->base.lua_ref = LUA_NOREF;
    component->base.lua_ref_count = 0;
    component->base.type = ENTITY_COMPONENT_SOUND;
    component->base.vtable = &sound_vtable;

    component->frame_count = 0;
    component->current_frame = 0;
    component->playing = false;
    component->repeat = false;

    if (sound_name != NULL) {
        component->sound_name = memory_manager.strdup(sound_name, MMTAG_ENTITY);
    } else {
        component->sound_name = NULL;
    }

    profile_count_add("entity_comp_sound_make_count");
    return &component->base;
}

EseEntityComponent *_entity_component_sound_copy(const EseEntityComponentSound *src) {
    log_assert("ENTITY_COMP", src, "_entity_component_sound_copy called with NULL src");

    EseEntityComponent *copy = _entity_component_sound_make(src->base.lua, src->sound_name);
    EseEntityComponentSound *sound_copy = (EseEntityComponentSound *)copy->data;

    // Copy playback state
    sound_copy->frame_count = src->frame_count;
    sound_copy->current_frame = src->current_frame;
    sound_copy->playing = src->playing;
    sound_copy->repeat = src->repeat;

    profile_count_add("entity_comp_sound_copy_count");
    return copy;
}

static void _entity_component_sound_cleanup(EseEntityComponentSound *component) {
    memory_manager.free(component->sound_name);
    ese_uuid_destroy(component->base.id);
    memory_manager.free(component);
    profile_count_add("entity_comp_sound_destroy_count");
}

void _entity_component_sound_destroy(EseEntityComponentSound *component) {
    log_assert("ENTITY_COMP", component, "_entity_component_sound_destroy called with NULL src");

    // Respect Lua registry ref-count; only free when no refs remain
    if (component->base.lua_ref != LUA_NOREF && component->base.lua_ref_count > 0) {
        component->base.lua_ref_count--;
        if (component->base.lua_ref_count == 0) {
            luaL_unref(component->base.lua->runtime, LUA_REGISTRYINDEX, component->base.lua_ref);
            component->base.lua_ref = LUA_NOREF;
            _entity_component_sound_cleanup(component);
        } else {
            // We don't "own" the component yet, still referenced from Lua
            return;
        }
    } else if (component->base.lua_ref == LUA_NOREF) {
        _entity_component_sound_cleanup(component);
    }
}

/**
 * @brief Lua method: comp:play(self)
 *
 * Supports both method-call syntax (comp:play()) and property-call syntax
 * (comp.play()) by either reading the component from the first argument
 * or from an upvalue bound by the __index metamethod.
 */
static int _entity_component_sound_play(lua_State *L) {
    EseEntityComponentSound *component = _entity_component_sound_get(L, 1);
    if (!component) {
        // Fallback: attempt to read bound component from upvalue (for comp.play()).
        component = _entity_component_sound_get(L, lua_upvalueindex(1));
        if (!component) {
            return 0;
        }
    }

    EseMutex *mtx = (g_sound_system_data ? g_sound_system_data->mutex : NULL);
    if (mtx) {
        ese_mutex_lock(mtx);
    }

    component->playing = true;

    if (mtx) {
        ese_mutex_unlock(mtx);
    }

    return 0;
}

/**
 * @brief Lua method: comp:pause(self)
 */
static int _entity_component_sound_pause(lua_State *L) {
    EseEntityComponentSound *component = _entity_component_sound_get(L, 1);
    if (!component) {
        component = _entity_component_sound_get(L, lua_upvalueindex(1));
        if (!component) {
            return 0;
        }
    }

    EseMutex *mtx = (g_sound_system_data ? g_sound_system_data->mutex : NULL);
    if (mtx) {
        ese_mutex_lock(mtx);
    }

    component->playing = false;

    if (mtx) {
        ese_mutex_unlock(mtx);
    }

    return 0;
}

/**
 * @brief Lua method: comp:stop(self)
 */
static int _entity_component_sound_stop(lua_State *L) {
    EseEntityComponentSound *component = _entity_component_sound_get(L, 1);
    if (!component) {
        component = _entity_component_sound_get(L, lua_upvalueindex(1));
        if (!component) {
            return 0;
        }
    }

    EseMutex *mtx = (g_sound_system_data ? g_sound_system_data->mutex : NULL);
    if (mtx) {
        ese_mutex_lock(mtx);
    }

    component->playing = false;
    component->current_frame = 0;

    if (mtx) {
        ese_mutex_unlock(mtx);
    }

    return 0;
}

/**
 * @brief Lua method: comp:seek(self, frame)
 */
static int _entity_component_sound_seek(lua_State *L) {
    EseEntityComponentSound *component = _entity_component_sound_get(L, 1);
    if (!component) {
        component = _entity_component_sound_get(L, lua_upvalueindex(1));
        if (!component) {
            return 0;
        }
    }

    EseMutex *mtx = (g_sound_system_data ? g_sound_system_data->mutex : NULL);
    if (mtx) {
        ese_mutex_lock(mtx);
    }

    lua_Integer frame = luaL_checkinteger(L, 2);
    if (frame < 0 || (uint32_t)frame > component->frame_count) {
        if (mtx) {
            ese_mutex_unlock(mtx);
        }
        return luaL_error(L, "seek frame must be between 0 and frame_count");
    }

    component->current_frame = (uint32_t)frame;

    if (mtx) {
        ese_mutex_unlock(mtx);
    }

    return 0;
}

/**
 * @brief Lua __index metamethod for EseEntityComponentSound objects (getter).
 */
static int _entity_component_sound_index(lua_State *L) {
    EseEntityComponentSound *component = _entity_component_sound_get(L, 1);
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
    } else if (strcmp(key, "sound") == 0) {
        if (component->sound_name) {
            lua_pushstring(L, component->sound_name);
        } else {
            lua_pushnil(L);
        }
        return 1;
    } else if (strcmp(key, "frame_count") == 0) {
        lua_pushinteger(L, (lua_Integer)component->frame_count);
        return 1;
    } else if (strcmp(key, "current_frame") == 0) {
        lua_pushinteger(L, (lua_Integer)component->current_frame);
        return 1;
    } else if (strcmp(key, "playing") == 0) {
        lua_pushboolean(L, component->playing);
        return 1;
    } else if (strcmp(key, "repeat") == 0) {
        lua_pushboolean(L, component->repeat);
        return 1;
    } else if (strcmp(key, "play") == 0) {
        /*
         * Return a closure bound to this component so both comp:play()
         * and comp.play() work correctly.
         */
        if (component->base.lua_ref != LUA_NOREF) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, component->base.lua_ref); /* push self userdata */
            lua_pushcclosure(L, _entity_component_sound_play, 1);
        } else {
            lua_pushcfunction(L, _entity_component_sound_play);
        }
        return 1;
    } else if (strcmp(key, "pause") == 0) {
        if (component->base.lua_ref != LUA_NOREF) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, component->base.lua_ref);
            lua_pushcclosure(L, _entity_component_sound_pause, 1);
        } else {
            lua_pushcfunction(L, _entity_component_sound_pause);
        }
        return 1;
    } else if (strcmp(key, "stop") == 0) {
        if (component->base.lua_ref != LUA_NOREF) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, component->base.lua_ref);
            lua_pushcclosure(L, _entity_component_sound_stop, 1);
        } else {
            lua_pushcfunction(L, _entity_component_sound_stop);
        }
        return 1;
    } else if (strcmp(key, "seek") == 0) {
        if (component->base.lua_ref != LUA_NOREF) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, component->base.lua_ref);
            lua_pushcclosure(L, _entity_component_sound_seek, 1);
        } else {
            lua_pushcfunction(L, _entity_component_sound_seek);
        }
        return 1;
    }

    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseEntityComponentSound objects (setter).
 */
static int _entity_component_sound_newindex(lua_State *L) {
    EseEntityComponentSound *component = _entity_component_sound_get(L, 1);
    const char *key = lua_tostring(L, 2);

    // SAFETY: Silently ignore writes to freed components
    if (!component) {
        return 0;
    }

    if (!key)
        return 0;

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
    } else if (strcmp(key, "sound") == 0) {
        if (!lua_isstring(L, 3) && !lua_isnil(L, 3)) {
            if (mtx) {
                ese_mutex_unlock(mtx);
            }
            return luaL_error(L, "sound must be a string or nil");
        }

        if (component->sound_name != NULL) {
            memory_manager.free(component->sound_name);
            component->sound_name = NULL;
        }

        if (lua_isstring(L, 3)) {
            const char *sound_name = lua_tostring(L, 3);
            component->sound_name = memory_manager.strdup(sound_name, MMTAG_ENTITY);
            // Reset playback state when sound changes
            component->current_frame = 0;
            component->frame_count = 0;
        }

        if (mtx) {
            ese_mutex_unlock(mtx);
        }
        return 0;
    } else if (strcmp(key, "frame_count") == 0 || strcmp(key, "current_frame") == 0) {
        if (mtx) {
            ese_mutex_unlock(mtx);
        }
        return luaL_error(L, "%s is read-only", key);
    } else if (strcmp(key, "repeat") == 0) {
        if (!lua_isboolean(L, 3)) {
            if (mtx) {
                ese_mutex_unlock(mtx);
            }
            return luaL_error(L, "repeat must be a boolean");
        }
        component->repeat = lua_toboolean(L, 3);
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
 * @brief Lua __gc metamethod for EseEntityComponentSound objects.
 */
static int _entity_component_sound_gc(lua_State *L) {
    // Get from userdata
    EseEntityComponentSound **ud =
        (EseEntityComponentSound **)luaL_testudata(L, 1, ENTITY_COMPONENT_SOUND_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }

    EseEntityComponentSound *component = *ud;
    if (component) {
        if (component->base.lua_ref == LUA_NOREF) {
            _entity_component_sound_destroy(component);
            *ud = NULL;
        }
    }

    return 0;
}

static int _entity_component_sound_tostring(lua_State *L) {
    EseEntityComponentSound *component = _entity_component_sound_get(L, 1);

    if (!component) {
        lua_pushstring(L, "EntityComponentSound: (invalid)");
        return 1;
    }

    char buf[256];
    snprintf(buf, sizeof(buf),
             "EntityComponentSound: %p (id=%s active=%s sound=%s frame_count=%u current_frame=%u playing=%s repeat=%s)",
             (void *)component, ese_uuid_get_value(component->base.id),
             component->base.active ? "true" : "false",
             component->sound_name ? component->sound_name : "nil",
             (unsigned int)component->frame_count, (unsigned int)component->current_frame,
             component->playing ? "true" : "false",
             component->repeat ? "true" : "false");
    lua_pushstring(L, buf);

    return 1;
}

/**
 * @brief Lua function to create a new EseEntityComponentSound object.
 *
 * @details Callable from Lua as EntityComponentSound.new().
 */
static int _entity_component_sound_new(lua_State *L) {
    const char *sound_name = NULL;

    int n_args = lua_gettop(L);
    if (n_args == 1 && lua_isstring(L, 1)) {
        sound_name = lua_tostring(L, 1);
    } else if (n_args == 1 && !lua_isstring(L, 1)) {
        log_debug("ENTITY_COMP", "Sound must be a string, ignored");
    } else if (n_args != 0) {
        log_debug("ENTITY_COMP",
                  "EntityComponentSound.new() or EntityComponentSound.new(String)");
    }

    // Set engine reference
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);

    // Create EseEntityComponent wrapper
    EseEntityComponent *component = _entity_component_sound_make(engine, sound_name);

    // For Lua-created components, create userdata without storing a persistent ref
    EseEntityComponentSound **ud =
        (EseEntityComponentSound **)lua_newuserdata(L, sizeof(EseEntityComponentSound *));
    *ud = (EseEntityComponentSound *)component->data;
    luaL_getmetatable(L, ENTITY_COMPONENT_SOUND_PROXY_META);
    lua_setmetatable(L, -2);

    profile_count_add("entity_comp_sound_new_count");
    return 1;
}

EseEntityComponentSound *_entity_component_sound_get(lua_State *L, int idx) {
    // Check if it's userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }

    // Get the userdata and check metatable
    EseEntityComponentSound **ud =
        (EseEntityComponentSound **)luaL_testudata(L, idx, ENTITY_COMPONENT_SOUND_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }

    return *ud;
}

void _entity_component_sound_init(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "_entity_component_sound_init called with NULL engine");

    // Create metatable
    lua_engine_new_object_meta(engine, ENTITY_COMPONENT_SOUND_PROXY_META,
                               _entity_component_sound_index, _entity_component_sound_newindex,
                               _entity_component_sound_gc, _entity_component_sound_tostring);

    // Create global EntityComponentSound table with functions
    const char *keys[] = {"new"};
    lua_CFunction functions[] = {_entity_component_sound_new};
    lua_engine_new_object(engine, "EntityComponentSound", 1, keys, functions);

    profile_count_add("entity_comp_sound_init_count");
}

EseEntityComponent *entity_component_sound_create(EseLuaEngine *engine, const char *sound_name) {
    log_assert("ENTITY_COMP", engine, "entity_component_sound_create called with NULL engine");

    EseEntityComponent *component = _entity_component_sound_make(engine, sound_name);

    // Register with Lua using ref system
    component->vtable->ref(component);

    profile_count_add("entity_comp_sound_create_count");
    return component;
}
