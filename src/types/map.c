#include <string.h>
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "core/memory_manager.h"
#include "types/map.h"
#include "types/map_cell.h"
#include "types/tileset.h"

/* ----------------- Internal Helpers ----------------- */

static char *_strdup_safe(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src) + 1;
    char *dst = (char *)memory_manager.malloc(len, MMTAG_MAP);
    if (dst) memcpy(dst, src, len);
    return dst;
}

static bool _allocate_cells_array(EseMap *map) {
    if (!map || map->width == 0 || map->height == 0 || !map->engine) return false;

    map->cells = (EseMapCell ***)memory_manager.malloc(
        sizeof(EseMapCell **) * map->height, MMTAG_MAP);
    if (!map->cells) return false;

    for (uint32_t y = 0; y < map->height; y++) {
        map->cells[y] = (EseMapCell **)memory_manager.malloc(sizeof(EseMapCell *) * map->width, MMTAG_MAP);
        for (uint32_t x = 0; x < map->width; x++) {
            EseMapCell *cell = ese_mapcell_create((EseLuaEngine *)map->engine);
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
                    ese_mapcell_destroy(map->cells[y][x]);
                }
            }
            memory_manager.free(map->cells[y]);
        }
    }
    memory_manager.free(map->cells);
    map->cells = NULL;
}

static EseMap *_map_make(uint32_t width, uint32_t height, EseMapType type) {
    EseMap *map = (EseMap *)memory_manager.malloc(sizeof(EseMap), MMTAG_MAP);
    map->title = _strdup_safe("Untitled Map");
    map->author = _strdup_safe("Unknown");
    map->version = 0;
    map->type = type;
    map->tileset = NULL;
    map->width = width;
    map->height = height;
    map->cells = NULL;
    map->lua_ref = LUA_NOREF;
    map->lua_ref_count = 0;
    
    return map;
}

void map_lua_push(EseMap *map) {
    log_assert("MAP", map, "map_lua_push called with NULL map");

    log_debug("MAP", "map_lua_push called with map %s", map->title);
    if (map->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseMap **ud = (EseMap **)lua_newuserdata(map->state, sizeof(EseMap *));
        *ud = map;

        // Attach metatable
        luaL_getmetatable(map->state, MAP_PROXY_META);
        lua_setmetatable(map->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(map->state, LUA_REGISTRYINDEX, map->lua_ref);
    }
}

/* ----------------- Lua Methods ----------------- */

static int _map_lua_get_cell(lua_State *L) {
    EseMap *map = map_lua_get(L, 1);
    if (!map) return luaL_error(L, "Invalid Map in get_cell");

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3))
        return luaL_error(L, "get_cell(x, y) requires two numbers");

    uint32_t x = (uint32_t)lua_tonumber(L, 2);
    uint32_t y = (uint32_t)lua_tonumber(L, 3);

    if (x >= map->width || y >= map->height ) {
        lua_pushnil(L);
        return 1;
    }

    EseMapCell *cell = map_get_cell(map, x, y);
    if (!cell) {
        lua_pushnil(L);
        return 1;
    }
    
    // Check if the cell has a valid state pointer for Lua operations
    if (!ese_mapcell_get_state(cell)) {
        lua_pushnil(L);
        return 1;
    }
    
    ese_mapcell_lua_push(cell);
    return 1;
}

