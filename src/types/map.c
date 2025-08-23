#include <string.h>
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "core/memory_manager.h"
#include "types/map.h"
#include "types/map_cell.h"
#include "types/tileset.h"

/* ----------------- Internal Helpers ----------------- */

static char *_strdup_safe(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src) + 1;
    char *dst = (char *)memory_manager.malloc(len, MMTAG_GENERAL);
    if (dst) memcpy(dst, src, len);
    return dst;
}

static bool _allocate_cells_array(EseMap *map) {
    if (!map || map->width == 0 || map->height == 0) return false;

    map->cells = (EseMapCell ***)memory_manager.malloc(
        sizeof(EseMapCell **) * map->height, MMTAG_GENERAL);
    if (!map->cells) return false;

    for (uint32_t y = 0; y < map->height; y++) {
        map->cells[y] = (EseMapCell **)memory_manager.malloc(
            sizeof(EseMapCell *) * map->width, MMTAG_GENERAL);
        if (!map->cells[y]) {
            for (uint32_t cleanup_y = 0; cleanup_y < y; cleanup_y++) {
                for (uint32_t x = 0; x < map->width; x++) {
                    if (map->cells[cleanup_y][x]) {
                        mapcell_destroy(map->cells[cleanup_y][x]);
                    }
                }
                memory_manager.free(map->cells[cleanup_y]);
            }
            memory_manager.free(map->cells);
            map->cells = NULL;
            return false;
        }

        for (uint32_t x = 0; x < map->width; x++) {
            EseMapCell *cell = mapcell_create((EseLuaEngine *)map->engine, false);
            if (!cell) {
                // cleanup
                for (uint32_t yy = 0; yy <= y; yy++) {
                    for (uint32_t xx = 0; xx < (yy == y ? x : map->width); xx++) {
                        if (map->cells[yy][xx]) {
                            mapcell_destroy(map->cells[yy][xx]);
                        }
                    }
                    memory_manager.free(map->cells[yy]);
                }
                memory_manager.free(map->cells);
                map->cells = NULL;
                return false;
            }
            map->cells[y][x] = cell;
        }
    }
    return true;
}

static void _free_cells_array(EseMap *map) {
    if (!map || !map->cells) return;

    for (uint32_t y = 0; y < map->height; y++) {
        if (map->cells[y]) {
            for (uint32_t x = 0; x < map->width; x++) {
                if (map->cells[y][x]) {
                    mapcell_destroy(map->cells[y][x]);
                }
            }
            memory_manager.free(map->cells[y]);
        }
    }
    memory_manager.free(map->cells);
    map->cells = NULL;
}

static void _map_lua_register(EseMap *map, bool is_lua_owned) {
    log_assert("MAP", map, "_map_lua_register called with NULL map");
    log_assert("MAP", map->lua_ref == LUA_NOREF,
               "_map_lua_register map already registered");

    lua_State *L = map->state;
    lua_newtable(L);

    lua_pushlightuserdata(L, map);
    lua_setfield(L, -2, "__ptr");

    lua_pushboolean(L, is_lua_owned);
    lua_setfield(L, -2, "__is_lua_owned");

    luaL_getmetatable(L, "MapProxyMeta");
    lua_setmetatable(L, -2);

    map->lua_ref = luaL_ref(L, LUA_REGISTRYINDEX);
}

void map_lua_push(EseMap *map) {
    log_assert("MAP", map, "map_lua_push called with NULL map");
    log_assert("MAP", map->lua_ref != LUA_NOREF,
               "map_lua_push map not registered with lua");
    lua_rawgeti(map->state, LUA_REGISTRYINDEX, map->lua_ref);
}

/* ----------------- Lua Methods ----------------- */

static int _map_lua_get_cell(lua_State *L) {
    EseMap *map = map_lua_get(L, 1);
    if (!map) return luaL_error(L, "Invalid Map in get_cell");

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3))
        return luaL_error(L, "get_cell(x, y) requires two numbers");

    uint32_t x = (uint32_t)lua_tonumber(L, 2);
    uint32_t y = (uint32_t)lua_tonumber(L, 3);

    EseMapCell *cell = map_get_cell(map, x, y);
    if (!cell) {
        lua_pushnil(L);
        return 1;
    }
    mapcell_lua_push(cell);
    return 1;
}

