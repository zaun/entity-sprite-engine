#include "entity/components/entity_component_map.h"
#include "core/asset_manager.h"
#include "core/collision_resolver.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "entity/components/collider.h"
#include "entity/components/entity_component_private.h"
#include "entity/entity_private.h"
#include "graphics/sprite.h"
#include "scripting/lua_engine.h"
#include "types/types.h"
#include "utility/array.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"
#include <math.h>
#include <string.h>

// Standard entity function names
static const char *STANDARD_FUNCTIONS[] = {"map_init", "map_update", "cell_update", "cell_enter",
                                           "cell_exit"};
static const size_t STANDARD_FUNCTIONS_COUNT =
    sizeof(STANDARD_FUNCTIONS) / sizeof(STANDARD_FUNCTIONS[0]);

// Forward declarations
bool _entity_component_map_collides_component(EseEntityComponentMap *component,
                                              EseEntityComponentCollider *collider,
                                              EseArray *out_hits);
void _entity_component_map_cache_functions(EseEntityComponentMap *component);
void _entity_component_map_clear_cache(EseEntityComponentMap *component);
static void _entity_component_map_changed(EseMap *map, void *userdata);
static int _entity_component_map_show_layer_index(lua_State *L);
static int _entity_component_map_tojson_lua(lua_State *L);
static int _entity_component_map_show_layer_newindex(lua_State *L);
static int _entity_component_map_show_layer_len(lua_State *L);
static int _entity_component_map_show_all_layers(lua_State *L);

// VTable wrapper functions
static EseEntityComponent *_map_vtable_copy(EseEntityComponent *component) {
    return _entity_component_map_copy((EseEntityComponentMap *)component->data);
}

static void _map_vtable_destroy(EseEntityComponent *component) {
    _entity_component_map_destroy((EseEntityComponentMap *)component->data);
}

static bool _map_vtable_run_function(EseEntityComponent *component, EseEntity *entity,
                                     const char *func_name, int argc, void *argv[]) {
    // Map components don't support function execution
    return false;
}

static void _map_vtable_collides_component(EseEntityComponent *a, EseEntityComponent *b,
                                           EseArray *out_hits) {
    _entity_component_map_collides_component((EseEntityComponentMap *)a->data,
                                             (EseEntityComponentCollider *)b->data, out_hits);
}

static void _map_vtable_ref(EseEntityComponent *component) {
    EseEntityComponentMap *map = (EseEntityComponentMap *)component->data;
    log_assert("ENTITY_COMP", map, "map vtable ref called with NULL");
    if (map->base.lua_ref == LUA_NOREF) {
        EseEntityComponentMap **ud = (EseEntityComponentMap **)lua_newuserdata(
            map->base.lua->runtime, sizeof(EseEntityComponentMap *));
        *ud = map;
        luaL_getmetatable(map->base.lua->runtime, ENTITY_COMPONENT_MAP_PROXY_META);
        lua_setmetatable(map->base.lua->runtime, -2);
        map->base.lua_ref = luaL_ref(map->base.lua->runtime, LUA_REGISTRYINDEX);
        map->base.lua_ref_count = 1;
    } else {
        map->base.lua_ref_count++;
    }
}

static void _map_vtable_unref(EseEntityComponent *component) {
    EseEntityComponentMap *map = (EseEntityComponentMap *)component->data;
    if (!map)
        return;
    if (map->base.lua_ref != LUA_NOREF && map->base.lua_ref_count > 0) {
        map->base.lua_ref_count--;
        if (map->base.lua_ref_count == 0) {
            luaL_unref(map->base.lua->runtime, LUA_REGISTRYINDEX, map->base.lua_ref);
            map->base.lua_ref = LUA_NOREF;
        }
    }
}

static cJSON *_map_vtable_serialize(EseEntityComponent *component) {
    return entity_component_map_serialize((EseEntityComponentMap *)component->data);
}

// Static vtable instance for map components
static const ComponentVTable map_vtable = {.copy = _map_vtable_copy,
                                           .destroy = _map_vtable_destroy,
                                           .run_function = _map_vtable_run_function,
                                           .collides = _map_vtable_collides_component,
                                           .ref = _map_vtable_ref,
                                           .unref = _map_vtable_unref,
                                           .serialize = _map_vtable_serialize};

// callback

static EseEntityComponent *_entity_component_map_make(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "_entity_component_map_make called with NULL engine");

    EseEntityComponentMap *component =
        memory_manager.malloc(sizeof(EseEntityComponentMap), MMTAG_COMP_MAP);
    component->base.data = component;
    component->base.active = true;
    component->base.id = ese_uuid_create(engine);
    component->base.lua = engine;
    component->base.lua_ref = LUA_NOREF;
    component->base.lua_ref_count = 0;
    component->base.type = ENTITY_COMPONENT_MAP;
    component->base.vtable = &map_vtable;

    component->map = NULL;
    component->size = 128;
    component->seed = 1000;

    // Lua Script
    component->script = NULL;
    component->engine = engine;
    component->instance_ref = LUA_NOREF;
    component->function_cache = hashmap_create(NULL);
    component->delta_time_arg = lua_value_create_number("delta time arg", 0);
    component->map_arg = lua_value_create_number("map arg", 0);
    component->cell_arg = lua_value_create_number("cell arg", 0);

    // Map
    component->position = ese_point_create(engine);
    ese_point_ref(component->position);

    component->sprite_frames = NULL;
    component->show_layer = NULL;
    component->show_layer_count = 0;

    return &component->base;
}

