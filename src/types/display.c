#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "types/display.h"

/**
 * @private
 * @brief An error function for the Lua metatable's `__newindex` event.
 * 
 * @details This function is used to ensure that Lua scripts cannot modify read-only tables
 * like the display state properties.
 * 
 * @param L The Lua state.
 * @return An error with a message indicating the table is read-only.
 */
static int _display_state_readonly_error(lua_State *L) {
    return luaL_error(L, "Display tables are read-only");
}

/**
 * @private
 * @brief A closure to get viewport properties by index for Lua.
 * 
 * @details This closure is bound to the `viewport` table in Lua, providing a read-only
 * indexer to access the viewport struct members.
 * 
 * @param L The Lua state.
 * @return A value on the Lua stack.
 * 
 * @note The `EseViewport` pointer is passed as an upvalue to this function.
 */
static int _display_state_viewport_index(lua_State *L) {
    EseViewport *viewport = (EseViewport *)lua_touserdata(L, lua_upvalueindex(1));
    const char *key = lua_tostring(L, 2);
    if (!viewport || !key) return 0;

    if (strcmp(key, "width") == 0) {
        lua_pushinteger(L, viewport->width);
        return 1;
    }
    if (strcmp(key, "height") == 0) {
        lua_pushinteger(L, viewport->height);
        return 1;
    }

    return 0;
}

/**
 * @private
 * @brief Pushes the EseDisplay as a read-only Lua userdata object.
 * 
 * @details This function creates a Lua table that acts as a proxy for the `EseDisplay`
 * C object, allowing Lua scripts to access its members.
 * 
 * @param display A pointer to the `EseDisplay` object to be pushed.
 * @param is_lua_owned A boolean flag indicating if Lua owns the memory for this object.
 * @return void
 * 
 * @note This function handles the creation of a Lua reference to avoid memory leaks.
 */
static void _display_state_lua_register(EseDisplay *display, bool is_lua_owned) {
    log_assert("DISPLAY", display, "_display_state_lua_register called with NULL display");
    log_assert("DISPLAY", display->lua_ref == LUA_NOREF, "_display_state_lua_register display is already registered");

    lua_newtable(display->state);
    lua_pushlightuserdata(display->state, display);
    lua_setfield(display->state, -2, "__ptr");

    // Store the ownership flag
    lua_pushboolean(display->state, is_lua_owned);
    lua_setfield(display->state, -2, "__is_lua_owned");

    luaL_getmetatable(display->state, "DisplayProxyMeta");
    lua_setmetatable(display->state, -2);

    // Store a reference to this proxy table in the Lua registry
    display->lua_ref = luaL_ref(display->state, LUA_REGISTRYINDEX);

    // Push the proxy table back onto the stack for Lua to receive
    lua_rawgeti(display->state, LUA_REGISTRYINDEX, display->lua_ref);
}

static void display_state_lua_push(EseDisplay *display) {
    log_assert("DISPLAY", display, "display_state_lua_push called with NULL display");
    log_assert("DISPLAY", display->lua_ref != LUA_NOREF, "display_state_lua_push display not registered with lua");

    // Push the proxy table back onto the stack for Lua to receive
    lua_rawgeti(display->state, LUA_REGISTRYINDEX, display->lua_ref);
}

/**
 * @private
 * @brief Extracts a EseDisplay pointer from a Lua userdata object.
 * 
 * @details This function validates that the Lua object at a given stack index is a valid
 * `EseDisplay` proxy and returns the underlying C pointer.
 * 
 * @param L A pointer to the Lua state.
 * @param idx The stack index of the Lua Display object.
 * @return A pointer to the `EseDisplay` object, or `NULL` if the object is invalid.
 * 
 * @note This function should be used to safely retrieve the C object from a Lua context.
 */
static EseDisplay *_display_state_lua_get(lua_State *L, int idx) {
    if (!lua_istable(L, idx)) return NULL;

    if (!lua_getmetatable(L, idx)) return NULL;

    luaL_getmetatable(L, "DisplayProxyMeta");
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
    return (EseDisplay *)ptr;
}

/**
 * @private
 * @brief A metatable `__index` function for the EseDisplay Lua proxy.
 * 
 * @details This function handles access to the members of the `EseDisplay` C object from Lua.
 * It returns sub-tables for viewport and numeric values for display properties.
 * 
 * @param L The Lua state.
 * @return The value of the requested member.
 */
static int _display_state_lua_index(lua_State *L) {
    EseDisplay *display = _display_state_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!display || !key) return 0;

    // Simple properties
    if (strcmp(key, "fullscreen") == 0) {
        lua_pushboolean(L, display->fullscreen);
        return 1;
    }
    if (strcmp(key, "width") == 0) {
        lua_pushinteger(L, display->width);
        return 1;
    }
    if (strcmp(key, "height") == 0) {
        lua_pushinteger(L, display->height);
        return 1;
    }
    if (strcmp(key, "aspect_ratio") == 0) {
        lua_pushnumber(L, display->aspect_ratio);
        return 1;
    }

    // viewport table proxy (read-only)
    if (strcmp(key, "viewport") == 0) {
        // Create the table
        lua_newtable(L);

        // Create and set the metatable
        lua_newtable(L);
        
        // Set __index closure with the viewport pointer as upvalue
        lua_pushlightuserdata(L, &display->viewport);
        lua_pushcclosure(L, _display_state_viewport_index, 1);
        lua_setfield(L, -2, "__index");
        
        // Set __newindex to error
        lua_pushcfunction(L, _display_state_readonly_error);
        lua_setfield(L, -2, "__newindex");
        
        // Apply metatable to the table
        lua_setmetatable(L, -2);

        return 1;
    }

    return 0;
}

