#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/display.h"
#include "types/display_private.h"
#include "types/display_lua.h"

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseDisplay *_ese_display_make(void);

// Private static setter forward declarations
static void _ese_display_set_lua_ref(EseDisplay *display, int lua_ref);
static void _ese_display_set_lua_ref_count(EseDisplay *display, int lua_ref_count);
static void _ese_display_set_state(EseDisplay *display, lua_State *state);

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
    _ese_display_set_state(display, NULL);
    _ese_display_set_lua_ref(display, LUA_NOREF);
    _ese_display_set_lua_ref_count(display, 0);
    return display;
}

// Private static setters for Lua state management
static void _ese_display_set_lua_ref(EseDisplay *display, int lua_ref) {
    display->lua_ref = lua_ref;
}

static void _ese_display_set_lua_ref_count(EseDisplay *display, int lua_ref_count) {
    display->lua_ref_count = lua_ref_count;
}

static void _ese_display_set_state(EseDisplay *display, lua_State *state) {
    display->state = state;
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



// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseDisplay *ese_display_create(EseLuaEngine *engine) {
    log_assert("DISPLAY_STATE", engine, "ese_display_create called with NULL engine");
    log_assert("DISPLAY_STATE", memory_manager.malloc, "memory_manager.malloc is NULL");

    EseDisplay *display = _ese_display_make();
    log_assert("DISPLAY_STATE", display, "ese_display_create failed to allocate memory");

    _ese_display_set_state(display, engine->runtime);
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
    _ese_display_set_state(copy, src->state);
    _ese_display_set_lua_ref(copy, LUA_NOREF);
    _ese_display_set_lua_ref_count(copy, 0);

    return copy;
}

void ese_display_destroy(EseDisplay *display) {
    if (!display) return;
    
    if (ese_display_get_lua_ref(display) == LUA_NOREF) {
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
    _ese_display_lua_init(engine);
}

void ese_display_lua_push(EseDisplay *display) {
    log_assert("DISPLAY", display, "ese_display_lua_push called with NULL display");

    if (ese_display_get_lua_ref(display) == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseDisplay **ud = (EseDisplay **)lua_newuserdata(ese_display_get_state(display), sizeof(EseDisplay *));
        *ud = display;

        // Attach metatable
        luaL_getmetatable(ese_display_get_state(display), DISPLAY_META);
        lua_setmetatable(ese_display_get_state(display), -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(ese_display_get_state(display), LUA_REGISTRYINDEX, ese_display_get_lua_ref(display));
    }
}

EseDisplay *ese_display_lua_get(lua_State *L, int idx) {
    log_assert("DISPLAY", L, "ese_display_lua_get called with NULL Lua state");
    
    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseDisplay **ud = (EseDisplay **)luaL_testudata(L, idx, DISPLAY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_display_ref(EseDisplay *display) {
    log_assert("DISPLAY", display, "ese_display_ref called with NULL display");
    
    if (ese_display_get_lua_ref(display) == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseDisplay **ud = (EseDisplay **)lua_newuserdata(ese_display_get_state(display), sizeof(EseDisplay *));
        *ud = display;

        // Attach metatable
        luaL_getmetatable(ese_display_get_state(display), DISPLAY_META);
        lua_setmetatable(ese_display_get_state(display), -2);

        // Store hard reference to prevent garbage collection
        _ese_display_set_lua_ref(display, luaL_ref(ese_display_get_state(display), LUA_REGISTRYINDEX));
        _ese_display_set_lua_ref_count(display, 1);
    } else {
        // Already referenced - just increment count
        _ese_display_set_lua_ref_count(display, ese_display_get_lua_ref_count(display) + 1);
    }

    profile_count_add("ese_display_ref_count");
}

void ese_display_unref(EseDisplay *display) {
    if (!display) return;
    
    if (ese_display_get_lua_ref(display) != LUA_NOREF && ese_display_get_lua_ref_count(display) > 0) {
        _ese_display_set_lua_ref_count(display, ese_display_get_lua_ref_count(display) - 1);
        
        if (ese_display_get_lua_ref_count(display) == 0) {
            // No more references - remove from registry
            luaL_unref(ese_display_get_state(display), LUA_REGISTRYINDEX, ese_display_get_lua_ref(display));
            _ese_display_set_lua_ref(display, LUA_NOREF);
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

EseViewport *ese_display_get_viewport(const EseDisplay *display) {
    log_assert("DISPLAY_STATE", display, "ese_display_get_viewport called with NULL display");
    return (EseViewport *)&display->viewport;
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
