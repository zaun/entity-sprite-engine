#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "types/input_state.h"

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

/**
 * @private
 * @brief An error function for the Lua metatable's `__newindex` event.
 * 
 * @details This function is used to ensure that Lua scripts cannot modify read-only tables
 * like the keys or mouse button state arrays.
 * 
 * @param L The Lua state.
 * @return An error with a message indicating the table is read-only.
 */
static int _input_state_readonly_error(lua_State *L) {
    return luaL_error(L, "Input tables are read-only");
}

/**
 * @private
 * @brief A closure to get key state by index for Lua.
 * 
 * @details This closure is bound to the `keys_down`, `keys_pressed`, and `keys_released` tables
 * in Lua, providing a read-only indexer to access the C arrays.
 * 
 * @param L The Lua state.
 * @return A boolean value on the Lua stack.
 * 
 * @note The key array is passed as an upvalue to this function.
 */
static int _input_state_keys_index(lua_State *L) {
    bool *arr = (bool *)lua_touserdata(L, lua_upvalueindex(1));
    int key_idx = (int)luaL_checkinteger(L, 2);
    log_assert("INPUT_STATE", (key_idx >= 0 && key_idx < InputKey_MAX), "Invalid key index");
    lua_pushboolean(L, arr[key_idx]);
    return 1;
}

/**
 * @private
 * @brief A closure to get mouse button state by index for Lua.
 * 
 * @details This closure is bound to the `mouse_buttons` table in Lua, providing a read-only
 * indexer to access the C array.
 * 
 * @param L The Lua state.
 * @return A boolean value on the Lua stack.
 * 
 * @note The `EseInputState` pointer is passed as an upvalue to this function.
 */
static int _input_state_mouse_buttons_index(lua_State *L) {
    EseInputState *input = (EseInputState *)lua_touserdata(L, lua_upvalueindex(1));
    int btn = (int)luaL_checkinteger(L, 2);
    if (btn < 0 || btn >= MOUSE_BUTTON_COUNT) {
        return luaL_error(L, "Invalid mouse button index");
    }
    lua_pushboolean(L, input->mouse_buttons[btn]);
    return 1;
}

/**
 * @private
 * @brief Registers a EseInputState object with LUA.
 * 
 * @param input A pointer to the `EseInputState` object to be pushed.
 * @param is_lua_owned A boolean flag indicating if Lua owns the memory for this object.
 * @return void
 * 
 * @note This function handles the creation of a Lua reference to avoid memory leaks.
 */
static void _input_state_lua_register(EseInputState *input, bool is_lua_owned) {
    log_assert("INPUT_STATE", input, "_input_state_lua_register called with NULL input");
    log_assert("INPUT_STATE", input->lua_ref == LUA_NOREF, "_input_state_lua_register input is already registered");

    lua_newtable(input->state);
    lua_pushlightuserdata(input->state, input);
    lua_setfield(input->state, -2, "__ptr");

    // Store the ownership flag
    lua_pushboolean(input->state, is_lua_owned);
    lua_setfield(input->state, -2, "__is_lua_owned");

    luaL_getmetatable(input->state, "InputProxyMeta");
    lua_setmetatable(input->state, -2);

    // Store a reference to this proxy table in the Lua registry
    input->lua_ref = luaL_ref(input->state, LUA_REGISTRYINDEX);
}

/**
 * @private
 * @brief Extracts an EseInputState pointer from a Lua userdata object.
 * 
 * @details This function validates that the Lua object at a given stack index is a valid
 * `EseInputState` proxy and returns the underlying C pointer.
 * 
 * @param L A pointer to the Lua state.
 * @param idx The stack index of the Lua Input object.
 * @return A pointer to the `EseInputState` object, or `NULL` if the object is invalid.
 * 
 * @note This function should be used to safely retrieve the C object from a Lua context.
 */
static EseInputState *_input_state_lua_get(lua_State *L, int idx) {
    if (!lua_istable(L, idx)) return NULL;

    if (!lua_getmetatable(L, idx)) return NULL;

    luaL_getmetatable(L, "InputProxyMeta");
    if (!lua_rawequal(L, -1, -2)) {
        lua_pop(L, 2);
        return NULL;
    }
    lua_pop(L, 2);

    lua_getfield(L, idx, "__ptr");
    if (!lua_islightuserdata(L, -1)) {
        lua_pop(L, 1);
        return NULL;
    }
    void *ptr = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return (EseInputState *)ptr;
}

/**
 * @private
 * @brief A metatable `__index` function for the EseInputState Lua proxy.
 * 
 * @details This function handles access to the members of the `EseInputState` C object from Lua.
 * It returns sub-tables for key and button states, and integer values for mouse coordinates.
 * 
 * @param L The Lua state.
 * @return The value of the requested member.
 */
static int _input_state_lua_index(lua_State *L) {
    EseInputState *input = _input_state_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!input || !key) return 0;

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

        return 1;
    }

    // mouse_x, mouse_y, mouse_scroll_dx, mouse_scroll_dy
    if (strcmp(key, "mouse_x") == 0) {
        lua_pushinteger(L, input->mouse_x);
        return 1;
    }
    if (strcmp(key, "mouse_y") == 0) {
        lua_pushinteger(L, input->mouse_y);
        return 1;
    }
    if (strcmp(key, "mouse_scroll_dx") == 0) {
        lua_pushinteger(L, input->mouse_scroll_dx);
        return 1;
    }
    if (strcmp(key, "mouse_scroll_dy") == 0) {
        lua_pushinteger(L, input->mouse_scroll_dy);
        return 1;
    }

    // mouse_buttons table proxy (read-only)
    if (strcmp(key, "mouse_buttons") == 0) {
        // Create the table
        lua_newtable(L);

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

        return 1;
    }

    // KEY table with constants
    if (strcmp(key, "KEY") == 0) {
        lua_newtable(L);
        for (int i = 0; i < InputKey_MAX; i++) {
            lua_pushinteger(L, i);
            lua_setfield(L, -2, input_state_key_names[i]);
        }
        return 1;
    }

    return 0;
}

