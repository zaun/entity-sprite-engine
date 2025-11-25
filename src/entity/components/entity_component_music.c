#include "entity/components/entity_component_music.h"
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
#include "audio/pcm.h"
#include <string.h>

// Forward declarations for Lua helpers
static int _entity_component_music_index(lua_State *L);
static int _entity_component_music_newindex(lua_State *L);
static int _entity_component_music_gc(lua_State *L);
static int _entity_component_music_tostring(lua_State *L);
static int _entity_component_music_tojson_lua(lua_State *L);
static int _entity_component_music_fromjson_lua(lua_State *L);

static int _entity_component_music_play(lua_State *L);
static int _entity_component_music_pause(lua_State *L);
static int _entity_component_music_stop(lua_State *L);
static int _entity_component_music_seek(lua_State *L);
static int _entity_component_music_current_time(lua_State *L);
static int _entity_component_music_total_time(lua_State *L);

// Playlist proxy (comp.music) helpers
static void _entity_component_music_list_ensure_capacity(EseEntityComponentMusic *component,
                                                         size_t min_capacity);
static int _entity_component_music_list_add(lua_State *L);
static int _entity_component_music_list_remove(lua_State *L);
static int _entity_component_music_list_clear(lua_State *L);

// VTable wrapper functions
static EseEntityComponent *_music_vtable_copy(EseEntityComponent *component) {
    return _entity_component_music_copy((EseEntityComponentMusic *)component->data);
}

static void _music_vtable_destroy(EseEntityComponent *component) {
    _entity_component_music_destroy((EseEntityComponentMusic *)component->data);
}

static bool _music_vtable_run_function(EseEntityComponent *component, EseEntity *entity,
                                       const char *func_name, int argc, void *argv[]) {
    (void)component;
    (void)entity;
    (void)func_name;
    (void)argc;
    (void)argv;
    // Music components don't support function execution (yet)
    return false;
}

static void _music_vtable_collides_component(EseEntityComponent *a, EseEntityComponent *b,
                                             EseArray *out_hits) {
    (void)a;
    (void)b;
    (void)out_hits;
}

static void _music_vtable_ref(EseEntityComponent *component) {
    EseEntityComponentMusic *music = (EseEntityComponentMusic *)component->data;
    log_assert("ENTITY_COMP", music, "music vtable ref called with NULL");
    if (music->base.lua_ref == LUA_NOREF) {
        EseEntityComponentMusic **ud = (EseEntityComponentMusic **)lua_newuserdata(
            music->base.lua->runtime, sizeof(EseEntityComponentMusic *));
        *ud = music;
        luaL_getmetatable(music->base.lua->runtime, ENTITY_COMPONENT_MUSIC_PROXY_META);
        lua_setmetatable(music->base.lua->runtime, -2);
        music->base.lua_ref = luaL_ref(music->base.lua->runtime, LUA_REGISTRYINDEX);
        music->base.lua_ref_count = 1;
    } else {
        music->base.lua_ref_count++;
    }
}

static void _music_vtable_unref(EseEntityComponent *component) {
    EseEntityComponentMusic *music = (EseEntityComponentMusic *)component->data;
    if (!music) {
        return;
    }
    if (music->base.lua_ref != LUA_NOREF && music->base.lua_ref_count > 0) {
        music->base.lua_ref_count--;
        if (music->base.lua_ref_count == 0) {
            luaL_unref(music->base.lua->runtime, LUA_REGISTRYINDEX, music->base.lua_ref);
            music->base.lua_ref = LUA_NOREF;
        }
    }
}

static cJSON *_music_vtable_serialize(EseEntityComponent *component) {
    return entity_component_music_serialize((EseEntityComponentMusic *)component->data);
}

// Static vtable instance for music components
static const ComponentVTable music_vtable = {.copy = _music_vtable_copy,
                                             .destroy = _music_vtable_destroy,
                                             .run_function = _music_vtable_run_function,
                                             .collides = _music_vtable_collides_component,
                                             .ref = _music_vtable_ref,
                                             .unref = _music_vtable_unref,
                                             .serialize = _music_vtable_serialize};

