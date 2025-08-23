#include <math.h>
#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "types/rect.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* conversions */
static inline float deg_to_rad(float d) { return d * (M_PI / 180.0f); }
static inline float rad_to_deg(float r) { return r * (180.0f / M_PI); }

/**
 * @brief Simple 2D vector structure for mathematical operations.
 * 
 * @details This structure represents a 2D vector with x and y components
 *          for use in collision detection and geometric calculations.
 */
typedef struct { float x, y; } Vec2;
static inline float dotf(Vec2 a, Vec2 b) { return a.x*b.x + a.y*b.y; }

/**
 * @brief Oriented bounding box structure for collision detection.
 * 
 * @details This structure represents an oriented bounding box using
 *          center point, normalized axes, and half-widths for efficient
 *          collision detection using the Separating Axis Theorem (SAT).
 */
typedef struct {
    Vec2 c;                         /**< Center point of the bounding box */
    Vec2 axis[2];                   /**< Normalized local axes in world space */
    float ext[2];                   /**< Half-widths along each axis */
} OBB;


static void _rect_to_obb(const EseRect *r, OBB *out) {
    float cx = r->x + r->width * 0.5f;
    float cy = r->y + r->height * 0.5f;
    float a = r->rotation;
    float ca = cosf(a);
    float sa = sinf(a);

    out->c.x = cx;
    out->c.y = cy;
    out->axis[0].x = ca; out->axis[0].y = sa;
    out->axis[1].x = -sa; out->axis[1].y = ca;
    out->ext[0] = r->width * 0.5f;
    out->ext[1] = r->height * 0.5f;
}

/* SAT test for two OBBs */
static bool _obb_overlap(const OBB *A, const OBB *B) {
    const float EPS = 1e-6f;
    Vec2 d = { B->c.x - A->c.x, B->c.y - A->c.y };

    /* test A's axes */
    for (int i = 0; i < 2; ++i) {
        Vec2 axis = A->axis[i];
        float ra = A->ext[i];
        float rb = B->ext[0] * fabsf(dotf(B->axis[0], axis))
                 + B->ext[1] * fabsf(dotf(B->axis[1], axis));
        float dist = fabsf(dotf(d, axis));
        if (dist > ra + rb + EPS) return false;
    }

    /* test B's axes */
    for (int i = 0; i < 2; ++i) {
        Vec2 axis = B->axis[i];
        float ra = A->ext[0] * fabsf(dotf(A->axis[0], axis))
                 + A->ext[1] * fabsf(dotf(A->axis[1], axis));
        float rb = B->ext[i];
        float dist = fabsf(dotf(d, axis));
        if (dist > ra + rb + EPS) return false;
    }

    return true;
}

/**
 * @brief Pushes a EseRect pointer as a Lua userdata object onto the stack.
 * 
 * @details Creates a new Lua table that acts as a proxy for the EseRect object,
 *          storing the C pointer as light userdata in the "__ptr" field and
 *          setting the RectProxyMeta metatable for property access.
 * 
 * @param rect Pointer to the EseRect object to wrap for Lua access
 * @param is_lua_owned True if LUA will handle freeing
 * 
 * @warning The EseRect object must remain valid for the lifetime of the Lua object
 */
void _rect_lua_register(EseRect *rect, bool is_lua_owned) {
    log_assert("RECT", rect, "_rect_lua_register called with NULL rect");
    log_assert("RECT", rect->lua_ref == LUA_NOREF, "_rect_lua_register rect is already registered");

    lua_newtable(rect->state);
    lua_pushlightuserdata(rect->state, rect);
    lua_setfield(rect->state, -2, "__ptr");

    // Store the ownership flag
    lua_pushboolean(rect->state, is_lua_owned);
    lua_setfield(rect->state, -2, "__is_lua_owned");

    luaL_getmetatable(rect->state, "RectProxyMeta");
    lua_setmetatable(rect->state, -2);

    // Store a reference to this proxy table in the Lua registry
    rect->lua_ref = luaL_ref(rect->state, LUA_REGISTRYINDEX);
}

