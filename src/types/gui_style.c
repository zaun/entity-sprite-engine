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
#include "graphics/gui/gui.h"
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


    // THEME/CONTEXT COLORS
    EseColor *primary;              // 0x0d6efd
    EseColor *primary_hover;        // 0x0b5ed7
    EseColor *primary_active;       // 0x0a58ca
    EseColor *secondary;            // 0x6c757d
    EseColor *secondary_hover;      // 0x5c636a
    EseColor *secondary_active;     // 0x565e64
    EseColor *success;              // 0x198754
    EseColor *success_hover;        // 0x157347
    EseColor *success_active;       // 0x146c43
    EseColor *info;                 // 0x0dcaf0
    EseColor *info_hover;           // 0x31d2f2
    EseColor *info_active;          // 0x3dd5f3
    EseColor *warning;              // 0xffc107
    EseColor *warning_hover;        // 0xffcd39
    EseColor *warning_active;       // 0xffdb6d
    EseColor *danger;               // 0xdc3545
    EseColor *danger_hover;         // 0xbb2d3b
    EseColor *danger_active;        // 0xb02a37
    EseColor *light;                // 0xf8f9fa
    EseColor *light_hover;          // 0xf9fafb
    EseColor *light_active;         // 0xf9fafb
    EseColor *dark;                 // 0x212529
    EseColor *dark_hover;           // 0x1c1f23
    EseColor *dark_active;          // 0x1a1e21

    // ALERTS
    EseColor *alert_success_bg;      // 0xd1e7dd
    EseColor *alert_success_text;    // 0x0f5132
    EseColor *alert_success_border;  // 0xbcd0c7
    EseColor *alert_info_bg;         // 0xcff4fc
    EseColor *alert_info_text;       // 0x055160
    EseColor *alert_info_border;     // 0xb6effb
    EseColor *alert_warning_bg;      // 0xfff3cd
    EseColor *alert_warning_text;    // 0x664d03
    EseColor *alert_warning_border;  // 0xffecb5
    EseColor *alert_danger_bg;       // 0xf8d7da
    EseColor *alert_danger_text;     // 0x842029
    EseColor *alert_danger_border;   // 0xf5c2c7

    // BACKGROUNDS
    EseColor *bg_primary;             // 0x0d6efd
    EseColor *bg_secondary;           // 0x6c757d
    EseColor *bg_success;             // 0x198754
    EseColor *bg_info;                // 0x0dcaf0
    EseColor *bg_warning;             // 0xffc107
    EseColor *bg_danger;              // 0xdc3545
    EseColor *bg_light;               // 0xf8f9fa
    EseColor *bg_dark;                // 0x212529
    EseColor *bg_white;               // 0xffffff
    EseColor *bg_transparent;         // 0x000000       /* alpha = 0 */

    // TEXT COLORS
    EseColor *text_primary;           // 0x0d6efd
    EseColor *text_secondary;         // 0x6c757d
    EseColor *text_success;           // 0x198754
    EseColor *text_info;              // 0x0dcaf0
    EseColor *text_warning;           // 0xffc107
    EseColor *text_danger;            // 0xdc3545
    EseColor *text_light;             // 0xf8f9fa
    EseColor *text_dark;              // 0x212529
    EseColor *text_body;              // 0x212529       // default body text
    EseColor *text_muted;             // 0x6c757d
    EseColor *text_white;             // 0xffffff
    EseColor *text_black;             // 0x000000
    EseColor *text_reset;             // 0x212529

    // BORDERS
    EseColor *border_primary;         // 0x0d6efd
    EseColor *border_secondary;       // 0x6c757d
    EseColor *border_success;         // 0x198754
    EseColor *border_info;            // 0x0dcaf0
    EseColor *border_warning;         // 0xffc107
    EseColor *border_danger;          // 0xdc3545
    EseColor *border_light;           // 0xf8f9fa
    EseColor *border_dark;            // 0x212529
    EseColor *border_white;           // 0xffffff
    EseColor *border_gray_200;        // 0xe9ecef
    EseColor *border_gray_300;        // 0xdee2e6

    // TOOLTIPS
    EseColor *tooltip_bg;             // 0x212529
    EseColor *tooltip_color;          // 0xffffff

    // MISCELLANEOUS UTILITIES
    EseColor *selection_bg;           // 0x0d6efd
    EseColor *selection_color;        // 0xffffff
    EseColor *focus_ring;             // 0x0d6efd
    EseColor *highlight;              // 0xfffae6
    EseColor *list_group_action;      // 0xf8f9fa

    int border_width;

    int padding_left;
    int padding_top;
    int padding_right;
    int padding_bottom;

    int spacing;
    int font_size;

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
    printf("ese_gui_style_make: Creating new EseGuiStyle\n");
    
    EseGuiStyle *style = (EseGuiStyle *)memory_manager.malloc(sizeof(EseGuiStyle), MMTAG_GUI_STYLE);
    style->direction = FLEX_DIRECTION_ROW;
    style->justify = FLEX_JUSTIFY_START;
    style->align_items = FLEX_ALIGN_ITEMS_START;
    
    style->border_width = 1;
    style->padding_left = 4;
    style->padding_top = 4;
    style->padding_right = 4;
    style->padding_bottom = 4;
    style->spacing = 4;
    style->font_size = 20;
    
    // Create colors
    style->primary = ese_color_create(engine);
    style->primary_hover = ese_color_create(engine);
    style->primary_active = ese_color_create(engine);
    style->secondary = ese_color_create(engine);
    style->secondary_hover = ese_color_create(engine);
    style->secondary_active = ese_color_create(engine);
    style->success = ese_color_create(engine);
    style->success_hover = ese_color_create(engine);
    style->success_active = ese_color_create(engine);
    style->info = ese_color_create(engine);
    style->info_hover = ese_color_create(engine);
    style->info_active = ese_color_create(engine);
    style->warning = ese_color_create(engine);
    style->warning_hover = ese_color_create(engine);
    style->warning_active = ese_color_create(engine);
    style->danger = ese_color_create(engine);
    style->danger_hover = ese_color_create(engine);
    style->danger_active = ese_color_create(engine);
    style->light = ese_color_create(engine);
    style->light_hover = ese_color_create(engine);
    style->light_active = ese_color_create(engine);
    style->dark = ese_color_create(engine);
    style->dark_hover = ese_color_create(engine);
    style->dark_active = ese_color_create(engine);

    style->alert_success_bg = ese_color_create(engine);
    style->alert_success_text = ese_color_create(engine);
    style->alert_success_border = ese_color_create(engine);
    style->alert_info_bg = ese_color_create(engine);
    style->alert_info_text = ese_color_create(engine);
    style->alert_info_border = ese_color_create(engine);
    style->alert_warning_bg = ese_color_create(engine);
    style->alert_warning_text = ese_color_create(engine);
    style->alert_warning_border = ese_color_create(engine);
    style->alert_danger_bg = ese_color_create(engine);
    style->alert_danger_text = ese_color_create(engine);
    style->alert_danger_border = ese_color_create(engine);

    style->bg_primary = ese_color_create(engine);
    style->bg_secondary = ese_color_create(engine);
    style->bg_success = ese_color_create(engine);
    style->bg_info = ese_color_create(engine);
    style->bg_warning = ese_color_create(engine);
    style->bg_danger = ese_color_create(engine);
    style->bg_light = ese_color_create(engine);
    style->bg_dark = ese_color_create(engine);
    style->bg_white = ese_color_create(engine);
    style->bg_transparent = ese_color_create(engine);

    style->text_primary = ese_color_create(engine);
    style->text_secondary = ese_color_create(engine);
    style->text_success = ese_color_create(engine);
    style->text_info = ese_color_create(engine);
    style->text_warning = ese_color_create(engine);
    style->text_danger = ese_color_create(engine);
    style->text_light = ese_color_create(engine);
    style->text_dark = ese_color_create(engine);
    style->text_body = ese_color_create(engine);
    style->text_muted = ese_color_create(engine);
    style->text_white = ese_color_create(engine);
    style->text_black = ese_color_create(engine);
    style->text_reset = ese_color_create(engine);

    style->border_primary = ese_color_create(engine);
    style->border_secondary = ese_color_create(engine);
    style->border_success = ese_color_create(engine);
    style->border_info = ese_color_create(engine);
    style->border_warning = ese_color_create(engine);
    style->border_danger = ese_color_create(engine);
    style->border_light = ese_color_create(engine);
    style->border_dark = ese_color_create(engine);
    style->border_white = ese_color_create(engine);
    style->border_gray_200 = ese_color_create(engine);
    style->border_gray_300 = ese_color_create(engine);

    style->tooltip_bg = ese_color_create(engine);
    style->tooltip_color = ese_color_create(engine);
    style->selection_bg = ese_color_create(engine);
    style->selection_color = ese_color_create(engine);
    style->focus_ring = ese_color_create(engine);
    style->highlight = ese_color_create(engine);
    style->list_group_action = ese_color_create(engine);

    // Set colors
    ese_color_set_hex(style->primary, "#0d6efd");
    ese_color_set_hex(style->primary_hover, "#0b5ed7");
    ese_color_set_hex(style->primary_active, "#0a58ca");
    ese_color_set_hex(style->secondary, "#6c757d");
    ese_color_set_hex(style->secondary_hover, "#5c636a");
    ese_color_set_hex(style->secondary_active, "#565e64");
    ese_color_set_hex(style->success, "#198754");
    ese_color_set_hex(style->success_hover, "#157347");
    ese_color_set_hex(style->success_active, "#146c43");
    ese_color_set_hex(style->info, "#0dcaf0");
    ese_color_set_hex(style->info_hover, "#31d2f2");
    ese_color_set_hex(style->info_active, "#3dd5f3");
    ese_color_set_hex(style->warning, "#ffc107");
    ese_color_set_hex(style->warning_hover, "#ffcd39");
    ese_color_set_hex(style->warning_active, "#ffdb6d");
    ese_color_set_hex(style->danger, "#dc3545");
    ese_color_set_hex(style->danger_hover, "#bb2d3b");
    ese_color_set_hex(style->danger_active, "#b02a37");
    ese_color_set_hex(style->light, "#f8f9fa");
    ese_color_set_hex(style->light_hover, "#f9fafb");
    ese_color_set_hex(style->light_active, "#f9fafb");
    ese_color_set_hex(style->dark, "#212529");
    ese_color_set_hex(style->dark_hover, "#1c1f23");
    ese_color_set_hex(style->dark_active, "#1a1e21");

    ese_color_set_hex(style->alert_success_bg, "#d1e7dd");
    ese_color_set_hex(style->alert_success_text, "#0f5132");
    ese_color_set_hex(style->alert_success_border, "#bcd0c7");
    ese_color_set_hex(style->alert_info_bg, "#cff4fc");
    ese_color_set_hex(style->alert_info_text, "#055160");
    ese_color_set_hex(style->alert_info_border, "#b6effb");
    ese_color_set_hex(style->alert_warning_bg, "#fff3cd");
    ese_color_set_hex(style->alert_warning_text, "#664d03");
    ese_color_set_hex(style->alert_warning_border, "#ffecb5");
    ese_color_set_hex(style->alert_danger_bg, "#f8d7da");
    ese_color_set_hex(style->alert_danger_text, "#842029");
    ese_color_set_hex(style->alert_danger_border, "#f5c2c7");

    ese_color_set_hex(style->bg_primary, "#0d6efd");
    ese_color_set_hex(style->bg_secondary, "#6c757d");
    ese_color_set_hex(style->bg_success, "#198754");
    ese_color_set_hex(style->bg_info, "#0dcaf0");
    ese_color_set_hex(style->bg_warning, "#ffc107");
    ese_color_set_hex(style->bg_danger, "#dc3545");
    ese_color_set_hex(style->bg_light, "#f8f9fa");
    ese_color_set_hex(style->bg_dark, "#212529");
    ese_color_set_hex(style->bg_white, "#ffffff");
    ese_color_set_hex(style->bg_transparent, "#00000000");

    ese_color_set_hex(style->text_primary, "#0d6efd");
    ese_color_set_hex(style->text_secondary, "#6c757d");
    ese_color_set_hex(style->text_success, "#198754");
    ese_color_set_hex(style->text_info, "#0dcaf0");
    ese_color_set_hex(style->text_warning, "#ffc107");
    ese_color_set_hex(style->text_danger, "#dc3545");
    ese_color_set_hex(style->text_light, "#f8f9fa");
    ese_color_set_hex(style->text_dark, "#212529");
    ese_color_set_hex(style->text_body, "#212529");
    ese_color_set_hex(style->text_muted, "#6c757d");
    ese_color_set_hex(style->text_white, "#ffffff");
    ese_color_set_hex(style->text_black, "#000000");
    ese_color_set_hex(style->text_reset, "#212529");

    ese_color_set_hex(style->border_primary, "#0d6efd");
    ese_color_set_hex(style->border_secondary, "#6c757d");
    ese_color_set_hex(style->border_success, "#198754");
    ese_color_set_hex(style->border_info, "#0dcaf0");
    ese_color_set_hex(style->border_warning, "#ffc107");
    ese_color_set_hex(style->border_danger, "#dc3545");
    ese_color_set_hex(style->border_light, "#f8f9fa");
    ese_color_set_hex(style->border_dark, "#212529");
    ese_color_set_hex(style->border_white, "#ffffff");
    ese_color_set_hex(style->border_gray_200, "#e9ecef");
    ese_color_set_hex(style->border_gray_300, "#dee2e6");

    ese_color_set_hex(style->tooltip_bg, "#212529");
    ese_color_set_hex(style->tooltip_color, "#ffffff");
    ese_color_set_hex(style->selection_bg, "#0d6efd");
    ese_color_set_hex(style->selection_color, "#ffffff");
    ese_color_set_hex(style->focus_ring, "#0d6efd");
    ese_color_set_hex(style->highlight, "#fffae6");
    ese_color_set_hex(style->list_group_action, "#f8f9fa");
    
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
    printf("ese_gui_style_copy: Copying EseGuiStyle\n");
    
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
    copy->font_size = source->font_size;
    
    // Copy all colors by value into pre-created EseColor instances
    #define SET_COLOR(dst, src) do { \
        ese_color_set_r((dst), ese_color_get_r((src))); \
        ese_color_set_g((dst), ese_color_get_g((src))); \
        ese_color_set_b((dst), ese_color_get_b((src))); \
        ese_color_set_a((dst), ese_color_get_a((src))); \
    } while (0)

    SET_COLOR(copy->primary, source->primary);
    SET_COLOR(copy->primary_hover, source->primary_hover);
    SET_COLOR(copy->primary_active, source->primary_active);
    SET_COLOR(copy->secondary, source->secondary);
    SET_COLOR(copy->secondary_hover, source->secondary_hover);
    SET_COLOR(copy->secondary_active, source->secondary_active);
    SET_COLOR(copy->success, source->success);
    SET_COLOR(copy->success_hover, source->success_hover);
    SET_COLOR(copy->success_active, source->success_active);
    SET_COLOR(copy->info, source->info);
    SET_COLOR(copy->info_hover, source->info_hover);
    SET_COLOR(copy->info_active, source->info_active);
    SET_COLOR(copy->warning, source->warning);
    SET_COLOR(copy->warning_hover, source->warning_hover);
    SET_COLOR(copy->warning_active, source->warning_active);
    SET_COLOR(copy->danger, source->danger);
    SET_COLOR(copy->danger_hover, source->danger_hover);
    SET_COLOR(copy->danger_active, source->danger_active);
    SET_COLOR(copy->light, source->light);
    SET_COLOR(copy->light_hover, source->light_hover);
    SET_COLOR(copy->light_active, source->light_active);
    SET_COLOR(copy->dark, source->dark);
    SET_COLOR(copy->dark_hover, source->dark_hover);
    SET_COLOR(copy->dark_active, source->dark_active);

    SET_COLOR(copy->alert_success_bg, source->alert_success_bg);
    SET_COLOR(copy->alert_success_text, source->alert_success_text);
    SET_COLOR(copy->alert_success_border, source->alert_success_border);
    SET_COLOR(copy->alert_info_bg, source->alert_info_bg);
    SET_COLOR(copy->alert_info_text, source->alert_info_text);
    SET_COLOR(copy->alert_info_border, source->alert_info_border);
    SET_COLOR(copy->alert_warning_bg, source->alert_warning_bg);
    SET_COLOR(copy->alert_warning_text, source->alert_warning_text);
    SET_COLOR(copy->alert_warning_border, source->alert_warning_border);
    SET_COLOR(copy->alert_danger_bg, source->alert_danger_bg);
    SET_COLOR(copy->alert_danger_text, source->alert_danger_text);
    SET_COLOR(copy->alert_danger_border, source->alert_danger_border);

    SET_COLOR(copy->bg_primary, source->bg_primary);
    SET_COLOR(copy->bg_secondary, source->bg_secondary);
    SET_COLOR(copy->bg_success, source->bg_success);
    SET_COLOR(copy->bg_info, source->bg_info);
    SET_COLOR(copy->bg_warning, source->bg_warning);
    SET_COLOR(copy->bg_danger, source->bg_danger);
    SET_COLOR(copy->bg_light, source->bg_light);
    SET_COLOR(copy->bg_dark, source->bg_dark);
    SET_COLOR(copy->bg_white, source->bg_white);
    SET_COLOR(copy->bg_transparent, source->bg_transparent);

    SET_COLOR(copy->text_primary, source->text_primary);
    SET_COLOR(copy->text_secondary, source->text_secondary);
    SET_COLOR(copy->text_success, source->text_success);
    SET_COLOR(copy->text_info, source->text_info);
    SET_COLOR(copy->text_warning, source->text_warning);
    SET_COLOR(copy->text_danger, source->text_danger);
    SET_COLOR(copy->text_light, source->text_light);
    SET_COLOR(copy->text_dark, source->text_dark);
    SET_COLOR(copy->text_body, source->text_body);
    SET_COLOR(copy->text_muted, source->text_muted);
    SET_COLOR(copy->text_white, source->text_white);
    SET_COLOR(copy->text_black, source->text_black);
    SET_COLOR(copy->text_reset, source->text_reset);

    SET_COLOR(copy->border_primary, source->border_primary);
    SET_COLOR(copy->border_secondary, source->border_secondary);
    SET_COLOR(copy->border_success, source->border_success);
    SET_COLOR(copy->border_info, source->border_info);
    SET_COLOR(copy->border_warning, source->border_warning);
    SET_COLOR(copy->border_danger, source->border_danger);
    SET_COLOR(copy->border_light, source->border_light);
    SET_COLOR(copy->border_dark, source->border_dark);
    SET_COLOR(copy->border_white, source->border_white);
    SET_COLOR(copy->border_gray_200, source->border_gray_200);
    SET_COLOR(copy->border_gray_300, source->border_gray_300);

    SET_COLOR(copy->tooltip_bg, source->tooltip_bg);
    SET_COLOR(copy->tooltip_color, source->tooltip_color);
    SET_COLOR(copy->selection_bg, source->selection_bg);
    SET_COLOR(copy->selection_color, source->selection_color);
    SET_COLOR(copy->focus_ring, source->focus_ring);
    SET_COLOR(copy->highlight, source->highlight);
    SET_COLOR(copy->list_group_action, source->list_group_action);

    #undef SET_COLOR
    
    // Copy Lua state
    _ese_gui_style_set_state(copy, source->state);
    
    return copy;
}

