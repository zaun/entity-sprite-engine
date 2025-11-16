#include "entity/components/entity_component_shape.h"
#include "core/asset_manager.h"
#include "core/engine.h"
#include "core/memory_manager.h"
#include "entity/components/entity_component.h"
#include "entity/components/entity_component_lua.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component_shape_path.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "scripting/lua_engine.h"
#include "types/color.h"
#include "types/poly_line.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SHAPE_POLYLINE_CAPACITY 4

static EseEntityComponentShape *_entity_component_shape_polylines_get_component(lua_State *L,
                                                                                int idx) {
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    EseEntityComponentShape **ud =
        (EseEntityComponentShape **)luaL_testudata(L, idx, "ShapePolylinesProxyMeta");
    if (!ud) {
        return NULL;
    }
    return *ud;
}

// Lua collection methods for polylines
static int _entity_component_shape_polylines_add(lua_State *L) {
    EseEntityComponentShape *shape =
        (EseEntityComponentShape *)lua_touserdata(L, lua_upvalueindex(1));
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
            shape->polylines, sizeof(EsePolyLine *) * new_capacity, MMTAG_ENTITY);
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
            shape->polylines, sizeof(EsePolyLine *) * new_capacity, MMTAG_ENTITY);
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
    if (!key)
        return 0;

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
static EseEntityComponent *_shape_vtable_copy(EseEntityComponent *component) {
    return _entity_component_shape_copy((EseEntityComponentShape *)component->data);
}

static void _shape_vtable_destroy(EseEntityComponent *component) {
    _entity_component_shape_destroy((EseEntityComponentShape *)component->data);
}

static bool _shape_vtable_run_function(EseEntityComponent *component, EseEntity *entity,
                                       const char *func_name, int argc, void *argv[]) {
    // Shape components don't support function execution
    return false;
}

static void _shape_vtable_collides_component(EseEntityComponent *a, EseEntityComponent *b,
                                             EseArray *out_hits) {
    (void)a;
    (void)b;
    (void)out_hits;
}

static void _shape_vtable_ref(EseEntityComponent *component) {
    EseEntityComponentShape *shape = (EseEntityComponentShape *)component->data;
    log_assert("ENTITY_COMP", shape, "shape vtable ref called with NULL");
    if (shape->base.lua_ref == LUA_NOREF) {
        EseEntityComponentShape **ud = (EseEntityComponentShape **)lua_newuserdata(
            shape->base.lua->runtime, sizeof(EseEntityComponentShape *));
        *ud = shape;
        luaL_getmetatable(shape->base.lua->runtime, ENTITY_COMPONENT_SHAPE_PROXY_META);
        lua_setmetatable(shape->base.lua->runtime, -2);
        shape->base.lua_ref = luaL_ref(shape->base.lua->runtime, LUA_REGISTRYINDEX);
        shape->base.lua_ref_count = 1;
    } else {
        shape->base.lua_ref_count++;
    }
}

static void _shape_vtable_unref(EseEntityComponent *component) {
    EseEntityComponentShape *shape = (EseEntityComponentShape *)component->data;
    if (!shape)
        return;
    if (shape->base.lua_ref != LUA_NOREF && shape->base.lua_ref_count > 0) {
        shape->base.lua_ref_count--;
        if (shape->base.lua_ref_count == 0) {
            luaL_unref(shape->base.lua->runtime, LUA_REGISTRYINDEX, shape->base.lua_ref);
            shape->base.lua_ref = LUA_NOREF;
        }
    }
}

// Static vtable instance for shape components
static const ComponentVTable shape_vtable = {.copy = _shape_vtable_copy,
                                             .destroy = _shape_vtable_destroy,
                                             .run_function = _shape_vtable_run_function,
                                             .collides = _shape_vtable_collides_component,
                                             .ref = _shape_vtable_ref,
                                             .unref = _shape_vtable_unref};

// Helper function to convert degrees to radians
static float _degrees_to_radians(float degrees) { return degrees * M_PI / 180.0f; }

