#include <string.h>
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "core/memory_manager.h"
#include "types/map_private.h"
#include "types/map_cell.h"
#include "types/tileset.h"
#include "types/map_lua.h"

/* --- Forward declarations --------------------------------------------------------------------- */

// Core allocation/setup
/// Free the 2D cells array and all contained cells.
static void _free_cells_array(EseMap *map);
/// Cell change callback that forwards notifications to the parent map.
static void _ese_map_on_cell_changed(EseMapCell *cell, void *userdata);

// Private static setters for Lua state management
static void _ese_map_set_lua_ref(EseMap *map, int lua_ref);
static void _ese_map_set_lua_ref_count(EseMap *map, int lua_ref_count);
static void _ese_map_set_state(EseMap *map, lua_State *state);
static void _ese_map_set_engine(EseMap *map, EseLuaEngine *engine);


/* --- Internal Helpers ------------------------------------------------------------------------- */

static void _ese_map_on_cell_changed(EseMapCell *cell, void *userdata) {
    EseMap *map = (EseMap *)userdata;
    if (!map) return;
    _ese_map_notify_watchers(map);
}

bool _allocate_cells_array(EseMap *map) {
    if (!map || map->width == 0 || map->height == 0 || !ese_map_get_engine(map)) return false;

    map->cells = (EseMapCell ***)memory_manager.malloc(
        sizeof(EseMapCell **) * map->height, MMTAG_MAP);
    if (!map->cells) return false;

    for (uint32_t y = 0; y < map->height; y++) {
        map->cells[y] = (EseMapCell **)memory_manager.malloc(sizeof(EseMapCell *) * map->width, MMTAG_MAP);
        for (uint32_t x = 0; x < map->width; x++) {
            EseMapCell *cell = ese_map_cell_create((EseLuaEngine *)ese_map_get_engine(map), map);
            map->cells[y][x] = cell;
            ese_map_cell_add_watcher(cell, _ese_map_on_cell_changed, map);
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
                    ese_map_cell_destroy(map->cells[y][x]);
                }
            }
            memory_manager.free(map->cells[y]);
        }
    }
    memory_manager.free(map->cells);
    map->cells = NULL;
}

// Private static setters for Lua state management
static void _ese_map_set_lua_ref(EseMap *map, int lua_ref) {
    map->lua_ref = lua_ref;
}

static void _ese_map_set_lua_ref_count(EseMap *map, int lua_ref_count) {
    map->lua_ref_count = lua_ref_count;
}

static void _ese_map_set_state(EseMap *map, lua_State *state) {
    map->state = state;
}

static void _ese_map_set_engine(EseMap *map, EseLuaEngine *engine) {
    map->engine = engine;
}

EseMap *_ese_map_make(uint32_t width, uint32_t height, EseMapType type) {
    EseMap *map = (EseMap *)memory_manager.malloc(sizeof(EseMap), MMTAG_MAP);
    map->title = memory_manager.strdup("Untitled Map", MMTAG_MAP);
    map->author = memory_manager.strdup("Unknown", MMTAG_MAP);
    map->version = 0;
    map->type = type;
    map->tileset = NULL;
    map->width = width;
    map->height = height;
    map->cells = NULL;
    _ese_map_set_lua_ref(map, LUA_NOREF);
    _ese_map_set_lua_ref_count(map, 0);
    map->destroyed = false;

    map->layer_count = 0;
    map->layer_count_dirty = true;
    
    // Watchers
    map->watchers = NULL;
    map->watcher_userdata = NULL;
    map->watcher_count = 0;
    map->watcher_capacity = 0;
    
    return map;
}

// Notify all map watchers helper
void _ese_map_notify_watchers(EseMap *map) {
    if (!map || map->watcher_count == 0) return;
    for (size_t i = 0; i < map->watcher_count; i++) {
        if (map->watchers[i]) {
            map->watchers[i](map, map->watcher_userdata[i]);
        }
    }
}

