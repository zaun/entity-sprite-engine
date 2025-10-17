/**
 * @file gui_style.c
 * @brief Implementation of GUI style type for styling GUI elements
 * @details Implements style properties for GUI elements including colors, layout, and spacing
 * 
 * @copyright Copyright (c) 2024 ESE Project
 * @license See LICENSE.md for license information
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/memory_manager.h"
#include "graphics/gui.h"
#include "scripting/lua_engine.h"
#include "types/color.h"
#include "types/gui_style.h"
#include "types/gui_style_lua.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"

// The actual EseGuiStyle struct definition (private to this file)
typedef struct EseGuiStyle {
    EseGuiFlexDirection direction;
    EseGuiFlexJustify justify;
    EseGuiFlexAlignItems align_items;

    EseColor *background;
    EseColor *background_hovered;
    EseColor *background_pressed;

    EseColor *border;
    EseColor *border_hovered;
    EseColor *border_pressed;

    EseColor *text;
    EseColor *text_hovered;
    EseColor *text_pressed;

    int border_width;

    int padding_left;
    int padding_top;
    int padding_right;
    int padding_bottom;

    int spacing;

    lua_State *state;   /** Lua State this EseGuiStyle belongs to */
    int lua_ref;        /** Lua registry reference to its own proxy table */
    int lua_ref_count;  /** Number of times this style has been referenced in C */
    
    // Watcher system
    EseGuiStyleWatcherCallback *watchers;     /** Array of watcher callbacks */
    void **watcher_userdata;                  /** Array of userdata for each watcher */
    size_t watcher_count;                     /** Number of registered watchers */
    size_t watcher_capacity;                  /** Capacity of the watcher arrays */
} EseGuiStyle;

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseGuiStyle *_ese_gui_style_make(EseLuaEngine *engine);
static void _ese_gui_style_notify_watchers(EseGuiStyle *style);

// Watcher system

// Private static setters for Lua state management
static void _ese_gui_style_set_lua_ref(EseGuiStyle *style, int lua_ref);
static void _ese_gui_style_set_lua_ref_count(EseGuiStyle *style, int lua_ref_count);
static void _ese_gui_style_set_state(EseGuiStyle *style, lua_State *state);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseGuiStyle instance with default values
 * 
 * Allocates memory for a new EseGuiStyle and initializes all fields to safe defaults.
 * The style starts with default layout values and no colors set.
 * 
 * @return Pointer to the newly created EseGuiStyle, or NULL on allocation failure
 */
EseGuiStyle *_ese_gui_style_make(EseLuaEngine *engine) {
    log_assert("GUI_STYLE", engine != NULL, "ese_gui_style_create called with NULL engine");
    
    EseGuiStyle *style = (EseGuiStyle *)memory_manager.malloc(sizeof(EseGuiStyle), MMTAG_GUI);
    style->direction = FLEX_DIRECTION_ROW;
    style->justify = FLEX_JUSTIFY_START;
    style->align_items = FLEX_ALIGN_ITEMS_START;
    
    style->border_width = 1;
    style->padding_left = 4;
    style->padding_top = 4;
    style->padding_right = 4;
    style->padding_bottom = 4;
    style->spacing = 4;
    
    // Create default colors
    style->background = ese_color_create(engine);
    ese_color_ref(style->background);
    ese_color_set_byte(style->background, 230, 230, 230, 255); // Light gray
    
    style->background_hovered = ese_color_create(engine);
    ese_color_ref(style->background_hovered);
    ese_color_set_byte(style->background_hovered, 204, 204, 204, 255); // Darker gray
    
    style->background_pressed = ese_color_create(engine);
    ese_color_ref(style->background_pressed);
    ese_color_set_byte(style->background_pressed, 179, 179, 179, 255); // Even darker gray
    
    style->border = ese_color_create(engine);
    ese_color_ref(style->border);
    ese_color_set_byte(style->border, 128, 128, 128, 255); // Medium gray
    
    style->border_hovered = ese_color_create(engine);
    ese_color_ref(style->border_hovered);
    ese_color_set_byte(style->border_hovered, 102, 102, 102, 255); // Darker gray
    
    style->border_pressed = ese_color_create(engine);
    ese_color_ref(style->border_pressed);
    ese_color_set_byte(style->border_pressed, 77, 77, 77, 255); // Even darker gray
    
    style->text = ese_color_create(engine);
    ese_color_ref(style->text);
    ese_color_set_byte(style->text, 26, 26, 26, 255); // Dark gray/black
    
    style->text_hovered = ese_color_create(engine);
    ese_color_ref(style->text_hovered);
    ese_color_set_byte(style->text_hovered, 0, 0, 0, 255); // Black
    
    style->text_pressed = ese_color_create(engine);
    ese_color_ref(style->text_pressed);
    ese_color_set_byte(style->text_pressed, 0, 0, 0, 255); // Black
    
    style->state = NULL;
    _ese_gui_style_set_lua_ref(style, LUA_NOREF);
    _ese_gui_style_set_lua_ref_count(style, 0);

    style->watchers = NULL;
    style->watcher_userdata = NULL;
    style->watcher_count = 0;
    style->watcher_capacity = 0;
    
    return style;
}