// Helper function to rotate a point around the origin
static void _rotate_point(float *x, float *y, float angle_radians) {
    float cos_angle = cosf(angle_radians);
    float sin_angle = sinf(angle_radians);

    float new_x = *x * cos_angle - *y * sin_angle;
    float new_y = *x * sin_angle + *y * cos_angle;

    *x = new_x;
    *y = new_y;
}

static void _entity_component_shape_register(EseEntityComponentShape *component,
                                             bool is_lua_owned) {
    log_assert("ENTITY_COMP", component,
               "_entity_component_shape_register called with NULL component");
    log_assert("ENTITY_COMP", component->base.lua_ref == LUA_NOREF,
               "_entity_component_shape_register component is already registered");

    // Use the ref system to register userdata in the registry
    _shape_vtable_ref(&component->base);
}

static EseEntityComponent *_entity_component_shape_make(EseLuaEngine *engine) {
    EseEntityComponentShape *component =
        memory_manager.malloc(sizeof(EseEntityComponentShape), MMTAG_ENTITY);
    component->base.data = component;
    component->base.active = true;
    component->base.id = ese_uuid_create(engine);
    component->base.lua = engine;
    component->base.lua_ref = LUA_NOREF;
    component->base.lua_ref_count = 0;
    component->base.type = ENTITY_COMPONENT_SHAPE;
    component->base.vtable = &shape_vtable;

    component->rotation = 0.0f;
    component->polylines =
        memory_manager.malloc(sizeof(EsePolyLine *) * SHAPE_POLYLINE_CAPACITY, MMTAG_ENTITY);
    component->polylines_capacity = SHAPE_POLYLINE_CAPACITY;
    component->polylines_count = 0;

    return &component->base;
}

