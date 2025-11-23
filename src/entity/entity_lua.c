#include "entity_lua.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "entity.h"
#include "entity/components/entity_component_map.h"
#include "entity/components/entity_component_private.h"
#include "entity_private.h"
#include "scripting/lua_engine.h"
#include "scripting/lua_engine_private.h"
#include "scripting/lua_value.h"
#include "types/types.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/lua/src/lauxlib.h"
#include "vendor/lua/src/lua.h"
#include "vendor/lua/src/lualib.h"
#include <string.h>

// Forward declarations
static int _entity_lua_subscribe(lua_State *L);
static int _entity_lua_unsubscribe(lua_State *L);
static int _entity_lua_publish(lua_State *L);
static void _entity_remove_subscription(EseEntity *entity, const char *topic_name,
                                        const char *function_name);

/**
 * @brief Increment ref-count and ensure a Lua userdata exists/registered for
 * the entity.
 */
void entity_ref(EseEntity *entity) {
    log_assert("ENTITY", entity, "_entity_lua_register called with NULL entity");
    profile_start(PROFILE_ENTITY_LUA_REGISTER);
    if (entity->lua_ref == LUA_NOREF) {
        // Create userdata holding the entity pointer
        EseEntity **ud = (EseEntity **)lua_newuserdata(entity->lua->runtime, sizeof(EseEntity *));
        *ud = entity;
        // Attach metatable
        luaL_getmetatable(entity->lua->runtime, "EntityProxyMeta");
        lua_setmetatable(entity->lua->runtime, -2);
        // Initialize environment table (Lua 5.1 userdata env) for `data`
        lua_newtable(entity->lua->runtime);
        lua_setfenv(entity->lua->runtime, -2);
        // Store a reference to this userdata in the Lua registry
        entity->lua_ref = luaL_ref(entity->lua->runtime, LUA_REGISTRYINDEX);
        entity->lua_ref_count = 1;
        lua_value_set_ref(entity->lua_val_ref, entity->lua_ref);
    } else {
        entity->lua_ref_count++;
    }
    profile_stop(PROFILE_ENTITY_LUA_REGISTER, "entity_ref");
    profile_count_add("entity_ref_count");
}

void entity_lua_push(EseEntity *entity) {
    log_assert("ENTITY", entity, "entity_lua_push called with NULL entity");
    log_assert("ENTITY", entity->lua_ref != LUA_NOREF,
               "entity_lua_push entity not registered with lua");

    profile_start(PROFILE_ENTITY_LUA_PROPERTY_ACCESS);

    // Push the proxy table back onto the stack for Lua to receive
    lua_rawgeti(entity->lua->runtime, LUA_REGISTRYINDEX, entity->lua_ref);

    profile_stop(PROFILE_ENTITY_LUA_PROPERTY_ACCESS, "entity_lua_push");
    profile_count_add("entity_lua_push_count");
}

/**
 * @brief Extracts an EseEntity pointer from a Lua userdata object with type
 * safety.
 */
EseEntity *entity_lua_get(lua_State *L, int idx) {
    EseEntity **ud = (EseEntity **)luaL_testudata(L, idx, "EntityProxyMeta");
    if (!ud)
        return NULL;
    return *ud;
}

/**
 * @brief Lua function to create a new EseEntity object.
 */
static int _entity_lua_new(lua_State *L) {
    EseLuaEngine *lua = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (!lua) {
        return luaL_error(L, "Lua engine not found");
    }
    EseEntity *entity = _entity_make(lua);

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    engine_add_entity(engine, entity);

    // ensure C-owned registry ref & userdata exist
    entity_ref(entity);
    entity_lua_push(entity);

    profile_count_add("entity_lua_new_count");
    return 1;
}

/**
 * @brief Helper to get EseEntity from components proxy table.
 */
