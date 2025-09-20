#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "scripting/lua_value.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/input_state.h"
#include "types/input_state_private.h"

/**
 * @private
 * @brief An array of string representations for each `EseInputKey`.
 * 
 * @details This array is used for mapping `EseInputKey` enum values to human-readable strings,
 * primarily for Lua integration to create a `KEY` table.
 */
static const char *const input_state_key_names[] = {
    "UNKNOWN",
    // Letters
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
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
    "MINUS", "EQUAL", "LEFTBRACKET", "RIGHTBRACKET", "BACKSLASH", "SEMICOLON", "APOSTROPHE", "GRAVE", "COMMA", "PERIOD", "SLASH",
    // Keypad
    "KP_0", "KP_1", "KP_2", "KP_3", "KP_4", "KP_5", "KP_6", "KP_7", "KP_8", "KP_9", "KP_DECIMAL", "KP_ENTER", "KP_PLUS", "KP_MINUS", "KP_MULTIPLY", "KP_DIVIDE",
    // Mouse buttons
    "MOUSE_LEFT", "MOUSE_RIGHT", "MOUSE_MIDDLE", "MOUSE_X1", "MOUSE_X2"
};

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseInputState *_input_state_make(void);

// Lua metamethods
static EseLuaValue* _input_state_lua_gc(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _input_state_lua_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _input_state_lua_newindex(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _input_state_lua_tostring(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

// Lua helpers
static EseLuaValue* _input_state_keys_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _input_state_mouse_buttons_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _input_state_key_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _input_state_readonly_error(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseInputState instance with default values
 * 
 * Allocates memory for a new EseInputState and initializes all fields to safe defaults.
 * All key states are initialized to false, mouse position to (0,0), and no Lua state or references.
 * 
 * @return Pointer to the newly created EseInputState, or NULL on allocation failure
 */
static EseInputState *_input_state_make() {
    EseInputState *input = (EseInputState *)memory_manager.malloc(sizeof(EseInputState), MMTAG_INPUT_STATE);
    memset(input->keys_down, 0, sizeof(input->keys_down));
    memset(input->keys_pressed, 0, sizeof(input->keys_pressed));
    memset(input->keys_released, 0, sizeof(input->keys_released));
    memset(input->mouse_buttons, 0, sizeof(input->mouse_buttons));

    input->mouse_x = 0;
    input->mouse_y = 0;
    input->mouse_scroll_dx = 0;
    input->mouse_scroll_dy = 0;
    input->lua_ref = ESE_LUA_NOREF;
    input->lua_ref_count = 0;
    return input;
}

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseInputState
 * 
 * Handles cleanup when a Lua proxy table for an EseInputState is garbage collected.
 * Only frees the underlying EseInputState if it has no C-side references.
 * 
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static EseLuaValue* _input_state_lua_gc(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return NULL;
    }

    // Get the input state from the first argument
    if (!lua_value_is_input_state(argv[0])) {
        return NULL;
    }

    EseInputState *input = lua_value_get_input_state(argv[0]);
    if (input) {
        // If lua_ref == ESE_LUA_NOREF, there are no more references to this input, 
        // so we can free it.
        // If lua_ref != ESE_LUA_NOREF, this input was referenced from C and should not be freed.
        if (input->lua_ref == ESE_LUA_NOREF) {
            ese_input_state_destroy(input);
        }
    }

    return NULL;
}

/**
 * @brief Lua __index metamethod for EseInputState property access
 * 
 * Provides read access to input state properties from Lua. When a Lua script
 * accesses input.mouse_x, input.keys_down, etc., this function is called to retrieve the values.
 * Creates read-only proxy tables for key arrays and mouse button states.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid properties, 0 for invalid)
 */
static int _input_state_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_INPUT_STATE_INDEX);
    EseInputState *input = ese_input_state_lua_get(L, 1);
    const char *key = lua_value_get_string(argv[2-1]);
    if (!input || !key) {
        profile_cancel(PROFILE_LUA_INPUT_STATE_INDEX);
        return lua_value_create_nil();
    }

    // keys_down, keys_pressed, keys_released tables
    if (strcmp(key, "keys_down") == 0 ||
        strcmp(key, "keys_pressed") == 0 ||
        strcmp(key, "keys_released") == 0) {
        bool *keys_array = NULL;
        if (strcmp(key, "keys_down") == 0) keys_array = (bool *)input->keys_down;
        else if (strcmp(key, "keys_pressed") == 0) keys_array = (bool *)input->keys_pressed;
        else if (strcmp(key, "keys_released") == 0) keys_array = (bool *)input->keys_released;

        // Create the table
        lua_newtable(L);

        // Create and set the metatable
        lua_newtable(L);
        
        // Set __index closure with the keys array as upvalue
        lua_pushlightuserdata(L, keys_array);
        lua_pushcclosure(L, _input_state_keys_index, 1);
        lua_setfield(L, -2, "__index");
        
        // Set __newindex to error
        lua_pushcfunction(L, _input_state_readonly_error);
        lua_setfield(L, -2, "__newindex");
        
        // Apply metatable to the table
        lua_setmetatable(L, -2);

        profile_stop(PROFILE_LUA_INPUT_STATE_INDEX, "input_state_lua_index (keys_table)");
        return lua_value_create_nil();
    }

    // mouse_x, mouse_y, mouse_scroll_dx, mouse_scroll_dy
    if (strcmp(key, "mouse_x") == 0) {
        lua_pushinteger(L, input->mouse_x);
        profile_stop(PROFILE_LUA_INPUT_STATE_INDEX, "input_state_lua_index (mouse_x)");
        return lua_value_create_nil();
    }
    if (strcmp(key, "mouse_y") == 0) {
        lua_pushinteger(L, input->mouse_y);
        profile_stop(PROFILE_LUA_INPUT_STATE_INDEX, "input_state_lua_index (mouse_y)");
        return lua_value_create_nil();
    }
    if (strcmp(key, "mouse_scroll_dx") == 0) {
        lua_pushinteger(L, input->mouse_scroll_dx);
        profile_stop(PROFILE_LUA_INPUT_STATE_INDEX, "input_state_lua_index (mouse_scroll_dx)");
        return lua_value_create_nil();
    }
    if (strcmp(key, "mouse_scroll_dy") == 0) {
        lua_pushinteger(L, input->mouse_scroll_dy);
        profile_stop(PROFILE_LUA_INPUT_STATE_INDEX, "input_state_lua_index (mouse_scroll_dy)");
        return lua_value_create_nil();
    }

    // mouse_buttons table proxy (read-only)
    if (strcmp(key, "mouse_buttons") == 0) {
        // Create and set the metatable
        lua_newtable(L);
        
        // Set __index closure with the input pointer as upvalue
        lua_pushlightuserdata(L, input);
        lua_pushcclosure(L, _input_state_mouse_buttons_index, 1);
        lua_setfield(L, -2, "__index");
        
        // Set __newindex to error
        lua_pushcfunction(L, _input_state_readonly_error);
        lua_setfield(L, -2, "__newindex");
        
        // Apply metatable to the table
        lua_setmetatable(L, -2);

        profile_stop(PROFILE_LUA_INPUT_STATE_INDEX, "input_state_lua_index (mouse_buttons)");
        return lua_value_create_nil();
    }

    // KEY table with constants
    if (strcmp(key, "KEY") == 0) {
        // Create empty table
        lua_newtable(L);

        // Attach KEY metatable
        luaL_getmetatable(L, INPUT_STATE_PROXY_META "_KEY");
        if (lua_setmetatable(L, -2) == 0) {
            log_assert("INPUT_STATE", 0, "Failed to get metatable for KEY table");
        }

        profile_stop(PROFILE_LUA_INPUT_STATE_INDEX, "input_state_lua_index (KEY_table)");
        return lua_value_create_nil();
    }

    profile_stop(PROFILE_LUA_INPUT_STATE_INDEX, "input_state_lua_index (invalid)");
    return lua_value_create_nil();
}

/**
 * @brief Lua __newindex metamethod for EseInputState property assignment
 * 
 * Provides write access to input state properties from Lua. Since input state is read-only,
 * this function always returns an error for any property assignment attempts.
 * 
 * @param L Lua state
 * @return Never returns (always calls luaL_error)
 */
static int _input_state_lua_newindex(lua_State *L) {
    profile_start(PROFILE_LUA_INPUT_STATE_NEWINDEX);
    profile_stop(PROFILE_LUA_INPUT_STATE_NEWINDEX, "input_state_lua_newindex (error)");
    return lua_value_create_error("result", "Input object is read-only");
}

/**
 * @brief Lua __tostring metamethod for EseInputState string representation
 * 
 * Converts an EseInputState to a human-readable string for debugging and display.
 * The format includes the memory address, mouse position, and visual representation of key states.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
static int _input_state_lua_tostring(lua_State *L) {
    EseInputState *input = ese_input_state_lua_get(L, 1);
    if (!input) {
        return lua_value_create_string("Input: (invalid)");
        return lua_value_create_nil();
    }
    char buf[1024];
    snprintf(
        buf, sizeof(buf),
        "Input: %p (mouse_x=%d, mouse_y=%d mouse_scroll_dx=%d, mouse_scroll_dy=%d)\n"
        "Down: %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
        (void *)input, input->mouse_x, input->mouse_y,
        input->mouse_scroll_dx, input->mouse_scroll_dy,
        input->keys_down[InputKey_A] ? 'A': ' ',
        input->keys_down[InputKey_B] ? 'B': ' ',
        input->keys_down[InputKey_C] ? 'C': ' ',
        input->keys_down[InputKey_D] ? 'D': ' ',
        input->keys_down[InputKey_E] ? 'E': ' ',
        input->keys_down[InputKey_F] ? 'F': ' ',
        input->keys_down[InputKey_G] ? 'G': ' ',
        input->keys_down[InputKey_H] ? 'H': ' ',
        input->keys_down[InputKey_I] ? 'I': ' ',
        input->keys_down[InputKey_J] ? 'J': ' ',
        input->keys_down[InputKey_K] ? 'K': ' ',
        input->keys_down[InputKey_L] ? 'L': ' ',
        input->keys_down[InputKey_M] ? 'M': ' ',
        input->keys_down[InputKey_N] ? 'N': ' ',
        input->keys_down[InputKey_O] ? 'O': ' ',
        input->keys_down[InputKey_P] ? 'P': ' ',
        input->keys_down[InputKey_Q] ? 'Q': ' ',
        input->keys_down[InputKey_R] ? 'R': ' ',
        input->keys_down[InputKey_S] ? 'S': ' ',
        input->keys_down[InputKey_T] ? 'T': ' ',
        input->keys_down[InputKey_U] ? 'U': ' ',
        input->keys_down[InputKey_V] ? 'V': ' ',
        input->keys_down[InputKey_W] ? 'W': ' ',
        input->keys_down[InputKey_X] ? 'X': ' ',
        input->keys_down[InputKey_Y] ? 'Y': ' ',
        input->keys_down[InputKey_Z] ? 'Z': ' ',
        input->keys_down[InputKey_0] ? '0': ' ',
        input->keys_down[InputKey_1] ? '1': ' ',
        input->keys_down[InputKey_2] ? '2': ' ',
        input->keys_down[InputKey_3] ? '3': ' ',
        input->keys_down[InputKey_4] ? '4': ' ',
        input->keys_down[InputKey_5] ? '5': ' ',
        input->keys_down[InputKey_6] ? '6': ' ',
        input->keys_down[InputKey_7] ? '7': ' ',
        input->keys_down[InputKey_8] ? '8': ' ',
        input->keys_down[InputKey_9] ? '9': ' ',
        input->keys_down[InputKey_LSHIFT] ? '<': ' ',
        input->keys_down[InputKey_RSHIFT] ? '>': ' '
    );
    return lua_value_create_string(buf);
    return lua_value_create_nil();
}

// Lua helpers
/**
 * @brief Lua helper function for accessing key state arrays
 * 
 * Provides read-only access to key state arrays (keys_down, keys_pressed, keys_released).
 * This function is used as the __index metamethod for the proxy tables created in the main index function.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - boolean key state)
 */
static int _input_state_keys_index(lua_State *L) {
    bool *arr = (bool *)lua_touserdata(L, lua_upvalueindex(1));
    int key_idx = (int)luaL_checkinteger(L, 2);
    log_assert("INPUT_STATE", (key_idx >= 0 && key_idx < InputKey_MAX), "Invalid key index");
    return lua_value_create_bool(arr[key_idx]);
    return lua_value_create_nil();
}

/**
 * @brief Lua helper function for accessing mouse button states
 * 
 * Provides read-only access to mouse button states. This function is used as the
 * __index metamethod for the mouse_buttons proxy table.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - boolean button state)
 */
static int _input_state_mouse_buttons_index(lua_State *L) {
    EseInputState *input = (EseInputState *)lua_touserdata(L, lua_upvalueindex(1));
    int btn = (int)luaL_checkinteger(L, 2);
    if (btn < 0 || btn >= MOUSE_BUTTON_COUNT) {
        return lua_value_create_error("result", "Invalid mouse button index");
    }
    return lua_value_create_bool(input->mouse_buttons[btn]);
    return lua_value_create_nil();
}


/**
 * @brief Lua helper function for accessing KEY constants
 * 
 * Provides read-only access to key constants. This function is used as the
 * __index metamethod for the KEY table.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - integer key value)
 */
static int _input_state_key_index(lua_State *L) {
    const char *key_name = lua_value_get_string(argv[2-1]);
    if (!key_name) {
        return lua_value_create_error("result", "Key name must be a string");
    }
    
    // Find the key name in the array
    for (int i = 0; i < InputKey_MAX; i++) {
        if (strcmp(input_state_key_names[i], key_name) == 0) {
            lua_pushinteger(L, i);
            return lua_value_create_nil();
        }
    }
    
    return lua_value_create_error("result", "Unknown key name: %s", key_name);
}

/**
 * @brief Lua helper function for read-only error handling
 * 
 * Called when any attempt is made to modify read-only input state tables.
 * Always returns an error indicating that the tables are read-only.
 * 
 * @param L Lua state
 * @return Never returns (always calls luaL_error)
 */
static int _input_state_readonly_error(lua_State *L) {
    return lua_value_create_error("result", "Input tables are read-only");
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Size and basic accessors
size_t ese_input_state_sizeof(void) {
    return sizeof(EseInputState);
}

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
    log_assert("INPUT_STATE", (key >= 0 && key < InputKey_MAX), "ese_input_state_get_key_down called with invalid key");
    return input->keys_down[key];
}

bool ese_input_state_get_key_pressed(const EseInputState *input, EseInputKey key) {
    log_assert("INPUT_STATE", input, "ese_input_state_get_key_pressed called with NULL input");
    log_assert("INPUT_STATE", (key >= 0 && key < InputKey_MAX), "ese_input_state_get_key_pressed called with invalid key");
    return input->keys_pressed[key];
}

bool ese_input_state_get_key_released(const EseInputState *input, EseInputKey key) {
    log_assert("INPUT_STATE", input, "ese_input_state_get_key_released called with NULL input");
    log_assert("INPUT_STATE", (key >= 0 && key < InputKey_MAX), "ese_input_state_get_key_released called with invalid key");
    return input->keys_released[key];
}

// Mouse button getters
bool ese_input_state_get_mouse_button(const EseInputState *input, int button) {
    log_assert("INPUT_STATE", input, "ese_input_state_get_mouse_button called with NULL input");
    log_assert("INPUT_STATE", (button >= 0 && button < MOUSE_BUTTON_COUNT), "ese_input_state_get_mouse_button called with invalid button");
    return input->mouse_buttons[button];
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
        input->state = engine->runtime;
    }
    return input;
}

