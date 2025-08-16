#include <string.h>
#include <stdio.h>
#include <math.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "types/types.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief Pushes a EseArc pointer as a Lua userdata object onto the stack.
 * 
 * @details Creates a new Lua table that acts as a proxy for the EseArc object,
 *          storing the C pointer as light userdata in the "__ptr" field and
 *          setting the ArcProxyMeta metatable for property access.
 * 
 * @param arc Pointer to the EseArc object to wrap for Lua access
 * @param is_lua_owned True if LUA will handle freeing
 * 
 * @warning The EseArc object must remain valid for the lifetime of the Lua object
 */
void _arc_lua_register(EseArc *arc, bool is_lua_owned) {
    log_assert("ARC", arc, "_arc_lua_register called with NULL arc");
    log_assert("ARC", arc->lua_ref == LUA_NOREF, "_arc_lua_register arc is already registered");

    lua_newtable(arc->state);
    lua_pushlightuserdata(arc->state, arc);
    lua_setfield(arc->state, -2, "__ptr");

    // Store the ownership flag
    lua_pushboolean(arc->state, is_lua_owned);
    lua_setfield(arc->state, -2, "__is_lua_owned");

    luaL_getmetatable(arc->state, "ArcProxyMeta");
    lua_setmetatable(arc->state, -2);

    // Store a reference to this proxy table in the Lua registry
    arc->lua_ref = luaL_ref(arc->state, LUA_REGISTRYINDEX);
}

void arc_lua_push(EseArc *arc) {
    log_assert("ARC", arc, "arc_lua_push called with NULL arc");
    log_assert("ARC", arc->lua_ref != LUA_NOREF, "arc_lua_push arc not registered with lua");

    // Push the proxy table back onto the stack for Lua to receive
    lua_rawgeti(arc->state, LUA_REGISTRYINDEX, arc->lua_ref);
}

/**
 * @brief Lua function to create a new EseArc object.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new arc object)
 */
static int _arc_lua_new(lua_State *L) {
    float x = 0.0f, y = 0.0f, radius = 1.0f, start_angle = 0.0f, end_angle = 2.0f * M_PI;

    int n_args = lua_gettop(L);
    if (n_args == 5) {
        if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || 
            !lua_isnumber(L, 4) || !lua_isnumber(L, 5)) {
            return luaL_error(L, "all arguments must be numbers");
        }
        x = (float)lua_tonumber(L, 1);
        y = (float)lua_tonumber(L, 2);
        radius = (float)lua_tonumber(L, 3);
        start_angle = (float)lua_tonumber(L, 4);
        end_angle = (float)lua_tonumber(L, 5);
    } else if (n_args != 0) {
        return luaL_error(L, "new() takes 0 or 5 arguments (x, y, radius, start_angle, end_angle)");
    }

    EseArc *arc = (EseArc *)memory_manager.malloc(sizeof(EseArc), MMTAG_GENERAL);
    arc->x = x;
    arc->y = y;
    arc->radius = radius;
    arc->start_angle = start_angle;
    arc->end_angle = end_angle;
    arc->state = L;
    arc->lua_ref = LUA_NOREF;
    _arc_lua_register(arc, true);

    arc_lua_push(arc);
    return 1;
}

/**
 * @brief Lua function to create a zero arc.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new arc object)
 */
static int _arc_lua_zero(lua_State *L) {
    EseArc *arc = (EseArc *)memory_manager.malloc(sizeof(EseArc), MMTAG_GENERAL);
    arc->x = 0.0f;
    arc->y = 0.0f;
    arc->radius = 1.0f;
    arc->start_angle = 0.0f;
    arc->end_angle = 2.0f * M_PI;
    arc->state = L;
    arc->lua_ref = LUA_NOREF;
    _arc_lua_register(arc, true);

    arc_lua_push(arc);
    return 1;
}

/**
 * @brief Lua method to check if point is on arc.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - boolean result)
 */
static int _arc_lua_contains_point(lua_State *L) {
    EseArc *arc = (EseArc *)lua_touserdata(L, lua_upvalueindex(1));
    if (!arc) {
        return luaL_error(L, "Invalid EseArc object in contains_point method");
    }
    
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        return luaL_error(L, "contains_point(x, y) requires two numbers");
    }
    
    float x = (float)lua_tonumber(L, 1);
    float y = (float)lua_tonumber(L, 2);
    float tolerance = 0.1f;
    
    if (lua_gettop(L) >= 3 && lua_isnumber(L, 3)) {
        tolerance = (float)lua_tonumber(L, 3);
    }
    
    lua_pushboolean(L, arc_contains_point(arc, x, y, tolerance));
    return 1;
}

/**
 * @brief Lua method to check if arc intersects rectangle.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - boolean result)
 */
