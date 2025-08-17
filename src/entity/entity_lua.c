#include <string.h>
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "types/types.h"
#include "entity/components/entity_component_private.h"
#include "core/engine.h"
#include "scripting/lua_engine_private.h"
#include "scripting/lua_value.h"
#include "vendor/lua/src/lua.h"
#include "vendor/lua/src/lauxlib.h"
#include "vendor/lua/src/lualib.h"
#include "core/memory_manager.h"
#include "entity_lua.h"
#include "entity_private.h"
#include "entity.h"

/**
 * @brief Register an EseEntity pointer as a Lua object.
 */
void _entity_lua_register(EseEntity *entity, bool is_lua_owned) {
    log_assert("ENTITY", entity, "_entity_lua_register called with NULL entity");
    log_assert("ENTITY", entity->lua_ref == LUA_NOREF, "_entity_lua_register entity is already registered");

    lua_newtable(entity->lua->runtime);
    lua_pushlightuserdata(entity->lua->runtime, entity);
    lua_setfield(entity->lua->runtime, -2, "__ptr");

    // Store the ownership flag
    lua_pushboolean(entity->lua->runtime, is_lua_owned);
    lua_setfield(entity->lua->runtime, -2, "__is_lua_owned");

    luaL_getmetatable(entity->lua->runtime, "EntityProxyMeta");
    lua_setmetatable(entity->lua->runtime, -2);

    // Store a reference to this proxy table in the Lua registry
    entity->lua_ref = luaL_ref(entity->lua->runtime, LUA_REGISTRYINDEX);
    lua_value_set_ref(entity->lua_val_ref, entity->lua_ref);
}

void entity_lua_push(EseEntity *entity) {
    log_assert("ENTITY", entity, "entity_lua_push called with NULL entity");
    log_assert("ENTITY", entity->lua_ref != LUA_NOREF, "entity_lua_push entity not registered with lua");

    // Push the proxy table back onto the stack for Lua to receive
    lua_rawgeti(entity->lua->runtime, LUA_REGISTRYINDEX, entity->lua_ref);
}

/**
 * @brief Extracts an EseEntity pointer from a Lua userdata object with type safety.
 */
EseEntity *entity_lua_get(lua_State *L, int idx) {
    if (!lua_istable(L, idx)) {
        log_debug("ENTITY", "Not a table");
        return NULL;
    }
    
    if (!lua_getmetatable(L, idx)) {
        log_debug("ENTITY", "Metatable get failed");
        return NULL;
    }
    
    luaL_getmetatable(L, "EntityProxyMeta");
    
    if (!lua_rawequal(L, -1, -2)) {
        log_debug("ENTITY", "Not a EntityProxyMeta");
        lua_pop(L, 2);
        return NULL;
    }
    
    lua_pop(L, 2);
    
    lua_getfield(L, idx, "__ptr");
    
    if (!lua_islightuserdata(L, -1)) {
        log_debug("ENTITY", "Missing __ptr");
        lua_pop(L, 1);
        return NULL;
    }
    
    void *entity = lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    return (EseEntity *)entity;
}

/**
 * @brief Lua function to create a new EseEntity object.
 */
static int _entity_lua_new(lua_State *L) {
    EseLuaEngine *lua = (EseLuaEngine *)lua_touserdata(L, lua_upvalueindex(1));
    EseEntity *entity = _entity_make(lua);

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    engine_add_entity(engine, entity);

    // always C-owned: GC of the Lua proxy will NOT free the C object
    _entity_lua_register(entity, false);
    entity_lua_push(entity);
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
    // Initial stack: [component]
    EseEntity *entity = (EseEntity *)lua_touserdata(L, lua_upvalueindex(1));

    EseEntityComponent *comp = entity_component_get(L);
    if (comp == NULL) {
        luaL_argerror(L, 1, "Expected a component argument not found.");
        lua_pushboolean(L, false);
        // Stack: [entity, component, false]
        return 1;
    }

    entity_component_add(entity, comp);

    // LUA no longer owns the memory
    lua_pushstring(L, "__is_lua_owned");
    // Stack: [component, "__is_lua_owned"]
    lua_pushboolean(L, false);
    // Stack: [component, "__is_lua_owned", false]
    lua_settable(L, 1);
    // Stack: [component]
    
    lua_pushboolean(L, true);
    // Stack: [component, true]
    return 1;
}

/**
 * @brief Lua function to remove a component from an entity.
 */
