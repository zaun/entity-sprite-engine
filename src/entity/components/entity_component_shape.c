#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utility/log.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "core/asset_manager.h"
#include "core/engine.h"
#include "entity/entity_private.h"
#include "entity/entity.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component_shape.h"
#include "entity/components/entity_component_shape_path.h"
#include "entity/components/entity_component.h"
#include "entity/components/entity_component_lua.h"
#include "types/poly_line.h"
#include "utility/profile.h"

#define SHAPE_POLYLINE_CAPACITY 4

static EseEntityComponentShape *_entity_component_shape_polylines_get_component(lua_State *L, int idx) {
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    EseEntityComponentShape **ud = (EseEntityComponentShape **)luaL_testudata(L, idx, "ShapePolylinesProxyMeta");
    if (!ud) {
        return NULL;
    }
    return *ud;
}

// Lua collection methods for polylines
static int _entity_component_shape_polylines_add(lua_State *L) {
    EseEntityComponentShape *shape = (EseEntityComponentShape *)lua_touserdata(L, lua_upvalueindex(1));
    if (!shape) {
        return luaL_error(L, "Invalid shape component in upvalue.");
    }

    int n_args = lua_gettop(L);
    EsePolyLine *pl = NULL;
    if (n_args == 2) {
        pl = ese_poly_line_lua_get(L, 2);
    } else if (n_args == 1) {
        pl = ese_poly_line_lua_get(L, 1);
    } else {
        return luaL_argerror(L, 1, "Expected a PolyLine argument.");
    }

    if (pl == NULL) {
        return luaL_argerror(L, (n_args == 2 ? 2 : 1), "Expected a PolyLine argument.");
    }

    if (shape->polylines_count == shape->polylines_capacity) {
        size_t new_capacity = shape->polylines_capacity * 2;
        EsePolyLine **new_arr = memory_manager.realloc(
            shape->polylines,
            sizeof(EsePolyLine*) * new_capacity,
            MMTAG_ENTITY
        );
        shape->polylines = new_arr;
        shape->polylines_capacity = new_capacity;
    }

    shape->polylines[shape->polylines_count++] = pl;
    ese_poly_line_ref(pl);
    return 0;
}

static int _entity_component_shape_polylines_remove(lua_State *L) {
    EseEntityComponentShape *shape = _entity_component_shape_polylines_get_component(L, 1);
    if (!shape) {
        return luaL_error(L, "Invalid shape object.");
    }

    EsePolyLine *pl = ese_poly_line_lua_get(L, 2);
    if (pl == NULL) {
        return luaL_argerror(L, 2, "Expected a PolyLine object.");
    }

    int idx = -1;
    for (size_t i = 0; i < shape->polylines_count; ++i) {
        if (shape->polylines[i] == pl) {
            idx = (int)i;
            break;
        }
    }

    if (idx < 0) {
        lua_pushboolean(L, false);
        return 1;
    }

    ese_poly_line_unref(pl);
    for (size_t i = idx; i < shape->polylines_count - 1; ++i) {
        shape->polylines[i] = shape->polylines[i + 1];
    }
    shape->polylines_count--;
    shape->polylines[shape->polylines_count] = NULL;

    lua_pushboolean(L, true);
    return 1;
}

static int _entity_component_shape_polylines_insert(lua_State *L) {
    EseEntityComponentShape *shape = _entity_component_shape_polylines_get_component(L, 1);
    if (!shape) {
        return luaL_error(L, "Invalid shape object.");
    }

    EsePolyLine *pl = ese_poly_line_lua_get(L, 2);
    if (pl == NULL) {
        return luaL_argerror(L, 2, "Expected a PolyLine object.");
    }

    int index = (int)luaL_checkinteger(L, 3) - 1;
    if (index < 0 || index > (int)shape->polylines_count) {
        return luaL_error(L, "Index out of bounds.");
    }

    if (shape->polylines_count == shape->polylines_capacity) {
        size_t new_capacity = shape->polylines_capacity * 2;
        EsePolyLine **new_arr = memory_manager.realloc(
            shape->polylines,
            sizeof(EsePolyLine*) * new_capacity,
            MMTAG_ENTITY
        );
        shape->polylines = new_arr;
        shape->polylines_capacity = new_capacity;
    }

    for (size_t i = shape->polylines_count; i > (size_t)index; --i) {
        shape->polylines[i] = shape->polylines[i - 1];
    }

    shape->polylines[index] = pl;
    shape->polylines_count++;
    ese_poly_line_ref(pl);
    return 0;
}