void ese_map_lua_push(EseMap *map) {
    log_assert("MAP", map, "ese_map_lua_push called with NULL map");

    log_verbose("MAP", "ese_map_lua_push called with map %s", map->title);
    if (ese_map_get_lua_ref(map) == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseMap **ud = (EseMap **)lua_newuserdata(ese_map_get_state(map), sizeof(EseMap *));
        *ud = map;

        // Attach metatable
        luaL_getmetatable(ese_map_get_state(map), MAP_PROXY_META);
        lua_setmetatable(ese_map_get_state(map), -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(ese_map_get_state(map), LUA_REGISTRYINDEX, ese_map_get_lua_ref(map));
    }
}

size_t ese_map_get_width(EseMap *map) {
    log_assert("MAP", map, "ese_map_get_width called with NULL map");
    return map->width;
}

size_t ese_map_get_height(EseMap *map) {
    log_assert("MAP", map, "ese_map_get_height called with NULL map");
    return map->height;
}

EseMapType ese_map_get_type(EseMap *map) {
    log_assert("MAP", map, "ese_map_get_type called with NULL map");

    return map->type;
}

EseTileSet *ese_map_get_tileset(EseMap *map) {
    log_assert("MAP", map, "ese_map_get_tileset called with NULL map");

    return map->tileset;
}

size_t ese_map_get_layer_count(EseMap *map) {
    log_assert("MAP", map, "ese_map_get_layer_count called with NULL map");

    if (!map->layer_count_dirty) {
        return map->layer_count;
    }

    map->layer_count = 0;
    for (uint32_t y = 0; y < map->height; y++) {
        for (uint32_t x = 0; x < map->width; x++) {
            EseMapCell *cell = map->cells[y][x];
            size_t count = ese_map_cell_get_layer_count(cell);
            if (cell && count > map->layer_count) {
                map->layer_count = count;
            }
        }
    }
    map->layer_count_dirty = false;

    return map->layer_count;
}

void _ese_map_set_layer_count_dirty(EseMap *map) {
    log_assert("MAP", map, "ese_map_set_layer_count_dirty called with NULL map");
    
    map->layer_count_dirty = true;
    _ese_map_notify_watchers(map);
}

// Lua-related access
lua_State *ese_map_get_state(const EseMap *map) {
    log_assert("MAP", map, "ese_map_get_state called with NULL map");
    return map->state;
}

int ese_map_get_lua_ref(const EseMap *map) {
    log_assert("MAP", map, "ese_map_get_lua_ref called with NULL map");
    return map->lua_ref;
}

int ese_map_get_lua_ref_count(const EseMap *map) {
    log_assert("MAP", map, "ese_map_get_lua_ref_count called with NULL map");
    return map->lua_ref_count;
}

EseLuaEngine *ese_map_get_engine(const EseMap *map) {
    log_assert("MAP", map, "ese_map_get_engine called with NULL map");
    return map->engine;
}

const char *ese_map_get_title(const EseMap *map) {
    log_assert("MAP", map, "ese_map_get_title called with NULL map");
    return map->title;
}

const char *ese_map_get_author(const EseMap *map) {
    log_assert("MAP", map, "ese_map_get_author called with NULL map");
    return map->author;
}

int ese_map_get_version(const EseMap *map) {
    log_assert("MAP", map, "ese_map_get_version called with NULL map");
    return map->version;
}

void ese_map_set_type(EseMap *map, EseMapType type) {
    log_assert("MAP", map, "ese_map_set_type called with NULL map");
    map->type = type;
    _ese_map_notify_watchers(map);
}

void ese_map_set_engine(EseMap *map, EseLuaEngine *engine) {
    log_assert("MAP", map, "ese_map_set_engine called with NULL map");
    map->engine = engine;
}

void ese_map_set_state(EseMap *map, lua_State *state) {
    log_assert("MAP", map, "ese_map_set_state called with NULL map");
    map->state = state;
}

void ese_map_lua_init(EseLuaEngine *engine) {
    log_assert("MAP", engine, "ese_map_lua_init called with NULL engine");
    
    _ese_map_lua_init(engine);
}

/* --- C API ------------------------------------------------------------------------------------ */

EseMap *ese_map_create(EseLuaEngine *engine, uint32_t width, uint32_t height, EseMapType type, bool c_only) {
    log_assert("MAP", engine, "ese_map_create called with NULL engine");
    EseMap *map = _ese_map_make(width, height, type);
    _ese_map_set_engine(map, engine);
    _ese_map_set_state(map, engine->runtime);
    _allocate_cells_array(map);
    return map;
}

void ese_map_destroy(EseMap *map) {
    if (!map || map->destroyed) return;

    // If Lua still has a hard reference, just drop one ref and return.
    // Avoid freeing internals while Lua may still access the map.
    if (map->lua_ref != LUA_NOREF) {
        ese_map_unref(map);
        return;
    }

    map->destroyed = true;

    _free_cells_array(map);
    if (map->tileset) ese_tileset_destroy(map->tileset);
    if (map->title) memory_manager.free(map->title);
    if (map->author) memory_manager.free(map->author);
    if (map->watchers) memory_manager.free(map->watchers);
    if (map->watcher_userdata) memory_manager.free(map->watcher_userdata);

    memory_manager.free(map);
}

EseMap *ese_map_lua_get(lua_State *L, int idx) {
    log_assert("MAP", L, "ese_map_lua_get called with NULL Lua state");
    
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

void ese_map_ref(EseMap *map) {
    log_assert("MAP", map, "ese_map_ref called with NULL map");
    
    if (ese_map_get_lua_ref(map) == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseMap **ud = (EseMap **)lua_newuserdata(ese_map_get_state(map), sizeof(EseMap *));
        *ud = map;

        // Attach metatable
        luaL_getmetatable(ese_map_get_state(map), MAP_PROXY_META);
        lua_setmetatable(ese_map_get_state(map), -2);

        // Store hard reference to prevent garbage collection
        _ese_map_set_lua_ref(map, luaL_ref(ese_map_get_state(map), LUA_REGISTRYINDEX));
        _ese_map_set_lua_ref_count(map, 1);
    } else {
        // Already referenced - just increment count
        _ese_map_set_lua_ref_count(map, ese_map_get_lua_ref_count(map) + 1);
    }

    profile_count_add("ese_map_ref_count");
}

void ese_map_unref(EseMap *map) {
    if (!map) return;
    
    if (ese_map_get_lua_ref(map) != LUA_NOREF && ese_map_get_lua_ref_count(map) > 0) {
        _ese_map_set_lua_ref_count(map, ese_map_get_lua_ref_count(map) - 1);
        
        if (ese_map_get_lua_ref_count(map) == 0) {
            // No more references - remove from registry
            luaL_unref(ese_map_get_state(map), LUA_REGISTRYINDEX, ese_map_get_lua_ref(map));
            _ese_map_set_lua_ref(map, LUA_NOREF);
        }
    }

    profile_count_add("ese_map_unref_count");
}

EseMapCell *ese_map_get_cell(const EseMap *map, uint32_t x, uint32_t y) {
    if (!map || !map->cells || x >= map->width || y >= map->height) return NULL;
    return map->cells[y][x];
}

bool ese_map_set_title(EseMap *map, const char *title) {
    if (!map) return false;
    if (map->title) memory_manager.free(map->title);
    map->title = memory_manager.strdup(title, MMTAG_MAP);
    _ese_map_notify_watchers(map);
    return map->title != NULL;
}

bool ese_map_set_author(EseMap *map, const char *author) {
    if (!map) return false;
    if (map->author) memory_manager.free(map->author);
    map->author = memory_manager.strdup(author, MMTAG_MAP);
    _ese_map_notify_watchers(map);
    return map->author != NULL;
}

void ese_map_set_version(EseMap *map, int version) {
    if (map) { map->version = version; _ese_map_notify_watchers(map); }
}

void ese_map_set_tileset(EseMap *map, EseTileSet *tileset) {
    if (map) { map->tileset = tileset; _ese_map_notify_watchers(map); }
}

bool ese_map_resize(EseMap *map, uint32_t new_width, uint32_t new_height) {
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
                ese_map_cell_destroy(map->cells[y][x]);
                map->cells[y][x] = ese_map_cell_copy(old_cells[y][x]);
                ese_map_cell_add_watcher(map->cells[y][x], _ese_map_on_cell_changed, map);
            }
        }
    }

    // Free old
    for (uint32_t y = 0; y < old_height; y++) {
        if (old_cells[y]) {
            for (uint32_t x = 0; x < old_width; x++) {
                if (old_cells[y][x]) {
                    ese_map_cell_destroy(old_cells[y][x]);
                }
            }
            memory_manager.free(old_cells[y]);
        }
    }
    memory_manager.free(old_cells);

    _ese_map_notify_watchers(map);
    return true;
}