static EseEntity *_entity_lua_components_get_entity(lua_State *L, int idx) {
    lua_getfield(L, idx, "__entity");
    EseEntity *entity = (EseEntity *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return entity;
}

/**
 * @brief Add the passed component to the entity.
 */
static int _entity_lua_components_add(lua_State *L) {
    // Supports both calls:
    //  - components:add(component)  -> stack: [components_proxy, component]
    //  - components.add(component)  -> stack: [component]
    int top = lua_gettop(L);
    int comp_idx = 1;
    if (top >= 2 && lua_istable(L, 1)) {
        comp_idx = 2;
    }

    bool is_updated = lua_isuserdata(L, comp_idx);
    EseEntity *entity = (EseEntity *)lua_touserdata(L, lua_upvalueindex(1));

    // Move the component argument to index 1 so entity_component_get can read
    // it
    lua_pushvalue(L, comp_idx);
    lua_replace(L, 1);

    EseEntityComponent *comp = entity_component_get(L);
    if (comp == NULL) {
        luaL_argerror(L, 1, "Expected a component argument not found.");
        lua_pushboolean(L, false);
        // Stack: [entity, component, false]
        return 1;
    }

    entity_component_add(entity, comp);

    if (is_updated) {
        comp->vtable->ref(comp);
        lua_pushboolean(L, true);
        return 1;
    }

    profile_count_add("entity_lua_components_add_count");

    lua_pushboolean(L, true);
    // Stack: [component, true]
    return 1;
}

/**
 * @brief Lua function to remove a component from an entity.
 */
static int _entity_lua_components_remove(lua_State *L) {
    // Expected stack: [components_proxy, component]
    bool is_updated = lua_isuserdata(L, 2);
    EseEntity *entity = _entity_lua_components_get_entity(L, 1);

    if (!entity) {
        luaL_error(L, "Invalid entity object.");
        lua_pushboolean(L, false);
        // Stack: [entity, component, false]
        return 1;
    }

    // Move the component argument to index 1 so entity_component_get can read
    // it
    lua_pushvalue(L, 2);
    lua_replace(L, 1);

    EseEntityComponent *comp_to_remove = entity_component_get(L);
    if (comp_to_remove == NULL) {
        luaL_argerror(L, 2, "Expected a component object.");
        lua_pushboolean(L, false);
        // Stack: [entity, component, false]
        return 1;
    }

    // Find the component in the entity's component array.
    // This loop doesn't use the Lua stack, so its state is unchanged.
    int idx = -1;
    for (size_t i = 0; i < entity->component_count; ++i) {
        if (entity->components[i] == comp_to_remove) {
            idx = (int)i;
            break;
        }
    }

    if (idx < 0) {
        lua_pushboolean(L, false);
        // Stack: [entity, component, false]
        return 1;
    }

    // Shift elements to remove the component.
    // This is C memory management and does not affect the Lua stack.
    for (size_t i = idx; i < entity->component_count - 1; ++i) {
        entity->components[i] = entity->components[i + 1];
    }

    entity->component_count--;
    entity->components[entity->component_count] = NULL;

    if (is_updated) {
        comp_to_remove->vtable->unref(comp_to_remove);
        lua_pushboolean(L, true);
        return 1;
    }

    lua_pushboolean(L, true);
    // Stack: [entity, component, true]

    profile_count_add("entity_lua_components_remove_count");
    return 1;
}

/**
 * @brief Lua function to insert a component at a specific index.
 */
static int _entity_lua_components_insert(lua_State *L) {
    // Expected stack: [components_proxy, component, index]
    bool is_updated = lua_isuserdata(L, 2);
    EseEntity *entity = _entity_lua_components_get_entity(L, 1);

    if (!entity) {
        log_warn("ENTITY", "Invalid entity object.");
        lua_pushboolean(L, false);
        // Stack: [entity, component, index, false]
        return 1;
    }

    // Move the component argument to index 1 so entity_component_get can read
    // it
    lua_pushvalue(L, 2);
    lua_replace(L, 1);

    EseEntityComponent *comp = entity_component_get(L);
    if (comp == NULL) {
        luaL_argerror(L, 2, "Expected a component object.");
        lua_pushboolean(L, false);
        // Stack: [entity, component, index, false]
        return 1;
    }

    int index = (int)luaL_checkinteger(L, 3) - 1; // Lua is 1-based

    if (index < 0 || index > (int)entity->component_count) {
        log_warn("ENTITY", "Index out of bounds.");
        lua_pushboolean(L, false);
        // Stack: [entity, component, index, false]
        return 1;
    }

    // Resize array if necessary
    if (entity->component_count == entity->component_capacity) {
        size_t new_capacity = entity->component_capacity * 2;
        EseEntityComponent **new_components = memory_manager.realloc(
            entity->components, sizeof(EseEntityComponent *) * new_capacity, MMTAG_ENTITY);
        entity->components = new_components;
        entity->component_capacity = new_capacity;
    }

    // Shift elements to make space for the new component
    for (size_t i = entity->component_count; i > index; --i) {
        entity->components[i] = entity->components[i - 1];
    }

    entity->components[index] = comp;
    entity->component_count++;

    if (is_updated) {
        comp->vtable->ref(comp);
        lua_pushboolean(L, true);
        return 1;
    }

    lua_pushboolean(L, true);
    // Stack: [entity, component, index, true]

    profile_count_add("entity_lua_components_insert_count");
    return 1;
}

/**
 * @brief Lua function to remove and return the last component.
 */
static int _entity_lua_components_pop(lua_State *L) {
    // Initial stack: [entity_proxy]
    EseEntity *entity = _entity_lua_components_get_entity(L, 1);

    if (!entity) {
        return luaL_error(L, "Invalid entity object.");
    }

    if (entity->component_count == 0) {
        lua_pushnil(L);
        // Stack: [entity_proxy, nil]
        return 1;
    }

    EseEntityComponent *comp = entity->components[entity->component_count - 1];
    comp->vtable->unref(comp);
    entity->components[entity->component_count - 1] = NULL;
    entity->component_count--;

    // Get the existing Lua proxy for the component from its Lua reference
    lua_rawgeti(L, LUA_REGISTRYINDEX, comp->lua_ref);
    // Stack: [entity_proxy, component_proxy]

    profile_count_add("entity_lua_components_pop_count");
    return 1;
}

/**
 * @brief Lua function to remove and return the first component.
 */
static int _entity_lua_components_shift(lua_State *L) {
    // Initial stack: [entity_proxy]
    EseEntity *entity = _entity_lua_components_get_entity(L, 1);

    if (!entity) {
        return luaL_error(L, "Invalid entity object.");
    }

    if (entity->component_count == 0) {
        lua_pushnil(L);
        // Stack: [entity_proxy, nil]
        return 1;
    }

    EseEntityComponent *comp = entity->components[0];
    comp->vtable->unref(comp);

    // Shift all elements
    for (size_t i = 0; i < entity->component_count - 1; ++i) {
        entity->components[i] = entity->components[i + 1];
    }

    entity->component_count--;
    entity->components[entity->component_count] = NULL;

    // if (is_updated) {
    //     comp->vtable->unref(comp);
    // }

    // Get the existing Lua proxy for the component from its Lua reference
    lua_rawgeti(L, LUA_REGISTRYINDEX, comp->lua_ref);
    // Stack: [entity_proxy, component_proxy]

    profile_count_add("entity_lua_components_shift_count");
    return 1;
}

/**
 * @brief Lua function to find component(s) by type and return their indexes
 */
static int _entity_lua_components_find(lua_State *L) {
    // Initial stack: [entity_proxy, comp_type_name_string]
    EseEntity *entity = _entity_lua_components_get_entity(L, 1);
    const char *comp_type_name = luaL_checkstring(L, 2);

    if (!entity) {
        return luaL_error(L, "Invalid entity object.");
    }

    char full_metatable_name[128];
    snprintf(full_metatable_name, sizeof(full_metatable_name), "%sProxyMeta", comp_type_name);

    lua_newtable(L);
    // Stack: [entity_proxy, comp_type_name_string, result_table]
    int table_index = 1;
    int result_table_idx = lua_gettop(L);

    for (size_t i = 0; i < entity->component_count; ++i) {
        EseEntityComponent *comp = entity->components[i];

        // Push the component's Lua proxy to the stack
        lua_rawgeti(L, LUA_REGISTRYINDEX, comp->lua_ref);
        // Stack: [..., result_table, component_proxy]

        if (lua_getmetatable(L, -1)) {
            // Stack: [..., result_table, component_proxy, metatable]
            lua_getfield(L, -1, "__name");
            // Stack: [..., result_table, component_proxy, metatable,
            // __name_string]
            if (lua_isstring(L, -1)) {
                const char *mt_name = lua_tostring(L, -1);
            }

            if (lua_isstring(L, -1) && strcmp(lua_tostring(L, -1), full_metatable_name) == 0) {
                lua_pop(L, 2); // Pop name and metatable
                // Stack: [..., result_table, component_proxy]

                lua_pushinteger(L, i + 1);
                // Stack: [..., result_table, component_proxy, index_integer]
                lua_rawseti(L, result_table_idx, table_index++);
                // Stack: [..., result_table, component_proxy] (index_integer is
                // popped)
            } else {
                lua_pop(L, 2); // Pop name and metatable
                               // Stack: [..., result_table, component_proxy]
            }
        }
        lua_pop(L, 1); // Pop component proxy
                       // Stack: [..., result_table]
    }

    // Stack: [entity_proxy, comp_type_name_string, result_table]

    profile_count_add("entity_lua_components_find_count");
    return 1;
}

/**
 * @brief Lua function to get a component by its ID.
 */
static int _entity_lua_components_get(lua_State *L) {
    // Initial stack: [entity_proxy, component_id_string]
    EseEntity *entity = _entity_lua_components_get_entity(L, 1);
    const char *id = luaL_checkstring(L, 2);

    if (!entity) {
        return luaL_error(L, "Invalid entity object.");
    }

    // Assuming _entity_component_find_index finds the component in the array by
    // ID.
    int idx = _entity_component_find_index(entity, id);

    if (idx >= 0) {
        EseEntityComponent *comp = entity->components[idx];
        // Get the existing Lua proxy for the component
        lua_rawgeti(L, LUA_REGISTRYINDEX, comp->lua_ref);
        // Stack: [entity_proxy, component_id_string, component_proxy]
    } else {
        lua_pushnil(L);
        // Stack: [entity_proxy, component_id_string, nil]
    }

    profile_count_add("entity_lua_components_get_count");
    return 1;
}

// Tag system Lua functions

/**
 * @brief Lua function to add a tag to an entity.
 *
 * Supports both:
 *   - entity:add_tag("tag")  -- colon syntax (entity is argument 1)
 *   - entity.add_tag("tag")  -- dot syntax (entity is captured as upvalue)
 */
static int _entity_lua_add_tag(lua_State *L) {
    EseEntity *entity = (EseEntity *)lua_engine_instance_method_normalize(
        L, (EseLuaGetSelfFn)entity_lua_get, "Entity");
    if (!entity) {
        return luaL_error(L, "Invalid entity");
    }

    // After normalization, entity:add_tag(tag) has 1 argument at index 1.
    if (!lua_isstring(L, 1)) {
        return luaL_error(L, "Tag must be a string");
    }

    const char *tag = lua_tostring(L, 1);
    bool success = entity_add_tag(entity, tag);
    lua_pushboolean(L, success);
    return 1;
}

/**
 * @brief Lua function to remove a tag from an entity.
 */
static int _entity_lua_remove_tag(lua_State *L) {
    EseEntity *entity = entity_lua_get(L, 1);
    if (!entity) {
        return luaL_error(L, "Invalid entity");
    }

    if (!lua_isstring(L, 2)) {
        return luaL_error(L, "Tag must be a string");
    }

    const char *tag = lua_tostring(L, 2);
    bool success = entity_remove_tag(entity, tag);
    lua_pushboolean(L, success);
    return 1;
}

/**
 * @brief Lua function to destroy an entity.
 */
static int _entity_lua_destroy(lua_State *L) {
    EseEntity *entity = entity_lua_get(L, 1);
    if (!entity) {
        return luaL_error(L, "Invalid entity");
    }

    // Get engine from registry
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    if (!engine) {
        return luaL_error(L, "Engine not found");
    }

    if (entity->destroyed) {
        // could be called more than once, just ignore it
        lua_pushboolean(L, true);
        return 1;
    }

    // Destroy the entity
    entity->destroyed = true;
    entity->active = false;
    engine_remove_entity(engine, entity);

    lua_pushboolean(L, true);
    return 1;
}

/**
 * @brief Lua function to check if an entity has a tag.
 */
static int _entity_lua_has_tag(lua_State *L) {
    EseEntity *entity = entity_lua_get(L, 1);
    if (!entity) {
        return luaL_error(L, "Invalid entity");
    }

    if (!lua_isstring(L, 2)) {
        return luaL_error(L, "Tag must be a string");
    }

    const char *tag = lua_tostring(L, 2);
    bool has_tag = entity_has_tag(entity, tag);
    lua_pushboolean(L, has_tag);
    return 1;
}

/**
 * @brief Lua function to find entities by tag.
 */
static int _entity_lua_find_by_tag(lua_State *L) {
    if (!lua_isstring(L, 1)) {
        return luaL_error(L, "Tag must be a string");
    }

    const char *tag = lua_tostring(L, 1);

    // Get engine from registry
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    if (!engine) {
        return luaL_error(L, "Engine not found");
    }

    // Find entities with tag (limit to 1000 for safety)
    EseEntity **found = engine_find_by_tag(engine, tag, 1000);
    if (!found) {
        lua_newtable(L);
        return 1;
    }

    // Count found entities
    int count = 0;
    while (found[count] != NULL) {
        count++;
    }

    // Create Lua table with found entities
    lua_newtable(L);
    for (int i = 0; i < count; i++) {
        entity_lua_push(found[i]);
        lua_rawseti(L, -2, i + 1); // Lua uses 1-based indexing
    }

    // Free the result array
    memory_manager.free(found);

    return 1;
}

/**
 * @brief Lua function to find the first entity by tag.
 */
static int _entity_lua_find_first_by_tag(lua_State *L) {
    if (!lua_isstring(L, 1)) {
        return luaL_error(L, "Tag must be a string");
    }

    const char *tag = lua_tostring(L, 1);

    // Get engine from registry
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    if (!engine) {
        return luaL_error(L, "Engine not found");
    }

    // Find 1 entity with tag
    EseEntity **found = engine_find_by_tag(engine, tag, 1);
    if (!found) {
        lua_pushnil(L);
        return 1;
    }

    // Push the found entity
    entity_lua_push(found[0]);
    memory_manager.free(found);
    return 1;
}

/**
 * @brief Lua function to find an entity by ID.
 */
static int _entity_lua_find_by_id(lua_State *L) {
    if (!lua_isstring(L, 1)) {
        return luaL_error(L, "UUID must be a string");
    }

    const char *uuid_string = lua_tostring(L, 1);

    // Get engine from registry
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    if (!engine) {
        return luaL_error(L, "Engine not found");
    }

    EseEntity *found = engine_find_by_id(engine, uuid_string);
    if (!found) {
        lua_pushnil(L);
        return 1;
    }

    entity_lua_push(found);
    return 1;
}

// Returns true if the function is run, false if the function is not found or
// run
static int _entity_lua_dispatch(lua_State *L) {
    EseEntity *entity = (EseEntity *)lua_touserdata(L, lua_upvalueindex(1));
    if (!entity) {
        lua_pushboolean(L, false);
        return 1;
    }

    int func_name_index = 1;
    int argc = 0;

    // Check if first argument is an Entity userdata (colon syntax:
    // entity:dispatch('func_name', ...))
    if (entity_lua_get(L, 1) != NULL) {
        // Colon syntax: entity:dispatch('func_name')
        // Stack: [entity, 'func_name', ...args]
        func_name_index = 2;
        argc = lua_gettop(L) - 2; // Total args minus entity and function name
    } else {
        // Dot syntax: entity.dispatch('func_name')
        // Stack: ['func_name', ...args]
        argc = lua_gettop(L) - 1; // Total args minus function name
    }

    const char *func_name = luaL_checkstring(L, func_name_index);
    EseLuaValue **argv = NULL;

    if (argc > 0) {
        argv = memory_manager.calloc(argc, sizeof(EseLuaValue *), MMTAG_LUA);
        if (!argv) {
            lua_pushboolean(L, false);
            return 1;
        }

        for (int i = 0; i < argc; ++i) {
            // The Lua arguments start after the function name
            int lua_arg_index = func_name_index + 1 + i;
            argv[i] = lua_value_from_stack(L, lua_arg_index);
        }
    }

    // For now, just call without arguments since we're not converting them
    entity_run_function_with_args(entity, func_name, argc, argv);

    if (argv) {
        for (int i = 0; i < argc; ++i) {
            lua_value_destroy(argv[i]);
        }
        memory_manager.free(argv);
    }

    lua_pushboolean(L, true);
    return 1;
}

/**
 * @brief __index for components proxy.
 */
static int _entity_lua_components_index(lua_State *L) {
    // Initial stack: [userdata, key]
    EseEntity *entity = _entity_lua_components_get_entity(L, 1);

    if (!entity) {
        lua_pushnil(L);
        // Stack after lua_pushnil: [userdata, key, nil]
        return 1;
    }

    if (entity->destroyed) {
        lua_pushnil(L);
        // Stack after lua_pushnil: [userdata, key, nil]
        return 1;
    }

    // Check if the key is a number for array access
    if (lua_isnumber(L, 2)) {
        int index = (int)lua_tointeger(L, 2) - 1; // Convert to 0-based
        if (index >= 0 && index < (int)entity->component_count) {
            EseEntityComponent *comp = entity->components[index];

            // Get the existing Lua proxy for the component from its Lua
            // reference
            lua_rawgeti(L, LUA_REGISTRYINDEX, comp->lua_ref);
            // Stack after lua_rawgeti: [userdata, key, component_proxy]
            return 1;
        } else {
            lua_pushnil(L);
            // Stack after lua_pushnil: [userdata, key, nil]
            return 1;
        }
    }

    // Check if the key is a string for method names
    const char *key = lua_tostring(L, 2);
    if (!key)
        return 0;

    if (strcmp(key, "count") == 0) {
        lua_pushinteger(L, entity->component_count);
        // Stack: [userdata, key, integer]
        return 1;
    } else if (strcmp(key, "add") == 0) {
        lua_pushlightuserdata(L, entity);
        // Stack: [userdata, key, lightuserdata]
        lua_pushcclosure(L, _entity_lua_components_add, 1);
        // Stack: [userdata, key, cclosure]
        return 1;
    } else if (strcmp(key, "remove") == 0) {
        lua_pushcfunction(L, _entity_lua_components_remove);
        // Stack: [userdata, key, cfunction]
        return 1;
    } else if (strcmp(key, "insert") == 0) {
        lua_pushcfunction(L, _entity_lua_components_insert);
        // Stack: [userdata, key, cfunction]
        return 1;
    } else if (strcmp(key, "pop") == 0) {
        lua_pushcfunction(L, _entity_lua_components_pop);
        // Stack: [userdata, key, cfunction]
        return 1;
    } else if (strcmp(key, "shift") == 0) {
        lua_pushcfunction(L, _entity_lua_components_shift);
        // Stack: [userdata, key, cfunction]
        return 1;
    } else if (strcmp(key, "find") == 0) {
        lua_pushcfunction(L, _entity_lua_components_find);
        // Stack: [userdata, key, cfunction]
        return 1;
    } else if (strcmp(key, "get") == 0) {
        lua_pushcfunction(L, _entity_lua_components_get);
        // Stack: [userdata, key, cfunction]
        return 1;
    }

    // The function returns 0, indicating no value was pushed.
    // The stack remains: [userdata, key]
    return 0;
}

/**
 * @brief Lua __index metamethod for EseEntity objects (getter).
 */
static int _entity_lua_index(lua_State *L) {
    EseEntity *entity = entity_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);

    // SAFETY: Return nil for freed object
    if (!entity) {
        lua_pushnil(L);
        return 1;
    }

    if (entity->destroyed) {
        lua_pushnil(L);
        // Stack after lua_pushnil: [userdata, key, nil]
        return 1;
    }

    if (!key)
        return 0;

    if (strcmp(key, "id") == 0) {
        lua_pushstring(L, ese_uuid_get_value(entity->id));
        return 1;
    } else if (strcmp(key, "active") == 0) {
        lua_pushboolean(L, entity->active);
        return 1;
    } else if (strcmp(key, "visible") == 0) {
        lua_pushboolean(L, entity->visible);
        return 1;
    } else if (strcmp(key, "persistent") == 0) {
        lua_pushboolean(L, entity->persistent);
        return 1;
    } else if (strcmp(key, "draw_order") == 0) {
        lua_pushinteger(L, (lua_Integer)(entity->draw_order >> DRAW_ORDER_SHIFT));
        return 1;
    } else if (strcmp(key, "position") == 0) {
        if (entity->position != NULL && ese_point_get_lua_ref(entity->position) != LUA_NOREF) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ese_point_get_lua_ref(entity->position));
            return 1;
        } else {
            lua_pushnil(L);
            return 1;
        }
    } else if (strcmp(key, "bounds") == 0) {
        if (entity->collision_bounds != NULL) {
            ese_rect_lua_push(entity->collision_bounds);
            return 1;
        } else {
            lua_pushnil(L);
            return 1;
        }
    } else if (strcmp(key, "world_bounds") == 0) {
        if (entity->collision_world_bounds != NULL) {
            ese_rect_lua_push(entity->collision_world_bounds);
            return 1;
        } else {
            lua_pushnil(L);
            return 1;
        }
    } else if (strcmp(key, "dispatch") == 0) {
        lua_pushlightuserdata(L, entity);
        lua_pushcclosure(L, _entity_lua_dispatch, 1);
        return 1;
    } else if (strcmp(key, "components") == 0) {
        // Create components proxy table
        lua_newtable(L);
        lua_pushlightuserdata(L, entity);
        lua_setfield(L, -2, "__entity");
        luaL_getmetatable(L, "ComponentsProxyMeta");
        lua_setmetatable(L, -2);
        return 1;
    } else if (strcmp(key, "data") == 0 || strcmp(key, "__data") == 0) {
        // Return or initialize the userdata environment table (Lua 5.1)
        lua_getfenv(L, 1);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setfenv(L, 1);
        }
        return 1;
    } else if (strcmp(key, "add_tag") == 0) {
        lua_pushlightuserdata(L, entity);
        lua_pushcclosure(L, _entity_lua_add_tag, 1);
        return 1;
    } else if (strcmp(key, "remove_tag") == 0) {
        lua_pushlightuserdata(L, entity);
        lua_pushcclosure(L, _entity_lua_remove_tag, 1);
        return 1;
    } else if (strcmp(key, "destroy") == 0) {
        lua_pushlightuserdata(L, entity);
        lua_pushcclosure(L, _entity_lua_destroy, 1);
        return 1;
    } else if (strcmp(key, "has_tag") == 0) {
        lua_pushlightuserdata(L, entity);
        lua_pushcclosure(L, _entity_lua_has_tag, 1);
        return 1;
    } else if (strcmp(key, "tags") == 0) {
        // Return a table of all tags
        lua_newtable(L);
        for (size_t i = 0; i < entity->tag_count; i++) {
            lua_pushstring(L, entity->tags[i]);
            lua_rawseti(L, -2, i + 1); // Lua uses 1-based indexing
        }
        return 1;
    } else if (strcmp(key, "subscribe") == 0) {
        lua_pushlightuserdata(L, entity);
        lua_pushcclosure(L, _entity_lua_subscribe, 1);
        return 1;
    } else if (strcmp(key, "unsubscribe") == 0) {
        lua_pushlightuserdata(L, entity);
        lua_pushcclosure(L, _entity_lua_unsubscribe, 1);
        return 1;
    }

    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseEntity objects (setter).
 */