static int _map_lua_set_cell(lua_State *L) {
    EseMap *map = map_lua_get(L, 1);
    if (!map) return luaL_error(L, "Invalid Map in set_cell");

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3))
        return luaL_error(L, "set_cell(x, y, cell) requires coordinates");

    uint32_t x = (uint32_t)lua_tonumber(L, 2);
    uint32_t y = (uint32_t)lua_tonumber(L, 3);

    EseMapCell *new_cell = mapcell_lua_get(L, 4);
    if (!new_cell) return luaL_error(L, "set_cell requires a valid MapCell");

    if (!map_set_cell(map, x, y, new_cell)) {
        lua_pushboolean(L, false);
        return 1;
    }
    lua_pushboolean(L, true);
    return 1;
}

static int _map_lua_resize(lua_State *L) {
    EseMap *map = map_lua_get(L, 1);
    if (!map) return luaL_error(L, "Invalid Map in resize");

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3))
        return luaL_error(L, "resize(width, height) requires two numbers");

    uint32_t new_width = (uint32_t)lua_tonumber(L, 2);
    uint32_t new_height = (uint32_t)lua_tonumber(L, 3);

    lua_pushboolean(L, map_resize(map, new_width, new_height));
    return 1;
}

static int _map_lua_set_tileset(lua_State *L) {
    EseMap *map = map_lua_get(L, 1);
    if (!map) return luaL_error(L, "Invalid Map in set_tileset");

    EseTileSet *tileset = tileset_lua_get(L, 2);
    if (!tileset) return luaL_error(L, "set_tileset requires a valid Tileset");

    map_set_tileset(map, tileset);
    return 0;
}

/* ----------------- Metamethods ----------------- */

static int _map_lua_index(lua_State *L) {
    EseMap *map = map_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!map || !key) return 0;

    if (strcmp(key, "title") == 0) {
        lua_pushstring(L, map->title ? map->title : "");
        return 1;
    } else if (strcmp(key, "author") == 0) {
        lua_pushstring(L, map->author ? map->author : "");
        return 1;
    } else if (strcmp(key, "version") == 0) {
        lua_pushnumber(L, map->version);
        return 1;
    } else if (strcmp(key, "type") == 0) {
        lua_pushstring(L, map_type_to_string(map->type));
        return 1;
    } else if (strcmp(key, "width") == 0) {
        lua_pushnumber(L, map->width);
        return 1;
    } else if (strcmp(key, "height") == 0) {
        lua_pushnumber(L, map->height);
        return 1;
    } else if (strcmp(key, "tileset") == 0) {
        if (map->tileset && map->tileset->lua_ref != LUA_NOREF) {
            tileset_lua_push(map->tileset);
        } else {
            lua_pushnil(L);
        }
        return 1;
    } else if (strcmp(key, "get_cell") == 0) {
        lua_pushcfunction(L, _map_lua_get_cell);
        return 1;
    } else if (strcmp(key, "set_cell") == 0) {
        lua_pushcfunction(L, _map_lua_set_cell);
        return 1;
    } else if (strcmp(key, "resize") == 0) {
        lua_pushcfunction(L, _map_lua_resize);
        return 1;
    } else if (strcmp(key, "set_tileset") == 0) {
        lua_pushcfunction(L, _map_lua_set_tileset);
        return 1;
    }

    return 0;
}

static int _map_lua_newindex(lua_State *L) {
    EseMap *map = map_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!map || !key) return 0;

    if (strcmp(key, "title") == 0) {
        map_set_title(map, lua_tostring(L, 3));
        return 0;
    } else if (strcmp(key, "author") == 0) {
        map_set_author(map, lua_tostring(L, 3));
        return 0;
    } else if (strcmp(key, "version") == 0) {
        map_set_version(map, (int)lua_tonumber(L, 3));
        return 0;
    } else if (strcmp(key, "type") == 0) {
        const char *type_str = lua_tostring(L, 3);
        if (type_str) map->type = map_type_from_string(type_str);
        return 0;
    }

    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

