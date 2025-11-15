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
#include "scripting/lua_engine.h"
#include "types/color.h"
#include "types/gui_style.h"
#include "types/gui_style_lua.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"

#define SET_COLOR(dst, src) do { \
    ese_color_set_r((dst), ese_color_get_r((src))); \
    ese_color_set_g((dst), ese_color_get_g((src))); \
    ese_color_set_b((dst), ese_color_get_b((src))); \
    ese_color_set_a((dst), ese_color_get_a((src))); \
} while (0)

// The actual EseGuiStyle struct definition (private to this file)
typedef struct EseGuiStyle {

    // THEME/CONTEXT COLORS
    EseColor *variant[GUI_STYLE_VARIANT_MAX];
    EseColor *variant_hover[GUI_STYLE_VARIANT_MAX];
    EseColor *variant_active[GUI_STYLE_VARIANT_MAX];

    // ALERTS
    EseColor *alert_bg_variant[GUI_STYLE_VARIANT_MAX];
    EseColor *alert_text_variant[GUI_STYLE_VARIANT_MAX];
    EseColor *alert_border_variant[GUI_STYLE_VARIANT_MAX];

    // BACKGROUNDS
    EseColor *bg_variant[GUI_STYLE_VARIANT_MAX];
    EseColor *bg_hover_variant[GUI_STYLE_VARIANT_MAX];
    EseColor *bg_active_variant[GUI_STYLE_VARIANT_MAX];

    // TEXT COLORS
    EseColor *text_variant[GUI_STYLE_VARIANT_MAX];
    EseColor *text_hover_variant[GUI_STYLE_VARIANT_MAX];
    EseColor *text_active_variant[GUI_STYLE_VARIANT_MAX];

    // BORDERS
    EseColor *border_variant[GUI_STYLE_VARIANT_MAX];
    EseColor *border_hover_variant[GUI_STYLE_VARIANT_MAX];
    EseColor *border_active_variant[GUI_STYLE_VARIANT_MAX];

    // TOOLTIPS
    EseColor *tooltip_bg_variant[GUI_STYLE_VARIANT_MAX];
    EseColor *tooltip_color_variant[GUI_STYLE_VARIANT_MAX];

    // MISCELLANEOUS UTILITIES
    EseColor *selection_bg_variant[GUI_STYLE_VARIANT_MAX];
    EseColor *selection_color_variant[GUI_STYLE_VARIANT_MAX];
    EseColor *focus_ring_variant[GUI_STYLE_VARIANT_MAX];
    EseColor *highlight_variant[GUI_STYLE_VARIANT_MAX];

    int border_width;
    int font_size;
    int padding_left;
    int padding_top;
    int padding_right;
    int padding_bottom;

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
    
    EseGuiStyle *style = (EseGuiStyle *)memory_manager.malloc(sizeof(EseGuiStyle), MMTAG_GUI_STYLE);
    
    style->border_width = GUI_STYLE_BORDER_WIDTH_WIDGET_DEFAULT;
    style->padding_left = 4;
    style->padding_top = 4;
    style->padding_right = 4;
    style->padding_bottom = 4;
    style->font_size = GUI_STYLE_FONT_SIZE_WIDGET_DEFAULT;
    
    // Create colors
    for (size_t i = 0; i < GUI_STYLE_VARIANT_MAX; i++) {
        style->variant[i] = ese_color_create(engine);
        style->variant_hover[i] = ese_color_create(engine);
        style->variant_active[i] = ese_color_create(engine);

        style->alert_bg_variant[i] = ese_color_create(engine);
        style->alert_text_variant[i] = ese_color_create(engine);
        style->alert_border_variant[i] = ese_color_create(engine);

        style->bg_variant[i] = ese_color_create(engine);
        style->bg_hover_variant[i] = ese_color_create(engine);
        style->bg_active_variant[i] = ese_color_create(engine);

        style->text_variant[i] = ese_color_create(engine);
        style->text_hover_variant[i] = ese_color_create(engine);
        style->text_active_variant[i] = ese_color_create(engine);

        style->border_variant[i] = ese_color_create(engine);
        style->border_hover_variant[i] = ese_color_create(engine);
        style->border_active_variant[i] = ese_color_create(engine);

        style->tooltip_bg_variant[i] = ese_color_create(engine);
        style->tooltip_color_variant[i] = ese_color_create(engine);

        style->selection_bg_variant[i] = ese_color_create(engine);
        style->selection_color_variant[i] = ese_color_create(engine);
        style->focus_ring_variant[i] = ese_color_create(engine);
        style->highlight_variant[i] = ese_color_create(engine);
    }

    // Set colors
    ese_color_set_hex(style->variant[GUI_STYLE_VARIANT_PRIMARY], "#0d6efd");
    ese_color_set_hex(style->variant_hover[GUI_STYLE_VARIANT_PRIMARY], "#0b5ed7");
    ese_color_set_hex(style->variant_active[GUI_STYLE_VARIANT_PRIMARY], "#0a58ca");
    ese_color_set_hex(style->variant[GUI_STYLE_VARIANT_SECONDARY], "#6c757d");
    ese_color_set_hex(style->variant_hover[GUI_STYLE_VARIANT_SECONDARY], "#5c636a");
    ese_color_set_hex(style->variant_active[GUI_STYLE_VARIANT_SECONDARY], "#565e64");
    ese_color_set_hex(style->alert_bg_variant[GUI_STYLE_VARIANT_SUCCESS], "#d1e7dd");
    ese_color_set_hex(style->alert_text_variant[GUI_STYLE_VARIANT_SUCCESS], "#0f5132");
    ese_color_set_hex(style->alert_border_variant[GUI_STYLE_VARIANT_SUCCESS], "#badbcc");
    ese_color_set_hex(style->variant[GUI_STYLE_VARIANT_INFO], "#0dcaf0");
    ese_color_set_hex(style->variant_hover[GUI_STYLE_VARIANT_INFO], "#31d2f2");
    ese_color_set_hex(style->variant_active[GUI_STYLE_VARIANT_INFO], "#3dd5f3");
    ese_color_set_hex(style->variant[GUI_STYLE_VARIANT_WARNING], "#ffc107");
    ese_color_set_hex(style->variant_hover[GUI_STYLE_VARIANT_WARNING], "#ffcd39");
    ese_color_set_hex(style->variant_active[GUI_STYLE_VARIANT_WARNING], "#ffdb6d");
    ese_color_set_hex(style->variant[GUI_STYLE_VARIANT_DANGER], "#dc3545");
    ese_color_set_hex(style->variant_hover[GUI_STYLE_VARIANT_DANGER], "#bb2d3b");
    ese_color_set_hex(style->variant_active[GUI_STYLE_VARIANT_DANGER], "#b02a37");
    ese_color_set_hex(style->variant[GUI_STYLE_VARIANT_LIGHT], "#f8f9fa");
    ese_color_set_hex(style->variant_hover[GUI_STYLE_VARIANT_LIGHT], "#f9fafb");
    ese_color_set_hex(style->variant_active[GUI_STYLE_VARIANT_LIGHT], "#f9fafb");
    ese_color_set_hex(style->variant[GUI_STYLE_VARIANT_DARK], "#212529");
    ese_color_set_hex(style->variant_hover[GUI_STYLE_VARIANT_DARK], "#1c1f23");
    ese_color_set_hex(style->variant_active[GUI_STYLE_VARIANT_DARK], "#1a1e21");
    ese_color_set_hex(style->variant[GUI_STYLE_VARIANT_WHITE], "#ffffff");
    ese_color_set_hex(style->variant_hover[GUI_STYLE_VARIANT_WHITE], "#f9fafb");
    ese_color_set_hex(style->variant_active[GUI_STYLE_VARIANT_WHITE], "#f9fafb");
    ese_color_set_hex(style->variant[GUI_STYLE_VARIANT_TRANSPARENT], "#00000000");
    ese_color_set_hex(style->variant_hover[GUI_STYLE_VARIANT_TRANSPARENT], "#00000000");
    ese_color_set_hex(style->variant_active[GUI_STYLE_VARIANT_TRANSPARENT], "#00000000");

    // Alerts
    ese_color_set_hex(style->alert_bg_variant[GUI_STYLE_VARIANT_PRIMARY], "#cfe2ff");
    ese_color_set_hex(style->alert_text_variant[GUI_STYLE_VARIANT_PRIMARY], "#084298");
    ese_color_set_hex(style->alert_border_variant[GUI_STYLE_VARIANT_PRIMARY], "#b6d4fe");
    ese_color_set_hex(style->alert_bg_variant[GUI_STYLE_VARIANT_SECONDARY], "#e2e3e5");
    ese_color_set_hex(style->alert_text_variant[GUI_STYLE_VARIANT_SECONDARY], "#41464b");
    ese_color_set_hex(style->alert_border_variant[GUI_STYLE_VARIANT_SECONDARY], "#d3d6d8");
    ese_color_set_hex(style->alert_bg_variant[GUI_STYLE_VARIANT_SUCCESS], "#d1e7dd");
    ese_color_set_hex(style->alert_text_variant[GUI_STYLE_VARIANT_SUCCESS], "#0f5132");
    ese_color_set_hex(style->alert_border_variant[GUI_STYLE_VARIANT_SUCCESS], "#badbcc");
    ese_color_set_hex(style->alert_bg_variant[GUI_STYLE_VARIANT_INFO], "#cff4fc");
    ese_color_set_hex(style->alert_text_variant[GUI_STYLE_VARIANT_INFO], "#055160");
    ese_color_set_hex(style->alert_border_variant[GUI_STYLE_VARIANT_INFO], "#b6effb");
    ese_color_set_hex(style->alert_bg_variant[GUI_STYLE_VARIANT_WARNING], "#fff3cd");
    ese_color_set_hex(style->alert_text_variant[GUI_STYLE_VARIANT_WARNING], "#664d03");
    ese_color_set_hex(style->alert_border_variant[GUI_STYLE_VARIANT_WARNING], "#ffecb5");
    ese_color_set_hex(style->alert_bg_variant[GUI_STYLE_VARIANT_DANGER], "#f8d7da");
    ese_color_set_hex(style->alert_text_variant[GUI_STYLE_VARIANT_DANGER], "#842029");
    ese_color_set_hex(style->alert_border_variant[GUI_STYLE_VARIANT_DANGER], "#f5c2c7");
    ese_color_set_hex(style->alert_bg_variant[GUI_STYLE_VARIANT_LIGHT], "#fefefe");
    ese_color_set_hex(style->alert_text_variant[GUI_STYLE_VARIANT_LIGHT], "#636464");
    ese_color_set_hex(style->alert_border_variant[GUI_STYLE_VARIANT_LIGHT], "#fdfdfe");
    ese_color_set_hex(style->alert_bg_variant[GUI_STYLE_VARIANT_DARK], "#d3d3d4");
    ese_color_set_hex(style->alert_text_variant[GUI_STYLE_VARIANT_DARK], "#141619");
    ese_color_set_hex(style->alert_border_variant[GUI_STYLE_VARIANT_DARK], "#bcbebf");
    ese_color_set_hex(style->alert_bg_variant[GUI_STYLE_VARIANT_WHITE], "#fcfcfd");
    ese_color_set_hex(style->alert_text_variant[GUI_STYLE_VARIANT_WHITE], "#ffffff");
    ese_color_set_hex(style->alert_border_variant[GUI_STYLE_VARIANT_WHITE], "#fdfdfe");
    ese_color_set_hex(style->alert_bg_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#00000000");
    ese_color_set_hex(style->alert_text_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#00000000");
    ese_color_set_hex(style->alert_border_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#00000000");

    // Backgrounds
    ese_color_set_hex(style->bg_variant[GUI_STYLE_VARIANT_PRIMARY], "#0d6efd");
    ese_color_set_hex(style->bg_hover_variant[GUI_STYLE_VARIANT_PRIMARY], "#0b5ed7");
    ese_color_set_hex(style->bg_active_variant[GUI_STYLE_VARIANT_PRIMARY], "#0a58ca");
    ese_color_set_hex(style->bg_variant[GUI_STYLE_VARIANT_SECONDARY], "#6c757d");
    ese_color_set_hex(style->bg_hover_variant[GUI_STYLE_VARIANT_SECONDARY], "#5c636a");
    ese_color_set_hex(style->bg_active_variant[GUI_STYLE_VARIANT_SECONDARY], "#565e64");
    ese_color_set_hex(style->bg_variant[GUI_STYLE_VARIANT_SUCCESS], "#198754");
    ese_color_set_hex(style->bg_hover_variant[GUI_STYLE_VARIANT_SUCCESS], "#157347");
    ese_color_set_hex(style->bg_active_variant[GUI_STYLE_VARIANT_SUCCESS], "#146c43");
    ese_color_set_hex(style->bg_variant[GUI_STYLE_VARIANT_INFO], "#0dcaf0");
    ese_color_set_hex(style->bg_hover_variant[GUI_STYLE_VARIANT_INFO], "#31d2f2");
    ese_color_set_hex(style->bg_active_variant[GUI_STYLE_VARIANT_INFO], "#3dd5f3");
    ese_color_set_hex(style->bg_variant[GUI_STYLE_VARIANT_WARNING], "#ffc107");
    ese_color_set_hex(style->bg_hover_variant[GUI_STYLE_VARIANT_WARNING], "#ffcd39");
    ese_color_set_hex(style->bg_active_variant[GUI_STYLE_VARIANT_WARNING], "#ffdb6d");
    ese_color_set_hex(style->bg_variant[GUI_STYLE_VARIANT_DANGER], "#dc3545");
    ese_color_set_hex(style->bg_hover_variant[GUI_STYLE_VARIANT_DANGER], "#bb2d3b");
    ese_color_set_hex(style->bg_active_variant[GUI_STYLE_VARIANT_DANGER], "#b02a37");
    ese_color_set_hex(style->bg_variant[GUI_STYLE_VARIANT_LIGHT], "#f8f9fa");
    ese_color_set_hex(style->bg_hover_variant[GUI_STYLE_VARIANT_LIGHT], "#f9fafb");
    ese_color_set_hex(style->bg_active_variant[GUI_STYLE_VARIANT_LIGHT], "#f9fafb");
    ese_color_set_hex(style->bg_variant[GUI_STYLE_VARIANT_DARK], "#212529");
    ese_color_set_hex(style->bg_hover_variant[GUI_STYLE_VARIANT_DARK], "#1c1f23");
    ese_color_set_hex(style->bg_active_variant[GUI_STYLE_VARIANT_DARK], "#1a1e21");
    ese_color_set_hex(style->bg_variant[GUI_STYLE_VARIANT_WHITE], "#ffffff");
    ese_color_set_hex(style->bg_hover_variant[GUI_STYLE_VARIANT_WHITE], "#f9fafb");
    ese_color_set_hex(style->bg_active_variant[GUI_STYLE_VARIANT_WHITE], "#f9fafb");
    ese_color_set_hex(style->bg_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#00000000");
    ese_color_set_hex(style->bg_hover_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#00000000");
    ese_color_set_hex(style->bg_active_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#00000000");

    // Text colors
    ese_color_set_hex(style->text_variant[GUI_STYLE_VARIANT_PRIMARY], "#0d6efd");
    ese_color_set_hex(style->text_hover_variant[GUI_STYLE_VARIANT_PRIMARY], "#0a58ca");
    ese_color_set_hex(style->text_active_variant[GUI_STYLE_VARIANT_PRIMARY], "#084298");
    ese_color_set_hex(style->text_variant[GUI_STYLE_VARIANT_SECONDARY], "#6c757d");
    ese_color_set_hex(style->text_hover_variant[GUI_STYLE_VARIANT_SECONDARY], "#565e64");
    ese_color_set_hex(style->text_active_variant[GUI_STYLE_VARIANT_SECONDARY], "#343a40");
    ese_color_set_hex(style->text_variant[GUI_STYLE_VARIANT_SUCCESS], "#198754");
    ese_color_set_hex(style->text_hover_variant[GUI_STYLE_VARIANT_SUCCESS], "#157347");
    ese_color_set_hex(style->text_active_variant[GUI_STYLE_VARIANT_SUCCESS], "#0f5132");
    ese_color_set_hex(style->text_variant[GUI_STYLE_VARIANT_INFO], "#0dcaf0");
    ese_color_set_hex(style->text_hover_variant[GUI_STYLE_VARIANT_INFO], "#055160");
    ese_color_set_hex(style->text_active_variant[GUI_STYLE_VARIANT_INFO], "#0a58ca");
    ese_color_set_hex(style->text_variant[GUI_STYLE_VARIANT_WARNING], "#ffc107");
    ese_color_set_hex(style->text_hover_variant[GUI_STYLE_VARIANT_WARNING], "#ffcd39");
    ese_color_set_hex(style->text_active_variant[GUI_STYLE_VARIANT_WARNING], "#664d03");
    ese_color_set_hex(style->text_variant[GUI_STYLE_VARIANT_DANGER], "#dc3545");
    ese_color_set_hex(style->text_hover_variant[GUI_STYLE_VARIANT_DANGER], "#b02a37");
    ese_color_set_hex(style->text_active_variant[GUI_STYLE_VARIANT_DANGER], "#842029");
    ese_color_set_hex(style->text_variant[GUI_STYLE_VARIANT_LIGHT], "#f8f9fa");
    ese_color_set_hex(style->text_hover_variant[GUI_STYLE_VARIANT_LIGHT], "#343a40");
    ese_color_set_hex(style->text_active_variant[GUI_STYLE_VARIANT_LIGHT], "#495057");
    ese_color_set_hex(style->text_variant[GUI_STYLE_VARIANT_DARK], "#212529");
    ese_color_set_hex(style->text_hover_variant[GUI_STYLE_VARIANT_DARK], "#18191a");
    ese_color_set_hex(style->text_active_variant[GUI_STYLE_VARIANT_DARK], "#0f0f10");
    ese_color_set_hex(style->text_variant[GUI_STYLE_VARIANT_WHITE], "#ffffff");
    ese_color_set_hex(style->text_hover_variant[GUI_STYLE_VARIANT_WHITE], "#e9ecef");
    ese_color_set_hex(style->text_active_variant[GUI_STYLE_VARIANT_WHITE], "#dee2e6");
    ese_color_set_hex(style->text_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#000000");
    ese_color_set_hex(style->text_hover_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#000000");
    ese_color_set_hex(style->text_active_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#000000");

    // Borders
    ese_color_set_hex(style->border_variant[GUI_STYLE_VARIANT_PRIMARY], "#0d6efd");
    ese_color_set_hex(style->border_hover_variant[GUI_STYLE_VARIANT_PRIMARY], "#0a58ca");
    ese_color_set_hex(style->border_active_variant[GUI_STYLE_VARIANT_PRIMARY], "#084298");
    ese_color_set_hex(style->border_variant[GUI_STYLE_VARIANT_SECONDARY], "#6c757d");
    ese_color_set_hex(style->border_hover_variant[GUI_STYLE_VARIANT_SECONDARY], "#565e64");
    ese_color_set_hex(style->border_active_variant[GUI_STYLE_VARIANT_SECONDARY], "#343a40");
    ese_color_set_hex(style->border_variant[GUI_STYLE_VARIANT_SUCCESS], "#198754");
    ese_color_set_hex(style->border_hover_variant[GUI_STYLE_VARIANT_SUCCESS], "#0f5132");
    ese_color_set_hex(style->border_active_variant[GUI_STYLE_VARIANT_SUCCESS], "#14532d");
    ese_color_set_hex(style->border_variant[GUI_STYLE_VARIANT_INFO], "#0dcaf0");
    ese_color_set_hex(style->border_hover_variant[GUI_STYLE_VARIANT_INFO], "#055160");
    ese_color_set_hex(style->border_active_variant[GUI_STYLE_VARIANT_INFO], "#31d2f2");
    ese_color_set_hex(style->border_variant[GUI_STYLE_VARIANT_WARNING], "#ffc107");
    ese_color_set_hex(style->border_hover_variant[GUI_STYLE_VARIANT_WARNING], "#ffcd39");
    ese_color_set_hex(style->border_active_variant[GUI_STYLE_VARIANT_WARNING], "#664d03");
    ese_color_set_hex(style->border_variant[GUI_STYLE_VARIANT_DANGER], "#dc3545");
    ese_color_set_hex(style->border_hover_variant[GUI_STYLE_VARIANT_DANGER], "#b02a37");
    ese_color_set_hex(style->border_active_variant[GUI_STYLE_VARIANT_DANGER], "#842029");
    ese_color_set_hex(style->border_variant[GUI_STYLE_VARIANT_LIGHT], "#f8f9fa");
    ese_color_set_hex(style->border_hover_variant[GUI_STYLE_VARIANT_LIGHT], "#dee2e6");
    ese_color_set_hex(style->border_active_variant[GUI_STYLE_VARIANT_LIGHT], "#ced4da");
    ese_color_set_hex(style->border_variant[GUI_STYLE_VARIANT_DARK], "#212529");
    ese_color_set_hex(style->border_hover_variant[GUI_STYLE_VARIANT_DARK], "#18191a");
    ese_color_set_hex(style->border_active_variant[GUI_STYLE_VARIANT_DARK], "#0f0f10");
    ese_color_set_hex(style->border_variant[GUI_STYLE_VARIANT_WHITE], "#ffffff");
    ese_color_set_hex(style->border_hover_variant[GUI_STYLE_VARIANT_WHITE], "#e9ecef");
    ese_color_set_hex(style->border_active_variant[GUI_STYLE_VARIANT_WHITE], "#dee2e6");
    ese_color_set_hex(style->border_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#00000000");
    ese_color_set_hex(style->border_hover_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#00000000");
    ese_color_set_hex(style->border_active_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#00000000");

    // Tooltips
    ese_color_set_hex(style->tooltip_bg_variant[GUI_STYLE_VARIANT_PRIMARY], "#0d6efd");
    ese_color_set_hex(style->tooltip_color_variant[GUI_STYLE_VARIANT_PRIMARY], "#ffffff");
    ese_color_set_hex(style->tooltip_bg_variant[GUI_STYLE_VARIANT_SECONDARY], "#6c757d");
    ese_color_set_hex(style->tooltip_color_variant[GUI_STYLE_VARIANT_SECONDARY], "#ffffff");
    ese_color_set_hex(style->tooltip_bg_variant[GUI_STYLE_VARIANT_SUCCESS], "#198754");
    ese_color_set_hex(style->tooltip_color_variant[GUI_STYLE_VARIANT_SUCCESS], "#ffffff");
    ese_color_set_hex(style->tooltip_bg_variant[GUI_STYLE_VARIANT_INFO], "#0dcaf0");
    ese_color_set_hex(style->tooltip_color_variant[GUI_STYLE_VARIANT_INFO], "#000000");
    ese_color_set_hex(style->tooltip_bg_variant[GUI_STYLE_VARIANT_WARNING], "#ffc107");
    ese_color_set_hex(style->tooltip_color_variant[GUI_STYLE_VARIANT_WARNING], "#000000");
    ese_color_set_hex(style->tooltip_bg_variant[GUI_STYLE_VARIANT_DANGER], "#dc3545");
    ese_color_set_hex(style->tooltip_color_variant[GUI_STYLE_VARIANT_DANGER], "#ffffff");
    ese_color_set_hex(style->tooltip_bg_variant[GUI_STYLE_VARIANT_LIGHT], "#f8f9fa");
    ese_color_set_hex(style->tooltip_color_variant[GUI_STYLE_VARIANT_LIGHT], "#000000");
    ese_color_set_hex(style->tooltip_bg_variant[GUI_STYLE_VARIANT_DARK], "#212529");
    ese_color_set_hex(style->tooltip_color_variant[GUI_STYLE_VARIANT_DARK], "#ffffff");
    ese_color_set_hex(style->tooltip_bg_variant[GUI_STYLE_VARIANT_WHITE], "#ffffff");
    ese_color_set_hex(style->tooltip_color_variant[GUI_STYLE_VARIANT_WHITE], "#000000");
    ese_color_set_hex(style->tooltip_bg_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#00000000");
    ese_color_set_hex(style->tooltip_color_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#000000");

    // Miscellaneous
    ese_color_set_hex(style->selection_bg_variant[GUI_STYLE_VARIANT_PRIMARY], "#b6d4fe");
    ese_color_set_hex(style->selection_color_variant[GUI_STYLE_VARIANT_PRIMARY], "#084298");
    ese_color_set_hex(style->selection_bg_variant[GUI_STYLE_VARIANT_SECONDARY], "#e2e3e5");
    ese_color_set_hex(style->selection_color_variant[GUI_STYLE_VARIANT_SECONDARY], "#41464b");
    ese_color_set_hex(style->selection_bg_variant[GUI_STYLE_VARIANT_SUCCESS], "#badbcc");
    ese_color_set_hex(style->selection_color_variant[GUI_STYLE_VARIANT_SUCCESS], "#0f5132");
    ese_color_set_hex(style->selection_bg_variant[GUI_STYLE_VARIANT_INFO], "#b6effb");
    ese_color_set_hex(style->selection_color_variant[GUI_STYLE_VARIANT_INFO], "#055160");
    ese_color_set_hex(style->selection_bg_variant[GUI_STYLE_VARIANT_WARNING], "#ffecb5");
    ese_color_set_hex(style->selection_color_variant[GUI_STYLE_VARIANT_WARNING], "#664d03");
    ese_color_set_hex(style->selection_bg_variant[GUI_STYLE_VARIANT_DANGER], "#f5c2c7");
    ese_color_set_hex(style->selection_color_variant[GUI_STYLE_VARIANT_DANGER], "#842029");
    ese_color_set_hex(style->selection_bg_variant[GUI_STYLE_VARIANT_LIGHT], "#f8f9fa");
    ese_color_set_hex(style->selection_color_variant[GUI_STYLE_VARIANT_LIGHT], "#636464");
    ese_color_set_hex(style->selection_bg_variant[GUI_STYLE_VARIANT_DARK], "#bcbebf");
    ese_color_set_hex(style->selection_color_variant[GUI_STYLE_VARIANT_DARK], "#141619");
    ese_color_set_hex(style->selection_bg_variant[GUI_STYLE_VARIANT_WHITE], "#ffffff");
    ese_color_set_hex(style->selection_color_variant[GUI_STYLE_VARIANT_WHITE], "#141619");
    ese_color_set_hex(style->selection_bg_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#00000000");
    ese_color_set_hex(style->selection_color_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#00000000");

    // Focus ring
    ese_color_set_hex(style->focus_ring_variant[GUI_STYLE_VARIANT_PRIMARY], "#0d6efd");
    ese_color_set_hex(style->focus_ring_variant[GUI_STYLE_VARIANT_SECONDARY], "#6c757d");
    ese_color_set_hex(style->focus_ring_variant[GUI_STYLE_VARIANT_SUCCESS], "#198754");
    ese_color_set_hex(style->focus_ring_variant[GUI_STYLE_VARIANT_INFO], "#0dcaf0");
    ese_color_set_hex(style->focus_ring_variant[GUI_STYLE_VARIANT_WARNING], "#ffc107");
    ese_color_set_hex(style->focus_ring_variant[GUI_STYLE_VARIANT_DANGER], "#dc3545");
    ese_color_set_hex(style->focus_ring_variant[GUI_STYLE_VARIANT_LIGHT], "#f8f9fa");
    ese_color_set_hex(style->focus_ring_variant[GUI_STYLE_VARIANT_DARK], "#212529");
    ese_color_set_hex(style->focus_ring_variant[GUI_STYLE_VARIANT_WHITE], "#ffffff");
    ese_color_set_hex(style->focus_ring_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#00000000");

    // Highlight
    ese_color_set_hex(style->highlight_variant[GUI_STYLE_VARIANT_PRIMARY], "#b6d4fe");
    ese_color_set_hex(style->highlight_variant[GUI_STYLE_VARIANT_SECONDARY], "#e2e3e5");
    ese_color_set_hex(style->highlight_variant[GUI_STYLE_VARIANT_SUCCESS], "#badbcc");
    ese_color_set_hex(style->highlight_variant[GUI_STYLE_VARIANT_INFO], "#b6effb");
    ese_color_set_hex(style->highlight_variant[GUI_STYLE_VARIANT_WARNING], "#ffecb5");
    ese_color_set_hex(style->highlight_variant[GUI_STYLE_VARIANT_DANGER], "#f5c2c7");
    ese_color_set_hex(style->highlight_variant[GUI_STYLE_VARIANT_LIGHT], "#fdfdfe");
    ese_color_set_hex(style->highlight_variant[GUI_STYLE_VARIANT_DARK], "#bcbebf");
    ese_color_set_hex(style->highlight_variant[GUI_STYLE_VARIANT_WHITE], "#fdfdfe");
    ese_color_set_hex(style->highlight_variant[GUI_STYLE_VARIANT_TRANSPARENT], "#00000000");
    
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
    copy->border_width = source->border_width;
    copy->padding_left = source->padding_left;
    copy->padding_top = source->padding_top;
    copy->padding_right = source->padding_right;
    copy->padding_bottom = source->padding_bottom;
    copy->font_size = source->font_size;
    
    // Copy all colors by reference into pre-created EseColor instances
    for (int i = 0; i < GUI_STYLE_VARIANT_MAX; i++) {
        SET_COLOR(copy->variant[i], source->variant[i]);
        SET_COLOR(copy->variant_hover[i], source->variant_hover[i]);
        SET_COLOR(copy->variant_active[i], source->variant_active[i]);
        SET_COLOR(copy->alert_bg_variant[i], source->alert_bg_variant[i]);
        SET_COLOR(copy->alert_text_variant[i], source->alert_text_variant[i]);
        SET_COLOR(copy->alert_border_variant[i], source->alert_border_variant[i]);
        SET_COLOR(copy->bg_variant[i], source->bg_variant[i]);
        SET_COLOR(copy->bg_hover_variant[i], source->bg_hover_variant[i]);
        SET_COLOR(copy->bg_active_variant[i], source->bg_active_variant[i]);
        SET_COLOR(copy->text_variant[i], source->text_variant[i]);
        SET_COLOR(copy->text_hover_variant[i], source->text_hover_variant[i]);
        SET_COLOR(copy->text_active_variant[i], source->text_active_variant[i]);
        SET_COLOR(copy->border_variant[i], source->border_variant[i]);
        SET_COLOR(copy->border_hover_variant[i], source->border_hover_variant[i]);
        SET_COLOR(copy->border_active_variant[i], source->border_active_variant[i]);
        SET_COLOR(copy->tooltip_bg_variant[i], source->tooltip_bg_variant[i]);
        SET_COLOR(copy->tooltip_color_variant[i], source->tooltip_color_variant[i]);
        SET_COLOR(copy->selection_bg_variant[i], source->selection_bg_variant[i]);
        SET_COLOR(copy->selection_color_variant[i], source->selection_color_variant[i]);
        SET_COLOR(copy->focus_ring_variant[i], source->focus_ring_variant[i]);
        SET_COLOR(copy->highlight_variant[i], source->highlight_variant[i]);
    }
    
    // Copy Lua state
    _ese_gui_style_set_state(copy, source->state);
    
    return copy;
}

void ese_gui_style_destroy(EseGuiStyle *style) {
    if (!style) return;
    
    if (ese_gui_style_get_lua_ref(style) == LUA_NOREF) {
    
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
        for (int i = 0; i < GUI_STYLE_VARIANT_MAX; i++) {
            ese_color_destroy(style->variant[i]);
            ese_color_destroy(style->variant_hover[i]);
            ese_color_destroy(style->variant_active[i]);
            ese_color_destroy(style->alert_bg_variant[i]);
            ese_color_destroy(style->alert_text_variant[i]);
            ese_color_destroy(style->alert_border_variant[i]);
            ese_color_destroy(style->bg_variant[i]);
            ese_color_destroy(style->bg_hover_variant[i]);
            ese_color_destroy(style->bg_active_variant[i]);
            ese_color_destroy(style->text_variant[i]);
            ese_color_destroy(style->text_hover_variant[i]);
            ese_color_destroy(style->text_active_variant[i]);
            ese_color_destroy(style->border_variant[i]);
            ese_color_destroy(style->border_hover_variant[i]);
            ese_color_destroy(style->border_active_variant[i]);
            ese_color_destroy(style->tooltip_bg_variant[i]);
            ese_color_destroy(style->tooltip_color_variant[i]);
            ese_color_destroy(style->selection_bg_variant[i]);
            ese_color_destroy(style->selection_color_variant[i]);
            ese_color_destroy(style->focus_ring_variant[i]);
            ese_color_destroy(style->highlight_variant[i]);
        }

        memory_manager.free(style);
    } else {
        printf("ese_gui_style_destroy: Lua references, not safe to free immediately\n");
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
        ese_gui_style_unref(style);

        // Unreference colors
        for (int i = 0; i < GUI_STYLE_VARIANT_MAX; i++) {
            ese_color_unref(style->variant[i]);
            ese_color_unref(style->variant_hover[i]);
            ese_color_unref(style->variant_active[i]);
            ese_color_unref(style->alert_bg_variant[i]);
            ese_color_unref(style->alert_text_variant[i]);
            ese_color_unref(style->alert_border_variant[i]);
            ese_color_unref(style->bg_variant[i]);
            ese_color_unref(style->bg_hover_variant[i]);
            ese_color_unref(style->bg_active_variant[i]);
            ese_color_unref(style->text_variant[i]);
            ese_color_unref(style->text_hover_variant[i]);
            ese_color_unref(style->text_active_variant[i]);
            ese_color_unref(style->border_variant[i]);
            ese_color_unref(style->border_hover_variant[i]);
            ese_color_unref(style->border_active_variant[i]);
            ese_color_unref(style->tooltip_bg_variant[i]);
            ese_color_unref(style->tooltip_color_variant[i]);
            ese_color_unref(style->selection_bg_variant[i]);
            ese_color_unref(style->selection_color_variant[i]);
            ese_color_unref(style->focus_ring_variant[i]);
            ese_color_unref(style->highlight_variant[i]);
        }
    }
}

size_t ese_gui_style_sizeof(void) {
    return sizeof(EseGuiStyle);
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

// Properties

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

void ese_gui_style_set_font_size(EseGuiStyle *style, int font_size) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    style->font_size = font_size;
    _ese_gui_style_notify_watchers(style);
}

int ese_gui_style_get_font_size(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    return style->font_size;
}

// Colors getters and setters

EseColor *ese_gui_style_get_color(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->variant[variant];
}

void ese_gui_style_set_color(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");

    SET_COLOR(style->variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_color_hover(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->variant_hover[variant];
}

void ese_gui_style_set_color_hover(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");
    
    SET_COLOR(style->variant_hover[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_color_active(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->variant_active[variant];
}

void ese_gui_style_set_color_active(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");
    
    SET_COLOR(style->variant_active[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_alert_bg(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->alert_bg_variant[variant];
}

void ese_gui_style_set_alert_bg(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");

    SET_COLOR(style->alert_bg_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_alert_text(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->alert_text_variant[variant];
}

void ese_gui_style_set_alert_text(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");
    SET_COLOR(style->alert_text_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_alert_border(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->alert_border_variant[variant];
}

void ese_gui_style_set_alert_border(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    SET_COLOR(style->alert_border_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_bg(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->bg_variant[variant];
}

void ese_gui_style_set_bg(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");

    SET_COLOR(style->bg_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_bg_hover(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->bg_hover_variant[variant];
}

void ese_gui_style_set_bg_hover(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    SET_COLOR(style->bg_hover_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_bg_active(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->bg_active_variant[variant];
}

void ese_gui_style_set_bg_active(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");

    SET_COLOR(style->bg_active_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_text(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->text_variant[variant];
}

void ese_gui_style_set_text(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");

    SET_COLOR(style->text_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_text_hover(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->text_hover_variant[variant];
}

void ese_gui_style_set_text_hover(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");

    SET_COLOR(style->text_hover_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_text_active(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->text_active_variant[variant];
}

void ese_gui_style_set_text_active(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");

    SET_COLOR(style->text_active_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_border(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->border_variant[variant];
}

void ese_gui_style_set_border(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");

    SET_COLOR(style->border_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_border_hover(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->border_hover_variant[variant];
}

void ese_gui_style_set_border_hover(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");

    SET_COLOR(style->border_hover_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_border_active(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->border_active_variant[variant];
}

void ese_gui_style_set_border_active(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");

    SET_COLOR(style->border_active_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_tooltip_bg(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->tooltip_bg_variant[variant];
}

void ese_gui_style_set_tooltip_bg(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");

    SET_COLOR(style->tooltip_bg_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_tooltip_color(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    
    return style->tooltip_color_variant[variant];
}

void ese_gui_style_set_tooltip_color(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");

    SET_COLOR(style->tooltip_color_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_selection_bg(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->selection_bg_variant[variant];
}

void ese_gui_style_set_selection_bg(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");

    SET_COLOR(style->selection_bg_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_selection_color(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->selection_color_variant[variant];
}

void ese_gui_style_set_selection_color(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");

    SET_COLOR(style->selection_color_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_focus_ring(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->focus_ring_variant[variant];
}

void ese_gui_style_set_focus_ring(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");

    SET_COLOR(style->focus_ring_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
}

EseColor *ese_gui_style_get_highlight(const EseGuiStyle *style, EseGuiStyleVariant variant) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");

    return style->highlight_variant[variant];
}

void ese_gui_style_set_highlight(EseGuiStyle *style, EseGuiStyleVariant variant, const EseColor *color) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    log_assert("GUI_STYLE", variant >= 0 && variant < GUI_STYLE_VARIANT_MAX, "Invalid variant");
    log_assert("GUI_STYLE", color != NULL, "NULL color parameter");

    SET_COLOR(style->highlight_variant[variant], color);
    _ese_gui_style_notify_watchers(style);
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
    log_assert("GUI_STYLE", style, "ese_gui_style_lua_push called with NULL style");

    if (ese_gui_style_get_lua_ref(style) == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseGuiStyle **ud = (EseGuiStyle **)lua_newuserdata(ese_gui_style_get_state(style), sizeof(EseGuiStyle *));
        *ud = style;

        // Attach metatable
        luaL_getmetatable(ese_gui_style_get_state(style), GUI_STYLE_PROXY_META);
        lua_setmetatable(ese_gui_style_get_state(style), -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(ese_gui_style_get_state(style), LUA_REGISTRYINDEX, ese_gui_style_get_lua_ref(style));
    }
}

EseGuiStyle *ese_gui_style_lua_get(lua_State *L, int idx) {
    log_assert("GUI_STYLE", L, "ese_gui_style_lua_get called with NULL Lua state");
    
    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseGuiStyle **ud = (EseGuiStyle **)luaL_testudata(L, idx, GUI_STYLE_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_gui_style_ref(EseGuiStyle *style) {
    log_assert("GUI_STYLE", style, "ese_gui_style_ref called with NULL style");
    
    if (ese_gui_style_get_lua_ref(style) == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseGuiStyle **ud = (EseGuiStyle **)lua_newuserdata(ese_gui_style_get_state(style), sizeof(EseGuiStyle *));
        *ud = style;

        // Attach metatable
        luaL_getmetatable(ese_gui_style_get_state(style), GUI_STYLE_PROXY_META);
        lua_setmetatable(ese_gui_style_get_state(style), -2);

        // Store hard reference to prevent garbage collection
        int ref = luaL_ref(ese_gui_style_get_state(style), LUA_REGISTRYINDEX);
        _ese_gui_style_set_lua_ref(style, ref);
        _ese_gui_style_set_lua_ref_count(style, 1);
    } else {
        // Already referenced - just increment count
        _ese_gui_style_set_lua_ref_count(style, ese_gui_style_get_lua_ref_count(style) + 1);
    }

    profile_count_add("ese_gui_style_ref_count");
}

void ese_gui_style_unref(EseGuiStyle *style) {
    if (!style) return;
    
    if (ese_gui_style_get_lua_ref(style) != LUA_NOREF && ese_gui_style_get_lua_ref_count(style) > 0) {
        _ese_gui_style_set_lua_ref_count(style, ese_gui_style_get_lua_ref_count(style) - 1);

        // Clears the pointer so Lua GC will later dereference it
        lua_rawgeti(ese_gui_style_get_state(style), LUA_REGISTRYINDEX, ese_gui_style_get_lua_ref(style));
        if (lua_isuserdata(ese_gui_style_get_state(style), -1)) {
            EseGuiStyle **ud = (EseGuiStyle **)lua_touserdata(ese_gui_style_get_state(style), -1);
            if (ud) *ud = NULL;
        }
        lua_pop(ese_gui_style_get_state(style), 1);

        // now drop the registry ref
        luaL_unref(ese_gui_style_get_state(style), LUA_REGISTRYINDEX, ese_gui_style_get_lua_ref(style));
        _ese_gui_style_set_lua_ref(style, LUA_NOREF);
    }

    profile_count_add("ese_gui_style_unref_count");
}

cJSON *ese_gui_style_serialize(const EseGuiStyle *style) {
    log_assert("GUI_STYLE", style != NULL, "NULL style parameter");
    
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, "type", "GUI_STYLE");
    
    cJSON_AddNumberToObject(json, "border_width", (double)style->border_width);
    cJSON_AddNumberToObject(json, "padding_left", (double)style->padding_left);
    cJSON_AddNumberToObject(json, "padding_top", (double)style->padding_top);
    cJSON_AddNumberToObject(json, "padding_right", (double)style->padding_right);
    cJSON_AddNumberToObject(json, "padding_bottom", (double)style->padding_bottom);
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
    
    cJSON *font_size = cJSON_GetObjectItemCaseSensitive(data, "font_size");
    if (cJSON_IsNumber(font_size)) {
        style->font_size = font_size->valueint;
    }
    
    // Color deserialization omitted for expanded palette
    
    return style;
}
