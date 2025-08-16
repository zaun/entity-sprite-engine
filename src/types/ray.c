#include <string.h>
#include <stdio.h>
#include <math.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "types/ray.h"
#include "types/rect.h"

/**
 * @brief Pushes a EseRay pointer as a Lua userdata object onto the stack.
 * 
 * @details Creates a new Lua table that acts as a proxy for the EseRay object,
 *          storing the C pointer as light userdata in the "__ptr" field and
 *          setting the RayProxyMeta metatable for property access.
 * 
 * @param ray Pointer to the EseRay object to wrap for Lua access
 * @param is_lua_owned True if LUA will handle freeing
 * 
 * @warning The EseRay object must remain valid for the lifetime of the Lua object
 */
void _ray_lua_register(EseRay *ray, bool is_lua_owned) {
    log_assert("RAY", ray, "_ray_lua_register called with NULL ray");
    log_assert("RAY", ray->lua_ref == LUA_NOREF, "_ray_lua_register ray is already registered");

    lua_newtable(ray->state);
    lua_pushlightuserdata(ray->state, ray);
    lua_setfield(ray->state, -2, "__ptr");

    // Store the ownership flag
    lua_pushboolean(ray->state, is_lua_owned);
    lua_setfield(ray->state, -2, "__is_lua_owned");

    luaL_getmetatable(ray->state, "RayProxyMeta");
    lua_setmetatable(ray->state, -2);

    // Store a reference to this proxy table in the Lua registry
    ray->lua_ref = luaL_ref(ray->state, LUA_REGISTRYINDEX);
}

void ray_lua_push(EseRay *ray) {
    log_assert("RAY", ray, "ray_lua_push called with NULL ray");
    log_assert("RAY", ray->lua_ref != LUA_NOREF, "ray_lua_push ray not registered with lua");

    // Push the proxy table back onto the stack for Lua to receive
    lua_rawgeti(ray->state, LUA_REGISTRYINDEX, ray->lua_ref);
}

/**
 * @brief Lua function to create a new EseRay object.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new ray object)
 */
static int _ray_lua_new(lua_State *L) {
    float x = 0.0f, y = 0.0f, dx = 1.0f, dy = 0.0f;

    int n_args = lua_gettop(L);
    if (n_args == 4) {
        if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || 
            !lua_isnumber(L, 3) || !lua_isnumber(L, 4)) {
            return luaL_error(L, "all arguments must be numbers");
        }
        x = (float)lua_tonumber(L, 1);
        y = (float)lua_tonumber(L, 2);
        dx = (float)lua_tonumber(L, 3);
        dy = (float)lua_tonumber(L, 4);
    } else if (n_args != 0) {
        return luaL_error(L, "new() takes 0 or 4 arguments (x, y, dx, dy)");
    }

    EseRay *ray = (EseRay *)memory_manager.malloc(sizeof(EseRay), MMTAG_GENERAL);
    ray->x = x;
    ray->y = y;
    ray->dx = dx;
    ray->dy = dy;
    ray->state = L;
    ray->lua_ref = LUA_NOREF;
    _ray_lua_register(ray, true);

    ray_lua_push(ray);
    return 1;
}

/**
 * @brief Lua function to create a zero ray.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new ray object)
 */
static int _ray_lua_zero(lua_State *L) {
    EseRay *ray = (EseRay *)memory_manager.malloc(sizeof(EseRay), MMTAG_GENERAL);
    ray->x = 0.0f;
    ray->y = 0.0f;
    ray->dx = 1.0f;
    ray->dy = 0.0f;
    ray->state = L;
    ray->lua_ref = LUA_NOREF;
    _ray_lua_register(ray, true);

    ray_lua_push(ray);
    return 1;
}

/**
 * @brief Lua method to check if ray intersects rectangle.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - boolean result)
 */
static int _ray_lua_intersects_rect(lua_State *L) {
    EseRay *ray = (EseRay *)lua_touserdata(L, lua_upvalueindex(1));
    if (!ray) {
        return luaL_error(L, "Invalid EseRay object in intersects_rect method");
    }
    
    EseRect *rect = rect_lua_get(L, 1);
    if (!rect) {
        return luaL_error(L, "intersects_rect() requires an EseRect object");
    }
    
    lua_pushboolean(L, ray_intersects_rect(ray, rect));
    return 1;
}