EseEntityComponent *_entity_component_map_copy(const EseEntityComponentMap *src) {
    log_assert("ENTITY_COMP", src, "_entity_component_map_copy called with NULL src");

    EseEntityComponentMap *copy =
        memory_manager.malloc(sizeof(EseEntityComponentMap), MMTAG_COMP_MAP);
    copy->base.data = copy;
    copy->base.active = true;
    copy->base.id = ese_uuid_create(src->base.lua);
    copy->base.lua = src->base.lua;
    copy->base.lua_ref = LUA_NOREF;
    copy->base.type = ENTITY_COMPONENT_MAP;

    copy->map = src->map; // we dont own the map, the engine does
    copy->show_layer = NULL;
    copy->show_layer_count = ese_map_get_layer_count(src->map);
    if (src->show_layer) {
        copy->show_layer =
            memory_manager.malloc(sizeof(bool) * copy->show_layer_count, MMTAG_COMP_MAP);
        memcpy(copy->show_layer, src->show_layer, sizeof(bool) * copy->show_layer_count);
    }
    copy->position = ese_point_create(src->base.lua);
    ese_point_ref(copy->position);
    ese_point_set_x(copy->position, ese_point_get_x(src->position));
    ese_point_set_y(copy->position, ese_point_get_y(src->position));
    copy->size = src->size;
    copy->seed = src->seed;

    if (copy->map) {
        ese_map_ref(copy->map);
        size_t cells = ese_map_get_width(copy->map) * ese_map_get_height(copy->map);
        copy->sprite_frames = memory_manager.malloc(sizeof(int) * cells, MMTAG_COMP_MAP);
        memset(copy->sprite_frames, 0, sizeof(int) * cells);
    } else {
        copy->sprite_frames = NULL;
    }

    // Lua Script
    copy->script = memory_manager.strdup(src->script, MMTAG_COMP_MAP);
    copy->engine = src->engine;
    copy->instance_ref = LUA_NOREF;
    copy->function_cache = hashmap_create(NULL);
    copy->map_arg = lua_value_create_number("map arg", 0);
    copy->cell_arg = lua_value_create_number("cell arg", 0);

    return &copy->base;
}

void _entity_component_map_cleanup(EseEntityComponentMap *component) {
    // Unref map if present (we don't own it)
    if (component->map) {
        ese_map_unref(component->map);
        ese_map_remove_watcher(component->map, _entity_component_map_changed, component);
        component->map = NULL;
    }
    if (component->sprite_frames) {
        memory_manager.free(component->sprite_frames);
    }
    if (component->show_layer) {
        memory_manager.free(component->show_layer);
    }
    ese_uuid_destroy(component->base.id);
    ese_point_unref(component->position);
    ese_point_destroy(component->position);

    memory_manager.free(component->script);
    if (component->function_cache) {
        _entity_component_map_clear_cache(component);
        hashmap_destroy(component->function_cache);
        component->function_cache = NULL;
    }

    if (component->instance_ref != LUA_NOREF) {
        lua_engine_instance_remove(component->engine, component->instance_ref);
        component->instance_ref = LUA_NOREF;
    }

    lua_value_destroy(component->map_arg);
    lua_value_destroy(component->cell_arg);
    lua_value_destroy(component->delta_time_arg);

    memory_manager.free(component);
    profile_count_add("entity_comp_map_destroy_count");
}

void _entity_component_map_destroy(EseEntityComponentMap *component) {
    log_assert("ENTITY_COMP", component, "_entity_component_map_destroy called with NULL src");

    // Respect Lua registry ref-count; only free when no refs remain
    if (component->base.lua_ref != LUA_NOREF && component->base.lua_ref_count > 0) {
        component->base.lua_ref_count--;
        if (component->base.lua_ref_count == 0) {
            luaL_unref(component->base.lua->runtime, LUA_REGISTRYINDEX, component->base.lua_ref);
            component->base.lua_ref = LUA_NOREF;
            _entity_component_map_cleanup(component);
        } else {
            // We dont "own" the sprite so dont free it}
            return;
        }
    } else if (component->base.lua_ref == LUA_NOREF) {
        _entity_component_map_cleanup(component);
    }
}

