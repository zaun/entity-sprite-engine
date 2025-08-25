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

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseArc *_arc_make(void);

// Lua metamethods
static int _arc_lua_gc(lua_State *L);
static int _arc_lua_index(lua_State *L);
static int _arc_lua_newindex(lua_State *L);
static int _arc_lua_tostring(lua_State *L);

// Lua constructors
static int _arc_lua_new(lua_State *L);
static int _arc_lua_zero(lua_State *L);

// Lua methods
static int _arc_lua_contains_point(lua_State *L);
static int _arc_lua_intersects_rect(lua_State *L);
static int _arc_lua_get_length(lua_State *L);
static int _arc_lua_get_point_at_angle(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
static EseArc *_arc_make() {
    EseArc *arc = (EseArc *)memory_manager.malloc(sizeof(EseArc), MMTAG_GENERAL);
    arc->x = 0.0f;
    arc->y = 0.0f;
    arc->radius = 1.0f;
    arc->start_angle = 0.0f;
    arc->end_angle = 2.0f * M_PI;
    arc->state = NULL;
    arc->lua_ref = LUA_NOREF;
    arc->lua_ref_count = 0;
    return arc;
}

// Lua metamethods
static int _arc_lua_gc(lua_State *L) {
    EseArc *arc = arc_lua_get(L, 1);

    if (arc) {
        // If lua_ref == LUA_NOREF, there are no more references to this arc, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this arc was referenced from C and should not be freed.
        if (arc->lua_ref == LUA_NOREF) {
            arc_destroy(arc);
        }
    }

    return 0;
}

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
    } else if (strcmp(key, "intersects_rect") == 0) {
        lua_pushlightuserdata(L, arc);
        lua_pushcclosure(L, _arc_lua_intersects_rect, 1);
        return 1;
    } else if (strcmp(key, "get_length") == 0) {
        lua_pushlightuserdata(L, arc);
        lua_pushcclosure(L, _arc_lua_get_length, 1);
        return 1;
    } else if (strcmp(key, "get_point_at_angle") == 0) {
        lua_pushlightuserdata(L, arc);
        lua_pushcclosure(L, _arc_lua_get_point_at_angle, 1);
        return 1;
    }
    return 0;
}

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

static int _arc_lua_tostring(lua_State *L) {
    EseArc *arc = arc_lua_get(L, 1);

    if (!arc) {
        lua_pushstring(L, "Arc: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Arc: %p (x=%.2f, y=%.2f, r=%.2f, start=%.2f, end=%.2f)", 
             (void*)arc, arc->x, arc->y, arc->radius, arc->start_angle, arc->end_angle);
    lua_pushstring(L, buf);

    return 1;
}

// Lua constructors
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

    // Create the arc using the standard creation function
    EseArc *arc = _arc_make();
    arc->x = x;
    arc->y = y;
    arc->radius = radius;
    arc->start_angle = start_angle;
    arc->end_angle = end_angle;
    arc->state = L;
    
    // Create proxy table for Lua-owned arc
    lua_newtable(L);
    lua_pushlightuserdata(L, arc);
    lua_setfield(L, -2, "__ptr");

    luaL_getmetatable(L, "ArcProxyMeta");
    lua_setmetatable(L, -2);

    return 1;
}

static int _arc_lua_zero(lua_State *L) {
    // Create the arc using the standard creation function
    EseArc *arc = _arc_make();  // We'll set the state manually
    arc->state = L;
    
    // Create proxy table for Lua-owned arc
    lua_newtable(L);
    lua_pushlightuserdata(L, arc);
    lua_setfield(L, -2, "__ptr");

    luaL_getmetatable(L, "ArcProxyMeta");
    lua_setmetatable(L, -2);

    return 1;
}

// Lua methods
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

static int _arc_lua_get_length(lua_State *L) {
    EseArc *arc = (EseArc *)lua_touserdata(L, lua_upvalueindex(1));
    if (!arc) {
        return luaL_error(L, "Invalid EseArc object in get_length method");
    }
    
    lua_pushnumber(L, arc_get_length(arc));
    return 1;
}

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
    
    if (success) {
        lua_pushboolean(L, true);
        lua_pushnumber(L, x);
        lua_pushnumber(L, y);
        return 3;
    } else {
        lua_pushboolean(L, false);
        return 1;
    }
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseArc *arc_create(EseLuaEngine *engine) {
    EseArc *arc = _arc_make();
    arc->state = engine->runtime;
    return arc;
}

EseArc *arc_copy(const EseArc *source) {
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
    copy->lua_ref_count = 0;
    return copy;
}

