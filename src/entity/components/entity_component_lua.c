#include <string.h>
#include "utility/log.h"
#include "utility/hashmap.h"
#include "utility/profile.h"
#include "core/memory_manager.h"
#include "platform/time.h"
#include "scripting/lua_engine.h"
#include "entity/entity.h"
#include "types/types.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component.h"
#include "entity/components/entity_component_lua.h"

// Standard entity function names
static const char *STANDARD_FUNCTIONS[] = {
    "entity_init",
    "entity_update", 
    "entity_collision_enter",
    "entity_collision_stay",
    "entity_collision_exit"
};
static const size_t STANDARD_FUNCTIONS_COUNT = sizeof(STANDARD_FUNCTIONS) / sizeof(STANDARD_FUNCTIONS[0]);

// VTable wrapper functions
static EseEntityComponent* _lua_vtable_copy(EseEntityComponent* component) {
    return _entity_component_lua_copy((EseEntityComponentLua*)component->data);
}

static void _lua_vtable_destroy(EseEntityComponent* component) {
    _entity_component_lua_destroy((EseEntityComponentLua*)component->data);
}

static void _lua_vtable_update(EseEntityComponent* component, EseEntity* entity, float delta_time) {
    _entity_component_lua_update((EseEntityComponentLua*)component->data, entity, delta_time);
}

static void _lua_vtable_draw(EseEntityComponent* component, int screen_x, int screen_y, void* callbacks, void* user_data) {
    // Lua components don't have draw functionality
}

static bool _lua_vtable_run_function(EseEntityComponent* component, EseEntity* entity, const char* func_name, int argc, void* argv[]) {
    return entity_component_lua_run((EseEntityComponentLua*)component->data, entity, func_name, argc, (EseLuaValue**)argv);
}

// Static vtable instance for lua components
static const ComponentVTable lua_vtable = {
    .copy = _lua_vtable_copy,
    .destroy = _lua_vtable_destroy,
    .update = _lua_vtable_update,
    .draw = _lua_vtable_draw,
    .run_function = _lua_vtable_run_function
};

static void _entity_component_lua_register(EseEntityComponentLua *component, bool is_lua_owned) {
    log_assert("ENTITY_COMP", component, "_entity_component_lua_register called with NULL component");
    log_assert("ENTITY_COMP", component->base.lua_ref == LUA_NOREF, "_entity_component_lua_register component is already registered");

    lua_newtable(component->base.lua->runtime);
    lua_pushlightuserdata(component->base.lua->runtime, component);
    lua_setfield(component->base.lua->runtime, -2, "__ptr");

    // Store the ownership flag
    lua_pushboolean(component->base.lua->runtime, is_lua_owned);
    lua_setfield(component->base.lua->runtime, -2, "__is_lua_owned");

    luaL_getmetatable(component->base.lua->runtime, ENTITY_COMPONENT_LUA_PROXY_META);
    lua_setmetatable(component->base.lua->runtime, -2);

    // Store a reference to this proxy table in the Lua registry
    component->base.lua_ref = luaL_ref(component->base.lua->runtime, LUA_REGISTRYINDEX);
    
    profile_count_add("entity_comp_lua_register_count");
}

static EseEntityComponent *_entity_component_lua_make(EseLuaEngine *engine, const char *script) {
    log_assert("ENTITY_COMP", engine, "_entity_component_lua_make called with NULL engine");

    EseEntityComponentLua *component = memory_manager.malloc(sizeof(EseEntityComponentLua), MMTAG_COMP_LUA);
    component->base.data = component;
    component->base.active = true;
    component->base.id = ese_uuid_create(engine);
    ese_uuid_ref(component->base.id);
    component->base.lua = engine;
    component->base.lua_ref = LUA_NOREF;
    component->base.type = ENTITY_COMPONENT_LUA;
    component->base.vtable = &lua_vtable;
    
    component->instance_ref = LUA_NOREF;
    component->engine = engine;
    component->arg = lua_value_create_number("argument count", 0);
    component->props = NULL;
    component->props_count = 0;

    // No free function needed for CachedLuaFunction
    component->function_cache = hashmap_create(NULL);

    if (script != NULL) {
        component->script = memory_manager.strdup(script, MMTAG_COMP_LUA);
    } else {
        component->script = NULL;
    }


    profile_count_add("entity_comp_lua_make_count");
    return &component->base;
}

