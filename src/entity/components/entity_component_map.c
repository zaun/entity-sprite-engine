#include <string.h>
#include <math.h>
#include "core/memory_manager.h"
#include "utility/array.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "scripting/lua_engine.h"
#include "entity/components/entity_component_collider.h"
#include "entity/components/entity_component_map.h"
#include "core/collision_resolver.h"
#include "entity/components/entity_component_private.h"
#include "core/asset_manager.h"
#include "core/engine_private.h"
#include "graphics/sprite.h"
#include "entity/entity_private.h"
#include "types/types.h"

// Standard entity function names
static const char *STANDARD_FUNCTIONS[] = {
    "map_init",
    "map_update", 
    "cell_update",
    "cell_enter",
    "cell_exit"
};
static const size_t STANDARD_FUNCTIONS_COUNT = sizeof(STANDARD_FUNCTIONS) / sizeof(STANDARD_FUNCTIONS[0]);


// Forward declarations
bool _entity_component_map_collides_component(EseEntityComponentMap *component, EseEntityComponentCollider *collider, EseArray *out_hits);
void _entity_component_map_cache_functions(EseEntityComponentMap *component);
void _entity_component_map_clear_cache(EseEntityComponentMap *component);
bool _entity_component_map_run(EseEntityComponentMap *component, EseEntity *entity, const char *func_name, int argc, EseLuaValue *argv[]);
static void _entity_component_map_changed(EseMap *map, void *userdata);
static int _entity_component_map_show_layer_index(lua_State *L);
static int _entity_component_map_show_layer_newindex(lua_State *L);
static int _entity_component_map_show_layer_len(lua_State *L);
static int _entity_component_map_show_all_layers(lua_State *L);
static void _entity_component_map_update_world_bounds(EseEntityComponentMap *component);

// VTable wrapper functions
static EseEntityComponent* _map_vtable_copy(EseEntityComponent* component) {
    return _entity_component_map_copy((EseEntityComponentMap*)component->data);
}

static void _map_vtable_destroy(EseEntityComponent* component) {
    _entity_component_map_destroy((EseEntityComponentMap*)component->data);
}

static void _map_vtable_update(EseEntityComponent* component, EseEntity* entity, float delta_time) {
    _entity_component_map_update((EseEntityComponentMap*)component->data, entity, delta_time);
}

static void _map_vtable_draw(EseEntityComponent* component, int screen_x, int screen_y, void* callbacks, void* user_data) {
    EntityDrawCallbacks* draw_callbacks = (EntityDrawCallbacks*)callbacks;
    _entity_component_map_draw((EseEntityComponentMap*)component->data, screen_x, screen_y, draw_callbacks->draw_texture, user_data);
}

static bool _map_vtable_run_function(EseEntityComponent* component, EseEntity* entity, const char* func_name, int argc, void* argv[]) {
    // Map components don't support function execution
    return false;
}

static void _map_vtable_collides_component(EseEntityComponent* a, EseEntityComponent* b, EseArray *out_hits) {
    _entity_component_map_collides_component((EseEntityComponentMap*)a->data, (EseEntityComponentCollider*)b->data, out_hits);
}

static void _map_vtable_ref(EseEntityComponent* component) {
    EseEntityComponentMap *map = (EseEntityComponentMap*)component->data;
    log_assert("ENTITY_COMP", map, "map vtable ref called with NULL");
    if (map->base.lua_ref == LUA_NOREF) {
        EseEntityComponentMap **ud = (EseEntityComponentMap **)lua_newuserdata(map->base.lua->runtime, sizeof(EseEntityComponentMap *));
        *ud = map;
        luaL_getmetatable(map->base.lua->runtime, ENTITY_COMPONENT_MAP_PROXY_META);
        lua_setmetatable(map->base.lua->runtime, -2);
        map->base.lua_ref = luaL_ref(map->base.lua->runtime, LUA_REGISTRYINDEX);
        map->base.lua_ref_count = 1;
    } else {
        map->base.lua_ref_count++;
    }
}

static void _map_vtable_unref(EseEntityComponent* component) {
    EseEntityComponentMap *map = (EseEntityComponentMap*)component->data;
    if (!map) return;
    if (map->base.lua_ref != LUA_NOREF && map->base.lua_ref_count > 0) {
        map->base.lua_ref_count--;
        if (map->base.lua_ref_count == 0) {
            luaL_unref(map->base.lua->runtime, LUA_REGISTRYINDEX, map->base.lua_ref);
            map->base.lua_ref = LUA_NOREF;
        }
    }
}

// Static vtable instance for map components
static const ComponentVTable map_vtable = {
    .copy = _map_vtable_copy,
    .destroy = _map_vtable_destroy,
    .update = _map_vtable_update,
    .draw = _map_vtable_draw,
    .run_function = _map_vtable_run_function,
    .collides = _map_vtable_collides_component,
    .ref = _map_vtable_ref,
    .unref = _map_vtable_unref
};

