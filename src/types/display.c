#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "types/display.h"

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseDisplay *_display_state_make(void);

// Lua metamethods
static int _display_state_lua_gc(lua_State *L);
static int _display_state_lua_index(lua_State *L);
static int _display_state_lua_newindex(lua_State *L);
static int _display_state_lua_tostring(lua_State *L);

// Lua constructors
// static int _display_state_lua_new(lua_State *L); // REMOVED

// Lua viewport helpers
static int _display_state_viewport_index(lua_State *L);
static int _display_state_readonly_error(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
static EseDisplay *_display_state_make() {
    EseDisplay *display = (EseDisplay *)memory_manager.malloc(sizeof(EseDisplay), MMTAG_GENERAL);
    display->fullscreen = false;
    display->width = 0;
    display->height = 0;
    display->aspect_ratio = 1.0f;
    display->viewport.width = 0;
    display->viewport.height = 0;
    display->state = NULL;
    display->lua_ref = LUA_NOREF;
    display->lua_ref_count = 0;
    return display;
}

// Lua metamethods
static int _display_state_lua_gc(lua_State *L) {
    EseDisplay *display = display_state_lua_get(L, 1);

    if (display) {
        // If lua_ref == LUA_NOREF, there are no more references to this display, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this display was referenced from C and should not be freed.
        if (display->lua_ref == LUA_NOREF) {
            display_state_destroy(display);
        }
    }

    return 0;
}

static int _display_state_lua_index(lua_State *L) {
    EseDisplay *display = display_state_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!display || !key) return 0;

    // Simple properties
    if (strcmp(key, "fullscreen") == 0) {
        lua_pushboolean(L, display->fullscreen);
        return 1;
    }
    if (strcmp(key, "width") == 0) {
        lua_pushinteger(L, display->width);
        return 1;
    }
    if (strcmp(key, "height") == 0) {
        lua_pushinteger(L, display->height);
        return 1;
    }
    if (strcmp(key, "aspect_ratio") == 0) {
        lua_pushnumber(L, display->aspect_ratio);
        return 1;
    }

    // viewport table proxy (read-only)
    if (strcmp(key, "viewport") == 0) {
        // Create the table
        lua_newtable(L);

        // Create and set the metatable
        lua_newtable(L);
        
        // Set __index closure with the viewport pointer as upvalue
        lua_pushlightuserdata(L, &display->viewport);
        lua_pushcclosure(L, _display_state_viewport_index, 1);
        lua_setfield(L, -2, "__index");
        
        // Set __newindex to error
        lua_pushcfunction(L, _display_state_readonly_error);
        lua_setfield(L, -2, "__newindex");
        
        // Apply metatable to the table
        lua_setmetatable(L, -2);

        return 1;
    }

    return 0;
}

static int _display_state_lua_newindex(lua_State *L) {
    return luaL_error(L, "Display object is read-only");
}

static int _display_state_lua_tostring(lua_State *L) {
    EseDisplay *display = display_state_lua_get(L, 1);
    if (!display) {
        lua_pushstring(L, "Display: (invalid)");
        return 1;
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "Display: %p (%dx%d, %s, viewport: %dx%d)", 
             (void *)display, display->width, display->height,
             display->fullscreen ? "fullscreen" : "windowed",
             display->viewport.width, display->viewport.height);
    lua_pushstring(L, buf);
    return 1;
}

// Lua constructors
// static int _display_state_lua_new(lua_State *L) { // REMOVED
//     // Create the display using the standard creation function
//     EseDisplay *display = _display_state_make();
//     display->state = L;
//     
//     // Create proxy table for Lua-owned display
//     lua_newtable(L);
//     lua_pushlightuserdata(L, display);
//     lua_setfield(L, -2, "__ptr");
// 
//     luaL_getmetatable(L, "DisplayProxyMeta");
//     lua_setmetatable(L, -2);
// 
//     return 1;
// }

// Lua viewport helpers
static int _display_state_viewport_index(lua_State *L) {
    EseViewport *viewport = (EseViewport *)lua_touserdata(L, lua_upvalueindex(1));
    const char *key = lua_tostring(L, 2);
    if (!viewport || !key) return 0;

    if (strcmp(key, "width") == 0) {
        lua_pushinteger(L, viewport->width);
        return 1;
    }
    if (strcmp(key, "height") == 0) {
        lua_pushinteger(L, viewport->height);
        return 1;
    }

    return 0;
}