// Watcher system
/**
 * @brief Notifies all registered watchers of a gui style change
 * 
 * Iterates through all registered watcher callbacks and invokes them with the
 * updated style and their associated userdata. This is called whenever any
 * style property is modified.
 * 
 * @param style Pointer to the EseGuiStyle that has changed
 */
void _ese_gui_style_notify_watchers(EseGuiStyle *style) {
    if (!style || style->watcher_count == 0) return;
    
    for (size_t i = 0; i < style->watcher_count; i++) {
        if (style->watchers[i]) {
            style->watchers[i](style, style->watcher_userdata[i]);
        }
    }
}

// Private static setters for Lua state management
/**
 * @brief Sets the Lua registry reference for this gui style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @param lua_ref The Lua registry reference value
 */
static void _ese_gui_style_set_lua_ref(EseGuiStyle *style, int lua_ref) {
    if (style) {
        style->lua_ref = lua_ref;
    }
}

/**
 * @brief Sets the Lua reference count for this gui style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @param lua_ref_count The Lua reference count value
 */
static void _ese_gui_style_set_lua_ref_count(EseGuiStyle *style, int lua_ref_count) {
    if (style) {
        style->lua_ref_count = lua_ref_count;
    }
}

/**
 * @brief Sets the Lua state associated with this gui style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @param state Pointer to the Lua state
 */
static void _ese_gui_style_set_state(EseGuiStyle *style, lua_State *state) {
    if (style) {
        style->state = state;
    }
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseGuiStyle *ese_gui_style_create(EseLuaEngine *engine) {
    log_assert("GUI_STYLE", engine != NULL, "ese_gui_style_create called with NULL engine");
    
    EseGuiStyle *style = _ese_gui_style_make(engine);
    _ese_gui_style_set_state(style, engine->runtime);
    
    return style;
}

EseGuiStyle *ese_gui_style_copy(const EseGuiStyle *source) {
    log_assert("GUI_STYLE", source != NULL, "ese_gui_style_copy called with NULL source");
    
    EseLuaEngine *engine = NULL;
    if (source->state) {
        engine = (EseLuaEngine *)lua_engine_get_registry_key(source->state, LUA_ENGINE_KEY);
    }
    
    // If we can't get the engine from the source, we can't create a proper copy
    if (!engine) {
        return NULL;
    }
    
    EseGuiStyle *copy = _ese_gui_style_make(engine);
    if (!copy) return NULL;
    
    // Copy all properties
    copy->direction = source->direction;
    copy->justify = source->justify;
    copy->align_items = source->align_items;
    
    copy->border_width = source->border_width;
    copy->padding_left = source->padding_left;
    copy->padding_top = source->padding_top;
    copy->padding_right = source->padding_right;
    copy->padding_bottom = source->padding_bottom;
    copy->spacing = source->spacing;
    
    // Copy color pointers 
    copy->background = ese_color_copy(source->background);
    copy->background_hovered = ese_color_copy(source->background_hovered);
    copy->background_pressed = ese_color_copy(source->background_pressed);
    copy->border = ese_color_copy(source->border);
    copy->border_hovered = ese_color_copy(source->border_hovered);
    copy->border_pressed = ese_color_copy(source->border_pressed);
    copy->text = ese_color_copy(source->text);
    copy->text_hovered = ese_color_copy(source->text_hovered);
    copy->text_pressed = ese_color_copy(source->text_pressed);
    
    // Copy Lua state
    _ese_gui_style_set_state(copy, source->state);
    
    return copy;
}

void ese_gui_style_destroy(EseGuiStyle *style) {
    if (!style) return;
    
    // If there are Lua references, just decrement the count
    if (style->lua_ref != LUA_NOREF) {
        _ese_gui_style_set_lua_ref_count(style, style->lua_ref_count - 1);
        if (style->lua_ref_count <= 0) {
            // Remove Lua reference and let Lua GC handle cleanup
            if (style->state) {
                luaL_unref(style->state, LUA_REGISTRYINDEX, style->lua_ref);
            }
            _ese_gui_style_set_lua_ref(style, LUA_NOREF);
            _ese_gui_style_set_lua_ref_count(style, 0);
        }
        return;
    }
    
    // Free watcher arrays
    if (style->watchers) {
        memory_manager.free(style->watchers);
    }
    if (style->watcher_userdata) {
        memory_manager.free(style->watcher_userdata);
    }
    
    // Free the style itself
    memory_manager.free(style);
}

size_t ese_gui_style_sizeof(void) {
    return sizeof(EseGuiStyle);
}

// Property access
void ese_gui_style_set_direction(EseGuiStyle *style, EseGuiFlexDirection direction) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    style->direction = direction;
    _ese_gui_style_notify_watchers(style);
}