static int _entity_lua_newindex(lua_State *L) {
    EseEntity *entity = entity_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);

    // SAFETY: Silently ignore writes to freed entities
    if (!entity) {
        return 0;
    }

    if (entity->destroyed) {
        return 0;
    }

    if (!key)
        return 0;

    if (strcmp(key, "id") == 0) {
        return luaL_error(L, "Entity id is a read-only property");
    } else if (strcmp(key, "active") == 0) {
        if (!lua_isboolean(L, 3)) {
            return luaL_error(L, "Entity active must be a boolean");
        }
        entity->active = lua_toboolean(L, 3);
        return 0;
    } else if (strcmp(key, "visible") == 0) {
        if (!lua_isboolean(L, 3)) {
            return luaL_error(L, "Entity visible must be a boolean");
        }
        entity->visible = lua_toboolean(L, 3);
        return 0;
    } else if (strcmp(key, "persistent") == 0) {
        if (!lua_isboolean(L, 3)) {
            return luaL_error(L, "Entity persistent must be a boolean");
        }
        entity->persistent = lua_toboolean(L, 3);
        return 0;
    } else if (strcmp(key, "draw_order") == 0) {
        if (!lua_isinteger_lj(L, 3)) {
            return luaL_error(L, "Entity draw_order must be an integer");
        }

        int public_z = (int)lua_tointeger(L, 3);
        if (public_z < 0 || (uint64_t)public_z > DRAW_ORDER_MAX_USERZ) {
            return luaL_error(L, "Entity draw_order must be an integer between 0 and %llu",
                              (unsigned long long)DRAW_ORDER_MAX_USERZ);
        }

        entity->draw_order = ((uint64_t)public_z << DRAW_ORDER_SHIFT);
        return 0;
    } else if (strcmp(key, "position") == 0) {
        EsePoint *new_position_point = ese_point_lua_get(L, 3);
        if (!new_position_point) {
            return luaL_error(L, "Entity position must be a EsePoint object");
        }
        // Use entity_set_position to ensure collision bounds are updated
        entity_set_position(entity, ese_point_get_x(new_position_point),
                            ese_point_get_y(new_position_point));
        // Pop the point off the stack
        lua_pop(L, 1);
        return 0;
    } else if (strcmp(key, "bounds") == 0) {
        return luaL_error(L, "Entity components is not assignable");
    } else if (strcmp(key, "world_bounds") == 0) {
        return luaL_error(L, "Entity components is not assignable");
    } else if (strcmp(key, "components") == 0) {
        return luaL_error(L, "Entity components is not assignable");
    } else if (strcmp(key, "data") == 0 || strcmp(key, "__data") == 0) {
        // Allow assignment to data (must be a table)
        if (!lua_istable(L, 3)) {
            return luaL_error(L, "Entity data must be a table");
        }
        lua_pushvalue(L, 3);
        lua_setfenv(L, 1);
        return 0;
    }
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __gc metamethod for EseEntity objects.
 */
