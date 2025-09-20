#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "scripting/lua_value.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/color.h"

// The actual EseColor struct definition (private to this file)
typedef struct EseColor {
    float r;            /**< The red component of the color (0.0-1.0) */
    float g;            /**< The green component of the color (0.0-1.0) */
    float b;            /**< The blue component of the color (0.0-1.0) */
    float a;            /**< The alpha component of the color (0.0-1.0) */

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
static EseLuaValue* _ese_color_lua_gc(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_color_lua_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_color_lua_newindex(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_color_lua_tostring(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

// Lua constructors
static EseLuaValue* _ese_color_lua_new(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_color_lua_white(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_color_lua_black(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_color_lua_red(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_color_lua_green(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_color_lua_blue(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

// Lua utility methods
static EseLuaValue* _ese_color_lua_set_hex(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_color_lua_set_byte(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

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
    color->lua_ref = ESE_LUA_NOREF;
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
static EseLuaValue* _ese_color_lua_gc(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return NULL;
    }

    // Get the color from the first argument
    if (!lua_value_is_color(argv[0])) {
        return NULL;
    }

    EseColor *color = lua_value_get_color(argv[0]);
    if (color) {
        // If lua_ref == ESE_LUA_NOREF, there are no more references to this color, 
        // so we can free it.
        // If lua_ref != ESE_LUA_NOREF, this color was referenced from C and should not be freed.
        if (color->lua_ref == ESE_LUA_NOREF) {
            ese_color_destroy(color);
        }
    }

    return NULL;
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
static EseLuaValue* _ese_color_lua_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_COLOR_INDEX);
    
    if (argc != 2) {
        profile_cancel(PROFILE_LUA_COLOR_INDEX);
        return lua_value_create_error("result", "index requires 2 arguments");
    }

    // Get the color from the first argument
    if (!lua_value_is_color(argv[0])) {
        profile_cancel(PROFILE_LUA_COLOR_INDEX);
        return lua_value_create_error("result", "first argument must be a color");
    }
    
    EseColor *color = lua_value_get_color(argv[0]);
    if (!color) {
        profile_cancel(PROFILE_LUA_COLOR_INDEX);
        return lua_value_create_nil("result");
    }

    // Get the property name
    if (!lua_value_is_string(argv[1])) {
        profile_cancel(PROFILE_LUA_COLOR_INDEX);
        return lua_value_create_error("result", "second argument must be a string");
    }
    
    const char *key = lua_value_get_string(argv[1]);
    
    if (strcmp(key, "r") == 0) {
        profile_stop(PROFILE_LUA_COLOR_INDEX, "ese_color_lua_index (getter)");
        return lua_value_create_number("result", color->r);
    } else if (strcmp(key, "g") == 0) {
        profile_stop(PROFILE_LUA_COLOR_INDEX, "ese_color_lua_index (getter)");
        return lua_value_create_number("result", color->g);
    } else if (strcmp(key, "b") == 0) {
        profile_stop(PROFILE_LUA_COLOR_INDEX, "ese_color_lua_index (getter)");
        return lua_value_create_number("result", color->b);
    } else if (strcmp(key, "a") == 0) {
        profile_stop(PROFILE_LUA_COLOR_INDEX, "ese_color_lua_index (getter)");
        return lua_value_create_number("result", color->a);
    } else if (strcmp(key, "set_hex") == 0) {
        profile_stop(PROFILE_LUA_COLOR_INDEX, "ese_color_lua_index (method)");
        return lua_value_create_cfunc("result", _ese_color_lua_set_hex, NULL);
    } else if (strcmp(key, "set_byte") == 0) {
        profile_stop(PROFILE_LUA_COLOR_INDEX, "ese_color_lua_index (method)");
        return lua_value_create_cfunc("result", _ese_color_lua_set_byte, NULL);
    } else {
        profile_stop(PROFILE_LUA_COLOR_INDEX, "ese_color_lua_index (nil)");
        return lua_value_create_nil("result");
    }
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
static EseLuaValue* _ese_color_lua_newindex(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_COLOR_NEWINDEX);
    
    if (argc != 3) {
        profile_cancel(PROFILE_LUA_COLOR_NEWINDEX);
        return lua_value_create_error("result", "newindex requires 3 arguments");
    }

    // Get the color from the first argument
    if (!lua_value_is_color(argv[0])) {
        profile_cancel(PROFILE_LUA_COLOR_NEWINDEX);
        return lua_value_create_error("result", "first argument must be a color");
    }
    
    EseColor *color = lua_value_get_color(argv[0]);
    if (!color) {
        profile_cancel(PROFILE_LUA_COLOR_NEWINDEX);
        return lua_value_create_error("result", "invalid color");
    }

    // Get the property name
    if (!lua_value_is_string(argv[1])) {
        profile_cancel(PROFILE_LUA_COLOR_NEWINDEX);
        return lua_value_create_error("result", "second argument must be a string");
    }
    
    const char *key = lua_value_get_string(argv[1]);

    if (strcmp(key, "r") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_COLOR_NEWINDEX);
            return lua_value_create_error("result", "color.r must be a number");
        }
        color->r = (float)lua_value_get_number(argv[2]);
        _ese_color_notify_watchers(color);
        profile_stop(PROFILE_LUA_COLOR_NEWINDEX, "ese_color_lua_newindex (setter)");
        return NULL;
    } else if (strcmp(key, "g") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_COLOR_NEWINDEX);
            return lua_value_create_error("result", "color.g must be a number");
        }
        color->g = (float)lua_value_get_number(argv[2]);
        _ese_color_notify_watchers(color);
        profile_stop(PROFILE_LUA_COLOR_NEWINDEX, "ese_color_lua_newindex (setter)");
        return NULL;
    } else if (strcmp(key, "b") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_COLOR_NEWINDEX);
            return lua_value_create_error("result", "color.b must be a number");
        }
        color->b = (float)lua_value_get_number(argv[2]);
        _ese_color_notify_watchers(color);
        profile_stop(PROFILE_LUA_COLOR_NEWINDEX, "ese_color_lua_newindex (setter)");
        return NULL;
    } else if (strcmp(key, "a") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_COLOR_NEWINDEX);
            return lua_value_create_error("result", "color.a must be a number");
        }
        color->a = (float)lua_value_get_number(argv[2]);
        _ese_color_notify_watchers(color);
        profile_stop(PROFILE_LUA_COLOR_NEWINDEX, "ese_color_lua_newindex (setter)");
        return NULL;
    }
    
    profile_stop(PROFILE_LUA_COLOR_NEWINDEX, "ese_color_lua_newindex (invalid)");
    return lua_value_create_error("result", "unknown or unassignable property '%s'", key);
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
static EseLuaValue* _ese_color_lua_tostring(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return lua_value_create_error("result", "tostring requires 1 argument");
    }

    // Get the color from the first argument
    if (!lua_value_is_color(argv[0])) {
        return lua_value_create_error("result", "first argument must be a color");
    }
    
    EseColor *color = lua_value_get_color(argv[0]);
    if (!color) {
        return lua_value_create_string("result", "Color: (invalid)");
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Color: %p (r=%.2f, g=%.2f, b=%.2f, a=%.2f)", 
             (void*)color, color->r, color->g, color->b, color->a);
    return lua_value_create_string("result", buf);
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
static EseLuaValue* _ese_color_lua_new(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_COLOR_NEW);

    if (argc < 3 || argc > 4) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return lua_value_create_error("result", "Color.new(r, g, b) takes 3 arguments\nColor.new(r, g, b, a) takes 4 arguments");
    }

    if (!lua_value_is_number(argv[0])) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return lua_value_create_error("result", "r must be a number");
    }
    if (!lua_value_is_number(argv[1])) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return lua_value_create_error("result", "g must be a number");
    }
    if (!lua_value_is_number(argv[2])) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return lua_value_create_error("result", "b must be a number");
    }
    if (argc == 4 && !lua_value_is_number(argv[3])) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return lua_value_create_error("result", "a must be a number");
    }

    float r = (float)lua_value_get_number(argv[0]);
    float g = (float)lua_value_get_number(argv[1]);
    float b = (float)lua_value_get_number(argv[2]);
    float a = 1.0f;
    if (argc == 4) {
        a = (float)lua_value_get_number(argv[3]);
    }

    if (r < 0.0f || r > 1.0f) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return lua_value_create_error("result", "r must be between 0.0 and 1.0");
    }
    if (g < 0.0f || g > 1.0f) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return lua_value_create_error("result", "g must be between 0.0 and 1.0");
    }
    if (b < 0.0f || b > 1.0f) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return lua_value_create_error("result", "b must be between 0.0 and 1.0");
    }
    if (a < 0.0f || a > 1.0f) {
        profile_cancel(PROFILE_LUA_COLOR_NEW);
        return lua_value_create_error("result", "a must be between 0.0 and 1.0");
    }

    // Create the color
    EseColor *color = _ese_color_make();
    color->r = r;
    color->g = g;
    color->b = b;
    color->a = a;

    printf("DEBUG: Creating color: r=%.3f, g=%.3f, b=%.3f, a=%.3f\n", r, g, b, a);

    // Create userdata using the engine API
    EseColor **ud = (EseColor **)lua_engine_create_userdata(engine, "ColorMeta", sizeof(EseColor *));
    *ud = color;

    profile_stop(PROFILE_LUA_COLOR_NEW, "ese_color_lua_new");
    return lua_value_create_nil();
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
static EseLuaValue* _ese_color_lua_white(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_COLOR_WHITE);
    EseColor *color = _ese_color_make();
    color->r = 1.0f;
    color->g = 1.0f;
    color->b = 1.0f;
    color->a = 1.0f;
    color->lua_ref = ESE_LUA_NOREF;
    color->lua_ref_count = 0;

    profile_stop(PROFILE_LUA_COLOR_WHITE, "ese_color_lua_white");
    return lua_value_create_color(color);
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
static EseLuaValue* _ese_color_lua_black(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_COLOR_BLACK);
    EseColor *color = _ese_color_make();
    color->r = 0.0f;
    color->g = 0.0f;
    color->b = 0.0f;
    color->a = 1.0f;
    color->lua_ref = ESE_LUA_NOREF;
    color->lua_ref_count = 0;

    profile_stop(PROFILE_LUA_COLOR_BLACK, "ese_color_lua_black");
    return lua_value_create_color(color);
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
static EseLuaValue* _ese_color_lua_red(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_COLOR_RED);
    EseColor *color = _ese_color_make();
    color->r = 1.0f;
    color->g = 0.0f;
    color->b = 0.0f;
    color->a = 1.0f;
    color->lua_ref = ESE_LUA_NOREF;
    color->lua_ref_count = 0;

    profile_stop(PROFILE_LUA_COLOR_RED, "ese_color_lua_red");
    return lua_value_create_color(color);
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
static EseLuaValue* _ese_color_lua_green(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_COLOR_GREEN);
    EseColor *color = _ese_color_make();
    color->r = 0.0f;
    color->g = 1.0f;
    color->b = 0.0f;
    color->a = 1.0f;
    color->lua_ref = ESE_LUA_NOREF;
    color->lua_ref_count = 0;

    profile_stop(PROFILE_LUA_COLOR_GREEN, "ese_color_lua_green");
    return lua_value_create_color(color);
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
static EseLuaValue* _ese_color_lua_blue(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_COLOR_BLUE);
    EseColor *color = _ese_color_make();
    color->r = 0.0f;
    color->g = 0.0f;
    color->b = 1.0f;
    color->a = 1.0f;
    color->lua_ref = ESE_LUA_NOREF;
    color->lua_ref_count = 0;

    profile_stop(PROFILE_LUA_COLOR_BLUE, "ese_color_lua_blue");
    return lua_value_create_color(color);
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
static EseLuaValue* _ese_color_lua_set_hex(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_COLOR_SET_HEX);

    if (argc != 2) {
        profile_cancel(PROFILE_LUA_COLOR_SET_HEX);
        return lua_value_create_error("result", "Color.set_hex(hex_string) takes 1 argument");
    }
    
    if (!lua_value_is_string(argv[1])) {
        profile_cancel(PROFILE_LUA_COLOR_SET_HEX);
        return lua_value_create_error("result", "Color.set_hex(hex_string) arguments must be a string");
    }
    
    if (!lua_value_is_color(argv[0])) {
        profile_cancel(PROFILE_LUA_COLOR_SET_HEX);
        return lua_value_create_error("result", "first argument must be a color");
    }
    
    EseColor *color = lua_value_get_color(argv[0]);
    const char *hex_string = lua_value_get_string(argv[1]);
    
    bool success = ese_color_set_hex(color, hex_string);
    if (!success) {
        profile_cancel(PROFILE_LUA_COLOR_SET_HEX);
        return lua_value_create_error("result", "Invalid hex string format (#RGB, #RRGGBB, #RGBA, #RRGGBBAA)");
    }
    
    profile_stop(PROFILE_LUA_COLOR_SET_HEX, "ese_color_lua_set_hex");
    return lua_value_create_nil();
}

