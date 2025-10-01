#include <string.h>
#include <stdio.h>
#include <math.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/types.h"
#include "types/rect.h"

// ========================================
// PRIVATE STRUCT DEFINITION
// ========================================

/**
 * @brief Internal structure for EseRay
 * 
 * @details This structure is only visible within this implementation file.
 *          External code must use the provided getter/setter functions.
 */
struct EseRay {
    float x;    /** X coordinate of the ray origin */
    float y;    /** Y coordinate of the ray origin */
    float dx;   /** X component of the ray direction */
    float dy;   /** Y component of the ray direction */

    lua_State *state;   /** Lua State this EseRay belongs to */
    int lua_ref;        /** Lua registry reference to its own proxy table */
    int lua_ref_count;  /** Number of times this ray has been referenced in C */
};

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseRay *_ese_ray_make(void);

// Lua metamethods
static int _ese_ray_lua_gc(lua_State *L);
static int _ese_ray_lua_index(lua_State *L);
static int _ese_ray_lua_newindex(lua_State *L);
static int _ese_ray_lua_tostring(lua_State *L);

// Lua constructors
static int _ese_ray_lua_new(lua_State *L);
static int _ese_ray_lua_zero(lua_State *L);

// Lua methods
static int _ese_ray_lua_intersects_rect(lua_State *L);
static int _ese_ray_lua_get_point_at_distance(lua_State *L);
static int _ese_ray_lua_normalize(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseRay instance with default values
 * 
 * Allocates memory for a new EseRay and initializes all fields to safe defaults.
 * The ray starts at origin (0,0) with direction (1,0) and no Lua state or references.
 * 
 * @return Pointer to the newly created EseRay, or NULL on allocation failure
 */
static EseRay *_ese_ray_make() {
    EseRay *ray = (EseRay *)memory_manager.malloc(sizeof(EseRay), MMTAG_RAY);
    ray->x = 0.0f;
    ray->y = 0.0f;
    ray->dx = 1.0f;
    ray->dy = 0.0f;
    ray->state = NULL;
    ray->lua_ref = LUA_NOREF;
    ray->lua_ref_count = 0;
    return ray;
}

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseRay
 * 
 * Handles cleanup when a Lua proxy table for an EseRay is garbage collected.
 * Only frees the underlying EseRay if it has no C-side references.
 * 
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static int _ese_ray_lua_gc(lua_State *L) {
    // Get from userdata
    EseRay **ud = (EseRay **)luaL_testudata(L, 1, RAY_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseRay *ray = *ud;
    if (ray) {
        // If lua_ref == LUA_NOREF, there are no more references to this ray, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this ray was referenced from C and should not be freed.
        if (ray->lua_ref == LUA_NOREF) {
            ese_ray_destroy(ray);
        }
    }

    return 0;
}

/**
 * @brief Lua __index metamethod for EseRay property access
 * 
 * Provides read access to ray properties (x, y, dx, dy) from Lua. When a Lua script
 * accesses ray.x, ray.y, ray.dx, or ray.dy, this function is called to retrieve the values.
 * Also provides access to methods like intersects_rect, get_point_at_distance, and normalize.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid properties/methods, 0 for invalid)
 */
static int _ese_ray_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_RAY_INDEX);
    EseRay *ray = ese_ray_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!ray || !key) {
        profile_cancel(PROFILE_LUA_RAY_INDEX);
        return 0;
    }

    if (strcmp(key, "x") == 0) {
        lua_pushnumber(L, ray->x);
        profile_stop(PROFILE_LUA_RAY_INDEX, "ray_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(L, ray->y);
        profile_stop(PROFILE_LUA_RAY_INDEX, "ray_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "dx") == 0) {
        lua_pushnumber(L, ray->dx);
        profile_stop(PROFILE_LUA_RAY_INDEX, "ray_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "dy") == 0) {
        lua_pushnumber(L, ray->dy);
        profile_stop(PROFILE_LUA_RAY_INDEX, "ray_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "intersects_rect") == 0) {
        lua_pushlightuserdata(L, ray);
        lua_pushcclosure(L, _ese_ray_lua_intersects_rect, 1);
        profile_stop(PROFILE_LUA_RAY_INDEX, "ray_lua_index (method)");
        return 1;
    } else if (strcmp(key, "get_point_at_distance") == 0) {
        lua_pushlightuserdata(L, ray);
        lua_pushcclosure(L, _ese_ray_lua_get_point_at_distance, 1);
        profile_stop(PROFILE_LUA_RAY_INDEX, "ray_lua_index (method)");
        return 1;
    } else if (strcmp(key, "normalize") == 0) {
        lua_pushlightuserdata(L, ray);
        lua_pushcclosure(L, _ese_ray_lua_normalize, 1);
        profile_stop(PROFILE_LUA_RAY_INDEX, "ray_lua_index (method)");
        return 1;
    }
    profile_stop(PROFILE_LUA_RAY_INDEX, "ray_lua_index (invalid)");
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseRay property assignment
 * 
 * Provides write access to ray properties (x, y, dx, dy) from Lua. When a Lua script
 * assigns to ray.x, ray.y, ray.dx, or ray.dy, this function is called to update the values.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_ray_lua_newindex(lua_State *L) {
    profile_start(PROFILE_LUA_RAY_NEWINDEX);
    EseRay *ray = ese_ray_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!ray || !key) {
        profile_cancel(PROFILE_LUA_RAY_NEWINDEX);
        return 0;
    }

    if (strcmp(key, "x") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_RAY_NEWINDEX);
            return luaL_error(L, "ray.x must be a number");
        }
        ray->x = (float)lua_tonumber(L, 3);
        profile_stop(PROFILE_LUA_RAY_NEWINDEX, "ray_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "y") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_RAY_NEWINDEX);
            return luaL_error(L, "ray.y must be a number");
        }
        ray->y = (float)lua_tonumber(L, 3);
        profile_stop(PROFILE_LUA_RAY_NEWINDEX, "ray_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "dx") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_RAY_NEWINDEX);
            return luaL_error(L, "ray.dx must be a number");
        }
        ray->dx = (float)lua_tonumber(L, 3);
        profile_stop(PROFILE_LUA_RAY_NEWINDEX, "ray_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "dy") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_RAY_NEWINDEX);
            return luaL_error(L, "ray.dy must be a number");
        }
        ray->dy = (float)lua_tonumber(L, 3);
        profile_stop(PROFILE_LUA_RAY_NEWINDEX, "ray_lua_newindex (setter)");
        return 0;
    }
    profile_stop(PROFILE_LUA_RAY_NEWINDEX, "ray_lua_newindex (invalid)");
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __tostring metamethod for EseRay string representation
 * 
 * Converts an EseRay to a human-readable string for debugging and display.
 * The format includes the memory address and current x,y,dx,dy values.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
static int _ese_ray_lua_tostring(lua_State *L) {
    EseRay *ray = ese_ray_lua_get(L, 1);

    if (!ray) {
        lua_pushstring(L, "Ray: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Ray: %p (x=%.2f, y=%.2f, dx=%.2f, dy=%.2f)", 
             (void*)ray, ray->x, ray->y, ray->dx, ray->dy);
    lua_pushstring(L, buf);

    return 1;
}

// Lua constructors
/**
 * @brief Lua constructor function for creating new EseRay instances
 * 
 * Creates a new EseRay from Lua with specified origin and direction coordinates.
 * This function is called when Lua code executes `Ray.new(x, y, dx, dy)` or
 * `Ray.new(point, vector)`. It validates the arguments, creates the underlying
 * EseRay, and returns a proxy table that provides access to the ray's properties and methods.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_ray_lua_new(lua_State *L) {
    profile_start(PROFILE_LUA_RAY_NEW);
    float x = 0.0f, y = 0.0f, dx = 1.0f, dy = 0.0f;

    int n_args = lua_gettop(L);
    if (n_args == 4) {
        if (lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER || 
            lua_type(L, 3) != LUA_TNUMBER || lua_type(L, 4) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_RAY_NEW);
            return luaL_error(L, "Ray.new(number, number, number, number) takes 4 arguments");
        }
        x = (float)lua_tonumber(L, 1);
        y = (float)lua_tonumber(L, 2);
        dx = (float)lua_tonumber(L, 3);
        dy = (float)lua_tonumber(L, 4);
    } else if (n_args == 2) {
        EsePoint *p = ese_point_lua_get(L, 1);
        EseVector *v = ese_vector_lua_get(L, 2);
        if (!p || !v) {
            profile_cancel(PROFILE_LUA_RAY_NEW);
            return luaL_error(L, "Ray.new(point, vector) takes 2 arguments");
        }
        x = ese_point_get_x(p);
        y = ese_point_get_y(p);
        dx = ese_vector_get_x(v);
        dy = ese_vector_get_y(v);
    } else {
        profile_cancel(PROFILE_LUA_RAY_NEW);
        return luaL_error(L, "Ray.new(x, y, dx, dy) or Ray.new(point, vector)");
    }

    // Create the ray using the standard creation function
    EseRay *ray = _ese_ray_make();
    ray->x = x;
    ray->y = y;
    ray->dx = dx;
    ray->dy = dy;
    ray->state = L;
    
    // Create userdata directly
    EseRay **ud = (EseRay **)lua_newuserdata(L, sizeof(EseRay *));
    *ud = ray;

    // Attach metatable
    luaL_getmetatable(L, RAY_PROXY_META);
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_RAY_NEW, "ray_lua_new");
    return 1;
}

/**
 * @brief Lua constructor function for creating EseRay at origin
 * 
 * Creates a new EseRay at the origin (0,0) with default direction (1,0) from Lua.
 * This function is called when Lua code executes `Ray.zero()`.
 * It's a convenience constructor for creating rays at the default position and direction.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_ray_lua_zero(lua_State *L) {
    profile_start(PROFILE_LUA_RAY_ZERO);

    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 0) {
        profile_cancel(PROFILE_LUA_RAY_ZERO);
        return luaL_error(L, "Ray.zero() takes 0 arguments");
    }

    // Create the ray using the standard creation function
    EseRay *ray = _ese_ray_make();  // We'll set the state manually
    ray->state = L;
    
    // Create userdata directly
    EseRay **ud = (EseRay **)lua_newuserdata(L, sizeof(EseRay *));
    *ud = ray;

    // Attach metatable
    luaL_getmetatable(L, RAY_PROXY_META);
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_RAY_ZERO, "ray_lua_zero");
    return 1;
}

// Lua methods
/**
 * @brief Lua method for testing ray-rectangle intersection
 * 
 * Tests whether the ray intersects with a given rectangle using efficient
 * AABB intersection algorithms. Returns a boolean result.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - boolean result)
 */
static int _ese_ray_lua_intersects_rect(lua_State *L) {
    // Get argument count
    int n_args = lua_gettop(L);
    if (n_args != 2) {
        return luaL_error(L, "ray:intersects_rect(rect) takes 1 argument");
    }

    EseRay *ray = (EseRay *)lua_touserdata(L, lua_upvalueindex(1));
    if (!ray) {
        return luaL_error(L, "Invalid EseRay object in intersects_rect method");
    }
    
    EseRect *rect = ese_rect_lua_get(L, 2);
    if (!rect) {
        return luaL_error(L, "ray:intersects_rect(rect) takes a Rect");
    }
    
    lua_pushboolean(L, ese_ray_intersects_rect(ray, rect));
    return 1;
}

/**
 * @brief Lua method for getting a point along the ray at a specified distance
 * 
 * Calculates the coordinates of a point that lies along the ray at the given
 * distance from the ray's origin. Returns both x and y coordinates.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 2 - x and y coordinates)
 */
static int _ese_ray_lua_get_point_at_distance(lua_State *L) {
    // Get argument count
    int n_args = lua_gettop(L);
    if (n_args != 2) {
        return luaL_error(L, "ray:get_point_at_distance(distance) takes 1 argument");
    }

    EseRay *ray = (EseRay *)lua_touserdata(L, lua_upvalueindex(1));
    if (!ray) {
        return luaL_error(L, "Invalid EseRay object in get_point_at_distance method");
    }
    
    if (lua_type(L, 2) != LUA_TNUMBER) {
        return luaL_error(L, "ray:get_point_at_distance(distance) takes a number");
    }
    
    float distance = (float)lua_tonumber(L, 2);
    float x, y;
    ese_ray_get_point_at_distance(ray, distance, &x, &y);
    
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

/**
 * @brief Lua method for normalizing the ray direction
 * 
 * Normalizes the ray's direction vector to unit length while preserving
 * its direction. If the direction has zero magnitude, no change is made.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_ray_lua_normalize(lua_State *L) {
    EseRay *ray = (EseRay *)lua_touserdata(L, lua_upvalueindex(1));
    if (!ray) {
        return luaL_error(L, "Invalid EseRay object in normalize method");
    }
    
    ese_ray_normalize(ray);
    return 0;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseRay *ese_ray_create(EseLuaEngine *engine) {
    log_assert("RAY", engine, "ese_ray_create called with NULL engine");

    EseRay *ray = _ese_ray_make();
    ray->state = engine->runtime;
    return ray;
}

EseRay *ese_ray_copy(const EseRay *source) {
    log_assert("RAY", source, "ese_ray_copy called with NULL source");

    EseRay *copy = (EseRay *)memory_manager.malloc(sizeof(EseRay), MMTAG_RAY);
    copy->x = source->x;
    copy->y = source->y;
    copy->dx = source->dx;
    copy->dy = source->dy;
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
    copy->lua_ref_count = 0;
    return copy;
}

void ese_ray_destroy(EseRay *ray) {
    if (!ray) return;
    
    if (ray->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(ray);
    } else {
        ese_ray_unref(ray);
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

// Lua integration
void ese_ray_lua_init(EseLuaEngine *engine) {
    if (luaL_newmetatable(engine->runtime, RAY_PROXY_META)) {
        log_debug("LUA", "Adding entity RayMeta to engine");
        lua_pushstring(engine->runtime, RAY_PROXY_META);
        lua_setfield(engine->runtime, -2, "__name");
        lua_pushcfunction(engine->runtime, _ese_ray_lua_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _ese_ray_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushcfunction(engine->runtime, _ese_ray_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");
        lua_pushcfunction(engine->runtime, _ese_ray_lua_tostring);
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
        lua_pushcfunction(engine->runtime, _ese_ray_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_pushcfunction(engine->runtime, _ese_ray_lua_zero);
        lua_setfield(engine->runtime, -2, "zero");
        lua_setglobal(engine->runtime, "Ray");
    } else {
        lua_pop(engine->runtime, 1);
    }
}

void ese_ray_lua_push(EseRay *ray) {
    log_assert("RAY", ray, "ese_ray_lua_push called with NULL ray");

    if (ray->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseRay **ud = (EseRay **)lua_newuserdata(ray->state, sizeof(EseRay *));
        *ud = ray;

        // Attach metatable
        luaL_getmetatable(ray->state, RAY_PROXY_META);
        lua_setmetatable(ray->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(ray->state, LUA_REGISTRYINDEX, ray->lua_ref);
    }
}

EseRay *ese_ray_lua_get(lua_State *L, int idx) {
    log_assert("RAY", L, "ese_ray_lua_get called with NULL Lua state");
    
    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseRay **ud = (EseRay **)luaL_testudata(L, idx, RAY_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_ray_ref(EseRay *ray) {
    log_assert("RAY", ray, "ese_ray_ref called with NULL ray");
    
    if (ray->lua_ref == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseRay **ud = (EseRay **)lua_newuserdata(ray->state, sizeof(EseRay *));
        *ud = ray;

        // Attach metatable
        luaL_getmetatable(ray->state, RAY_PROXY_META);
        lua_setmetatable(ray->state, -2);

        // Store hard reference to prevent garbage collection
        ray->lua_ref = luaL_ref(ray->state, LUA_REGISTRYINDEX);
        ray->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        ray->lua_ref_count++;
    }

    profile_count_add("ese_ray_ref_count");
}

void ese_ray_unref(EseRay *ray) {
    if (!ray) return;
    
    if (ray->lua_ref != LUA_NOREF && ray->lua_ref_count > 0) {
        ray->lua_ref_count--;
        
        if (ray->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(ray->state, LUA_REGISTRYINDEX, ray->lua_ref);
            ray->lua_ref = LUA_NOREF;
        }
    }

    profile_count_add("ese_ray_unref_count");
}

// ========================================
// OPQUE ACCESSOR FUNCTIONS
// ========================================

size_t ese_ray_sizeof(void) {
    return sizeof(struct EseRay);
}

float ese_ray_get_x(const EseRay *ray) {
    log_assert("RAY", ray, "ese_ray_get_x called with NULL ray");
    return ray->x;
}

void ese_ray_set_x(EseRay *ray, float x) {
    log_assert("RAY", ray, "ese_ray_set_x called with NULL ray");
    ray->x = x;
}

float ese_ray_get_y(const EseRay *ray) {
    log_assert("RAY", ray, "ese_ray_get_y called with NULL ray");
    return ray->y;
}

void ese_ray_set_y(EseRay *ray, float y) {
    log_assert("RAY", ray, "ese_ray_set_y called with NULL ray");
    ray->y = y;
}

float ese_ray_get_dx(const EseRay *ray) {
    log_assert("RAY", ray, "ese_ray_get_dx called with NULL ray");
    return ray->dx;
}

void ese_ray_set_dx(EseRay *ray, float dx) {
    log_assert("RAY", ray, "ese_ray_set_dx called with NULL ray");
    ray->dx = dx;
}

float ese_ray_get_dy(const EseRay *ray) {
    log_assert("RAY", ray, "ese_ray_get_dy called with NULL ray");
    return ray->dy;
}

void ese_ray_set_dy(EseRay *ray, float dy) {
    log_assert("RAY", ray, "ese_ray_set_dy called with NULL ray");
    ray->dy = dy;
}

lua_State *ese_ray_get_state(const EseRay *ray) {
    log_assert("RAY", ray, "ese_ray_get_state called with NULL ray");
    return ray->state;
}

int ese_ray_get_lua_ref(const EseRay *ray) {
    log_assert("RAY", ray, "ray_get_lua_ref called with NULL ray");
    return ray->lua_ref;
}

int ese_ray_get_lua_ref_count(const EseRay *ray) {
    log_assert("RAY", ray, "ese_ray_get_lua_ref_count called with NULL ray");
    return ray->lua_ref_count;
}

// Mathematical operations
bool ese_ray_intersects_rect(const EseRay *ray, const EseRect *rect) {
    log_assert("RAY", ray, "ese_ray_intersects_rect called with NULL ray");
    log_assert("RAY", rect, "ese_ray_intersects_rect called with NULL rect");

    // Simple AABB intersection test for now
    // This could be enhanced with more sophisticated ray-rectangle intersection
    float t_near = -INFINITY;
    float t_far = INFINITY;

    if (ray->dx != 0.0f) {
        float t1 = (ese_rect_get_x(rect) - ray->x) / ray->dx;
        float t2 = (ese_rect_get_x(rect) + ese_rect_get_width(rect) - ray->x) / ray->dx;
        if (t1 > t2) {
            float temp = t1;
            t1 = t2;
            t2 = temp;
        }
        if (t1 > t_near) t_near = t1;
        if (t2 < t_far) t_far = t2;
    } else if (ray->x < ese_rect_get_x(rect) || ray->x > ese_rect_get_x(rect) + ese_rect_get_width(rect)) {
        return false;
    }

    if (ray->dy != 0.0f) {
        float t1 = (ese_rect_get_y(rect) - ray->y) / ray->dy;
        float t2 = (ese_rect_get_y(rect) + ese_rect_get_height(rect) - ray->y) / ray->dy;
        if (t1 > t2) {
            float temp = t1;
            t1 = t2;
            t2 = temp;
        }
        if (t1 > t_near) t_near = t1;
        if (t2 < t_far) t_far = t2;
    } else if (ray->y < ese_rect_get_y(rect) || ray->y > ese_rect_get_y(rect) + ese_rect_get_height(rect)) {
        return false;
    }

    if (t_near > t_far || t_far < 0.0f) {
        return false;
    }

    return true;
}

void ese_ray_get_point_at_distance(const EseRay *ray, float distance, float *out_x, float *out_y) {
    if (!ray || !out_x || !out_y) return;
    
    *out_x = ray->x + ray->dx * distance;
    *out_y = ray->y + ray->dy * distance;
}

void ese_ray_normalize(EseRay *ray) {
    if (!ray) return;
    
    float length = sqrtf(ray->dx * ray->dx + ray->dy * ray->dy);
    if (length > 0.0f) {
        ray->dx /= length;
        ray->dy /= length;
    }
}
