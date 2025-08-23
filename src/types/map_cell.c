#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "vendor/lua/src/lauxlib.h"
#include "types/map_cell.h"

#define INITIAL_LAYER_CAPACITY 4

/* ----------------- Internal Helpers ----------------- */

/**
 * @brief Register a MapCell proxy table in Lua.
 */
static void _mapcell_lua_register(EseMapCell *cell, bool is_lua_owned) {
    log_assert("MAPCELL", cell, "_mapcell_lua_register called with NULL cell");
    log_assert("MAPCELL", cell->lua_ref == LUA_NOREF,
               "_mapcell_lua_register cell already registered");

    lua_State *L = cell->state;

    lua_newtable(L);

    // Store pointer
    lua_pushlightuserdata(L, cell);
    lua_setfield(L, -2, "__ptr");

    // Store ownership flag
    lua_pushboolean(L, is_lua_owned);
    lua_setfield(L, -2, "__is_lua_owned");

    // Set metatable
    luaL_getmetatable(L, "MapCellProxyMeta");
    lua_setmetatable(L, -2);

    // Store registry reference
    cell->lua_ref = luaL_ref(L, LUA_REGISTRYINDEX);
}

/**
 * @brief Push proxy table back onto Lua stack.
 */
void mapcell_lua_push(EseMapCell *cell) {
    log_assert("MAPCELL", cell, "mapcell_lua_push called with NULL cell");
    log_assert("MAPCELL", cell->lua_ref != LUA_NOREF,
               "mapcell_lua_push cell not registered with lua");

    lua_rawgeti(cell->state, LUA_REGISTRYINDEX, cell->lua_ref);
}

/**
 * @brief Lua constructor for MapCell.
 * 
 * Usage: local cell = MapCell.new()
 */
static int _mapcell_lua_new(lua_State *L) {
    EseMapCell *cell = (EseMapCell *)memory_manager.malloc(
        sizeof(EseMapCell), MMTAG_GENERAL);

    cell->tile_ids = (uint8_t *)memory_manager.malloc(
        sizeof(uint8_t) * INITIAL_LAYER_CAPACITY, MMTAG_GENERAL);
    cell->layer_count = 0;
    cell->layer_capacity = INITIAL_LAYER_CAPACITY;
    cell->isDynamic = false;
    cell->flags = 0;
    cell->data = NULL;
    cell->state = L;
    cell->lua_ref = LUA_NOREF;

    _mapcell_lua_register(cell, true);
    mapcell_lua_push(cell);
    return 1;
}

/* ----------------- Lua Methods ----------------- */

static int _mapcell_lua_add_layer(lua_State *L) {
    EseMapCell *cell = mapcell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in add_layer");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "add_layer(tile_id) requires a number");

    uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
    lua_pushboolean(L, mapcell_add_layer(cell, tile_id));
    return 1;
}

static int _mapcell_lua_remove_layer(lua_State *L) {
    EseMapCell *cell = mapcell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in remove_layer");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "remove_layer(index) requires a number");

    size_t idx = (size_t)lua_tonumber(L, 2);
    lua_pushboolean(L, mapcell_remove_layer(cell, idx));
    return 1;
}

static int _mapcell_lua_get_layer(lua_State *L) {
    EseMapCell *cell = mapcell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in get_layer");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "get_layer(index) requires a number");

    size_t idx = (size_t)lua_tonumber(L, 2);
    lua_pushnumber(L, mapcell_get_layer(cell, idx));
    return 1;
}

static int _mapcell_lua_set_layer(lua_State *L) {
    EseMapCell *cell = mapcell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in set_layer");

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3))
        return luaL_error(L, "set_layer(index, tile_id) requires two numbers");

    size_t idx = (size_t)lua_tonumber(L, 2);
    uint8_t tile_id = (uint8_t)lua_tonumber(L, 3);
    lua_pushboolean(L, mapcell_set_layer(cell, idx, tile_id));
    return 1;
}

static int _mapcell_lua_clear_layers(lua_State *L) {
    EseMapCell *cell = mapcell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in clear_layers");

    mapcell_clear_layers(cell);
    return 0;
}

static int _mapcell_lua_has_flag(lua_State *L) {
    EseMapCell *cell = mapcell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in has_flag");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "has_flag(flag) requires a number");

    uint32_t flag = (uint32_t)lua_tonumber(L, 2);
    lua_pushboolean(L, mapcell_has_flag(cell, flag));
    return 1;
}