static int _map_lua_gc(lua_State *L) {
    EseMap *map = map_lua_get(L, 1);
    if (map) {
        lua_getfield(L, 1, "__is_lua_owned");
        bool is_lua_owned = lua_toboolean(L, -1);
        lua_pop(L, 1);

        if (is_lua_owned) {
            map_destroy(map);
            log_debug("LUA_GC", "Map (Lua-owned) freed.");
        } else {
            log_debug("LUA_GC", "Map (C-owned) collected, not freed.");
        }
    }
    return 0;
}

static int _map_lua_tostring(lua_State *L) {
    EseMap *map = map_lua_get(L, 1);
    if (!map) {
        lua_pushstring(L, "Map: (invalid)");
        return 1;
    }

    char buf[160];
    snprintf(buf, sizeof(buf), "Map: %p (title=%s, size=%ux%u, type=%s)",
             (void *)map,
             map->title ? map->title : "(null)",
             map->width, map->height,
             map_type_to_string(map->type));
    lua_pushstring(L, buf);
    return 1;
}

/* ----------------- Lua Init ----------------- */

static int _map_lua_new(lua_State *L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
        return luaL_error(L, "Map.new(width, height, [type]) requires at least two numbers");

    uint32_t width = (uint32_t)lua_tonumber(L, 1);
    uint32_t height = (uint32_t)lua_tonumber(L, 2);
    EseMapType type = MAP_TYPE_GRID;

    if (lua_isstring(L, 3)) {
        type = map_type_from_string(lua_tostring(L, 3));
    }

    EseMap *map = map_create((EseLuaEngine *)lua_getextraspace(L), width, height, type, true);
    if (!map) return luaL_error(L, "Failed to create map");

    _map_lua_register(map, true);
    map_lua_push(map);
    return 1;
}