static int _entity_lua_components_remove(lua_State *L) {
    // Initial stack: [entity, component]
    EseEntity *entity = _entity_lua_components_get_entity(L, 1);
    
    if (!entity) {
        luaL_error(L, "Invalid entity object.");
        lua_pushboolean(L, false);
        // Stack: [entity, component, false]
        return 1;
    }
    
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
        entity->components[i] = entity->components[i+1];
    }
    
    entity->component_count--;
    entity->components[entity->component_count] = NULL;

    // C no longer owns the memory.
    lua_pushstring(L, "__is_lua_owned");
    // Stack: [entity, component, "__is_lua_owned"]
    lua_pushboolean(L, true);
    // Stack: [entity, component, "__is_lua_owned", true]
    lua_settable(L, 2);
    // Stack: [entity, component]
    
    lua_pushboolean(L, true);
    // Stack: [entity, component, true]
    return 1;
}

/**
 * @brief Lua function to insert a component at a specific index.
 */
static int _entity_lua_components_insert(lua_State *L) {
    // Initial stack: [entity, component, index]
    EseEntity *entity = _entity_lua_components_get_entity(L, 1);
    
    if (!entity) {
        lua_warning(L, "Invalid entity object.", false);
        lua_pushboolean(L, false);
        // Stack: [entity, component, index, false]
        return 1;
    }
    
    EseEntityComponent *comp = entity_component_get(L);
    if (comp == NULL) {
        luaL_argerror(L, 2, "Expected a component object.");
        lua_pushboolean(L, false);
        // Stack: [entity, component, index, false]
        return 1;
    }
    
    int index = (int)luaL_checkinteger(L, 3) - 1; // Lua is 1-based
    
    if (index < 0 || index > (int)entity->component_count) {
        lua_warning(L, "Index out of bounds.", false);
        lua_pushboolean(L, false);
        // Stack: [entity, component, index, false]
        return 1;
    }
    
    // Resize array if necessary
    if (entity->component_count == entity->component_capacity) {
        size_t new_capacity = entity->component_capacity * 2;
        EseEntityComponent **new_components = memory_manager.realloc(
            entity->components, 
            sizeof(EseEntityComponent*) * new_capacity, 
            MMTAG_ENTITY
        );
        entity->components = new_components;
        entity->component_capacity = new_capacity;
    }
    
    // Shift elements to make space for the new component
    for (size_t i = entity->component_count; i > index; --i) {
        entity->components[i] = entity->components[i - 1];
    }
    
    entity->components[index] = comp;
    entity->component_count++;
    
    // The component is now owned by C
    lua_pushstring(L, "__is_lua_owned");
    // Stack: [entity, component, index, "__is_lua_owned"]
    lua_pushboolean(L, false);
    // Stack: [entity, component, index, "__is_lua_owned", false]
    lua_settable(L, 2);
    // Stack: [entity, component, index]
    
    lua_pushboolean(L, true);
    // Stack: [entity, component, index, true]
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
    entity->components[entity->component_count - 1] = NULL;
    entity->component_count--;
    
    // Get the existing Lua proxy for the component from its Lua reference
    lua_rawgeti(L, LUA_REGISTRYINDEX, comp->lua_ref);
    // Stack: [entity_proxy, component_proxy]
    
    // Set the ownership flag on the returned component proxy
    lua_pushboolean(L, true);
    // Stack: [entity_proxy, component_proxy, true]
    lua_setfield(L, -2, "__is_lua_owned");
    // Stack: [entity_proxy, component_proxy]

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
    
    // Shift all elements
    for (size_t i = 0; i < entity->component_count - 1; ++i) {
        entity->components[i] = entity->components[i + 1];
    }
    
    entity->component_count--;
    entity->components[entity->component_count] = NULL;
    
    // Get the existing Lua proxy for the component from its Lua reference
    lua_rawgeti(L, LUA_REGISTRYINDEX, comp->lua_ref);
    // Stack: [entity_proxy, component_proxy]
    
    // Set the ownership flag on the returned component proxy
    lua_pushboolean(L, true);
    // Stack: [entity_proxy, component_proxy, true]
    lua_setfield(L, -2, "__is_lua_owned");
    // Stack: [entity_proxy, component_proxy]
    
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
            // Stack: [..., result_table, component_proxy, metatable, __name_string]
            
            if (lua_isstring(L, -1) && strcmp(lua_tostring(L, -1), full_metatable_name) == 0) {
                lua_pop(L, 2); // Pop name and metatable
                // Stack: [..., result_table, component_proxy]
                
                lua_pushinteger(L, i + 1);
                // Stack: [..., result_table, component_proxy, index_integer]
                lua_rawseti(L, result_table_idx, table_index++);
                // Stack: [..., result_table, component_proxy] (index_integer is popped)
            } else {
                lua_pop(L, 2); // Pop name and metatable
                // Stack: [..., result_table, component_proxy]
            }
        }
        lua_pop(L, 1); // Pop component proxy
        // Stack: [..., result_table]
    }
    
    // Stack: [entity_proxy, comp_type_name_string, result_table]
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
    
    // Assuming _entity_component_find_index finds the component in the array by ID.
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
    
    return 1;
}