EseGuiFlexDirection ese_gui_style_get_direction(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->direction;
}

void ese_gui_style_set_justify(EseGuiStyle *style, EseGuiFlexJustify justify) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    style->justify = justify;
    _ese_gui_style_notify_watchers(style);
}

EseGuiFlexJustify ese_gui_style_get_justify(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->justify;
}

void ese_gui_style_set_align_items(EseGuiStyle *style, EseGuiFlexAlignItems align_items) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    style->align_items = align_items;
    _ese_gui_style_notify_watchers(style);
}

EseGuiFlexAlignItems ese_gui_style_get_align_items(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->align_items;
}

void ese_gui_style_set_background(EseGuiStyle *style, const EseColor *background) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    if (background) {
        ese_color_set_r(style->background, ese_color_get_r(background));
        ese_color_set_g(style->background, ese_color_get_g(background));
        ese_color_set_b(style->background, ese_color_get_b(background));
        ese_color_set_a(style->background, ese_color_get_a(background));
    }
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_background(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->background;
}

void ese_gui_style_set_background_hovered(EseGuiStyle *style, const EseColor *background_hovered) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    if (background_hovered) {
        ese_color_set_r(style->background_hovered, ese_color_get_r(background_hovered));
        ese_color_set_g(style->background_hovered, ese_color_get_g(background_hovered));
        ese_color_set_b(style->background_hovered, ese_color_get_b(background_hovered));
        ese_color_set_a(style->background_hovered, ese_color_get_a(background_hovered));
    }
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_background_hovered(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->background_hovered;
}

void ese_gui_style_set_background_pressed(EseGuiStyle *style, const EseColor *background_pressed) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    if (background_pressed) {
        ese_color_set_r(style->background_pressed, ese_color_get_r(background_pressed));
        ese_color_set_g(style->background_pressed, ese_color_get_g(background_pressed));
        ese_color_set_b(style->background_pressed, ese_color_get_b(background_pressed));
        ese_color_set_a(style->background_pressed, ese_color_get_a(background_pressed));
    }
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_background_pressed(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->background_pressed;
}

void ese_gui_style_set_border(EseGuiStyle *style, const EseColor *border) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    if (border) {
        ese_color_set_r(style->border, ese_color_get_r(border));
        ese_color_set_g(style->border, ese_color_get_g(border));
        ese_color_set_b(style->border, ese_color_get_b(border));
        ese_color_set_a(style->border, ese_color_get_a(border));
    }
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_border(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->border;
}

void ese_gui_style_set_border_hovered(EseGuiStyle *style, const EseColor *border_hovered) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    if (border_hovered) {
        ese_color_set_r(style->border_hovered, ese_color_get_r(border_hovered));
        ese_color_set_g(style->border_hovered, ese_color_get_g(border_hovered));
        ese_color_set_b(style->border_hovered, ese_color_get_b(border_hovered));
        ese_color_set_a(style->border_hovered, ese_color_get_a(border_hovered));
    }
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_border_hovered(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->border_hovered;
}