void map_lua_init(EseLuaEngine *engine) {
    if (luaL_newmetatable(engine->runtime, "MapProxyMeta")) {
        log_debug("LUA", "Adding MapProxyMeta");
        lua_pushcfunction(engine->runtime, _map_lua_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _map_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushcfunction(engine->runtime, _map_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");
        lua_pushcfunction(engine->runtime, _map_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);

    lua_getglobal(engine->runtime, "Map");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);
        log_debug("LUA", "Creating global Map table");
        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _map_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_setglobal(engine->runtime, "Map");
    } else {
        lua_pop(engine->runtime, 1);
    }
}

/* ----------------- C API ----------------- */

EseMap *map_create(EseLuaEngine *engine, uint32_t width, uint32_t height, EseMapType type, bool c_only) {
    if (width == 0 || height == 0) return NULL;

    EseMap *map = (EseMap *)memory_manager.malloc(sizeof(EseMap), MMTAG_GENERAL);
    map->title = _strdup_safe("Untitled Map");
    map->author = _strdup_safe("Unknown");
    map->version = 0;
    map->type = type;
    map->tileset = NULL;
    map->width = width;
    map->height = height;
    map->cells = NULL;
    map->state = engine->runtime;
    map->engine = engine;
    map->lua_ref = LUA_NOREF;

    if (!_allocate_cells_array(map)) {
        if (map->title) memory_manager.free(map->title);
        if (map->author) memory_manager.free(map->author);
        memory_manager.free(map);
        return NULL;
    }

    if (!c_only) {
        _map_lua_register(map, false);
    }
    return map;
}

void map_destroy(EseMap *map) {
    if (map) {
        if (map->lua_ref != LUA_NOREF) {
            luaL_unref(map->state, LUA_REGISTRYINDEX, map->lua_ref);
        }
        _free_cells_array(map);
        if (map->title) memory_manager.free(map->title);
        if (map->author) memory_manager.free(map->author);
        memory_manager.free(map);
    }
}

EseMap *map_lua_get(lua_State *L, int idx) {
    if (!lua_istable(L, idx)) return NULL;
    if (!lua_getmetatable(L, idx)) return NULL;
    luaL_getmetatable(L, "MapProxyMeta");
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
    return (EseMap *)ptr;
}

EseMapCell *map_get_cell(const EseMap *map, uint32_t x, uint32_t y) {
    if (!map || !map->cells || x >= map->width || y >= map->height) return NULL;
    return map->cells[y][x];
}

bool map_set_cell(EseMap *map, uint32_t x, uint32_t y, EseMapCell *cell) {
    if (!map || !map->cells || !cell || x >= map->width || y >= map->height) return false;

    // Destroy old cell
    if (map->cells[y][x]) {
        mapcell_destroy(map->cells[y][x]);
    }

    // Replace with new cell (C-owned)
    map->cells[y][x] = mapcell_copy(cell, false);
    return map->cells[y][x] != NULL;
}

bool map_set_title(EseMap *map, const char *title) {
    if (!map) return false;
    if (map->title) memory_manager.free(map->title);
    map->title = _strdup_safe(title);
    return map->title != NULL;
}

bool map_set_author(EseMap *map, const char *author) {
    if (!map) return false;
    if (map->author) memory_manager.free(map->author);
    map->author = _strdup_safe(author);
    return map->author != NULL;
}

void map_set_version(EseMap *map, int version) {
    if (map) map->version = version;
}

void map_set_tileset(EseMap *map, EseTileSet *tileset) {
    if (map) map->tileset = tileset;
}

bool map_resize(EseMap *map, uint32_t new_width, uint32_t new_height) {
    if (!map || new_width == 0 || new_height == 0) return false;
    if (new_width == map->width && new_height == map->height) return true;

    // Save old
    uint32_t old_width = map->width;
    uint32_t old_height = map->height;
    EseMapCell ***old_cells = map->cells;

    map->width = new_width;
    map->height = new_height;
    map->cells = NULL;

    if (!_allocate_cells_array(map)) {
        map->width = old_width;
        map->height = old_height;
        map->cells = old_cells;
        return false;
    }

    // Copy over cells that fit
    uint32_t copy_width = (old_width < new_width) ? old_width : new_width;
    uint32_t copy_height = (old_height < new_height) ? old_height : new_height;

    for (uint32_t y = 0; y < copy_height; y++) {
        for (uint32_t x = 0; x < copy_width; x++) {
            if (old_cells[y][x]) {
                mapcell_destroy(map->cells[y][x]);
                map->cells[y][x] = mapcell_copy(old_cells[y][x], false);
            }
        }
    }

    // Free old
    for (uint32_t y = 0; y < old_height; y++) {
        if (old_cells[y]) {
            for (uint32_t x = 0; x < old_width; x++) {
                if (old_cells[y][x]) {
                    mapcell_destroy(old_cells[y][x]);
                }
            }
            memory_manager.free(old_cells[y]);
        }
    }
    memory_manager.free(old_cells);

    return true;
}

const char *map_type_to_string(EseMapType type) {
    switch (type) {
        case MAP_TYPE_GRID: return "grid";
        case MAP_TYPE_HEX_POINT_UP: return "hex_point_up";
        case MAP_TYPE_HEX_FLAT_UP: return "hex_flat_up";
        case MAP_TYPE_ISO: return "iso";
        default: return "grid";
    }
}

EseMapType map_type_from_string(const char *type_str) {
    if (!type_str) return MAP_TYPE_GRID;
    if (strcmp(type_str, "grid") == 0) return MAP_TYPE_GRID;
    if (strcmp(type_str, "hex_point_up") == 0) return MAP_TYPE_HEX_POINT_UP;
    if (strcmp(type_str, "hex_flat_up") == 0) return MAP_TYPE_HEX_FLAT_UP;
    if (strcmp(type_str, "iso") == 0) return MAP_TYPE_ISO;
    return MAP_TYPE_GRID;
}
