#include <string.h>
#include <math.h>
#include "utility/log.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "core/asset_manager.h"
#include "core/engine_private.h"
#include "core/engine.h"
#include "entity/entity_private.h"
#include "entity/entity.h"
#include "types/rect.h"
#include "entity/components/entity_component_collider.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component.h"
#include "entity/components/entity_component_lua.h"

#define COLLIDER_RECT_CAPACITY 5

static void _entity_component_collider_rect_changed(EseRect *rect, void *userdata);

static void _entity_component_collider_register(EseEntityComponentCollider *component, bool is_lua_owned) {
    log_assert("ENTITY_COMP", component, "_entity_component_collider_push called with NULL component");
    log_assert("ENTITY_COMP", component->base.lua_ref == LUA_NOREF, "_entity_component_collider_push component is already registered");

    lua_newtable(component->base.lua->runtime);
    lua_pushlightuserdata(component->base.lua->runtime, component);
    lua_setfield(component->base.lua->runtime, -2, "__ptr");

    // Store the ownership flag
    lua_pushboolean(component->base.lua->runtime, is_lua_owned);
    lua_setfield(component->base.lua->runtime, -2, "__is_lua_owned");

    luaL_getmetatable(component->base.lua->runtime, COLLIDER_PROXY_META);
    lua_setmetatable(component->base.lua->runtime, -2);

    // Store a reference to this proxy table in the Lua registry
    component->base.lua_ref = luaL_ref(component->base.lua->runtime, LUA_REGISTRYINDEX);
}

static EseEntityComponent *_entity_component_collider_make(EseLuaEngine *engine) {
    EseEntityComponentCollider *component = memory_manager.malloc(sizeof(EseEntityComponentCollider), MMTAG_ENTITY);
    component->base.data = component;
    component->base.active = true;
    component->base.id = uuid_create(engine);
    uuid_ref(component->base.id);
    component->base.lua = engine;
    component->base.lua_ref = LUA_NOREF;
    component->base.type = ENTITY_COMPONENT_COLLIDER;


    component->rects = memory_manager.malloc(sizeof(EseRect*) * COLLIDER_RECT_CAPACITY, MMTAG_ENTITY);
    component->rects_capacity = COLLIDER_RECT_CAPACITY;
    component->rects_count = 0;
    component->draw_debug = false;

    return &component->base;
}

EseEntityComponent *_entity_component_collider_copy(const EseEntityComponentCollider *src) {
    log_assert("ENTITY_COMP", src, "_entity_component_collider_copy called with NULL src");


    EseEntityComponentCollider *copy = memory_manager.malloc(sizeof(EseEntityComponentCollider), MMTAG_ENTITY);
    copy->base.data = copy;
    copy->base.active = true;
    copy->base.id = uuid_create(src->base.lua);
    uuid_ref(copy->base.id);
    copy->base.lua = src->base.lua;
    copy->base.lua_ref = LUA_NOREF;
    copy->base.type = ENTITY_COMPONENT_COLLIDER;

    // Copy rects
    copy->rects = memory_manager.malloc(sizeof(EseRect*) * src->rects_capacity, MMTAG_ENTITY);
    copy->rects_capacity = src->rects_capacity;
    copy->rects_count = src->rects_count;
    copy->draw_debug = src->draw_debug;

    for (size_t i = 0; i < copy->rects_count; ++i) {
        EseRect *src_comp = src->rects[i];
        EseRect *dst_comp = rect_copy(src_comp);        
        copy->rects[i] = dst_comp;
    }

    return &copy->base;
}

void _entity_component_collider_destroy(EseEntityComponentCollider *component) {
    log_assert("ENTITY_COMP", component, "_entity_component_collider_destroy called with NULL src");

    uuid_destroy(component->base.id);

    for (size_t i = 0; i < component->rects_count; ++i) {
        // Remove watcher before destroying rect
        rect_remove_watcher(component->rects[i], _entity_component_collider_rect_changed, component);
        rect_destroy(component->rects[i]);
    }
    memory_manager.free(component->rects);

    memory_manager.free(component);
}