static int _display_state_readonly_error(lua_State *L) {
    return luaL_error(L, "Display tables are read-only");
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseDisplay *display_state_create(EseLuaEngine *engine) {
    log_assert("DISPLAY_STATE", engine, "display_state_create called with NULL engine");
    log_assert("DISPLAY_STATE", memory_manager.malloc, "memory_manager.malloc is NULL");

    EseDisplay *display = _display_state_make();
    log_assert("DISPLAY_STATE", display, "display_state_create failed to allocate memory");

    display->state = engine->runtime;
    return display;
}

EseDisplay *display_state_copy(const EseDisplay *src) {
    log_assert("DISPLAY_STATE", src, "display_state_copy called with NULL src");
    log_assert("DISPLAY_STATE", memory_manager.malloc, "memory_manager.malloc is NULL");

    EseDisplay *copy = (EseDisplay *)memory_manager.malloc(sizeof(EseDisplay), MMTAG_GENERAL);
    log_assert("DISPLAY_STATE", copy, "display_state_copy failed to allocate memory");

    copy->fullscreen = src->fullscreen;
    copy->width = src->width;
    copy->height = src->height;
    copy->aspect_ratio = src->aspect_ratio;
    copy->viewport = src->viewport; // Struct copy
    copy->state = src->state;
    copy->lua_ref = LUA_NOREF;
    copy->lua_ref_count = 0;

    return copy;
}

void display_state_destroy(EseDisplay *display) {
    if (!display) return;
    
    if (display->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(display);
    } else {
        // Has Lua references, decrement counter
        if (display->lua_ref_count > 0) {
            display->lua_ref_count--;
            
            if (display->lua_ref_count == 0) {
                // No more C references, unref from Lua registry
                // Let Lua's GC handle the final cleanup
                luaL_unref(display->state, LUA_REGISTRYINDEX, display->lua_ref);
                display->lua_ref = LUA_NOREF;
            }
        }
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

// Lua integration
void display_state_lua_init(EseLuaEngine *engine) {
    log_assert("DISPLAY_STATE", engine, "display_state_lua_init called with NULL engine");
    log_assert("DISPLAY_STATE", engine->runtime, "display_state_lua_init called with NULL engine->runtime");

    if (luaL_newmetatable(engine->runtime, "DisplayProxyMeta")) {
        log_debug("LUA", "Adding DisplayProxyMeta to engine");
        lua_pushcfunction(engine->runtime, _display_state_lua_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _display_state_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushcfunction(engine->runtime, _display_state_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");
        lua_pushcfunction(engine->runtime, _display_state_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
    
    // REMOVED: Global Display table creation
}

void display_state_lua_push(EseDisplay *display) {
    log_assert("DISPLAY", display, "display_state_lua_push called with NULL display");

    if (display->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new proxy table since we don't store them
        lua_newtable(display->state);
        lua_pushlightuserdata(display->state, display);
        lua_setfield(display->state, -2, "__ptr");
        
        luaL_getmetatable(display->state, "DisplayProxyMeta");
        lua_setmetatable(display->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(display->state, LUA_REGISTRYINDEX, display->lua_ref);
    }
}

EseDisplay *display_state_lua_get(lua_State *L, int idx) {
    if (!lua_istable(L, idx)) return NULL;

    if (!lua_getmetatable(L, idx)) return NULL;

    luaL_getmetatable(L, "DisplayProxyMeta");
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
    void *ptr = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return (EseDisplay *)ptr;
}

void display_state_ref(EseDisplay *display) {
    log_assert("DISPLAY", display, "display_state_ref called with NULL display");
    
    if (display->lua_ref == LUA_NOREF) {
        // First time referencing - create proxy table and store reference
        lua_newtable(display->state);
        lua_pushlightuserdata(display->state, display);
        lua_setfield(display->state, -2, "__ptr");

        luaL_getmetatable(display->state, "DisplayProxyMeta");
        lua_setmetatable(display->state, -2);

        // Store hard reference to prevent garbage collection
        display->lua_ref = luaL_ref(display->state, LUA_REGISTRYINDEX);
        display->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        display->lua_ref_count++;
    }
}

void display_state_unref(EseDisplay *display) {
    if (!display) return;
    
    if (display->lua_ref != LUA_NOREF && display->lua_ref_count > 0) {
        display->lua_ref_count--;
        
        if (display->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(display->state, LUA_REGISTRYINDEX, display->lua_ref);
            display->lua_ref = LUA_NOREF;
        }
    }
}

// State management
void display_state_set_dimensions(EseDisplay *display, int width, int height) {
    log_assert("DISPLAY_STATE", display, "display_state_set_dimensions called with NULL display");

    display->width = width;
    display->height = height;
    display->aspect_ratio = (height > 0) ? (float)width / (float)height : 1.0f;
}

void display_state_set_fullscreen(EseDisplay *display, bool fullscreen) {
    log_assert("DISPLAY_STATE", display, "display_state_set_fullscreen called with NULL display");
    display->fullscreen = fullscreen;
}

void display_state_set_viewport(EseDisplay *display, int width, int height) {
    log_assert("DISPLAY_STATE", display, "display_state_set_viewport called with NULL display");
    
    display->viewport.width = width;
    display->viewport.height = height;
}
