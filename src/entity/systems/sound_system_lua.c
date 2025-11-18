#include "entity/systems/sound_system_private.h"
#include "entity/systems/sound_system.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "vendor/lua/src/lauxlib.h"
#include "vendor/lua/src/lua.h"

/**
 * Lua binding for the sound system global state.
 *
 * Exposes a read-only global table `Sound` with a `devices` field:
 *   - `#Sound.devices` returns the total number of playback devices.
 *   - `Sound.devices[idx]` returns the device name (1-based index).
 *   - `Sound.devices[idx] = ...` raises an error (read-only).
 */

// ----------------------------------------
// devices helpers
// ----------------------------------------

static int _sound_devices_newindex(lua_State *L) {
    (void)L;
    return luaL_error(L, "Sound.devices is read-only");
}

// ----------------------------------------
// public init
// ----------------------------------------

void sound_system_lua_init(EseLuaEngine *engine) {
    log_assert("SOUND_LUA", engine, "sound_system_lua_init called with NULL engine");
    log_assert("SOUND_LUA", engine->runtime,
               "sound_system_lua_init called with NULL engine->runtime");

    lua_State *L = engine->runtime;

    // Create or reuse global Sound table
    lua_getglobal(L, "Sound");
    if (lua_isnil(L, -1)) {
        // Pop nil and create new Sound table
        lua_pop(L, 1);
        lua_newtable(L); // Sound

        // Create devices table and populate with device names so that
        // the built-in length operator (#) reflects the number of
        // devices without relying on __len metamethods.
        lua_newtable(L); // Sound.devices

        if (g_sound_system_data && g_sound_system_data->ready &&
            g_sound_system_data->device_infos &&
            g_sound_system_data->device_info_count > 0) {
            for (ma_uint32 i = 0; i < g_sound_system_data->device_info_count; i++) {
                const char *name = g_sound_system_data->device_infos[i].name;
                if (name) {
                    lua_pushstring(L, name);
                } else {
                    lua_pushnil(L);
                }
                lua_rawseti(L, -2, (int)i + 1);
            }
        }

        // Metatable for devices table: make it read-only while keeping
        // the array contents intact for #Sound.devices.
        lua_newtable(L);

        // __newindex -> error (read-only)
        lua_pushcfunction(L, _sound_devices_newindex);
        lua_setfield(L, -2, "__newindex");

        // Hide metatable from Lua scripts
        lua_pushstring(L, "locked");
        lua_setfield(L, -2, "__metatable");

        // Attach metatable to devices table
        lua_setmetatable(L, -2); // setmetatable(Sound.devices, mt)

        // Attach devices table to Sound
        lua_setfield(L, -2, "devices"); // Sound.devices = devices

        // Optionally lock Sound itself by hiding its metatable to prevent
        // tampering from Lua. The engine's global lock already prevents
        // adding/removing globals, but we keep this local.
        lua_newtable(L);
        lua_pushstring(L, "locked");
        lua_setfield(L, -2, "__metatable");
        lua_setmetatable(L, -2);

        // Set global Sound
        lua_setglobal(L, "Sound");

        log_debug("SOUND_LUA", "Sound global created with read-only devices list");
    } else {
        // Sound already exists (should not happen in normal engine startup),
        // just pop and log.
        lua_pop(L, 1);
        log_debug("SOUND_LUA", "Sound global already exists; skipping initialization");
    }
}