void rect_lua_push(EseRect *rect) {
    log_assert("RECT", rect, "rect_lua_push called with NULL rect");
    log_assert("RECT", rect->lua_ref != LUA_NOREF, "rect_lua_push rect not registered with lua");

    // Push the proxy table back onto the stack for Lua to receive
    lua_rawgeti(rect->state, LUA_REGISTRYINDEX, rect->lua_ref);
}

/**
 * @brief Lua function to create a new EseRect object.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new rect object)
 */
static int _rect_lua_new(lua_State *L) {
    float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;

    int n_args = lua_gettop(L);
    if (n_args == 4) {
        if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || 
            !lua_isnumber(L, 3) || !lua_isnumber(L, 4)) {
            return luaL_error(L, "all arguments must be numbers");
        }
        x = (float)lua_tonumber(L, 1);
        y = (float)lua_tonumber(L, 2);
        width = (float)lua_tonumber(L, 3);
        height = (float)lua_tonumber(L, 4);
    } else if (n_args != 0) {
        return luaL_error(L, "new() takes 0 or 4 arguments (x, y, width, height)");
    }

    EseRect *rect = (EseRect *)memory_manager.malloc(sizeof(EseRect), MMTAG_GENERAL);
    rect->x = x;
    rect->y = y;
    rect->width = width;
    rect->height = height;
    rect->rotation = 0.0f;
    rect->state = L;
    rect->lua_ref = LUA_NOREF;
    _rect_lua_register(rect, true);

    rect_lua_push(rect);
    return 1;
}

/**
 * @brief Lua function to create a zero rect.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new rect object)
 */
static int _rect_lua_zero(lua_State *L) {
    EseRect *rect = (EseRect *)memory_manager.malloc(sizeof(EseRect), MMTAG_GENERAL);
    rect->x = 0.0f;
    rect->y = 0.0f;
    rect->width = 0.0f;
    rect->height = 0.0f;
    rect->rotation = 0.0f;
    rect->state = L;
    rect->lua_ref = LUA_NOREF;
    _rect_lua_register(rect, true);

    rect_lua_push(rect);
    return 1;
}

/**
 * @brief Lua method to check if point is inside rectangle.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - boolean result)
 */
static int _rect_lua_contains_point(lua_State *L) {
    EseRect *rect = (EseRect *)lua_touserdata(L, lua_upvalueindex(1));
    if (!rect) {
        return luaL_error(L, "Invalid EseRect object in contains_point method");
    }
    
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        return luaL_error(L, "contains_point(x, y) requires two numbers");
    }
    
    float x = (float)lua_tonumber(L, 1);
    float y = (float)lua_tonumber(L, 2);
    
    lua_pushboolean(L, rect_contains_point(rect, x, y));
    return 1;
}

/**
 * @brief Lua method to check if rectangles intersect.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - boolean result)
 */
static int _rect_lua_intersects(lua_State *L) {
    EseRect *rect = (EseRect *)lua_touserdata(L, lua_upvalueindex(1));
    if (!rect) {
        return luaL_error(L, "Invalid EseRect object in intersects method");
    }
    
    EseRect *other = rect_lua_get(L, 1);
    if (!other) {
        return luaL_error(L, "intersects() requires another EseRect object");
    }
    
    lua_pushboolean(L, rect_intersects(rect, other));
    return 1;
}

/**
 * @brief Lua method to get rectangle area.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the area)
 */
static int _rect_lua_area(lua_State *L) {
    EseRect *rect = (EseRect *)lua_touserdata(L, lua_upvalueindex(1));
    if (!rect) {
        return luaL_error(L, "Invalid EseRect object in area method");
    }
    
    lua_pushnumber(L, rect_area(rect));
    return 1;
}

/**
 * @brief Lua __index metamethod for EseRect objects (getter).
 * 
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack
 */
