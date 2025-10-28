/**
 * @file display.h
 *
 * @brief This header file contains the public API for managing display state.
 *
 * @details This module provides definitions for display configuration including
 * viewport, screen dimensions, and fullscreen state. It is designed to be used
 * in conjunction with a Lua scripting environment.
 */
#ifndef ESE_DISPLAY_H
#define ESE_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;

// Constants
#define DISPLAY_META "DisplayMeta"

/**
 * @brief A structure to define a viewport rectangle.
 */
typedef struct {
  int width;  /** The width of the viewport in pixels. */
  int height; /** The height of the viewport in pixels. */
} EseViewport;

/**
 * @brief Opaque structure for display state.
 *
 * @details The actual structure definition is in display_private.h.
 * This ensures the display remains opaque to external code.
 */
typedef struct EseDisplay EseDisplay;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
/**
 * @brief Creates a new EseDisplay object with default values.
 *
 * @details This function allocates memory for a new `EseDisplay` object and
 * initializes it with default display configuration. The display is created
 * without Lua references and must be explicitly referenced with
 * ese_display_ref() if Lua access is desired.
 *
 * @param engine A pointer to the `EseLuaEngine` for the Lua state.
 * @return A pointer to the newly allocated `EseDisplay` object.
 *
 * @warning The caller is responsible for freeing the returned memory with
 * `ese_display_destroy`.
 */
EseDisplay *ese_display_create(EseLuaEngine *engine);

/**
 * @brief Creates a deep copy of a EseDisplay object.
 *
 * @details This function allocates new memory and copies the state from a
 * source `EseDisplay` object. It ensures all data, including viewport
 * configuration, is duplicated. The copy is created without Lua references and
 * must be explicitly referenced with ese_display_ref() if Lua access is
 * desired.
 *
 * @param src A pointer to the source `EseDisplay` object to copy from.
 * @return A pointer to the new copied `EseDisplay`.
 *
 * @note This function performs a memory allocation and a deep copy of the data.
 * @warning The caller is responsible for freeing the returned memory with
 * `ese_display_destroy`.
 */
EseDisplay *ese_display_copy(const EseDisplay *src);

/**
 * @brief Destroys a EseDisplay object, managing memory based on Lua references.
 *
 * @details If the display has no Lua references (lua_ref == LUA_NOREF), frees
 * memory immediately. If the display has Lua references, decrements the
 * reference counter. When the counter reaches 0, removes the Lua reference and
 * lets Lua's garbage collector handle final cleanup.
 *
 * @note If the display is Lua-owned, memory may not be freed immediately.
 *       Lua's garbage collector will finalize it once no references remain.
 *
 * @param display A pointer to the `EseDisplay` object to be destroyed.
 * @return void
 *
 * @note This function uses `log_assert` to check for a `NULL` display pointer.
 */
void ese_display_destroy(EseDisplay *display);

// Lua integration
/**
 * @brief Initializes the Display userdata type in the Lua state.
 *
 * @details This function registers the 'DisplayProxyMeta' metatable in the Lua
 * registry, which defines how the `EseDisplay` C object interacts with Lua
 * scripts. Also creates the global "Display" table with "new" constructor.
 *
 * @param engine A pointer to the `EseLuaEngine` where the Display type will be
 * registered.
 * @return void
 *
 * @note This function should be called once during engine initialization.
 */
void ese_display_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EseDisplay object to the Lua stack.
 *
 * @details If the display has no Lua references (lua_ref == LUA_NOREF), creates
 * a new proxy table. If the display has Lua references, retrieves the existing
 *          proxy table from the registry.
 *
 * @param display Pointer to the EseDisplay object to push to Lua
 */
void ese_display_lua_push(EseDisplay *display);

/**
 * @brief Extracts a EseDisplay pointer from a Lua userdata object with type
 * safety.
 *
 * @details Retrieves the C EseDisplay pointer from the "__ptr" field of a Lua
 *          table that was created by ese_display_lua_push(). Performs
 *          type checking to ensure the object is a valid EseDisplay proxy table
 *          with the correct metatable and userdata pointer.
 *
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseDisplay object
 * @return Pointer to the EseDisplay object, or NULL if extraction fails or type
 * check fails
 *
 * @warning Returns NULL for invalid objects - always check return value before
 * use
 */
EseDisplay *ese_display_lua_get(lua_State *L, int idx);