cJSON *entity_component_map_serialize(const EseEntityComponentMap *component) {
    log_assert("ENTITY_COMP", component,
               "entity_component_map_serialize called with NULL component");

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        log_error("ENTITY_COMP", "Map serialize: failed to create JSON object");
        return NULL;
    }

    if (!cJSON_AddStringToObject(json, "type", "ENTITY_COMPONENT_MAP")) {
        log_error("ENTITY_COMP", "Map serialize: failed to add type");
        cJSON_Delete(json);
        return NULL;
    }

    if (!cJSON_AddBoolToObject(json, "active", component->base.active)) {
        log_error("ENTITY_COMP", "Map serialize: failed to add active");
        cJSON_Delete(json);
        return NULL;
    }

    if (component->script) {
        if (!cJSON_AddStringToObject(json, "script", component->script)) {
            log_error("ENTITY_COMP", "Map serialize: failed to add script");
            cJSON_Delete(json);
            return NULL;
        }
    } else {
        if (!cJSON_AddNullToObject(json, "script")) {
            log_error("ENTITY_COMP", "Map serialize: failed to add script null");
            cJSON_Delete(json);
            return NULL;
        }
    }

    if (!cJSON_AddNumberToObject(json, "size", (double)component->size)) {
        log_error("ENTITY_COMP", "Map serialize: failed to add size");
        cJSON_Delete(json);
        return NULL;
    }

    if (!cJSON_AddNumberToObject(json, "seed", (double)component->seed)) {
        log_error("ENTITY_COMP", "Map serialize: failed to add seed");
        cJSON_Delete(json);
        return NULL;
    }

    // Serialize position as embedded object { x, y }
    cJSON *pos = cJSON_CreateObject();
    if (!pos) {
        log_error("ENTITY_COMP", "Map serialize: failed to create position object");
        cJSON_Delete(json);
        return NULL;
    }
    if (!cJSON_AddNumberToObject(pos, "x", (double)ese_point_get_x(component->position)) ||
        !cJSON_AddNumberToObject(pos, "y", (double)ese_point_get_y(component->position)) ||
        !cJSON_AddItemToObject(json, "position", pos)) {
        log_error("ENTITY_COMP", "Map serialize: failed to add position");
        cJSON_Delete(pos);
        cJSON_Delete(json);
        return NULL;
    }

    // Serialize show_layer as an array of booleans
    cJSON *layers = cJSON_CreateArray();
    if (!layers) {
        log_error("ENTITY_COMP", "Map serialize: failed to create layers array");
        cJSON_Delete(json);
        return NULL;
    }
    for (size_t i = 0; i < component->show_layer_count; i++) {
        cJSON *val = cJSON_CreateBool(component->show_layer ? component->show_layer[i] : true);
        if (!val || !cJSON_AddItemToArray(layers, val)) {
            log_error("ENTITY_COMP", "Map serialize: failed to add layer value");
            cJSON_Delete(layers);
            cJSON_Delete(json);
            return NULL;
        }
    }
    if (!cJSON_AddItemToObject(json, "show_layer", layers)) {
        log_error("ENTITY_COMP", "Map serialize: failed to attach layers array");
        cJSON_Delete(layers);
        cJSON_Delete(json);
        return NULL;
    }

    return json;
}

EseEntityComponent *entity_component_map_deserialize(EseLuaEngine *engine,
                                                     const cJSON *data) {
    log_assert("ENTITY_COMP", engine,
               "entity_component_map_deserialize called with NULL engine");
    log_assert("ENTITY_COMP", data, "entity_component_map_deserialize called with NULL data");

    if (!cJSON_IsObject(data)) {
        log_error("ENTITY_COMP", "Map deserialize: data is not an object");
        return NULL;
    }

    const cJSON *type_item = cJSON_GetObjectItemCaseSensitive(data, "type");
    if (!cJSON_IsString(type_item) || strcmp(type_item->valuestring, "ENTITY_COMPONENT_MAP") != 0) {
        log_error("ENTITY_COMP", "Map deserialize: invalid or missing type");
        return NULL;
    }

    const cJSON *active_item = cJSON_GetObjectItemCaseSensitive(data, "active");
    if (!cJSON_IsBool(active_item)) {
        log_error("ENTITY_COMP", "Map deserialize: missing active field");
        return NULL;
    }

    const cJSON *script_item = cJSON_GetObjectItemCaseSensitive(data, "script");
    const char *script_name = NULL;
    if (cJSON_IsString(script_item)) {
        script_name = script_item->valuestring;
    }

    const cJSON *size_item = cJSON_GetObjectItemCaseSensitive(data, "size");
    if (!cJSON_IsNumber(size_item)) {
        log_error("ENTITY_COMP", "Map deserialize: missing size");
        return NULL;
    }

    const cJSON *seed_item = cJSON_GetObjectItemCaseSensitive(data, "seed");
    if (!cJSON_IsNumber(seed_item)) {
        log_error("ENTITY_COMP", "Map deserialize: missing seed");
        return NULL;
    }

    const cJSON *pos_item = cJSON_GetObjectItemCaseSensitive(data, "position");
    const cJSON *pos_x = pos_item ? cJSON_GetObjectItemCaseSensitive(pos_item, "x") : NULL;
    const cJSON *pos_y = pos_item ? cJSON_GetObjectItemCaseSensitive(pos_item, "y") : NULL;

    const cJSON *layers_item = cJSON_GetObjectItemCaseSensitive(data, "show_layer");

    EseEntityComponent *base = entity_component_map_create(engine);
    if (!base) {
        log_error("ENTITY_COMP", "Map deserialize: failed to create component");
        return NULL;
    }

    EseEntityComponentMap *map = (EseEntityComponentMap *)base->data;
    map->base.active = cJSON_IsTrue(active_item);

    // script
    if (map->script) {
        memory_manager.free(map->script);
        map->script = NULL;
    }
    if (script_name) {
        map->script = memory_manager.strdup(script_name, MMTAG_COMP_MAP);
    }

    map->size = (int)size_item->valuedouble;
    map->seed = (uint32_t)seed_item->valuedouble;

    if (pos_x && cJSON_IsNumber(pos_x) && pos_y && cJSON_IsNumber(pos_y)) {
        ese_point_set_x(map->position, (float)pos_x->valuedouble);
        ese_point_set_y(map->position, (float)pos_y->valuedouble);
    }

    // show_layer
    if (layers_item && cJSON_IsArray(layers_item)) {
        size_t count = (size_t)cJSON_GetArraySize(layers_item);
        if (map->show_layer) {
            memory_manager.free(map->show_layer);
            map->show_layer = NULL;
            map->show_layer_count = 0;
        }
        if (count > 0) {
            map->show_layer =
                memory_manager.malloc(sizeof(bool) * count, MMTAG_COMP_MAP);
            map->show_layer_count = count;
            for (size_t i = 0; i < count; i++) {
                const cJSON *item = cJSON_GetArrayItem(layers_item, (int)i);
                map->show_layer[i] = cJSON_IsBool(item) ? cJSON_IsTrue(item) : true;
            }
        }
    }

    return base;
}

