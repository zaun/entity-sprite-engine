#include "types/input_state.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/input_state_lua.h"
#include "types/input_state_private.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <stdio.h>
#include <string.h>

/**
 * @private
 * @brief An array of string representations for each `EseInputKey`.
 *
 * @details This array is used for mapping `EseInputKey` enum values to
 * human-readable strings, primarily for Lua integration to create a `KEY`
 * table.
 */
const char *const input_state_key_names[] = {
    "UNKNOWN",
    // Letters
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S",
    "T", "U", "V", "W", "X", "Y", "Z",
    // Numbers
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    // Function keys
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15",
    // Control keys
    "LSHIFT", "RSHIFT", "LCTRL", "RCTRL", "LALT", "RALT", "LCMD", "RCMD",
    // Navigation keys
    "UP", "DOWN", "LEFT", "RIGHT", "HOME", "END", "PAGEUP", "PAGEDOWN", "INSERT", "DELETE",
    // Special keys
    "SPACE", "ENTER", "ESCAPE", "TAB", "BACKSPACE", "CAPSLOCK",
    // Symbols
    "MINUS", "EQUAL", "LEFTBRACKET", "RIGHTBRACKET", "BACKSLASH", "SEMICOLON", "APOSTROPHE",
    "GRAVE", "COMMA", "PERIOD", "SLASH",
    // Keypad
    "KP_0", "KP_1", "KP_2", "KP_3", "KP_4", "KP_5", "KP_6", "KP_7", "KP_8", "KP_9", "KP_DECIMAL",
    "KP_ENTER", "KP_PLUS", "KP_MINUS", "KP_MULTIPLY", "KP_DIVIDE",
    // Mouse buttons
    "MOUSE_LEFT", "MOUSE_RIGHT", "MOUSE_MIDDLE", "MOUSE_X1", "MOUSE_X2"};

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseInputState *_input_state_make(void);

// Private static setter forward declarations
static void _ese_input_state_set_lua_ref(EseInputState *input, int lua_ref);
static void _ese_input_state_set_lua_ref_count(EseInputState *input, int lua_ref_count);
static void _ese_input_state_set_state(EseInputState *input, lua_State *state);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseInputState instance with default values
 *
 * Allocates memory for a new EseInputState and initializes all fields to safe
 * defaults. All key states are initialized to false, mouse position to (0,0),
 * and no Lua state or references.
 *
 * @return Pointer to the newly created EseInputState, or NULL on allocation
 * failure
 */
static EseInputState *_input_state_make() {
    EseInputState *input =
        (EseInputState *)memory_manager.malloc(sizeof(EseInputState), MMTAG_INPUT_STATE);
    memset(input->keys_down, 0, sizeof(input->keys_down));
    memset(input->keys_pressed, 0, sizeof(input->keys_pressed));
    memset(input->keys_released, 0, sizeof(input->keys_released));
    memset(input->mouse_down, 0, sizeof(input->mouse_down));
    memset(input->mouse_clicked, 0, sizeof(input->mouse_clicked));
    memset(input->mouse_released, 0, sizeof(input->mouse_released));

    input->mouse_x = 0;
    input->mouse_y = 0;
    input->mouse_scroll_dx = 0;
    input->mouse_scroll_dy = 0;
    _ese_input_state_set_state(input, NULL);
    _ese_input_state_set_lua_ref(input, LUA_NOREF);
    _ese_input_state_set_lua_ref_count(input, 0);
    return input;
}

// Private static setters for Lua state management
static void _ese_input_state_set_lua_ref(EseInputState *input, int lua_ref) {
    input->lua_ref = lua_ref;
}

static void _ese_input_state_set_lua_ref_count(EseInputState *input, int lua_ref_count) {
    input->lua_ref_count = lua_ref_count;
}

