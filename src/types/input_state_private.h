/**
 * @file input_state_private.h
 * 
 * @brief Private header for EseInputState with direct field access.
 * 
 * @details This header contains the actual struct definition for EseInputState
 * and is intended for use only by platform code and engine_update.
 * The public API in input_state.h provides read-only access through getter functions.
 */

#ifndef ESE_INPUT_STATE_PRIVATE_H
#define ESE_INPUT_STATE_PRIVATE_H

#include <stdbool.h>
#include "input_state.h"

// Forward declarations
typedef struct lua_State lua_State;

#define MOUSE_BUTTON_COUNT 8 /** The total number of mouse buttons supported. */

/**
 * @brief A structure to hold the current state of all inputs.
 * 
 * @details This is the actual definition of EseInputState. Platform code and
 * engine_update can directly access these fields for maximum performance.
 * Other code should use the getter functions in input_state.h.
 */
struct EseInputState {
    bool keys_down[InputKey_MAX];           /** State of keys currently held down. */
    bool keys_pressed[InputKey_MAX];        /** Keys that were pressed this frame. */
    bool keys_released[InputKey_MAX];       /** Keys that were released this frame. */
    int mouse_x;                            /** The current X coordinate of the mouse cursor. */
    int mouse_y;                            /** The current Y coordinate of the mouse cursor. */
    int mouse_scroll_dx;                    /** The horizontal scroll delta this frame. */
    int mouse_scroll_dy;                    /** The vertical scroll delta this frame. */
    bool mouse_buttons[MOUSE_BUTTON_COUNT]; /** State of mouse buttons currently held down. */

    lua_State *state;                       /** A pointer to the Lua state. */
    int lua_ref;                            /** A reference to the Lua userdata object . */
    int lua_ref_count;                      /** Number of times this input state has been referenced in C */
};

#endif // ESE_INPUT_STATE_PRIVATE_H