void _entity_component_map_cache_functions(EseEntityComponentMap *component) {
    log_assert("ENTITY_COMP", component,
               "_entity_component_map_cache_functions called with NULL component");

    if (!component->engine || component->instance_ref == LUA_NOREF) {
        profile_count_add("entity_comp_map_cache_functions_no_engine_or_instance");
        return;
    }

    profile_start(PROFILE_ENTITY_COMP_MAP_FUNCTION_CACHE);

    lua_State *L = component->engine->runtime;

    // Clear existing cache first
    _entity_component_map_clear_cache(component);

    // Push the instance table onto the stack
    lua_rawgeti(L, LUA_REGISTRYINDEX, component->instance_ref);

    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        profile_cancel(PROFILE_ENTITY_COMP_MAP_FUNCTION_CACHE);
        profile_count_add("entity_comp_map_cache_functions_not_table");
        return;
    }

    // Cache each standard function
    for (size_t i = 0; i < STANDARD_FUNCTIONS_COUNT; ++i) {
        const char *func_name = STANDARD_FUNCTIONS[i];

        // Try to get the function from the instance table
        lua_getfield(L, -1, func_name);

        if (lua_isfunction(L, -1)) {
            // Function exists, cache the reference
            int ref = luaL_ref(L, LUA_REGISTRYINDEX);

            CachedLuaFunction *cached =
                memory_manager.malloc(sizeof(CachedLuaFunction), MMTAG_COMP_MAP);
            cached->function_ref = ref;
            cached->exists = true;
            hashmap_set(component->function_cache, func_name, cached);
        } else {
            // Function doesn't exist, cache as LUA_NOREF
            lua_pop(L, 1); // pop nil
            CachedLuaFunction *cached =
                memory_manager.malloc(sizeof(CachedLuaFunction), MMTAG_COMP_MAP);
            cached->function_ref = LUA_NOREF;
            cached->exists = false;
            hashmap_set(component->function_cache, func_name, cached);
        }
    }

    // Pop the instance table
    lua_pop(L, 1);

    profile_stop(PROFILE_ENTITY_COMP_MAP_FUNCTION_CACHE, "entity_comp_map_cache_functions");
    profile_count_add("entity_comp_map_cache_functions_success");
}

void _entity_component_map_clear_cache(EseEntityComponentMap *component) {
    log_assert("ENTITY_COMP", component,
               "_entity_component_map_clear_cache called with NULL component");

    if (!component->function_cache) {
        return;
    }

    // Iterate through cache and free all CachedLuaFunction entries
    EseHashMapIter *iter = hashmap_iter_create(component->function_cache);
    if (iter) {
        const char *key;
        void *value;

        while (hashmap_iter_next(iter, &key, &value)) {
            CachedLuaFunction *cached = (CachedLuaFunction *)value;

            // Unreference the function from Lua registry if it exists
            if (cached->exists && cached->function_ref != LUA_NOREF && component->engine) {
                luaL_unref(component->engine->runtime, LUA_REGISTRYINDEX, cached->function_ref);
            }

            // Free the cached function structure
            memory_manager.free(cached);
        }

        hashmap_iter_free(iter);
    }

    // Clear the hashmap
    hashmap_clear(component->function_cache);
}

/**
 * @brief Lua function to create a new EseEntityComponentMap object.
 *
 * @details Callable from Lua as EseEntityComponentMap.new().
 *
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new point object)
 *
 * @warning Items created in Lua are owned by Lua
 */