/**
 * @brief Lua function to create a new EseEntityComponentCollider object.
 * 
 * @details Callable from Lua as EseEntityComponentCollider.new().
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new point object)
 * 
 * @warning Items created in Lua are owned by Lua
 */
static int _entity_component_collider_new(lua_State *L) {
    EseRect *rect = NULL;

    int n_args = lua_gettop(L);
    if (n_args == 1) {
        // The rect parameter is at index 1 (first argument to the function)
        EseRect *rect = rect_lua_get(L, 1);
        if (rect == NULL) {
            luaL_argerror(L, 1, "EntityComponentCollider.new() or EntityComponentCollider.new(Rect)");
            return 0;
        }
    } else if (n_args > 1) {
        luaL_argerror(L, 1, "EntityComponentCollider.new() or EntityComponentCollider.new(Rect)");
        return 0;
    }

    // Set engine reference
    EseLuaEngine *lua = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);

    // Create EseEntityComponent wrapper
    EseEntityComponent *component = _entity_component_collider_make(lua);

    // Push EseEntityComponent to Lua
    _entity_component_collider_register((EseEntityComponentCollider *)component->data, true);
    entity_component_push(component);
    
    if (rect) {
        entity_component_collider_rects_add((EseEntityComponentCollider *)component->data, rect);
    }

    return 1;
}

EseEntityComponentCollider *_entity_component_collider_get(lua_State *L, int idx) {
    // Check if the value at idx is a table
    if (!lua_istable(L, idx)) {
        return NULL;
    }
    
    // Check if it has the correct metatable
    if (!lua_getmetatable(L, idx)) {
        return NULL; // No metatable
    }
    
    // Get the expected metatable for comparison
    luaL_getmetatable(L, COLLIDER_PROXY_META);
    
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
    
    return (EseEntityComponentCollider *)comp;
}

/**
 * @brief Add the passed component to the entity.
 */
static int _entity_component_collider_rects_add(lua_State *L) {
    // Get the collider component from the upvalue
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)lua_touserdata(L, lua_upvalueindex(1));
    
    if (!collider) {
        return luaL_error(L, "Invalid collider component in upvalue.");
    }

    // The rect parameter is at index 1 (first argument to the function)
    EseRect *rect = rect_lua_get(L, 1);
    if (rect == NULL) {
        return luaL_argerror(L, 1, "Expected a Rect argument.");
    }

    // Add the rect to the collider
    entity_component_collider_rects_add(collider, rect);

    // Mark the rect as no longer owned by Lua
    // Note: We need to get the rect's proxy table to set this flag
    lua_getfield(L, 1, "__is_lua_owned");
    if (!lua_isnil(L, -1)) {
        lua_pop(L, 1); // Pop the current value
        lua_pushboolean(L, false);
        lua_setfield(L, 1, "__is_lua_owned");
    } else {
        lua_pop(L, 1); // Pop the nil
    }

    return 0;
}

/**
 * @brief Lua function to remove a component from an entity.
 */
static int _entity_component_collider_rects_remove(lua_State *L) {
    EseEntityComponentCollider *collider = _entity_component_collider_get(L, 1);
    
    if (!collider) {
        return luaL_error(L, "Invalid collider object.");
    }
    
    EseRect *rect_to_remove = rect_lua_get(L, 2);
    if (rect_to_remove == NULL) {
        return luaL_argerror(L, 2, "Expected a Rect object.");
    }

    // Find the rect in the collider's rects array
    int idx = -1;
    for (size_t i = 0; i < collider->rects_count; ++i) {
        if (collider->rects[i] == rect_to_remove) {
            idx = (int)i;
            break;
        }
    }
    
    if (idx < 0) {
        lua_pushboolean(L, false);
        return 1;
    }

    // Remove the watcher before removing the rect
    rect_remove_watcher(rect_to_remove, _entity_component_collider_rect_changed, collider);
    rect_unref(rect_to_remove);
    
    // Shift elements to remove the component
    for (size_t i = idx; i < collider->rects_count - 1; ++i) {
        collider->rects[i] = collider->rects[i+1];
    }
    
    collider->rects_count--;
    collider->rects[collider->rects_count] = NULL;

    // Update entity's collision bounds after removing rect
    entity_component_collider_update_bounds(collider);

    // C no longer owns the memory
    lua_pushstring(L, "__is_lua_owned");
    lua_pushboolean(L, true);
    lua_settable(L, 2);
    
    lua_pushboolean(L, true);
    return 1;
}