static void _ese_input_state_set_state(EseInputState *input, lua_State *state) {
    input->state = state;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Size and basic accessors
size_t ese_input_state_sizeof(void) { return sizeof(EseInputState); }

// Mouse position getters
int ese_input_state_get_mouse_x(const EseInputState *input) {
    log_assert("INPUT_STATE", input, "ese_input_state_get_mouse_x called with NULL input");
    return input->mouse_x;
}

int ese_input_state_get_mouse_y(const EseInputState *input) {
    log_assert("INPUT_STATE", input, "ese_input_state_get_mouse_y called with NULL input");
    return input->mouse_y;
}

int ese_input_state_get_mouse_scroll_dx(const EseInputState *input) {
    log_assert("INPUT_STATE", input, "ese_input_state_get_mouse_scroll_dx called with NULL input");
    return input->mouse_scroll_dx;
}

int ese_input_state_get_mouse_scroll_dy(const EseInputState *input) {
    log_assert("INPUT_STATE", input, "ese_input_state_get_mouse_scroll_dy called with NULL input");
    return input->mouse_scroll_dy;
}

// Key state getters
bool ese_input_state_get_key_down(const EseInputState *input, EseInputKey key) {
    log_assert("INPUT_STATE", input, "ese_input_state_get_key_down called with NULL input");
    log_assert("INPUT_STATE", (key >= 0 && key < InputKey_MAX),
               "ese_input_state_get_key_down called with invalid key");
    return input->keys_down[key];
}

bool ese_input_state_get_key_pressed(const EseInputState *input, EseInputKey key) {
    log_assert("INPUT_STATE", input, "ese_input_state_get_key_pressed called with NULL input");
    log_assert("INPUT_STATE", (key >= 0 && key < InputKey_MAX),
               "ese_input_state_get_key_pressed called with invalid key");
    return input->keys_pressed[key];
}

bool ese_input_state_get_key_released(const EseInputState *input, EseInputKey key) {
    log_assert("INPUT_STATE", input, "ese_input_state_get_key_released called with NULL input");
    log_assert("INPUT_STATE", (key >= 0 && key < InputKey_MAX),
               "ese_input_state_get_key_released called with invalid key");
    return input->keys_released[key];
}

// Mouse button getters
bool ese_input_state_get_mouse_down(const EseInputState *input, int button) {
    log_assert("INPUT_STATE", input, "ese_input_state_get_mouse_down called with NULL input");
    log_assert("INPUT_STATE", (button >= 0 && button < MOUSE_BUTTON_COUNT),
               "ese_input_state_get_mouse_down called with invalid button");
    return input->mouse_down[button];
}

bool ese_input_state_get_mouse_clicked(const EseInputState *input, int button) {
    log_assert("INPUT_STATE", input, "ese_input_state_get_mouse_clicked called with NULL input");
    log_assert("INPUT_STATE", (button >= 0 && button < MOUSE_BUTTON_COUNT),
               "ese_input_state_get_mouse_clicked called with invalid button");
    return input->mouse_clicked[button];
}

bool ese_input_state_get_mouse_released(const EseInputState *input, int button) {
    log_assert("INPUT_STATE", input, "ese_input_state_get_mouse_released called with NULL input");
    log_assert("INPUT_STATE", (button >= 0 && button < MOUSE_BUTTON_COUNT),
               "ese_input_state_get_mouse_released called with invalid button");
    return input->mouse_released[button];
}

// Lua state getters
lua_State *ese_input_state_get_state(const EseInputState *input) {
    log_assert("INPUT_STATE", input, "ese_input_state_get_state called with NULL input");
    return input->state;
}

int ese_input_state_get_lua_ref_count(const EseInputState *input) {
    log_assert("INPUT_STATE", input, "ese_input_state_get_lua_ref_count called with NULL input");
    return input->lua_ref_count;
}

int ese_input_state_get_lua_ref(const EseInputState *input) {
    log_assert("INPUT_STATE", input, "ese_input_state_get_lua_ref called with NULL input");
    return input->lua_ref;
}

// Core lifecycle
EseInputState *ese_input_state_create(EseLuaEngine *engine) {
    EseInputState *input = _input_state_make();
    if (engine) {
        _ese_input_state_set_state(input, engine->runtime);
    }
    return input;
}

EseInputState *ese_input_state_copy(const EseInputState *src) {
    log_assert("INPUT_STATE", src, "ese_input_state_copy called with NULL src");
    log_assert("INPUT_STATE", memory_manager.malloc, "memory_manager.malloc is NULL");

    EseInputState *copy =
        (EseInputState *)memory_manager.malloc(sizeof(EseInputState), MMTAG_INPUT_STATE);
    log_assert("INPUT_STATE", copy, "ese_input_state_copy failed to allocate memory");

    memcpy(copy->keys_down, src->keys_down, sizeof(src->keys_down));
    memcpy(copy->keys_pressed, src->keys_pressed, sizeof(src->keys_pressed));
    memcpy(copy->keys_released, src->keys_released, sizeof(src->keys_released));
    memcpy(copy->mouse_down, src->mouse_down, sizeof(src->mouse_down));
    memcpy(copy->mouse_clicked, src->mouse_clicked, sizeof(src->mouse_clicked));
    memcpy(copy->mouse_released, src->mouse_released, sizeof(src->mouse_released));

    copy->mouse_x = src->mouse_x;
    copy->mouse_y = src->mouse_y;
    copy->mouse_scroll_dx = src->mouse_scroll_dx;
    copy->mouse_scroll_dy = src->mouse_scroll_dy;

    _ese_input_state_set_state(copy, src->state);
    _ese_input_state_set_lua_ref(copy, LUA_NOREF);
    _ese_input_state_set_lua_ref_count(copy, 0);

    return copy;
}

void ese_input_state_destroy(EseInputState *input) {
    if (!input)
        return;

    if (ese_input_state_get_lua_ref(input) == LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(input);
    } else {
        ese_input_state_unref(input);
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

// Lua integration
void ese_input_state_lua_init(EseLuaEngine *engine) { _ese_input_state_lua_init(engine); }

void ese_input_state_lua_push(EseInputState *input) {
    log_assert("INPUT_STATE", input, "ese_input_state_lua_push called with NULL input");

    if (ese_input_state_get_lua_ref(input) == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseInputState **ud = (EseInputState **)lua_newuserdata(ese_input_state_get_state(input),
                                                               sizeof(EseInputState *));
        *ud = input;

        // Attach metatable
        luaL_getmetatable(ese_input_state_get_state(input), INPUT_STATE_PROXY_META);
        lua_setmetatable(ese_input_state_get_state(input), -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(ese_input_state_get_state(input), LUA_REGISTRYINDEX,
                    ese_input_state_get_lua_ref(input));
    }
}

EseInputState *ese_input_state_lua_get(lua_State *L, int idx) {
    log_assert("INPUT_STATE", L, "ese_input_state_lua_get called with NULL Lua state");

    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }

    // Get the userdata and check metatable
    EseInputState **ud = (EseInputState **)luaL_testudata(L, idx, INPUT_STATE_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }

    return *ud;
}

void ese_input_state_ref(EseInputState *input) {
    log_assert("INPUT_STATE", input, "ese_input_state_ref called with NULL input");
    log_assert("INPUT_STATE", ese_input_state_get_state(input),
               "ese_input_state_ref called with C only input");

    if (ese_input_state_get_lua_ref(input) == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseInputState **ud = (EseInputState **)lua_newuserdata(ese_input_state_get_state(input),
                                                               sizeof(EseInputState *));
        *ud = input;

        // Attach metatable
        luaL_getmetatable(ese_input_state_get_state(input), INPUT_STATE_PROXY_META);
        lua_setmetatable(ese_input_state_get_state(input), -2);

        // Store hard reference to prevent garbage collection
        _ese_input_state_set_lua_ref(input,
                                     luaL_ref(ese_input_state_get_state(input), LUA_REGISTRYINDEX));
        _ese_input_state_set_lua_ref_count(input, 1);
    } else {
        // Already referenced - just increment count
        _ese_input_state_set_lua_ref_count(input, ese_input_state_get_lua_ref_count(input) + 1);
    }

    profile_count_add("ese_input_state_ref_count");
}

void ese_input_state_unref(EseInputState *input) {
    if (!input)
        return;
    log_assert("INPUT_STATE", ese_input_state_get_state(input),
               "ese_input_state_unref called with C only input");

    if (ese_input_state_get_lua_ref(input) != LUA_NOREF &&
        ese_input_state_get_lua_ref_count(input) > 0) {
        _ese_input_state_set_lua_ref_count(input, ese_input_state_get_lua_ref_count(input) - 1);

        if (ese_input_state_get_lua_ref_count(input) == 0) {
            // No more references - remove from registry
            luaL_unref(ese_input_state_get_state(input), LUA_REGISTRYINDEX,
                       ese_input_state_get_lua_ref(input));
            _ese_input_state_set_lua_ref(input, LUA_NOREF);
        }
    }

    profile_count_add("ese_input_state_unref_count");
}