static int _entity_component_map_new(lua_State *L) {
    const char *collider_name = NULL;

    int n_args = lua_gettop(L);
    if (n_args != 0) {
        log_debug("ENTITY_COMP", "EntityComponentCollider.new()");
        lua_pushnil(L);
        return 1;
    }

    // Set engine reference
    EseLuaEngine *lua = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);

    // Create EseEntityComponent wrapper
    EseEntityComponent *component = _entity_component_map_make(lua);

    // For Lua-created components, create userdata without storing a persistent
    // ref
    EseEntityComponentMap **ud =
        (EseEntityComponentMap **)lua_newuserdata(L, sizeof(EseEntityComponentMap *));
    *ud = (EseEntityComponentMap *)component->data;

    luaL_getmetatable(L, ENTITY_COMPONENT_MAP_PROXY_META);
    lua_setmetatable(L, -2);

    return 1;
}

EseEntityComponentMap *_entity_component_map_get(lua_State *L, int idx) {
    // Check if it's userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }

    // Get the userdata and check metatable
    EseEntityComponentMap **ud =
        (EseEntityComponentMap **)luaL_testudata(L, idx, ENTITY_COMPONENT_MAP_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }

    return *ud;
}

/**
 * @brief Lua __index metamethod for EseEntityComponentMap objects (getter).
 *
 * @details Handles property access for EseEntityComponentMap objects from Lua.
 *
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack (1 for valid properties,
 * 0 otherwise)
 */
static int _entity_component_map_index(lua_State *L) {
    EseEntityComponentMap *component = _entity_component_map_get(L, 1);
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
    } else if (strcmp(key, "map") == 0) {
        if (component->map) {
            ese_map_lua_push(component->map);
        } else {
            lua_pushnil(L);
        }
        return 1;
    } else if (strcmp(key, "position") == 0) {
        ese_point_lua_push(component->position);
        return 1;
    } else if (strcmp(key, "size") == 0) {
        lua_pushnumber(L, component->size);
        return 1;
    } else if (strcmp(key, "seed") == 0) {
        lua_pushnumber(L, component->seed);
        return 1;
    } else if (strcmp(key, "script") == 0) {
        lua_pushstring(L, component->script ? component->script : "");
        return 1;
    } else if (strcmp(key, "toJSON") == 0) {
        lua_pushcfunction(L, _entity_component_map_tojson_lua);
        return 1;
    } else if (strcmp(key, "show_layer") == 0) {
        // Create a proxy table for component->show_layer
        lua_newtable(L);

        // metatable for proxy
        lua_newtable(L);

        // __index closure upvalue: component pointer
        lua_pushlightuserdata(L, component);
        lua_pushcclosure(L, _entity_component_map_show_layer_index, 1);
        lua_setfield(L, -2, "__index");

        // __newindex closure upvalue: component pointer
        lua_pushlightuserdata(L, component);
        lua_pushcclosure(L, _entity_component_map_show_layer_newindex, 1);
        lua_setfield(L, -2, "__newindex");

        // __len closure upvalue: component pointer
        lua_pushlightuserdata(L, component);
        lua_pushcclosure(L, _entity_component_map_show_layer_len, 1);
        lua_setfield(L, -2, "__len");

        // lock metatable
        lua_pushstring(L, "locked");
        lua_setfield(L, -2, "__metatable");

        // set metatable on proxy table
        lua_setmetatable(L, -2);

        return 1;
    } else if (strcmp(key, "show_all_layers") == 0) {
        // Return bound function: map_component.show_all_layers()
        lua_pushlightuserdata(L, component);
        lua_pushcclosure(L, _entity_component_map_show_all_layers, 1);
        return 1;
    }

    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseEntityComponentMap objects (setter).
 *
 * @details Handles property assignment for EseEntityComponentMap objects from
 * Lua.
 *
 * @param L Lua state pointer
 * @return Always returns 0 (no return values) or throws Lua error for invalid
 * operations
 */