static int _entity_component_shape_polylines_pop(lua_State *L) {
    EseEntityComponentShape *shape = _entity_component_shape_polylines_get_component(L, 1);
    if (!shape) {
        return luaL_error(L, "Invalid shape object.");
    }

    if (shape->polylines_count == 0) {
        lua_pushnil(L);
        return 1;
    }

    EsePolyLine *pl = shape->polylines[shape->polylines_count - 1];
    ese_poly_line_unref(pl);
    shape->polylines[shape->polylines_count - 1] = NULL;
    shape->polylines_count--;

    ese_poly_line_lua_push(pl);
    return 1;
}

static int _entity_component_shape_polylines_shift(lua_State *L) {
    EseEntityComponentShape *shape = _entity_component_shape_polylines_get_component(L, 1);
    if (!shape) {
        return luaL_error(L, "Invalid shape object.");
    }

    if (shape->polylines_count == 0) {
        lua_pushnil(L);
        return 1;
    }

    EsePolyLine *pl = shape->polylines[0];
    ese_poly_line_unref(pl);
    for (size_t i = 0; i < shape->polylines_count - 1; ++i) {
        shape->polylines[i] = shape->polylines[i + 1];
    }
    shape->polylines_count--;
    shape->polylines[shape->polylines_count] = NULL;

    ese_poly_line_lua_push(pl);
    return 1;
}

int _entity_component_shape_polylines_index(lua_State *L) {
    EseEntityComponentShape *component = _entity_component_shape_polylines_get_component(L, 1);
    if (!component) {
        lua_pushnil(L);
        return 1;
    }

    if (lua_isnumber(L, 2)) {
        int index = (int)lua_tointeger(L, 2) - 1;
        if (index >= 0 && index < (int)component->polylines_count) {
            EsePolyLine *pl = component->polylines[index];
            ese_poly_line_lua_push(pl);
            return 1;
        } else {
            lua_pushnil(L);
            return 1;
        }
    }

    const char *key = lua_tostring(L, 2);
    if (!key) return 0;

    if (strcmp(key, "count") == 0) {
        lua_pushinteger(L, component->polylines_count);
        return 1;
    } else if (strcmp(key, "add") == 0) {
        lua_pushlightuserdata(L, component);
        lua_pushcclosure(L, _entity_component_shape_polylines_add, 1);
        return 1;
    } else if (strcmp(key, "remove") == 0) {
        lua_pushcfunction(L, _entity_component_shape_polylines_remove);
        return 1;
    } else if (strcmp(key, "insert") == 0) {
        lua_pushcfunction(L, _entity_component_shape_polylines_insert);
        return 1;
    } else if (strcmp(key, "pop") == 0) {
        lua_pushcfunction(L, _entity_component_shape_polylines_pop);
        return 1;
    } else if (strcmp(key, "shift") == 0) {
        lua_pushcfunction(L, _entity_component_shape_polylines_shift);
        return 1;
    }

    return 0;
}
// VTable wrapper functions
static EseEntityComponent* _shape_vtable_copy(EseEntityComponent* component) {
    return _entity_component_shape_copy((EseEntityComponentShape*)component->data);
}

static void _shape_vtable_destroy(EseEntityComponent* component) {
    _entity_component_shape_destroy((EseEntityComponentShape*)component->data);
}

static void _shape_vtable_update(EseEntityComponent* component, EseEntity* entity, float delta_time) {
    // Shape components don't have update functionality
}

static void _shape_vtable_draw(EseEntityComponent* component, int screen_x, int screen_y, void* callbacks, void* user_data) {
    EntityDrawCallbacks* draw_callbacks = (EntityDrawCallbacks*)callbacks;
    _entity_component_shape_draw((EseEntityComponentShape*)component->data, screen_x, screen_y, draw_callbacks, user_data);
}