void ese_gui_style_set_border_pressed(EseGuiStyle *style, const EseColor *border_pressed) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    if (border_pressed) {
        ese_color_set_r(style->border_pressed, ese_color_get_r(border_pressed));
        ese_color_set_g(style->border_pressed, ese_color_get_g(border_pressed));
        ese_color_set_b(style->border_pressed, ese_color_get_b(border_pressed));
        ese_color_set_a(style->border_pressed, ese_color_get_a(border_pressed));
    }
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_border_pressed(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->border_pressed;
}

void ese_gui_style_set_text(EseGuiStyle *style, const EseColor *text) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    if (text) {
        ese_color_set_r(style->text, ese_color_get_r(text));
        ese_color_set_g(style->text, ese_color_get_g(text));
        ese_color_set_b(style->text, ese_color_get_b(text));
        ese_color_set_a(style->text, ese_color_get_a(text));
    }
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_text(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->text;
}

void ese_gui_style_set_text_hovered(EseGuiStyle *style, const EseColor *text_hovered) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    if (text_hovered) {
        ese_color_set_r(style->text_hovered, ese_color_get_r(text_hovered));
        ese_color_set_g(style->text_hovered, ese_color_get_g(text_hovered));
        ese_color_set_b(style->text_hovered, ese_color_get_b(text_hovered));
        ese_color_set_a(style->text_hovered, ese_color_get_a(text_hovered));
    }
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_text_hovered(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->text_hovered;
}

void ese_gui_style_set_text_pressed(EseGuiStyle *style, const EseColor *text_pressed) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    if (text_pressed) {
        ese_color_set_r(style->text_pressed, ese_color_get_r(text_pressed));
        ese_color_set_g(style->text_pressed, ese_color_get_g(text_pressed));
        ese_color_set_b(style->text_pressed, ese_color_get_b(text_pressed));
        ese_color_set_a(style->text_pressed, ese_color_get_a(text_pressed));
    }
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_text_pressed(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->text_pressed;
}

void ese_gui_style_set_border_width(EseGuiStyle *style, int border_width) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    style->border_width = border_width;
    _ese_gui_style_notify_watchers(style);
}

int ese_gui_style_get_border_width(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->border_width;
}

void ese_gui_style_set_padding_left(EseGuiStyle *style, int padding_left) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    style->padding_left = padding_left;
    _ese_gui_style_notify_watchers(style);
}

int ese_gui_style_get_padding_left(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->padding_left;
}

void ese_gui_style_set_padding_top(EseGuiStyle *style, int padding_top) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    style->padding_top = padding_top;
    _ese_gui_style_notify_watchers(style);
}

int ese_gui_style_get_padding_top(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->padding_top;
}

void ese_gui_style_set_padding_right(EseGuiStyle *style, int padding_right) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    style->padding_right = padding_right;
    _ese_gui_style_notify_watchers(style);
}

int ese_gui_style_get_padding_right(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->padding_right;
}

void ese_gui_style_set_padding_bottom(EseGuiStyle *style, int padding_bottom) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    style->padding_bottom = padding_bottom;
    _ese_gui_style_notify_watchers(style);
}

int ese_gui_style_get_padding_bottom(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->padding_bottom;
}

void ese_gui_style_set_spacing(EseGuiStyle *style, int spacing) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    style->spacing = spacing;
    _ese_gui_style_notify_watchers(style);
}

int ese_gui_style_get_spacing(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->spacing;
}

// Lua-related access
lua_State *ese_gui_style_get_state(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->state;
}

int ese_gui_style_get_lua_ref(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->lua_ref;
}

int ese_gui_style_get_lua_ref_count(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->lua_ref_count;
}

bool ese_gui_style_add_watcher(EseGuiStyle *style, EseGuiStyleWatcherCallback callback, void *userdata) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", callback != NULL, "NULL callback parameter");
    
    // Check if we need to expand the watcher arrays
    if (style->watcher_count >= style->watcher_capacity) {
        size_t new_capacity = style->watcher_capacity == 0 ? 4 : style->watcher_capacity * 2;
        
        EseGuiStyleWatcherCallback *new_watchers = (EseGuiStyleWatcherCallback *)memory_manager.realloc(
            style->watchers, sizeof(EseGuiStyleWatcherCallback) * new_capacity, MMTAG_GUI);
        if (!new_watchers) return false;
        
        void **new_userdata = (void **)memory_manager.realloc(
            style->watcher_userdata, sizeof(void *) * new_capacity, MMTAG_GUI);
        if (!new_userdata) {
            memory_manager.free(new_watchers);
            return false;
        }
        
        style->watchers = new_watchers;
        style->watcher_userdata = new_userdata;
        style->watcher_capacity = new_capacity;
    }
    
    // Add the new watcher
    style->watchers[style->watcher_count] = callback;
    style->watcher_userdata[style->watcher_count] = userdata;
    style->watcher_count++;
    
    return true;
}

