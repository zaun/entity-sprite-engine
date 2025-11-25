#include "core/memory_manager.h"
#include "vendor/json/cJSON.h"
#include "entity/components/entity_component.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/listener.h"
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

static cJSON *_listener_vtable_serialize(EseEntityComponent *component) {
    return entity_component_listener_serialize((EseEntityComponentListener *)component->data);
}

// Static vtable instance for listener components
static const ComponentVTable listener_vtable = {.copy = _listener_vtable_copy,
                                                .destroy = _listener_vtable_destroy,
                                                .run_function = _listener_vtable_run_function,
                                                .collides = _listener_vtable_collides_component,
                                                .ref = _listener_vtable_ref,
                                                .unref = _listener_vtable_unref,
                                                .serialize = _listener_vtable_serialize};

EseEntityComponent *entity_component_listener_make(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "entity_component_listener_make called with NULL engine");

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

    EseEntityComponent *copy = entity_component_listener_make(src->base.lua);
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

EseEntityComponent *entity_component_listener_create(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine,
               "entity_component_listener_create called with NULL engine");

    EseEntityComponent *component = entity_component_listener_make(engine);

    // Register with Lua using ref system
    component->vtable->ref(component);

    profile_count_add("entity_comp_listener_create_count");
    return component;
}
