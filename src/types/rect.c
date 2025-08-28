#include <math.h>
#include <stdio.h>
#include <string.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/rect.h"
#include "utility/log.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// The actual EseRect struct definition (private to this file)
typedef struct EseRect {
    float x;            /**< The x-coordinate of the rectangle's top-left corner */
    float y;            /**< The y-coordinate of the rectangle's top-left corner */
    float width;        /**< The width of the rectangle */
    float height;       /**< The height of the rectangle */
    float rotation;     /**< The rotation of the rect around the center point in radians */

    lua_State *state;   /**< Lua State this EseRect belongs to */
    int lua_ref;        /**< Lua registry reference to its own proxy table */
    int lua_ref_count;  /**< Number of times this rect has been referenced in C */
    
    // Watcher system
    EseRectWatcherCallback *watchers;     /**< Array of watcher callbacks */
    void **watcher_userdata;              /**< Array of userdata for each watcher */
    size_t watcher_count;                 /**< Number of registered watchers */
    size_t watcher_capacity;              /**< Capacity of the watcher arrays */
} EseRect;

/**
 * @brief Simple 2D vector structure for mathematical operations.
 * 
 * @details This structure represents a 2D vector with x and y components
 *          for use in collision detection and geometric calculations.
 */
typedef struct { float x, y; } Vec2;

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


/* mini helpers */
static inline float deg_to_rad(float d) { return d * (M_PI / 180.0f); }
static inline float rad_to_deg(float r) { return r * (180.0f / M_PI); }
static inline float dotf(Vec2 a, Vec2 b) { return a.x*b.x + a.y*b.y; }


// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseRect *_rect_make(void);
static void _rect_to_obb(const EseRect *r, OBB *out);

// Collision detection
static bool _obb_overlap(const OBB *A, const OBB *B);

// Lua metamethods
static int _rect_lua_gc(lua_State *L);
static int _rect_lua_index(lua_State *L);
static int _rect_lua_newindex(lua_State *L);
static int _rect_lua_tostring(lua_State *L);

// Lua constructors
static int _rect_lua_new(lua_State *L);
static int _rect_lua_zero(lua_State *L);

// Lua methods
static int _rect_lua_area(lua_State *L);
static int _rect_lua_contains_point(lua_State *L);
static int _rect_lua_intersects(lua_State *L);

// Watcher system
static void _rect_notify_watchers(EseRect *rect);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
static EseRect *_rect_make() {
    EseRect *rect = (EseRect *)memory_manager.malloc(sizeof(EseRect), MMTAG_GENERAL);
    rect->x = 0.0f;
    rect->y = 0.0f;
    rect->width = 0.0f;
    rect->height = 0.0f;
    rect->rotation = 0.0f;
    rect->state = NULL;
    rect->lua_ref = LUA_NOREF;
    rect->lua_ref_count = 0;
    rect->watchers = NULL;
    rect->watcher_userdata = NULL;
    rect->watcher_count = 0;
    rect->watcher_capacity = 0;
    return rect;
}

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

// Collision detection
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

// Lua metamethods
static int _rect_lua_gc(lua_State *L) {
    EseRect *rect = rect_lua_get(L, 1);

    if (rect) {
        // If lua_ref == LUA_NOREF, there are no more references to this rect, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this rect was referenced from C and should not be freed.
        if (rect->lua_ref == LUA_NOREF) {
            rect_destroy(rect);
        }
    }

    return 0;
}

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