static bool _shape_vtable_run_function(EseEntityComponent* component, EseEntity* entity, const char* func_name, int argc, void* argv[]) {
    // Shape components don't support function execution
    return false;
}

static void _shape_vtable_ref(EseEntityComponent* component) {
    EseEntityComponentShape *shape = (EseEntityComponentShape*)component->data;
    log_assert("ENTITY_COMP", shape, "shape vtable ref called with NULL");
    if (shape->base.lua_ref == LUA_NOREF) {
        EseEntityComponentShape **ud = (EseEntityComponentShape **)lua_newuserdata(shape->base.lua->runtime, sizeof(EseEntityComponentShape *));
        *ud = shape;
        luaL_getmetatable(shape->base.lua->runtime, ENTITY_COMPONENT_SHAPE_PROXY_META);
        lua_setmetatable(shape->base.lua->runtime, -2);
        shape->base.lua_ref = luaL_ref(shape->base.lua->runtime, LUA_REGISTRYINDEX);
        shape->base.lua_ref_count = 1;
    } else {
        shape->base.lua_ref_count++;
    }
}

static void _shape_vtable_unref(EseEntityComponent* component) {
    EseEntityComponentShape *shape = (EseEntityComponentShape*)component->data;
    if (!shape) return;
    if (shape->base.lua_ref != LUA_NOREF && shape->base.lua_ref_count > 0) {
        shape->base.lua_ref_count--;
        if (shape->base.lua_ref_count == 0) {
            luaL_unref(shape->base.lua->runtime, LUA_REGISTRYINDEX, shape->base.lua_ref);
            shape->base.lua_ref = LUA_NOREF;
        }
    }
}

// Static vtable instance for shape components
static const ComponentVTable shape_vtable = {
    .copy = _shape_vtable_copy,
    .destroy = _shape_vtable_destroy,
    .update = _shape_vtable_update,
    .draw = _shape_vtable_draw,
    .run_function = _shape_vtable_run_function,
    .ref = _shape_vtable_ref,
    .unref = _shape_vtable_unref
};

// Helper function to convert degrees to radians
static float _degrees_to_radians(float degrees) {
    return degrees * M_PI / 180.0f;
}

// Helper function to rotate a point around the origin
static void _rotate_point(float *x, float *y, float angle_radians) {
    float cos_angle = cosf(angle_radians);
    float sin_angle = sinf(angle_radians);
    
    float new_x = *x * cos_angle - *y * sin_angle;
    float new_y = *x * sin_angle + *y * cos_angle;
    
    *x = new_x;
    *y = new_y;
}

static void _entity_component_shape_register(EseEntityComponentShape *component, bool is_lua_owned) {
    log_assert("ENTITY_COMP", component, "_entity_component_shape_register called with NULL component");
    log_assert("ENTITY_COMP", component->base.lua_ref == LUA_NOREF, "_entity_component_shape_register component is already registered");

    // Use the ref system to register userdata in the registry
    _shape_vtable_ref(&component->base);
}

static EseEntityComponent *_entity_component_shape_make(EseLuaEngine *engine) {
    EseEntityComponentShape *component = memory_manager.malloc(sizeof(EseEntityComponentShape), MMTAG_ENTITY);
    component->base.data = component;
    component->base.active = true;
    component->base.id = ese_uuid_create(engine);
    component->base.lua = engine;
    component->base.lua_ref = LUA_NOREF;
    component->base.lua_ref_count = 0;
    component->base.type = ENTITY_COMPONENT_SHAPE;
    component->base.vtable = &shape_vtable;
    
    component->rotation = 0.0f;
    component->polylines = memory_manager.malloc(sizeof(EsePolyLine*) * SHAPE_POLYLINE_CAPACITY, MMTAG_ENTITY);
    component->polylines_capacity = SHAPE_POLYLINE_CAPACITY;
    component->polylines_count = 0;

    return &component->base;
}