EseEntityComponent *_entity_component_lua_copy(const EseEntityComponentLua *src) {
    log_assert("ENTITY_COMP", src, "entity_component_lua_copy called with NULL src");

    EseEntityComponent *copy = _entity_component_lua_make(src->base.lua, src->script);

    profile_count_add("entity_comp_lua_copy_count");
    return copy;
}

void _entity_component_lua_destroy(EseEntityComponentLua *component) {
    log_assert("ENTITY_COMP", component, "_entity_component_lua_destroy called with NULL src");

    if (component->instance_ref != LUA_NOREF && component->engine) {
        lua_engine_instance_remove(component->engine, component->instance_ref);
    }

    // Clear and free the function cache
    if (component->function_cache) {
        _entity_component_lua_clear_cache(component);
        hashmap_free(component->function_cache);
    }

    memory_manager.free(component->script);
    ese_uuid_destroy(component->base.id);
    lua_value_free(component->arg);

    memory_manager.free(component);
    
    profile_count_add("entity_comp_lua_destroy_count");
}

void _entity_component_lua_update(EseEntityComponentLua *component, EseEntity *entity, double delta_time) {
    log_assert("ENTITY_COMP", component, "_entity_component_lua_update called with NULL component");
    log_assert("ENTITY_COMP", entity, "_entity_component_lua_update called with NULL entity");

    profile_start(PROFILE_ENTITY_COMP_LUA_UPDATE);

    // If not script, just return
    if (component->script == NULL) {
        profile_cancel(PROFILE_ENTITY_COMP_LUA_UPDATE);
        profile_count_add("entity_comp_lua_update_no_script");
        return;
    }

    // Check for 1st time running
    if (component->instance_ref == LUA_NOREF) {
        // Script instance creation timing
        profile_start(PROFILE_ENTITY_COMP_LUA_INSTANCE_CREATE);
        component->instance_ref = lua_engine_instance_script(component->engine, component->script);
        profile_stop(PROFILE_ENTITY_COMP_LUA_INSTANCE_CREATE, "entity_comp_lua_instance_create");
        
        if (component->instance_ref == LUA_NOREF) {
            profile_cancel(PROFILE_ENTITY_COMP_LUA_UPDATE);
            profile_count_add("entity_comp_lua_update_instance_creation_failed");
            return;
        }

        // Function caching timing
        profile_start(PROFILE_ENTITY_COMP_LUA_FUNCTION_CACHE);
        _entity_component_lua_cache_functions(component);
        profile_stop(PROFILE_ENTITY_COMP_LUA_FUNCTION_CACHE, "entity_comp_lua_function_cache");

        // Init function timing
        profile_start(PROFILE_ENTITY_COMP_LUA_FUNCTION_RUN);
        entity_component_lua_run(component, entity, "entity_init", 0, NULL);
        profile_stop(PROFILE_ENTITY_COMP_LUA_FUNCTION_RUN, "entity_comp_lua_init_function");

        profile_count_add("entity_comp_lua_update_first_time_setup");
    }

    // Update function timing
    profile_start(PROFILE_ENTITY_COMP_LUA_FUNCTION_RUN);
    
    // Argument setup timing
    lua_value_set_number(component->arg, delta_time);
    
    // Function execution timing
    EseLuaValue *args[] = {component->arg};
    entity_component_lua_run(component, entity, "entity_update", 1, args);
    
    profile_stop(PROFILE_ENTITY_COMP_LUA_FUNCTION_RUN, "entity_comp_lua_update_function");

    profile_stop(PROFILE_ENTITY_COMP_LUA_UPDATE, "entity_comp_lua_update");
    profile_count_add("entity_comp_lua_update_success");
}

/**
 * @brief Lua function to create a new EseEntityComponentLua object.
 * 
 * @details Callable from Lua as EseEntityComponentLua.new(). Creates a new EsePoint.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new point object)
 * 
 * @warning Items created in Lua are owned by Lua
 */
static int _entity_component_lua_new(lua_State *L) {
    const char *script = NULL;

    int n_args = lua_gettop(L);
    if (n_args == 1 && lua_isstring(L, 1)) {
        script = lua_tostring(L, 1);
    } else if (n_args == 1 && !lua_isstring(L, 1)) {
        log_debug("ENTITY_COMP", "Script must be a string, ignored");
    } else if (n_args != 0) {
        log_debug("ENTITY_COMP", "EntityComponentLua.new() or EseEntityComponentLua.new(String)");
    }
    
    // Set engine reference
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
        
    // Create EseEntityComponent wrapper
    EseEntityComponent *component = _entity_component_lua_make(engine, script);
    
    // Push EseEntityComponent to Lua
    _entity_component_lua_register((EseEntityComponentLua *)component->data, true);
    entity_component_push(component);
    
    profile_count_add("entity_comp_lua_new_count");
    return 1;
}

