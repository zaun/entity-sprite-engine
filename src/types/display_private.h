/**
 * @file display_private.h
 * 
 * @brief Private header file for EseDisplay implementation.
 * 
 * @details This file contains the private structure definition for EseDisplay
 * and is only included by the implementation file. This ensures the structure
 * remains opaque to external code.
 */

#ifndef ESE_DISPLAY_PRIVATE_H
#define ESE_DISPLAY_PRIVATE_H

#include "display.h"
#include <lua.h>

/**
 * @brief Private structure definition for EseDisplay.
 * 
 * @details This structure contains all the internal state for a display object.
 * It is only accessible within the display.c implementation file.
 */
struct EseDisplay {
    bool fullscreen;        /**< Whether the display is in fullscreen mode. */
    int width;              /**< The width of the display in pixels. */
    int height;             /**< The height of the display in pixels. */
    float aspect_ratio;     /**< The aspect ratio of the display (width/height). */
    EseViewport viewport;   /**< The current viewport configuration. */

    lua_State *state;       /**< A pointer to the Lua state. */
    int lua_ref;            /**< A reference to the Lua userdata object. */
    int lua_ref_count;      /**< Number of times this display has been referenced in C */
};

#endif // ESE_DISPLAY_PRIVATE_H
