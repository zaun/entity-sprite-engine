#include <stdint.h>
#include "core/engine.h"
#include "core/memory_manager.h"
#include "graphics/gui_lua.h"
#include "graphics/gui_private.h"
#include "graphics/gui.h"
#include "types/gui_style.h"
#include "types/gui_style_lua.h"
#include "utility/log.h"
#include "scripting/lua_engine.h"

// Private functions
static int _ese_gui_lua_begin(lua_State *L);
static int _ese_gui_lua_end(lua_State *L);
static int _ese_gui_lua_open_flex(lua_State *L);
static int _ese_gui_lua_close_flex(lua_State *L);
static int _ese_gui_lua_open_stack(lua_State *L);
static int _ese_gui_lua_close_stack(lua_State *L);
static int _ese_gui_lua_push_button(lua_State *L);
static int _ese_gui_lua_push_image(lua_State *L);
static int _ese_gui_lua_get_style(lua_State *L);
static int _ese_gui_lua_set_style(lua_State *L);

typedef struct EseGuiLuaButtonCallback {
    lua_State *L;
    int lua_ref;
    void *userdata;
} EseGuiLuaButtonCallback;

static void _ese_gui_lua_button_callback_wrapper(void *userdata) {
    log_assert("GUI_LUA", userdata, "_ese_gui_lua_button_callback_wrapper called with NULL userdata");

    EseGuiLuaButtonCallback *callback = (EseGuiLuaButtonCallback *)userdata;
    lua_rawgeti(callback->L, LUA_REGISTRYINDEX, callback->lua_ref);
    lua_call(callback->L, 0, 0);
}

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
        lua_pushcfunction(engine->runtime, _ese_gui_lua_open_stack);
        lua_setfield(engine->runtime, -2, "open_stack");
        lua_pushcfunction(engine->runtime, _ese_gui_lua_close_stack);
        lua_setfield(engine->runtime, -2, "close_stack");
        lua_pushcfunction(engine->runtime, _ese_gui_lua_push_button);
        lua_setfield(engine->runtime, -2, "push_button");
        lua_pushcfunction(engine->runtime, _ese_gui_lua_push_image);
        lua_setfield(engine->runtime, -2, "push_image");
        lua_pushcfunction(engine->runtime, _ese_gui_lua_get_style);
        lua_setfield(engine->runtime, -2, "get_style");
        lua_pushcfunction(engine->runtime, _ese_gui_lua_set_style);
        lua_setfield(engine->runtime, -2, "set_style");

        // Create STYLE table
        lua_newtable(engine->runtime);
        lua_pushinteger(engine->runtime, GUI_AUTO_SIZE);
        lua_setfield(engine->runtime, -2, "AUTO_SIZE");

        // Create DIRECTION table under STYLE
        lua_newtable(engine->runtime);
        lua_pushinteger(engine->runtime, FLEX_DIRECTION_ROW);
        lua_setfield(engine->runtime, -2, "ROW");
        lua_pushinteger(engine->runtime, FLEX_DIRECTION_COLUMN);
        lua_setfield(engine->runtime, -2, "COLUMN");
        lua_setfield(engine->runtime, -2, "DIRECTION");

        // Create JUSTIFY table under STYLE
        lua_newtable(engine->runtime);
        lua_pushinteger(engine->runtime, FLEX_JUSTIFY_START);
        lua_setfield(engine->runtime, -2, "START");
        lua_pushinteger(engine->runtime, FLEX_JUSTIFY_CENTER);
        lua_setfield(engine->runtime, -2, "CENTER");
        lua_pushinteger(engine->runtime, FLEX_JUSTIFY_END);
        lua_setfield(engine->runtime, -2, "END");
        lua_setfield(engine->runtime, -2, "JUSTIFY");

        // Create ALIGN table under STYLE
        lua_newtable(engine->runtime);
        lua_pushinteger(engine->runtime, FLEX_ALIGN_ITEMS_START);
        lua_setfield(engine->runtime, -2, "START");
        lua_pushinteger(engine->runtime, FLEX_ALIGN_ITEMS_CENTER);
        lua_setfield(engine->runtime, -2, "CENTER");
        lua_pushinteger(engine->runtime, FLEX_ALIGN_ITEMS_END);
        lua_setfield(engine->runtime, -2, "END");
        lua_setfield(engine->runtime, -2, "ALIGN");

        lua_setfield(engine->runtime, -2, "STYLE");

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
        return luaL_error(L, "GUI.start() takes 5 arguments (draw_order, x, y, width, height)");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout != NULL) {
        return luaL_error(L, "GUI.start() called while another GUI is active");
    }
    
    ese_gui_begin(gui, z_index, x, y, width, height);

    return 0;
}

