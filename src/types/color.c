#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/color.h"

// The actual EseColor struct definition (private to this file)
typedef struct EseColor {
    float r;            /**< The red component of the color (0.0-1.0) */
    float g;            /**< The green component of the color (0.0-1.0) */
    float b;            /**< The blue component of the color (0.0-1.0) */
    float a;            /**< The alpha component of the color (0.0-1.0) */

    lua_State *state;   /**< Lua State this EseColor belongs to */
    int lua_ref;        /**< Lua registry reference to its own proxy table */
    int lua_ref_count;  /**< Number of times this color has been referenced in C */
    
    // Watcher system
    EseColorWatcherCallback *watchers;     /**< Array of watcher callbacks */
    void **watcher_userdata;               /**< Array of userdata for each watcher */
    size_t watcher_count;                  /**< Number of registered watchers */
    size_t watcher_capacity;               /**< Capacity of the watcher arrays */
} EseColor;

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseColor *_ese_color_make(void);

// Watcher system
static void _ese_color_notify_watchers(EseColor *color);

// Lua metamethods
static int _ese_color_lua_gc(lua_State *L);
static int _ese_color_lua_index(lua_State *L);
static int _ese_color_lua_newindex(lua_State *L);
static int _ese_color_lua_tostring(lua_State *L);

// Lua constructors
static int _ese_color_lua_new(lua_State *L);
static int _ese_color_lua_white(lua_State *L);
static int _ese_color_lua_black(lua_State *L);
static int _ese_color_lua_red(lua_State *L);
static int _ese_color_lua_green(lua_State *L);
static int _ese_color_lua_blue(lua_State *L);