static EseEntityComponent *_entity_component_music_make(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "_entity_component_music_make called with NULL engine");

    EseEntityComponentMusic *component =
        memory_manager.malloc(sizeof(EseEntityComponentMusic), MMTAG_ENTITY);
    component->base.data = component;
    component->base.active = true;
    component->base.id = ese_uuid_create(engine);
    component->base.lua = engine;
    component->base.lua_ref = LUA_NOREF;
    component->base.lua_ref_count = 0;
    component->base.type = ENTITY_COMPONENT_MUSIC;
    component->base.vtable = &music_vtable;

    component->tracks = NULL;
    component->track_count = 0;
    component->track_capacity = 0;

    component->current_track = 0;
    component->current_pcm = NULL;
    component->frame_count = 0;
    component->current_frame = 0;

    component->playing = false;
    component->repeat = false;
    component->spatial = true;
    component->xfade_time = 0.0f;

    profile_count_add("entity_comp_music_make_count");
    return &component->base;
}

EseEntityComponent *_entity_component_music_copy(const EseEntityComponentMusic *src) {
    log_assert("ENTITY_COMP", src, "_entity_component_music_copy called with NULL src");

    EseEntityComponent *base = _entity_component_music_make(src->base.lua);
    EseEntityComponentMusic *copy = (EseEntityComponentMusic *)base->data;

    // Copy playlist
    if (src->track_count > 0) {
        copy->tracks = memory_manager.malloc(sizeof(char *) * src->track_count, MMTAG_ENTITY);
        copy->track_capacity = src->track_count;
        copy->track_count = src->track_count;
        for (size_t i = 0; i < src->track_count; i++) {
            if (src->tracks[i]) {
                copy->tracks[i] = memory_manager.strdup(src->tracks[i], MMTAG_ENTITY);
            } else {
                copy->tracks[i] = NULL;
            }
        }
    }

    copy->current_track = src->current_track;
    copy->current_pcm = src->current_pcm;
    copy->frame_count = src->frame_count;
    copy->current_frame = src->current_frame;
    copy->playing = src->playing;
    copy->repeat = src->repeat;
    copy->spatial = src->spatial;
    copy->xfade_time = src->xfade_time;

    profile_count_add("entity_comp_music_copy_count");
    return &copy->base;
}

static void _entity_component_music_cleanup(EseEntityComponentMusic *component) {
    if (component->tracks) {
        for (size_t i = 0; i < component->track_count; i++) {
            if (component->tracks[i]) {
                memory_manager.free(component->tracks[i]);
            }
        }
        memory_manager.free(component->tracks);
    }
    ese_uuid_destroy(component->base.id);
    memory_manager.free(component);
    profile_count_add("entity_comp_music_destroy_count");
}

void _entity_component_music_destroy(EseEntityComponentMusic *component) {
    log_assert("ENTITY_COMP", component,
               "_entity_component_music_destroy called with NULL component");

    // Respect Lua registry ref-count; only free when no refs remain
    if (component->base.lua_ref != LUA_NOREF && component->base.lua_ref_count > 0) {
        component->base.lua_ref_count--;
        if (component->base.lua_ref_count == 0) {
            luaL_unref(component->base.lua->runtime, LUA_REGISTRYINDEX, component->base.lua_ref);
            component->base.lua_ref = LUA_NOREF;
            _entity_component_music_cleanup(component);
        } else {
            // Still referenced from Lua
            return;
        }
    } else if (component->base.lua_ref == LUA_NOREF) {
        _entity_component_music_cleanup(component);
    }
}