/**
 * @brief Lua function to insert a component at a specific index.
 */
static int _entity_component_collider_rects_insert(lua_State *L) {
    EseEntityComponentCollider *collider = _entity_component_collider_get(L, 1);
    
    if (!collider) {
        return luaL_error(L, "Invalid collider object.");
    }
    
    EseRect *rect = rect_lua_get(L, 2);
    if (rect == NULL) {
        return luaL_argerror(L, 2, "Expected a rect object.");
    }
    
    int index = (int)luaL_checkinteger(L, 3) - 1; // Lua is 1-based
    
    if (index < 0 || index > (int)collider->rects_count) {
        return luaL_error(L, "Index out of bounds.");
    }
    
    // Resize array if necessary
    if (collider->rects_count == collider->rects_capacity) {
        size_t new_capacity = collider->rects_capacity * 2;
        EseRect **new_rects = memory_manager.realloc(
            collider->rects, 
            sizeof(EseRect*) * new_capacity, 
            MMTAG_ENTITY
        );
        collider->rects = new_rects;
        collider->rects_capacity = new_capacity;
    }
    
    // Shift elements to make space for the new component
    for (size_t i = collider->rects_count; i > index; --i) {
        collider->rects[i] = collider->rects[i - 1];
    }
    
    collider->rects[index] = rect;
    collider->rects_count++;
    rect_ref(rect);
    
    // Register a watcher to automatically update bounds when rect properties change
    rect_add_watcher(rect, _entity_component_collider_rect_changed, collider);
    
    // Update entity's collision bounds after inserting rect
    entity_component_collider_update_bounds(collider);
    
    lua_pushstring(L, "__is_lua_owned");
    lua_pushboolean(L, false);
    lua_settable(L, 2);
    
    return 0;
}

/**
 * @brief Lua function to remove and return the last component.
 */
static int _entity_component_collider_rects_pop(lua_State *L) {
    EseEntityComponentCollider *collider = _entity_component_collider_get(L, 1);
    
    if (!collider) {
        return luaL_error(L, "Invalid collider object.");
    }
    
    if (collider->rects_count == 0) {
        lua_pushnil(L);
        return 1;
    }
    
    EseRect *rect = collider->rects[collider->rects_count - 1];
    
    // Remove the watcher before removing the rect
    rect_remove_watcher(rect, _entity_component_collider_rect_changed, collider);
    rect_unref(rect);

    collider->rects[collider->rects_count - 1] = NULL;
    collider->rects_count--;
    
    // Update entity's collision bounds after removing rect
    entity_component_collider_update_bounds(collider);
    
    // Get the existing Lua proxy for the rect from its Lua reference
    lua_rawgeti(L, LUA_REGISTRYINDEX, rect_get_lua_ref(rect));    
    lua_pushboolean(L, true);
    lua_setfield(L, -2, "__is_lua_owned");

    return 1;
}

/**
 * @brief Lua function to remove and return the first component.
 */