bool ese_gui_style_remove_watcher(EseGuiStyle *style, EseGuiStyleWatcherCallback callback, void *userdata) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", callback != NULL, "NULL callback parameter");
    
    for (size_t i = 0; i < style->watcher_count; i++) {
        if (style->watchers[i] == callback && style->watcher_userdata[i] == userdata) {
            // Move remaining watchers down
            for (size_t j = i; j < style->watcher_count - 1; j++) {
                style->watchers[j] = style->watchers[j + 1];
                style->watcher_userdata[j] = style->watcher_userdata[j + 1];
            }
            style->watcher_count--;
            return true;
        }
    }
    
    return false;
}

// Lua integration
void ese_gui_style_lua_init(EseLuaEngine *engine) {
    _ese_gui_style_lua_init(engine);
}

void ese_gui_style_lua_push(EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    
    lua_State *L = ese_gui_style_get_state(style);
    if (!L) {
        return; // No Lua state available
    }
    
    // Check if we already have a proxy table for this style
    if (ese_gui_style_get_lua_ref(style) != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ese_gui_style_get_lua_ref(style));
        return;
    }
    
    // Create a new proxy table
    lua_newtable(L);
    
    // Create userdata to hold the style pointer
    EseGuiStyle **ud = (EseGuiStyle **)lua_newuserdata(L, sizeof(EseGuiStyle *));
    *ud = style;
    
    // Set the metatable
    luaL_setmetatable(L, GUI_STYLE_PROXY_META);
    
    // Store the userdata in the proxy table
    lua_setfield(L, -2, "__ptr");
    
    // Set the proxy table's metatable
    luaL_setmetatable(L, GUI_STYLE_PROXY_META);
}

EseGuiStyle *ese_gui_style_lua_get(lua_State *L, int idx) {
    if (!lua_istable(L, idx)) {
        return NULL;
    }
    
    // Get the __ptr field
    lua_getfield(L, idx, "__ptr");
    if (!lua_isuserdata(L, -1)) {
        lua_pop(L, 1);
        return NULL;
    }
    
    EseGuiStyle **ud = (EseGuiStyle **)lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    if (!ud) {
        return NULL;
    }
    
    return *ud;
}

void ese_gui_style_ref(EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    
    if (style->lua_ref == LUA_NOREF) {
        // Push to Lua and reference it
        ese_gui_style_lua_push(style);
        if (style->state) {
            int ref = luaL_ref(style->state, LUA_REGISTRYINDEX);
            _ese_gui_style_set_lua_ref(style, ref);
            _ese_gui_style_set_lua_ref_count(style, 1);
        }
    } else {
        // Increment existing reference count
        _ese_gui_style_set_lua_ref_count(style, style->lua_ref_count + 1);
    }
}

void ese_gui_style_unref(EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    
    if (style->lua_ref != LUA_NOREF) {
        _ese_gui_style_set_lua_ref_count(style, style->lua_ref_count - 1);
        if (style->lua_ref_count <= 0) {
            if (style->state) {
                luaL_unref(style->state, LUA_REGISTRYINDEX, style->lua_ref);
            }
            _ese_gui_style_set_lua_ref(style, LUA_NOREF);
            _ese_gui_style_set_lua_ref_count(style, 0);
        }
    }
}

