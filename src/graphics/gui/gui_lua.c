#include "graphics/gui/gui_lua.h"
#include "core/engine.h"
#include "core/memory_manager.h"
#include "graphics/gui/gui.h"
#include "graphics/gui/gui_private.h"
#include "graphics/gui/gui_widget.h"
#include "graphics/gui/gui_widget_button.h"
#include "graphics/gui/gui_widget_flex.h"
#include "graphics/gui/gui_widget_image.h"
#include "graphics/gui/gui_widget_stack.h"
#include "scripting/lua_engine.h"
#include "types/gui_style.h"
#include "types/gui_style_lua.h"
#include "utility/log.h"
#include <stdint.h>

// Private functions
static int _ese_gui_lua_begin(lua_State *L);
static int _ese_gui_lua_end(lua_State *L);
// Widget-specific Lua is registered by each widget vtable via
// _ese_widget_register
static int _ese_gui_lua_get_default_style(lua_State *L);
static int _ese_gui_lua_set_default_style(lua_State *L);
static int _ese_gui_lua_reset_default_style(lua_State *L);
static int _ese_gui_lua_push_variant(lua_State *L);
static int _ese_gui_lua_pop_variant(lua_State *L);

typedef struct EseGuiLuaButtonCallback {
    lua_State *L;
    int lua_ref;
    void *userdata;
} EseGuiLuaButtonCallback;

static void _ese_gui_lua_button_callback_wrapper(void *userdata) {
    log_assert("GUI_LUA", userdata,
               "_ese_gui_lua_button_callback_wrapper called with NULL userdata");

    EseGuiLuaButtonCallback *callback = (EseGuiLuaButtonCallback *)userdata;
    lua_rawgeti(callback->L, LUA_REGISTRYINDEX, callback->lua_ref);
    lua_call(callback->L, 0, 0);
}