static int _entity_lua_gc(lua_State *L) {
    EseEntity *entity = entity_lua_get(L, 1);
    if (!entity)
        return 0;
    if (entity->lua_ref == LUA_NOREF) {
        entity_destroy(entity);
    }
    return 0;
}

static int _entity_lua_tostring(lua_State *L) {
    EseEntity *entity = entity_lua_get(L, 1);

    if (!entity) {
        lua_pushstring(L, "Entity: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Entity: %p (id=%s active=%s components=%zu)", (void *)entity,
             ese_uuid_get_value(entity->id), entity->active ? "true" : "false",
             entity->component_count);
    lua_pushstring(L, buf);

    return 1;
}

/**
 * @brief Helper function to convert a EseLuaValue to Lua stack values
 * recursively.
 *
 * @details Pushes the appropriate Lua value onto the stack based on the
 * EseLuaValue type. For tables, recursively converts all items and creates a
 * Lua table.
 *
 * @param L Lua state pointer
 * @param value Pointer to the EseLuaValue to convert
 *
 * @warning This is an internal helper function - value must not be NULL
 */
static void _lua_value_to_stack(lua_State *L, EseLuaValue *value) {
    switch (value->type) {
    case LUA_VAL_NIL:
        lua_pushnil(L);
        break;

    case LUA_VAL_BOOL:
        lua_pushboolean(L, value->value.boolean);
        break;

    case LUA_VAL_NUMBER:
        lua_pushnumber(L, value->value.number);
        break;

    case LUA_VAL_STRING:
        lua_pushstring(L, value->value.string ? value->value.string : "");
        break;

    case LUA_VAL_TABLE: {
        lua_newtable(L);
        int table_idx = lua_gettop(L);

        // Convert all table items
        for (size_t i = 0; i < value->value.table.count; ++i) {
            EseLuaValue *item = value->value.table.items[i];

            if (item->name) {
                // Named property - use as key-value pair
                _lua_value_to_stack(L, item);
                lua_setfield(L, table_idx, item->name);
            } else {
                // Unnamed property - use as array element
                _lua_value_to_stack(L, item);
                lua_rawseti(L, table_idx, i + 1); // Lua uses 1-based indexing
            }
        }
        break;
    }

    default:
        log_warn("ENTITY", "_lua_value_to_stack: unknown EseLuaValue type %d", value->type);
        lua_pushnil(L);
        break;
    }
}

/**
 * @brief Gets the number of entities in the engine.
 */
static int _entity_lua_get_count(lua_State *L) {
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    if (!engine) {
        return luaL_error(L, "Engine not found");
    }
    lua_pushinteger(L, engine_get_entity_count(engine));
    return 1;
}

/**
 * @brief Lua function to subscribe an entity to a topic.
 */
static int _entity_lua_subscribe(lua_State *L) {
    EseEntity *entity = entity_lua_get(L, 1);
    if (!entity) {
        return luaL_error(L, "Invalid entity");
    }

    if (!lua_isstring(L, 2)) {
        return luaL_error(L, "Event name must be a string");
    }
    if (!lua_isstring(L, 3)) {
        return luaL_error(L, "Function name must be a string");
    }

    const char *event_name = lua_tostring(L, 2);
    const char *function_name = lua_tostring(L, 3);

    // Get engine from registry
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    if (!engine) {
        return luaL_error(L, "Engine not found");
    }

    // Subscribe using entity-based callback
    engine_pubsub_sub(engine, event_name, entity, function_name);

    // Track subscription in entity
    if (!entity->subscriptions) {
        entity->subscriptions = array_create(4, _entity_subscription_free);
    }
    EseEntitySubscription *sub = memory_manager.malloc(sizeof(EseEntitySubscription), MMTAG_ENTITY);
    sub->topic_name = memory_manager.strdup(event_name, MMTAG_ENTITY);
    sub->function_name = memory_manager.strdup(function_name, MMTAG_ENTITY);
    array_push(entity->subscriptions, sub);
    log_verbose("ENTITY", "Added subscription %s.", event_name);

    lua_pushboolean(L, true);
    return 1;
}

/**
 * @brief Lua function to unsubscribe an entity from a topic.
 */
static int _entity_lua_unsubscribe(lua_State *L) {
    EseEntity *entity = entity_lua_get(L, 1);
    if (!entity) {
        return luaL_error(L, "Invalid entity");
    }

    if (!lua_isstring(L, 2)) {
        return luaL_error(L, "Event name must be a string");
    }
    if (!lua_isstring(L, 3)) {
        return luaL_error(L, "Function name must be a string");
    }

    const char *event_name = lua_tostring(L, 2);
    const char *function_name = lua_tostring(L, 3);

    // Get engine from registry
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    if (!engine) {
        return luaL_error(L, "Engine not found");
    }

    // Unsubscribe from pub/sub system
    engine_pubsub_unsub(engine, event_name, entity, function_name);

    // Remove from entity's subscription tracking
    _entity_remove_subscription(entity, event_name, function_name);

    lua_pushboolean(L, true);
    return 1;
}

/**
 * @brief Lua function to publish data to a topic.
 */
static int _entity_lua_publish(lua_State *L) {
    if (!lua_isstring(L, 1)) {
        return luaL_error(L, "Event name must be a string");
    }

    const char *event_name = lua_tostring(L, 1);

    // Get engine from registry
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    if (!engine) {
        return luaL_error(L, "Engine not found");
    }

    // Convert Lua value to EseLuaValue
    EseLuaValue *data = lua_value_from_stack(L, 2);

    // Publish
    engine_pubsub_pub(engine, event_name, data);

    lua_value_destroy(data);
    lua_pushboolean(L, true);
    return 1;
}

/**
 * @brief Helper function to remove subscription from entity tracking.
 */
static void _entity_remove_subscription(EseEntity *entity, const char *topic_name,
                                        const char *function_name) {
    if (!entity->subscriptions)
        return;

    size_t count = array_size(entity->subscriptions);
    for (size_t i = 0; i < count; i++) {
        EseEntitySubscription *sub = array_get(entity->subscriptions, i);
        if (sub && strcmp(sub->topic_name, topic_name) == 0 &&
            strcmp(sub->function_name, function_name) == 0) {
            // Remove from array; the array's free_fn will free the subscription
            array_remove_at(entity->subscriptions, i);
            // If now empty, destroy and reset to NULL to release backing
            // storage
            if (array_size(entity->subscriptions) == 0) {
                array_destroy(entity->subscriptions);
                entity->subscriptions = NULL;
            }
            break;
        }
    }
}

void entity_lua_init(EseLuaEngine *engine) {
    // Create EntityProxyMeta metatable
    lua_engine_new_object_meta(engine, "EntityProxyMeta", _entity_lua_index, _entity_lua_newindex,
                               _entity_lua_gc, _entity_lua_tostring);

    // Create ComponentsProxyMeta metatable
    lua_engine_new_object_meta(engine, "ComponentsProxyMeta", _entity_lua_components_index, NULL,
                               NULL, NULL);

    // Create global Entity table with functions
    const char *keys[] = {"new",        "find_by_tag", "find_first_by_tag",
                          "find_by_id", "count",       "publish"};
    lua_CFunction functions[] = {
        _entity_lua_new,        _entity_lua_find_by_tag, _entity_lua_find_first_by_tag,
        _entity_lua_find_by_id, _entity_lua_get_count,   _entity_lua_publish};
    lua_engine_new_object(engine, "Entity", 6, keys, functions);
}

bool _entity_lua_to_data(EseEntity *entity, EseLuaValue *value) {
    // Get the entity's userdata from registry
    lua_State *L = entity->lua->runtime;
    lua_rawgeti(L, LUA_REGISTRYINDEX, entity->lua_ref);
    if (!lua_isuserdata(L, -1)) {
        lua_pop(L, 1);
        log_error("ENTITY", "entity_add_prop: invalid entity Lua reference "
                            "(expected userdata)");
        return false;
    }

    // Get or create environment table (Lua 5.1)
    lua_getfenv(L, -1);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfenv(L, -3);
    }

    // Convert EseLuaValue to Lua stack value and set property in data table
    _lua_value_to_stack(L, value);
    lua_setfield(L, -2, lua_value_get_name(value));

    // Clean up stack: pop data table and userdata
    lua_pop(L, 2);
    return true;
}