static int _map_lua_set_cell(lua_State *L) {
    EseMap *map = map_lua_get(L, 1);
    if (!map) return luaL_error(L, "Invalid Map in set_cell");

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3))
        return luaL_error(L, "set_cell(x, y, cell) requires coordinates");

    uint32_t x = (uint32_t)lua_tonumber(L, 2);
    uint32_t y = (uint32_t)lua_tonumber(L, 3);

    EseMapCell *new_cell = ese_mapcell_lua_get(L, 4);
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
        if (map->tileset) {
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
    // Get from userdata
    EseMap **ud = (EseMap **)luaL_testudata(L, 1, MAP_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseMap *map = *ud;
    if (map) {
        // If lua_ref == LUA_NOREF, there are no more references to this map, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this map was referenced from C and should not be freed.
        if (map->lua_ref == LUA_NOREF) {
            map_destroy(map);
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
    snprintf(buf, sizeof(buf), "Map: %p (title=%s, width=%u, height=%u, type=%s)",
             (void *)map,
             map->title ? map->title : "(null)",
             map->width, map->height,
             map_type_to_string(map->type));
    lua_pushstring(L, buf);
    return 1;
}

/* ----------------- Lua Init ----------------- */

static int _map_lua_new(lua_State *L) {
    profile_start(PROFILE_LUA_MAP_NEW);

    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        profile_cancel(PROFILE_LUA_MAP_NEW);
        return luaL_error(L, "Map.new(width, height, [type]) requires at least two numbers");
    }

    uint32_t width = (uint32_t)lua_tonumber(L, 1);
    uint32_t height = (uint32_t)lua_tonumber(L, 2);
    EseMapType type = MAP_TYPE_GRID;

    if (width == 0 || height == 0) {
        profile_cancel(PROFILE_LUA_MAP_NEW);
        return luaL_error(L, "Map.new(width, height, [type]) width and height must be greater than 0");
    }

    if (lua_isstring(L, 3)) {
        type = map_type_from_string(lua_tostring(L, 3));
    }

    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    EseMap *map = _map_make(width, height, type);
    map->engine = engine;
    map->state = engine->runtime;
    
    // Allocate cells after setting the correct state
    _allocate_cells_array(map);

    // Create userdata directly
    EseMap **ud = (EseMap **)lua_newuserdata(L, sizeof(EseMap *));
    *ud = map;

    // Attach metatable
    luaL_getmetatable(L, MAP_PROXY_META);
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_MAP_NEW, "map_lua_new");
    return 1;
}

void map_lua_init(EseLuaEngine *engine) {
    log_assert("MAP", engine, "map_lua_init called with NULL engine");
    if (luaL_newmetatable(engine->runtime, MAP_PROXY_META)) {
        log_debug("LUA", "Adding MapProxyMeta to engine");
        lua_pushstring(engine->runtime, MAP_PROXY_META);
        lua_setfield(engine->runtime, -2, "__name");
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
    log_assert("MAP", engine, "map_create called with NULL engine");
    EseMap *map = _map_make(width, height, type);
    map->engine = engine;
    map->state = engine->runtime;
    _allocate_cells_array(map);
    return map;
}

void map_destroy(EseMap *map) {
    if (!map) return;
    
    _free_cells_array(map);
    if (map->tileset) tileset_destroy(map->tileset);
    if (map->title) memory_manager.free(map->title);
    if (map->author) memory_manager.free(map->author);
    
    if (map->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(map);
    } else {
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
        map_unref(map);
    }
}

EseMap *map_lua_get(lua_State *L, int idx) {
    log_assert("MAP", L, "map_lua_get called with NULL Lua state");
    
    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseMap **ud = (EseMap **)luaL_testudata(L, idx, MAP_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void map_ref(EseMap *map) {
    log_assert("MAP", map, "map_ref called with NULL map");
    
    if (map->lua_ref == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseMap **ud = (EseMap **)lua_newuserdata(map->state, sizeof(EseMap *));
        *ud = map;

        // Attach metatable
        luaL_getmetatable(map->state, MAP_PROXY_META);
        lua_setmetatable(map->state, -2);

        // Store hard reference to prevent garbage collection
        map->lua_ref = luaL_ref(map->state, LUA_REGISTRYINDEX);
        map->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        map->lua_ref_count++;
    }

    profile_count_add("map_ref_count");
}

void map_unref(EseMap *map) {
    if (!map) return;
    
    if (map->lua_ref != LUA_NOREF && map->lua_ref_count > 0) {
        map->lua_ref_count--;
        
        if (map->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(map->state, LUA_REGISTRYINDEX, map->lua_ref);
            map->lua_ref = LUA_NOREF;
        }
    }

    profile_count_add("map_unref_count");
}

EseMapCell *map_get_cell(const EseMap *map, uint32_t x, uint32_t y) {
    if (!map || !map->cells || x >= map->width || y >= map->height) return NULL;
    return map->cells[y][x];
}

bool map_set_cell(EseMap *map, uint32_t x, uint32_t y, EseMapCell *cell) {
    if (!map || !map->cells || !cell || x >= map->width || y >= map->height) return false;

    // Destroy old cell
    if (map->cells[y][x]) {
        ese_mapcell_destroy(map->cells[y][x]);
    }

    // Replace with new cell (C-owned)
    map->cells[y][x] = ese_mapcell_copy(cell);
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
                ese_mapcell_destroy(map->cells[y][x]);
                map->cells[y][x] = ese_mapcell_copy(old_cells[y][x]);
            }
        }
    }

    // Free old
    for (uint32_t y = 0; y < old_height; y++) {
        if (old_cells[y]) {
            for (uint32_t x = 0; x < old_width; x++) {
                if (old_cells[y][x]) {
                    ese_mapcell_destroy(old_cells[y][x]);
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