cJSON *entity_component_music_serialize(const EseEntityComponentMusic *component) {
    log_assert("ENTITY_COMP", component,
               "entity_component_music_serialize called with NULL component");

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        log_error("ENTITY_COMP", "Music serialize: failed to create JSON object");
        return NULL;
    }

    if (!cJSON_AddStringToObject(json, "type", "ENTITY_COMPONENT_MUSIC") ||
        !cJSON_AddBoolToObject(json, "active", component->base.active) ||
        !cJSON_AddBoolToObject(json, "repeat", component->repeat) ||
        !cJSON_AddBoolToObject(json, "is_spatial", component->spatial) ||
        !cJSON_AddNumberToObject(json, "xfade_time", (double)component->xfade_time)) {
        log_error("ENTITY_COMP", "Music serialize: failed to add base fields");
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *tracks = cJSON_CreateArray();
    if (!tracks) {
        log_error("ENTITY_COMP", "Music serialize: failed to create tracks array");
        cJSON_Delete(json);
        return NULL;
    }
    for (size_t i = 0; i < component->track_count; i++) {
        const char *name = component->tracks && component->tracks[i] ? component->tracks[i] : NULL;
        cJSON *item = name ? cJSON_CreateString(name) : cJSON_CreateNull();
        if (!item || !cJSON_AddItemToArray(tracks, item)) {
            log_error("ENTITY_COMP", "Music serialize: failed to add track");
            cJSON_Delete(tracks);
            cJSON_Delete(json);
            return NULL;
        }
    }
    if (!cJSON_AddItemToObject(json, "tracks", tracks)) {
        log_error("ENTITY_COMP", "Music serialize: failed to attach tracks");
        cJSON_Delete(tracks);
        cJSON_Delete(json);
        return NULL;
    }

    return json;
}

EseEntityComponent *entity_component_music_deserialize(EseLuaEngine *engine,
                                                       const cJSON *data) {
    log_assert("ENTITY_COMP", engine,
               "entity_component_music_deserialize called with NULL engine");
    log_assert("ENTITY_COMP", data,
               "entity_component_music_deserialize called with NULL data");

    if (!cJSON_IsObject(data)) {
        log_error("ENTITY_COMP", "Music deserialize: data is not an object");
        return NULL;
    }

    const cJSON *type_item = cJSON_GetObjectItemCaseSensitive(data, "type");
    if (!cJSON_IsString(type_item) ||
        strcmp(type_item->valuestring, "ENTITY_COMPONENT_MUSIC") != 0) {
        log_error("ENTITY_COMP", "Music deserialize: invalid or missing type");
        return NULL;
    }

    const cJSON *active_item = cJSON_GetObjectItemCaseSensitive(data, "active");
    const cJSON *repeat_item = cJSON_GetObjectItemCaseSensitive(data, "repeat");
    const cJSON *spatial_item = cJSON_GetObjectItemCaseSensitive(data, "is_spatial");
    const cJSON *xfade_item = cJSON_GetObjectItemCaseSensitive(data, "xfade_time");
    const cJSON *tracks_item = cJSON_GetObjectItemCaseSensitive(data, "tracks");

    EseEntityComponent *base = entity_component_music_create(engine);
    if (!base) {
        log_error("ENTITY_COMP", "Music deserialize: failed to create component");
        return NULL;
    }

    EseEntityComponentMusic *comp = (EseEntityComponentMusic *)base->data;
    if (cJSON_IsBool(active_item)) {
        comp->base.active = cJSON_IsTrue(active_item);
    }
    if (cJSON_IsBool(repeat_item)) {
        comp->repeat = cJSON_IsTrue(repeat_item);
    }
    if (cJSON_IsBool(spatial_item)) {
        comp->spatial = cJSON_IsTrue(spatial_item);
    }
    if (cJSON_IsNumber(xfade_item)) {
        comp->xfade_time = (float)xfade_item->valuedouble;
    }

    // Replace tracks
    if (tracks_item && cJSON_IsArray(tracks_item)) {
        if (comp->tracks) {
            for (size_t i = 0; i < comp->track_count; i++) {
                if (comp->tracks[i]) {
                    memory_manager.free(comp->tracks[i]);
                }
            }
            memory_manager.free(comp->tracks);
            comp->tracks = NULL;
            comp->track_count = 0;
            comp->track_capacity = 0;
        }
        int count = cJSON_GetArraySize(tracks_item);
        if (count > 0) {
            comp->tracks = memory_manager.malloc(sizeof(char *) * (size_t)count, MMTAG_ENTITY);
            comp->track_capacity = (size_t)count;
            comp->track_count = (size_t)count;
            for (int i = 0; i < count; i++) {
                const cJSON *item = cJSON_GetArrayItem(tracks_item, i);
                if (cJSON_IsString(item)) {
                    comp->tracks[i] = memory_manager.strdup(item->valuestring, MMTAG_ENTITY);
                } else {
                    comp->tracks[i] = NULL;
                }
            }
        }
    }

    return base;
}