/**
 * @private
 * @brief A metatable `__newindex` function for the EseDisplay Lua proxy.
 * 
 * @details This function prevents any modification of the `EseDisplay` object from Lua,
 * ensuring it remains a read-only representation of the current display state.
 * 
 * @param L The Lua state.
 * @return An error with a message indicating the object is read-only.
 */
static int _display_state_lua_newindex(lua_State *L) {
    return luaL_error(L, "Display object is read-only");
}

/**
 * @private
 * @brief A metatable `__gc` (garbage collection) function for the EseDisplay Lua proxy.
 * 
 * @details This function is responsible for correctly handling the memory of the
 * `EseDisplay` C object when its Lua proxy is garbage collected. It only frees the
 * C memory if the `is_lua_owned` flag is true.
 * 
 * @param L The Lua state.
 * @return The number of results pushed onto the stack (0).
 */
static int _display_state_lua_gc(lua_State *L) {
    EseDisplay *display = _display_state_lua_get(L, 1);

    if (display) {
        lua_getfield(L, 1, "__is_lua_owned");
        bool is_lua_owned = lua_toboolean(L, -1);
        lua_pop(L, 1);

        if (is_lua_owned) {
            display_state_destroy(display);
            log_debug("LUA_GC", "Display object (Lua-owned) garbage collected and C memory freed.");
        } else {
            log_debug("LUA_GC", "Display object (C-owned) garbage collected, C memory *not* freed.");
        }
    }

    return 0;
}

/**
 * @private
 * @brief A metatable `__tostring` function for the EseDisplay Lua proxy.
 * 
 * @details This function provides a string representation of the `EseDisplay` object
 * for easy debugging and printing in Lua.
 * 
 * @param L The Lua state.
 * @return A string representation of the `EseDisplay`.
 */
static int _display_state_lua_tostring(lua_State *L) {
    EseDisplay *display = _display_state_lua_get(L, 1);
    if (!display) {
        lua_pushstring(L, "Display: (invalid)");
        return 1;
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "Display: %p (%dx%d, %s, viewport: %dx%d)", 
             (void *)display, display->width, display->height,
             display->fullscreen ? "fullscreen" : "windowed",
             display->viewport.width, display->viewport.height);
    lua_pushstring(L, buf);
    return 1;
}

void display_state_lua_init(EseLuaEngine *engine) {
    log_assert("DISPLAY_STATE", engine, "display_state_lua_init called with NULL engine");
    log_assert("DISPLAY_STATE", engine->runtime, "display_state_lua_init called with NULL engine->runtime");

    if (luaL_newmetatable(engine->runtime, "DisplayProxyMeta")) {
        log_debug("LUA", "Adding DisplayProxyMeta to engine");
        lua_pushcfunction(engine->runtime, _display_state_lua_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _display_state_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushcfunction(engine->runtime, _display_state_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");
        lua_pushcfunction(engine->runtime, _display_state_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
}

EseDisplay *display_state_create(EseLuaEngine *engine) {
    log_assert("DISPLAY_STATE", engine, "display_state_create called with NULL engine");
    log_assert("DISPLAY_STATE", memory_manager.malloc, "memory_manager.malloc is NULL");

    EseDisplay *display = (EseDisplay *)memory_manager.malloc(sizeof(EseDisplay), MMTAG_GENERAL);
    log_assert("DISPLAY_STATE", display, "display_state_create failed to allocate memory");

    display->fullscreen = false;
    display->width = 0;
    display->height = 0;
    display->aspect_ratio = 1.0f;

    // Initialize viewport to match display dimensions
    display->viewport.width = 0;
    display->viewport.height = 0;

    display->state = engine->runtime;
    display->lua_ref = LUA_NOREF;

    _display_state_lua_register(display, false);

    return display;
}

EseDisplay *display_state_copy(const EseDisplay *src, EseLuaEngine *engine) {
    log_assert("DISPLAY_STATE", src, "display_state_copy called with NULL src");
    log_assert("DISPLAY_STATE", engine, "display_state_copy called with NULL engine");
    log_assert("DISPLAY_STATE", memory_manager.malloc, "memory_manager.malloc is NULL");

    EseDisplay *copy = (EseDisplay *)memory_manager.malloc(sizeof(EseDisplay), MMTAG_GENERAL);
    log_assert("DISPLAY_STATE", copy, "display_state_copy failed to allocate memory");

    copy->fullscreen = src->fullscreen;
    copy->width = src->width;
    copy->height = src->height;
    copy->aspect_ratio = src->aspect_ratio;
    copy->viewport = src->viewport; // Struct copy

    copy->state = engine->runtime;
    copy->lua_ref = LUA_NOREF;

    _display_state_lua_register(copy, false);

    return copy;
}

void display_state_destroy(EseDisplay *display) {
    if (display) {
        if (display->lua_ref != LUA_NOREF) {
            luaL_unref(display->state, LUA_REGISTRYINDEX, display->lua_ref);
        }
        memory_manager.free(display);
    }
}

void display_state_set_dimensions(EseDisplay *display, int width, int height) {
    log_assert("DISPLAY_STATE", display, "display_state_set_dimensions called with NULL display");

    display->width = width;
    display->height = height;
    display->aspect_ratio = (height > 0) ? (float)width / (float)height : 1.0f;
}

void display_state_set_fullscreen(EseDisplay *display, bool fullscreen) {
    log_assert("DISPLAY_STATE", display, "display_state_set_fullscreen called with NULL display");
    display->fullscreen = fullscreen;
}

void display_state_set_viewport(EseDisplay *display, int width, int height) {
    log_assert("DISPLAY_STATE", display, "display_state_set_viewport called with NULL display");
    
    display->viewport.width = width;
    display->viewport.height = height;
}
