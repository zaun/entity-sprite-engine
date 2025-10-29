/**
 * @file input_state.h
 *
 * @brief This header file contains the public API for managing input state.
 *
 * @details This module provides definitions for keyboard and mouse input keys,
 * and an API to create, manage, and query the state of a single input context.
 * It is designed to be used in conjunction with a Lua scripting environment.
 */
#ifndef ESE_INPUT_STATE_H
#define ESE_INPUT_STATE_H

#include <stdbool.h>
#include <stddef.h>

#define INPUT_STATE_PROXY_META "InputStateProxyMeta"
#define INPUT_STATE_META "InputStateMeta"

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief An enumeration of all supported keyboard and mouse input keys.
 */
typedef enum {
    InputKey_UNKNOWN = 0, /** An unknown or unmapped key. */

    // Letters
    InputKey_A,
    InputKey_B,
    InputKey_C,
    InputKey_D,
    InputKey_E,
    InputKey_F,
    InputKey_G,
    InputKey_H,
    InputKey_I,
    InputKey_J,
    InputKey_K,
    InputKey_L,
    InputKey_M,
    InputKey_N,
    InputKey_O,
    InputKey_P,
    InputKey_Q,
    InputKey_R,
    InputKey_S,
    InputKey_T,
    InputKey_U,
    InputKey_V,
    InputKey_W,
    InputKey_X,
    InputKey_Y,
    InputKey_Z,

    // Numbers (top row)
    InputKey_0,
    InputKey_1,
    InputKey_2,
    InputKey_3,
    InputKey_4,
    InputKey_5,
    InputKey_6,
    InputKey_7,
    InputKey_8,
    InputKey_9,

    // Function keys
    InputKey_F1,
    InputKey_F2,
    InputKey_F3,
    InputKey_F4,
    InputKey_F5,
    InputKey_F6,
    InputKey_F7,
    InputKey_F8,
    InputKey_F9,
    InputKey_F10,
    InputKey_F11,
    InputKey_F12,
    InputKey_F13,
    InputKey_F14,
    InputKey_F15,

    // Control keys
    InputKey_LSHIFT,
    InputKey_RSHIFT,
    InputKey_LCTRL,
    InputKey_RCTRL,
    InputKey_LALT,
    InputKey_RALT,
    InputKey_LCMD,
    InputKey_RCMD,

    // Navigation keys
    InputKey_UP,
    InputKey_DOWN,
    InputKey_LEFT,
    InputKey_RIGHT,
    InputKey_HOME,
    InputKey_END,
    InputKey_PAGEUP,
    InputKey_PAGEDOWN,
    InputKey_INSERT,
    InputKey_DELETE,

    // Special keys
    InputKey_SPACE,
    InputKey_ENTER,
    InputKey_ESCAPE,
    InputKey_TAB,
    InputKey_BACKSPACE,
    InputKey_CAPSLOCK,

    // Symbols
    InputKey_MINUS,
    InputKey_EQUAL,
    InputKey_LEFTBRACKET,
    InputKey_RIGHTBRACKET,
    InputKey_BACKSLASH,
    InputKey_SEMICOLON,
    InputKey_APOSTROPHE,
    InputKey_GRAVE,
    InputKey_COMMA,
    InputKey_PERIOD,
    InputKey_SLASH,

    // Keypad
    InputKey_KP_0,
    InputKey_KP_1,
    InputKey_KP_2,
    InputKey_KP_3,
    InputKey_KP_4,
    InputKey_KP_5,
    InputKey_KP_6,
    InputKey_KP_7,
    InputKey_KP_8,
    InputKey_KP_9,
    InputKey_KP_DECIMAL,
    InputKey_KP_ENTER,
    InputKey_KP_PLUS,
    InputKey_KP_MINUS,
    InputKey_KP_MULTIPLY,
    InputKey_KP_DIVIDE,

    InputKey_MAX /** The total number of keys supported. */
} EseInputKey;

typedef enum {
    InputMouse_LEFT,
    InputMouse_RIGHT,
    InputMouse_MIDDLE,
    InputMouse_X1,
    InputMouse_X2,
    InputMouse_MAX,
} EseInputMouseButton;

/**
 * @brief An opaque structure to hold the current state of all inputs.
 */
typedef struct EseInputState EseInputState;

/**
 * @brief Returns the size of the EseInputState structure in bytes.
 *
 * @return The size of the EseInputState structure in bytes.
 */
size_t ese_input_state_sizeof(void);

/**
 * @brief Gets the current X coordinate of the mouse cursor.
 *
 * @param input A pointer to the EseInputState object.
 * @return The current X coordinate of the mouse cursor.
 */
int ese_input_state_get_mouse_x(const EseInputState *input);

/**
 * @brief Gets the current Y coordinate of the mouse cursor.
 *
 * @param input A pointer to the EseInputState object.
 * @return The current Y coordinate of the mouse cursor.
 */
int ese_input_state_get_mouse_y(const EseInputState *input);

/**
 * @brief Gets the horizontal scroll delta for this frame.
 *
 * @param input A pointer to the EseInputState object.
 * @return The horizontal scroll delta for this frame.
 */
int ese_input_state_get_mouse_scroll_dx(const EseInputState *input);

/**
 * @brief Gets the vertical scroll delta for this frame.
 *
 * @param input A pointer to the EseInputState object.
 * @return The vertical scroll delta for this frame.
 */
int ese_input_state_get_mouse_scroll_dy(const EseInputState *input);

/**
 * @brief Gets the state of a specific key (whether it's currently held down).
 *
 * @param input A pointer to the EseInputState object.
 * @param key The input key to check.
 * @return true if the key is currently held down, false otherwise.
 */