void arc_destroy(EseArc *arc) {
    if (!arc) return;
    
    if (arc->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(arc);
    } else {
        // Has Lua references, decrement counter
        if (arc->lua_ref_count > 0) {
            arc->lua_ref_count--;
            
            if (arc->lua_ref_count == 0) {
                // No more C references, unref from Lua registry
                // Let Lua's GC handle the final cleanup
                luaL_unref(arc->state, LUA_REGISTRYINDEX, arc->lua_ref);
                arc->lua_ref = LUA_NOREF;
            }
        }
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

// Lua integration
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

void arc_lua_push(EseArc *arc) {
    log_assert("ARC", arc, "arc_lua_push called with NULL arc");

    if (arc->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new proxy table since we don't store them
        lua_newtable(arc->state);
        lua_pushlightuserdata(arc->state, arc);
        lua_setfield(arc->state, -2, "__ptr");
        
        luaL_getmetatable(arc->state, "ArcProxyMeta");
        lua_setmetatable(arc->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(arc->state, LUA_REGISTRYINDEX, arc->lua_ref);
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

void arc_ref(EseArc *arc) {
    log_assert("ARC", arc, "arc_ref called with NULL arc");
    
    if (arc->lua_ref == LUA_NOREF) {
        // First time referencing - create proxy table and store reference
        lua_newtable(arc->state);
        lua_pushlightuserdata(arc->state, arc);
        lua_setfield(arc->state, -2, "__ptr");

        luaL_getmetatable(arc->state, "ArcProxyMeta");
        lua_setmetatable(arc->state, -2);

        // Store hard reference to prevent garbage collection
        arc->lua_ref = luaL_ref(arc->state, LUA_REGISTRYINDEX);
        arc->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        arc->lua_ref_count++;
    }
}

void arc_unref(EseArc *arc) {
    if (!arc) return;
    
    if (arc->lua_ref != LUA_NOREF && arc->lua_ref_count > 0) {
        arc->lua_ref_count--;
        
        if (arc->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(arc->state, LUA_REGISTRYINDEX, arc->lua_ref);
            arc->lua_ref = LUA_NOREF;
        }
    }
}

// Mathematical operations
bool arc_contains_point(const EseArc *arc, float x, float y, float tolerance) {
    if (!arc) return false;
    
    // Calculate distance from point to arc center
    float dx = x - arc->x;
    float dy = y - arc->y;
    float distance = sqrtf(dx * dx + dy * dy);
    
    // Check if point is within radius tolerance
    if (fabsf(distance - arc->radius) > tolerance) {
        return false;
    }
    
    // Check if point is within angle range
    float angle = atan2f(dy, dx);
    if (angle < 0) {
        angle += 2.0f * M_PI;
    }
    
    // Normalize start and end angles
    float start = arc->start_angle;
    float end = arc->end_angle;
    
    if (end < start) {
        end += 2.0f * M_PI;
    }
    
    return (angle >= start && angle <= end);
}

float arc_get_length(const EseArc *arc) {
    if (!arc) return 0.0f;
    
    float angle_diff = arc->end_angle - arc->start_angle;
    if (angle_diff < 0) {
        angle_diff += 2.0f * M_PI;
    }
    
    return arc->radius * angle_diff;
}

bool arc_get_point_at_angle(const EseArc *arc, float angle, float *out_x, float *out_y) {
    if (!arc || !out_x || !out_y) return false;
    
    // Normalize start and end angles
    float start = arc->start_angle;
    float end = arc->end_angle;
    
    if (end < start) {
        end += 2.0f * M_PI;
    }
    
    // Normalize input angle
    while (angle < start) {
        angle += 2.0f * M_PI;
    }
    
    if (angle > end) {
        return false;
    }
    
    *out_x = arc->x + arc->radius * cosf(angle);
    *out_y = arc->y + arc->radius * sinf(angle);
    
    return true;
}

bool arc_intersects_rect(const EseArc *arc, const EseRect *rect) {
    if (!arc || !rect) return false;
    
    // Simple bounding box check for now
    // This could be enhanced with more sophisticated arc-rectangle intersection
    float arc_left = arc->x - arc->radius;
    float arc_right = arc->x + arc->radius;
    float arc_top = arc->y - arc->radius;
    float arc_bottom = arc->y + arc->radius;
    
    float rect_left = rect->x;
    float rect_right = rect->x + rect->width;
    float rect_top = rect->y;
    float rect_bottom = rect->y + rect->height;
    
    return !(arc_right < rect_left || arc_left > rect_right ||
             arc_bottom < rect_top || arc_top > rect_bottom);
}