static int _rect_lua_newindex(lua_State *L) {
    EseRect *rect = rect_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!rect || !key) return 0;

    if (strcmp(key, "x") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "rect.x must be a number");
        }
        rect->x = (float)lua_tonumber(L, 3);
        _rect_notify_watchers(rect);
        return 0;
    } else if (strcmp(key, "y") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "rect.y must be a number");
        }
        rect->y = (float)lua_tonumber(L, 3);
        _rect_notify_watchers(rect);
        return 0;
    } else if (strcmp(key, "width") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "rect.width must be a number");
        }
        rect->width = (float)lua_tonumber(L, 3);
        _rect_notify_watchers(rect);
        return 0;
    } else if (strcmp(key, "height") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "rect.height must be a number");
        }
        rect->height = (float)lua_tonumber(L, 3);
        _rect_notify_watchers(rect);
        return 0;
    }  else if (strcmp(key, "rotation") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "rect.rotation must be a number (degrees)");
        }
        float deg = (float)lua_tonumber(L, 3);
        rect->rotation = deg_to_rad(deg);
        _rect_notify_watchers(rect);
        return 0;
    }
    return luaL_error(L, "unknown or unassignable property '%s'", key);
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

// Lua constructors
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

    // Create the rect using the standard creation function
    EseRect *rect = _rect_make();
    rect->x = x;
    rect->y = y;
    rect->width = width;
    rect->height = height;
    rect->state = L;
    
    // Create proxy table for Lua-owned rect
    lua_newtable(L);
    lua_pushlightuserdata(L, rect);
    lua_setfield(L, -2, "__ptr");

    luaL_getmetatable(L, "RectProxyMeta");
    lua_setmetatable(L, -2);

    return 1;
}

static int _rect_lua_zero(lua_State *L) {
    // Create the rect using the standard creation function
    EseRect *rect = _rect_make();  // We'll set the state manually
    rect->state = L;
    
    // Create proxy table for Lua-owned rect
    lua_newtable(L);
    lua_pushlightuserdata(L, rect);
    lua_setfield(L, -2, "__ptr");

    luaL_getmetatable(L, "RectProxyMeta");
    lua_setmetatable(L, -2);

    return 1;
}

// Lua methods
static int _rect_lua_area(lua_State *L) {
    EseRect *rect = (EseRect *)lua_touserdata(L, lua_upvalueindex(1));
    if (!rect) {
        return luaL_error(L, "Invalid EseRect object in area method");
    }
    
    lua_pushnumber(L, rect_area(rect));
    return 1;
}

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

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseRect *rect_create(EseLuaEngine *engine) {
    log_assert("RECT", engine, "rect_create called with NULL engine");
    EseRect *rect = _rect_make();
    rect->state = engine->runtime;
    return rect;
}

EseRect *rect_copy(const EseRect *source) {
    log_assert("RECT", source, "rect_copy called with NULL source");
    
    EseRect *copy = (EseRect *)memory_manager.malloc(sizeof(EseRect), MMTAG_GENERAL);
    copy->x = source->x;
    copy->y = source->y;
    copy->width = source->width;
    copy->height = source->height;
    copy->rotation = source->rotation;
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
    copy->lua_ref_count = 0;
    copy->watchers = NULL;
    copy->watcher_userdata = NULL;
    copy->watcher_count = 0;
    copy->watcher_capacity = 0;
    return copy;
}