static int _entity_component_collider_rects_shift(lua_State *L) {
    EseEntityComponentCollider *collider = _entity_component_collider_get(L, 1);
    
    if (!collider) {
        return luaL_error(L, "Invalid collider object.");
    }
    
    if (collider->rects_count == 0) {
        lua_pushnil(L);
        return 1;
    }
    
    EseRect *rect = collider->rects[0];
    
    // Remove the watcher before removing the rect
    rect_remove_watcher(rect, _entity_component_collider_rect_changed, collider);
    rect_unref(rect);

    // Shift all elements
    for (size_t i = 0; i < collider->rects_count - 1; ++i) {
        collider->rects[i] = collider->rects[i + 1];
    }
    
    collider->rects_count--;
    collider->rects[collider->rects_count] = NULL;

    // Update entity's collision bounds after removing rect
    entity_component_collider_update_bounds(collider);
    
    // Get the existing Lua proxy for the rect from its Lua reference
    lua_rawgeti(L, LUA_REGISTRYINDEX, rect_get_lua_ref(rect));    
    lua_pushboolean(L, true);
    lua_setfield(L, -2, "__is_lua_owned");
    
    return 1;
}

/**
 * @brief Lua __index metamethod for EseEntityComponentCollider objects (getter).
 * 
 * @details Handles property access for EseEntityComponentCollider objects from Lua. 
 * 
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack (1 for valid properties, 0 otherwise)
 */
static int _entity_component_collider_index(lua_State *L) {
    EseEntityComponentCollider *component = _entity_component_collider_get(L, 1);
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
        lua_pushstring(L, component->base.id->value);
        return 1;
    } else if (strcmp(key, "draw_debug") == 0) {
        lua_pushboolean(L, component->draw_debug);
        return 1;
    } else if (strcmp(key, "rects") == 0) {
        // Create components proxy table
        lua_newtable(L);
        lua_pushlightuserdata(L, component);
        lua_setfield(L, -2, "__component");
        luaL_getmetatable(L, "ColliderRectsProxyMeta");
        lua_setmetatable(L, -2);
        return 1;
    }
    
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseEntityComponentCollider objects (setter).
 * 
 * @details Handles property assignment for EseEntityComponentCollider objects from Lua.
 * 
 * @param L Lua state pointer
 * @return Always returns 0 (no return values) or throws Lua error for invalid operations
 */
static int _entity_component_collider_newindex(lua_State *L) {
    EseEntityComponentCollider *component = _entity_component_collider_get(L, 1);
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
    } else if (strcmp(key, "draw_debug") == 0) {
        if (!lua_isboolean(L, 3)) {
            return luaL_error(L, "draw_debug must be a boolean");
        }
        component->draw_debug = lua_toboolean(L, 3);
        lua_pushboolean(L, component->draw_debug);
        return 1;
    } else if (strcmp(key, "rects") == 0) {
        return luaL_error(L, "rects is not assignable");
    }
    
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __index metamethod for EseEntityComponentCollider rects collection (getter).
 * 
 * @details Handles property access for EseEntityComponentCollider rects collection from Lua. 
 * 
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack (1 for valid properties, 0 otherwise)
 */
