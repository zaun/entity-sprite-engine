#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "scripting/lua_value.h"
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
static EseLuaValue* _ese_display_lua_gc(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_display_lua_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_display_lua_newindex(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_display_lua_tostring(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

// Lua constructors
// static EseLuaValue* _ese_display_lua_new(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]); // REMOVED

// Lua viewport helpers
static EseLuaValue* _ese_display_viewport_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_display_readonly_error(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

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
    display->lua_ref = ESE_LUA_NOREF;
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
static EseLuaValue* _ese_display_lua_gc(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return NULL;
    }

    // Get the display from the first argument
    if (!lua_value_is_display(argv[0])) {
        return NULL;
    }

    EseDisplay *display = lua_value_get_display(argv[0]);
    if (display) {
        // If lua_ref == ESE_LUA_NOREF, there are no more references to this display, 
        // so we can free it.
        // If lua_ref != ESE_LUA_NOREF, this display was referenced from C and should not be freed.
        if (display->lua_ref == ESE_LUA_NOREF) {
            ese_display_destroy(display);
        }
    }

    return NULL;
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
static EseLuaValue* _ese_display_lua_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_DISPLAY_INDEX);
    
    if (argc != 2) {
        profile_cancel(PROFILE_LUA_DISPLAY_INDEX);
        return lua_value_create_error("result", "index requires 2 arguments");
    }

    // Get the display from the first argument
    if (!lua_value_is_display(argv[0])) {
        profile_cancel(PROFILE_LUA_DISPLAY_INDEX);
        return lua_value_create_error("result", "first argument must be a display");
    }
    
    EseDisplay *display = lua_value_get_display(argv[0]);
    if (!display) {
        profile_cancel(PROFILE_LUA_DISPLAY_INDEX);
        return lua_value_create_error("result", "invalid display");
    }

    // Get the property name
    if (!lua_value_is_string(argv[1])) {
        profile_cancel(PROFILE_LUA_DISPLAY_INDEX);
        return lua_value_create_error("result", "second argument must be a string");
    }
    
    const char *key = lua_value_get_string(argv[1]);
    if (!key) {
        profile_cancel(PROFILE_LUA_DISPLAY_INDEX);
        return lua_value_create_error("result", "invalid key");
    }

    // Simple properties
    if (strcmp(key, "fullscreen") == 0) {
        profile_stop(PROFILE_LUA_DISPLAY_INDEX, "ese_display_lua_index (fullscreen)");
        return lua_value_create_bool(display->fullscreen);
    }
    if (strcmp(key, "width") == 0) {
        profile_stop(PROFILE_LUA_DISPLAY_INDEX, "ese_display_lua_index (width)");
        return lua_value_create_number(display->width);
    }
    if (strcmp(key, "height") == 0) {
        profile_stop(PROFILE_LUA_DISPLAY_INDEX, "ese_display_lua_index (height)");
        return lua_value_create_number(display->height);
    }
    if (strcmp(key, "aspect_ratio") == 0) {
        profile_stop(PROFILE_LUA_DISPLAY_INDEX, "ese_display_lua_index (aspect_ratio)");
        return lua_value_create_number(display->aspect_ratio);
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
        return lua_value_create_nil();
    }

    profile_stop(PROFILE_LUA_DISPLAY_INDEX, "ese_display_lua_index (invalid)");
    return lua_value_create_nil();
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
static EseLuaValue* _ese_display_lua_newindex(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_DISPLAY_NEWINDEX);
    profile_stop(PROFILE_LUA_DISPLAY_NEWINDEX, "ese_display_lua_newindex (error)");
    return lua_value_create_error("result", "Display object is read-only");
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
static EseLuaValue* _ese_display_lua_tostring(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return lua_value_create_error("result", "tostring requires 1 argument");
    }
    
    if (!lua_value_is_display(argv[0])) {
        return lua_value_create_error("result", "first argument must be a display");
    }
    
    EseDisplay *display = lua_value_get_display(argv[0]);
    if (!display) {
        return lua_value_create_string("Display: (invalid)");
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "Display: %p (%dx%d, %s, viewport: %dx%d)", 
             (void *)display, display->width, display->height,
             display->fullscreen ? "fullscreen" : "windowed",
             display->viewport.width, display->viewport.height);
    return lua_value_create_string(buf);
}

// Lua constructors
// static EseLuaValue* _ese_display_lua_new(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) { // REMOVED
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
//     return lua_value_create_nil();
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
    const char *key = lua_value_get_string(argv[2-1]);
    if (!viewport || !key) return lua_value_create_nil();

    if (strcmp(key, "width") == 0) {
        lua_pushinteger(L, viewport->width);
        return lua_value_create_nil();
    }
    if (strcmp(key, "height") == 0) {
        lua_pushinteger(L, viewport->height);
        return lua_value_create_nil();
    }

    return lua_value_create_nil();
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
    return lua_value_create_error("result", "Display tables are read-only");
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

    if (luaL_newmetatable(engine->runtime, "DisplayMeta")) {
        log_debug("LUA", "Adding DisplayMeta to engine");
        lua_pushstring(engine->runtime, "DisplayMeta");
        lua_setfield(engine->runtime, -2, "__name");
        lua_pushcfunction(engine->runtime, _ese_display_lua_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _ese_display_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushcfunction(engine->runtime, _ese_display_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");
        lua_pushcfunction(engine->runtime, _ese_display_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
    
    // REMOVED: Global Display table creation
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