void _entity_component_lua_cache_functions(EseEntityComponentLua *component) {
    log_assert("ENTITY_COMP", component, "_entity_component_lua_cache_functions called with NULL component");
    
    if (!component->engine || component->instance_ref == LUA_NOREF) {
        profile_count_add("entity_comp_lua_cache_functions_no_engine_or_instance");
        return;
    }
    
    profile_start(PROFILE_ENTITY_COMP_LUA_FUNCTION_CACHE);
    
    lua_State *L = component->engine->runtime;
    
    // Clear existing cache first
    _entity_component_lua_clear_cache(component);
    
    // Push the instance table onto the stack
    lua_rawgeti(L, LUA_REGISTRYINDEX, component->instance_ref);
    
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        profile_cancel(PROFILE_ENTITY_COMP_LUA_FUNCTION_CACHE);
        profile_count_add("entity_comp_lua_cache_functions_not_table");
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
            
            CachedLuaFunction *cached = memory_manager.malloc(sizeof(CachedLuaFunction), MMTAG_COMP_LUA);
            cached->function_ref = ref;
            cached->exists = true;
            hashmap_set(component->function_cache, func_name, cached);
        } else {
            // Function doesn't exist, cache as LUA_NOREF
            lua_pop(L, 1); // pop nil
            CachedLuaFunction *cached = memory_manager.malloc(sizeof(CachedLuaFunction), MMTAG_COMP_LUA);
            cached->function_ref = LUA_NOREF;
            cached->exists = false;
            hashmap_set(component->function_cache, func_name, cached);
        }
    }
    
    // Pop the instance table
    lua_pop(L, 1);
    
    profile_stop(PROFILE_ENTITY_COMP_LUA_FUNCTION_CACHE, "entity_comp_lua_cache_functions");
    profile_count_add("entity_comp_lua_cache_functions_success");
}

void _entity_component_lua_clear_cache(EseEntityComponentLua *component) {
    log_assert("ENTITY_COMP", component, "_entity_component_lua_clear_cache called with NULL component");
    
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

bool entity_component_lua_run(EseEntityComponentLua *component, EseEntity *entity, const char *func_name, int argc, EseLuaValue *argv[]) {
    log_assert("ENTITY_COMP", component, "entity_component_lua_run called with NULL component");
    log_assert("ENTITY_COMP", entity, "entity_component_lua_run called with NULL entity");
    log_assert("ENTITY_COMP", func_name, "entity_component_lua_run called with NULL func_name");
    
    profile_start(PROFILE_ENTITY_COMP_LUA_FUNCTION_RUN);
    
    if (!component->function_cache || !component->engine) {
        profile_cancel(PROFILE_ENTITY_COMP_LUA_FUNCTION_RUN);
        profile_count_add("entity_comp_lua_run_no_cache_or_engine");
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
                profile_cancel(PROFILE_ENTITY_COMP_LUA_FUNCTION_RUN);
                profile_count_add("entity_comp_lua_run_no_script");
                return false;
            }
            
            // Script instance creation timing
            profile_start(PROFILE_ENTITY_COMP_LUA_INSTANCE_CREATE);
            component->instance_ref = lua_engine_instance_script(component->engine, component->script);
            profile_stop(PROFILE_ENTITY_COMP_LUA_INSTANCE_CREATE, "entity_comp_lua_instance_create");
            
            if (component->instance_ref == LUA_NOREF) {
                profile_cancel(PROFILE_ENTITY_COMP_LUA_FUNCTION_RUN);
                profile_count_add("entity_comp_lua_run_instance_creation_failed");
                return false;
            }
            
            if (strcmp(func_name, "entity_init") != 0) {
                entity_component_lua_run(component, entity, "entity_init", 0, NULL);
            }

            // Function caching timing
            profile_start(PROFILE_ENTITY_COMP_LUA_FUNCTION_CACHE);
            _entity_component_lua_cache_functions(component);
            profile_stop(PROFILE_ENTITY_COMP_LUA_FUNCTION_CACHE, "entity_comp_lua_function_cache");
        }
        
        // Registry access timing
        lua_rawgeti(L, LUA_REGISTRYINDEX, component->instance_ref);
        
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            profile_cancel(PROFILE_ENTITY_COMP_LUA_FUNCTION_RUN);
            profile_count_add("entity_comp_lua_run_instance_not_table");
            return false;
        }
        
        // Function field lookup timing
        lua_getfield(L, -1, func_name);
        
        if (lua_isfunction(L, -1)) {
            // Function reference creation timing
            int ref = luaL_ref(L, LUA_REGISTRYINDEX);
            
            // Memory allocation timing
            cached = memory_manager.malloc(sizeof(CachedLuaFunction), MMTAG_COMP_LUA);
            cached->function_ref = ref;
            cached->exists = true;
            
            // Hashmap insertion timing
            hashmap_set(component->function_cache, func_name, cached);
        } else {
            // Function doesn't exist, cache as LUA_NOREF
            lua_pop(L, 1); // pop nil
            cached = memory_manager.malloc(sizeof(CachedLuaFunction), MMTAG_COMP_LUA);
            cached->function_ref = LUA_NOREF;
            cached->exists = false;
            hashmap_set(component->function_cache, func_name, cached);
        }
        
        // Pop the instance table
        lua_pop(L, 1);
    }
    
    // If function doesn't exist, ignore
    if (!cached->exists) {
        profile_cancel(PROFILE_ENTITY_COMP_LUA_FUNCTION_RUN);
        profile_count_add("entity_comp_lua_run_function_not_exists");
        return false;
    }
    
    // Engine function execution timing
    bool result = lua_engine_run_function_ref(component->engine, cached->function_ref, entity_get_lua_ref(entity), argc, argv, NULL);
    
    if (result) {
        profile_stop(PROFILE_ENTITY_COMP_LUA_FUNCTION_RUN, "entity_comp_lua_function_run");
        profile_count_add("entity_comp_lua_run_success");
    } else {
        profile_cancel(PROFILE_ENTITY_COMP_LUA_FUNCTION_RUN);
        profile_count_add("entity_comp_lua_run_failed");
    }
    
    return result;
}

