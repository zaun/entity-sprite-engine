#include <stdint.h>
#include "core/engine.h"
#include "graphics/gui_lua.h"
#include "graphics/gui_private.h"
#include "graphics/gui.h"
#include "utility/log.h"
#include "scripting/lua_engine.h"

// Private functions
static int _ese_gui_lua_begin(lua_State *L);
static int _ese_gui_lua_end(lua_State *L);
static int _ese_gui_lua_open_flex(lua_State *L);
static int _ese_gui_lua_close_flex(lua_State *L);
static int _ese_gui_lua_open_box(lua_State *L);
static int _ese_gui_lua_close_box(lua_State *L);
static int _ese_gui_lua_push_button(lua_State *L);
static int _ese_gui_lua_push_image(lua_State *L);

void ese_gui_lua_init(EseLuaEngine *engine) {
    log_assert("GUI_LUA", engine, "ese_gui_lua_init called with NULL engine");
    
    log_debug("GUI_LUA", "Initializing GUI Lua bindings");
    
    // Create global GUI table with constructor
    lua_getglobal(engine->runtime, "GUI");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);

        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _ese_gui_lua_begin);
        lua_setfield(engine->runtime, -2, "start");
        lua_pushcfunction(engine->runtime, _ese_gui_lua_end);
        lua_setfield(engine->runtime, -2, "finish"); // end is a reserved keyword
        lua_pushcfunction(engine->runtime, _ese_gui_lua_open_flex);
        lua_setfield(engine->runtime, -2, "open_flex");
        lua_pushcfunction(engine->runtime, _ese_gui_lua_close_flex);
        lua_setfield(engine->runtime, -2, "close_flex");
        lua_pushcfunction(engine->runtime, _ese_gui_lua_open_box);
        lua_setfield(engine->runtime, -2, "open_box");
        lua_pushcfunction(engine->runtime, _ese_gui_lua_close_box);
        lua_setfield(engine->runtime, -2, "close_box");
        lua_pushcfunction(engine->runtime, _ese_gui_lua_push_button);
        lua_setfield(engine->runtime, -2, "push_button");
        lua_pushcfunction(engine->runtime, _ese_gui_lua_push_image);
        lua_setfield(engine->runtime, -2, "push_image");

        // Create DIRECTION table
        lua_newtable(engine->runtime);
        lua_pushinteger(engine->runtime, FLEX_DIRECTION_ROW);
        lua_setfield(engine->runtime, -2, "ROW");
        lua_pushinteger(engine->runtime, FLEX_DIRECTION_COLUMN);
        lua_setfield(engine->runtime, -2, "COLUMN");
        lua_setfield(engine->runtime, -2, "DIRECTION");

        // Create JUSTIFY table
        lua_newtable(engine->runtime);
        lua_pushinteger(engine->runtime, FLEX_JUSTIFY_START);
        lua_setfield(engine->runtime, -2, "START");
        lua_pushinteger(engine->runtime, FLEX_JUSTIFY_CENTER);
        lua_setfield(engine->runtime, -2, "CENTER");
        lua_pushinteger(engine->runtime, FLEX_JUSTIFY_END);
        lua_setfield(engine->runtime, -2, "END");
        lua_setfield(engine->runtime, -2, "JUSTIFY");

        // Create ALIGN table
        lua_newtable(engine->runtime);
        lua_pushinteger(engine->runtime, FLEX_ALIGN_ITEMS_START);
        lua_setfield(engine->runtime, -2, "START");
        lua_pushinteger(engine->runtime, FLEX_ALIGN_ITEMS_CENTER);
        lua_setfield(engine->runtime, -2, "CENTER");
        lua_pushinteger(engine->runtime, FLEX_ALIGN_ITEMS_END);
        lua_setfield(engine->runtime, -2, "END");
        lua_setfield(engine->runtime, -2, "ALIGN");

        // Lock GUI table
        lua_newtable(engine->runtime);
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
        lua_setmetatable(engine->runtime, -2);

        lua_setglobal(engine->runtime, "GUI");
        log_debug("GUI_LUA", "GUI table created and set globally");
    } else {
        lua_pop(engine->runtime, 1);
        log_debug("GUI_LUA", "GUI table already exists");
    }
}

static int _ese_gui_lua_begin(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_begin called with NULL Lua state");

    uint64_t z_index = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    int n_args = lua_gettop(L);
    if (n_args == 5) {
        if (lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER || lua_type(L, 3) != LUA_TNUMBER || 
            lua_type(L, 4) != LUA_TNUMBER || lua_type(L, 5) != LUA_TNUMBER) {
            return luaL_error(L, "all arguments must be numbers");
        }
        z_index = (uint64_t)lua_tonumber(L, 1);
        x = (int)lua_tonumber(L, 2);
        y = (int)lua_tonumber(L, 3);
        width = (int)lua_tonumber(L, 4);
        height = (int)lua_tonumber(L, 5);
    } else {
        return luaL_error(L, "GUI.begin() takes 5 arguments (draw_order, x, y, width, height)");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout != NULL) {
        return luaL_error(L, "GUI.begin() called while another GUI is active");
    }
    
    ese_gui_begin(gui, z_index, x, y, width, height);

    return 0;
}