EseEntityComponent *_entity_component_shape_copy(const EseEntityComponentShape *src) {
    log_assert("ENTITY_COMP", src, "_entity_component_shape_copy called with NULL src");

    EseEntityComponent *copy = _entity_component_shape_make(src->base.lua);
    EseEntityComponentShape *shape_copy = (EseEntityComponentShape *)copy->data;
    
    // Copy rotation
    shape_copy->rotation = ((EseEntityComponentShape*)src)->rotation;

    // Copy polylines array
    memory_manager.free(shape_copy->polylines);
    shape_copy->polylines = memory_manager.malloc(sizeof(EsePolyLine*) * src->polylines_capacity, MMTAG_ENTITY);
    shape_copy->polylines_capacity = src->polylines_capacity;
    shape_copy->polylines_count = src->polylines_count;

    for (size_t i = 0; i < shape_copy->polylines_count; ++i) {
        EsePolyLine *src_pl = src->polylines[i];
        EsePolyLine *dst_pl = ese_poly_line_copy(src_pl);
        ese_poly_line_ref(dst_pl);
        shape_copy->polylines[i] = dst_pl;
    }

    return copy;
}

void _entity_component_ese_shape_cleanup(EseEntityComponentShape *component)
{
    for (size_t i = 0; i < component->polylines_count; ++i) {
        ese_poly_line_unref(component->polylines[i]);
        ese_poly_line_destroy(component->polylines[i]);
    }
    if (component->polylines) {
        memory_manager.free(component->polylines);
        component->polylines = NULL;
    }
    ese_uuid_destroy(component->base.id);
    memory_manager.free(component);
    profile_count_add("entity_comp_shape_destroy_count");
}


void _entity_component_shape_destroy(EseEntityComponentShape *component) {
    if (component == NULL) return;

    // Unref the component to clean up Lua references
    if (component->base.lua_ref != LUA_NOREF && component->base.lua_ref_count > 0) {
        component->base.lua_ref_count--;
        if (component->base.lua_ref_count == 0) {
            luaL_unref(component->base.lua->runtime, LUA_REGISTRYINDEX, component->base.lua_ref);
            component->base.lua_ref = LUA_NOREF;
            _entity_component_ese_shape_cleanup(component);
        } else {
            // We dont own the component so dont free it
            return;
        }
    } else if (component->base.lua_ref == LUA_NOREF) {
        _entity_component_ese_shape_cleanup(component);
    }
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

    // For Lua-created components, create userdata without storing a persistent ref
    EseEntityComponentShape **ud = (EseEntityComponentShape **)lua_newuserdata(L, sizeof(EseEntityComponentShape *));
    *ud = (EseEntityComponentShape *)component->data;
    luaL_getmetatable(L, ENTITY_COMPONENT_SHAPE_PROXY_META);
    lua_setmetatable(L, -2);
    
    return 1;
}