// Function to convert a Lua value on the stack to an EseLuaValue struct
static EseLuaValue _convert_lua_value_to_ese_lua_value(lua_State *L, int index) {
    EseLuaValue result;
    result.type = LUA_VAL_NIL;

    int arg_type = lua_type(L, index);
    switch (arg_type) {
        case LUA_TNUMBER:
            lua_value_set_number(&result, lua_tonumber(L, index));
            break;
        case LUA_TBOOLEAN:
            lua_value_set_bool(&result, lua_toboolean(L, index));
            break;
        case LUA_TSTRING:
            lua_value_set_string(&result, lua_tostring(L, index));
            break;
        case LUA_TUSERDATA: {
            void *udata = lua_touserdata(L, index);
            lua_value_set_userdata(&result, udata);
            break;
        }
        case LUA_TTABLE: {
            // Create a temporary table EseLuaValue
            EseLuaValue *table_value = lua_value_create_table(NULL);
            if (table_value) {
                lua_pushnil(L);
                while (lua_next(L, index) != 0) {
                    const char *key_str = NULL;
                    int key_type = lua_type(L, -2);
                    
                    if (key_type == LUA_TSTRING) {
                        key_str = lua_tostring(L, -2);
                    } else if (key_type == LUA_TNUMBER) {
                        char num_str[64];
                        snprintf(num_str, sizeof(num_str), "%g", lua_tonumber(L, -2));
                        key_str = num_str;
                    }
                    
                    EseLuaValue *item_ptr = lua_value_create_nil(key_str);
                    if (item_ptr) {
                        *item_ptr = _convert_lua_value_to_ese_lua_value(L, -1);
                        lua_value_push(table_value, item_ptr, false);
                    }
                    lua_pop(L, 1);
                }
                result = *table_value;
                memory_manager.free(table_value);
            }
            break;
        }
        default:
            lua_value_set_nil(&result);
            break;
    }
    return result;
}