const char *ese_map_type_to_string(EseMapType type) {
    switch (type) {
        case MAP_TYPE_GRID: return "grid";
        case MAP_TYPE_HEX_POINT_UP: return "hex_point_up";
        case MAP_TYPE_HEX_FLAT_UP: return "hex_flat_up";
        case MAP_TYPE_ISO: return "iso";
        default: return "grid";
    }
}

/* --- Watcher API ------------------------------------------------------------------------------ */

bool ese_map_add_watcher(EseMap *map, EseMapWatcherCallback callback, void *userdata) {
    log_assert("MAP", map, "ese_map_add_watcher called with NULL map");
    log_assert("MAP", callback, "ese_map_add_watcher called with NULL callback");

    if (map->watcher_count == 0) {
        map->watcher_capacity = 4;
        map->watchers = memory_manager.malloc(sizeof(EseMapWatcherCallback) * map->watcher_capacity, MMTAG_MAP);
        map->watcher_userdata = memory_manager.malloc(sizeof(void*) * map->watcher_capacity, MMTAG_MAP);
        map->watcher_count = 0;
    }

    if (map->watcher_count >= map->watcher_capacity) {
        size_t new_capacity = map->watcher_capacity * 2;
        EseMapWatcherCallback *new_watchers = memory_manager.realloc(
            map->watchers,
            sizeof(EseMapWatcherCallback) * new_capacity,
            MMTAG_MAP
        );
        void **new_userdata = memory_manager.realloc(
            map->watcher_userdata,
            sizeof(void*) * new_capacity,
            MMTAG_MAP
        );
        if (!new_watchers || !new_userdata) return false;
        map->watchers = new_watchers;
        map->watcher_userdata = new_userdata;
        map->watcher_capacity = new_capacity;
    }

    map->watchers[map->watcher_count] = callback;
    map->watcher_userdata[map->watcher_count] = userdata;
    map->watcher_count++;
    return true;
}

bool ese_map_remove_watcher(EseMap *map, EseMapWatcherCallback callback, void *userdata) {
    log_assert("MAP", map, "ese_map_remove_watcher called with NULL map");
    log_assert("MAP", callback, "ese_map_remove_watcher called with NULL callback");

    for (size_t i = 0; i < map->watcher_count; i++) {
        if (map->watchers[i] == callback && map->watcher_userdata[i] == userdata) {
            for (size_t j = i; j < map->watcher_count - 1; j++) {
                map->watchers[j] = map->watchers[j + 1];
                map->watcher_userdata[j] = map->watcher_userdata[j + 1];
            }
            map->watcher_count--;
            return true;
        }
    }
    return false;
}

EseMapType ese_map_type_from_string(const char *type_str) {
    if (!type_str) return MAP_TYPE_GRID;
    if (strcmp(type_str, "grid") == 0) return MAP_TYPE_GRID;
    if (strcmp(type_str, "hex_point_up") == 0) return MAP_TYPE_HEX_POINT_UP;
    if (strcmp(type_str, "hex_flat_up") == 0) return MAP_TYPE_HEX_FLAT_UP;
    if (strcmp(type_str, "iso") == 0) return MAP_TYPE_ISO;
    return MAP_TYPE_GRID;
}