static int _rect_lua_index(lua_State *L) {
    EseRect *rect = rect_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!rect || !key) return 0;

    if (strcmp(key, "x") == 0) {
        lua_pushnumber(L, rect->x);
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(L, rect->y);
        return 1;
    } else if (strcmp(key, "width") == 0) {
        lua_pushnumber(L, rect->width);
        return 1;
    } else if (strcmp(key, "height") == 0) {
        lua_pushnumber(L, rect->height);
        return 1;
    } else if (strcmp(key, "rotation") == 0) {
        lua_pushnumber(L, (double)rad_to_deg(rect->rotation));
        return 1;
    } else if (strcmp(key, "contains_point") == 0) {
        lua_pushlightuserdata(L, rect);
        lua_pushcclosure(L, _rect_lua_contains_point, 1);
        return 1;
    } else if (strcmp(key, "intersects") == 0) {
        lua_pushlightuserdata(L, rect);
        lua_pushcclosure(L, _rect_lua_intersects, 1);
        return 1;
    } else if (strcmp(key, "area") == 0) {
        lua_pushlightuserdata(L, rect);
        lua_pushcclosure(L, _rect_lua_area, 1);
        return 1;
    }
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseRect objects (setter).
 * 
 * @param L Lua state pointer
 * @return Always returns 0 or throws Lua error for invalid operations
 */
static int _rect_lua_newindex(lua_State *L) {
    EseRect *rect = rect_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!rect || !key) return 0;

    if (strcmp(key, "x") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "rect.x must be a number");
        }
        rect->x = (float)lua_tonumber(L, 3);
        return 0;
    } else if (strcmp(key, "y") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "rect.y must be a number");
        }
        rect->y = (float)lua_tonumber(L, 3);
        return 0;
    } else if (strcmp(key, "width") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "rect.width must be a number");
        }
        rect->width = (float)lua_tonumber(L, 3);
        return 0;
    } else if (strcmp(key, "height") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "rect.height must be a number");
        }
        rect->height = (float)lua_tonumber(L, 3);
        return 0;
    }  else if (strcmp(key, "rotation") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "rect.rotation must be a number (degrees)");
        }
        float deg = (float)lua_tonumber(L, 3);
        rect->rotation = deg_to_rad(deg);
        return 0;
    }
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __gc metamethod for EseRect objects.
 * 
 * @param L Lua state pointer
 * @return Always 0 (no return values)
 */
static int _rect_lua_gc(lua_State *L) {
    EseRect *rect = rect_lua_get(L, 1);

    if (rect) {
        lua_getfield(L, 1, "__is_lua_owned");
        bool is_lua_owned = lua_toboolean(L, -1);
        lua_pop(L, 1);

        if (is_lua_owned) {
            rect_destroy(rect);
            log_debug("LUA_GC", "Rect object (Lua-owned) garbage collected and C memory freed.");
        } else {
            log_debug("LUA_GC", "Rect object (C-owned) garbage collected, C memory *not* freed.");
        }
    }

    return 0;
}

static int _rect_lua_tostring(lua_State *L) {
    EseRect *rect = rect_lua_get(L, 1);

    if (!rect) {
        lua_pushstring(L, "Rect: (invalid)");
        return 1;
    }

    char buf[160];
    snprintf(
        buf, sizeof(buf),
        "Rect: %p (x=%.2f, y=%.2f, w=%.2f, h=%.2f, rot=%.2fdeg)",
        (void*)rect, rect->x, rect->y, rect->width, rect->height,
        (double)rad_to_deg(rect->rotation)
    );
    lua_pushstring(L, buf);

    return 1;
}