// callback


static EseEntityComponent *_entity_component_map_make(EseLuaEngine *engine)
{
    log_assert("ENTITY_COMP", engine, "_entity_component_map_make called with NULL engine");

    EseEntityComponentMap *component = memory_manager.malloc(sizeof(EseEntityComponentMap), MMTAG_COMP_MAP);
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

EseEntityComponent *_entity_component_map_copy(const EseEntityComponentMap *src)
{
    log_assert("ENTITY_COMP", src, "_entity_component_map_copy called with NULL src");

    EseEntityComponentMap *copy = memory_manager.malloc(sizeof(EseEntityComponentMap), MMTAG_COMP_MAP);
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
        copy->show_layer = memory_manager.malloc(sizeof(bool) * copy->show_layer_count, MMTAG_COMP_MAP);
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

void _entity_component_map_cleanup(EseEntityComponentMap *component)
{
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

void _entity_component_map_destroy(EseEntityComponentMap *component)
{
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

void _entity_component_map_update(EseEntityComponentMap *component, EseEntity *entity, float delta_time)
{
    log_assert("ENTITY_COMP", component, "_entity_component_map_update called with NULL component");
    log_assert("ENTITY_COMP", entity, "_entity_component_map_update called with NULL src");

    // Keep world bounds in sync so spatial index can pair map entities correctly
    _entity_component_map_update_world_bounds(component);

    // If not script, just return
    if (component->script == NULL) {
        profile_cancel(PROFILE_ENTITY_COMP_MAP_UPDATE);
        return;
    }

    // Check for 1st time running
    if (component->instance_ref == LUA_NOREF) {
        // Script instance creation timing
        component->instance_ref = lua_engine_instance_script(component->engine, component->script);
        
        if (component->instance_ref == LUA_NOREF) {
            profile_cancel(PROFILE_ENTITY_COMP_MAP_UPDATE);
            profile_count_add("entity_comp_map_update_instance_creation_failed");
            return;
        }

        // Function caching timing
        _entity_component_map_cache_functions(component);

        // Init function timing
        // Argument setup timing
        lua_value_set_map(component->map_arg, component->map);
        
        // Function execution timing
        EseLuaValue *args[] = {component->map_arg};
        _entity_component_map_run(component, entity, "map_init", 1, args);

        profile_count_add("entity_comp_map_update_first_time_setup");
    }

    // Argument setup timing
    lua_value_set_number(component->delta_time_arg, delta_time);
    lua_value_set_map(component->map_arg, component->map);
    
    // Function execution timing
    EseLuaValue *args[] = {component->delta_time_arg, component->map_arg};
    _entity_component_map_run(component, entity, "map_update", 2, args);
    
    profile_stop(PROFILE_ENTITY_COMP_MAP_UPDATE, "entity_comp_map_update");
    profile_count_add("entity_comp_map_update_success");
}

void _entity_component_map_cache_functions(EseEntityComponentMap *component) {
    log_assert("ENTITY_COMP", component, "_entity_component_map_cache_functions called with NULL component");
    
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
            
            CachedLuaFunction *cached = memory_manager.malloc(sizeof(CachedLuaFunction), MMTAG_COMP_MAP);
            cached->function_ref = ref;
            cached->exists = true;
            hashmap_set(component->function_cache, func_name, cached);
        } else {
            // Function doesn't exist, cache as LUA_NOREF
            lua_pop(L, 1); // pop nil
            CachedLuaFunction *cached = memory_manager.malloc(sizeof(CachedLuaFunction), MMTAG_COMP_MAP);
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
    log_assert("ENTITY_COMP", component, "_entity_component_map_clear_cache called with NULL component");
    
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

bool _entity_component_map_run(EseEntityComponentMap *component, EseEntity *entity, const char *func_name, int argc, EseLuaValue *argv[]) {
    log_assert("ENTITY_COMP", component, "_entity_component_map_run called with NULL component");
    log_assert("ENTITY_COMP", entity, "_entity_component_map_run called with NULL entity");
    log_assert("ENTITY_COMP", func_name, "_entity_component_map_run called with NULL func_name");
    
    profile_start(PROFILE_ENTITY_COMP_MAP_FUNCTION_RUN);
    
    if (!component->function_cache || !component->engine) {
        profile_cancel(PROFILE_ENTITY_COMP_MAP_FUNCTION_RUN);
        profile_count_add("entity_comp_map_run_no_cache_or_engine");
        return false;
    }
    
    // Function cache lookup timing
    CachedLuaFunction *cached = hashmap_get(component->function_cache, func_name);
    
    if (!cached) {
        // Function lookup and caching timing
        lua_State *L = component->engine->runtime;
        
        if (component->instance_ref == LUA_NOREF) {
            // Initialize the component if it hasn't been initialized yet
            if (component->script == NULL) {
                profile_cancel(PROFILE_ENTITY_COMP_MAP_FUNCTION_RUN);
                profile_count_add("entity_comp_map_run_no_script");
                return false;
            }
            
            // Script instance creation timing
            profile_start(PROFILE_ENTITY_COMP_MAP_INSTANCE_CREATE);
            component->instance_ref = lua_engine_instance_script(component->engine, component->script);
            profile_stop(PROFILE_ENTITY_COMP_MAP_INSTANCE_CREATE, "entity_comp_map_instance_create");
            
            if (component->instance_ref == LUA_NOREF) {
                profile_cancel(PROFILE_ENTITY_COMP_MAP_FUNCTION_RUN);
                profile_count_add("entity_comp_map_run_instance_creation_failed");
                return false;
            }
            
            if (strcmp(func_name, "entity_init") != 0) {
                _entity_component_map_run(component, entity, "entity_init", 0, NULL);
            }

            // Function caching timing
            profile_start(PROFILE_ENTITY_COMP_MAP_FUNCTION_CACHE);
            _entity_component_map_cache_functions(component);
            profile_stop(PROFILE_ENTITY_COMP_MAP_FUNCTION_CACHE, "entity_comp_map_function_cache");
        }
        
        // After potential pre-caching, check the cache again for this function
        cached = hashmap_get(component->function_cache, func_name);

        // If it is now cached (e.g., standard lifecycle function), skip manual lookup
        if (!cached) {
            // Registry access timing
            lua_rawgeti(L, LUA_REGISTRYINDEX, component->instance_ref);
            
            if (!lua_istable(L, -1)) {
                lua_pop(L, 1);
                profile_cancel(PROFILE_ENTITY_COMP_MAP_FUNCTION_RUN);
                profile_count_add("entity_comp_map_run_instance_not_table");
                return false;
            }
            
            // Function field lookup timing
            lua_getfield(L, -1, func_name);
            
            if (lua_isfunction(L, -1)) {
                // Function reference creation timing
                int ref = luaL_ref(L, LUA_REGISTRYINDEX);
                
                // Memory allocation timing
                cached = memory_manager.malloc(sizeof(CachedLuaFunction), MMTAG_COMP_MAP);
                cached->function_ref = ref;
                cached->exists = true;
                
                // Hashmap insertion timing
                hashmap_set(component->function_cache, func_name, cached);
            } else {
                // Function doesn't exist, cache as LUA_NOREF
                lua_pop(L, 1); // pop nil
                cached = memory_manager.malloc(sizeof(CachedLuaFunction), MMTAG_COMP_MAP);
                cached->function_ref = LUA_NOREF;
                cached->exists = false;
                hashmap_set(component->function_cache, func_name, cached);
            }
            
            // Pop the instance table
            lua_pop(L, 1);
        }
    }
    
    // If function doesn't exist, ignore
    if (!cached->exists) {
        profile_cancel(PROFILE_ENTITY_COMP_MAP_FUNCTION_RUN);
        profile_count_add("entity_comp_map_run_function_not_exists");
        return false;
    }
    
    // Engine function execution timing
    bool result = lua_engine_run_function_ref(component->engine, cached->function_ref, entity_get_lua_ref(entity), argc, argv, NULL);
    
    if (result) {
        profile_stop(PROFILE_ENTITY_COMP_MAP_FUNCTION_RUN, "entity_comp_map_function_run");
        profile_count_add("entity_comp_map_run_success");
    } else {
        profile_cancel(PROFILE_ENTITY_COMP_MAP_FUNCTION_RUN);
        profile_count_add("entity_comp_map_run_failed");
    }
    
    return result;
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
static int _entity_component_map_new(lua_State *L)
{
    const char *collider_name = NULL;

    int n_args = lua_gettop(L);
    if (n_args != 0)
    {
        log_debug("ENTITY_COMP", "EntityComponentCollider.new()");
        lua_pushnil(L);
        return 1;
    }

    // Set engine reference
    EseLuaEngine *lua = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);

    // Create EseEntityComponent wrapper
    EseEntityComponent *component = _entity_component_map_make(lua);

    // For Lua-created components, create userdata without storing a persistent ref
    EseEntityComponentMap **ud = (EseEntityComponentMap **)lua_newuserdata(L, sizeof(EseEntityComponentMap *));
    *ud = (EseEntityComponentMap *)component->data;

    luaL_getmetatable(L, ENTITY_COMPONENT_MAP_PROXY_META);
    lua_setmetatable(L, -2);

    return 1;
}

EseEntityComponentMap *_entity_component_map_get(lua_State *L, int idx)
{
    // Check if it's userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }

    // Get the userdata and check metatable
    EseEntityComponentMap **ud = (EseEntityComponentMap **)luaL_testudata(L, idx, ENTITY_COMPONENT_MAP_PROXY_META);
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
 * @return Number of return values pushed to Lua stack (1 for valid properties, 0 otherwise)
 */
static int _entity_component_map_index(lua_State *L)
{
    EseEntityComponentMap *component = _entity_component_map_get(L, 1);
    const char *key = lua_tostring(L, 2);

    // SAFETY: Return nil for freed components
    if (!component)
    {
        lua_pushnil(L);
        return 1;
    }

    if (!key)
        return 0;

    if (strcmp(key, "active") == 0)
    {
        lua_pushboolean(L, component->base.active);
        return 1;
    }
    else if (strcmp(key, "id") == 0)
    {
        lua_pushstring(L, ese_uuid_get_value(component->base.id));
        return 1;
    }
    else if (strcmp(key, "map") == 0)
    {
        if (component->map)
        {
            ese_map_lua_push(component->map);
        }
        else
        {
            lua_pushnil(L);
        }
        return 1;
    }
    else if (strcmp(key, "position") == 0)
    {
        ese_point_lua_push(component->position);
        return 1;
    }
    else if (strcmp(key, "size") == 0)
    {
        lua_pushnumber(L, component->size);
        return 1;
    }
    else if (strcmp(key, "seed") == 0)
    {
        lua_pushnumber(L, component->seed);
        return 1;
    } else if (strcmp(key, "script") == 0) {
        lua_pushstring(L, component->script ? component->script : "");
        return 1;
    }
    else if (strcmp(key, "show_layer") == 0)
    {
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
    }
    else if (strcmp(key, "show_all_layers") == 0)
    {
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
 * @details Handles property assignment for EseEntityComponentMap objects from Lua.
 *
 * @param L Lua state pointer
 * @return Always returns 0 (no return values) or throws Lua error for invalid operations
 */
static int _entity_component_map_newindex(lua_State *L)
{
    EseEntityComponentMap *component = _entity_component_map_get(L, 1);
    const char *key = lua_tostring(L, 2);

    // SAFETY: Silently ignore writes to freed components
    if (!component)
    {
        return 0;
    }

    if (!key)
        return 0;

    if (strcmp(key, "active") == 0)
    {
        if (!lua_isboolean(L, 3))
        {
            return luaL_error(L, "active must be a boolean");
        }
        component->base.active = lua_toboolean(L, 3);
        lua_pushboolean(L, component->base.active);
        return 1;
    }
    else if (strcmp(key, "id") == 0)
    {
        return luaL_error(L, "id is read-only");
    }
    else if (strcmp(key, "map") == 0)
    {
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
        component->show_layer = memory_manager.malloc(sizeof(bool) * component->show_layer_count, MMTAG_COMP_MAP);
        memset(component->show_layer, true, sizeof(bool) * component->show_layer_count);
        ese_map_add_watcher(component->map, _entity_component_map_changed, component);

        if (component->sprite_frames) {
            memory_manager.free(component->sprite_frames);
        }

        size_t cells = ese_map_get_width(component->map) * ese_map_get_height(component->map);
        component->sprite_frames = memory_manager.malloc(sizeof(int) * cells, MMTAG_COMP_MAP);
        memset(component->sprite_frames, 0, sizeof(int) * cells);

        // Update world bounds to cover full map
        _entity_component_map_update_world_bounds(component);
        return 0;
    }
    else if (strcmp(key, "position") == 0)
    {
        EsePoint *new_position_point = ese_point_lua_get(L, 3);
        if (!new_position_point)
        {
            return luaL_error(L, "Entity position must be a EsePoint object");
        }
        // Copy values, don't copy reference (ownership safety)
        ese_point_set_x(component->position, ese_point_get_x(new_position_point));
        ese_point_set_y(component->position, ese_point_get_y(new_position_point));
        // Changing center cell affects map extents relative to entity
        _entity_component_map_update_world_bounds(component);
        return 0;
    }
    else if (strcmp(key, "size") == 0)
    {
        if (!lua_isnumber(L, 3))
        {
            return luaL_error(L, "size must be a number");
        }

        int new_size = (int)lua_tonumber(L, 3);

        if (new_size < 0) {
            new_size = 0;
        }
        component->size = new_size;
        // Tile size changes the map's pixel extents
        _entity_component_map_update_world_bounds(component);
        return 0;
    }
    else if (strcmp(key, "seed") == 0)
    {
        if (!lua_isnumber(L, 3))
        {
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
static int _entity_component_map_show_layer_index(lua_State *L)
{
    EseEntityComponentMap *component = (EseEntityComponentMap *)lua_touserdata(L, lua_upvalueindex(1));
    if (!component) { lua_pushnil(L); return 1; }
    if (!lua_isnumber(L, 2)) { lua_pushnil(L); return 1; }

    int index = (int)lua_tointeger(L, 2);
    if (index <= 0) { lua_pushnil(L); return 1; }
    size_t i = (size_t)(index - 1);

    if (!component->show_layer || i >= component->show_layer_count) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushboolean(L, component->show_layer[i]);
    return 1;
}

// show_layer proxy: __newindex
static int _entity_component_map_show_layer_newindex(lua_State *L)
{
    EseEntityComponentMap *component = (EseEntityComponentMap *)lua_touserdata(L, lua_upvalueindex(1));
    if (!component) { return 0; }

    if (!lua_isnumber(L, 2)) {
        return luaL_error(L, "show_layer index must be a number");
    }
    if (!lua_isboolean(L, 3)) {
        return luaL_error(L, "show_layer[index] must be a boolean");
    }

    int index = (int)lua_tointeger(L, 2);
    if (index <= 0) {
        return luaL_error(L, "show_layer index must be >= 1");
    }
    size_t i = (size_t)(index - 1);

    if (!component->show_layer || i >= component->show_layer_count) {
        return luaL_error(L, "show_layer index out of range (1 to %d)", (int)component->show_layer_count);
    }

    component->show_layer[i] = lua_toboolean(L, 3);
    return 0;
}

// show_layer proxy: __len
static int _entity_component_map_show_layer_len(lua_State *L)
{
    EseEntityComponentMap *component = (EseEntityComponentMap *)lua_touserdata(L, lua_upvalueindex(1));
    int len = (component && component->show_layer) ? (int)component->show_layer_count : 0;
    lua_pushinteger(L, len);
    return 1;
}

// map_component.show_all_layers()
static int _entity_component_map_show_all_layers(lua_State *L)
{
    EseEntityComponentMap *component = (EseEntityComponentMap *)lua_touserdata(L, lua_upvalueindex(1));
    if (!component || !component->show_layer) return 0;
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
static int _entity_component_map_gc(lua_State *L)
{
    // Get from userdata
    EseEntityComponentMap **ud = (EseEntityComponentMap **)luaL_testudata(L, 1, ENTITY_COMPONENT_MAP_PROXY_META);
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

static int _entity_component_map_tostring(lua_State *L)
{
    EseEntityComponentMap *component = _entity_component_map_get(L, 1);

    if (!component)
    {
        lua_pushstring(L, "EseEntityComponentMap: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "EseEntityComponentMap: %p (id=%s active=%s ma[]=%p)",
             (void *)component,
             ese_uuid_get_value(component->base.id),
             component->base.active ? "true" : "false",
             component->map);
    lua_pushstring(L, buf);

    return 1;
}

void _entity_component_map_init(EseLuaEngine *engine)
{
    log_assert("ENTITY_COMP", engine, "_entity_component_map_init called with NULL engine");

    lua_State *L = engine->runtime;

    // Register EntityComponentMap metatable
    if (luaL_newmetatable(L, ENTITY_COMPONENT_MAP_PROXY_META))
    {
        log_debug("LUA", "Adding %s to engine", ENTITY_COMPONENT_MAP_PROXY_META);
        lua_pushstring(L, ENTITY_COMPONENT_MAP_PROXY_META);
        lua_setfield(L, -2, "__name");
        lua_pushcfunction(L, _entity_component_map_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, _entity_component_map_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, _entity_component_map_gc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, _entity_component_map_tostring);
        lua_setfield(L, -2, "__tostring");
        lua_pushstring(L, "locked");
        lua_setfield(L, -2, "__metatable");
    }
    lua_pop(L, 1);

    // Create global EntityComponentMap table with constructor
    lua_getglobal(L, "EntityComponentMap");
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        log_debug("LUA", "Creating global EntityComponentMap table");
        lua_newtable(L);
        lua_pushcfunction(L, _entity_component_map_new);
        lua_setfield(L, -2, "new");
        lua_setglobal(L, "EntityComponentMap");
    }
    else
    {
        lua_pop(L, 1);
    }
}

bool _entity_component_map_collides_component(EseEntityComponentMap *component, EseEntityComponentCollider *collider, EseArray *out_hits) {
    log_assert("ENTITY_COMP_MAP", component, "_entity_component_map_collides_component called with NULL map");
    log_assert("ENTITY_COMP_MAP", collider, "_entity_component_map_collides_component called with NULL collider");
    log_assert("ENTITY_COMP_MAP", out_hits, "_entity_component_map_collides_component called with NULL out_hits");

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
    
    EseRect **world_rects = memory_manager.malloc(sizeof(EseRect*) * collider->rects_count, MMTAG_COMP_MAP);
    for (size_t i = 0; i < collider->rects_count; i++) {
        world_rects[i] = ese_rect_create(component->base.lua);
        ese_rect_set_x(
            world_rects[i],
            ese_rect_get_x(collider->rects[i]) +
            ese_point_get_x(collider->base.entity->position)
        );
        ese_rect_set_y(world_rects[i],
                       ese_rect_get_y(collider->rects[i]) +
                       ese_point_get_y(collider->base.entity->position));
        ese_rect_set_width(world_rects[i],
                           ese_rect_get_width(collider->rects[i]));
        ese_rect_set_height(world_rects[i],
                            ese_rect_get_height(collider->rects[i]));
        ese_rect_set_rotation(world_rects[i],
                              ese_rect_get_rotation(collider->rects[i]));
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
            if (intersect ) {
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

void _entity_component_map_draw_grid(
    EseEntityComponentMap *component,
    float screen_x, float screen_y,
    EntityDrawTextureCallback texCallback,
    void *callback_user_data)
{
    log_assert("ENTITY_COMP_MAP",
        (component->base.entity->draw_order & (DRAW_ORDER_SCALE - 1ULL)) == 0,
        "draw_order must be a multiple of %llu", (unsigned long long)DRAW_ORDER_SCALE); 

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(component->base.lua->runtime, ENGINE_KEY);
    ese_tileset_set_seed(ese_map_get_tileset(component->map), component->seed);

    // tile display size
    const int tw = component->size, th = component->size;

    // center cell
    float cx = ese_point_get_x(component->position);
    float cy = ese_point_get_y(component->position);

    // map size
    int mw = ese_map_get_width(component->map);
    int mh = ese_map_get_height(component->map);

    for (uint32_t y = 0; y < mh; y++)
    {
        for (uint32_t x = 0; x < mw; x++)
        {
            EseMapCell *cell = ese_map_get_cell(component->map, x, y);
            float dx = screen_x + (x - cx) * tw;
            float dy = screen_y + (y - cy) * th;

            for (size_t i = 0; i < ese_map_cell_get_layer_count(cell); i++)
            {
                int tid = ese_map_cell_get_layer(cell, i);
                if (tid == -1 || !component->show_layer[i]) {
                    continue;
                }

                const char *sprite_id = ese_tileset_get_sprite(ese_map_get_tileset(component->map), tid);
                if (!sprite_id)
                {
                    continue;
                }

                EseSprite *sprite = engine_get_sprite(engine, sprite_id);
                if (!sprite) {
                    continue;
                }

                uint64_t z_index = component->base.entity->draw_order;
                z_index += ((uint64_t)(i * 2) << DRAW_ORDER_SHIFT);
                z_index += y * mw + x;

                const char *texture_id;
                float x1, y1, x2, y2;
                int w, h;
                sprite_get_frame(
                    sprite, component->sprite_frames[y * mw + x],
                    &texture_id, &x1, &y1, &x2, &y2, &w, &h
                );

                texCallback(
                    dx, dy, tw, th,
                    z_index,
                    texture_id, x1, y1, x2, y2, w, h,
                    callback_user_data);
            }
        }
    }
}

void _entity_component_map_draw_hex_point_up(
    EseEntityComponentMap *component,
    float screen_x, float screen_y,
    EntityDrawTextureCallback texCallback,
    void *callback_user_data)
{
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(component->base.lua->runtime, ENGINE_KEY);
    ese_tileset_set_seed(ese_map_get_tileset(component->map), component->seed);

    // For hex point up: width = height * sqrt(3) / 2
    const int th = component->size;
    const int tw = (int)(th * 0.866025f); // sqrt(3) / 2 ≈ 0.866025

    // center cell
    float cx = ese_point_get_x(component->position);
    float cy = ese_point_get_y(component->position);

    // map size
    int mw = ese_map_get_width(component->map);
    int mh = ese_map_get_height(component->map);

    for (uint32_t y = 0; y < mh; y++)
    {
        for (uint32_t x = 0; x < mw; x++)
        {
            EseMapCell *cell = ese_map_get_cell(component->map, x, y);
            
            // Hex point up positioning: offset every other row
            float dx = screen_x + (x - cx) * tw;
            float dy = screen_y + (y - cy) * (th * 0.75f); // 3/4 of height for vertical spacing
            
            // Offset odd rows by half width
            if (y % 2 == 1) {
                dx += tw / 2.0f;
            }

            for (size_t i = 0; i < ese_map_cell_get_layer_count(cell); i++)
            {
                int tid = ese_map_cell_get_layer(cell, i);
                if (tid == -1 || !component->show_layer[i]) {
                    continue;
                }

                const char *sprite_id = ese_tileset_get_sprite(ese_map_get_tileset(component->map), tid);
                if (!sprite_id)
                {
                    continue;
                }

                EseSprite *sprite = engine_get_sprite(engine, sprite_id);
                if (!sprite) {
                    continue;
                }

                int z_index = component->base.entity->draw_order;
                z_index += y * mw;
                z_index += x;

                const char *texture_id;
                float x1, y1, x2, y2;
                int w, h;
                sprite_get_frame(
                    sprite, component->sprite_frames[y * mw + x],
                    &texture_id, &x1, &y1, &x2, &y2, &w, &h
                );

                texCallback(
                    dx, dy, tw, th,
                    z_index,
                    texture_id, x1, y1, x2, y2, w, h,
                    callback_user_data);
            }
        }
    }
}

void _entity_component_map_draw_hex_flat_up(
    EseEntityComponentMap *component,
    float screen_x, float screen_y,
    EntityDrawTextureCallback texCallback,
    void *callback_user_data)
{
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(component->base.lua->runtime, ENGINE_KEY);
    ese_tileset_set_seed(ese_map_get_tileset(component->map), component->seed);

    // For hex flat up: width = height * 2 / sqrt(3)
    const int th = component->size;
    const int tw = (int)(th * 1.154701f); // 2 / sqrt(3) ≈ 1.154701

    // center cell
    float cx = ese_point_get_x(component->position);
    float cy = ese_point_get_y(component->position);

    // map size
    int mw = ese_map_get_width(component->map);
    int mh = ese_map_get_height(component->map);

    for (uint32_t y = 0; y < mh; y++)
    {
        for (uint32_t x = 0; x < mw; x++)
        {
            EseMapCell *cell = ese_map_get_cell(component->map, x, y);
            
            // Hex flat up positioning: offset every other column
            float dx = screen_x + (x - cx) * (tw * 0.75f); // 3/4 of width for horizontal spacing
            float dy = screen_y + (y - cy) * th;
            
            // Offset odd columns by half height
            if (x % 2 == 1) {
                dy += th / 2.0f;
            }

            for (size_t i = 0; i < ese_map_cell_get_layer_count(cell); i++)
            {
                int tid = ese_map_cell_get_layer(cell, i);
                if (tid == -1 || !component->show_layer[i]) {
                    continue;
                }

                const char *sprite_id = ese_tileset_get_sprite(ese_map_get_tileset(component->map), tid);
                if (!sprite_id)
                {
                    continue;
                }

                EseSprite *sprite = engine_get_sprite(engine, sprite_id);
                if (!sprite) {
                    continue;
                }

                int z_index = component->base.entity->draw_order;
                z_index += y * mw;
                z_index += x;

                const char *texture_id;
                float x1, y1, x2, y2;
                int w, h;
                sprite_get_frame(
                    sprite, component->sprite_frames[y * mw + x],
                    &texture_id, &x1, &y1, &x2, &y2, &w, &h
                );

                texCallback(
                    dx, dy, tw, th,
                    z_index,
                    texture_id, x1, y1, x2, y2, w, h,
                    callback_user_data);
            }
        }
    }
}

void _entity_component_map_draw_iso(
    EseEntityComponentMap *component,
    float screen_x, float screen_y,
    EntityDrawTextureCallback texCallback,
    void *callback_user_data)
{
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(component->base.lua->runtime, ENGINE_KEY);
    ese_tileset_set_seed(ese_map_get_tileset(component->map), component->seed);

    // For isometric: width = height * 2 (standard 2:1 isometric ratio)
    const int th = component->size;
    const int tw = th * 2;

    // center cell
    float cx = ese_point_get_x(component->position);
    float cy = ese_point_get_y(component->position);

    // map size
    int mw = ese_map_get_width(component->map);
    int mh = ese_map_get_height(component->map);

    for (uint32_t y = 0; y < mh; y++)
    {
        for (uint32_t x = 0; x < mw; x++)
        {
            EseMapCell *cell = ese_map_get_cell(component->map, x, y);
            
            // Isometric positioning: diamond-shaped grid
            float dx = screen_x + (x - cx) * (tw / 2.0f) - (y - cy) * (tw / 2.0f);
            float dy = screen_y + (x - cx) * (th / 2.0f) + (y - cy) * (th / 2.0f);

            for (size_t i = 0; i < ese_map_cell_get_layer_count(cell); i++)
            {
                int tid = ese_map_cell_get_layer(cell, i);
                if (tid == -1 || !component->show_layer[i]) {
                    continue;
                }

                const char *sprite_id = ese_tileset_get_sprite(ese_map_get_tileset(component->map), tid);
                if (!sprite_id)
                {
                    continue;
                }

                EseSprite *sprite = engine_get_sprite(engine, sprite_id);
                if (!sprite) {
                    continue;
                }

                int z_index = component->base.entity->draw_order;
                z_index += y * mw;
                z_index += x;

                const char *texture_id;
                float x1, y1, x2, y2;
                int w, h;
                sprite_get_frame(
                    sprite, component->sprite_frames[y * mw + x],
                    &texture_id, &x1, &y1, &x2, &y2, &w, &h
                );

                texCallback(
                    dx, dy, tw, th,
                    z_index,
                    texture_id, x1, y1, x2, y2, w, h,
                    callback_user_data);
            }
        }
    }
}

void _entity_component_map_draw(
    EseEntityComponentMap *component,
    float screen_x, float screen_y,
    EntityDrawTextureCallback texCallback,
    void *callback_user_data)
{
    if (!component->map) {
        log_debug("ENTITY_COMP_MAP", "map not set");
        return;
    }

    switch(ese_map_get_type(component->map))
    {
        case MAP_TYPE_GRID:
            _entity_component_map_draw_grid(component, screen_x, screen_y, texCallback, callback_user_data);
            break;
        case MAP_TYPE_HEX_POINT_UP:
            _entity_component_map_draw_hex_point_up(component, screen_x, screen_y, texCallback, callback_user_data);
            break;
        case MAP_TYPE_HEX_FLAT_UP:
            _entity_component_map_draw_hex_flat_up(component, screen_x, screen_y, texCallback, callback_user_data);
            break;
        case MAP_TYPE_ISO:
            _entity_component_map_draw_iso(component, screen_x, screen_y, texCallback, callback_user_data);
            break;
        default:
            log_debug("ENTITY_COMP_MAP", "map type not supported");
            break;
    }
}

EseEntityComponent *entity_component_map_create(EseLuaEngine *engine)
{
    log_assert("ENTITY_COMP", engine, "entity_component_map_create called with NULL engine");

    EseEntityComponent *component = _entity_component_map_make(engine);

    // Register with Lua using ref system
    component->vtable->ref(component);

    return component;
}

static void _entity_component_map_changed(EseMap *map, void *userdata)
{
	EseEntityComponentMap *component = (EseEntityComponentMap *)userdata;
	log_assert("ENTITY_COMP", component, "_entity_component_map_changed called with NULL component");

    size_t new_count = ese_map_get_layer_count(map);
    if (component->show_layer_count != new_count) {
        size_t old_count = component->show_layer_count;
        component->show_layer = memory_manager.realloc(component->show_layer, sizeof(bool) * new_count, MMTAG_COMP_MAP);
        if (new_count > old_count) {
            for (size_t i = old_count; i < new_count; i++) {
                component->show_layer[i] = true;
            }
        }
        component->show_layer_count = new_count;

    // Map structure changed; refresh world bounds
    _entity_component_map_update_world_bounds(component);
    }
}

// ========================================
// Public helpers
// ========================================

EseRect *entity_component_map_get_cell_rect(EseEntityComponentMap *component, int x, int y)
{
    log_assert("ENTITY_COMP_MAP", component, "entity_component_map_get_cell_rect called with NULL component");
    log_assert("ENTITY_COMP_MAP", component->map, "entity_component_map_get_cell_rect called with NULL map");

    EseLuaEngine *engine = component->base.lua;
    EseRect *rect = ese_rect_create(engine);

    switch (ese_map_get_type(component->map))
    {
        case MAP_TYPE_GRID:
        {
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
        case MAP_TYPE_HEX_POINT_UP:
        {
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
        case MAP_TYPE_HEX_FLAT_UP:
        {
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
        case MAP_TYPE_ISO:
        {
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
        default:
        {
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

// ========================================
// Private helpers
// ========================================

static void _entity_component_map_update_world_bounds(EseEntityComponentMap *component)
{
    log_assert("ENTITY_COMP_MAP", component, "_entity_component_map_update_world_bounds called with NULL component");

    // If not attached yet, or no map set, clear bounds
    if (!component->base.entity || !component->map) {
        if (component->base.entity && component->base.entity->collision_world_bounds) {
            ese_rect_destroy(component->base.entity->collision_world_bounds);
            component->base.entity->collision_world_bounds = NULL;
        }
        return;
    }

    if (!component->base.entity->collision_world_bounds) {
        component->base.entity->collision_world_bounds = ese_rect_create(component->base.lua);
    }

    EseRect *world_bounds = component->base.entity->collision_world_bounds;
    int mw = ese_map_get_width(component->map);
    int mh = ese_map_get_height(component->map);
    float px = ese_point_get_x(component->base.entity->position);
    float py = ese_point_get_y(component->base.entity->position);
    
    // Calculate bounds only for solid cells, not the entire map
    float min_x = INFINITY, min_y = INFINITY, max_x = -INFINITY, max_y = -INFINITY;
    bool has_solid_cells = false;
    
    for (int y = 0; y < mh; y++) {
        for (int x = 0; x < mw; x++) {
            EseMapCell *cell = ese_map_get_cell(component->map, x, y);
            if (cell && (ese_map_cell_get_flags(cell) & MAP_CELL_FLAG_SOLID)) {
                float cell_x = px + (float)x * (float)component->size;
                float cell_y = py + (float)y * (float)component->size;
                min_x = fminf(min_x, cell_x);
                min_y = fminf(min_y, cell_y);
                max_x = fmaxf(max_x, cell_x + (float)component->size);
                max_y = fmaxf(max_y, cell_y + (float)component->size);
                has_solid_cells = true;
            }
        }
    }
    
    if (has_solid_cells) {
        ese_rect_set_x(world_bounds, min_x);
        ese_rect_set_y(world_bounds, min_y);
        ese_rect_set_width(world_bounds, max_x - min_x);
        ese_rect_set_height(world_bounds, max_y - min_y);
        ese_rect_set_rotation(world_bounds, 0.0f);
    } else {
        // No solid cells - set bounds to zero size
        ese_rect_set_x(world_bounds, px);
        ese_rect_set_y(world_bounds, py);
        ese_rect_set_width(world_bounds, 0.0f);
        ese_rect_set_height(world_bounds, 0.0f);
        ese_rect_set_rotation(world_bounds, 0.0f);
    }
}