EseEntityComponentLua *_entity_component_lua_get(lua_State *L, int idx) {
    // Check if the value at idx is a table
    if (!lua_istable(L, idx)) {
        return NULL;
    }
    
    // Check if it has the correct metatable
    if (!lua_getmetatable(L, idx)) {
        return NULL; // No metatable
    }
    
    // Get the expected metatable for comparison
    luaL_getmetatable(L, ENTITY_COMPONENT_LUA_PROXY_META);
    
    // Compare metatables
    if (!lua_rawequal(L, -1, -2)) {
        lua_pop(L, 2); // Pop both metatables
        return NULL; // Wrong metatable
    }
    
    lua_pop(L, 2); // Pop both metatables
    
    // Get the __ptr field
    lua_getfield(L, idx, "__ptr");
    
    // Check if __ptr exists and is light userdata
    if (!lua_islightuserdata(L, -1)) {
        lua_pop(L, 1); // Pop the __ptr value (or nil)
        return NULL;
    }
    
    // Extract the pointer
    void *comp = lua_touserdata(L, -1);
    lua_pop(L, 1); // Pop the __ptr value
    
    return (EseEntityComponentLua *)comp;
}

/**
 * @brief Lua __index metamethod for EseEntityComponentLua objects (getter).
 * 
 * @details Handles property access for EseEntityComponentLua objects from Lua. 
 * 
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack (1 for valid properties, 0 otherwise)
 */