void rect_destroy(EseRect *rect) {
    if (!rect) return;
    
    // Free watcher arrays if they exist
    if (rect->watchers) {
        memory_manager.free(rect->watchers);
        rect->watchers = NULL;
    }
    if (rect->watcher_userdata) {
        memory_manager.free(rect->watcher_userdata);
        rect->watcher_userdata = NULL;
    }
    rect->watcher_count = 0;
    rect->watcher_capacity = 0;
    
    if (rect->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(rect);
    } else {
        // Has Lua references, decrement counter
        if (rect->lua_ref_count > 0) {
            rect->lua_ref_count--;
            
            if (rect->lua_ref_count == 0) {
                // No more C references, unref from Lua registry
                // Let Lua's GC handle the final cleanup
                luaL_unref(rect->state, LUA_REGISTRYINDEX, rect->lua_ref);
                rect->lua_ref = LUA_NOREF;
            }
        }
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

size_t rect_sizeof(void) {
    return sizeof(EseRect);
}

// Lua integration
void rect_lua_init(EseLuaEngine *engine) {
    log_assert("RECT", engine, "rect_lua_init called with NULL engine");
    if (luaL_newmetatable(engine->runtime, "RectProxyMeta")) {
        log_debug("LUA", "Adding entity RectProxyMeta to engine");
        lua_pushstring(engine->runtime, "RectProxyMeta");
        lua_setfield(engine->runtime, -2, "__name");
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

void rect_lua_push(EseRect *rect) {
    log_assert("RECT", rect, "rect_lua_push called with NULL rect");

    if (rect->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new proxy table since we don't store them
        lua_newtable(rect->state);
        lua_pushlightuserdata(rect->state, rect);
        lua_setfield(rect->state, -2, "__ptr");
        
        luaL_getmetatable(rect->state, "RectProxyMeta");
        lua_setmetatable(rect->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(rect->state, LUA_REGISTRYINDEX, rect->lua_ref);
    }
}

EseRect *rect_lua_get(lua_State *L, int idx) {
    log_assert("RECT", L, "rect_lua_get called with NULL Lua state");
    
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

void rect_ref(EseRect *rect) {
    log_assert("RECT", rect, "rect_ref called with NULL rect");
    
    if (rect->lua_ref == LUA_NOREF) {
        // First time referencing - create proxy table and store reference
        lua_newtable(rect->state);
        lua_pushlightuserdata(rect->state, rect);
        lua_setfield(rect->state, -2, "__ptr");

        luaL_getmetatable(rect->state, "RectProxyMeta");
        lua_setmetatable(rect->state, -2);

        // Store hard reference to prevent garbage collection
        rect->lua_ref = luaL_ref(rect->state, LUA_REGISTRYINDEX);
        rect->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        rect->lua_ref_count++;
    }
}

void rect_unref(EseRect *rect) {
    if (!rect) return;
    
    if (rect->lua_ref != LUA_NOREF && rect->lua_ref_count > 0) {
        rect->lua_ref_count--;
        
        if (rect->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(rect->state, LUA_REGISTRYINDEX, rect->lua_ref);
            rect->lua_ref = LUA_NOREF;
        }
    }
}

// Mathematical operations
bool rect_contains_point(const EseRect *rect, float x, float y) {
    log_assert("RECT", rect, "rect_contains_point called with NULL rect");

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
    log_assert("RECT", rect1, "rect_intersects called with NULL first rect");
    log_assert("RECT", rect2, "rect_intersects called with NULL second rect");

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
    log_assert("RECT", rect, "rect_area called with NULL rect");
    return rect->width * rect->height;
}

// Property access
void rect_set_rotation(EseRect *rect, float radians) {
    log_assert("RECT", rect, "rect_set_rotation called with NULL rect");
    rect->rotation = radians;
    _rect_notify_watchers(rect);
}

float rect_get_rotation(const EseRect *rect) {
    log_assert("RECT", rect, "rect_get_rotation called with NULL rect");
    return rect->rotation;
}

void rect_set_x(EseRect *rect, float x) {
    log_assert("RECT", rect, "rect_set_x called with NULL rect");
    rect->x = x;
    _rect_notify_watchers(rect);
}

float rect_get_x(const EseRect *rect) {
    log_assert("RECT", rect, "rect_get_x called with NULL rect");
    return rect->x;
}

void rect_set_y(EseRect *rect, float y) {
    log_assert("RECT", rect, "rect_set_y called with NULL rect");
    rect->y = y;
    _rect_notify_watchers(rect);
}

float rect_get_y(const EseRect *rect) {
    log_assert("RECT", rect, "rect_get_y called with NULL rect");
    return rect->y;
}

void rect_set_width(EseRect *rect, float width) {
    log_assert("RECT", rect, "rect_set_width called with NULL rect");
    rect->width = width;
    _rect_notify_watchers(rect);
}

float rect_get_width(const EseRect *rect) {
    log_assert("RECT", rect, "rect_get_width called with NULL rect");
    return rect->width;
}

void rect_set_height(EseRect *rect, float height) {
    log_assert("RECT", rect, "rect_set_height called with NULL rect");
    rect->height = height;
    _rect_notify_watchers(rect);
}

float rect_get_height(const EseRect *rect) {
    log_assert("RECT", rect, "rect_get_height called with NULL rect");
    return rect->height;
}

// Lua-related access
lua_State *rect_get_state(const EseRect *rect) {
    log_assert("RECT", rect, "rect_get_state called with NULL rect");
    return rect->state;
}

int rect_get_lua_ref(const EseRect *rect) {
    log_assert("RECT", rect, "rect_get_lua_ref called with NULL rect");
    return rect->lua_ref;
}

int rect_get_lua_ref_count(const EseRect *rect) {
    log_assert("RECT", rect, "rect_get_lua_ref_count called with NULL rect");
    return rect->lua_ref_count;
}

// Watcher system
bool rect_add_watcher(EseRect *rect, EseRectWatcherCallback callback, void *userdata) {
    log_assert("RECT", rect, "rect_add_watcher called with NULL rect");
    log_assert("RECT", callback, "rect_add_watcher called with NULL callback");
    
    // Initialize watcher arrays if this is the first watcher
    if (rect->watcher_count == 0) {
        rect->watcher_capacity = 4; // Start with capacity for 4 watchers
        rect->watchers = memory_manager.malloc(sizeof(EseRectWatcherCallback) * rect->watcher_capacity, MMTAG_GENERAL);
        rect->watcher_userdata = memory_manager.malloc(sizeof(void*) * rect->watcher_capacity, MMTAG_GENERAL);
        rect->watcher_count = 0;
    }
    
    // Expand arrays if needed
    if (rect->watcher_count >= rect->watcher_capacity) {
        size_t new_capacity = rect->watcher_capacity * 2;
        EseRectWatcherCallback *new_watchers = memory_manager.realloc(
            rect->watchers, 
            sizeof(EseRectWatcherCallback) * new_capacity, 
            MMTAG_GENERAL
        );
        void **new_userdata = memory_manager.realloc(
            rect->watcher_userdata, 
            sizeof(void*) * new_capacity, 
            MMTAG_GENERAL
        );
        
        if (!new_watchers || !new_userdata) return false;
        
        rect->watchers = new_watchers;
        rect->watcher_userdata = new_userdata;
        rect->watcher_capacity = new_capacity;
    }
    
    // Add the new watcher
    rect->watchers[rect->watcher_count] = callback;
    rect->watcher_userdata[rect->watcher_count] = userdata;
    rect->watcher_count++;
    
    return true;
}

bool rect_remove_watcher(EseRect *rect, EseRectWatcherCallback callback, void *userdata) {
    log_assert("RECT", rect, "rect_remove_watcher called with NULL rect");
    log_assert("RECT", callback, "rect_remove_watcher called with NULL callback");
    
    for (size_t i = 0; i < rect->watcher_count; i++) {
        if (rect->watchers[i] == callback && rect->watcher_userdata[i] == userdata) {
            // Remove this watcher by shifting remaining ones
            for (size_t j = i; j < rect->watcher_count - 1; j++) {
                rect->watchers[j] = rect->watchers[j + 1];
                rect->watcher_userdata[j] = rect->watcher_userdata[j + 1];
            }
            rect->watcher_count--;
            return true;
        }
    }
    
    return false;
}

// Helper function to notify all watchers
static void _rect_notify_watchers(EseRect *rect) {
    if (!rect || rect->watcher_count == 0) return;
    
    for (size_t i = 0; i < rect->watcher_count; i++) {
        if (rect->watchers[i]) {
            rect->watchers[i](rect, rect->watcher_userdata[i]);
        }
    }
}

// Lua integration
