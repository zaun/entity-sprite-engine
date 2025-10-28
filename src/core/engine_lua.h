/**
 * @file engine_lua.h
 *
 * @brief This file contains the declarations for the functions that expose
 * engine functionality to the Lua scripting environment.
 *
 * @details The functions declared here are used to bridge the C engine code
 * with Lua, allowing scripts to interact with engine systems like printing and
 * asset management.
 */
#ifndef ESE_ENGINE_LUA_H
#define ESE_ENGINE_LUA_H

typedef struct lua_State lua_State;

/**
 * @brief Prints a message to the engine's log.
 *
 * @details This function is the Lua-exposed equivalent of a print function,
 * formatting multiple arguments into a single log message.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 0, as the function doesn't return any values to Lua.
 *
 * @note This function asserts that the Lua state `L` is not NULL.
 */
int _lua_print(lua_State *L);

/**
 * @brief Loads a script file via the asset manager.
 *
 * @details This function takes a script path as a string argument from Lua,
 * loads the script using the engine's asset manager, and returns a boolean
 * indicating success or failure.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, pushing a boolean value onto the Lua stack.
 *
 * @note This function validates that it receives exactly one string argument.
 */
int _lua_asset_load_script(lua_State *L);

/**
 * @brief Loads a sprite atlas file via the asset manager.
 *
 * @details This function takes an atlas path as a string argument from Lua,
 * loads it into the asset manager, and returns a boolean indicating success or
 * failure.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, pushing a boolean value onto the Lua stack.
 *
 * @note This function validates that it receives exactly one string argument.
 */
int _lua_asset_load_atlas(lua_State *L);

/**
 * @brief Loads a shader file into the renderer.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, pushing a boolean value onto the Lua stack.
 *
 * @note This function validates that it receives exactly one string argument.
 */
int _lua_asset_load_shader(lua_State *L);

int _lua_asset_load_map(lua_State *L);

int _lua_asset_get_map(lua_State *L);

int _lua_set_pipeline(lua_State *L);

int _lua_detect_collision(lua_State *L);

int _lua_scene_clear(lua_State *L);

int _lua_scene_reset(lua_State *L);

#endif // ESE_ENGINE_LUA_H