EseEntityComponentShape *_entity_component_shape_get(lua_State *L, int idx) {
    // Check if it's userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseEntityComponentShape **ud = (EseEntityComponentShape **)luaL_testudata(L, idx, ENTITY_COMPONENT_SHAPE_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

// Lua method: set_path(path)
// Parses an SVG path string and replaces the component's polylines with the result
static int _entity_component_shape_set_path(lua_State *L) {
    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 2 && argc != 3) {
        return luaL_error(L, "component:set_path(string[, number]) takes 1 or 2 arguments");
    }
    
    EseEntityComponentShape *component = _entity_component_shape_get(L, 1);
    if (!component) {
        return luaL_error(L, "Invalid shape component.");
    }

    const char *path = luaL_checkstring(L, 2);
    if (!path) {
        return luaL_error(L, "Invalid path string.");
    }

    float scale = 1.0f;
    if (argc == 3) {
        scale = (float)luaL_checknumber(L, 3);
    }

    size_t new_count = 0;
    EsePolyLine **lines = shape_path_to_polylines(component->base.lua, scale, path, &new_count);
    if (!lines && new_count == 0) {
        lua_pushboolean(L, 0);
        return 1;
    }

    // Clear existing polylines: unref and reset count
    for (size_t i = 0; i < component->polylines_count; ++i) {
        if (component->polylines[i]) {
            ese_poly_line_unref(component->polylines[i]);
            component->polylines[i] = NULL;
        }
    }
    component->polylines_count = 0;

    // Adopt new polylines, growing capacity as needed
    for (size_t i = 0; i < new_count; ++i) {
        if (component->polylines_count == component->polylines_capacity) {
            size_t new_capacity = component->polylines_capacity * 2;
            EsePolyLine **new_arr = memory_manager.realloc(
                component->polylines,
                sizeof(EsePolyLine*) * new_capacity,
                MMTAG_ENTITY
            );
            component->polylines = new_arr;
            component->polylines_capacity = new_capacity;
        }

        EsePolyLine *pl = lines[i];
        ese_poly_line_ref(pl);
        component->polylines[component->polylines_count++] = pl;
    }

    if (lines) {
        free(lines);
    }

    lua_pushboolean(L, 1);
    return 1;
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
    } else if (strcmp(key, "rotation") == 0) {
        lua_pushnumber(L, component->rotation);
        return 1;
    } else if (strcmp(key, "polylines") == 0) {
        // Create polylines proxy userdata
        EseEntityComponentShape **ud = (EseEntityComponentShape **)lua_newuserdata(L, sizeof(EseEntityComponentShape *));
        *ud = component;
        luaL_getmetatable(L, "ShapePolylinesProxyMeta");
        lua_setmetatable(L, -2);
        return 1;
    } else if (strcmp(key, "set_path") == 0) {
        lua_pushcfunction(L, _entity_component_shape_set_path);
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
    } else if (strcmp(key, "rotation") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            return luaL_error(L, "polyline.rotation must be a number");
        }

        float rotation = lua_tonumber(L, 3);
        if (rotation < 0) {
            rotation = 360 + rotation;
        } else if (rotation > 360) {
            rotation = rotation - 360;
        }
        component->rotation = rotation;

        lua_pushnumber(L, component->rotation);
        return 1;
    }
    
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __gc metamethod for EseEntityComponentShape objects.
 *
 * @param L Lua state pointer
 * @return Always 0 (no return values)
 */