void ese_gui_lua_init(EseLuaEngine *engine) {
    log_assert("GUI_LUA", engine, "ese_gui_lua_init called with NULL engine");

    log_debug("GUI_LUA", "Initializing GUI Lua bindings");

    // Create global GUI table with session begin/end; widgets register their
    // own functions
    lua_getglobal(engine->runtime, "GUI");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);

        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _ese_gui_lua_begin);
        lua_setfield(engine->runtime, -2, "start");
        lua_pushcfunction(engine->runtime, _ese_gui_lua_end);
        lua_setfield(engine->runtime, -2,
                     "finish"); // end is a reserved keyword
        lua_pushcfunction(engine->runtime, _ese_gui_lua_get_default_style);
        lua_setfield(engine->runtime, -2, "get_default_style");
        lua_pushcfunction(engine->runtime, _ese_gui_lua_set_default_style);
        lua_setfield(engine->runtime, -2, "set_default_style");
        lua_pushcfunction(engine->runtime, _ese_gui_lua_reset_default_style);
        lua_setfield(engine->runtime, -2, "reset_default_style");
        lua_pushcfunction(engine->runtime, _ese_gui_lua_push_variant);
        lua_setfield(engine->runtime, -2, "push_variant");
        lua_pushcfunction(engine->runtime, _ese_gui_lua_pop_variant);
        lua_setfield(engine->runtime, -2, "pop_variant");

        // Create STYLE table and constants baseline, attach to GUI
        lua_newtable(engine->runtime); // GUI.STYLE
        lua_pushinteger(engine->runtime, GUI_AUTO_SIZE);
        lua_setfield(engine->runtime, -2, "AUTO_SIZE");
        lua_setfield(engine->runtime, -2, "STYLE"); // Attach STYLE table to GUI

        // Lock GUI table
        lua_newtable(engine->runtime);
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
        lua_setmetatable(engine->runtime, -2);

        lua_setglobal(engine->runtime, "GUI");
        log_debug("GUI_LUA", "GUI table created and set globally");

        // Register all widgets' Lua bindings
        _ese_widget_register(engine);
    } else {
        lua_pop(engine->runtime, 1);
        log_debug("GUI_LUA", "GUI table already exists");
        // Ensure widgets are registered even if GUI existed
        _ese_widget_register(engine);
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
        if (lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER ||
            lua_type(L, 3) != LUA_TNUMBER || lua_type(L, 4) != LUA_TNUMBER ||
            lua_type(L, 5) != LUA_TNUMBER) {
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

    // Check if we need to grow the frame stack
    if (gui->layouts_count >= gui->layouts_capacity) {
        // For now, just log an error - in a real implementation we'd grow the
        // array
        log_error("GUI", "ese_gui_begin called with no capacity to grow frame stack");
        return 0;
    }

    // Create new frame layout
    EseGuiLayout *layout = &gui->layouts[gui->layouts_count++];
    layout->z_index = z_index;
    layout->x = x;
    layout->y = y;
    layout->width = width;
    layout->height = height;

    // Initialize draw scissor state
    layout->draw_scissors_active = false;
    layout->draw_scissors_x = 0.0f;
    layout->draw_scissors_y = 0.0f;
    layout->draw_scissors_w = 0.0f;
    layout->draw_scissors_h = 0.0f;

    // Initialize widget tree for this layout
    layout->root = NULL;
    layout->current_widget = NULL;

    // Initialize variant stack with DEFAULT
    layout->variant_stack_count = 0;
    layout->variant_stack[layout->variant_stack_count++] = GUI_STYLE_VARIANT_DEFAULT;

    // currently open frame layout
    gui->open_layout = layout;

    // Reset to the default style
    // ese_gui_reset_default_style(gui);

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

    gui->open_layout = NULL;

    return 0;
}

static int _ese_gui_lua_get_default_style(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_get_default_style called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 0) {
        return luaL_error(L, "GUI.get_default_style() takes no arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    EseGuiStyle *style = ese_gui_get_default_style(gui);
    ese_gui_style_lua_push(style);

    return 1;
}

static int _ese_gui_lua_set_default_style(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_set_default_style called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 1) {
        return luaL_error(L, "GUI.set_default_style(style) takes 1 argument");
    }

    EseGuiStyle *style = ese_gui_style_lua_get(L, 1);
    if (!style) {
        return luaL_error(L, "GUI.set_default_style() requires a valid GuiStyle object");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    ese_gui_set_default_style(gui, style);

    return 0;
}

static int _ese_gui_lua_reset_default_style(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_reset_default_style called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 0) {
        return luaL_error(L, "GUI.reset_default_style() takes no arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    ese_gui_reset_default_style(gui);

    return 0;
}

static int _ese_gui_lua_push_variant(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_push_variant called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 1) {
        return luaL_error(L, "GUI.push_variant(variant) takes 1 argument");
    }

    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "GUI.push_variant() requires a variant constant (number)");
    }

    int variant = (int)lua_tonumber(L, 1);
    if (variant < GUI_STYLE_VARIANT_DEFAULT || variant >= GUI_STYLE_VARIANT_MAX) {
        return luaL_error(L, "GUI.push_variant() invalid variant value");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout == NULL) {
        return luaL_error(L, "GUI.push_variant() called with no open GUI");
    }

    ese_gui_push_variant(gui, (EseGuiStyleVariant)variant);

    return 0;
}

static int _ese_gui_lua_pop_variant(lua_State *L) {
    log_assert("GUI_LUA", L, "ese_gui_lua_pop_variant called with NULL Lua state");

    int n_args = lua_gettop(L);
    if (n_args != 0) {
        return luaL_error(L, "GUI.pop_variant() takes no arguments");
    }

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    EseGui *gui = engine_get_gui(engine);

    if (gui->open_layout == NULL) {
        return luaL_error(L, "GUI.pop_variant() called with no open GUI");
    }

    if (gui->open_layout->variant_stack_count <= 1) {
        return luaL_error(L, "GUI.pop_variant() called with only default variant on stack");
    }

    ese_gui_pop_variant(gui);

    return 0;
}