/**
 * @brief Lua method for setting color from byte values
 * 
 * Sets the color from byte values (0-255) for each component
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static EseLuaValue* _ese_color_lua_set_byte(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_COLOR_SET_BYTE);

    if (argc != 5) {
        profile_cancel(PROFILE_LUA_COLOR_SET_BYTE);
        return lua_value_create_error("result", "Color.set_byte(r, g, b, a) takes 4 arguments");
    }
    
    if (!lua_value_is_number(argv[1]) || !lua_value_is_number(argv[2]) ||
        !lua_value_is_number(argv[3]) || !lua_value_is_number(argv[4])) {
        profile_cancel(PROFILE_LUA_COLOR_SET_BYTE);
        return lua_value_create_error("result", "Color.set_byte(r, g, b, a) arguments must be numbers");
    }
    
    if (!lua_value_is_color(argv[0])) {
        profile_cancel(PROFILE_LUA_COLOR_SET_BYTE);
        return lua_value_create_error("result", "first argument must be a color");
    }
    
    EseColor *color = lua_value_get_color(argv[0]);
    if (!color) {
        profile_cancel(PROFILE_LUA_COLOR_SET_BYTE);
        return lua_value_create_error("result", "set_byte requires a color");
    }
    
    unsigned char r = (unsigned char)lua_value_get_number(argv[1]);
    unsigned char g = (unsigned char)lua_value_get_number(argv[2]);
    unsigned char b = (unsigned char)lua_value_get_number(argv[3]);
    unsigned char a = (unsigned char)lua_value_get_number(argv[4]);
    
    ese_color_set_byte(color, r, g, b, a);
    
    profile_stop(PROFILE_LUA_COLOR_SET_BYTE, "ese_color_lua_set_byte");
    return lua_value_create_nil();
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseColor *ese_color_create(EseLuaEngine *engine) {
    EseColor *color = _ese_color_make();
    return color;
}

EseColor *ese_color_copy(const EseColor *source) {
    log_assert("COLOR", source, "ese_color_copy called with NULL source");
    
    EseColor *copy = (EseColor *)memory_manager.malloc(sizeof(EseColor), MMTAG_COLOR);
    copy->r = source->r;
    copy->g = source->g;
    copy->b = source->b;
    copy->a = source->a;
    copy->lua_ref = ESE_LUA_NOREF;
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
    // Add the metatable using the new API
    lua_engine_add_metatable(engine, "ColorMeta", 
                            _ese_color_lua_index, 
                            _ese_color_lua_newindex, 
                            _ese_color_lua_gc, 
                            _ese_color_lua_tostring);
    
    // Create global Color table with constructors
    const char *function_names[] = {"new", "white", "black", "red", "green", "blue"};
    EseLuaCFunction functions[] = {_ese_color_lua_new, _ese_color_lua_white, _ese_color_lua_black, 
                                  _ese_color_lua_red, _ese_color_lua_green, _ese_color_lua_blue};
    lua_engine_add_globaltable(engine, "Color", 6, function_names, functions);
}

void ese_color_lua_push(EseLuaEngine *engine, EseColor *color) {
    log_assert("COLOR", engine, "ese_color_lua_push called with NULL engine");
    log_assert("COLOR", color, "ese_color_lua_push called with NULL color");

    if (color->lua_ref == ESE_LUA_NOREF) {
        // Lua-owned: create a new userdata using the engine API
        EseColor **ud = (EseColor **)lua_engine_create_userdata(engine, "ColorMeta", sizeof(EseColor *));
        *ud = color;
    } else {
        // C-owned: get from registry using the engine API
        lua_engine_get_registry_value(engine, color->lua_ref);
    }
}

EseColor *ese_color_lua_get(EseLuaEngine *engine, int idx) {
    log_assert("COLOR", engine, "ese_color_lua_get called with NULL engine");
    
    // Check if the value at idx is userdata
    if (!lua_engine_is_userdata(engine, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseColor **ud = (EseColor **)lua_engine_test_userdata(engine, idx, "ColorMeta");
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_color_ref(EseLuaEngine *engine, EseColor *color) {
    log_assert("COLOR", color, "ese_color_ref called with NULL color");
    
    if (color->lua_ref == ESE_LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseColor **ud = (EseColor **)lua_engine_create_userdata(engine, "ColorMeta", sizeof(EseColor *));
        *ud = color;

        // Get the reference from the engine's Lua state
        color->lua_ref = lua_engine_get_reference(engine);
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