static int _entity_component_collider_rects_rects_index(lua_State *L) {
    // Get the collider component from the proxy table's __component field
    lua_getfield(L, 1, "__component");
    if (!lua_islightuserdata(L, -1)) {
        lua_pop(L, 1);
        lua_pushnil(L);
        return 1;
    }
    
    EseEntityComponentCollider *component = (EseEntityComponentCollider *)lua_touserdata(L, -1);
    lua_pop(L, 1); // Pop the __component value

    if (!component) {
        lua_pushnil(L);
        return 1;
    }
    
    // Check if it's a number (array access)
    if (lua_isnumber(L, 2)) {
        int index = (int)lua_tointeger(L, 2) - 1; // Convert to 0-based
        if (index >= 0 && index < (int)component->rects_count) {
            EseRect *rect = component->rects[index];
            
            // Get the existing Lua proxy for the component from its Lua reference
            lua_rawgeti(L, LUA_REGISTRYINDEX, rect_get_lua_ref(rect));
            return 1;
        } else {
            lua_pushnil(L);
            return 1;
        }
    }
    
    // Check for method names
    const char *key = lua_tostring(L, 2);
    if (!key) return 0;
    
    if (strcmp(key, "count") == 0) {
        lua_pushinteger(L, component->rects_count);
        return 1;
    } else if (strcmp(key, "add") == 0) {
        lua_pushlightuserdata(L, component);
        lua_pushcclosure(L, _entity_component_collider_rects_add, 1);
        return 1;
    } else if (strcmp(key, "remove") == 0) {
        lua_pushcfunction(L, _entity_component_collider_rects_remove);
        return 1;
    } else if (strcmp(key, "insert") == 0) {
        lua_pushcfunction(L, _entity_component_collider_rects_insert);
        return 1;
    } else if (strcmp(key, "pop") == 0) {
        lua_pushcfunction(L, _entity_component_collider_rects_pop);
        return 1;
    } else if (strcmp(key, "shift") == 0) {
        lua_pushcfunction(L, _entity_component_collider_rects_shift);
        return 1;
    }

    return 0;
}

/**
 * @brief Lua __gc metamethod for EseEntityComponentCollider objects.
 *
 * @details Checks the '__is_lua_owned' flag in the proxy table. If true,
 * it means this EseEntityComponentCollider's memory was allocated by Lua and should be freed.
 * If false, the EseEntityComponentCollider's memory is managed externally (by C) and is not freed here.
 *
 * @param L Lua state pointer
 * @return Always 0 (no return values)
 */
static int _entity_component_collider_gc(lua_State *L) {
    EseEntityComponentCollider *component = _entity_component_collider_get(L, 1);
    
    if (component) {
        lua_getfield(L, 1, "__is_lua_owned");
        bool is_lua_owned = lua_toboolean(L, -1);
        lua_pop(L, 1);
        
        if (is_lua_owned) {
            _entity_component_collider_destroy(component);
            log_debug("LUA_GC", "EntityComponentCollider object (Lua-owned) garbage collected and C memory freed.");
        } else {
            log_debug("LUA_GC", "EntityComponentCollider object (C-owned) garbage collected, C memory *not* freed.");
        }
    }
    
    return 0;
}

static int _entity_component_collider_tostring(lua_State *L) {
    EseEntityComponentCollider *component = _entity_component_collider_get(L, 1);
    
    if (!component) {
        lua_pushstring(L, "EntityComponentCollider: (invalid)");
        return 1;
    }
    
    char buf[128];
    snprintf(buf, sizeof(buf), "EntityComponentCollider: %p (id=%s active=%s draw_debug=%s)", 
             (void*)component,
             component->base.id->value,
             component->base.active ? "true" : "false",
             component->draw_debug ? "true" : "false");
    lua_pushstring(L, buf);
    
    return 1;
}

void _entity_component_collider_init(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "_entity_component_collider_init called with NULL engine");

    lua_State *L = engine->runtime;
    
    // Register EseEntityComponentCollider metatable
    if (luaL_newmetatable(L, COLLIDER_PROXY_META)) {
        log_debug("LUA", "Adding EntityComponentColliderProxyMeta to engine");
        lua_pushstring(L, COLLIDER_PROXY_META);
        lua_setfield(L, -2, "__name");
        lua_pushcfunction(L, _entity_component_collider_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, _entity_component_collider_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, _entity_component_collider_gc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, _entity_component_collider_tostring);
        lua_setfield(L, -2, "__tostring");
    }
    lua_pop(L, 1);
    
    // Create global EseEntityComponentCollider table with constructor
    lua_getglobal(L, "EntityComponentCollider");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        log_debug("LUA", "Creating global EseEntityComponentCollider table");
        lua_newtable(L);
        lua_pushcfunction(L, _entity_component_collider_new);
        lua_setfield(L, -2, "new");
        lua_setglobal(L, "EntityComponentCollider");
    } else {
        lua_pop(L, 1);
    }

    if (luaL_newmetatable(engine->runtime, "ColliderRectsProxyMeta")) {
        log_debug("LUA", "Adding entity ColliderRectsProxyMeta to engine");
        lua_pushstring(engine->runtime, "ColliderRectsProxyMeta");
        lua_setfield(engine->runtime, -2, "__name");
        lua_pushcfunction(engine->runtime, _entity_component_collider_rects_rects_index);
        lua_setfield(engine->runtime, -2, "__index");
    }
    lua_pop(engine->runtime, 1);
}

