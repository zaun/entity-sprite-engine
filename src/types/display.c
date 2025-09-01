#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
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
/**
 * @brief Creates a new EseDisplay instance with default values
 * 
 * Allocates memory for a new EseDisplay and initializes all fields to safe defaults.
 * The display starts in windowed mode with zero dimensions, 1.0 aspect ratio, and no Lua state or references.
 * 
 * @return Pointer to the newly created EseDisplay, or NULL on allocation failure
 */
static EseDisplay *_display_state_make() {
    EseDisplay *display = (EseDisplay *)memory_manager.malloc(sizeof(EseDisplay), MMTAG_DISPLAY);
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
/**
 * @brief Lua garbage collection metamethod for EseDisplay
 * 
 * Handles cleanup when a Lua proxy table for an EseDisplay is garbage collected.
 * Only frees the underlying EseDisplay if it has no C-side references.
 * 
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static int _display_state_lua_gc(lua_State *L) {
    // Try to get from userdata (GC guard)
    EseDisplay **ud = (EseDisplay **)luaL_testudata(L, 1, "DisplayProxyMeta");
    EseDisplay *display = NULL;
    if (ud) {
        display = *ud;
    } else {
        // Fallback: maybe called on a table (unlikely, but safe)
        display = display_state_lua_get(L, 1);
    }

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

/**
 * @brief Lua __index metamethod for EseDisplay property access
 * 
 * Provides read access to display properties from Lua. When a Lua script
 * accesses display.width, display.height, display.fullscreen, etc., this function is called
 * to retrieve the values. Creates read-only proxy tables for viewport data.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid properties, 0 for invalid)
 */
static int _display_state_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_DISPLAY_INDEX);
    EseDisplay *display = display_state_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!display || !key) {
        profile_cancel(PROFILE_LUA_DISPLAY_INDEX);
        return 0;
    }

    // Simple properties
    if (strcmp(key, "fullscreen") == 0) {
        lua_pushboolean(L, display->fullscreen);
        profile_stop(PROFILE_LUA_DISPLAY_INDEX, "display_state_lua_index (fullscreen)");
        return 1;
    }
    if (strcmp(key, "width") == 0) {
        lua_pushinteger(L, display->width);
        profile_stop(PROFILE_LUA_DISPLAY_INDEX, "display_state_lua_index (width)");
        return 1;
    }
    if (strcmp(key, "height") == 0) {
        lua_pushinteger(L, display->height);
        profile_stop(PROFILE_LUA_DISPLAY_INDEX, "display_state_lua_index (height)");
        return 1;
    }
    if (strcmp(key, "aspect_ratio") == 0) {
        lua_pushnumber(L, display->aspect_ratio);
        profile_stop(PROFILE_LUA_DISPLAY_INDEX, "display_state_lua_index (aspect_ratio)");
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

        profile_stop(PROFILE_LUA_DISPLAY_INDEX, "display_state_lua_index (viewport)");
        return 1;
    }

    profile_stop(PROFILE_LUA_DISPLAY_INDEX, "display_state_lua_index (invalid)");
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseDisplay property assignment
 * 
 * Provides write access to display properties from Lua. Since display state is read-only,
 * this function always returns an error for any property assignment attempts.
 * 
 * @param L Lua state
 * @return Never returns (always calls luaL_error)
 */
static int _display_state_lua_newindex(lua_State *L) {
    profile_start(PROFILE_LUA_DISPLAY_NEWINDEX);
    profile_stop(PROFILE_LUA_DISPLAY_NEWINDEX, "display_state_lua_newindex (error)");
    return luaL_error(L, "Display object is read-only");
}

/**
 * @brief Lua __tostring metamethod for EseDisplay string representation
 * 
 * Converts an EseDisplay to a human-readable string for debugging and display.
 * The format includes the memory address, dimensions, fullscreen status, and viewport size.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
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
/**
 * @brief Lua helper function for accessing viewport properties
 * 
 * Provides read-only access to viewport properties (width, height) from Lua.
 * This function is used as the __index metamethod for the viewport proxy table.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid properties, 0 for invalid)
 */
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

