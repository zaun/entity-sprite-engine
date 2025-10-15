#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/display.h"
#include "types/display_private.h"

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseDisplay *_ese_display_make(void);

// Lua metamethods
static int _ese_display_lua_gc(lua_State *L);
static int _ese_display_lua_index(lua_State *L);
static int _ese_display_lua_newindex(lua_State *L);
static int _ese_display_lua_tostring(lua_State *L);

// Lua constructors
// static int _ese_display_lua_new(lua_State *L); // REMOVED

// Lua viewport helpers
static int _ese_display_viewport_index(lua_State *L);
static int _ese_display_readonly_error(lua_State *L);

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
static EseDisplay *_ese_display_make() {
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
static int _ese_display_lua_gc(lua_State *L) {
    // Get from userdata
    EseDisplay **ud = (EseDisplay **)luaL_testudata(L, 1, "DisplayMeta");
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseDisplay *display = *ud;
    if (display) {
        // If lua_ref == LUA_NOREF, there are no more references to this display, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this display was referenced from C and should not be freed.
        if (display->lua_ref == LUA_NOREF) {
            ese_display_destroy(display);
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
static int _ese_display_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_DISPLAY_INDEX);
    EseDisplay *display = ese_display_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!display || !key) {
        profile_cancel(PROFILE_LUA_DISPLAY_INDEX);
        return 0;
    }

    // Simple properties
    if (strcmp(key, "fullscreen") == 0) {
        lua_pushboolean(L, display->fullscreen);
        profile_stop(PROFILE_LUA_DISPLAY_INDEX, "ese_display_lua_index (fullscreen)");
        return 1;
    }
    if (strcmp(key, "width") == 0) {
        lua_pushinteger(L, display->width);
        profile_stop(PROFILE_LUA_DISPLAY_INDEX, "ese_display_lua_index (width)");
        return 1;
    }
    if (strcmp(key, "height") == 0) {
        lua_pushinteger(L, display->height);
        profile_stop(PROFILE_LUA_DISPLAY_INDEX, "ese_display_lua_index (height)");
        return 1;
    }
    if (strcmp(key, "aspect_ratio") == 0) {
        lua_pushnumber(L, display->aspect_ratio);
        profile_stop(PROFILE_LUA_DISPLAY_INDEX, "ese_display_lua_index (aspect_ratio)");
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
        lua_pushcclosure(L, _ese_display_viewport_index, 1);
        lua_setfield(L, -2, "__index");
        
        // Set __newindex to error
        lua_pushcfunction(L, _ese_display_readonly_error);
        lua_setfield(L, -2, "__newindex");
        
        // Apply metatable to the table
        lua_setmetatable(L, -2);

        profile_stop(PROFILE_LUA_DISPLAY_INDEX, "ese_display_lua_index (viewport)");
        return 1;
    }

    profile_stop(PROFILE_LUA_DISPLAY_INDEX, "ese_display_lua_index (invalid)");
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
static int _ese_display_lua_newindex(lua_State *L) {
    profile_start(PROFILE_LUA_DISPLAY_NEWINDEX);
    profile_stop(PROFILE_LUA_DISPLAY_NEWINDEX, "ese_display_lua_newindex (error)");
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
static int _ese_display_lua_tostring(lua_State *L) {
    EseDisplay *display = ese_display_lua_get(L, 1);
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
// static int _ese_display_lua_new(lua_State *L) { // REMOVED
//     // Create the display using the standard creation function
//     EseDisplay *display = _ese_display_make();
//     display->state = L;
//     
//     // Create proxy table for Lua-owned display
//     lua_newtable(L);
//     lua_pushlightuserdata(L, display);
//     lua_setfield(L, -2, "__ptr");
// 
//     luaL_getmetatable(L, "DisplayMeta");
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
static int _ese_display_viewport_index(lua_State *L) {
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
static int _ese_display_readonly_error(lua_State *L) {
    return luaL_error(L, "Display tables are read-only");
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseDisplay *ese_display_create(EseLuaEngine *engine) {
    log_assert("DISPLAY_STATE", engine, "ese_display_create called with NULL engine");
    log_assert("DISPLAY_STATE", memory_manager.malloc, "memory_manager.malloc is NULL");

    EseDisplay *display = _ese_display_make();
    log_assert("DISPLAY_STATE", display, "ese_display_create failed to allocate memory");

    display->state = engine->runtime;
    return display;
}

EseDisplay *ese_display_copy(const EseDisplay *src) {
    log_assert("DISPLAY_STATE", src, "ese_display_copy called with NULL src");
    log_assert("DISPLAY_STATE", memory_manager.malloc, "memory_manager.malloc is NULL");

    EseDisplay *copy = (EseDisplay *)memory_manager.malloc(sizeof(EseDisplay), MMTAG_DISPLAY);
    log_assert("DISPLAY_STATE", copy, "ese_display_copy failed to allocate memory");

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

void ese_display_destroy(EseDisplay *display) {
    if (!display) return;
    
    if (display->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(display);
    } else {
        ese_display_unref(display);
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

// Lua integration
void ese_display_lua_init(EseLuaEngine *engine) {
    log_assert("DISPLAY_STATE", engine, "ese_display_lua_init called with NULL engine");
    log_assert("DISPLAY_STATE", engine->runtime, "ese_display_lua_init called with NULL engine->runtime");

    // Create metatable
    lua_engine_new_object_meta(engine, "DisplayMeta", 
        _ese_display_lua_index, 
        _ese_display_lua_newindex, 
        _ese_display_lua_gc, 
        _ese_display_lua_tostring);    
}

void ese_display_lua_push(EseDisplay *display) {
    log_assert("DISPLAY", display, "ese_display_lua_push called with NULL display");

    if (display->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseDisplay **ud = (EseDisplay **)lua_newuserdata(display->state, sizeof(EseDisplay *));
        *ud = display;

        // Attach metatable
        luaL_getmetatable(display->state, "DisplayMeta");
        lua_setmetatable(display->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(display->state, LUA_REGISTRYINDEX, display->lua_ref);
    }
}

EseDisplay *ese_display_lua_get(lua_State *L, int idx) {
    log_assert("DISPLAY", L, "ese_display_lua_get called with NULL Lua state");
    
    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseDisplay **ud = (EseDisplay **)luaL_testudata(L, idx, "DisplayMeta");
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_display_ref(EseDisplay *display) {
    log_assert("DISPLAY", display, "ese_display_ref called with NULL display");
    
    if (display->lua_ref == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseDisplay **ud = (EseDisplay **)lua_newuserdata(display->state, sizeof(EseDisplay *));
        *ud = display;

        // Attach metatable
        luaL_getmetatable(display->state, "DisplayMeta");
        lua_setmetatable(display->state, -2);

        // Store hard reference to prevent garbage collection
        display->lua_ref = luaL_ref(display->state, LUA_REGISTRYINDEX);
        display->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        display->lua_ref_count++;
    }

    profile_count_add("ese_display_ref_count");
}

void ese_display_unref(EseDisplay *display) {
    if (!display) return;
    
    if (display->lua_ref != LUA_NOREF && display->lua_ref_count > 0) {
        display->lua_ref_count--;
        
        if (display->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(display->state, LUA_REGISTRYINDEX, display->lua_ref);
            display->lua_ref = LUA_NOREF;
        }
    }

    profile_count_add("ese_display_unref_count");
}

// State management
void ese_display_set_dimensions(EseDisplay *display, int width, int height) {
    log_assert("DISPLAY_STATE", display, "ese_display_set_dimensions called with NULL display");

    display->width = width;
    display->height = height;
    display->aspect_ratio = (height > 0) ? (float)width / (float)height : 1.0f;
}

void ese_display_set_fullscreen(EseDisplay *display, bool fullscreen) {
    log_assert("DISPLAY_STATE", display, "ese_display_set_fullscreen called with NULL display");
    display->fullscreen = fullscreen;
}

void ese_display_set_viewport(EseDisplay *display, int width, int height) {
    log_assert("DISPLAY_STATE", display, "ese_display_set_viewport called with NULL display");
    
    display->viewport.width = width;
    display->viewport.height = height;
}

// Getter functions
bool ese_display_get_fullscreen(const EseDisplay *display) {
    log_assert("DISPLAY_STATE", display, "ese_display_get_fullscreen called with NULL display");
    return display->fullscreen;
}

int ese_display_get_width(const EseDisplay *display) {
    log_assert("DISPLAY_STATE", display, "ese_display_get_width called with NULL display");
    return display->width;
}

int ese_display_get_height(const EseDisplay *display) {
    log_assert("DISPLAY_STATE", display, "ese_display_get_height called with NULL display");
    return display->height;
}

float ese_display_get_aspect_ratio(const EseDisplay *display) {
    log_assert("DISPLAY_STATE", display, "ese_display_get_aspect_ratio called with NULL display");
    return display->aspect_ratio;
}

int ese_display_get_viewport_width(const EseDisplay *display) {
    log_assert("DISPLAY_STATE", display, "ese_display_get_viewport_width called with NULL display");
    return display->viewport.width;
}

int ese_display_get_viewport_height(const EseDisplay *display) {
    log_assert("DISPLAY_STATE", display, "ese_display_get_viewport_height called with NULL display");
    return display->viewport.height;
}

lua_State *ese_display_get_state(const EseDisplay *display) {
    log_assert("DISPLAY_STATE", display, "ese_display_get_state called with NULL display");
    return display->state;
}

int ese_display_get_lua_ref_count(const EseDisplay *display) {
    log_assert("DISPLAY_STATE", display, "ese_display_get_lua_ref_count called with NULL display");
    return display->lua_ref_count;
}

int ese_display_get_lua_ref(const EseDisplay *display) {
    log_assert("DISPLAY_STATE", display, "ese_display_get_lua_ref called with NULL display");
    return display->lua_ref;
}

size_t ese_display_sizeof(void) {
    return sizeof(EseDisplay);
}