static int _entity_component_shape_gc(lua_State *L) {
    // Get from userdata
    EseEntityComponentShape **ud = (EseEntityComponentShape **)luaL_testudata(L, 1, ENTITY_COMPONENT_SHAPE_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseEntityComponentShape *component = *ud;
    if (component) {
        if (component->base.lua_ref == LUA_NOREF) {
            _entity_component_shape_destroy(component);
            *ud = NULL;
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
    snprintf(buf, sizeof(buf), "EntityComponentShape: %p (id=%s active=%s polylines=%zu)", 
             (void*)component,
             ese_uuid_get_value(component->base.id),
             component->base.active ? "true" : "false",
             component->polylines_count);
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

    // Register polylines proxy metatable
    if (luaL_newmetatable(engine->runtime, "ShapePolylinesProxyMeta")) {
        log_debug("LUA", "Adding ShapePolylinesProxyMeta to engine");
        lua_pushstring(engine->runtime, "ShapePolylinesProxyMeta");
        lua_setfield(engine->runtime, -2, "__name");
        extern int _entity_component_shape_polylines_index(lua_State *L);
        lua_pushcfunction(engine->runtime, _entity_component_shape_polylines_index);
        lua_setfield(engine->runtime, -2, "__index");
    }
    lua_pop(engine->runtime, 1);
}

void _entity_component_shape_draw(EseEntityComponentShape *component, float screen_x, float screen_y, EntityDrawCallbacks *callbacks, void *callback_user_data) {
    log_assert("ENTITY_COMP", component, "_entity_component_shape_draw called with NULL component");
    log_assert("ENTITY_COMP", callbacks, "_entity_component_shape_draw called with NULL callbacks");
    log_assert("ENTITY_COMP", callbacks->draw_polyline, "_entity_component_shape_draw called with NULL draw_polyline callback");

    profile_start(PROFILE_ENTITY_COMP_SHAPE_DRAW);

    // Convert rotation from degrees to radians
    float rotation_radians = _degrees_to_radians(component->rotation);

    for (size_t idx = 0; idx < component->polylines_count; ++idx) {
        EsePolyLine *polyline = component->polylines[idx];
        if (!polyline) continue;

        size_t point_count = ese_poly_line_get_point_count(polyline);
        if (point_count < 2) continue;

        EsePolyLineType polyline_type = ese_poly_line_get_type(polyline);
        float stroke_width = ese_poly_line_get_stroke_width(polyline);

        float *points_to_use = NULL;
        size_t point_count_to_use = point_count;
        bool needs_cleanup = false;

        if ((polyline_type == POLY_LINE_CLOSED || polyline_type == POLY_LINE_FILLED) && point_count >= 3) {
            points_to_use = memory_manager.malloc(sizeof(float) * (point_count + 1) * 2, MMTAG_ENTITY);
            if (points_to_use) {
                const float *original_points = ese_poly_line_get_points(polyline);
                for (size_t i = 0; i < point_count; i++) {
                    float x = original_points[i * 2];
                    float y = original_points[i * 2 + 1];
                    if (rotation_radians != 0.0f) {
                        _rotate_point(&x, &y, rotation_radians);
                    }
                    points_to_use[i * 2] = x;
                    points_to_use[i * 2 + 1] = y;
                }
                float x = original_points[0];
                float y = original_points[1];
                if (rotation_radians != 0.0f) {
                    _rotate_point(&x, &y, rotation_radians);
                }
                points_to_use[point_count * 2] = x;
                points_to_use[point_count * 2 + 1] = y;
                point_count_to_use = point_count + 1;
                needs_cleanup = true;
            } else {
                points_to_use = memory_manager.malloc(sizeof(float) * point_count * 2, MMTAG_ENTITY);
                if (points_to_use) {
                    const float *original_points = ese_poly_line_get_points(polyline);
                    for (size_t i = 0; i < point_count; i++) {
                        float x = original_points[i * 2];
                        float y = original_points[i * 2 + 1];
                        if (rotation_radians != 0.0f) {
                            _rotate_point(&x, &y, rotation_radians);
                        }
                        points_to_use[i * 2] = x;
                        points_to_use[i * 2 + 1] = y;
                    }
                    needs_cleanup = true;
                } else {
                    points_to_use = (float*)ese_poly_line_get_points(polyline);
                }
            }
        } else {
            points_to_use = memory_manager.malloc(sizeof(float) * point_count * 2, MMTAG_ENTITY);
            if (points_to_use) {
                const float *original_points = ese_poly_line_get_points(polyline);
                for (size_t i = 0; i < point_count; i++) {
                    float x = original_points[i * 2];
                    float y = original_points[i * 2 + 1];
                    if (rotation_radians != 0.0f) {
                        _rotate_point(&x, &y, rotation_radians);
                    }
                    points_to_use[i * 2] = x;
                    points_to_use[i * 2 + 1] = y;
                }
                needs_cleanup = true;
            } else {
                points_to_use = (float*)ese_poly_line_get_points(polyline);
            }
        }

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

        bool should_draw_fill = false;
        bool should_draw_stroke = false;

        switch (polyline_type) {
            case POLY_LINE_OPEN:
                should_draw_stroke = true;
                break;
            case POLY_LINE_CLOSED:
                should_draw_stroke = true;
                break;
            case POLY_LINE_FILLED:
                should_draw_fill = true;
                should_draw_stroke = true;
                break;
        }

        if (!should_draw_fill) fill_a = 0;
        if (!should_draw_stroke) stroke_a = 0;

        callbacks->draw_polyline(
            screen_x, screen_y, 0,
            points_to_use, point_count_to_use, stroke_width,
            fill_r, fill_g, fill_b, fill_a,
            stroke_r, stroke_g, stroke_b, stroke_a,
            callback_user_data
        );

        if (needs_cleanup) {
            memory_manager.free(points_to_use);
        }
    }

    profile_stop(PROFILE_ENTITY_COMP_SHAPE_DRAW, "entity_component_shape_draw");
}

EseEntityComponent *entity_component_shape_create(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "entity_component_shape_create called with NULL engine");
    
    EseEntityComponent *component = _entity_component_shape_make(engine);

    // Register with Lua using ref system
    component->vtable->ref(component);

    return component;
}