/**
 * @brief Lua helper function for read-only error handling
 * 
 * Called when any attempt is made to modify read-only display state tables.
 * Always returns an error indicating that the tables are read-only.
 * 
 * @param L Lua state
 * @return Never returns (always calls luaL_error)
 */
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

    EseDisplay *copy = (EseDisplay *)memory_manager.malloc(sizeof(EseDisplay), MMTAG_DISPLAY);
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
        display_state_unref(display);
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
        lua_pushstring(engine->runtime, "DisplayProxyMeta");
        lua_setfield(engine->runtime, -2, "__name");
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

        // Create hidden userdata for GC
        EseDisplay **ud = (EseDisplay **)lua_newuserdata(display->state, sizeof(EseDisplay *));
        *ud = display;

        // Attach metatable with __gc
        luaL_getmetatable(display->state, "DisplayProxyMeta");
        lua_setmetatable(display->state, -2);

        // Store userdata inside the table (hidden field)
        lua_setfield(display->state, -2, "__gc_guard");

        // Finally set the table's metatable (for __index, __newindex, etc.)
        luaL_getmetatable(display->state, "DisplayProxyMeta");
        lua_setmetatable(display->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(display->state, LUA_REGISTRYINDEX, display->lua_ref);
    }
}

EseDisplay *display_state_lua_get(lua_State *L, int idx) {
    log_assert("DISPLAY", L, "display_state_lua_get called with NULL Lua state");
    
    // Check if the value at idx is a table
    if (!lua_istable(L, idx)) {
        return NULL;
    }
    
    // Check if it has the correct metatable
    if (!lua_getmetatable(L, idx)) {
        return NULL; // No metatable
    }
    
    // Get the expected metatable for comparison
    luaL_getmetatable(L, "DisplayProxyMeta");
    
    // Compare metatables
    if (!lua_rawequal(L, -1, -2)) {
        lua_pop(L, 2); // Pop both metatables
        return NULL; // Wrong metatable
    }
    
    lua_pop(L, 2); // Pop both metatables
    
    // Get the __ptr field
    lua_getfield(L, idx, "__ptr");
    
    // Check if __ptr exists and is light userdata
    if (!lua_islightuserdata(L, -1)) {
        lua_pop(L, 1); // Pop the __ptr value (or nil)
        return NULL;
    }
    
    // Extract the pointer
    void *ptr = lua_touserdata(L, -1);
    lua_pop(L, 1); // Pop the __ptr value
    
    return (EseDisplay *)ptr;
}

void display_state_ref(EseDisplay *display) {
    log_assert("DISPLAY", display, "display_state_ref called with NULL display");
    
    if (display->lua_ref == LUA_NOREF) {
        // First time referencing - create proxy table and store reference
        lua_newtable(display->state);
        lua_pushlightuserdata(display->state, display);
        lua_setfield(display->state, -2, "__ptr");

        // Create hidden userdata for GC
        EseDisplay **ud = (EseDisplay **)lua_newuserdata(display->state, sizeof(EseDisplay *));
        *ud = display;

        // Attach metatable with __gc
        luaL_getmetatable(display->state, "DisplayProxyMeta");
        lua_setmetatable(display->state, -2);

        // Store userdata inside the table (hidden field)
        lua_setfield(display->state, -2, "__gc_guard");

        // Finally set the table's metatable (for __index, __newindex, etc.)
        luaL_getmetatable(display->state, "DisplayProxyMeta");
        lua_setmetatable(display->state, -2);

        // Store hard reference to prevent garbage collection
        display->lua_ref = luaL_ref(display->state, LUA_REGISTRYINDEX);
        display->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        display->lua_ref_count++;
    }

    profile_count_add("display_state_ref_count");
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

    profile_count_add("display_state_unref_count");
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
