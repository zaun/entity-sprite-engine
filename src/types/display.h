/**
 * @file display.h
 * 
 * @brief This header file contains the public API for managing display state.
 * 
 * @details This module provides definitions for display configuration including viewport,
 * screen dimensions, and fullscreen state. It is designed to be used in conjunction with
 * a Lua scripting environment.
 */
#ifndef ESE_DISPLAY_STATE_H
#define ESE_DISPLAY_STATE_H

#include <stdbool.h>

// Forward declarations
typedef struct lua_State lua_State;

/**
 * @brief A structure to define a viewport rectangle.
 */
typedef struct {
    int width;          /**< The width of the viewport in pixels. */
    int height;         /**< The height of the viewport in pixels. */
} EseViewport;

/**
 * @brief A structure to hold the current state of the display.
 */
typedef struct {
    bool fullscreen;        /**< Whether the display is in fullscreen mode. */
    int width;              /**< The width of the display in pixels. */
    int height;             /**< The height of the display in pixels. */
    float aspect_ratio;     /**< The aspect ratio of the display (width/height). */
    EseViewport viewport;      /**< The current viewport configuration. */
    lua_State *state;       /**< A pointer to the Lua state. */
    int lua_ref;            /**< A reference to the Lua userdata object. */
} EseDisplay;

/**
 * @brief Initializes the Display userdata type in the Lua state.
 * 
 * @details This function registers the 'DisplayProxyMeta' metatable in the Lua registry,
 * which defines how the `EseDisplay` C object interacts with Lua scripts.
 * 
 * @param engine A pointer to the `EseLuaEngine` where the Display type will be registered.
 * @return void
 * 
 * @note This function should be called once during engine initialization.
 */
void display_state_lua_init(EseLuaEngine *engine);

/**
 * @brief Creates a new EseDisplay object with default values.
 * 
 * @details This function allocates memory for a new `EseDisplay` object and initializes
 * it with default display configuration. It also sets up a Lua proxy for the new object.
 * 
 * @param engine A pointer to the `EseLuaEngine` for the Lua state.
 * @return A pointer to the newly allocated `EseDisplay` object.
 * 
 * @warning The caller is responsible for freeing the returned memory with `display_state_destroy`.
 */
EseDisplay *display_state_create(EseLuaEngine *engine);

/**
 * @brief Creates a deep copy of a EseDisplay object.
 * 
 * @details This function allocates new memory and copies the state from a source `EseDisplay`
 * object. It ensures all data, including viewport configuration, is duplicated.
 * 
 * @param src A pointer to the source `EseDisplay` object to copy from.
 * @param engine A pointer to the `EseLuaEngine` for the new object's Lua state.
 * @return A pointer to the new copied `EseDisplay`.
 * 
 * @note This function performs a memory allocation and a deep copy of the data.
 * @warning The caller is responsible for freeing the returned memory with `display_state_destroy`.
 */
EseDisplay *display_state_copy(const EseDisplay *src, EseLuaEngine *engine);

/**
 * @brief Destroys and frees the memory for a EseDisplay object.
 * 
 * @details This function releases any associated Lua references and deallocates the
 * memory for the `EseDisplay` C object.
 * 
 * @param display A pointer to the `EseDisplay` object to be destroyed.
 * @return void
 * 
 * @note This function uses `log_assert` to check for a `NULL` display pointer.
 */
void display_state_destroy(EseDisplay *display);

/**
 * @brief Updates the display dimensions and recalculates dependent values.
 * 
 * @details This function updates the width, height, and aspect ratio of the display state.
 * It also updates the viewport to match the new dimensions if it was previously full-sized.
 * 
 * @param display A pointer to the `EseDisplay` object to update.
 * @param width The new width in pixels.
 * @param height The new height in pixels.
 * @return void
 */
void display_state_set_dimensions(EseDisplay *display, int width, int height);

/**
 * @brief Sets the fullscreen state of the display.
 * 
 * @details This function updates the fullscreen flag of the display state.
 * 
 * @param display A pointer to the `EseDisplay` object to update.
 * @param fullscreen Whether the display should be in fullscreen mode.
 * @return void
 */
void display_state_set_fullscreen(EseDisplay *display, bool fullscreen);

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
void display_state_set_viewport(EseDisplay *display, int width, int height);

#endif // ESE_DISPLAY_STATE_H
