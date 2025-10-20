/**
 * @file gui_style.h
 * @brief GUI style type for styling GUI elements
 * @details Provides style properties for GUI elements including colors, layout, and spacing
 * 
 * @copyright Copyright (c) 2024 ESE Project
 * @license See LICENSE.md for license information
 */

#ifndef ESE_GUI_STYLE_H
#define ESE_GUI_STYLE_H

#include <stdbool.h>
#include <stddef.h>
#include "vendor/json/cJSON.h"

// ========================================
// DEFINES AND STRUCTS
// ========================================

#define GUI_STYLE_PROXY_META "GuiStyleProxyMeta"
#define GUI_STYLE_META "GuiStyleMeta"

/**
 * @brief Represents a GUI style with layout, colors, and spacing properties.
 */
typedef struct EseGuiStyle EseGuiStyle;

/**
 * @brief Callback function type for gui style property change notifications.
 * 
 * @param style Pointer to the EseGuiStyle that changed
 * @param userdata User-provided data passed when registering the watcher
 */
typedef void (*EseGuiStyleWatcherCallback)(EseGuiStyle *style, void *userdata);

// ========================================
// FORWARD DECLARATIONS
// ========================================

typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseColor EseColor;
typedef enum EseGuiFlexDirection EseGuiFlexDirection;
typedef enum EseGuiFlexJustify EseGuiFlexJustify;
typedef enum EseGuiFlexAlignItems EseGuiFlexAlignItems;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
/**
 * @brief Creates a new EseGuiStyle object.
 * 
 * @details Allocates memory for a new EseGuiStyle and initializes to default values.
 *          The style is created without Lua references and must be explicitly
 *          referenced with ese_gui_style_ref() if Lua access is desired.
 * 
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseGuiStyle object
 * 
 * @warning The returned EseGuiStyle must be freed with ese_gui_style_destroy() to prevent memory leaks
 */
EseGuiStyle *ese_gui_style_create(EseLuaEngine *engine);

/**
 * @brief Copies a source EseGuiStyle into a new EseGuiStyle object.
 * 
 * @details This function creates a deep copy of an EseGuiStyle object. It allocates a new EseGuiStyle
 *          struct and copies all members. The copy is created without Lua references
 *          and must be explicitly referenced with ese_gui_style_ref() if Lua access is desired.
 * 
 * @param source Pointer to the source EseGuiStyle to copy.
 * @return A new, distinct EseGuiStyle object that is a copy of the source.
 * 
 * @warning The returned EseGuiStyle must be freed with ese_gui_style_destroy() to prevent memory leaks
 */
EseGuiStyle *ese_gui_style_copy(const EseGuiStyle *source);

/**
 * @brief Destroys a EseGuiStyle object, managing memory based on Lua references.
 * 
 * @details If the style has no Lua references (lua_ref == LUA_NOREF), frees memory immediately.
 *          If the style has Lua references, decrements the reference counter.
 *          When the counter reaches 0, removes the Lua reference and lets
 *          Lua's garbage collector handle final cleanup.
 * 
 * @note If the style is Lua-owned, memory may not be freed immediately.
 *       Lua's garbage collector will finalize it once no references remain.
 * 
 * @param style Pointer to the EseGuiStyle object to destroy
 */
void ese_gui_style_destroy(EseGuiStyle *style);

/**
 * @brief Gets the size of the EseGuiStyle structure in bytes.
 * 
 * @return The size of the EseGuiStyle structure in bytes
 */
size_t ese_gui_style_sizeof(void);

// Property access
/**
 * @brief Sets the flex direction of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @param direction The flex direction value
 */
void ese_gui_style_set_direction(EseGuiStyle *style, EseGuiFlexDirection direction);

/**
 * @brief Gets the flex direction of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @return The flex direction value
 */
EseGuiFlexDirection ese_gui_style_get_direction(const EseGuiStyle *style);

/**
 * @brief Sets the flex justify of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @param justify The flex justify value
 */
void ese_gui_style_set_justify(EseGuiStyle *style, EseGuiFlexJustify justify);

/**
 * @brief Gets the flex justify of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @return The flex justify value
 */
EseGuiFlexJustify ese_gui_style_get_justify(const EseGuiStyle *style);

/**
 * @brief Sets the flex align items of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @param align_items The flex align items value
 */
void ese_gui_style_set_align_items(EseGuiStyle *style, EseGuiFlexAlignItems align_items);

/**
 * @brief Gets the flex align items of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @return The flex align items value
 */
EseGuiFlexAlignItems ese_gui_style_get_align_items(const EseGuiStyle *style);