static int _mapcell_lua_set_flag(lua_State *L) {
    EseMapCell *cell = mapcell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in set_flag");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "set_flag(flag) requires a number");

    uint32_t flag = (uint32_t)lua_tonumber(L, 2);
    mapcell_set_flag(cell, flag);
    return 0;
}

static int _mapcell_lua_clear_flag(lua_State *L) {
    EseMapCell *cell = mapcell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in clear_flag");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "clear_flag(flag) requires a number");

    uint32_t flag = (uint32_t)lua_tonumber(L, 2);
    mapcell_clear_flag(cell, flag);
    return 0;
}

/* ----------------- Metamethods ----------------- */

static int _mapcell_lua_index(lua_State *L) {
    EseMapCell *cell = mapcell_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!cell || !key) return 0;

    if (strcmp(key, "isDynamic") == 0) {
        lua_pushboolean(L, cell->isDynamic);
        return 1;
    } else if (strcmp(key, "flags") == 0) {
        lua_pushnumber(L, cell->flags);
        return 1;
    } else if (strcmp(key, "layer_count") == 0) {
        lua_pushnumber(L, cell->layer_count);
        return 1;
    } else if (strcmp(key, "add_layer") == 0) {
        lua_pushcfunction(L, _mapcell_lua_add_layer);
        return 1;
    } else if (strcmp(key, "remove_layer") == 0) {
        lua_pushcfunction(L, _mapcell_lua_remove_layer);
        return 1;
    } else if (strcmp(key, "get_layer") == 0) {
        lua_pushcfunction(L, _mapcell_lua_get_layer);
        return 1;
    } else if (strcmp(key, "set_layer") == 0) {
        lua_pushcfunction(L, _mapcell_lua_set_layer);
        return 1;
    } else if (strcmp(key, "clear_layers") == 0) {
        lua_pushcfunction(L, _mapcell_lua_clear_layers);
        return 1;
    } else if (strcmp(key, "has_flag") == 0) {
        lua_pushcfunction(L, _mapcell_lua_has_flag);
        return 1;
    } else if (strcmp(key, "set_flag") == 0) {
        lua_pushcfunction(L, _mapcell_lua_set_flag);
        return 1;
    } else if (strcmp(key, "clear_flag") == 0) {
        lua_pushcfunction(L, _mapcell_lua_clear_flag);
        return 1;
    }

    return 0;
}

static int _mapcell_lua_newindex(lua_State *L) {
    EseMapCell *cell = mapcell_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!cell || !key) return 0;

    if (strcmp(key, "isDynamic") == 0) {
        cell->isDynamic = lua_toboolean(L, 3);
        return 0;
    } else if (strcmp(key, "flags") == 0) {
        if (!lua_isnumber(L, 3))
            return luaL_error(L, "flags must be a number");
        cell->flags = (uint32_t)lua_tonumber(L, 3);
        return 0;
    }

    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

static int _mapcell_lua_gc(lua_State *L) {
    EseMapCell *cell = mapcell_lua_get(L, 1);
    if (cell) {
        lua_getfield(L, 1, "__is_lua_owned");
        bool is_lua_owned = lua_toboolean(L, -1);
        lua_pop(L, 1);

        if (is_lua_owned) {
            mapcell_destroy(cell);
            log_debug("LUA_GC", "MapCell (Lua-owned) freed.");
        } else {
            log_debug("LUA_GC", "MapCell (C-owned) collected, not freed.");
        }
    }
    return 0;
}