EseInputState *ese_input_state_copy(const EseInputState *src) {
    log_assert("INPUT_STATE", src, "ese_input_state_copy called with NULL src");
    log_assert("INPUT_STATE", memory_manager.malloc, "memory_manager.malloc is NULL");

    EseInputState *copy = (EseInputState *)memory_manager.malloc(sizeof(EseInputState), MMTAG_INPUT_STATE);
    log_assert("INPUT_STATE", copy, "ese_input_state_copy failed to allocate memory");

    memcpy(copy->keys_down, src->keys_down, sizeof(src->keys_down));
    memcpy(copy->keys_pressed, src->keys_pressed, sizeof(src->keys_pressed));
    memcpy(copy->keys_released, src->keys_released, sizeof(src->keys_released));
    memcpy(copy->mouse_buttons, src->mouse_buttons, sizeof(src->mouse_buttons));

    copy->mouse_x = src->mouse_x;
    copy->mouse_y = src->mouse_y;
    copy->mouse_scroll_dx = src->mouse_scroll_dx;
    copy->mouse_scroll_dy = src->mouse_scroll_dy;

    copy->state = src->state;
    copy->lua_ref = LUA_NOREF;
    copy->lua_ref_count = 0;

    return copy;
}

void ese_input_state_destroy(EseInputState *input) {
    if (!input) return;
    
    if (input->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(input);
    } else {
        ese_input_state_unref(input);
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

// Lua integration
void ese_input_state_lua_init(EseLuaEngine *engine) {
    log_assert("INPUT_STATE", engine, "ese_input_state_lua_init called with NULL engine");
    log_assert("INPUT_STATE", engine->runtime, "ese_input_state_lua_init called with NULL engine->runtime");

    if (luaL_newmetatable(engine->runtime, INPUT_STATE_PROXY_META)) {
        log_debug("LUA", "Adding InputStateMeta to engine");
        lua_pushstring(engine->runtime, INPUT_STATE_PROXY_META);
        lua_setfield(engine->runtime, -2, "__name");
        lua_pushcfunction(engine->runtime, _input_state_lua_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _input_state_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushcfunction(engine->runtime, _input_state_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");
        lua_pushcfunction(engine->runtime, _input_state_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);

    // Create KEY table metatable
    if (luaL_newmetatable(engine->runtime, INPUT_STATE_PROXY_META "_KEY")) {
        lua_pushstring(engine->runtime, INPUT_STATE_PROXY_META "_KEY");
        lua_setfield(engine->runtime, -2, "__name");
        lua_pushcfunction(engine->runtime, _input_state_key_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _input_state_readonly_error);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
}

void ese_input_state_lua_push(EseInputState *input) {
    log_assert("INPUT_STATE", input, "ese_input_state_lua_push called with NULL input");

    if (input->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseInputState **ud = (EseInputState **)lua_newuserdata(input->state, sizeof(EseInputState *));
        *ud = input;

        // Attach metatable
        luaL_getmetatable(input->state, INPUT_STATE_PROXY_META);
        lua_setmetatable(input->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(input->state, LUA_REGISTRYINDEX, input->lua_ref);
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
    log_assert("INPUT_STATE", input->state, "ese_input_state_ref called with C only input");
    
    if (input->lua_ref == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseInputState **ud = (EseInputState **)lua_newuserdata(input->state, sizeof(EseInputState *));
        *ud = input;

        // Attach metatable
        luaL_getmetatable(input->state, INPUT_STATE_PROXY_META);
        lua_setmetatable(input->state, -2);

        // Store hard reference to prevent garbage collection
        input->lua_ref = luaL_ref(input->state, LUA_REGISTRYINDEX);
        input->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        input->lua_ref_count++;
    }

    profile_count_add("ese_input_state_ref_count");
}

void ese_input_state_unref(EseInputState *input) {
    if (!input) return;
    log_assert("INPUT_STATE", input->state, "ese_input_state_unref called with C only input");

    if (input->lua_ref != LUA_NOREF && input->lua_ref_count > 0) {
        input->lua_ref_count--;
        
        if (input->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(input->state, LUA_REGISTRYINDEX, input->lua_ref);
            input->lua_ref = LUA_NOREF;
        }
    }

    profile_count_add("ese_input_state_unref_count");
}
