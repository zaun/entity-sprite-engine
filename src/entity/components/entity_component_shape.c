#include <string.h>
#include "utility/log.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "core/asset_manager.h"
#include "core/engine.h"
#include "entity/entity_private.h"
#include "entity/entity.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component_shape.h"
#include "entity/components/entity_component.h"
#include "entity/components/entity_component_lua.h"
#include "types/poly_line.h"
#include "utility/profile.h"

static void _entity_component_shape_register(EseEntityComponentShape *component, bool is_lua_owned) {
    log_assert("ENTITY_COMP", component, "_entity_component_shape_register called with NULL component");
    log_assert("ENTITY_COMP", component->base.lua_ref == LUA_NOREF, "_entity_component_shape_register component is already registered");

    lua_newtable(component->base.lua->runtime);
    lua_pushlightuserdata(component->base.lua->runtime, component);
    lua_setfield(component->base.lua->runtime, -2, "__ptr");

    // Store the ownership flag
    lua_pushboolean(component->base.lua->runtime, is_lua_owned);
    lua_setfield(component->base.lua->runtime, -2, "__is_lua_owned");

    luaL_getmetatable(component->base.lua->runtime, ENTITY_COMPONENT_SHAPE_PROXY_META);
    lua_setmetatable(component->base.lua->runtime, -2);

    // Store a reference to this proxy table in the Lua registry
    component->base.lua_ref = luaL_ref(component->base.lua->runtime, LUA_REGISTRYINDEX);
}

static EseEntityComponent *_entity_component_shape_make(EseLuaEngine *engine) {
    EseEntityComponentShape *component = memory_manager.malloc(sizeof(EseEntityComponentShape), MMTAG_ENTITY);
    component->base.data = component;
    component->base.active = true;
    component->base.id = ese_uuid_create(engine);
    ese_uuid_ref(component->base.id);
    component->base.lua = engine;
    component->base.lua_ref = LUA_NOREF;
    component->base.type = ENTITY_COMPONENT_SHAPE;

    // Create a new polyline for the shape
    component->polyline = ese_poly_line_create(engine);
    ese_poly_line_ref(component->polyline);

    return &component->base;
}

EseEntityComponent *_entity_component_shape_copy(const EseEntityComponentShape *src) {
    log_assert("ENTITY_COMP", src, "_entity_component_shape_copy called with NULL src");

    EseEntityComponent *copy = _entity_component_shape_make(src->base.lua);
    EseEntityComponentShape *shape_copy = (EseEntityComponentShape *)copy->data;
    
    // Copy the polyline
    ese_poly_line_destroy(shape_copy->polyline);
    shape_copy->polyline = ese_poly_line_copy(src->polyline);
    ese_poly_line_ref(shape_copy->polyline);

    return copy;
}

void _entity_component_shape_destroy(EseEntityComponentShape *component) {
    if (component == NULL) return;

    ese_poly_line_destroy(component->polyline);
    ese_uuid_destroy(component->base.id);
    memory_manager.free(component);
}

/**
 * @brief Lua function to create a new EseEntityComponentShape object.
 * 
 * @details Callable from Lua as EseEntityComponentShape.new().
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new shape object)
 * 
 * @warning Items created in Lua are owned by Lua
 */
static int _entity_component_shape_new(lua_State *L) {
    int n_args = lua_gettop(L);
    if (n_args != 0) {
        log_debug("ENTITY_COMP", "EntityComponentShape.new() - no arguments expected");
    }

    // Set engine reference
    EseLuaEngine *lua = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);

    // Create EseEntityComponent wrapper
    EseEntityComponent *component = _entity_component_shape_make(lua);

    // Push EseEntityComponent to Lua
    _entity_component_shape_register((EseEntityComponentShape *)component->data, true);
    entity_component_push(component);
    
    return 1;
}