// Theme/context colors
void ese_gui_style_set_primary(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_primary(const EseGuiStyle *style);
void ese_gui_style_set_primary_hover(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_primary_hover(const EseGuiStyle *style);
void ese_gui_style_set_primary_active(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_primary_active(const EseGuiStyle *style);
void ese_gui_style_set_secondary(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_secondary(const EseGuiStyle *style);
void ese_gui_style_set_secondary_hover(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_secondary_hover(const EseGuiStyle *style);
void ese_gui_style_set_secondary_active(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_secondary_active(const EseGuiStyle *style);
void ese_gui_style_set_success(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_success(const EseGuiStyle *style);
void ese_gui_style_set_success_hover(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_success_hover(const EseGuiStyle *style);
void ese_gui_style_set_success_active(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_success_active(const EseGuiStyle *style);
void ese_gui_style_set_info(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_info(const EseGuiStyle *style);
void ese_gui_style_set_info_hover(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_info_hover(const EseGuiStyle *style);
void ese_gui_style_set_info_active(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_info_active(const EseGuiStyle *style);
void ese_gui_style_set_warning(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_warning(const EseGuiStyle *style);
void ese_gui_style_set_warning_hover(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_warning_hover(const EseGuiStyle *style);
void ese_gui_style_set_warning_active(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_warning_active(const EseGuiStyle *style);
void ese_gui_style_set_danger(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_danger(const EseGuiStyle *style);
void ese_gui_style_set_danger_hover(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_danger_hover(const EseGuiStyle *style);
void ese_gui_style_set_danger_active(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_danger_active(const EseGuiStyle *style);
void ese_gui_style_set_light(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_light(const EseGuiStyle *style);
void ese_gui_style_set_light_hover(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_light_hover(const EseGuiStyle *style);
void ese_gui_style_set_light_active(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_light_active(const EseGuiStyle *style);
void ese_gui_style_set_dark(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_dark(const EseGuiStyle *style);
void ese_gui_style_set_dark_hover(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_dark_hover(const EseGuiStyle *style);
void ese_gui_style_set_dark_active(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_dark_active(const EseGuiStyle *style);

// Alerts
void ese_gui_style_set_alert_success_bg(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_alert_success_bg(const EseGuiStyle *style);
void ese_gui_style_set_alert_success_text(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_alert_success_text(const EseGuiStyle *style);
void ese_gui_style_set_alert_success_border(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_alert_success_border(const EseGuiStyle *style);
void ese_gui_style_set_alert_info_bg(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_alert_info_bg(const EseGuiStyle *style);
void ese_gui_style_set_alert_info_text(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_alert_info_text(const EseGuiStyle *style);
void ese_gui_style_set_alert_info_border(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_alert_info_border(const EseGuiStyle *style);
void ese_gui_style_set_alert_warning_bg(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_alert_warning_bg(const EseGuiStyle *style);
void ese_gui_style_set_alert_warning_text(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_alert_warning_text(const EseGuiStyle *style);
void ese_gui_style_set_alert_warning_border(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_alert_warning_border(const EseGuiStyle *style);
void ese_gui_style_set_alert_danger_bg(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_alert_danger_bg(const EseGuiStyle *style);
void ese_gui_style_set_alert_danger_text(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_alert_danger_text(const EseGuiStyle *style);
void ese_gui_style_set_alert_danger_border(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_alert_danger_border(const EseGuiStyle *style);

// Backgrounds
void ese_gui_style_set_bg_primary(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_bg_primary(const EseGuiStyle *style);
void ese_gui_style_set_bg_secondary(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_bg_secondary(const EseGuiStyle *style);
void ese_gui_style_set_bg_success(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_bg_success(const EseGuiStyle *style);
void ese_gui_style_set_bg_info(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_bg_info(const EseGuiStyle *style);
void ese_gui_style_set_bg_warning(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_bg_warning(const EseGuiStyle *style);
void ese_gui_style_set_bg_danger(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_bg_danger(const EseGuiStyle *style);
void ese_gui_style_set_bg_light(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_bg_light(const EseGuiStyle *style);
void ese_gui_style_set_bg_dark(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_bg_dark(const EseGuiStyle *style);
void ese_gui_style_set_bg_white(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_bg_white(const EseGuiStyle *style);
void ese_gui_style_set_bg_transparent(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_bg_transparent(const EseGuiStyle *style);

// Text colors
void ese_gui_style_set_text_primary(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_text_primary(const EseGuiStyle *style);
void ese_gui_style_set_text_secondary(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_text_secondary(const EseGuiStyle *style);
void ese_gui_style_set_text_success(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_text_success(const EseGuiStyle *style);
void ese_gui_style_set_text_info(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_text_info(const EseGuiStyle *style);
void ese_gui_style_set_text_warning(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_text_warning(const EseGuiStyle *style);
void ese_gui_style_set_text_danger(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_text_danger(const EseGuiStyle *style);
void ese_gui_style_set_text_light(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_text_light(const EseGuiStyle *style);
void ese_gui_style_set_text_dark(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_text_dark(const EseGuiStyle *style);
void ese_gui_style_set_text_body(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_text_body(const EseGuiStyle *style);
void ese_gui_style_set_text_muted(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_text_muted(const EseGuiStyle *style);
void ese_gui_style_set_text_white(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_text_white(const EseGuiStyle *style);
void ese_gui_style_set_text_black(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_text_black(const EseGuiStyle *style);
void ese_gui_style_set_text_reset(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_text_reset(const EseGuiStyle *style);

// Borders
void ese_gui_style_set_border_primary(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_border_primary(const EseGuiStyle *style);
void ese_gui_style_set_border_secondary(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_border_secondary(const EseGuiStyle *style);
void ese_gui_style_set_border_success(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_border_success(const EseGuiStyle *style);
void ese_gui_style_set_border_info(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_border_info(const EseGuiStyle *style);
void ese_gui_style_set_border_warning(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_border_warning(const EseGuiStyle *style);
void ese_gui_style_set_border_danger(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_border_danger(const EseGuiStyle *style);
void ese_gui_style_set_border_light(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_border_light(const EseGuiStyle *style);
void ese_gui_style_set_border_dark(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_border_dark(const EseGuiStyle *style);
void ese_gui_style_set_border_white(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_border_white(const EseGuiStyle *style);
void ese_gui_style_set_border_gray_200(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_border_gray_200(const EseGuiStyle *style);
void ese_gui_style_set_border_gray_300(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_border_gray_300(const EseGuiStyle *style);

// Tooltips
void ese_gui_style_set_tooltip_bg(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_tooltip_bg(const EseGuiStyle *style);
void ese_gui_style_set_tooltip_color(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_tooltip_color(const EseGuiStyle *style);

// Misc
void ese_gui_style_set_selection_bg(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_selection_bg(const EseGuiStyle *style);
void ese_gui_style_set_selection_color(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_selection_color(const EseGuiStyle *style);
void ese_gui_style_set_focus_ring(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_focus_ring(const EseGuiStyle *style);
void ese_gui_style_set_highlight(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_highlight(const EseGuiStyle *style);
void ese_gui_style_set_list_group_action(EseGuiStyle *style, const EseColor *color);
EseColor *ese_gui_style_get_list_group_action(const EseGuiStyle *style);

/**
 * @brief Sets the border width of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @param border_width The border width value
 */
void ese_gui_style_set_border_width(EseGuiStyle *style, int border_width);

/**
 * @brief Gets the border width of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @return The border width value
 */
int ese_gui_style_get_border_width(const EseGuiStyle *style);

/**
 * @brief Sets the padding left of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @param padding_left The padding left value
 */
void ese_gui_style_set_padding_left(EseGuiStyle *style, int padding_left);

/**
 * @brief Gets the padding left of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @return The padding left value
 */
int ese_gui_style_get_padding_left(const EseGuiStyle *style);

/**
 * @brief Sets the padding top of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @param padding_top The padding top value
 */
void ese_gui_style_set_padding_top(EseGuiStyle *style, int padding_top);

/**
 * @brief Gets the padding top of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @return The padding top value
 */
int ese_gui_style_get_padding_top(const EseGuiStyle *style);

/**
 * @brief Sets the padding right of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @param padding_right The padding right value
 */
void ese_gui_style_set_padding_right(EseGuiStyle *style, int padding_right);

/**
 * @brief Gets the padding right of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @return The padding right value
 */
int ese_gui_style_get_padding_right(const EseGuiStyle *style);

/**
 * @brief Sets the padding bottom of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @param padding_bottom The padding bottom value
 */
void ese_gui_style_set_padding_bottom(EseGuiStyle *style, int padding_bottom);

/**
 * @brief Gets the padding bottom of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @return The padding bottom value
 */
int ese_gui_style_get_padding_bottom(const EseGuiStyle *style);

/**
 * @brief Sets the spacing of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @param spacing The spacing value
 */
void ese_gui_style_set_spacing(EseGuiStyle *style, int spacing);

/**
 * @brief Gets the spacing of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @return The spacing value
 */
int ese_gui_style_get_spacing(const EseGuiStyle *style);

/**
 * @brief Sets the font size of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @param font_size The font size value
 */
void ese_gui_style_set_font_size(EseGuiStyle *style, int font_size);

/**
 * @brief Gets the font size of the style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @return The font size value
 */
int ese_gui_style_get_font_size(const EseGuiStyle *style);

// Lua-related access
/**
 * @brief Gets the Lua state associated with this gui style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @return Pointer to the Lua state, or NULL if none
 */
lua_State *ese_gui_style_get_state(const EseGuiStyle *style);

/**
 * @brief Gets the Lua registry reference for this gui style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @return The Lua registry reference value
 */
int ese_gui_style_get_lua_ref(const EseGuiStyle *style);

/**
 * @brief Gets the Lua reference count for this gui style.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @return The current reference count
 */
int ese_gui_style_get_lua_ref_count(const EseGuiStyle *style);

/**
 * @brief Adds a watcher callback to be notified when any gui style property changes.
 * 
 * @details The callback will be called whenever any property of the gui style is modified.
 *          Multiple watchers can be registered on the same gui style.
 * 
 * @param style Pointer to the EseGuiStyle object to watch
 * @param callback Function to call when properties change
 * @param userdata User-provided data to pass to the callback
 * @return true if watcher was added successfully, false otherwise
 */
bool ese_gui_style_add_watcher(EseGuiStyle *style, EseGuiStyleWatcherCallback callback, void *userdata);

/**
 * @brief Removes a previously registered watcher callback.
 * 
 * @details Removes the first occurrence of the callback with matching userdata.
 *          If the same callback is registered multiple times with different userdata,
 *          only the first match will be removed.
 * 
 * @param style Pointer to the EseGuiStyle object
 * @param callback Function to remove
 * @param userdata User data that was used when registering
 * @return true if watcher was removed, false if not found
 */
bool ese_gui_style_remove_watcher(EseGuiStyle *style, EseGuiStyleWatcherCallback callback, void *userdata);

// Lua integration
/**
 * @brief Initializes the EseGuiStyle userdata type in the Lua state.
 * 
 * @details Creates and registers the "GuiStyleProxyMeta" metatable with __index, __newindex,
 *          __gc, __tostring metamethods for property access and garbage collection.
 *          This allows EseGuiStyle objects to be used naturally from Lua with dot notation.
 *          Also creates the global "GuiStyle" table with "new" constructor.
 * 
 * @param engine EseLuaEngine pointer where the EseGuiStyle type will be registered
 */
void ese_gui_style_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EseGuiStyle object to the Lua stack.
 * 
 * @details If the style has no Lua references (lua_ref == LUA_NOREF), creates a new
 *          proxy table. If the style has Lua references, retrieves the existing
 *          proxy table from the registry.
 * 
 * @param style Pointer to the EseGuiStyle object to push to Lua
 */
void ese_gui_style_lua_push(EseGuiStyle *style);

/**
 * @brief Extracts a EseGuiStyle pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EseGuiStyle pointer from the "__ptr" field of a Lua
 *          table that was created by ese_gui_style_lua_push(). Performs
 *          type checking to ensure the object is a valid EseGuiStyle proxy table
 *          with the correct metatable and userdata pointer.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseGuiStyle object
 * @return Pointer to the EseGuiStyle object, or NULL if extraction fails or type check fails
 * 
 * @warning Returns NULL for invalid objects - always check return value before use
 */
EseGuiStyle *ese_gui_style_lua_get(lua_State *L, int idx);

/**
 * @brief References a EseGuiStyle object for Lua access with reference counting.
 * 
 * @details If style->lua_ref is LUA_NOREF, pushes the style to Lua and references it,
 *          setting lua_ref_count to 1. If style->lua_ref is already set, increments
 *          the reference count by 1. This prevents the style from being garbage
 *          collected while C code holds references to it.
 * 
 * @param style Pointer to the EseGuiStyle object to reference
 */
void ese_gui_style_ref(EseGuiStyle *style);

/**
 * @brief Unreferences a EseGuiStyle object, decrementing the reference count.
 * 
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua reference
 *          is removed from the registry. This function does NOT free memory.
 * 
 * @param style Pointer to the EseGuiStyle object to unreference
 */
void ese_gui_style_unref(EseGuiStyle *style);

/**
 * @brief Serializes an EseGuiStyle to a cJSON object.
 *
 * @details Creates a cJSON object representing the gui style with type "GUI_STYLE"
 *          and all style properties. Only serializes the style data, not
 *          Lua-related fields.
 *
 * @param style Pointer to the EseGuiStyle object to serialize
 * @return cJSON object representing the gui style, or NULL on failure
 *
 * @warning The caller is responsible for calling cJSON_Delete() on the returned object
 */
cJSON *ese_gui_style_serialize(const EseGuiStyle *style);

/**
 * @brief Deserializes an EseGuiStyle from a cJSON object.
 *
 * @details Creates a new EseGuiStyle from a cJSON object with type "GUI_STYLE"
 *          and all style properties. The style is created with the specified engine
 *          and must be explicitly referenced with ese_gui_style_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for gui style creation
 * @param data cJSON object containing gui style data
 * @return Pointer to newly created EseGuiStyle object, or NULL on failure
 *
 * @warning The returned EseGuiStyle must be freed with ese_gui_style_destroy() to prevent memory leaks
 */
EseGuiStyle *ese_gui_style_deserialize(EseLuaEngine *engine, const cJSON *data);

#endif // ESE_GUI_STYLE_H