void _entity_component_collider_draw(EseEntityComponentCollider *collider, float screen_x, float screen_y, EntityDrawRectCallback rectCallback, void *callback_user_data) {
    log_assert("ENTITY_COMP", collider, "_entity_component_collider_drawable called with NULL collider");

    if (!collider->draw_debug) {
        return;
    }

    for (size_t i = 0; i < collider->rects_count; i++) {
        EseRect *rect = collider->rects[i];
        rectCallback(
            screen_x, screen_y, collider->base.entity->draw_order,
            rect_get_width(rect), rect_get_height(rect), rect_get_rotation(rect), false,
            0, 0, 255, 255,
            callback_user_data
        );
    }
};

EseEntityComponent *entity_component_collider_create(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "entity_component_collider_create called with NULL engine");
    
    EseEntityComponent *component = _entity_component_collider_make(engine);

    // Push EseEntityComponent to Lua
    _entity_component_collider_register((EseEntityComponentCollider *)component->data, false);

    return component;
}

static void _entity_component_collider_rect_changed(EseRect *rect, void *userdata) {
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)userdata;
    if (collider) {
        entity_component_collider_update_bounds(collider);
    }
}

void entity_component_collider_rects_add(EseEntityComponentCollider *collider, EseRect *rect) {
    log_assert("ENTITY", collider, "entity_component_collider_rects_add called with NULL collider");
    log_assert("ENTITY", rect, "entity_component_collider_rects_add called with NULL rect");

    if (collider->rects_count == collider->rects_capacity) {
        size_t new_capacity = collider->rects_capacity * 2;
        EseRect **new_rects = memory_manager.realloc(
            collider->rects, 
            sizeof(EseRect*) * new_capacity, 
            MMTAG_ENTITY
        );
        collider->rects = new_rects;
        collider->rects_capacity = new_capacity;
    }

    collider->rects[collider->rects_count++] = rect;
    rect_ref(rect);
    
    // Register a watcher to automatically update bounds when rect properties change
    rect_add_watcher(rect, _entity_component_collider_rect_changed, collider);
    
    // Update entity's collision bounds after adding rect
    entity_component_collider_update_bounds(collider);
}