static int _ese_gui_lua_end(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_end called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 0) {
        return luaL_error(L, "GUI.end() takes no arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout == NULL) {
        return luaL_error(L, "GUI.end() called with no open GUI active");
    }

    ese_gui_end(gui);

    return 0;
}

static int _ese_gui_lua_open_flex(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_open_flex called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 1) {
        return luaL_error(L, "open_flex(options) takes 1 argument");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout == NULL) {
        return luaL_error(L, "GUI.open_flex() called with no open GUI active");
    }

    // Get options table
    luaL_checktype(L, 1, LUA_TTABLE);  
    
    int spacing = 0;
    lua_getfield(L, 1, "spacing");
    spacing = luaL_optinteger(L, -1, 0);
    
    int padding_left = 0;
    lua_getfield(L, 1, "padding_left");
    padding_left = luaL_optinteger(L, -1, 0);
    
    int padding_top = 0;
    lua_getfield(L, 1, "padding_top");
    padding_top = luaL_optinteger(L, -1, 0);
    
    int padding_right = 0;
    lua_getfield(L, 1, "padding_right");
    padding_right = luaL_optinteger(L, -1, 0);
    
    int padding_bottom = 0;
    lua_getfield(L, 1, "padding_bottom");
    padding_bottom = luaL_optinteger(L, -1, 0);

    int direction = FLEX_DIRECTION_ROW;
    lua_getfield(L, 1, "direction");
    direction = luaL_optinteger(L, -1, FLEX_DIRECTION_ROW);
    if (direction < 0 || direction >= ESE_GUI_FLEX_DIRECTION_MAX) {
        return luaL_error(L, "invalid direction");
    }

    int align_items = FLEX_ALIGN_ITEMS_START;
    lua_getfield(L, 1, "align_items");
    align_items = luaL_optinteger(L, -1, FLEX_ALIGN_ITEMS_START);
    if (align_items < 0 || align_items >= ESE_GUI_FLEX_ALIGN_ITEMS_MAX) {
        return luaL_error(L, "invalid align_items");
    }
    
    int justify = FLEX_JUSTIFY_START;
    lua_getfield(L, 1, "justify");
    justify = luaL_optinteger(L, -1, FLEX_JUSTIFY_START);
    if (justify < 0 || justify >= ESE_GUI_FLEX_JUSTIFY_MAX) {
        return luaL_error(L, "invalid justify");
    }

    EseColor *background_color = NULL;
    lua_getfield(L, 1, "background_color");
    background_color = ese_color_lua_get(L, -1);

    ese_gui_open_flex(gui, direction, justify, align_items, spacing,
        padding_left, padding_top, padding_right, padding_bottom,
        background_color);

    return 0;
}

static int _ese_gui_lua_close_flex(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_close_flex called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 0) {
        return luaL_error(L, "GUI.close_flex() takes no arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout->current_container == NULL || gui->open_layout->current_container->widget_type != ESE_GUI_WIDGET_FLEX) {
        return luaL_error(L, "GUI.close_flex() called with no open FLEX containers");
    }

    ese_gui_close_flex(gui);

    return 0;
}

static int _ese_gui_lua_open_box(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_open_box called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 2 && n_args != 3) {
        if (lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER) {
            return luaL_error(L, "width and height must be numbers");
        }
        return luaL_error(L, "GUI.open_box(width, height[, options]) takes 2 or 3 arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout == NULL) {
        return luaL_error(L, "GUI.open_box() called with no open GUI active");
    }
    

    int width = 0;
    int height = 0;
    lua_getfield(L, 1, "width");
    width = luaL_optinteger(L, -1, 0);
    lua_getfield(L, 1, "height");
    height = luaL_optinteger(L, -1, 0);

    // Get options table
    luaL_checktype(L, 2, LUA_TTABLE);  

    EseColor *background_color = NULL;
    lua_getfield(L, 2, "background_color");
    background_color = ese_color_lua_get(L, -1);

    ese_gui_open_box(gui, width, height, background_color);

    return 0;
}

static int _ese_gui_lua_close_box(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_close_box called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 0) {
        return luaL_error(L, "GUI.close_box() takes no arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout->current_container == NULL || gui->open_layout->current_container->widget_type != ESE_GUI_WIDGET_BOX) {
        return luaL_error(L, "GUI.close_box() called with no open BOX containers");
    }

    ese_gui_close_box(gui);

    return 0;
}

static int _ese_gui_lua_push_button(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_push_button called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 2) {
        return luaL_error(L, "GUI.push_button(text, callback) takes 2 arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout == NULL) {
        return luaL_error(L, "GUI.push_button() called with no open GUI active");
    }

    return 0;
}

static int _ese_gui_lua_push_image(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_push_image called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 2) {
        return luaL_error(L, "GUI.push_image(sprite_id, fit) takes 2 arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout == NULL) {
        return luaL_error(L, "GUI.push_button() called with no open GUI active");
    }

    return 0;
}