/**
 * @brief Lua method to get point at distance.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 2 - x and y coordinates)
 */
static int _ray_lua_get_point_at_distance(lua_State *L) {
    EseRay *ray = (EseRay *)lua_touserdata(L, lua_upvalueindex(1));
    if (!ray) {
        return luaL_error(L, "Invalid EseRay object in get_point_at_distance method");
    }
    
    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "get_point_at_distance(distance) requires a number");
    }
    
    float distance = (float)lua_tonumber(L, 1);
    float x, y;
    ray_get_point_at_distance(ray, distance, &x, &y);
    
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

/**
 * @brief Lua method to normalize ray direction.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 0)
 */
static int _ray_lua_normalize(lua_State *L) {
    EseRay *ray = (EseRay *)lua_touserdata(L, lua_upvalueindex(1));
    if (!ray) {
        return luaL_error(L, "Invalid EseRay object in normalize method");
    }
    
    ray_normalize(ray);
    return 0;
}

/**
 * @brief Lua __index metamethod for EseRay objects (getter).
 * 
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack
 */
static int _ray_lua_index(lua_State *L) {
    EseRay *ray = ray_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!ray || !key) return 0;

    if (strcmp(key, "x") == 0) {
        lua_pushnumber(L, ray->x);
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(L, ray->y);
        return 1;
    } else if (strcmp(key, "dx") == 0) {
        lua_pushnumber(L, ray->dx);
        return 1;
    } else if (strcmp(key, "dy") == 0) {
        lua_pushnumber(L, ray->dy);
        return 1;
    } else if (strcmp(key, "intersects_rect") == 0) {
        lua_pushlightuserdata(L, ray);
        lua_pushcclosure(L, _ray_lua_intersects_rect, 1);
        return 1;
    } else if (strcmp(key, "get_point_at_distance") == 0) {
        lua_pushlightuserdata(L, ray);
        lua_pushcclosure(L, _ray_lua_get_point_at_distance, 1);
        return 1;
    } else if (strcmp(key, "normalize") == 0) {
        lua_pushlightuserdata(L, ray);
        lua_pushcclosure(L, _ray_lua_normalize, 1);
        return 1;
    }
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseRay objects (setter).
 * 
 * @param L Lua state pointer
 * @return Always returns 0 or throws Lua error for invalid operations
 */
static int _ray_lua_newindex(lua_State *L) {
    EseRay *ray = ray_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!ray || !key) return 0;

    if (strcmp(key, "x") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "ray.x must be a number");
        }
        ray->x = (float)lua_tonumber(L, 3);
        return 0;
    } else if (strcmp(key, "y") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "ray.y must be a number");
        }
        ray->y = (float)lua_tonumber(L, 3);
        return 0;
    } else if (strcmp(key, "dx") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "ray.dx must be a number");
        }
        ray->dx = (float)lua_tonumber(L, 3);
        return 0;
    } else if (strcmp(key, "dy") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "ray.dy must be a number");
        }
        ray->dy = (float)lua_tonumber(L, 3);
        return 0;
    }
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __gc metamethod for EseRay objects.
 * 
 * @param L Lua state pointer
 * @return Always 0 (no return values)
 */
static int _ray_lua_gc(lua_State *L) {
    EseRay *ray = ray_lua_get(L, 1);

    if (ray) {
        lua_getfield(L, 1, "__is_lua_owned");
        bool is_lua_owned = lua_toboolean(L, -1);
        lua_pop(L, 1);

        if (is_lua_owned) {
            ray_destroy(ray);
            log_debug("LUA_GC", "Ray object (Lua-owned) garbage collected and C memory freed.");
        } else {
            log_debug("LUA_GC", "Ray object (C-owned) garbage collected, C memory *not* freed.");
        }
    }

    return 0;
}