EseEntityComponent *_entity_component_shape_copy(const EseEntityComponentShape *src) {
    log_assert("ENTITY_COMP", src, "_entity_component_shape_copy called with NULL src");

    EseEntityComponent *copy = _entity_component_shape_make(src->base.lua);
    EseEntityComponentShape *shape_copy = (EseEntityComponentShape *)copy->data;

    // Copy rotation
    shape_copy->rotation = ((EseEntityComponentShape *)src)->rotation;

    // Copy polylines array
    memory_manager.free(shape_copy->polylines);
    shape_copy->polylines =
        memory_manager.malloc(sizeof(EsePolyLine *) * src->polylines_capacity, MMTAG_ENTITY);
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

void _entity_component_ese_shape_cleanup(EseEntityComponentShape *component) {
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
    if (component == NULL)
        return;

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

    // For Lua-created components, create userdata without storing a persistent
    // ref
    EseEntityComponentShape **ud =
        (EseEntityComponentShape **)lua_newuserdata(L, sizeof(EseEntityComponentShape *));
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
    EseEntityComponentShape **ud =
        (EseEntityComponentShape **)luaL_testudata(L, idx, ENTITY_COMPONENT_SHAPE_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }

    return *ud;
}

static int _entity_component_shape_clear_path(lua_State *L) {
    EseEntityComponentShape *component = _entity_component_shape_get(L, 1);
    if (!component) {
        return luaL_error(L, "Invalid shape component.");
    }

    // Clear existing polylines: unref and reset count
    for (size_t i = 0; i < component->polylines_count; ++i) {
        if (component->polylines[i]) {
            ese_poly_line_unref(component->polylines[i]);
            component->polylines[i] = NULL;
        }
    }
    component->polylines_count = 0;

    lua_pushboolean(L, true);
    return 1;
}

// Lua method: set_path(path)
// Parses an SVG path string and replaces the component's polylines with the
// result
static int _entity_component_shape_set_path(lua_State *L) {
    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 2 && argc != 3) {
        return luaL_error(L, "component:set_path(string[, table]) takes 1 or 2 arguments");
    }

    EseEntityComponentShape *component = _entity_component_shape_get(L, 1);
    if (!component) {
        return luaL_error(L, "Invalid shape component.");
    }

    const char *path = luaL_checkstring(L, 2);
    if (!path) {
        return luaL_error(L, "Invalid path string.");
    }

    // Defaults
    float scale = 1.0f;
    float stroke_width = 1.0f;
    bool has_fill_option = false;

    // Optional options table
    if (argc == 3) {
        if (lua_istable(L, 3)) {
            // scale
            lua_getfield(L, 3, "scale");
            if (lua_isnumber(L, -1)) {
                scale = (float)lua_tonumber(L, -1);
            }
            lua_pop(L, 1);

            // stroke_width (support common misspelling 'stroek_width')
            lua_getfield(L, 3, "stroke_width");
            if (!lua_isnumber(L, -1)) {
                lua_pop(L, 1);
                lua_getfield(L, 3, "stroek_width");
            }
            if (lua_isnumber(L, -1)) {
                stroke_width = (float)lua_tonumber(L, -1);
            }
            lua_pop(L, 1);

            // Presence of fill_color flag (we will read actual value later
            // per-polyline)
            lua_getfield(L, 3, "fill_color");
            if (!lua_isnil(L, -1) && !lua_isnone(L, -1)) {
                has_fill_option = true;
            }
            lua_pop(L, 1);
        } else {
            return luaL_error(L, "component:set_path expects options table as 3rd arg");
        }
    }

    size_t new_count = 0;
    EsePolyLine **lines = shape_path_to_polylines(component->base.lua, scale, path, &new_count);
    if (!lines && new_count == 0) {
        lua_pushboolean(L, 0);
        return 1;
    }

    // Pre-fetch color templates from options (do not attach directly; we'll
    // copy per polyline)
    EseColor *opt_stroke_template = NULL;
    EseColor *opt_fill_template = NULL;
    bool destroy_stroke_template = false;
    bool destroy_fill_template = false;

    if (argc == 3) {
        // stroke_color or 'stoke_color' (typo support)
        lua_getfield(L, 3, "stroke_color");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 3, "stoke_color");
        }
        if (lua_isstring(L, -1)) {
            const char *hex = lua_tostring(L, -1);
            opt_stroke_template = ese_color_create(component->base.lua);
            ese_color_set_hex(opt_stroke_template, hex);
            destroy_stroke_template = true;
        } else if (lua_isuserdata(L, -1)) {
            opt_stroke_template = ese_color_lua_get(L, -1);
            // Do not ref or destroy here; we'll copy later
        }
        lua_pop(L, 1);

        // fill_color
        lua_getfield(L, 3, "fill_color");
        if (lua_isstring(L, -1)) {
            const char *hex = lua_tostring(L, -1);
            opt_fill_template = ese_color_create(component->base.lua);
            ese_color_set_hex(opt_fill_template, hex);
            destroy_fill_template = true;
        } else if (lua_isuserdata(L, -1)) {
            opt_fill_template = ese_color_lua_get(L, -1);
            // Do not ref or destroy here; we'll copy later
        }
        lua_pop(L, 1);
    }

    // Adopt new polylines, growing capacity as needed, and apply options
    for (size_t i = 0; i < new_count; ++i) {
        if (component->polylines_count == component->polylines_capacity) {
            size_t new_capacity = component->polylines_capacity * 2;
            EsePolyLine **new_arr = memory_manager.realloc(
                component->polylines, sizeof(EsePolyLine *) * new_capacity, MMTAG_ENTITY);
            component->polylines = new_arr;
            component->polylines_capacity = new_capacity;
        }

        EsePolyLine *pl = lines[i];

        // Stroke width
        ese_poly_line_set_stroke_width(pl, stroke_width);

        // Stroke color: copy template or create default white per polyline
        EseColor *stroke_to_set = NULL;
        if (opt_stroke_template) {
            stroke_to_set = ese_color_copy(opt_stroke_template);
        } else {
            stroke_to_set = ese_color_create(component->base.lua);
            ese_color_set_byte(stroke_to_set, 255, 255, 255,
                               255); // default white
        }
        ese_poly_line_set_stroke_color(pl, stroke_to_set);

        // Fill color: copy template or create default transparent per polyline
        EseColor *fill_to_set = NULL;
        if (opt_fill_template) {
            fill_to_set = ese_color_copy(opt_fill_template);
        } else {
            fill_to_set = ese_color_create(component->base.lua);
            ese_color_set_byte(fill_to_set, 0, 0, 0, 0); // default transparent
        }
        ese_poly_line_set_fill_color(pl, fill_to_set);

        // Adjust type: if path contained Z (CLOSED) and options included
        // fill_color, mark as FILLED
        if (has_fill_option && ese_poly_line_get_type(pl) == POLY_LINE_CLOSED) {
            ese_poly_line_set_type(pl, POLY_LINE_FILLED);
        }

        ese_poly_line_ref(pl);
        component->polylines[component->polylines_count++] = pl;
    }

    if (destroy_stroke_template && opt_stroke_template) {
        ese_color_destroy(opt_stroke_template);
    }
    if (destroy_fill_template && opt_fill_template) {
        ese_color_destroy(opt_fill_template);
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
 * @details Handles property access for EseEntityComponentShape objects from
 * Lua.
 *
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack (1 for valid properties,
 * 0 otherwise)
 */
static int _entity_component_shape_index(lua_State *L) {
    EseEntityComponentShape *component = _entity_component_shape_get(L, 1);
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
    } else if (strcmp(key, "rotation") == 0) {
        lua_pushnumber(L, component->rotation);
        return 1;
    } else if (strcmp(key, "polylines") == 0) {
        // Create polylines proxy userdata
        EseEntityComponentShape **ud =
            (EseEntityComponentShape **)lua_newuserdata(L, sizeof(EseEntityComponentShape *));
        *ud = component;
        luaL_getmetatable(L, "ShapePolylinesProxyMeta");
        lua_setmetatable(L, -2);
        return 1;
    } else if (strcmp(key, "set_path") == 0) {
        lua_pushcfunction(L, _entity_component_shape_set_path);
        return 1;
    } else if (strcmp(key, "clear_path") == 0) {
        lua_pushcfunction(L, _entity_component_shape_clear_path);
        return 1;
    }

    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseEntityComponentShape objects
 * (setter).
 *
 * @details Handles property assignment for EseEntityComponentShape objects from
 * Lua.
 *
 * @param L Lua state pointer
 * @return Always returns 0 (no return values) or throws Lua error for invalid
 * operations
 */
static int _entity_component_shape_newindex(lua_State *L) {
    EseEntityComponentShape *component = _entity_component_shape_get(L, 1);
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
    EseEntityComponentShape **ud =
        (EseEntityComponentShape **)luaL_testudata(L, 1, ENTITY_COMPONENT_SHAPE_PROXY_META);
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
             (void *)component, ese_uuid_get_value(component->base.id),
             component->base.active ? "true" : "false", component->polylines_count);
    lua_pushstring(L, buf);

    return 1;
}

void _entity_component_shape_init(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "_entity_component_shape_init called with NULL engine");

    // Create main metatable
    lua_engine_new_object_meta(engine, ENTITY_COMPONENT_SHAPE_PROXY_META,
                               _entity_component_shape_index, _entity_component_shape_newindex,
                               _entity_component_shape_gc, _entity_component_shape_tostring);

    // Create global EntityComponentShape table with functions
    const char *keys[] = {"new"};
    lua_CFunction functions[] = {_entity_component_shape_new};
    lua_engine_new_object(engine, "EntityComponentShape", 1, keys, functions);

    // Create ShapePolylinesProxyMeta metatable
    extern int _entity_component_shape_polylines_index(lua_State * L);
    lua_engine_new_object_meta(engine, "ShapePolylinesProxyMeta",
                               _entity_component_shape_polylines_index, NULL, NULL, NULL);
}

EseEntityComponent *entity_component_shape_create(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "entity_component_shape_create called with NULL engine");

    EseEntityComponent *component = _entity_component_shape_make(engine);

    // Register with Lua using ref system
    component->vtable->ref(component);

    return component;
}