/**
 * @private
 * @brief A metatable `__newindex` function for the EseInputState Lua proxy.
 * 
 * @details This function prevents any modification of the `EseInputState` object from Lua,
 * ensuring it remains a read-only representation of the current input.
 * 
 * @param L The Lua state.
 * @return An error with a message indicating the object is read-only.
 */
static int _input_state_lua_newindex(lua_State *L) {
    return luaL_error(L, "Input object is read-only");
}

/**
 * @private
 * @brief A metatable `__gc` (garbage collection) function for the EseInputState Lua proxy.
 * 
 * @details This function is responsible for correctly handling the memory of the
 * `EseInputState` C object when its Lua proxy is garbage collected. It only frees the
 * C memory if the `is_lua_owned` flag is true.
 * 
 * @param L The Lua state.
 * @return The number of results pushed onto the stack (0).
 */
static int _input_state_lua_gc(lua_State *L) {
    EseInputState *state = _input_state_lua_get(L, 1);

    if (state) {
        lua_getfield(L, 1, "__is_lua_owned");
        bool is_lua_owned = lua_toboolean(L, -1);
        lua_pop(L, 1);

        if (is_lua_owned) {
            input_state_destroy(state);
            log_debug("LUA_GC", "InputState object (Lua-owned) garbage collected and C memory freed.");
        } else {
            log_debug("LUA_GC", "InputState object (C-owned) garbage collected, C memory *not* freed.");
        }
    }

    return 0;
}

/**
 * @private
 * @brief A metatable `__tostring` function for the EseInputState Lua proxy.
 * 
 * @details This function provides a string representation of the `EseInputState` object
 * for easy debugging and printing in Lua.
 * 
 * @param L The Lua state.
 * @return A string representation of the `EseInputState`.
 */
static int _input_state_lua_tostring(lua_State *L) {
    EseInputState *input = _input_state_lua_get(L, 1);
    if (!input) {
        lua_pushstring(L, "Input: (invalid)");
        return 1;
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
    lua_pushstring(L, buf);
    return 1;
}

void input_state_lua_init(EseLuaEngine *engine) {
    log_assert("INPUT_STATE", engine, "input_state_lua_init called with NULL engine");
    log_assert("INPUT_STATE", engine->runtime, "input_state_lua_init called with NULL engine->runtime");

    if (luaL_newmetatable(engine->runtime, "InputProxyMeta")) {
        log_debug("LUA", "Adding entity InputProxyMeta to engine");
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
}

EseInputState *input_state_create(EseLuaEngine *engine) {
    log_assert("INPUT_STATE", memory_manager.malloc, "memory_manager.malloc is NULL");

    EseInputState *input = (EseInputState *)memory_manager.malloc(sizeof(EseInputState), MMTAG_GENERAL);
    log_assert("INPUT_STATE", input, "input_state_create failed to allocate memory");

    memset(input->keys_down, 0, sizeof(input->keys_down));
    memset(input->keys_pressed, 0, sizeof(input->keys_pressed));
    memset(input->keys_released, 0, sizeof(input->keys_released));
    memset(input->mouse_buttons, 0, sizeof(input->mouse_buttons));

    input->mouse_x = 0;
    input->mouse_y = 0;
    input->mouse_scroll_dx = 0;
    input->mouse_scroll_dy = 0;

    if (engine) {
        input->state = engine->runtime;
        input->lua_ref = LUA_NOREF;

        _input_state_lua_register(input, false);
    }

    return input;
}

EseInputState *input_state_copy(const EseInputState *src, EseLuaEngine *engine) {
    log_assert("INPUT_STATE", src, "input_state_copy called with NULL src");
    log_assert("INPUT_STATE", engine, "input_state_copy called with NULL engine");
    log_assert("INPUT_STATE", memory_manager.malloc, "memory_manager.malloc is NULL");

    EseInputState *copy = (EseInputState *)memory_manager.malloc(sizeof(EseInputState), MMTAG_GENERAL);
    log_assert("INPUT_STATE", copy, "input_state_copy failed to allocate memory");

    memcpy(copy->keys_down, src->keys_down, sizeof(src->keys_down));
    memcpy(copy->keys_pressed, src->keys_pressed, sizeof(src->keys_pressed));
    memcpy(copy->keys_released, src->keys_released, sizeof(src->keys_released));
    memcpy(copy->mouse_buttons, src->mouse_buttons, sizeof(src->mouse_buttons));

    copy->mouse_x = src->mouse_x;
    copy->mouse_y = src->mouse_y;
    copy->mouse_scroll_dx = src->mouse_scroll_dx;
    copy->mouse_scroll_dy = src->mouse_scroll_dy;

    copy->state = engine->runtime;
    copy->lua_ref = LUA_NOREF;

    _input_state_lua_register(copy, false);

    return copy;
}

void input_state_destroy(EseInputState *input) {
    if (input) {
        if (input->lua_ref != LUA_NOREF) {
            luaL_unref(input->state, LUA_REGISTRYINDEX, input->lua_ref);
        }
        memory_manager.free(input);
    }
}

static void input_state_lua_push(EseInputState *input) {
    log_assert("INPUT_STATE", input, "input_state_lua_push called with NULL input");
    log_assert("INPUT_STATE", input->lua_ref -= LUA_NOREF, "input_state_lua_push input not registered with lua");

    lua_rawgeti(input->state, LUA_REGISTRYINDEX, input->lua_ref);
}