void entity_component_collider_update_bounds(EseEntityComponentCollider *collider) {
    log_assert("ENTITY", collider, "entity_component_collider_update_bounds called with NULL collider");
    
    // If component isn't attached to an entity yet, skip bounds update
    if (!collider->base.entity) {
        return;
    }
    
    if (collider->rects_count == 0) {
        // No rects, clear both collision bounds
        if (collider->base.entity->collision_bounds) {
            rect_destroy(collider->base.entity->collision_bounds);
            collider->base.entity->collision_bounds = NULL;
        }
        if (collider->base.entity->collision_world_bounds) {
            rect_destroy(collider->base.entity->collision_world_bounds);
            collider->base.entity->collision_world_bounds = NULL;
        }
        return;
    }
    
    // Compute bounds from all rects in this collider (relative to entity)
    float min_x = INFINITY, min_y = INFINITY, max_x = -INFINITY, max_y = -INFINITY;
    
    for (size_t i = 0; i < collider->rects_count; i++) {
        EseRect *r = collider->rects[i];
        if (!r) continue;
        
        // Use rect coordinates directly (they're already relative to entity)
        float rx = rect_get_x(r);
        float ry = rect_get_y(r);
        float rw = rect_get_width(r);
        float rh = rect_get_height(r);
        
        // Update min/max bounds
        min_x = fminf(min_x, rx);
        min_y = fminf(min_y, ry);
        max_x = fmaxf(max_x, rx + rw);
        max_y = fmaxf(max_y, ry + rh);
    }
    
    // Create or update the collision bounds (relative to entity)
    if (!collider->base.entity->collision_bounds) {
        collider->base.entity->collision_bounds = rect_create(collider->base.lua);
    }
    
    EseRect *bounds = collider->base.entity->collision_bounds;
    rect_set_x(bounds, min_x);
    rect_set_y(bounds, min_y);
    rect_set_width(bounds, max_x - min_x);
    rect_set_height(bounds, max_y - min_y);
    rect_set_rotation(bounds, 0.0f); // Collision bounds are axis-aligned
    
    // Create or update the collision world bounds
    if (!collider->base.entity->collision_world_bounds) {
        collider->base.entity->collision_world_bounds = rect_create(collider->base.lua);
    }
    
    EseRect *world_bounds = collider->base.entity->collision_world_bounds;
    rect_set_x(world_bounds, min_x + point_get_x(collider->base.entity->position));
    rect_set_y(world_bounds, min_y + point_get_y(collider->base.entity->position));
    rect_set_width(world_bounds, max_x - min_x);
    rect_set_height(world_bounds, max_y - min_y);
    rect_set_rotation(world_bounds, 0.0f); // Collision bounds are axis-aligned
}

void entity_component_collider_rect_updated(EseEntityComponentCollider *collider) {
    log_assert("ENTITY", collider, "entity_component_collider_rect_updated called with NULL collider");
    
    // Simply call the bounds update function
    entity_component_collider_update_bounds(collider);
}

void entity_component_collider_position_changed(EseEntityComponentCollider *collider) {
    log_assert("ENTITY", collider, "entity_component_collider_position_changed called with NULL collider");
    
    // Update bounds since entity position affects all rect world positions
    entity_component_collider_update_bounds(collider);
}

void entity_component_collider_update_world_bounds_only(EseEntityComponentCollider *collider) {
    log_assert("ENTITY", collider, "entity_component_collider_update_world_bounds_only called with NULL collider");
    
    // If component isn't attached to an entity yet, skip bounds update
    if (!collider->base.entity) {
        return;
    }
    
    // If no entity bounds exist, can't update world bounds
    if (!collider->base.entity->collision_bounds) {
        return;
    }
    
    // Update ONLY the world bounds based on current entity position and entity bounds
    if (!collider->base.entity->collision_world_bounds) {
        collider->base.entity->collision_world_bounds = rect_create(collider->base.lua);
    }
    
    EseRect *entity_bounds = collider->base.entity->collision_bounds;
    EseRect *world_bounds = collider->base.entity->collision_world_bounds;
    
    // Copy entity bounds to world bounds and add entity position offset
    rect_set_x(world_bounds, rect_get_x(entity_bounds) + point_get_x(collider->base.entity->position));
    rect_set_y(world_bounds, rect_get_y(entity_bounds) + point_get_y(collider->base.entity->position));
    rect_set_width(world_bounds, rect_get_width(entity_bounds));
    rect_set_height(world_bounds, rect_get_height(entity_bounds));
    rect_set_rotation(world_bounds, rect_get_rotation(entity_bounds));
}

bool entity_component_collider_get_draw_debug(EseEntityComponentCollider *collider) {
    log_assert("ENTITY_COMP", collider, "entity_component_collider_get_draw_debug called with NULL collider");
    
    return collider->draw_debug;
}

void entity_component_collider_set_draw_debug(EseEntityComponentCollider *collider, bool draw_debug) {
    log_assert("ENTITY_COMP", collider, "entity_component_collider_set_draw_debug called with NULL collider");
    
    collider->draw_debug = draw_debug;
}