bool ese_input_state_get_key_down(const EseInputState *input, EseInputKey key);

/**
 * @brief Gets the state of a specific key (whether it was pressed this frame).
 *
 * @param input A pointer to the EseInputState object.
 * @param key The input key to check.
 * @return true if the key was pressed this frame, false otherwise.
 */
bool ese_input_state_get_key_pressed(const EseInputState *input, EseInputKey key);

/**
 * @brief Gets the state of a specific key (whether it was released this frame).
 *
 * @param input A pointer to the EseInputState object.
 * @param key The input key to check.
 * @return true if the key was released this frame, false otherwise.
 */
bool ese_input_state_get_key_released(const EseInputState *input, EseInputKey key);

/**
 * @brief Gets the state of a specific mouse button.
 *
 * @param input A pointer to the EseInputState object.
 * @param button The mouse button index (0-7).
 * @return true if the mouse button is currently held down, false otherwise.
 */
bool ese_input_state_get_mouse_down(const EseInputState *input, int button);

/**
 * @brief Gets the state of a specific mouse button (whether it was pressed this
 * frame).
 *
 * @param input A pointer to the EseInputState object.
 * @param button The mouse button index (0-7).
 * @return true if the mouse button was pressed this frame, false otherwise.
 */
bool ese_input_state_get_mouse_clicked(const EseInputState *input, int button);

/**
 * @brief Gets the state of a specific mouse button (whether it was released
 * this frame).
 *
 * @param input A pointer to the EseInputState object.
 * @param button The mouse button index (0-7).
 * @return true if the mouse button was released this frame, false otherwise.
 */
bool ese_input_state_get_mouse_released(const EseInputState *input, int button);

/**
 * @brief Gets the Lua state associated with this input state.
 *
 * @param input A pointer to the EseInputState object.
 * @return A pointer to the Lua state, or NULL if not set.
 */
lua_State *ese_input_state_get_state(const EseInputState *input);

/**
 * @brief Gets the Lua reference count for this input state.
 *
 * @param input A pointer to the EseInputState object.
 * @return The current Lua reference count.
 */
int ese_input_state_get_lua_ref_count(const EseInputState *input);

/**
 * @brief Gets the Lua reference for this input state.
 *
 * @param input A pointer to the EseInputState object.
 * @return The Lua reference, or LUA_NOREF if not referenced.
 */
int ese_input_state_get_lua_ref(const EseInputState *input);

/**
 * @brief Initializes the Input userdata type in the Lua state.
 *
 * @details This function registers the 'InputProxyMeta' metatable in the Lua
 * registry, which defines how the `EseInputState` C object interacts with Lua
 * scripts.
 *
 * @param engine A pointer to the `EseLuaEngine` where the Input type will be
 * registered.
 * @return void
 *
 * @note This function should be called once during engine initialization.
 */
void ese_input_state_lua_init(EseLuaEngine *engine);

/**
 * @brief Creates a new EseInputState object with a zeroed state.
 *
 * @details This function allocates memory for a new `EseInputState` object and
 * initializes all key and mouse states to their default (unpressed) values. It
 * also sets up a Lua proxy for the new object.
 *
 * @param engine A pointer to the `EseLuaEngine` for the Lua state.
 * @return A pointer to the newly allocated `EseInputState` object.
 *
 * @warning The caller is responsible for freeing the returned memory with
 * `ese_input_state_destroy`.
 */
EseInputState *ese_input_state_create(EseLuaEngine *engine);

/**
 * @brief Creates a deep copy of an EseInputState object.
 *
 * @details This function allocates new memory and copies the state from a
 * source `EseInputState` object. It ensures all data, including key and mouse
 * states, is duplicated.
 *
 * @param src A pointer to the source `EseInputState` object to copy from.
 * @return A pointer to the new copied `EseInputState`.
 *
 * @note This function performs a memory allocation and a deep copy of the data.
 * @warning The caller is responsible for freeing the returned memory with
 * `ese_input_state_destroy`.
 */
EseInputState *ese_input_state_copy(const EseInputState *src);

/**
 * @brief Destroys and frees the memory for an EseInputState object.
 *
 * @details This function releases any associated Lua references and deallocates
 * the memory for the `EseInputState` C object.
 *
 * @param input A pointer to the `EseInputState` object to be destroyed.
 * @return void
 *
 * @note This function uses `log_assert` to check for a `NULL` input pointer.
 */
void ese_input_state_destroy(EseInputState *input);

/**
 * @private
 * @brief Pushes the EseInputState to the stacl.
 *
 * @param input A pointer to the `EseInputState` object to be pushed.
 * @return void
 */
void ese_input_state_lua_push(EseInputState *input);

/**
 * @brief Gets an EseInputState pointer from a Lua object.
 *
 * @param L The Lua state.
 * @param idx The stack index of the Lua Input object.
 * @return A pointer to the `EseInputState` object, or `NULL` if the object is
 * invalid.
 */
EseInputState *ese_input_state_lua_get(lua_State *L, int idx);

/**
 * @brief Increments the reference count for an EseInputState object.
 *
 * @details This function creates a Lua proxy table and stores a reference to
 * prevent garbage collection. Call this when storing the object in C-side data
 * structures.
 *
 * @param input A pointer to the `EseInputState` object to reference.
 * @return void
 */
void ese_input_state_ref(EseInputState *input);

/**
 * @brief Decrements the reference count for an EseInputState object.
 *
 * @details This function decrements the reference count and removes the Lua
 * registry reference when the count reaches zero. Call this when removing the
 * object from C-side data structures.
 *
 * @param input A pointer to the `EseInputState` object to unreference.
 * @return void
 */
void ese_input_state_unref(EseInputState *input);

#endif // ESE_INPUT_STATE_H