// Lua utility methods
static int _ese_color_lua_set_hex(lua_State *L);
static int _ese_color_lua_set_byte(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseColor instance with default values
 * 
 * Allocates memory for a new EseColor and initializes all fields to safe defaults.
 * The color starts at black (0,0,0,1) with no Lua state or watchers.
 * 
 * @return Pointer to the newly created EseColor, or NULL on allocation failure
 */
static EseColor *_ese_color_make() {
    EseColor *color = (EseColor *)memory_manager.malloc(sizeof(EseColor), MMTAG_COLOR);
    color->r = 0.0f;
    color->g = 0.0f;
    color->b = 0.0f;
    color->a = 1.0f;
    color->state = NULL;
    color->lua_ref = LUA_NOREF;
    color->lua_ref_count = 0;
    color->watchers = NULL;
    color->watcher_userdata = NULL;
    color->watcher_count = 0;
    color->watcher_capacity = 0;
    return color;
}

// Watcher system
/**
 * @brief Notifies all registered watchers of a color change
 * 
 * Iterates through all registered watcher callbacks and invokes them with the
 * updated color and their associated userdata. This is called whenever the
 * color's r, g, b, or a components are modified.
 * 
 * @param color Pointer to the EseColor that has changed
 */
static void _ese_color_notify_watchers(EseColor *color) {
    if (!color || color->watcher_count == 0) return;
    
    for (size_t i = 0; i < color->watcher_count; i++) {
        if (color->watchers[i]) {
            color->watchers[i](color, color->watcher_userdata[i]);
        }
    }
}

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseColor
 * 
 * Handles cleanup when a Lua proxy table for an EseColor is garbage collected.
 * Only frees the underlying EseColor if it has no C-side references.
 * 
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static int _ese_color_lua_gc(lua_State *L) {
    // Get from userdata
    EseColor **ud = (EseColor **)luaL_testudata(L, 1, "ColorMeta");
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseColor *color = *ud;
    if (color) {
        // If lua_ref == LUA_NOREF, there are no more references to this color, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this color was referenced from C and should not be freed.
        if (color->lua_ref == LUA_NOREF) {
            ese_color_destroy(color);
        }
    }

    return 0;
}

/**
 * @brief Lua __index metamethod for EseColor property access
 * 
 * Provides read access to color properties (r, g, b, a) from Lua. When a Lua script
 * accesses color.r, color.g, color.b, or color.a, this function is called to retrieve the values.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid properties, 0 for invalid)
 */
static int _ese_color_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_COLOR_INDEX);
    EseColor *color = ese_color_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!color || !key) {
        profile_cancel(PROFILE_LUA_COLOR_INDEX);
        return 0;
    }

    if (strcmp(key, "r") == 0) {
        lua_pushnumber(L, color->r);
        profile_stop(PROFILE_LUA_COLOR_INDEX, "ese_color_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "g") == 0) {
        lua_pushnumber(L, color->g);
        profile_stop(PROFILE_LUA_COLOR_INDEX, "ese_color_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "b") == 0) {
        lua_pushnumber(L, color->b);
        profile_stop(PROFILE_LUA_COLOR_INDEX, "ese_color_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "a") == 0) {
        lua_pushnumber(L, color->a);
        profile_stop(PROFILE_LUA_COLOR_INDEX, "ese_color_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "set_hex") == 0) {
        lua_pushcfunction(L, _ese_color_lua_set_hex);
        profile_stop(PROFILE_LUA_COLOR_INDEX, "ese_color_lua_index (method)");
        return 1;
    } else if (strcmp(key, "set_byte") == 0) {
        lua_pushcfunction(L, _ese_color_lua_set_byte);
        profile_stop(PROFILE_LUA_COLOR_INDEX, "ese_color_lua_index (method)");
        return 1;
    }
    profile_stop(PROFILE_LUA_COLOR_INDEX, "ese_color_lua_index (invalid)");
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseColor property assignment
 * 
 * Provides write access to color properties (r, g, b, a) from Lua. When a Lua script
 * assigns to color.r, color.g, color.b, or color.a, this function is called to update the values
 * and notify any registered watchers of the change.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_color_lua_newindex(lua_State *L) {
    profile_start(PROFILE_LUA_COLOR_NEWINDEX);
    EseColor *color = ese_color_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!color || !key) {
        profile_cancel(PROFILE_LUA_COLOR_NEWINDEX);
        return 0;
    }

    if (strcmp(key, "r") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_COLOR_NEWINDEX);
            return luaL_error(L, "color.r must be a number");
        }
        color->r = (float)lua_tonumber(L, 3);
        _ese_color_notify_watchers(color);
        profile_stop(PROFILE_LUA_COLOR_NEWINDEX, "ese_color_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "g") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_COLOR_NEWINDEX);
            return luaL_error(L, "color.g must be a number");
        }
        color->g = (float)lua_tonumber(L, 3);
        _ese_color_notify_watchers(color);
        profile_stop(PROFILE_LUA_COLOR_NEWINDEX, "ese_color_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "b") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_COLOR_NEWINDEX);
            return luaL_error(L, "color.b must be a number");
        }
        color->b = (float)lua_tonumber(L, 3);
        _ese_color_notify_watchers(color);
        profile_stop(PROFILE_LUA_COLOR_NEWINDEX, "ese_color_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "a") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_COLOR_NEWINDEX);
            return luaL_error(L, "color.a must be a number");
        }
        color->a = (float)lua_tonumber(L, 3);
        _ese_color_notify_watchers(color);
        profile_stop(PROFILE_LUA_COLOR_NEWINDEX, "ese_color_lua_newindex (setter)");
        return 0;
    }
    profile_stop(PROFILE_LUA_COLOR_NEWINDEX, "ese_color_lua_newindex (invalid)");
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __tostring metamethod for EseColor string representation
 * 
 * Converts an EseColor to a human-readable string for debugging and display.
 * The format includes the memory address and current r,g,b,a values.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
static int _ese_color_lua_tostring(lua_State *L) {
    EseColor *color = ese_color_lua_get(L, 1);

    if (!color) {
        lua_pushstring(L, "Color: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Color: %p (r=%.2f, g=%.2f, b=%.2f, a=%.2f)", 
             (void*)color, color->r, color->g, color->b, color->a);
    lua_pushstring(L, buf);

    return 1;
}

// Lua constructors
/**
 * @brief Lua constructor function for creating new EseColor instances
 * 
 * Creates a new EseColor from Lua with specified r,g,b,a values. This function
 * is called when Lua code executes `Color.new(r, g, b, a)`. It validates the arguments,
 * creates the underlying EseColor, and returns a proxy table that provides
 * access to the color's properties and methods.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_color_lua_new(lua_State *L) {
    profile_start(PROFILE_LUA_COLOR_NEW);

    int argc = lua_gettop(L);
    if (argc < 3 || argc > 4) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return luaL_error(L, "Color.new(r, g, b) takes 3 arguments\nColor.new(r, g, b, a) takes 4 arguments");
    }

    if (lua_type(L, 1) != LUA_TNUMBER) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return luaL_error(L, "r must be a number");
    }
    if (lua_type(L, 2) != LUA_TNUMBER) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return luaL_error(L, "g must be a number");
    }
    if (lua_type(L, 3) != LUA_TNUMBER) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return luaL_error(L, "b must be a number");
    }
    if (argc == 4 && lua_type(L, 4) != LUA_TNUMBER) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return luaL_error(L, "a must be a number");
    }

    float r = (float)lua_tonumber(L, 1);
    float g = (float)lua_tonumber(L, 2);
    float b = (float)lua_tonumber(L, 3);
    float a = 1.0f;
    if (argc == 4) {
        a = (float)lua_tonumber(L, 4);
    }

    if (r < 0.0f || r > 1.0f) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return luaL_error(L, "r must be between 0.0 and 1.0");
    }
    if (g < 0.0f || g > 1.0f) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return luaL_error(L, "g must be between 0.0 and 1.0");
    }
    if (b < 0.0f || b > 1.0f) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return luaL_error(L, "b must be between 0.0 and 1.0");
    }
    if (a < 0.0f || a > 1.0f) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return luaL_error(L, "a must be between 0.0 and 1.0");
    }

    // Create the color
    EseColor *color = _ese_color_make();
    color->state = L;
    color->r = r;
    color->g = g;
    color->b = b;
    color->a = a;

    printf("DEBUG: Creating color: r=%.3f, g=%.3f, b=%.3f, a=%.3f\n", r, g, b, a);

    // Create userdata directly
    EseColor **ud = (EseColor **)lua_newuserdata(L, sizeof(EseColor *));
    *ud = color;

    // Attach metatable
    luaL_getmetatable(L, "ColorMeta");
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_COLOR_NEW, "ese_color_lua_new");
    return 1;
}

/**
 * @brief Lua constructor function for creating white EseColor
 * 
 * Creates a new EseColor with white color (1,1,1,1) from Lua. This function is called
 * when Lua code executes `Color.white()`.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_color_lua_white(lua_State *L) {
    profile_start(PROFILE_LUA_COLOR_WHITE);
    EseColor *color = _ese_color_make();
    color->r = 1.0f;
    color->g = 1.0f;
    color->b = 1.0f;
    color->a = 1.0f;
    color->state = L;

    // Create userdata directly
    EseColor **ud = (EseColor **)lua_newuserdata(L, sizeof(EseColor *));
    *ud = color;

    // Attach metatable
    luaL_getmetatable(L, "ColorMeta");
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_COLOR_WHITE, "ese_color_lua_white");
    return 1;
}

/**
 * @brief Lua constructor function for creating black EseColor
 * 
 * Creates a new EseColor with black color (0,0,0,1) from Lua. This function is called
 * when Lua code executes `Color.black()`.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_color_lua_black(lua_State *L) {
    profile_start(PROFILE_LUA_COLOR_BLACK);
    EseColor *color = _ese_color_make();
    color->r = 0.0f;
    color->g = 0.0f;
    color->b = 0.0f;
    color->a = 1.0f;
    color->state = L;

    // Create userdata directly
    EseColor **ud = (EseColor **)lua_newuserdata(L, sizeof(EseColor *));
    *ud = color;

    // Attach metatable
    luaL_getmetatable(L, "ColorMeta");
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_COLOR_BLACK, "ese_color_lua_black");
    return 1;
}

/**
 * @brief Lua constructor function for creating red EseColor
 * 
 * Creates a new EseColor with red color (1,0,0,1) from Lua. This function is called
 * when Lua code executes `Color.red()`.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_color_lua_red(lua_State *L) {
    profile_start(PROFILE_LUA_COLOR_RED);
    EseColor *color = _ese_color_make();
    color->r = 1.0f;
    color->g = 0.0f;
    color->b = 0.0f;
    color->a = 1.0f;
    color->state = L;

    // Create userdata directly
    EseColor **ud = (EseColor **)lua_newuserdata(L, sizeof(EseColor *));
    *ud = color;

    // Attach metatable
    luaL_getmetatable(L, "ColorMeta");
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_COLOR_RED, "ese_color_lua_red");
    return 1;
}

/**
 * @brief Lua constructor function for creating green EseColor
 * 
 * Creates a new EseColor with green color (0,1,0,1) from Lua. This function is called
 * when Lua code executes `Color.green()`.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_color_lua_green(lua_State *L) {
    profile_start(PROFILE_LUA_COLOR_GREEN);
    EseColor *color = _ese_color_make();
    color->r = 0.0f;
    color->g = 1.0f;
    color->b = 0.0f;
    color->a = 1.0f;
    color->state = L;

    // Create userdata directly
    EseColor **ud = (EseColor **)lua_newuserdata(L, sizeof(EseColor *));
    *ud = color;

    // Attach metatable
    luaL_getmetatable(L, "ColorMeta");
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_COLOR_GREEN, "ese_color_lua_green");
    return 1;
}

/**
 * @brief Lua constructor function for creating blue EseColor
 * 
 * Creates a new EseColor with blue color (0,0,1,1) from Lua. This function is called
 * when Lua code executes `Color.blue()`.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _ese_color_lua_blue(lua_State *L) {
    profile_start(PROFILE_LUA_COLOR_BLUE);
    EseColor *color = _ese_color_make();
    color->r = 0.0f;
    color->g = 0.0f;
    color->b = 1.0f;
    color->a = 1.0f;
    color->state = L;

    // Create userdata directly
    EseColor **ud = (EseColor **)lua_newuserdata(L, sizeof(EseColor *));
    *ud = color;

    // Attach metatable
    luaL_getmetatable(L, "ColorMeta");
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_COLOR_BLUE, "ese_color_lua_blue");
    return 1;
}

// Lua utility methods
/**
 * @brief Lua method for setting color from hex string
 * 
 * Sets the color from a hex string in formats: #RGB, #RRGGBB, #RGBA, #RRGGBBAA
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_color_lua_set_hex(lua_State *L) {
    profile_start(PROFILE_LUA_COLOR_SET_HEX);

    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 2) {
        profile_cancel(PROFILE_LUA_COLOR_SET_HEX);
        return luaL_error(L, "Color.set_hex(hex_string) takes 1 argument");
    }
    
    if (lua_type(L, 2) != LUA_TSTRING) {
        profile_cancel(PROFILE_LUA_COLOR_SET_HEX);
        return luaL_error(L, "Color.set_hex(hex_string) arguments must be a string");
    }
    
    EseColor *color = ese_color_lua_get(L, 1);
    const char *hex_string = lua_tostring(L, 2);
    
    bool success = ese_color_set_hex(color, hex_string);
    if (!success) {
        profile_cancel(PROFILE_LUA_COLOR_SET_HEX);
        return luaL_error(L, "Invalid hex string format (#RGB, #RRGGBB, #RGBA, #RRGGBBAA)");
    }
    
    profile_stop(PROFILE_LUA_COLOR_SET_HEX, "ese_color_lua_set_hex");
    return 0;
}

/**
 * @brief Lua method for setting color from byte values
 * 
 * Sets the color from byte values (0-255) for each component
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_color_lua_set_byte(lua_State *L) {
    profile_start(PROFILE_LUA_COLOR_SET_BYTE);

    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 5) {
        profile_cancel(PROFILE_LUA_POINT_ZERO);
        return luaL_error(L, "Color.set_byte(r, g, b, a) takes 4 arguments");
    }
    
    if (lua_type(L, 2) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER ||
        lua_type(L, 3) != LUA_TNUMBER || lua_type(L, 4) != LUA_TNUMBER ||
        lua_type(L, 5) != LUA_TNUMBER) {
        profile_cancel(PROFILE_LUA_COLOR_SET_BYTE);
        return luaL_error(L, "Color.set_byte(r, g, b, a) arguments must be numbers");
    }
    
    EseColor *color = ese_color_lua_get(L, 1);
    if (!color) {
        profile_cancel(PROFILE_LUA_COLOR_SET_BYTE);
        return luaL_error(L, "set_byte requires a color");
    }
    
    unsigned char r = (unsigned char)lua_tonumber(L, 2);
    unsigned char g = (unsigned char)lua_tonumber(L, 3);
    unsigned char b = (unsigned char)lua_tonumber(L, 4);
    unsigned char a = (unsigned char)lua_tonumber(L, 5);
    
    ese_color_set_byte(color, r, g, b, a);
    
    profile_stop(PROFILE_LUA_COLOR_SET_BYTE, "ese_color_lua_set_byte");
    return 0;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseColor *ese_color_create(EseLuaEngine *engine) {
    log_assert("COLOR", engine, "ese_color_create called with NULL engine");
    EseColor *color = _ese_color_make();
    color->state = engine->runtime;
    return color;
}

EseColor *ese_color_copy(const EseColor *source) {
    log_assert("COLOR", source, "ese_color_copy called with NULL source");
    
    EseColor *copy = (EseColor *)memory_manager.malloc(sizeof(EseColor), MMTAG_COLOR);
    copy->r = source->r;
    copy->g = source->g;
    copy->b = source->b;
    copy->a = source->a;
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
    copy->lua_ref_count = 0;
    copy->watchers = NULL;
    copy->watcher_userdata = NULL;
    copy->watcher_count = 0;
    copy->watcher_capacity = 0;
    return copy;
}

void ese_color_destroy(EseColor *color) {
    if (!color) return;
    
    // Free watcher arrays if they exist
    if (color->watchers) {
        memory_manager.free(color->watchers);
        color->watchers = NULL;
    }
    if (color->watcher_userdata) {
        memory_manager.free(color->watcher_userdata);
        color->watcher_userdata = NULL;
    }
    color->watcher_count = 0;
    color->watcher_capacity = 0;
    
    if (color->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(color);
    } else {
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
        ese_color_unref(color);
    }
}

// Property access
void ese_color_set_r(EseColor *color, float r) {
    log_assert("COLOR", color, "ese_color_set_r called with NULL color");
    color->r = r;
    _ese_color_notify_watchers(color);
}

float ese_color_get_r(const EseColor *color) {
    log_assert("COLOR", color, "ese_color_get_r called with NULL color");
    return color->r;
}

void ese_color_set_g(EseColor *color, float g) {
    log_assert("COLOR", color, "ese_color_set_g called with NULL color");
    color->g = g;
    _ese_color_notify_watchers(color);
}

float ese_color_get_g(const EseColor *color) {
    log_assert("COLOR", color, "ese_color_get_g called with NULL color");
    return color->g;
}

void ese_color_set_b(EseColor *color, float b) {
    log_assert("COLOR", color, "ese_color_set_b called with NULL color");
    color->b = b;
    _ese_color_notify_watchers(color);
}

float ese_color_get_b(const EseColor *color) {
    log_assert("COLOR", color, "ese_color_get_b called with NULL color");
    return color->b;
}

void ese_color_set_a(EseColor *color, float a) {
    log_assert("COLOR", color, "ese_color_set_a called with NULL color");
    color->a = a;
    _ese_color_notify_watchers(color);
}

float ese_color_get_a(const EseColor *color) {
    log_assert("COLOR", color, "ese_color_get_a called with NULL color");
    return color->a;
}

// Lua-related access
lua_State *ese_color_get_state(const EseColor *color) {
    log_assert("COLOR", color, "ese_color_get_state called with NULL color");
    return color->state;
}

int ese_color_get_lua_ref(const EseColor *color) {
    log_assert("COLOR", color, "ese_color_get_lua_ref called with NULL color");
    return color->lua_ref;
}

int ese_color_get_lua_ref_count(const EseColor *color) {
    log_assert("COLOR", color, "ese_color_get_lua_ref_count called with NULL color");
    return color->lua_ref_count;
}

// Watcher system
bool ese_color_add_watcher(EseColor *color, EseColorWatcherCallback callback, void *userdata) {
    log_assert("COLOR", color, "ese_color_add_watcher called with NULL color");
    log_assert("COLOR", callback, "ese_color_add_watcher called with NULL callback");
    
    // Initialize watcher arrays if this is the first watcher
    if (color->watcher_count == 0) {
        color->watcher_capacity = 4; // Start with capacity for 4 watchers
        color->watchers = memory_manager.malloc(sizeof(EseColorWatcherCallback) * color->watcher_capacity, MMTAG_COLOR);
        color->watcher_userdata = memory_manager.malloc(sizeof(void*) * color->watcher_capacity, MMTAG_COLOR);
        color->watcher_count = 0;
    }
    
    // Expand arrays if needed
    if (color->watcher_count >= color->watcher_capacity) {
        size_t new_capacity = color->watcher_capacity * 2;
        EseColorWatcherCallback *new_watchers = memory_manager.realloc(
            color->watchers, 
            sizeof(EseColorWatcherCallback) * new_capacity, 
            MMTAG_COLOR
        );
        void **new_userdata = memory_manager.realloc(
            color->watcher_userdata, 
            sizeof(void*) * new_capacity, 
            MMTAG_COLOR
        );
        
        if (!new_watchers || !new_userdata) return false;
        
        color->watchers = new_watchers;
        color->watcher_userdata = new_userdata;
        color->watcher_capacity = new_capacity;
    }
    
    // Add the new watcher
    color->watchers[color->watcher_count] = callback;
    color->watcher_userdata[color->watcher_count] = userdata;
    color->watcher_count++;
    
    return true;
}

bool ese_color_remove_watcher(EseColor *color, EseColorWatcherCallback callback, void *userdata) {
    log_assert("COLOR", color, "ese_color_remove_watcher called with NULL color");
    log_assert("COLOR", callback, "ese_color_remove_watcher called with NULL callback");
    
    for (size_t i = 0; i < color->watcher_count; i++) {
        if (color->watchers[i] == callback && color->watcher_userdata[i] == userdata) {
            // Remove this watcher by shifting remaining ones
            for (size_t j = i; j < color->watcher_count - 1; j++) {
                color->watchers[j] = color->watchers[j + 1];
                color->watcher_userdata[j] = color->watcher_userdata[j + 1];
            }
            color->watcher_count--;
            return true;
        }
    }
    
    return false;
}

// Lua integration
void ese_color_lua_init(EseLuaEngine *engine) {
    log_assert("COLOR", engine, "ese_color_lua_init called with NULL engine");
    if (luaL_newmetatable(engine->runtime, "ColorMeta")) {
        log_debug("LUA", "Adding entity ColorMeta to engine");
        lua_pushstring(engine->runtime, "ColorMeta");
        lua_setfield(engine->runtime, -2, "__name");
        lua_pushcfunction(engine->runtime, _ese_color_lua_index);
        lua_setfield(engine->runtime, -2, "__index");               // For property getters
        lua_pushcfunction(engine->runtime, _ese_color_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");            // For property setters
        lua_pushcfunction(engine->runtime, _ese_color_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");                  // For garbage collection
        lua_pushcfunction(engine->runtime, _ese_color_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");            // For printing/debugging
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
    
    // Create global EseColor table with constructors
    lua_getglobal(engine->runtime, "Color");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1); // Pop the nil value
        log_debug("LUA", "Creating global color table");
        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _ese_color_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_pushcfunction(engine->runtime, _ese_color_lua_white);
        lua_setfield(engine->runtime, -2, "white");
        lua_pushcfunction(engine->runtime, _ese_color_lua_black);
        lua_setfield(engine->runtime, -2, "black");
        lua_pushcfunction(engine->runtime, _ese_color_lua_red);
        lua_setfield(engine->runtime, -2, "red");
        lua_pushcfunction(engine->runtime, _ese_color_lua_green);
        lua_setfield(engine->runtime, -2, "green");
        lua_pushcfunction(engine->runtime, _ese_color_lua_blue);
        lua_setfield(engine->runtime, -2, "blue");
        lua_setglobal(engine->runtime, "Color");
    } else {
        lua_pop(engine->runtime, 1); // Pop the existing color table
    }
}

void ese_color_lua_push(EseColor *color) {
    log_assert("COLOR", color, "ese_color_lua_push called with NULL color");

    if (color->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseColor **ud = (EseColor **)lua_newuserdata(color->state, sizeof(EseColor *));
        *ud = color;

        // Attach metatable
        luaL_getmetatable(color->state, "ColorMeta");
        lua_setmetatable(color->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(color->state, LUA_REGISTRYINDEX, color->lua_ref);
    }
}

EseColor *ese_color_lua_get(lua_State *L, int idx) {
    log_assert("COLOR", L, "ese_color_lua_get called with NULL Lua state");
    
    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseColor **ud = (EseColor **)luaL_testudata(L, idx, "ColorMeta");
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_color_ref(EseColor *color) {
    log_assert("COLOR", color, "ese_color_ref called with NULL color");
    
    if (color->lua_ref == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseColor **ud = (EseColor **)lua_newuserdata(color->state, sizeof(EseColor *));
        *ud = color;

        // Attach metatable
        luaL_getmetatable(color->state, "ColorMeta");
        lua_setmetatable(color->state, -2);

        // Store hard reference to prevent garbage collection
        color->lua_ref = luaL_ref(color->state, LUA_REGISTRYINDEX);
        color->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        color->lua_ref_count++;
    }

    profile_count_add("ese_color_ref_count");
}

void ese_color_unref(EseColor *color) {
    if (!color) return;
    
    if (color->lua_ref != LUA_NOREF && color->lua_ref_count > 0) {
        color->lua_ref_count--;
        
        if (color->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(color->state, LUA_REGISTRYINDEX, color->lua_ref);
            color->lua_ref = LUA_NOREF;
        }
    }

    profile_count_add("ese_color_unref_count");
}

// Utility functions
bool ese_color_set_hex(EseColor *color, const char *hex_string) {
    log_assert("COLOR", color, "ese_color_set_hex called with NULL color");
    log_assert("COLOR", hex_string, "ese_color_set_hex called with NULL hex_string");
    
    if (!hex_string || hex_string[0] != '#') {
        return false;
    }
    
    size_t len = strlen(hex_string);
    if (len < 4 || len > 9) { // #RGB to #RRGGBBAA
        return false;
    }
    
    unsigned int r, g, b, a = 255; // Default alpha to 255
    
    if (len == 4) { // #RGB
        if (sscanf(hex_string, "#%1x%1x%1x", &r, &g, &b) != 3) {
            return false;
        }
        r = (r << 4) | r; // Expand to 8-bit
        g = (g << 4) | g;
        b = (b << 4) | b;
    } else if (len == 5) { // #RGBA
        if (sscanf(hex_string, "#%1x%1x%1x%1x", &r, &g, &b, &a) != 4) {
            return false;
        }
        r = (r << 4) | r; // Expand to 8-bit
        g = (g << 4) | g;
        b = (b << 4) | b;
        a = (a << 4) | a;
    } else if (len == 7) { // #RRGGBB
        if (sscanf(hex_string, "#%2x%2x%2x", &r, &g, &b) != 3) {
            return false;
        }
    } else if (len == 9) { // #RRGGBBAA
        if (sscanf(hex_string, "#%2x%2x%2x%2x", &r, &g, &b, &a) != 4) {
            return false;
        }
    } else {
        return false;
    }
    
    // Convert to normalized floats
    color->r = (float)r / 255.0f;
    color->g = (float)g / 255.0f;
    color->b = (float)b / 255.0f;
    color->a = (float)a / 255.0f;
    
    _ese_color_notify_watchers(color);
    return true;
}

void ese_color_set_byte(EseColor *color, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    log_assert("COLOR", color, "ese_color_set_byte called with NULL color");
    
    color->r = (float)r / 255.0f;
    color->g = (float)g / 255.0f;
    color->b = (float)b / 255.0f;
    color->a = (float)a / 255.0f;
    
    _ese_color_notify_watchers(color);
}

void ese_color_get_byte(const EseColor *color, unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a) {
    log_assert("COLOR", color, "ese_color_get_byte called with NULL color");
    log_assert("COLOR", r, "ese_color_get_byte called with NULL r pointer");
    log_assert("COLOR", g, "ese_color_get_byte called with NULL g pointer");
    log_assert("COLOR", b, "ese_color_get_byte called with NULL b pointer");
    log_assert("COLOR", a, "ese_color_get_byte called with NULL a pointer");
    
    *r = (unsigned char)(color->r * 255.0f + 0.5f);
    *g = (unsigned char)(color->g * 255.0f + 0.5f);
    *b = (unsigned char)(color->b * 255.0f + 0.5f);
    *a = (unsigned char)(color->a * 255.0f + 0.5f);
}

size_t ese_color_sizeof(void) {
    return sizeof(EseColor);
}