EseEntityComponentShape *_entity_component_shape_get(lua_State *L, int idx) {
    // Check if the value at idx is a table
    if (!lua_istable(L, idx)) {
        return NULL;
    }
    
    // Check if it has the correct metatable
    if (!lua_getmetatable(L, idx)) {
        return NULL; // No metatable
    }
    
    // Get the expected metatable for comparison
    luaL_getmetatable(L, ENTITY_COMPONENT_SHAPE_PROXY_META);
    
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
    
    return (EseEntityComponentShape *)comp;
}

/**
 * @brief Lua __index metamethod for EseEntityComponentShape objects (getter).
 * 
 * @details Handles property access for EseEntityComponentShape objects from Lua. 
 * 
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack (1 for valid properties, 0 otherwise)
 */
static int _entity_component_shape_index(lua_State *L) {
    EseEntityComponentShape *component = _entity_component_shape_get(L, 1);
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
    } else if (strcmp(key, "polyline") == 0) {
        // Push the polyline to Lua
        lua_newtable(L);
        lua_pushlightuserdata(L, component->polyline);
        lua_setfield(L, -2, "__ptr");
        lua_pushstring(L, "PolyLineProxyMeta");
        lua_setfield(L, -2, "__name");
        luaL_getmetatable(L, "PolyLineProxyMeta");
        lua_setmetatable(L, -2);
        return 1;
    }
    
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseEntityComponentShape objects (setter).
 * 
 * @details Handles property assignment for EseEntityComponentShape objects from Lua.
 * 
 * @param L Lua state pointer
 * @return Always returns 0 (no return values) or throws Lua error for invalid operations
 */
static int _entity_component_shape_newindex(lua_State *L) {
    EseEntityComponentShape *component = _entity_component_shape_get(L, 1);
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
    } else if (strcmp(key, "polyline") == 0) {
        EsePolyLine *new_polyline = ese_poly_line_lua_get(L, 3);
        if (!new_polyline) {
            return luaL_error(L, "polyline must be a PolyLine object or nil");
        }

        if (component->polyline) {
            ese_poly_line_unref(component->polyline);
        }
        component->polyline = new_polyline;
        ese_poly_line_ref(component->polyline);

        return 0;
    }
    
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __gc metamethod for EseEntityComponentShape objects.
 *
 * @details Checks the '__is_lua_owned' flag in the proxy table. If true,
 * it means this EseEntityComponentShape's memory was allocated by Lua and should be freed.
 * If false, the EseEntityComponentShape's memory is managed externally (by C) and is not freed here.
 *
 * @param L Lua state pointer
 * @return Always 0 (no return values)
 */
static int _entity_component_shape_gc(lua_State *L) {
    EseEntityComponentShape *component = _entity_component_shape_get(L, 1);
    
    if (component) {
        lua_getfield(L, 1, "__is_lua_owned");
        bool is_lua_owned = lua_toboolean(L, -1);
        lua_pop(L, 1);
        
        if (is_lua_owned) {
            _entity_component_shape_destroy(component);
            log_debug("LUA_GC", "EntityComponentShape object (Lua-owned) garbage collected and C memory freed.");
        } else {
            log_debug("LUA_GC", "EntityComponentShape object (C-owned) garbage collected, C memory *not* freed.");
        }
    }
    
    return 0;
}

static int _entity_component_shape_tostring(lua_State *L) {
    EseEntityComponentShape *component = _entity_component_shape_get(L, 1);
    
    if (!component) {
        lua_pushstring(L, "EntityComponentShape: (invalid)");
        return 1;
    }
    
    char buf[128];
    snprintf(buf, sizeof(buf), "EntityComponentShape: %p (id=%s active=%s polyline=%p)", 
             (void*)component,
             ese_uuid_get_value(component->base.id),
             component->base.active ? "true" : "false",
             component->polyline);
    lua_pushstring(L, buf);
    
    return 1;
}

