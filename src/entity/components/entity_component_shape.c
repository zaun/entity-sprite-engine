#include <string.h>
#include <math.h>
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
    component->polyline = ese_poly_line_create(engine);
    ese_poly_line_ref(component->polyline);

    return &component->base;
}

EseEntityComponent *_entity_component_shape_copy(const EseEntityComponentShape *src) {
    log_assert("ENTITY_COMP", src, "_entity_component_shape_copy called with NULL src");

    EseEntityComponent *copy = _entity_component_shape_make(src->base.lua);
    EseEntityComponentShape *shape_copy = (EseEntityComponentShape *)copy->data;
    
    // Replace the initially created polyline: unref then destroy to free immediately
    ese_poly_line_unref(shape_copy->polyline);
    ese_poly_line_destroy(shape_copy->polyline);
    shape_copy->polyline = ese_poly_line_copy(src->polyline);
    ese_poly_line_ref(shape_copy->polyline);

    return copy;
}

void _entity_component_ese_shape_cleanup(EseEntityComponentShape *component)
{
    ese_poly_line_unref(component->polyline);
    ese_poly_line_destroy(component->polyline);
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
            // We dont "own" the polyline so dont free it
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
    } else if (strcmp(key, "polyline") == 0) {
        // Push the polyline via its Lua proxy
        ese_poly_line_lua_push(component->polyline);
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
    } else if (strcmp(key, "polyline") == 0) {
        EsePolyLine *new_polyline = ese_poly_line_lua_get(L, 3);
        if (!new_polyline) {
            return luaL_error(L, "polyline must be a PolyLine object or nil");
        }

        if (component->polyline) {
            EsePolyLine *old_pl = component->polyline;
            ese_poly_line_unref(old_pl);
            ese_poly_line_destroy(old_pl);
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

    // Get the polyline type and other properties
    EsePolyLineType polyline_type = ese_poly_line_get_type(polyline);
    float stroke_width = ese_poly_line_get_stroke_width(polyline);
    
    // Convert rotation from degrees to radians
    float rotation_radians = _degrees_to_radians(component->rotation);
    
    // Handle auto-closing for CLOSED and FILLED types and offset by screen position
    float *points_to_use = NULL;
    size_t point_count_to_use = point_count;
    bool needs_cleanup = false;
    
    if ((polyline_type == POLY_LINE_CLOSED || polyline_type == POLY_LINE_FILLED) && point_count >= 3) {
        // Need to close the polyline by adding the first point again
        points_to_use = memory_manager.malloc(sizeof(float) * (point_count + 1) * 2, MMTAG_ENTITY);
        if (points_to_use) {
            // Copy existing points (don't offset by screen position - render system handles this)
            const float *original_points = ese_poly_line_get_points(polyline);
            for (size_t i = 0; i < point_count; i++) {
                float x = original_points[i * 2];
                float y = original_points[i * 2 + 1];
                
                // Apply rotation if not zero
                if (rotation_radians != 0.0f) {
                    _rotate_point(&x, &y, rotation_radians);
                }
                
                points_to_use[i * 2] = x;
                points_to_use[i * 2 + 1] = y;
            }
            
            // Add the first point again to close the shape
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
            // Fallback: create points from original
            points_to_use = memory_manager.malloc(sizeof(float) * point_count * 2, MMTAG_ENTITY);
            if (points_to_use) {
                const float *original_points = ese_poly_line_get_points(polyline);
                for (size_t i = 0; i < point_count; i++) {
                    float x = original_points[i * 2];
                    float y = original_points[i * 2 + 1];
                    
                    // Apply rotation if not zero
                    if (rotation_radians != 0.0f) {
                        _rotate_point(&x, &y, rotation_radians);
                    }
                    
                    points_to_use[i * 2] = x;
                    points_to_use[i * 2 + 1] = y;
                }
                needs_cleanup = true;
            } else {
                // Final fallback to original points
                points_to_use = (float*)ese_poly_line_get_points(polyline);
            }
        }
    } else {
        // Use original points for OPEN type or insufficient points
        points_to_use = memory_manager.malloc(sizeof(float) * point_count * 2, MMTAG_ENTITY);
        if (points_to_use) {
            const float *original_points = ese_poly_line_get_points(polyline);
            for (size_t i = 0; i < point_count; i++) {
                float x = original_points[i * 2];
                float y = original_points[i * 2 + 1];
                
                // Apply rotation if not zero
                if (rotation_radians != 0.0f) {
                    _rotate_point(&x, &y, rotation_radians);
                }
                
                points_to_use[i * 2] = x;
                points_to_use[i * 2 + 1] = y;
            }
            needs_cleanup = true;
        } else {
            // Final fallback to original points
            points_to_use = (float*)ese_poly_line_get_points(polyline);
        }
    }
    
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
    

    // Determine what to draw based on polyline type
    bool should_draw_fill = false;
    bool should_draw_stroke = false;
    
    switch (polyline_type) {
        case POLY_LINE_OPEN:
            // Only draw stroke for open polylines
            should_draw_stroke = true;
            break;
        case POLY_LINE_CLOSED:
            // Only draw stroke for closed polylines
            should_draw_stroke = true;
            break;
        case POLY_LINE_FILLED:
            // Draw both fill and stroke for filled polylines
            should_draw_fill = true;
            should_draw_stroke = true;
            break;
    }
    
    // Set fill alpha to 0 if we shouldn't draw fill
    if (!should_draw_fill) {
        fill_a = 0;
    }
    
    // Set stroke alpha to 0 if we shouldn't draw stroke
    if (!should_draw_stroke) {
        stroke_a = 0;
    }


    // Draw the polyline using the callback
    callbacks->draw_polyline(
        screen_x, screen_y, 0, // z_index = 0 for now
        points_to_use, point_count_to_use, stroke_width,
        fill_r, fill_g, fill_b, fill_a,
        stroke_r, stroke_g, stroke_b, stroke_a,
        callback_user_data
    );
    
    // Clean up temporary points array if we allocated one
    if (needs_cleanup) {
        memory_manager.free(points_to_use);
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