// Returns true if the function is run, false if the function is not found or run
static int _entity_lua_dispatch(lua_State *L) {
    EseEntity *entity = (EseEntity*)lua_touserdata(L, lua_upvalueindex(1));
    if (!entity) {
        lua_pushboolean(L, false);
        return 1;
    }

    // The function name is now at stack index 1.
    const char *func_name = luaL_checkstring(L, 1);
    // The number of arguments is the total number of arguments minus one for the function name.
    int argc = lua_gettop(L) - 1;
    EseLuaValue *argv = NULL;

    if (argc > 0) {
        argv = memory_manager.calloc(argc, sizeof(EseLuaValue), MMTAG_LUA);
        if (!argv) {
            lua_pushboolean(L, false);
            return 1;
        }

        for (int i = 0; i < argc; ++i) {
            // The Lua arguments start at index 2.
            int lua_arg_index = i + 2;
            argv[i] = _convert_lua_value_to_ese_lua_value(L, lua_arg_index);
        }
    }

    entity_run_function_with_args(entity, func_name, argc, argv);

    if (argv) {
        for (int i = 0; i < argc; ++i) {
            lua_value_free(&argv[i]);
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
    
    // Check if the key is a number for array access
    if (lua_isnumber(L, 2)) {
        int index = (int)lua_tointeger(L, 2) - 1; // Convert to 0-based
        if (index >= 0 && index < (int)entity->component_count) {
            EseEntityComponent *comp = entity->components[index];
            
            // Get the existing Lua proxy for the component from its Lua reference
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
    if (!key) return 0;
    
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
    
    if (!key) return 0;

    if (strcmp(key, "id") == 0) {
        lua_pushstring(L, entity->id->value);
        return 1;
    } else if (strcmp(key, "active") == 0) {
        lua_pushboolean(L, entity->active);
        return 1;
    } else if (strcmp(key, "draw_order") == 0) {
        lua_pushinteger(L, entity->draw_order);
        return 1;
    } else if (strcmp(key, "position") == 0) {
        if (entity->position != NULL && entity->position->lua_ref != LUA_NOREF) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, entity->position->lua_ref);
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
    } else if (strcmp(key, "data") == 0) {
        // Check if script_data table already exists
        lua_getfield(L, 1, "__data");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setfield(L, 1, "__data");
        }
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
    
    if (!key) return 0;

    if (strcmp(key, "id") == 0) {
        return luaL_error(L, "Entity id is a read-only property");
    } else if (strcmp(key, "active") == 0) {
        if (!lua_isboolean(L, 3)) {
            return luaL_error(L, "Entity active must be a boolean");
        }
        entity->active = lua_toboolean(L, 3);
        return 0;
    } else if (strcmp(key, "draw_order") == 0) {
        if (!lua_isinteger(L, 3)) {
            return luaL_error(L, "Entity draw_order must be an integer");
        }
        entity->draw_order = (int)lua_tointeger(L, 3);
        return 0;
    } else if (strcmp(key, "position") == 0) {
        EsePoint *new_position_point = point_lua_get(L, 3);
        if (!new_position_point) {
            return luaL_error(L, "Entity position must be a EsePoint object");
        }
        // Copy values, don't copy reference (ownership safety)
        entity->position->x = new_position_point->x;
        entity->position->y = new_position_point->y;
        return 0;
    } else if (strcmp(key, "components") == 0) {
        return luaL_error(L, "Entity components is not assignable");
    } else if (strcmp(key, "data") == 0) {
        // Allow assignment to data (must be a table)
        if (!lua_istable(L, 3)) {
            return luaL_error(L, "Entity data must be a table");
        }
        lua_setfield(L, 1, "__data");
        return 0;
    } else if (strcmp(key, "__data") == 0) {
        // Allow internal assignment for script_data
        lua_rawset(L, 1);
        return 0;
    }
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __gc metamethod for EseEntity objects.
 */
static int _entity_lua_gc(lua_State *L) {
    EseEntity *entity = entity_lua_get(L, 1);

    if (entity) {
        lua_getfield(L, 1, "__is_lua_owned");
        bool is_lua_owned = lua_toboolean(L, -1);
        lua_pop(L, 1);

        if (is_lua_owned) {
            entity_destroy(entity);
            log_debug("LUA_GC", "Entity object (Lua-owned) garbage collected and C memory freed.");
        } else {
            log_debug("LUA_GC", "Entity object (C-owned) garbage collected, C memory *not* freed.");
        }
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
    snprintf(buf, sizeof(buf), "Entity: %p (id=%s active=%s components=%zu)", (void*)entity, entity->id->value, entity->active ? "true" : "false", entity->component_count);
    lua_pushstring(L, buf);

    return 1;
}

/**
 * @brief Helper function to convert a EseLuaValue to Lua stack values recursively.
 * 
 * @details Pushes the appropriate Lua value onto the stack based on the EseLuaValue type.
 *          For tables, recursively converts all items and creates a Lua table.
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

void entity_lua_init(EseLuaEngine *engine) {
    if (luaL_newmetatable(engine->runtime, "EntityProxyMeta")) {
        log_debug("LUA", "Adding entity EntityProxyMeta to engine");
        lua_pushcfunction(engine->runtime, _entity_lua_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _entity_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushcfunction(engine->runtime, _entity_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");
        lua_pushcfunction(engine->runtime, _entity_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);

    if (luaL_newmetatable(engine->runtime, "ComponentsProxyMeta")) {
        log_debug("LUA", "Adding entity ComponentsProxyMeta to engine");
        lua_pushcfunction(engine->runtime, _entity_lua_components_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
    
    // Create global EseEntity table with constructor
    lua_getglobal(engine->runtime, "Entity");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);
        log_debug("LUA", "Creating global EseEntity table");
        lua_newtable(engine->runtime);
        lua_pushlightuserdata(engine->runtime, engine);
        lua_pushcclosure(engine->runtime, _entity_lua_new, 1);
        lua_setfield(engine->runtime, -2, "new");
        lua_setglobal(engine->runtime, "Entity");
    } else {
        lua_pop(engine->runtime, 1);
    }
}

bool _entity_lua_to_data(EseEntity *entity, EseLuaValue *value) {
    // Get the entity's proxy table from registry
    lua_State *L = entity->lua->runtime;
    lua_rawgeti(L, LUA_REGISTRYINDEX, entity->lua_ref);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        log_error("ENTITY", "entity_add_prop: invalid entity Lua reference");
        return false;
    }
    
    // Get the __data table from the entity proxy table
    lua_getfield(L, -1, "__data");
    if (!lua_istable(L, -1)) {
        // If the __data table doesn't exist, create it.
        lua_pop(L, 1); // Pop the nil value
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "__data"); 
    }

    // Convert EseLuaValue to Lua stack value and set the property inside the __data table
    _lua_value_to_stack(L, value);
    lua_setfield(L, -2, lua_value_get_name(value));

    // Clean up stack
    lua_pop(L, 2);
    return true;
}