static int _ray_lua_tostring(lua_State *L) {
    EseRay *ray = ray_lua_get(L, 1);

    if (!ray) {
        lua_pushstring(L, "Ray: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Ray: %p (x=%.2f, y=%.2f, dx=%.2f, dy=%.2f)", (void*)ray, ray->x, ray->y, ray->dx, ray->dy);
    lua_pushstring(L, buf);

    return 1;
}

void ray_lua_init(EseLuaEngine *engine) {
    if (luaL_newmetatable(engine->runtime, "RayProxyMeta")) {
        log_debug("LUA", "Adding entity RayProxyMeta to engine");
        lua_pushcfunction(engine->runtime, _ray_lua_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _ray_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushcfunction(engine->runtime, _ray_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");
        lua_pushcfunction(engine->runtime, _ray_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
    
    // Create global EseRay table with constructor
    lua_getglobal(engine->runtime, "Ray");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);
        log_debug("LUA", "Creating global EseRay table");
        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _ray_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_pushcfunction(engine->runtime, _ray_lua_zero);
        lua_setfield(engine->runtime, -2, "zero");
        lua_setglobal(engine->runtime, "Ray");
    } else {
        lua_pop(engine->runtime, 1);
    }
}

EseRay *ray_create(EseLuaEngine *engine, bool c_only) {
    EseRay *ray = (EseRay *)memory_manager.malloc(sizeof(EseRay), MMTAG_GENERAL);
    ray->x = 0.0f;
    ray->y = 0.0f;
    ray->dx = 1.0f;
    ray->dy = 0.0f;
    ray->state = engine->runtime;
    ray->lua_ref = LUA_NOREF;
    if (!c_only) {
        _ray_lua_register(ray, false);
    }
    return ray;
}

EseRay *ray_copy(const EseRay *source, bool c_only) {
    if (source == NULL) {
        return NULL;
    }

    EseRay *copy = (EseRay *)memory_manager.malloc(sizeof(EseRay), MMTAG_GENERAL);
    copy->x = source->x;
    copy->y = source->y;
    copy->dx = source->dx;
    copy->dy = source->dy;
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
    if (!c_only) {
        _ray_lua_register(copy, false);
    }
    return copy;
}

void ray_destroy(EseRay *ray) {
    if (ray) {
        if (ray->lua_ref != LUA_NOREF) {
            luaL_unref(ray->state, LUA_REGISTRYINDEX, ray->lua_ref);
        }
        memory_manager.free(ray);
    }
}

EseRay *ray_lua_get(lua_State *L, int idx) {
    if (!lua_istable(L, idx)) {
        return NULL;
    }
    
    if (!lua_getmetatable(L, idx)) {
        return NULL;
    }
    
    luaL_getmetatable(L, "RayProxyMeta");
    
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
    
    void *ray = lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    return (EseRay *)ray;
}

bool ray_intersects_rect(const EseRay *ray, const EseRect *rect) {
    if (!ray || !rect) return false;

    // Ray-AABB intersection using slab method
    float t_min = 0.0f;
    float t_max = INFINITY;

    // Check X slab
    if (fabsf(ray->dx) < 0.0001f) {
        if (ray->x < rect->x || ray->x > rect->x + rect->width) {
            return false;
        }
    } else {
        float inv_dx = 1.0f / ray->dx;
        float t1 = (rect->x - ray->x) * inv_dx;
        float t2 = (rect->x + rect->width - ray->x) * inv_dx;
        
        if (t1 > t2) {
            float temp = t1;
            t1 = t2;
            t2 = temp;
        }
        
        t_min = fmaxf(t_min, t1);
        t_max = fminf(t_max, t2);
        
        if (t_min > t_max) return false;
    }

    // Check Y slab
    if (fabsf(ray->dy) < 0.0001f) {
        if (ray->y < rect->y || ray->y > rect->y + rect->height) {
            return false;
        }
    } else {
        float inv_dy = 1.0f / ray->dy;
        float t1 = (rect->y - ray->y) * inv_dy;
        float t2 = (rect->y + rect->height - ray->y) * inv_dy;
        
        if (t1 > t2) {
            float temp = t1;
            t1 = t2;
            t2 = temp;
        }
        
        t_min = fmaxf(t_min, t1);
        t_max = fminf(t_max, t2);
        
        if (t_min > t_max) return false;
    }

    return t_max >= 0.0f;
}

void ray_get_point_at_distance(const EseRay *ray, float distance, float *out_x, float *out_y) {
    if (!ray || !out_x || !out_y) return;
    
    *out_x = ray->x + ray->dx * distance;
    *out_y = ray->y + ray->dy * distance;
}

void ray_normalize(EseRay *ray) {
    if (!ray) return;
    
    float length = sqrtf(ray->dx * ray->dx + ray->dy * ray->dy);
    if (length > 0.0001f) {
        ray->dx /= length;
        ray->dy /= length;
    }
}