static int _entity_component_lua_index(lua_State *L) {
    EseEntityComponentLua *component = _entity_component_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    
    // SAFETY: Return nil for freed components
    if (!component) {
        lua_pushnil(L);
        return 1;
    }
    
    if (!key) return 0;
    
    if (strcmp(key, "active") == 0) {
        lua_pushboolean(L, component->base.active);
        return 1;
    } else if (strcmp(key, "id") == 0) {
        lua_pushstring(L, ese_uuid_get_value(component->base.id));
        return 1;
    } else if (strcmp(key, "script") == 0) {
        lua_pushstring(L, component->script ? component->script : "");
        return 1;
    }
    
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseEntityComponentLua objects (setter).
 * 
 * @details Handles property assignment for EseEntityComponentLua objects from Lua.
 * 
 * @param L Lua state pointer
 * @return Always returns 0 (no return values) or throws Lua error for invalid operations
 */
static int _entity_component_lua_newindex(lua_State *L) {
    EseEntityComponentLua *component = _entity_component_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    
    // SAFETY: Silently ignore writes to freed components
    if (!component) {
        return 0;
    }
    
    if (!key) return 0;
    
    if (strcmp(key, "active") == 0) {
        if (!lua_isboolean(L, 3)) {
            return luaL_error(L, "active must be a boolean");
        }
        component->base.active = lua_toboolean(L, 3);
        lua_pushboolean(L, component->base.active);
        return 1;
    } else if (strcmp(key, "id") == 0) {
        return luaL_error(L, "id is read-only");
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
            _entity_component_lua_clear_cache(component);
        }

        if (component->script != NULL) {
            memory_manager.free(component->script);
            component->script = NULL;
        }
        
        if (lua_isstring(L, 3)) {
            const char *script = lua_tostring(L, 3);
            component->script = memory_manager.strdup(script, MMTAG_COMP_LUA);
        }

        return 0;
    }
    
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __gc metamethod for EseEntityComponentLua objects.
 *
 * @details Checks the '__is_lua_owned' flag in the proxy table. If true,
 * it means this EseEntityComponentLua's memory was allocated by Lua and should be freed.
 * If false, the EseEntityComponentLua's memory is managed externally (by C) and is not freed here.
 *
 * @param L Lua state pointer
 * @return Always 0 (no return values)
 */
static int _entity_component_lua_gc(lua_State *L) {
    EseEntityComponentLua *component = _entity_component_lua_get(L, 1);
    
    if (component) {
        lua_getfield(L, 1, "__is_lua_owned");
        bool is_lua_owned = lua_toboolean(L, -1);
        lua_pop(L, 1);
        
        if (is_lua_owned) {
            _entity_component_lua_destroy(component);
            log_debug("LUA_GC", "EntityComponentLua object (Lua-owned) garbage collected and C memory freed.");
        } else {
            log_debug("LUA_GC", "EntityComponentLua object (C-owned) garbage collected, C memory *not* freed.");
        }
    }
    
    return 0;
}

static int _entity_component_lua_tostring(lua_State *L) {
    EseEntityComponentLua *component = _entity_component_lua_get(L, 1);
    
    if (!component) {
        lua_pushstring(L, "EntityComponentLua: (invalid)");
        return 1;
    }
    
    char buf[128];
    snprintf(buf, sizeof(buf), "EntityComponentLua: %p (id=%s active=%s script=%s)", 
             (void*)component,
             ese_uuid_get_value(component->base.id),
             component->base.active ? "true" : "false",
             component->script ? component->script : "none");
    lua_pushstring(L, buf);
    
    return 1;
}

void _entity_component_lua_init(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "_entity_component_lua_init called with NULL engine");

    lua_State *L = engine->runtime;
    
    // Register EseEntityComponentLua metatable
    if (luaL_newmetatable(L, ENTITY_COMPONENT_LUA_PROXY_META)) {
        log_debug("LUA", "Adding EntityComponentLuaProxyMeta to engine");
        lua_pushstring(L, "EntityComponentLuaProxyMeta");
        lua_setfield(L, -2, "__name");
        lua_pushcfunction(L, _entity_component_lua_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, _entity_component_lua_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, _entity_component_lua_gc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, _entity_component_lua_tostring);
        lua_setfield(L, -2, "__tostring");
        lua_pushstring(L, "locked");
        lua_setfield(L, -2, "__metatable");
    }
    lua_pop(L, 1);
    
    // Create global EseEntityComponentLua table with constructor
    lua_getglobal(L, "EntityComponentLua");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        log_debug("LUA", "Creating global EseEntityComponentLua table");
        lua_newtable(L);
        lua_pushcfunction(L, _entity_component_lua_new);
        lua_setfield(L, -2, "new");
        lua_setglobal(L, "EntityComponentLua");
    } else {
        lua_pop(L, 1);
    }
    
    profile_count_add("entity_comp_lua_init_count");
}

EseEntityComponent *entity_component_lua_create(EseLuaEngine *engine, const char *script) {
    log_assert("ENTITY_COMP", engine, "entity_component_lua_create called with NULL engine");
    
    EseEntityComponent *component = _entity_component_lua_make(engine, script);

    // Push EseEntityComponent to Lua
    _entity_component_lua_register((EseEntityComponentLua *)component->data, false);

    profile_count_add("entity_comp_lua_create_count");
    return component;
}