static int _arc_lua_intersects_rect(lua_State *L) {
    EseArc *arc = (EseArc *)lua_touserdata(L, lua_upvalueindex(1));
    if (!arc) {
        return luaL_error(L, "Invalid EseArc object in intersects_rect method");
    }
    
    EseRect *rect = rect_lua_get(L, 1);
    if (!rect) {
        return luaL_error(L, "intersects_rect() requires an EseRect object");
    }
    
    lua_pushboolean(L, arc_intersects_rect(arc, rect));
    return 1;
}

/**
 * @brief Lua method to get arc length.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the length)
 */
static int _arc_lua_get_length(lua_State *L) {
    EseArc *arc = (EseArc *)lua_touserdata(L, lua_upvalueindex(1));
    if (!arc) {
        return luaL_error(L, "Invalid EseArc object in get_length method");
    }
    
    lua_pushnumber(L, arc_get_length(arc));
    return 1;
}

/**
 * @brief Lua method to get point at angle.
 * 
 * @param L Lua state pointer
 * @return Number of return values (3 - success boolean, x, y coordinates)
 */
static int _arc_lua_get_point_at_angle(lua_State *L) {
    EseArc *arc = (EseArc *)lua_touserdata(L, lua_upvalueindex(1));
    if (!arc) {
        return luaL_error(L, "Invalid EseArc object in get_point_at_angle method");
    }
    
    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "get_point_at_angle(angle) requires a number");
    }
    
    float angle = (float)lua_tonumber(L, 1);
    float x, y;
    bool success = arc_get_point_at_angle(arc, angle, &x, &y);
    
    lua_pushboolean(L, success);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 3;
}

/**
 * @brief Lua __index metamethod for EseArc objects (getter).
 * 
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack
 */
static int _arc_lua_index(lua_State *L) {
    EseArc *arc = arc_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!arc || !key) return 0;

    if (strcmp(key, "x") == 0) {
        lua_pushnumber(L, arc->x);
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(L, arc->y);
        return 1;
    } else if (strcmp(key, "radius") == 0) {
        lua_pushnumber(L, arc->radius);
        return 1;
    } else if (strcmp(key, "start_angle") == 0) {
        lua_pushnumber(L, arc->start_angle);
        return 1;
    } else if (strcmp(key, "end_angle") == 0) {
        lua_pushnumber(L, arc->end_angle);
        return 1;
    } else if (strcmp(key, "contains_point") == 0) {
        lua_pushlightuserdata(L, arc);
        lua_pushcclosure(L, _arc_lua_contains_point, 1);
        return 1;
    } else if (strcmp(key, "get_length") == 0) {
        lua_pushlightuserdata(L, arc);
        lua_pushcclosure(L, _arc_lua_get_length, 1);
        return 1;
    } else if (strcmp(key, "get_point_at_angle") == 0) {
        lua_pushlightuserdata(L, arc);
        lua_pushcclosure(L, _arc_lua_get_point_at_angle, 1);
        return 1;
    } else if (strcmp(key, "intersects_rect") == 0) {
        lua_pushlightuserdata(L, arc);
        lua_pushcclosure(L, _arc_lua_intersects_rect, 1);
        return 1;
    }
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseArc objects (setter).
 * 
 * @param L Lua state pointer
 * @return Always returns 0 or throws Lua error for invalid operations
 */
static int _arc_lua_newindex(lua_State *L) {
    EseArc *arc = arc_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!arc || !key) return 0;

    if (strcmp(key, "x") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "arc.x must be a number");
        }
        arc->x = (float)lua_tonumber(L, 3);
        return 0;
    } else if (strcmp(key, "y") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "arc.y must be a number");
        }
        arc->y = (float)lua_tonumber(L, 3);
        return 0;
    } else if (strcmp(key, "radius") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "arc.radius must be a number");
        }
        arc->radius = (float)lua_tonumber(L, 3);
        return 0;
    } else if (strcmp(key, "start_angle") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "arc.start_angle must be a number");
        }
        arc->start_angle = (float)lua_tonumber(L, 3);
        return 0;
    } else if (strcmp(key, "end_angle") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "arc.end_angle must be a number");
        }
        arc->end_angle = (float)lua_tonumber(L, 3);
        return 0;
    }
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __gc metamethod for EseArc objects.
 * 
 * @param L Lua state pointer
 * @return Always 0 (no return values)
 */
static int _arc_lua_gc(lua_State *L) {
    EseArc *arc = arc_lua_get(L, 1);

    if (arc) {
        lua_getfield(L, 1, "__is_lua_owned");
        bool is_lua_owned = lua_toboolean(L, -1);
        lua_pop(L, 1);

        if (is_lua_owned) {
            arc_destroy(arc);
            log_debug("LUA_GC", "Arc object (Lua-owned) garbage collected and C memory freed.");
        } else {
            log_debug("LUA_GC", "Arc object (C-owned) garbage collected, C memory *not* freed.");
        }
    }

    return 0;
}