cJSON *ese_gui_style_serialize(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, "type", "GUI_STYLE");
    cJSON_AddNumberToObject(json, "direction", (double)style->direction);
    cJSON_AddNumberToObject(json, "justify", (double)style->justify);
    cJSON_AddNumberToObject(json, "align_items", (double)style->align_items);
    
    cJSON_AddNumberToObject(json, "border_width", (double)style->border_width);
    cJSON_AddNumberToObject(json, "padding_left", (double)style->padding_left);
    cJSON_AddNumberToObject(json, "padding_top", (double)style->padding_top);
    cJSON_AddNumberToObject(json, "padding_right", (double)style->padding_right);
    cJSON_AddNumberToObject(json, "padding_bottom", (double)style->padding_bottom);
    cJSON_AddNumberToObject(json, "spacing", (double)style->spacing);
    
    // Serialize colors if they exist
    if (style->background) {
        cJSON *bg_json = ese_color_serialize(style->background);
        if (bg_json) cJSON_AddItemToObject(json, "background", bg_json);
    }
    if (style->background_hovered) {
        cJSON *bg_hov_json = ese_color_serialize(style->background_hovered);
        if (bg_hov_json) cJSON_AddItemToObject(json, "background_hovered", bg_hov_json);
    }
    if (style->background_pressed) {
        cJSON *bg_press_json = ese_color_serialize(style->background_pressed);
        if (bg_press_json) cJSON_AddItemToObject(json, "background_pressed", bg_press_json);
    }
    if (style->border) {
        cJSON *border_json = ese_color_serialize(style->border);
        if (border_json) cJSON_AddItemToObject(json, "border", border_json);
    }
    if (style->border_hovered) {
        cJSON *border_hov_json = ese_color_serialize(style->border_hovered);
        if (border_hov_json) cJSON_AddItemToObject(json, "border_hovered", border_hov_json);
    }
    if (style->border_pressed) {
        cJSON *border_press_json = ese_color_serialize(style->border_pressed);
        if (border_press_json) cJSON_AddItemToObject(json, "border_pressed", border_press_json);
    }
    if (style->text) {
        cJSON *text_json = ese_color_serialize(style->text);
        if (text_json) cJSON_AddItemToObject(json, "text", text_json);
    }
    if (style->text_hovered) {
        cJSON *text_hov_json = ese_color_serialize(style->text_hovered);
        if (text_hov_json) cJSON_AddItemToObject(json, "text_hovered", text_hov_json);
    }
    if (style->text_pressed) {
        cJSON *text_press_json = ese_color_serialize(style->text_pressed);
        if (text_press_json) cJSON_AddItemToObject(json, "text_pressed", text_press_json);
    }
    
    return json;
}