static int _entity_component_map_newindex(lua_State *L) {
    EseEntityComponentMap *component = _entity_component_map_get(L, 1);
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
        lua_pushboolean(L, component->base.active);
        return 1;
    } else if (strcmp(key, "id") == 0) {
        return luaL_error(L, "id is read-only");
    } else if (strcmp(key, "map") == 0) {
        if (component->map) {
            ese_map_unref(component->map);
            ese_map_remove_watcher(component->map, _entity_component_map_changed, component);
        }

        component->map = ese_map_lua_get(L, 3);
        if (!component->map) {
            return luaL_error(L, "map must be a Map object");
        }

        ese_map_ref(component->map);
        if (component->show_layer_count) {
            memory_manager.free(component->show_layer);
        }
        component->show_layer_count = ese_map_get_layer_count(component->map);
        component->show_layer =
            memory_manager.malloc(sizeof(bool) * component->show_layer_count, MMTAG_COMP_MAP);
        memset(component->show_layer, true, sizeof(bool) * component->show_layer_count);
        ese_map_add_watcher(component->map, _entity_component_map_changed, component);

        if (component->sprite_frames) {
            memory_manager.free(component->sprite_frames);
        }

        size_t cells = ese_map_get_width(component->map) * ese_map_get_height(component->map);
        component->sprite_frames = memory_manager.malloc(sizeof(int) * cells, MMTAG_COMP_MAP);
        memset(component->sprite_frames, 0, sizeof(int) * cells);
        return 0;
    } else if (strcmp(key, "position") == 0) {
        EsePoint *new_position_point = ese_point_lua_get(L, 3);
        if (!new_position_point) {
            return luaL_error(L, "Entity position must be a EsePoint object");
        }
        // Copy values, don't copy reference (ownership safety)
        ese_point_set_x(component->position, ese_point_get_x(new_position_point));
        ese_point_set_y(component->position, ese_point_get_y(new_position_point));
        return 0;
    } else if (strcmp(key, "size") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "size must be a number");
        }

        int new_size = (int)lua_tonumber(L, 3);

        if (new_size < 0) {
            new_size = 0;
        }
        component->size = new_size;
        return 0;
    } else if (strcmp(key, "seed") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "seed must be a number");
        }

        uint32_t new_seed = (uint32_t)lua_tointeger(L, 3);

        if (new_seed < 0) {
            new_seed = 0;
        }
        component->seed = new_seed;
        return 0;
    } else if (strcmp(key, "script") == 0) {
        if (!lua_isstring(L, 3) && !lua_isnil(L, 3)) {
            return luaL_error(L, "script must be a string or nil");
        }

        if (component->instance_ref != LUA_NOREF) {
            lua_engine_instance_remove(component->engine, component->instance_ref);
            component->instance_ref = LUA_NOREF;
        }

        // Clear the function cache when script changes
        if (component->function_cache) {
            _entity_component_map_clear_cache(component);
        }

        if (component->script != NULL) {
            memory_manager.free(component->script);
            component->script = NULL;
        }

        if (lua_isstring(L, 3)) {
            const char *script = lua_tostring(L, 3);
            component->script = memory_manager.strdup(script, MMTAG_COMP_MAP);
        }

        return 0;
    }

    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

// show_layer proxy: __index
static int _entity_component_map_show_layer_index(lua_State *L) {
    EseEntityComponentMap *component = (EseEntityComponentMap *)lua_engine_instance_method_normalize(
        L, (EseLuaGetSelfFn)_entity_component_map_get, "EntityComponentMap");
    if (!component) {
        lua_pushnil(L);
        return 1;
    }
    if (!lua_isnumber(L, 1)) {
        lua_pushnil(L);
        return 1;
    }

    int index = (int)lua_tointeger(L, 1);
    if (index <= 0) {
        lua_pushnil(L);
        return 1;
    }
    size_t i = (size_t)(index - 1);

    if (!component->show_layer || i >= component->show_layer_count) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushboolean(L, component->show_layer[i]);
    return 1;
}

// show_layer proxy: __newindex
static int _entity_component_map_show_layer_newindex(lua_State *L) {
    EseEntityComponentMap *component = (EseEntityComponentMap *)lua_engine_instance_method_normalize(
        L, (EseLuaGetSelfFn)_entity_component_map_get, "EntityComponentMap");
    if (!component) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "show_layer index must be a number");
    }
    if (!lua_isboolean(L, 2)) {
        return luaL_error(L, "show_layer[index] must be a boolean");
    }

    int index = (int)lua_tointeger(L, 1);
    if (index <= 0) {
        return luaL_error(L, "show_layer index must be >= 1");
    }
    size_t i = (size_t)(index - 1);

    if (!component->show_layer || i >= component->show_layer_count) {
        return luaL_error(L, "show_layer index out of range (1 to %d)",
                          (int)component->show_layer_count);
    }

    component->show_layer[i] = lua_toboolean(L, 3);
    return 0;
}

// show_layer proxy: __len
static int _entity_component_map_show_layer_len(lua_State *L) {
    EseEntityComponentMap *component = (EseEntityComponentMap *)lua_engine_instance_method_normalize(
        L, (EseLuaGetSelfFn)_entity_component_map_get, "EntityComponentMap");
    int len = (component && component->show_layer) ? (int)component->show_layer_count : 0;
    lua_pushinteger(L, len);
    return 1;
}

// map_component.show_all_layers()
static int _entity_component_map_show_all_layers(lua_State *L) {
    EseEntityComponentMap *component = (EseEntityComponentMap *)lua_engine_instance_method_normalize(
        L, (EseLuaGetSelfFn)_entity_component_map_get, "EntityComponentMap");
    if (!component || !component->show_layer)
        return 0;
    for (size_t i = 0; i < component->show_layer_count; i++) {
        component->show_layer[i] = true;
    }
    return 0;
}

/**
 * @brief Lua __gc metamethod for EseEntityComponentMap objects.
 *
 * @param L Lua state pointer
 * @return Always 0 (no return values)
 */
static int _entity_component_map_gc(lua_State *L) {
    // Get from userdata
    EseEntityComponentMap **ud =
        (EseEntityComponentMap **)luaL_testudata(L, 1, ENTITY_COMPONENT_MAP_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }

    EseEntityComponentMap *component = *ud;
    if (component) {
        if (component->base.lua_ref == LUA_NOREF) {
            _entity_component_map_destroy(component);
            *ud = NULL;
        }
    }

    return 0;
}