/**
 * @brief References a EseDisplay object for Lua access with reference counting.
 *
 * @details If display->lua_ref is LUA_NOREF, pushes the display to Lua and
 * references it, setting lua_ref_count to 1. If display->lua_ref is already
 * set, increments the reference count by 1. This prevents the display from
 * being garbage collected while C code holds references to it.
 *
 * @param display Pointer to the EseDisplay object to reference
 */
void ese_display_ref(EseDisplay *display);

/**
 * @brief Unreferences a EseDisplay object, decrementing the reference count.
 *
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua
 * reference is removed from the registry. This function does NOT free memory.
 *
 * @param display Pointer to the EseDisplay object to unreference
 */
void ese_display_unref(EseDisplay *display);

// State management
/**
 * @brief Updates the display dimensions and recalculates dependent values.
 *
 * @details This function updates the width, height, and aspect ratio of the
 * display state. It also updates the viewport to match the new dimensions if it
 * was previously full-sized.
 *
 * @param display A pointer to the `EseDisplay` object to update.
 * @param width The new width in pixels.
 * @param height The new height in pixels.
 * @return void
 */
void ese_display_set_dimensions(EseDisplay *display, int width, int height);

/**
 * @brief Sets the fullscreen state of the display.
 *
 * @details This function updates the fullscreen flag of the display state.
 *
 * @param display A pointer to the `EseDisplay` object to update.
 * @param fullscreen Whether the display should be in fullscreen mode.
 * @return void
 */
void ese_display_set_fullscreen(EseDisplay *display, bool fullscreen);

/**
 * @brief Sets the viewport configuration.
 *
 * @details This function updates the viewport rectangle within the display.
 *
 * @param display A pointer to the `EseDisplay` object to update.
 * @param width The width of the viewport.
 * @param height The height of the viewport.
 * @return void
 */
void ese_display_set_viewport(EseDisplay *display, int width, int height);

// Getter functions
/**
 * @brief Gets the fullscreen state of the display.
 *
 * @param display A pointer to the `EseDisplay` object.
 * @return The fullscreen state (true if fullscreen, false otherwise).
 */
bool ese_display_get_fullscreen(const EseDisplay *display);

/**
 * @brief Gets the width of the display.
 *
 * @param display A pointer to the `EseDisplay` object.
 * @return The width in pixels.
 */
int ese_display_get_width(const EseDisplay *display);

/**
 * @brief Gets the height of the display.
 *
 * @param display A pointer to the `EseDisplay` object.
 * @return The height in pixels.
 */
int ese_display_get_height(const EseDisplay *display);

/**
 * @brief Gets the aspect ratio of the display.
 *
 * @param display A pointer to the `EseDisplay` object.
 * @return The aspect ratio (width/height).
 */
float ese_display_get_aspect_ratio(const EseDisplay *display);

/**
 * @brief Gets the viewport width.
 *
 * @param display A pointer to the `EseDisplay` object.
 * @return The viewport width in pixels.
 */
int ese_display_get_viewport_width(const EseDisplay *display);

/**
 * @brief Gets the viewport height.
 *
 * @param display A pointer to the `EseDisplay` object.
 * @return The viewport height in pixels.
 */
int ese_display_get_viewport_height(const EseDisplay *display);

/**
 * @brief Gets the viewport pointer.
 *
 * @param display A pointer to the `EseDisplay` object.
 * @return A pointer to the viewport structure.
 */
EseViewport *ese_display_get_viewport(const EseDisplay *display);

/**
 * @brief Gets the Lua state pointer.
 *
 * @param display A pointer to the `EseDisplay` object.
 * @return The Lua state pointer.
 */
lua_State *ese_display_get_state(const EseDisplay *display);

/**
 * @brief Gets the Lua reference count.
 *
 * @param display A pointer to the `EseDisplay` object.
 * @return The Lua reference count.
 */
int ese_display_get_lua_ref_count(const EseDisplay *display);

/**
 * @brief Gets the Lua reference.
 *
 * @param display A pointer to the `EseDisplay` object.
 * @return The Lua reference (LUA_NOREF if not referenced).
 */
int ese_display_get_lua_ref(const EseDisplay *display);

/**
 * @brief Gets the size of the EseDisplay structure.
 *
 * @return The size in bytes of the EseDisplay structure.
 */
size_t ese_display_sizeof(void);

#endif // ESE_DISPLAY_H