EseGuiStyle *ese_gui_style_deserialize(EseLuaEngine *engine, const cJSON *data) {
    log_assert("GUI_STYLE", engine != NULL, "ese_gui_style_deserialize called with NULL engine");
    log_assert("GUI_STYLE", data != NULL, "ese_gui_style_deserialize called with NULL data");
    
    // Check if this is a GUI_STYLE object
    cJSON *type = cJSON_GetObjectItemCaseSensitive(data, "type");
    if (!type || !cJSON_IsString(type) || strcmp(type->valuestring, "GUI_STYLE") != 0) {
        return NULL;
    }
    
    EseGuiStyle *style = ese_gui_style_create(engine);
    if (!style) return NULL;
    
    // Deserialize properties
    cJSON *direction = cJSON_GetObjectItemCaseSensitive(data, "direction");
    if (cJSON_IsNumber(direction)) {
        style->direction = (EseGuiFlexDirection)direction->valueint;
    }
    
    cJSON *justify = cJSON_GetObjectItemCaseSensitive(data, "justify");
    if (cJSON_IsNumber(justify)) {
        style->justify = (EseGuiFlexJustify)justify->valueint;
    }
    
    cJSON *align_items = cJSON_GetObjectItemCaseSensitive(data, "align_items");
    if (cJSON_IsNumber(align_items)) {
        style->align_items = (EseGuiFlexAlignItems)align_items->valueint;
    }
    
    cJSON *border_width = cJSON_GetObjectItemCaseSensitive(data, "border_width");
    if (cJSON_IsNumber(border_width)) {
        style->border_width = border_width->valueint;
    }
    
    cJSON *padding_left = cJSON_GetObjectItemCaseSensitive(data, "padding_left");
    if (cJSON_IsNumber(padding_left)) {
        style->padding_left = padding_left->valueint;
    }
    
    cJSON *padding_top = cJSON_GetObjectItemCaseSensitive(data, "padding_top");
    if (cJSON_IsNumber(padding_top)) {
        style->padding_top = padding_top->valueint;
    }
    
    cJSON *padding_right = cJSON_GetObjectItemCaseSensitive(data, "padding_right");
    if (cJSON_IsNumber(padding_right)) {
        style->padding_right = padding_right->valueint;
    }
    
    cJSON *padding_bottom = cJSON_GetObjectItemCaseSensitive(data, "padding_bottom");
    if (cJSON_IsNumber(padding_bottom)) {
        style->padding_bottom = padding_bottom->valueint;
    }
    
    cJSON *spacing = cJSON_GetObjectItemCaseSensitive(data, "spacing");
    if (cJSON_IsNumber(spacing)) {
        style->spacing = spacing->valueint;
    }
    
    // Deserialize colors
    cJSON *background = cJSON_GetObjectItemCaseSensitive(data, "background");
    if (cJSON_IsObject(background)) {
        style->background = ese_color_deserialize(engine, background);
    } else {
        style->background = ese_color_create(engine);
        ese_color_ref(style->background);
        ese_color_set_byte(style->background, 230, 230, 230, 255); // Light gray
    }
    
    cJSON *background_hovered = cJSON_GetObjectItemCaseSensitive(data, "background_hovered");
    if (cJSON_IsObject(background_hovered)) {
        style->background_hovered = ese_color_deserialize(engine, background_hovered);
    } else {
        style->background_hovered = ese_color_create(engine);
        ese_color_ref(style->background_hovered);
        ese_color_set_byte(style->background_hovered, 204, 204, 204, 255); // Darker gray
    }
    
    cJSON *background_pressed = cJSON_GetObjectItemCaseSensitive(data, "background_pressed");
    if (cJSON_IsObject(background_pressed)) {
        style->background_pressed = ese_color_deserialize(engine, background_pressed);
    } else {
        style->background_pressed = ese_color_create(engine);
        ese_color_ref(style->background_pressed);
        ese_color_set_byte(style->background_pressed, 179, 179, 179, 255); // Even darker gray
    }
    
    cJSON *border = cJSON_GetObjectItemCaseSensitive(data, "border");
    if (cJSON_IsObject(border)) {
        style->border = ese_color_deserialize(engine, border);
    } else {
        style->border = ese_color_create(engine);
        ese_color_ref(style->border);
        ese_color_set_byte(style->border, 128, 128, 128, 255); // Medium gray
    }
    
    cJSON *border_hovered = cJSON_GetObjectItemCaseSensitive(data, "border_hovered");
    if (cJSON_IsObject(border_hovered)) {
        style->border_hovered = ese_color_deserialize(engine, border_hovered);
    } else {
        style->border_hovered = ese_color_create(engine);
        ese_color_ref(style->border_hovered);
        ese_color_set_byte(style->border_hovered, 102, 102, 102, 255); // Darker gray
    }
    
    cJSON *border_pressed = cJSON_GetObjectItemCaseSensitive(data, "border_pressed");
    if (cJSON_IsObject(border_pressed)) {
        style->border_pressed = ese_color_deserialize(engine, border_pressed);
    } else {
        style->border_pressed = ese_color_create(engine);
        ese_color_ref(style->border_pressed);
        ese_color_set_byte(style->border_pressed, 77, 77, 77, 255); // Even darker gray
    }
    
    cJSON *text = cJSON_GetObjectItemCaseSensitive(data, "text");
    if (cJSON_IsObject(text)) {
        style->text = ese_color_deserialize(engine, text);
    } else {
        style->text = ese_color_create(engine);
        ese_color_ref(style->text);
        ese_color_set_byte(style->text, 26, 26, 26, 255); // Dark gray/black
    }
    
    cJSON *text_hovered = cJSON_GetObjectItemCaseSensitive(data, "text_hovered");
    if (cJSON_IsObject(text_hovered)) {
        style->text_hovered = ese_color_deserialize(engine, text_hovered);
    } else {
        style->text_hovered = ese_color_create(engine);
        ese_color_ref(style->text_hovered);
        ese_color_set_byte(style->text_hovered, 0, 0, 0, 255); // Black
    }
    
    cJSON *text_pressed = cJSON_GetObjectItemCaseSensitive(data, "text_pressed");
    if (cJSON_IsObject(text_pressed)) {
        style->text_pressed = ese_color_deserialize(engine, text_pressed);
    } else {
        style->text_pressed = ese_color_create(engine);
        ese_color_ref(style->text_pressed);
        ese_color_set_byte(style->text_pressed, 0, 0, 0, 255); // Black
    }
    
    return style;
}