static int _entity_component_map_tostring(lua_State *L) {
    EseEntityComponentMap *component = _entity_component_map_get(L, 1);

    if (!component) {
        lua_pushstring(L, "EseEntityComponentMap: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "EseEntityComponentMap: %p (id=%s active=%s ma[]=%p)",
             (void *)component, ese_uuid_get_value(component->base.id),
             component->base.active ? "true" : "false", component->map);
    lua_pushstring(L, buf);

    return 1;
}

void _entity_component_map_init(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "_entity_component_map_init called with NULL engine");

    // Create metatable
    lua_engine_new_object_meta(engine, ENTITY_COMPONENT_MAP_PROXY_META, _entity_component_map_index,
                               _entity_component_map_newindex, _entity_component_map_gc,
                               _entity_component_map_tostring);

    // Create global EntityComponentMap table with functions
    const char *keys[] = {"new"};
    lua_CFunction functions[] = {_entity_component_map_new};
    lua_engine_new_object(engine, "EntityComponentMap", 1, keys, functions);
}

bool _entity_component_map_collides_component(EseEntityComponentMap *component,
                                              EseEntityComponentCollider *collider,
                                              EseArray *out_hits) {
    log_assert("ENTITY_COMP_MAP", component,
               "_entity_component_map_collides_component called with NULL map");
    log_assert("ENTITY_COMP_MAP", collider,
               "_entity_component_map_collides_component called with NULL collider");
    log_assert("ENTITY_COMP_MAP", out_hits,
               "_entity_component_map_collides_component called with NULL out_hits");

    // If not set map, return false
    if (!component->map) {
        return false;
    }

    // If not set collision world bounds, return false
    EseRect *map_bounds = component->base.entity->collision_world_bounds;
    if (!map_bounds) {
        profile_count_add("map_collides_early_no_map_bounds");
        return false;
    }

    // If no rects, return false
    if (collider->rects_count == 0) {
        profile_count_add("map_collides_early_no_collider_rects");
        return false;
    }

    profile_start(PROFILE_ENTITY_COMP_MAP_COLLIDES);

    // High level bounds check
    EseRect *collider_bounds = collider->base.entity->collision_world_bounds;
    if (!collider_bounds || !ese_rect_intersects(map_bounds, collider_bounds)) {
        profile_cancel(PROFILE_ENTITY_COMP_MAP_COLLIDES);
        profile_count_add("map_collides_early_world_bounds_miss");
        return false;
    }

    // map size
    size_t mw = ese_map_get_width(component->map);
    size_t mh = ese_map_get_height(component->map);

    EseRect **world_rects =
        memory_manager.malloc(sizeof(EseRect *) * collider->rects_count, MMTAG_COMP_MAP);
    for (size_t i = 0; i < collider->rects_count; i++) {
        world_rects[i] = ese_rect_create(component->base.lua);
        ese_rect_set_x(world_rects[i], ese_rect_get_x(collider->rects[i]) +
                                           ese_point_get_x(collider->base.entity->position));
        ese_rect_set_y(world_rects[i], ese_rect_get_y(collider->rects[i]) +
                                           ese_point_get_y(collider->base.entity->position));
        ese_rect_set_width(world_rects[i], ese_rect_get_width(collider->rects[i]));
        ese_rect_set_height(world_rects[i], ese_rect_get_height(collider->rects[i]));
        ese_rect_set_rotation(world_rects[i], ese_rect_get_rotation(collider->rects[i]));
    }

    bool did_hit = false;
    for (size_t y = 0; y < mh; y++) {
        for (size_t x = 0; x < mw; x++) {
            profile_count_add("map_collides_cell_checked");
            // We don't own the cell, so we don't need to destroy it
            EseMapCell *cell = ese_map_get_cell(component->map, x, y);
            // cell_rect is in world coords
            EseRect *cell_rect = entity_component_map_get_cell_rect(component, x, y);

            bool intersect = false;
            for (size_t i = 0; i < collider->rects_count; i++) {
                if (ese_rect_intersects(world_rects[i], cell_rect)) {
                    intersect = true;
                    break;
                }
            }
            ese_rect_destroy(cell_rect);

            // Only count cells that are marked solid
            if (intersect) {
                profile_count_add("map_collides_solid_hits");
                // hit is owned by the caller's array
                EseCollisionHit *hit = ese_collision_hit_create(collider->base.entity->lua);
                ese_collision_hit_set_kind(hit, COLLISION_KIND_MAP);
                ese_collision_hit_set_entity(hit, collider->base.entity);
                ese_collision_hit_set_target(hit, component->base.entity);
                ese_collision_hit_set_state(hit, COLLISION_STATE_STAY);
                ese_collision_hit_set_map(hit, component->map);
                ese_collision_hit_set_cell_x(hit, x);
                ese_collision_hit_set_cell_y(hit, y);

                array_push(out_hits, hit);
                did_hit = true;
            }
        }
    }

    for (size_t i = 0; i < collider->rects_count; i++) {
        ese_rect_destroy(world_rects[i]);
    }
    memory_manager.free(world_rects);

    profile_stop(PROFILE_ENTITY_COMP_MAP_COLLIDES, "entity_comp_map_collides_comp");
    return did_hit;
}