void ese_gui_style_destroy(EseGuiStyle *style) {
    if (!style) return;
    
    if (ese_gui_style_get_lua_ref(style) == LUA_NOREF) {
        // No Lua references, safe to free immediately
        printf("ese_gui_style_destroy: No Lua references, safe to free immediately\n");
    
        // Free watcher arrays if they exist
        if (style->watchers) {
            memory_manager.free(style->watchers);
            style->watchers = NULL;
        }
        if (style->watcher_userdata) {
            memory_manager.free(style->watcher_userdata);
            style->watcher_userdata = NULL;
        }
        style->watcher_count = 0;
        style->watcher_capacity = 0;
        

        // destroy colors
        ese_color_destroy(style->primary);
        ese_color_destroy(style->primary_hover);
        ese_color_destroy(style->primary_active);
        ese_color_destroy(style->secondary);
        ese_color_destroy(style->secondary_hover);
        ese_color_destroy(style->secondary_active);
        ese_color_destroy(style->success);
        ese_color_destroy(style->success_hover);
        ese_color_destroy(style->success_active);
        ese_color_destroy(style->info);
        ese_color_destroy(style->info_hover);
        ese_color_destroy(style->info_active);
        ese_color_destroy(style->warning);
        ese_color_destroy(style->warning_hover);
        ese_color_destroy(style->warning_active);
        ese_color_destroy(style->danger);
        ese_color_destroy(style->danger_hover);
        ese_color_destroy(style->danger_active);
        ese_color_destroy(style->light);
        ese_color_destroy(style->light_hover);
        ese_color_destroy(style->light_active);
        ese_color_destroy(style->dark);
        ese_color_destroy(style->dark_hover);
        ese_color_destroy(style->dark_active);

        ese_color_destroy(style->alert_success_bg);
        ese_color_destroy(style->alert_success_text);
        ese_color_destroy(style->alert_success_border);
        ese_color_destroy(style->alert_info_bg);
        ese_color_destroy(style->alert_info_text);
        ese_color_destroy(style->alert_info_border);
        ese_color_destroy(style->alert_warning_bg);
        ese_color_destroy(style->alert_warning_text);
        ese_color_destroy(style->alert_warning_border);
        ese_color_destroy(style->alert_danger_bg);
        ese_color_destroy(style->alert_danger_text);
        ese_color_destroy(style->alert_danger_border);

        ese_color_destroy(style->bg_primary);
        ese_color_destroy(style->bg_secondary);
        ese_color_destroy(style->bg_success);
        ese_color_destroy(style->bg_info);
        ese_color_destroy(style->bg_warning);
        ese_color_destroy(style->bg_danger);
        ese_color_destroy(style->bg_light);
        ese_color_destroy(style->bg_dark);
        ese_color_destroy(style->bg_white);
        ese_color_destroy(style->bg_transparent);

        ese_color_destroy(style->text_primary);
        ese_color_destroy(style->text_secondary);
        ese_color_destroy(style->text_success);
        ese_color_destroy(style->text_info);
        ese_color_destroy(style->text_warning);
        ese_color_destroy(style->text_danger);
        ese_color_destroy(style->text_light);
        ese_color_destroy(style->text_dark);
        ese_color_destroy(style->text_body);
        ese_color_destroy(style->text_muted);
        ese_color_destroy(style->text_white);
        ese_color_destroy(style->text_black);
        ese_color_destroy(style->text_reset);

        ese_color_destroy(style->border_primary);
        ese_color_destroy(style->border_secondary);
        ese_color_destroy(style->border_success);
        ese_color_destroy(style->border_info);
        ese_color_destroy(style->border_warning);
        ese_color_destroy(style->border_danger);
        ese_color_destroy(style->border_light);
        ese_color_destroy(style->border_dark);
        ese_color_destroy(style->border_white);
        ese_color_destroy(style->border_gray_200);
        ese_color_destroy(style->border_gray_300);

        ese_color_destroy(style->tooltip_bg);
        ese_color_destroy(style->tooltip_color);
        ese_color_destroy(style->selection_bg);
        ese_color_destroy(style->selection_color);
        ese_color_destroy(style->focus_ring);
        ese_color_destroy(style->highlight);
        ese_color_destroy(style->list_group_action);

        memory_manager.free(style);
    } else {
        printf("ese_gui_style_destroy: Lua references, not safe to free immediately\n");
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
        ese_gui_style_unref(style);

        ese_color_unref(style->primary);
        ese_color_unref(style->primary_hover);
        ese_color_unref(style->primary_active);
        ese_color_unref(style->secondary);
        ese_color_unref(style->secondary_hover);
        ese_color_unref(style->secondary_active);
        ese_color_unref(style->success);
        ese_color_unref(style->success_hover);
        ese_color_unref(style->success_active);
        ese_color_unref(style->info);
        ese_color_unref(style->info_hover);
        ese_color_unref(style->info_active);
        ese_color_unref(style->warning);
        ese_color_unref(style->warning_hover);
        ese_color_unref(style->warning_active);
        ese_color_unref(style->danger);
        ese_color_unref(style->danger_hover);
        ese_color_unref(style->danger_active);
        ese_color_unref(style->light);
        ese_color_unref(style->light_hover);
        ese_color_unref(style->light_active);
        ese_color_unref(style->dark);
        ese_color_unref(style->dark_hover);
        ese_color_unref(style->dark_active);
        ese_color_unref(style->alert_success_bg);
        ese_color_unref(style->alert_success_text);
        ese_color_unref(style->alert_success_border);
        ese_color_unref(style->alert_info_bg);
        ese_color_unref(style->alert_info_text);
        ese_color_unref(style->alert_info_border);
        ese_color_unref(style->alert_warning_bg);
        ese_color_unref(style->alert_warning_text);
        ese_color_unref(style->alert_warning_border);
        ese_color_unref(style->alert_danger_bg);
        ese_color_unref(style->alert_danger_text);
        ese_color_unref(style->alert_danger_border);
        ese_color_unref(style->bg_primary);
        ese_color_unref(style->bg_secondary);
        ese_color_unref(style->bg_success);
        ese_color_unref(style->bg_info);
        ese_color_unref(style->bg_warning);
        ese_color_unref(style->bg_danger);
        ese_color_unref(style->bg_light);
        ese_color_unref(style->bg_dark);
        ese_color_unref(style->bg_white);
        ese_color_unref(style->bg_transparent);
        ese_color_unref(style->text_primary);
        ese_color_unref(style->text_secondary);
        ese_color_unref(style->text_success);
        ese_color_unref(style->text_info);
        ese_color_unref(style->text_warning);
        ese_color_unref(style->text_danger);
        ese_color_unref(style->text_light);
        ese_color_unref(style->text_dark);
        ese_color_unref(style->text_body);
        ese_color_unref(style->text_muted);
        ese_color_unref(style->text_white);
        ese_color_unref(style->text_reset);

        ese_color_unref(style->border_primary);
        ese_color_unref(style->border_secondary);
        ese_color_unref(style->border_success);
        ese_color_unref(style->border_info);
        ese_color_unref(style->border_warning);
        ese_color_unref(style->border_danger);
        ese_color_unref(style->border_light);
        ese_color_unref(style->border_dark);
        ese_color_unref(style->border_white);
        ese_color_unref(style->border_gray_200);
        ese_color_unref(style->border_gray_300);

        ese_color_unref(style->tooltip_bg);
        ese_color_unref(style->tooltip_color);
        ese_color_unref(style->selection_bg);
        ese_color_unref(style->selection_color);
        ese_color_unref(style->focus_ring);
        ese_color_unref(style->highlight);
        ese_color_unref(style->list_group_action);
    }
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

// Generic color setter/getter generator
static void _ese_gui_style_assign_color(EseColor *dst, const EseColor *src) {
    if (!src || !dst) {
        return;
    }
    ese_color_set_r(dst, ese_color_get_r(src));
    ese_color_set_g(dst, ese_color_get_g(src));
    ese_color_set_b(dst, ese_color_get_b(src));
    ese_color_set_a(dst, ese_color_get_a(src));
}

// Theme/context colors
void ese_gui_style_set_primary(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->primary, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_primary(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->primary; }
void ese_gui_style_set_primary_hover(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->primary_hover, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_primary_hover(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->primary_hover; }
void ese_gui_style_set_primary_active(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->primary_active, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_primary_active(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->primary_active; }
void ese_gui_style_set_secondary(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->secondary, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_secondary(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->secondary; }
void ese_gui_style_set_secondary_hover(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->secondary_hover, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_secondary_hover(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->secondary_hover; }
void ese_gui_style_set_secondary_active(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->secondary_active, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_secondary_active(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->secondary_active; }
void ese_gui_style_set_success(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->success, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_success(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->success; }
void ese_gui_style_set_success_hover(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->success_hover, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_success_hover(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->success_hover; }
void ese_gui_style_set_success_active(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->success_active, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_success_active(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->success_active; }
void ese_gui_style_set_info(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->info, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_info(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->info; }
void ese_gui_style_set_info_hover(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->info_hover, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_info_hover(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->info_hover; }
void ese_gui_style_set_info_active(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->info_active, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_info_active(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->info_active; }
void ese_gui_style_set_warning(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->warning, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_warning(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->warning; }
void ese_gui_style_set_warning_hover(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->warning_hover, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_warning_hover(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->warning_hover; }
void ese_gui_style_set_warning_active(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->warning_active, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_warning_active(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->warning_active; }
void ese_gui_style_set_danger(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->danger, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_danger(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->danger; }
void ese_gui_style_set_danger_hover(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->danger_hover, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_danger_hover(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->danger_hover; }
void ese_gui_style_set_danger_active(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->danger_active, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_danger_active(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->danger_active; }
void ese_gui_style_set_light(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->light, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_light(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->light; }
void ese_gui_style_set_light_hover(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->light_hover, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_light_hover(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->light_hover; }
void ese_gui_style_set_light_active(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->light_active, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_light_active(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->light_active; }
void ese_gui_style_set_dark(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->dark, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_dark(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->dark; }
void ese_gui_style_set_dark_hover(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->dark_hover, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_dark_hover(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->dark_hover; }
void ese_gui_style_set_dark_active(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->dark_active, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_dark_active(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->dark_active; }

// (explicit setters/getters defined above)

// Alerts
void ese_gui_style_set_alert_success_bg(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->alert_success_bg, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_alert_success_bg(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->alert_success_bg; }
void ese_gui_style_set_alert_success_text(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->alert_success_text, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_alert_success_text(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->alert_success_text; }
void ese_gui_style_set_alert_success_border(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->alert_success_border, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_alert_success_border(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->alert_success_border; }
void ese_gui_style_set_alert_info_bg(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->alert_info_bg, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_alert_info_bg(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->alert_info_bg; }
void ese_gui_style_set_alert_info_text(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->alert_info_text, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_alert_info_text(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->alert_info_text; }
void ese_gui_style_set_alert_info_border(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->alert_info_border, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_alert_info_border(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->alert_info_border; }
void ese_gui_style_set_alert_warning_bg(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->alert_warning_bg, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_alert_warning_bg(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->alert_warning_bg; }
void ese_gui_style_set_alert_warning_text(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->alert_warning_text, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_alert_warning_text(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->alert_warning_text; }
void ese_gui_style_set_alert_warning_border(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->alert_warning_border, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_alert_warning_border(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->alert_warning_border; }
void ese_gui_style_set_alert_danger_bg(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->alert_danger_bg, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_alert_danger_bg(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->alert_danger_bg; }
void ese_gui_style_set_alert_danger_text(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->alert_danger_text, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_alert_danger_text(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->alert_danger_text; }
void ese_gui_style_set_alert_danger_border(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->alert_danger_border, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_alert_danger_border(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->alert_danger_border; }

// Backgrounds
void ese_gui_style_set_bg_primary(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->bg_primary, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_bg_primary(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->bg_primary; }
void ese_gui_style_set_bg_secondary(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->bg_secondary, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_bg_secondary(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->bg_secondary; }
void ese_gui_style_set_bg_success(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->bg_success, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_bg_success(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->bg_success; }
void ese_gui_style_set_bg_info(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->bg_info, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_bg_info(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->bg_info; }
void ese_gui_style_set_bg_warning(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->bg_warning, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_bg_warning(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->bg_warning; }
void ese_gui_style_set_bg_danger(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->bg_danger, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_bg_danger(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->bg_danger; }
void ese_gui_style_set_bg_light(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->bg_light, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_bg_light(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->bg_light; }
void ese_gui_style_set_bg_dark(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->bg_dark, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_bg_dark(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->bg_dark; }
void ese_gui_style_set_bg_white(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->bg_white, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_bg_white(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->bg_white; }
void ese_gui_style_set_bg_transparent(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->bg_transparent, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_bg_transparent(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->bg_transparent; }

// Text colors
void ese_gui_style_set_text_primary(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->text_primary, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_text_primary(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->text_primary; }
void ese_gui_style_set_text_secondary(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->text_secondary, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_text_secondary(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->text_secondary; }
void ese_gui_style_set_text_success(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->text_success, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_text_success(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->text_success; }
void ese_gui_style_set_text_info(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->text_info, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_text_info(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->text_info; }
void ese_gui_style_set_text_warning(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->text_warning, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_text_warning(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->text_warning; }
void ese_gui_style_set_text_danger(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->text_danger, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_text_danger(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->text_danger; }
void ese_gui_style_set_text_light(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->text_light, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_text_light(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->text_light; }
void ese_gui_style_set_text_dark(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->text_dark, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_text_dark(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->text_dark; }
void ese_gui_style_set_text_body(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->text_body, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_text_body(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->text_body; }
void ese_gui_style_set_text_muted(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->text_muted, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_text_muted(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->text_muted; }
void ese_gui_style_set_text_white(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->text_white, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_text_white(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->text_white; }
void ese_gui_style_set_text_black(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->text_black, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_text_black(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->text_black; }
void ese_gui_style_set_text_reset(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->text_reset, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_text_reset(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->text_reset; }

// Borders
void ese_gui_style_set_border_primary(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->border_primary, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_border_primary(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->border_primary; }
void ese_gui_style_set_border_secondary(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->border_secondary, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_border_secondary(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->border_secondary; }
void ese_gui_style_set_border_success(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->border_success, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_border_success(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->border_success; }
void ese_gui_style_set_border_info(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->border_info, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_border_info(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->border_info; }
void ese_gui_style_set_border_warning(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->border_warning, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_border_warning(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->border_warning; }
void ese_gui_style_set_border_danger(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->border_danger, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_border_danger(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->border_danger; }
void ese_gui_style_set_border_light(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->border_light, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_border_light(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->border_light; }
void ese_gui_style_set_border_dark(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->border_dark, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_border_dark(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->border_dark; }
void ese_gui_style_set_border_white(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->border_white, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_border_white(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->border_white; }
void ese_gui_style_set_border_gray_200(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->border_gray_200, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_border_gray_200(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->border_gray_200; }
void ese_gui_style_set_border_gray_300(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->border_gray_300, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_border_gray_300(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->border_gray_300; }

// Tooltips
void ese_gui_style_set_tooltip_bg(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->tooltip_bg, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_tooltip_bg(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->tooltip_bg; }
void ese_gui_style_set_tooltip_color(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->tooltip_color, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_tooltip_color(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->tooltip_color; }

// Misc
void ese_gui_style_set_selection_bg(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->selection_bg, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_selection_bg(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->selection_bg; }
void ese_gui_style_set_selection_color(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->selection_color, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_selection_color(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->selection_color; }
void ese_gui_style_set_focus_ring(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->focus_ring, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_focus_ring(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->focus_ring; }
void ese_gui_style_set_highlight(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->highlight, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_highlight(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->highlight; }
void ese_gui_style_set_list_group_action(EseGuiStyle *style, const EseColor *color) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); _ese_gui_style_assign_color(style->list_group_action, color); _ese_gui_style_notify_watchers(style); }
EseColor *ese_gui_style_get_list_group_action(const EseGuiStyle *style) { log_assert("GUI_STYLE", style != NULL, "NULL style parameter"); return style->list_group_action; }

// end of explicit setters/getters

// (no deprecated wrappers)

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

void ese_gui_style_set_font_size(EseGuiStyle *style, int font_size) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    style->font_size = font_size;
    _ese_gui_style_notify_watchers(style);
}

int ese_gui_style_get_font_size(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->font_size;
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
            style->watchers, sizeof(EseGuiStyleWatcherCallback) * new_capacity, MMTAG_GUI_STYLE);
        if (!new_watchers) return false;
        
        void **new_userdata = (void **)memory_manager.realloc(
            style->watcher_userdata, sizeof(void *) * new_capacity, MMTAG_GUI_STYLE);
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
    
    // First try exact match (callback + userdata)
    for (size_t i = 0; i < style->watcher_count; i++) {
        if (style->watchers[i] == callback && style->watcher_userdata[i] == userdata) {
            for (size_t j = i; j < style->watcher_count - 1; j++) {
                style->watchers[j] = style->watchers[j + 1];
                style->watcher_userdata[j] = style->watcher_userdata[j + 1];
            }
            style->watcher_count--;
            return true;
        }
    }
    // Fallback: remove by callback only if exact match not found
    for (size_t i = 0; i < style->watcher_count; i++) {
        if (style->watchers[i] == callback) {
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
    cJSON_AddNumberToObject(json, "font_size", (double)style->font_size);
    
    // Color serialization is intentionally omitted for the expanded palette here.
    
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
    
    cJSON *font_size = cJSON_GetObjectItemCaseSensitive(data, "font_size");
    if (cJSON_IsNumber(font_size)) {
        style->font_size = font_size->valueint;
    }
    
    // Color deserialization omitted for expanded palette
    
    return style;
}
