#include "types/input_state.h"
#include "types/input_state_lua.h"
#include "types/input_state_private.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <string.h>
#include <stdio.h>

// External declaration for key names array
extern const char *const input_state_key_names[];

// ========================================
// PRIVATE LUA HELPER FUNCTIONS
// ========================================

// Lua helper functions
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
    lua_pushboolean(L, arr[key_idx]);
    return 1;
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
static int _input_state_mouse_down_index(lua_State *L) {
    EseInputState *input = (EseInputState *)lua_touserdata(L, lua_upvalueindex(1));
    int btn = (int)luaL_checkinteger(L, 2);
    if (btn < 0 || btn >= MOUSE_BUTTON_COUNT) {
        return luaL_error(L, "Invalid mouse button index");
    }
    lua_pushboolean(L, input->mouse_down[btn]);
    return 1;
}

static int _input_state_mouse_clicked_index(lua_State *L) {
    EseInputState *input = (EseInputState *)lua_touserdata(L, lua_upvalueindex(1));
    int btn = (int)luaL_checkinteger(L, 2);
    if (btn < 0 || btn >= MOUSE_BUTTON_COUNT) {
        return luaL_error(L, "Invalid mouse button index");
    }
    lua_pushboolean(L, input->mouse_clicked[btn]);
    return 1;
}

static int _input_state_mouse_released_index(lua_State *L) {
    EseInputState *input = (EseInputState *)lua_touserdata(L, lua_upvalueindex(1));
    int btn = (int)luaL_checkinteger(L, 2);
    if (btn < 0 || btn >= MOUSE_BUTTON_COUNT) {
        return luaL_error(L, "Invalid mouse button index");
    }
    lua_pushboolean(L, input->mouse_released[btn]);
    return 1;
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
    const char *key_name = lua_tostring(L, 2);
    if (!key_name) {
        return luaL_error(L, "Key name must be a string");
    }
    
    // Find the key name in the array
    for (int i = 0; i < InputKey_MAX; i++) {
        if (strcmp(input_state_key_names[i], key_name) == 0) {
            lua_pushinteger(L, i);
            return 1;
        }
    }
    
    return luaL_error(L, "Unknown key name: %s", key_name);
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
    return luaL_error(L, "Input tables are read-only");
}