static int _ese_gui_lua_end(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_end called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 0) {
        return luaL_error(L, "GUI.finish() takes no arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout == NULL) {
        return luaL_error(L, "GUI.finish() called with no open GUI active");
    }

    ese_gui_end(gui);

    return 0;
}

static int _ese_gui_lua_open_flex(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_open_flex called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args > 2 || n_args < 0) {
        return luaL_error(L, "GUI.open_flex([width, height]) takes 0, 1, or 2 arguments");
    }

    if (n_args >= 1 && lua_type(L, 1) != LUA_TNUMBER) {
        return luaL_error(L, "width must be a number or GUI.AUTO_SIZE");
    }

    if (n_args >= 2 && lua_type(L, 2) != LUA_TNUMBER) {
        return luaL_error(L, "height must be a number or GUI.AUTO_SIZE");
    }

    int width = GUI_AUTO_SIZE;
    int height = GUI_AUTO_SIZE;
    if (n_args >= 1) {
        width = (int)lua_tonumber(L, 1);
    }
    if (n_args >= 2) {
        height = (int)lua_tonumber(L, 2);
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout == NULL) {
        return luaL_error(L, "GUI.open_flex() called with no open GUI active");
    }


    ese_gui_open_flex(gui, width, height);

    return 0;
}

static int _ese_gui_lua_close_flex(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_close_flex called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 0) {
        return luaL_error(L, "GUI.close_flex() takes 0 arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout->current_container == NULL || gui->open_layout->current_container->widget_type != ESE_GUI_WIDGET_FLEX) {
        return luaL_error(L, "GUI.close_flex() called with no open FLEX containers");
    }

    ese_gui_close_flex(gui);

    return 0;
}

static int _ese_gui_lua_open_stack(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_open_stack called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args > 2 || n_args < 0) {
        return luaL_error(L, "GUI.open_stack([width, height]) takes 0, 1, or 2 arguments");
    }

    if (n_args >= 1 && lua_type(L, 1) != LUA_TNUMBER) {
        return luaL_error(L, "width must be a numbe or GUI.AUTO_SIZE");
    }

    if (n_args >= 2 && lua_type(L, 2) != LUA_TNUMBER) {
        return luaL_error(L, "height must be a number or GUI.AUTO_SIZE");
    }

    int width = GUI_AUTO_SIZE;
    int height = GUI_AUTO_SIZE;
    if (n_args >= 1) {
        width = (int)lua_tonumber(L, 1);
    }
    if (n_args >= 2) {
        height = (int)lua_tonumber(L, 2);
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout == NULL) {
        return luaL_error(L, "GUI.open_stack() called with no open GUI active");
    }
    
    ese_gui_open_stack(gui, width, height);

    return 0;
}

static int _ese_gui_lua_close_stack(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_close_stack called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 0) {
        return luaL_error(L, "GUI.close_stack() takes no arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout->current_container == NULL || gui->open_layout->current_container->widget_type != ESE_GUI_WIDGET_STACK) {
        return luaL_error(L, "GUI.close_stack() called with no open STACK containers");
    }

    ese_gui_close_stack(gui);

    return 0;
}

static int _ese_gui_lua_push_button(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_push_button called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 2 && n_args != 3) {
        return luaL_error(L, "GUI.push_button(text, callback[, userdata]) takes 2 or 3 arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout == NULL) {
        return luaL_error(L, "GUI.push_button() called with no open GUI active");
    }

    const char *text = luaL_checkstring(L, 1);
    if (!lua_isfunction(L, 2)) {
        return luaL_error(L, "GUI.push_button() callback must be a function");
    }

    void *userdata = NULL;
    if (n_args == 3) {
        userdata = lua_touserdata(L, 3);
    }

    // Push the function onto the stack so we can create a reference to it
    lua_pushvalue(L, 2);  // Push the function from argument 2 onto the stack

    EseGuiLuaButtonCallback *callback = (EseGuiLuaButtonCallback *)memory_manager.calloc(1, sizeof(EseGuiLuaButtonCallback), MMTAG_GUI);
    callback->L = L;
    callback->lua_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    callback->userdata = userdata;

    ese_gui_push_button(gui, text, _ese_gui_lua_button_callback_wrapper, callback);

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

static int _ese_gui_lua_get_style(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_get_style called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 0) {
        return luaL_error(L, "GUI.get_style() takes no arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    EseGuiStyle *style = ese_gui_get_style(gui);
    ese_gui_style_lua_push(style);

    return 1;
}

static int _ese_gui_lua_set_style(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_set_style called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 1) {
        return luaL_error(L, "GUI.set_style(style) takes 1 argument");
    }

    EseGuiStyle *style = ese_gui_style_lua_get(L, 1);
    if (!style) {
        return luaL_error(L, "GUI.set_style() requires a valid GuiStyle object");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    ese_gui_set_style(gui, style);

    return 0;
}