// ------------------------
// Lua methods: play/pause/stop/seek/time
// ------------------------

static int _entity_component_music_play(lua_State *L) {
    EseEntityComponentMusic *component = (EseEntityComponentMusic *)lua_engine_instance_method_normalize(
        L, (EseLuaGetSelfFn)_entity_component_music_get, "EntityComponentMusic");
    if (!component) {
        return 0;
    }

    // After normalization, comp:play() takes 0 arguments.
    if (lua_gettop(L) != 0) {
        return luaL_error(L, "play() takes no arguments");
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

static int _entity_component_music_pause(lua_State *L) {
    EseEntityComponentMusic *component = (EseEntityComponentMusic *)lua_engine_instance_method_normalize(
        L, (EseLuaGetSelfFn)_entity_component_music_get, "EntityComponentMusic");
    if (!component) {
        return 0;
    }

    if (lua_gettop(L) != 0) {
        return luaL_error(L, "pause() takes no arguments");
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

static int _entity_component_music_stop(lua_State *L) {
    EseEntityComponentMusic *component = (EseEntityComponentMusic *)lua_engine_instance_method_normalize(
        L, (EseLuaGetSelfFn)_entity_component_music_get, "EntityComponentMusic");
    if (!component) {
        return 0;
    }

    if (lua_gettop(L) != 0) {
        return luaL_error(L, "stop() takes no arguments");
    }

    EseMutex *mtx = (g_sound_system_data ? g_sound_system_data->mutex : NULL);
    if (mtx) {
        ese_mutex_lock(mtx);
    }

    component->playing = false;
    component->current_frame = 0;
    component->current_track = 0;
    component->current_pcm = NULL;
    component->frame_count = 0;

    if (mtx) {
        ese_mutex_unlock(mtx);
    }

    return 0;
}

static int _entity_component_music_seek(lua_State *L) {
    EseEntityComponentMusic *component = (EseEntityComponentMusic *)lua_engine_instance_method_normalize(
        L, (EseLuaGetSelfFn)_entity_component_music_get, "EntityComponentMusic");
    if (!component) {
        return 0;
    }

    // After normalization, comp:seek(frame) has 1 argument at index 1.
    if (lua_gettop(L) != 1) {
        return luaL_error(L, "seek(frame) takes exactly 1 argument");
    }

    lua_Integer frame = luaL_checkinteger(L, 1);

    EseMutex *mtx = (g_sound_system_data ? g_sound_system_data->mutex : NULL);
    if (mtx) {
        ese_mutex_lock(mtx);
    }

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

static int _entity_component_music_current_time(lua_State *L) {
    EseEntityComponentMusic *component = (EseEntityComponentMusic *)lua_engine_instance_method_normalize(
        L, (EseLuaGetSelfFn)_entity_component_music_get, "EntityComponentMusic");
    if (!component) {
        return 0;
    }

    if (lua_gettop(L) != 0) {
        return luaL_error(L, "current_time() takes no arguments");
    }

    lua_Number seconds = 0.0;
    EsePcm *pcm = component->current_pcm;
    if (pcm) {
        uint32_t sample_rate = pcm_get_sample_rate(pcm);
        if (sample_rate > 0) {
            seconds = (lua_Number)component->current_frame / (lua_Number)sample_rate;
        }
    }

    lua_pushnumber(L, seconds);
    return 1;
}

static int _entity_component_music_total_time(lua_State *L) {
    EseEntityComponentMusic *component = (EseEntityComponentMusic *)lua_engine_instance_method_normalize(
        L, (EseLuaGetSelfFn)_entity_component_music_get, "EntityComponentMusic");
    if (!component) {
        return 0;
    }

    if (lua_gettop(L) != 0) {
        return luaL_error(L, "total_time() takes no arguments");
    }

    lua_Number seconds = 0.0;
    EsePcm *pcm = component->current_pcm;
    if (pcm) {
        uint32_t sample_rate = pcm_get_sample_rate(pcm);
        if (sample_rate > 0) {
            seconds = (lua_Number)component->frame_count / (lua_Number)sample_rate;
        }
    }

    lua_pushnumber(L, seconds);
    return 1;
}

// ------------------------
// Lua metamethods for EntityComponentMusic
// ------------------------

static int _entity_component_music_index(lua_State *L) {
    EseEntityComponentMusic *component = _entity_component_music_get(L, 1);
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
    } else if (strcmp(key, "frame_count") == 0) {
        lua_pushinteger(L, (lua_Integer)component->frame_count);
        return 1;
    } else if (strcmp(key, "current_frame") == 0) {
        lua_pushinteger(L, (lua_Integer)component->current_frame);
        return 1;
    } else if (strcmp(key, "is_playing") == 0) {
        lua_pushboolean(L, component->playing);
        return 1;
    } else if (strcmp(key, "repeat") == 0) {
        lua_pushboolean(L, component->repeat);
        return 1;
    } else if (strcmp(key, "is_spatial") == 0) {
        lua_pushboolean(L, component->spatial);
        return 1;
    } else if (strcmp(key, "xfade_time") == 0) {
        lua_pushnumber(L, (lua_Number)component->xfade_time);
        return 1;
    } else if (strcmp(key, "toJSON") == 0) {
        lua_pushcfunction(L, _entity_component_music_tojson_lua);
        return 1;
    } else if (strcmp(key, "music") == 0) {
        // Playlist proxy: return a plain Lua table so that #comp.music works
        // under Lua 5.1 (which does not support __len for userdata).
        lua_newtable(L);

        // Numeric array part: 1..track_count with track names
        for (size_t i = 0; i < component->track_count; i++) {
            if (component->tracks && component->tracks[i]) {
                lua_pushstring(L, component->tracks[i]);
                lua_rawseti(L, -2, (lua_Integer)i + 1);
            }
        }

        // Expose count property
        lua_pushinteger(L, (lua_Integer)component->track_count);
        lua_setfield(L, -2, "count");

        // Methods: add/remove/clear capture the C component pointer as upvalues
        lua_pushlightuserdata(L, component);
        lua_pushcclosure(L, _entity_component_music_list_add, 1);
        lua_setfield(L, -2, "add");

        lua_pushlightuserdata(L, component);
        lua_pushcclosure(L, _entity_component_music_list_remove, 1);
        lua_setfield(L, -2, "remove");

        lua_pushlightuserdata(L, component);
        lua_pushcclosure(L, _entity_component_music_list_clear, 1);
        lua_setfield(L, -2, "clear");

        return 1;
    } else if (strcmp(key, "play") == 0) {
        lua_pushlightuserdata(L, component);
        lua_pushcclosure(L, _entity_component_music_play, 1);
        return 1;
    } else if (strcmp(key, "pause") == 0) {
        lua_pushlightuserdata(L, component);
        lua_pushcclosure(L, _entity_component_music_pause, 1);
        return 1;
    } else if (strcmp(key, "stop") == 0) {
        lua_pushlightuserdata(L, component);
        lua_pushcclosure(L, _entity_component_music_stop, 1);
        return 1;
    } else if (strcmp(key, "seek") == 0) {
        lua_pushlightuserdata(L, component);
        lua_pushcclosure(L, _entity_component_music_seek, 1);
        return 1;
    } else if (strcmp(key, "current_time") == 0) {
        lua_pushlightuserdata(L, component);
        lua_pushcclosure(L, _entity_component_music_current_time, 1);
        return 1;
    } else if (strcmp(key, "total_time") == 0) {
        lua_pushlightuserdata(L, component);
        lua_pushcclosure(L, _entity_component_music_total_time, 1);
        return 1;
    }

    return 0;
}

static int _entity_component_music_newindex(lua_State *L) {
    EseEntityComponentMusic *component = _entity_component_music_get(L, 1);
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
    } else if (strcmp(key, "is_spatial") == 0) {
        if (!lua_isboolean(L, 3)) {
            if (mtx) {
                ese_mutex_unlock(mtx);
            }
            return luaL_error(L, "is_spatial must be a boolean");
        }
        component->spatial = lua_toboolean(L, 3);
        if (mtx) {
            ese_mutex_unlock(mtx);
        }
        return 0;
    } else if (strcmp(key, "xfade_time") == 0) {
        if (!lua_isnumber(L, 3)) {
            if (mtx) {
                ese_mutex_unlock(mtx);
            }
            return luaL_error(L, "xfade_time must be a number");
        }
        float t = (float)lua_tonumber(L, 3);
        if (t < 0.0f) {
            t = 0.0f;
        }
        component->xfade_time = t;
        if (mtx) {
            ese_mutex_unlock(mtx);
        }
        return 0;
    } else if (strcmp(key, "music") == 0) {
        if (mtx) {
            ese_mutex_unlock(mtx);
        }
        return luaL_error(L, "music list is not assignable; use music:add/remove/clear");
    }

    if (mtx) {
        ese_mutex_unlock(mtx);
    }

    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

static int _entity_component_music_gc(lua_State *L) {
    // Get from userdata
    EseEntityComponentMusic **ud = (EseEntityComponentMusic **)luaL_testudata(
        L, 1, ENTITY_COMPONENT_MUSIC_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }

    EseEntityComponentMusic *component = *ud;
    if (component) {
        if (component->base.lua_ref == LUA_NOREF) {
            _entity_component_music_destroy(component);
            *ud = NULL;
        }
    }

    return 0;
}

static int _entity_component_music_tostring(lua_State *L) {
    EseEntityComponentMusic *component = _entity_component_music_get(L, 1);

    if (!component) {
        lua_pushstring(L, "EntityComponentMusic: (invalid)");
        return 1;
    }

    char buf[256];
    snprintf(buf, sizeof(buf),
             "EntityComponentMusic: %p (id=%s active=%s tracks=%zu current_track=%u "
             "frame_count=%u current_frame=%u playing=%s repeat=%s)",
             (void *)component, ese_uuid_get_value(component->base.id),
             component->base.active ? "true" : "false", component->track_count,
             (unsigned int)component->current_track, (unsigned int)component->frame_count,
             (unsigned int)component->current_frame, component->playing ? "true" : "false",
             component->repeat ? "true" : "false");
    lua_pushstring(L, buf);

    return 1;
}

// ------------------------
// Lua constructor and helpers
// ------------------------

static int _entity_component_music_new(lua_State *L) {
    const char *initial_track = NULL;

    int n_args = lua_gettop(L);
    if (n_args == 1 && lua_isstring(L, 1)) {
        initial_track = lua_tostring(L, 1);
    } else if (n_args == 1 && !lua_isstring(L, 1)) {
        log_debug("ENTITY_COMP", "EntityComponentMusic.new(String) expects a string; argument ignored");
    } else if (n_args != 0) {
        log_debug("ENTITY_COMP",
                  "EntityComponentMusic.new() or EntityComponentMusic.new(String)");
    }

    // Set engine reference
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);

    // Create EseEntityComponent wrapper
    EseEntityComponent *base = _entity_component_music_make(engine);
    EseEntityComponentMusic *component = (EseEntityComponentMusic *)base->data;

    if (initial_track) {
        component->tracks = memory_manager.malloc(sizeof(char *), MMTAG_ENTITY);
        component->tracks[0] = memory_manager.strdup(initial_track, MMTAG_ENTITY);
        component->track_count = 1;
        component->track_capacity = 1;
    }

    // For Lua-created components, create userdata without storing a persistent ref
    EseEntityComponentMusic **ud =
        (EseEntityComponentMusic **)lua_newuserdata(L, sizeof(EseEntityComponentMusic *));
    *ud = component;
    luaL_getmetatable(L, ENTITY_COMPONENT_MUSIC_PROXY_META);
    lua_setmetatable(L, -2);

    profile_count_add("entity_comp_music_new_count");
    return 1;
}

EseEntityComponentMusic *_entity_component_music_get(lua_State *L, int idx) {
    // Check if it's userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }

    // Get the userdata and check metatable
    EseEntityComponentMusic **ud =
        (EseEntityComponentMusic **)luaL_testudata(L, idx, ENTITY_COMPONENT_MUSIC_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }

    return *ud;
}

// ------------------------
// Playlist proxy (comp.music)
// ------------------------

static void _entity_component_music_list_ensure_capacity(EseEntityComponentMusic *component,
                                                         size_t                    min_capacity) {
    if (component->track_capacity >= min_capacity) {
        return;
    }
    size_t new_capacity = component->track_capacity ? component->track_capacity * 2 : 4;
    if (new_capacity < min_capacity) {
        new_capacity = min_capacity;
    }
    char **new_tracks =
        memory_manager.realloc(component->tracks, sizeof(char *) * new_capacity, MMTAG_ENTITY);
    component->tracks = new_tracks;
    component->track_capacity = new_capacity;
}

static int _entity_component_music_list_add(lua_State *L) {
    // C component pointer is captured as upvalue[1].
    EseEntityComponentMusic *component =
        (EseEntityComponentMusic *)lua_touserdata(L, lua_upvalueindex(1));
    if (!component) {
        return 0;
    }

    int top = lua_gettop(L);
    const char *name = NULL;

    // Support both:
    //   music:add("id")  -> [self, name]
    //   music.add("id")   -> [name]
    if (top == 1 && lua_isstring(L, 1)) {
        name = lua_tostring(L, 1);
    } else if (top == 2 && lua_istable(L, 1) && lua_isstring(L, 2)) {
        name = lua_tostring(L, 2);
    } else {
        return luaL_error(L, "add(name) expects a single string argument");
    }

    EseMutex *mtx = (g_sound_system_data ? g_sound_system_data->mutex : NULL);
    if (mtx) {
        ese_mutex_lock(mtx);
    }

    _entity_component_music_list_ensure_capacity(component, component->track_count + 1);
    component->tracks[component->track_count] = memory_manager.strdup(name, MMTAG_ENTITY);
    component->track_count++;

    if (mtx) {
        ese_mutex_unlock(mtx);
    }

    return 0;
}

static int _entity_component_music_list_remove(lua_State *L) {
    EseEntityComponentMusic *component =
        (EseEntityComponentMusic *)lua_touserdata(L, lua_upvalueindex(1));
    if (!component) {
        return 0;
    }

    int top = lua_gettop(L);
    const char *name = NULL;

    // Support music:remove("id") and music.remove("id")
    if (top == 1 && lua_isstring(L, 1)) {
        name = lua_tostring(L, 1);
    } else if (top == 2 && lua_istable(L, 1) && lua_isstring(L, 2)) {
        name = lua_tostring(L, 2);
    } else {
        return luaL_error(L, "remove(name) expects a single string argument");
    }

    EseMutex *mtx = (g_sound_system_data ? g_sound_system_data->mutex : NULL);
    if (mtx) {
        ese_mutex_lock(mtx);
    }

    int found_index = -1;
    for (size_t i = 0; i < component->track_count; i++) {
        if (component->tracks[i] && strcmp(component->tracks[i], name) == 0) {
            found_index = (int)i;
            break;
        }
    }

    if (found_index >= 0) {
        memory_manager.free(component->tracks[found_index]);
        for (size_t i = (size_t)found_index; i + 1 < component->track_count; i++) {
            component->tracks[i] = component->tracks[i + 1];
        }
        component->track_count--;
        if (component->track_count == 0) {
            component->playing = false;
            component->current_track = 0;
            component->current_frame = 0;
            component->current_pcm = NULL;
            component->frame_count = 0;
        } else if ((size_t)found_index <= component->current_track && component->current_track > 0) {
            // Keep current_track pointing at the same logical song if possible
            component->current_track--;
        }
        lua_pushboolean(L, 1);
    } else {
        lua_pushboolean(L, 0);
    }

    if (mtx) {
        ese_mutex_unlock(mtx);
    }

    return 1;
}

static int _entity_component_music_list_clear(lua_State *L) {
    EseEntityComponentMusic *component =
        (EseEntityComponentMusic *)lua_touserdata(L, lua_upvalueindex(1));
    if (!component) {
        return 0;
    }

    int top = lua_gettop(L);
    // Allow both music:clear() -> [self] and music.clear() -> []
    if (!(top == 0 || (top == 1 && lua_istable(L, 1)))) {
        return luaL_error(L, "clear() takes no arguments");
    }

    EseMutex *mtx = (g_sound_system_data ? g_sound_system_data->mutex : NULL);
    if (mtx) {
        ese_mutex_lock(mtx);
    }

    for (size_t i = 0; i < component->track_count; i++) {
        if (component->tracks[i]) {
            memory_manager.free(component->tracks[i]);
            component->tracks[i] = NULL;
        }
    }
    component->track_count = 0;
    component->playing = false;
    component->current_track = 0;
    component->current_frame = 0;
    component->current_pcm = NULL;
    component->frame_count = 0;

    if (mtx) {
        ese_mutex_unlock(mtx);
    }

    return 0;
}

// ------------------------
// Public init / factory
// ------------------------

void _entity_component_music_init(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "_entity_component_music_init called with NULL engine");

    // Create main metatable for EntityComponentMusic
    lua_engine_new_object_meta(engine, ENTITY_COMPONENT_MUSIC_PROXY_META,
                               _entity_component_music_index, _entity_component_music_newindex,
                               _entity_component_music_gc, _entity_component_music_tostring);

    // Create global EntityComponentMusic table with functions
    const char *keys[] = {"new", "fromJSON"};
    lua_CFunction functions[] = {_entity_component_music_new, _entity_component_music_fromjson_lua};
    lua_engine_new_object(engine, "EntityComponentMusic", 2, keys, functions);

    profile_count_add("entity_comp_music_init_count");
}

EseEntityComponent *entity_component_music_create(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "entity_component_music_create called with NULL engine");

    EseEntityComponent *component = _entity_component_music_make(engine);

    // Register with Lua using ref system
    component->vtable->ref(component);

    profile_count_add("entity_comp_music_create_count");
    return component;
}

static int _entity_component_music_tojson_lua(lua_State *L) {
    EseEntityComponentMusic *self = _entity_component_music_get(L, 1);
    if (!self) {
        return luaL_error(L, "EntityComponentMusic:toJSON() called on invalid component");
    }
    if (lua_gettop(L) != 1) {
        return luaL_error(L, "EntityComponentMusic:toJSON() takes 0 arguments");
    }
    cJSON *json = entity_component_music_serialize(self);
    if (!json) {
        return luaL_error(L, "EntityComponentMusic:toJSON() failed to serialize");
    }
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!json_str) {
        return luaL_error(L, "EntityComponentMusic:toJSON() failed to stringify");
    }
    lua_pushstring(L, json_str);
    free(json_str);
    return 1;
}

static int _entity_component_music_fromjson_lua(lua_State *L) {
    const char *json_str = luaL_checkstring(L, 1);
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (!engine) {
        return luaL_error(L, "EntityComponentMusic.fromJSON() could not get engine");
    }

    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        return luaL_error(L, "EntityComponentMusic.fromJSON() failed to parse JSON");
    }

    EseEntityComponent *base = entity_component_music_deserialize(engine, json);
    cJSON_Delete(json);
    if (!base) {
        return luaL_error(L, "EntityComponentMusic.fromJSON() failed to deserialize");
    }

    EseEntityComponentMusic *comp = (EseEntityComponentMusic *)base->data;

    // Create userdata proxy and attach metatable
    EseEntityComponentMusic **ud =
        (EseEntityComponentMusic **)lua_newuserdata(L, sizeof(EseEntityComponentMusic *));
    *ud = comp;
    luaL_getmetatable(L, ENTITY_COMPONENT_MUSIC_PROXY_META);
    lua_setmetatable(L, -2);

    return 1;
}