// ========================================
// PRIVATE LUA FUNCTIONS
// ========================================

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
static int _input_state_lua_gc(lua_State *L) {
    // Get from userdata
    EseInputState **ud = (EseInputState **)luaL_testudata(L, 1, INPUT_STATE_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseInputState *input = *ud;
    if (input) {
        // If lua_ref == LUA_NOREF, there are no more references to this input, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this input was referenced from C and should not be freed.
        if (ese_input_state_get_lua_ref(input) == LUA_NOREF) {
            ese_input_state_destroy(input);
        }
    }

    return 0;
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
    const char *key = lua_tostring(L, 2);
    if (!input || !key) {
        profile_cancel(PROFILE_LUA_INPUT_STATE_INDEX);
        return 0;
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
        return 1;
    }

    // mouse_x, mouse_y, mouse_scroll_dx, mouse_scroll_dy
    if (strcmp(key, "mouse_x") == 0) {
        lua_pushinteger(L, ese_input_state_get_mouse_x(input));
        profile_stop(PROFILE_LUA_INPUT_STATE_INDEX, "input_state_lua_index (mouse_x)");
        return 1;
    }
    if (strcmp(key, "mouse_y") == 0) {
        lua_pushinteger(L, ese_input_state_get_mouse_y(input));
        profile_stop(PROFILE_LUA_INPUT_STATE_INDEX, "input_state_lua_index (mouse_y)");
        return 1;
    }
    if (strcmp(key, "mouse_scroll_dx") == 0) {
        lua_pushinteger(L, ese_input_state_get_mouse_scroll_dx(input));
        profile_stop(PROFILE_LUA_INPUT_STATE_INDEX, "input_state_lua_index (mouse_scroll_dx)");
        return 1;
    }
    if (strcmp(key, "mouse_scroll_dy") == 0) {
        lua_pushinteger(L, ese_input_state_get_mouse_scroll_dy(input));
        profile_stop(PROFILE_LUA_INPUT_STATE_INDEX, "input_state_lua_index (mouse_scroll_dy)");
        return 1;
    }

    // mouse_buttons table proxy (read-only)
    if (strcmp(key, "mouse_down") == 0) {
        // Create and set the metatable
        lua_newtable(L);
        
        // Set __index closure with the input pointer as upvalue
        lua_pushlightuserdata(L, input);
        lua_pushcclosure(L, _input_state_mouse_down_index, 1);
        lua_setfield(L, -2, "__index");
        
        // Set __newindex to error
        lua_pushcfunction(L, _input_state_readonly_error);
        lua_setfield(L, -2, "__newindex");
        
        // Apply metatable to the table
        lua_setmetatable(L, -2);

        profile_stop(PROFILE_LUA_INPUT_STATE_INDEX, "input_state_lua_index (mouse_down)");
        return 1;
    }
    if (strcmp(key, "mouse_clicked") == 0) {
        // Create and set the metatable
        lua_newtable(L);
        
        // Set __index closure with the input pointer as upvalue
        lua_pushlightuserdata(L, input);
        lua_pushcclosure(L, _input_state_mouse_clicked_index, 1);
        lua_setfield(L, -2, "__index");
        
        // Set __newindex to error
        lua_pushcfunction(L, _input_state_readonly_error);
        lua_setfield(L, -2, "__newindex");
        
        // Apply metatable to the table
        lua_setmetatable(L, -2);

        profile_stop(PROFILE_LUA_INPUT_STATE_INDEX, "input_state_lua_index (mouse_clicked)");
        return 1;
    }
    if (strcmp(key, "mouse_released") == 0) {
        // Create and set the metatable
        lua_newtable(L);
        
        // Set __index closure with the input pointer as upvalue
        lua_pushlightuserdata(L, input);
        lua_pushcclosure(L, _input_state_mouse_released_index, 1);
        lua_setfield(L, -2, "__index");
        
        // Set __newindex to error
        lua_pushcfunction(L, _input_state_readonly_error);
        lua_setfield(L, -2, "__newindex");
        
        // Apply metatable to the table
        lua_setmetatable(L, -2);

        profile_stop(PROFILE_LUA_INPUT_STATE_INDEX, "input_state_lua_index (mouse_released)");
        return 1;
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
        return 1;
    }

    profile_stop(PROFILE_LUA_INPUT_STATE_INDEX, "input_state_lua_index (invalid)");
    return 0;
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
    return luaL_error(L, "Input object is read-only");
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
        lua_pushstring(L, "Input: (invalid)");
        return 1;
    }
    char buf[1024];
    snprintf(
        buf, sizeof(buf),
        "Input: %p (mouse_x=%d, mouse_y=%d mouse_scroll_dx=%d, mouse_scroll_dy=%d)\n"
        "Down: %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
        (void *)input, ese_input_state_get_mouse_x(input), ese_input_state_get_mouse_y(input),
        ese_input_state_get_mouse_scroll_dx(input), ese_input_state_get_mouse_scroll_dy(input),
        ese_input_state_get_key_down(input, InputKey_A) ? 'A': ' ',
        ese_input_state_get_key_down(input, InputKey_B) ? 'B': ' ',
        ese_input_state_get_key_down(input, InputKey_C) ? 'C': ' ',
        ese_input_state_get_key_down(input, InputKey_D) ? 'D': ' ',
        ese_input_state_get_key_down(input, InputKey_E) ? 'E': ' ',
        ese_input_state_get_key_down(input, InputKey_F) ? 'F': ' ',
        ese_input_state_get_key_down(input, InputKey_G) ? 'G': ' ',
        ese_input_state_get_key_down(input, InputKey_H) ? 'H': ' ',
        ese_input_state_get_key_down(input, InputKey_I) ? 'I': ' ',
        ese_input_state_get_key_down(input, InputKey_J) ? 'J': ' ',
        ese_input_state_get_key_down(input, InputKey_K) ? 'K': ' ',
        ese_input_state_get_key_down(input, InputKey_L) ? 'L': ' ',
        ese_input_state_get_key_down(input, InputKey_M) ? 'M': ' ',
        ese_input_state_get_key_down(input, InputKey_N) ? 'N': ' ',
        ese_input_state_get_key_down(input, InputKey_O) ? 'O': ' ',
        ese_input_state_get_key_down(input, InputKey_P) ? 'P': ' ',
        ese_input_state_get_key_down(input, InputKey_Q) ? 'Q': ' ',
        ese_input_state_get_key_down(input, InputKey_R) ? 'R': ' ',
        ese_input_state_get_key_down(input, InputKey_S) ? 'S': ' ',
        ese_input_state_get_key_down(input, InputKey_T) ? 'T': ' ',
        ese_input_state_get_key_down(input, InputKey_U) ? 'U': ' ',
        ese_input_state_get_key_down(input, InputKey_V) ? 'V': ' ',
        ese_input_state_get_key_down(input, InputKey_W) ? 'W': ' ',
        ese_input_state_get_key_down(input, InputKey_X) ? 'X': ' ',
        ese_input_state_get_key_down(input, InputKey_Y) ? 'Y': ' ',
        ese_input_state_get_key_down(input, InputKey_Z) ? 'Z': ' ',
        ese_input_state_get_key_down(input, InputKey_0) ? '0': ' ',
        ese_input_state_get_key_down(input, InputKey_1) ? '1': ' ',
        ese_input_state_get_key_down(input, InputKey_2) ? '2': ' ',
        ese_input_state_get_key_down(input, InputKey_3) ? '3': ' ',
        ese_input_state_get_key_down(input, InputKey_4) ? '4': ' ',
        ese_input_state_get_key_down(input, InputKey_5) ? '5': ' ',
        ese_input_state_get_key_down(input, InputKey_6) ? '6': ' ',
        ese_input_state_get_key_down(input, InputKey_7) ? '7': ' ',
        ese_input_state_get_key_down(input, InputKey_8) ? '8': ' ',
        ese_input_state_get_key_down(input, InputKey_9) ? '9': ' ',
        ese_input_state_get_key_down(input, InputKey_LSHIFT) ? '<': ' ',
        ese_input_state_get_key_down(input, InputKey_RSHIFT) ? '>': ' '
    );
    lua_pushstring(L, buf);
    return 1;
}

// ========================================
// PUBLIC LUA FUNCTIONS
// ========================================

/**
 * @brief Initializes the Input userdata type in the Lua state.
 * 
 * @details This function registers the 'InputStateProxyMeta' metatable in the Lua registry,
 * which defines how the `EseInputState` C object interacts with Lua scripts.
 * 
 * @param engine A pointer to the `EseLuaEngine` where the Input type will be registered.
 * @return void
 * 
 * @note This function should be called once during engine initialization.
 */
void _ese_input_state_lua_init(EseLuaEngine *engine) {
    log_assert("INPUT_STATE", engine, "_ese_input_state_lua_init called with NULL engine");
    log_assert("INPUT_STATE", engine->runtime, "_ese_input_state_lua_init called with NULL engine->runtime");

    // Create main metatable
    lua_engine_new_object_meta(engine, INPUT_STATE_PROXY_META, 
        _input_state_lua_index, 
        _input_state_lua_newindex, 
        _input_state_lua_gc, 
        _input_state_lua_tostring);

    // Create KEY table metatable
    lua_engine_new_object_meta(engine, INPUT_STATE_PROXY_META "_KEY", 
        _input_state_key_index, 
        _input_state_readonly_error, 
        NULL, 
        NULL);
}