EseEntityComponent *entity_component_map_create(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "entity_component_map_create called with NULL engine");

    EseEntityComponent *component = _entity_component_map_make(engine);

    // Register with Lua using ref system
    component->vtable->ref(component);

    return component;
}

static int _entity_component_map_tojson_lua(lua_State *L) {
    EseEntityComponentMap *self = _entity_component_map_get(L, 1);
    if (!self) {
        return luaL_error(L, "EntityComponentMap:toJSON() called on invalid component");
    }
    if (lua_gettop(L) != 1) {
        return luaL_error(L, "EntityComponentMap:toJSON() takes 0 arguments");
    }
    cJSON *json = entity_component_map_serialize(self);
    if (!json) {
        return luaL_error(L, "EntityComponentMap:toJSON() failed to serialize");
    }
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!json_str) {
        return luaL_error(L, "EntityComponentMap:toJSON() failed to stringify");
    }
    lua_pushstring(L, json_str);
    free(json_str);
    return 1;
}

static void _entity_component_map_changed(EseMap *map, void *userdata) {
    EseEntityComponentMap *component = (EseEntityComponentMap *)userdata;
    log_assert("ENTITY_COMP", component,
               "_entity_component_map_changed called with NULL component");

    size_t new_count = ese_map_get_layer_count(map);
    if (component->show_layer_count != new_count) {
        size_t old_count = component->show_layer_count;
        component->show_layer =
            memory_manager.realloc(component->show_layer, sizeof(bool) * new_count, MMTAG_COMP_MAP);
        if (new_count > old_count) {
            for (size_t i = old_count; i < new_count; i++) {
                component->show_layer[i] = true;
            }
        }
        component->show_layer_count = new_count;
    }
}

// ========================================
// Public helpers
// ========================================

EseRect *entity_component_map_get_cell_rect(EseEntityComponentMap *component, int x, int y) {
    log_assert("ENTITY_COMP_MAP", component,
               "entity_component_map_get_cell_rect called with NULL component");
    log_assert("ENTITY_COMP_MAP", component->map,
               "entity_component_map_get_cell_rect called with NULL map");

    EseLuaEngine *engine = component->base.lua;
    EseRect *rect = ese_rect_create(engine);

    switch (ese_map_get_type(component->map)) {
    case MAP_TYPE_GRID: {
        float map_x = ese_point_get_x(component->base.entity->position);
        float map_y = ese_point_get_y(component->base.entity->position);
        float rx = x * component->size + map_x;
        float ry = y * component->size + map_y;
        ese_rect_set_x(rect, rx);
        ese_rect_set_y(rect, ry);
        ese_rect_set_width(rect, (float)component->size);
        ese_rect_set_height(rect, (float)component->size);
        ese_rect_set_rotation(rect, 0.0f);
        break;
    }
    case MAP_TYPE_HEX_POINT_UP: {
        const int th = component->size;
        const int tw = (int)(th * 0.866025f);
        float cx = ese_point_get_x(component->position);
        float cy = ese_point_get_y(component->position);
        float rx = (x - cx) * tw;
        float ry = (y - cy) * (th * 0.75f);
        if ((y % 2) == 1) {
            rx += tw / 2.0f;
        }
        ese_rect_set_x(rect, rx);
        ese_rect_set_y(rect, ry);
        ese_rect_set_width(rect, (float)tw);
        ese_rect_set_height(rect, (float)th);
        ese_rect_set_rotation(rect, 0.0f);
        break;
    }
    case MAP_TYPE_HEX_FLAT_UP: {
        const int th = component->size;
        const int tw = (int)(th * 1.154701f);
        float cx = ese_point_get_x(component->position);
        float cy = ese_point_get_y(component->position);
        float rx = (x - cx) * (tw * 0.75f);
        float ry = (y - cy) * th;
        if ((x % 2) == 1) {
            ry += th / 2.0f;
        }
        ese_rect_set_x(rect, rx);
        ese_rect_set_y(rect, ry);
        ese_rect_set_width(rect, (float)tw);
        ese_rect_set_height(rect, (float)th);
        ese_rect_set_rotation(rect, 0.0f);
        break;
    }
    case MAP_TYPE_ISO: {
        const int th = component->size;
        const int tw = th * 2;
        float cx = ese_point_get_x(component->position);
        float cy = ese_point_get_y(component->position);
        float rx = (x - cx) * (tw / 2.0f) - (y - cy) * (tw / 2.0f);
        float ry = (x - cx) * (th / 2.0f) + (y - cy) * (th / 2.0f);
        ese_rect_set_x(rect, rx);
        ese_rect_set_y(rect, ry);
        ese_rect_set_width(rect, (float)tw);
        ese_rect_set_height(rect, (float)th);
        ese_rect_set_rotation(rect, 0.0f);
        break;
    }
    default: {
        // Unknown type; return zero rect
        ese_rect_set_x(rect, 0);
        ese_rect_set_y(rect, 0);
        ese_rect_set_width(rect, 0);
        ese_rect_set_height(rect, 0);
        ese_rect_set_rotation(rect, 0.0f);
        break;
    }
    }

    return rect;
}