void rect_lua_init(EseLuaEngine *engine) {
    if (luaL_newmetatable(engine->runtime, "RectProxyMeta")) {
        log_debug("LUA", "Adding entity RectProxyMeta to engine");
        lua_pushcfunction(engine->runtime, _rect_lua_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _rect_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushcfunction(engine->runtime, _rect_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");
        lua_pushcfunction(engine->runtime, _rect_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
    
    // Create global EseRect table with constructor
    lua_getglobal(engine->runtime, "Rect");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);
        log_debug("LUA", "Creating global EseRect table");
        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _rect_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_pushcfunction(engine->runtime, _rect_lua_zero);
        lua_setfield(engine->runtime, -2, "zero");
        lua_setglobal(engine->runtime, "Rect");
    } else {
        lua_pop(engine->runtime, 1);
    }
}

EseRect *rect_create(EseLuaEngine *engine, bool c_only) {
    EseRect *rect = (EseRect *)memory_manager.malloc(sizeof(EseRect), MMTAG_GENERAL);
    rect->x = 0.0f;
    rect->y = 0.0f;
    rect->width = 0.0f;
    rect->height = 0.0f;
    rect->rotation = 0.0f;
    rect->state = engine->runtime;
    rect->lua_ref = LUA_NOREF;
    if (!c_only) {
        _rect_lua_register(rect, false);
    }
    return rect;
}

EseRect *rect_copy(const EseRect *source, bool c_only) {
    if (source == NULL) {
        return NULL;
    }

    EseRect *copy = (EseRect *)memory_manager.malloc(sizeof(EseRect), MMTAG_GENERAL);
    copy->x = source->x;
    copy->y = source->y;
    copy->width = source->width;
    copy->height = source->height;
    copy->rotation = source->rotation;
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
    if (!c_only) {
        _rect_lua_register(copy, false);
    }
    return copy;
}

void rect_destroy(EseRect *rect) {
    if (rect) {
        if (rect->lua_ref != LUA_NOREF) {
            luaL_unref(rect->state, LUA_REGISTRYINDEX, rect->lua_ref);
        }
        memory_manager.free(rect);
    }
}

EseRect *rect_lua_get(lua_State *L, int idx) {
    if (!lua_istable(L, idx)) {
        return NULL;
    }
    
    if (!lua_getmetatable(L, idx)) {
        return NULL;
    }
    
    luaL_getmetatable(L, "RectProxyMeta");
    
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
    
    void *rect = lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    return (EseRect *)rect;
}

void rect_set_rotation(EseRect *rect, float radians) {
    if (!rect) return;
    rect->rotation = radians;
}

float rect_get_rotation(const EseRect *rect) {
    if (!rect) return 0.0f;
    return rect->rotation;
}

bool rect_contains_point(const EseRect *rect, float x, float y) {
    if (!rect) return false;

    /* fast AABB path */
    if (fabsf(rect->rotation) < 1e-6f) {
        return (x >= rect->x && x <= rect->x + rect->width &&
                y >= rect->y && y <= rect->y + rect->height);
    }

    /* transform point to rect-local coordinates by rotating around center by -rotation */
    float cx = rect->x + rect->width * 0.5f;
    float cy = rect->y + rect->height * 0.5f;
    float ca = cosf(rect->rotation);
    float sa = sinf(rect->rotation);

    float dx = x - cx;
    float dy = y - cy;

    float localX = ca * dx + sa * dy;
    float localY = -sa * dx + ca * dy;

    float halfW = rect->width * 0.5f;
    float halfH = rect->height * 0.5f;

    return (localX >= -halfW && localX <= halfW && localY >= -halfH && localY <= halfH);
}

bool rect_intersects(const EseRect *rect1, const EseRect *rect2) {
    if (!rect1 || !rect2) return false;

    /* fast AABB path if both rotations are effectively zero */
    if (fabsf(rect1->rotation) < 1e-6f && fabsf(rect2->rotation) < 1e-6f) {
        return !(rect1->x > rect2->x + rect2->width ||
                 rect2->x > rect1->x + rect1->width ||
                 rect1->y > rect2->y + rect2->height ||
                 rect2->y > rect1->y + rect1->height);
    }

    OBB a, b;
    _rect_to_obb(rect1, &a);
    _rect_to_obb(rect2, &b);
    return _obb_overlap(&a, &b);
}

float rect_area(const EseRect *rect) {
    if (!rect) return 0.0f;
    return rect->width * rect->height;
}