static int _mapcell_lua_tostring(lua_State *L) {
    EseMapCell *cell = mapcell_lua_get(L, 1);
    if (!cell) {
        lua_pushstring(L, "MapCell: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "MapCell: %p (layers=%zu, flags=%u, dynamic=%d)",
             (void *)cell, cell->layer_count, cell->flags, cell->isDynamic);
    lua_pushstring(L, buf);
    return 1;
}

/* ----------------- Lua Init ----------------- */
void mapcell_lua_init(EseLuaEngine *engine) {
    if (luaL_newmetatable(engine->runtime, "MapCellProxyMeta")) {
        log_debug("LUA", "Adding MapCellProxyMeta");
        lua_pushcfunction(engine->runtime, _mapcell_lua_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _mapcell_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushcfunction(engine->runtime, _mapcell_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");
        lua_pushcfunction(engine->runtime, _mapcell_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);

    // Create global MapCell table with constructor
    lua_getglobal(engine->runtime, "MapCell");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);
        log_debug("LUA", "Creating global MapCell table");
        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _mapcell_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_setglobal(engine->runtime, "MapCell");
    } else {
        lua_pop(engine->runtime, 1);
    }
}

/* ----------------- C API ----------------- */

EseMapCell *mapcell_create(EseLuaEngine *engine, bool c_only) {
    EseMapCell *cell =
        (EseMapCell *)memory_manager.malloc(sizeof(EseMapCell), MMTAG_GENERAL);
    cell->tile_ids = (uint8_t *)memory_manager.malloc(
        sizeof(uint8_t) * INITIAL_LAYER_CAPACITY, MMTAG_GENERAL);
    cell->layer_count = 0;
    cell->layer_capacity = INITIAL_LAYER_CAPACITY;
    cell->isDynamic = false;
    cell->flags = 0;
    cell->data = NULL;
    cell->state = engine->runtime;
    cell->lua_ref = LUA_NOREF;

    if (!c_only) {
        _mapcell_lua_register(cell, false);
    }
    return cell;
}

EseMapCell *mapcell_copy(const EseMapCell *src, bool c_only) {
    if (!src) return NULL;
    EseMapCell *copy =
        (EseMapCell *)memory_manager.malloc(sizeof(EseMapCell), MMTAG_GENERAL);
    copy->tile_ids = (uint8_t *)memory_manager.malloc(
        sizeof(uint8_t) * src->layer_capacity, MMTAG_GENERAL);
    memcpy(copy->tile_ids, src->tile_ids, sizeof(uint8_t) * src->layer_count);
    copy->layer_count = src->layer_count;
    copy->layer_capacity = src->layer_capacity;
    copy->isDynamic = src->isDynamic;
    copy->flags = src->flags;
    copy->state = src->state;
    copy->lua_ref = LUA_NOREF;

    if (!c_only) {
        _mapcell_lua_register(copy, false);
    }
    return copy;
}

void mapcell_destroy(EseMapCell *cell) {
    if (cell) {
        if (cell->lua_ref != LUA_NOREF) {
            luaL_unref(cell->state, LUA_REGISTRYINDEX, cell->lua_ref);
        }
        if (cell->tile_ids) {
            memory_manager.free(cell->tile_ids);
        }
        memory_manager.free(cell);
    }
}

EseMapCell *mapcell_lua_get(lua_State *L, int idx) {
    if (!lua_istable(L, idx)) return NULL;
    if (!lua_getmetatable(L, idx)) return NULL;
    luaL_getmetatable(L, "MapCellProxyMeta");
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
    return (EseMapCell *)ptr;
}

/* ----------------- Tile/Flag Helpers ----------------- */

bool mapcell_add_layer(EseMapCell *cell, uint8_t tile_id) {
    if (!cell) return false;

    if (cell->layer_count >= cell->layer_capacity) {
        size_t new_capacity = cell->layer_capacity * 2;
        uint8_t *new_array = (uint8_t *)memory_manager.realloc(
            cell->tile_ids, sizeof(uint8_t) * new_capacity, MMTAG_GENERAL);
        if (!new_array) return false;
        cell->tile_ids = new_array;
        cell->layer_capacity = new_capacity;
    }

    cell->tile_ids[cell->layer_count++] = tile_id;
    return true;
}

bool mapcell_remove_layer(EseMapCell *cell, size_t layer_index) {
    if (!cell || layer_index >= cell->layer_count) return false;

    for (size_t i = layer_index; i < cell->layer_count - 1; i++) {
        cell->tile_ids[i] = cell->tile_ids[i + 1];
    }
    cell->layer_count--;
    return true;
}

uint8_t mapcell_get_layer(const EseMapCell *cell, size_t layer_index) {
    if (!cell || layer_index >= cell->layer_count) return 0;
    return cell->tile_ids[layer_index];
}

bool mapcell_set_layer(EseMapCell *cell, size_t layer_index, uint8_t tile_id) {
    if (!cell || layer_index >= cell->layer_count) return false;
    cell->tile_ids[layer_index] = tile_id;
    return true;
}

void mapcell_clear_layers(EseMapCell *cell) {
    if (cell) cell->layer_count = 0;
}

bool mapcell_has_flag(const EseMapCell *cell, uint32_t flag) {
    if (!cell) return false;
    return (cell->flags & flag) != 0;
}

void mapcell_set_flag(EseMapCell *cell, uint32_t flag) {
    if (cell) cell->flags |= flag;
}

void mapcell_clear_flag(EseMapCell *cell, uint32_t flag) {
    if (cell) cell->flags &= ~flag;
}