static int _arc_lua_tostring(lua_State *L) {
    EseArc *arc = arc_lua_get(L, 1);

    if (!arc) {
        lua_pushstring(L, "Arc: (invalid)");
        return 1;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "Arc: %p (x=%.2f, y=%.2f, radius=%.2f, start=%.2f, end=%.2f)", 
             (void*)arc, arc->x, arc->y, arc->radius, arc->start_angle, arc->end_angle);
    lua_pushstring(L, buf);

    return 1;
}

void arc_lua_init(EseLuaEngine *engine) {
    if (luaL_newmetatable(engine->runtime, "ArcProxyMeta")) {
        log_debug("LUA", "Adding entity ArcProxyMeta to engine");
        lua_pushcfunction(engine->runtime, _arc_lua_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _arc_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushcfunction(engine->runtime, _arc_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");
        lua_pushcfunction(engine->runtime, _arc_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
    
    // Create global EseArc table with constructor
    lua_getglobal(engine->runtime, "Arc");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);
        log_debug("LUA", "Creating global EseArc table");
        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _arc_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_pushcfunction(engine->runtime, _arc_lua_zero);
        lua_setfield(engine->runtime, -2, "zero");
        lua_setglobal(engine->runtime, "Arc");
    } else {
        lua_pop(engine->runtime, 1);
    }
}

EseArc *arc_create(EseLuaEngine *engine, bool c_only) {
    EseArc *arc = (EseArc *)memory_manager.malloc(sizeof(EseArc), MMTAG_GENERAL);
    arc->x = 0.0f;
    arc->y = 0.0f;
    arc->radius = 1.0f;
    arc->start_angle = 0.0f;
    arc->end_angle = 2.0f * M_PI;
    arc->state = engine->runtime;
    arc->lua_ref = LUA_NOREF;
    if (!c_only) {
        _arc_lua_register(arc, false);
    }
    return arc;
}

EseArc *arc_copy(const EseArc *source, bool c_only) {
    if (source == NULL) {
        return NULL;
    }

    EseArc *copy = (EseArc *)memory_manager.malloc(sizeof(EseArc), MMTAG_GENERAL);
    copy->x = source->x;
    copy->y = source->y;
    copy->radius = source->radius;
    copy->start_angle = source->start_angle;
    copy->end_angle = source->end_angle;
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
    if (!c_only) {
        _arc_lua_register(copy, false);
    }
    return copy;
}

void arc_destroy(EseArc *arc) {
    if (arc) {
        if (arc->lua_ref != LUA_NOREF) {
            luaL_unref(arc->state, LUA_REGISTRYINDEX, arc->lua_ref);
        }
        memory_manager.free(arc);
    }
}

EseArc *arc_lua_get(lua_State *L, int idx) {
    if (!lua_istable(L, idx)) {
        return NULL;
    }
    
    if (!lua_getmetatable(L, idx)) {
        return NULL;
    }
    
    luaL_getmetatable(L, "ArcProxyMeta");
    
    if (!lua_rawequal(L, -1, -2)) {
        lua_pop(L, 2);
        return NULL;
    }
    
    lua_pop(L, 2);
    
    lua_getfield(L, idx, "__ptr");
    
    if (!lua_islightuserdata(L, -1)) {
        lua_pop(L, 1);
        return NULL;
    }
    
    void *arc = lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    return (EseArc *)arc;
}

bool arc_contains_point(const EseArc *arc, float x, float y, float tolerance) {
    if (!arc) return false;
    
    // Calculate distance from center
    float dx = x - arc->x;
    float dy = y - arc->y;
    float distance = sqrtf(dx * dx + dy * dy);
    
    // Check if point is on the circle (within tolerance)
    if (fabsf(distance - arc->radius) > tolerance) {
        return false;
    }
    
    // Calculate angle of the point
    float angle = atan2f(dy, dx);
    if (angle < 0) angle += 2.0f * M_PI;
    
    // Normalize arc angles
    float start = arc->start_angle;
    float end = arc->end_angle;
    
    // Handle angle wrapping
    if (start > end) {
        return (angle >= start) || (angle <= end);
    } else {
        return (angle >= start) && (angle <= end);
    }
}

float arc_get_length(const EseArc *arc) {
    if (!arc) return 0.0f;
    
    float angle_diff = arc->end_angle - arc->start_angle;
    if (angle_diff < 0) angle_diff += 2.0f * M_PI;
    
    return arc->radius * angle_diff;
}

bool arc_get_point_at_angle(const EseArc *arc, float angle, float *out_x, float *out_y) {
    if (!arc || !out_x || !out_y) return false;
    
    // Normalize angle
    while (angle < 0) angle += 2.0f * M_PI;
    while (angle >= 2.0f * M_PI) angle -= 2.0f * M_PI;
    
    // Check if angle is within arc range
    float start = arc->start_angle;
    float end = arc->end_angle;
    
    bool in_range;
    if (start > end) {
        in_range = (angle >= start) || (angle <= end);
    } else {
        in_range = (angle >= start) && (angle <= end);
    }
    
    if (!in_range) return false;
    
    // Calculate point
    *out_x = arc->x + arc->radius * cosf(angle);
    *out_y = arc->y + arc->radius * sinf(angle);
    
    return true;
}

bool arc_intersects_rect(const EseArc *arc, const EseRect *rect) {
    if (!arc || !rect) return false;
    
    // First check if the full circle could possibly intersect the rectangle
    // Find closest point on rectangle to arc center
    float closest_x = fmaxf(rect->x, fminf(arc->x, rect->x + rect->width));
    float closest_y = fmaxf(rect->y, fminf(arc->y, rect->y + rect->height));
    
    // Calculate distance from arc center to closest point on rectangle
    float dx = arc->x - closest_x;
    float dy = arc->y - closest_y;
    float distance_squared = dx * dx + dy * dy;
    float radius_squared = arc->radius * arc->radius;
    
    // If circle doesn't intersect rectangle, arc can't intersect either
    if (distance_squared > radius_squared) {
        return false;
    }
    
    // Now check if the specific arc segment intersects the rectangle
    
    // Method 1: Check if any of the rectangle corners are within the arc
    float corners[4][2] = {
        {rect->x, rect->y},                          // Top-left
        {rect->x + rect->width, rect->y},            // Top-right
        {rect->x + rect->width, rect->y + rect->height}, // Bottom-right
        {rect->x, rect->y + rect->height}            // Bottom-left
    };
    
    for (int i = 0; i < 4; i++) {
        float corner_dx = corners[i][0] - arc->x;
        float corner_dy = corners[i][1] - arc->y;
        float corner_distance_squared = corner_dx * corner_dx + corner_dy * corner_dy;
        
        // Check if corner is on the circle (within small tolerance)
        if (fabsf(corner_distance_squared - radius_squared) < 0.01f) {
            // Corner is on circle, check if it's within arc angle range
            float corner_angle = atan2f(corner_dy, corner_dx);
            if (corner_angle < 0) corner_angle += 2.0f * M_PI;
            
            float start = arc->start_angle;
            float end = arc->end_angle;
            
            bool in_range;
            if (start > end) {
                in_range = (corner_angle >= start) || (corner_angle <= end);
            } else {
                in_range = (corner_angle >= start) && (corner_angle <= end);
            }
            
            if (in_range) return true;
        }
    }
    
    // Method 2: Sample points along the arc and check if any are inside rectangle
    const int num_samples = 32;
    float angle_step = (arc->end_angle - arc->start_angle) / (float)num_samples;
    
    // Handle wraparound case
    if (arc->end_angle < arc->start_angle) {
        angle_step = (arc->end_angle + 2.0f * M_PI - arc->start_angle) / (float)num_samples;
    }
    
    for (int i = 0; i <= num_samples; i++) {
        float angle = arc->start_angle + angle_step * i;
        
        // Handle wraparound
        while (angle >= 2.0f * M_PI) angle -= 2.0f * M_PI;
        while (angle < 0) angle += 2.0f * M_PI;
        
        float point_x = arc->x + arc->radius * cosf(angle);
        float point_y = arc->y + arc->radius * sinf(angle);
        
        // Check if this point is inside the rectangle
        if (point_x >= rect->x && point_x <= rect->x + rect->width &&
            point_y >= rect->y && point_y <= rect->y + rect->height) {
            return true;
        }
    }
    
    // Method 3: Check if arc endpoints are inside rectangle
    float start_x = arc->x + arc->radius * cosf(arc->start_angle);
    float start_y = arc->y + arc->radius * sinf(arc->start_angle);
    if (start_x >= rect->x && start_x <= rect->x + rect->width &&
        start_y >= rect->y && start_y <= rect->y + rect->height) {
        return true;
    }
    
    float end_x = arc->x + arc->radius * cosf(arc->end_angle);
    float end_y = arc->y + arc->radius * sinf(arc->end_angle);
    if (end_x >= rect->x && end_x <= rect->x + rect->width &&
        end_y >= rect->y && end_y <= rect->y + rect->height) {
        return true;
    }
    
    return false;
}