void _entity_component_shape_init(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "_entity_component_shape_init called with NULL engine");

    lua_State *L = engine->runtime;
    
    // Register EntityComponentShape metatable
    if (luaL_newmetatable(L, ENTITY_COMPONENT_SHAPE_PROXY_META)) {
        log_debug("LUA", "Adding EntityComponentShapeProxyMeta to engine");
        lua_pushstring(L, ENTITY_COMPONENT_SHAPE_PROXY_META);
        lua_setfield(L, -2, "__name");
        lua_pushcfunction(L, _entity_component_shape_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, _entity_component_shape_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, _entity_component_shape_gc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, _entity_component_shape_tostring);
        lua_setfield(L, -2, "__tostring");
    }
    lua_pop(L, 1);
    
    // Create global EntityComponentShape table with constructor
    lua_getglobal(L, "EntityComponentShape");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        log_debug("LUA", "Creating global EntityComponentShape table");
        lua_newtable(L);
        lua_pushcfunction(L, _entity_component_shape_new);
        lua_setfield(L, -2, "new");
        lua_setglobal(L, "EntityComponentShape");
    } else {
        lua_pop(L, 1);
    }
}

void _entity_component_shape_draw(EseEntityComponentShape *component, float screen_x, float screen_y, EntityDrawCallbacks *callbacks, void *callback_user_data) {
    log_assert("ENTITY_COMP", component, "_entity_component_shape_draw called with NULL component");
    log_assert("ENTITY_COMP", callbacks, "_entity_component_shape_draw called with NULL callbacks");
    log_assert("ENTITY_COMP", callbacks->draw_polyline, "_entity_component_shape_draw called with NULL draw_polyline callback");

    profile_start(PROFILE_ENTITY_COMP_SHAPE_DRAW);

    // Get the polyline data
    EsePolyLine *polyline = component->polyline;
    if (!polyline) {
        return; // No polyline to draw
    }

    // Get the number of points
    size_t point_count = ese_poly_line_get_point_count(polyline);
    
    if (point_count < 2) {
        return; // Need at least 2 points to draw a line
    }

    // Get the points directly as float array
    const float *points = ese_poly_line_get_points(polyline);

    // Get the stroke width and colors from the polyline
    float stroke_width = ese_poly_line_get_stroke_width(polyline);
    
    // Get color objects and use the existing getter functions
    EseColor *fill_color = ese_poly_line_get_fill_color(polyline);
    EseColor *stroke_color = ese_poly_line_get_stroke_color(polyline);
    
    unsigned char fill_r = (unsigned char)(fill_color ? ese_color_get_r(fill_color) * 255 : 0);
    unsigned char fill_g = (unsigned char)(fill_color ? ese_color_get_g(fill_color) * 255 : 0);
    unsigned char fill_b = (unsigned char)(fill_color ? ese_color_get_b(fill_color) * 255 : 0);
    unsigned char fill_a = (unsigned char)(fill_color ? ese_color_get_a(fill_color) * 255 : 255);
    
    unsigned char stroke_r = (unsigned char)(stroke_color ? ese_color_get_r(stroke_color) * 255 : 0);
    unsigned char stroke_g = (unsigned char)(stroke_color ? ese_color_get_g(stroke_color) * 255 : 0);
    unsigned char stroke_b = (unsigned char)(stroke_color ? ese_color_get_b(stroke_color) * 255 : 0);
    unsigned char stroke_a = (unsigned char)(stroke_color ? ese_color_get_a(stroke_color) * 255 : 255);

    // Draw the polyline using the callback
    callbacks->draw_polyline(
        screen_x, screen_y, 0, // z_index = 0 for now
        points, point_count, stroke_width,
        fill_r, fill_g, fill_b, fill_a,
        stroke_r, stroke_g, stroke_b, stroke_a,
        callback_user_data
    );

    profile_stop(PROFILE_ENTITY_COMP_SHAPE_DRAW, "entity_component_shape_draw");
}

EseEntityComponent *entity_component_shape_create(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "entity_component_shape_create called with NULL engine");
    
    EseEntityComponent *component = _entity_component_shape_make(engine);

    // Push EseEntityComponent to Lua
    _entity_component_shape_register((EseEntityComponentShape *)component->data, false);

    return component;
}
