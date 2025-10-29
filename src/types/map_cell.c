#include "types/map_cell.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/map_cell_lua.h"
#include "types/map_private.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/lua/src/lauxlib.h"
#include <stdio.h>
#include <string.h>

/* --- Defines
 * ----------------------------------------------------------------------------------
 */

#define INITIAL_LAYER_CAPACITY 4

/* --- Structs
 * ----------------------------------------------------------------------------------
 */
// Private EseMapCell definition (internal to this translation unit)
typedef struct EseMapCell {
    EseMap *map; /** Pointer to the EseMap this cell belongs to */

    // Multiple tile layers for this cell position
    int *tile_ids;         /** Array of tile IDs for layering */
    size_t layer_count;    /** Number of layers in this specific cell */
    size_t layer_capacity; /** Allocated capacity for tile_ids array */

    // Cell-wide properties
    bool isDynamic; /** If false (default), Map component renders all layers; if
                       true, ignored */
    uint32_t flags; /** Bitfield for properties (applies to whole cell) */

    // Optional data payload
    void *data; /** Game-specific data (JSON, custom struct, etc.) */

    // Lua integration
    lua_State *state;  /** Lua State this EseMapCell belongs to */
    int lua_ref;       /** Lua registry reference to its own userdata */
    int lua_ref_count; /** Number of times this map cell has been referenced in
                        * C
                        */

    // Watcher system
    EseMapCellWatcherCallback *watchers; /** Array of watcher callbacks */
    void **watcher_userdata;             /** Array of userdata for each watcher */
    size_t watcher_count;                /** Number of registered watchers */
    size_t watcher_capacity;             /** Capacity of the watcher arrays */
} EseMapCell;

/* --- Forward declarations
 * --------------------------------------------------------------------- */

// Core helpers
/// Create and initialize a new MapCell with default values.
static EseMapCell *_ese_map_cell_make(EseMap *map);

// Watcher helpers
/// Notify all registered MapCell watchers of a change.
static void _ese_map_cell_notify_watchers(EseMapCell *cell);

// Private static setters for Lua state management
static void _ese_map_cell_set_lua_ref(EseMapCell *cell, int lua_ref);
static void _ese_map_cell_set_lua_ref_count(EseMapCell *cell, int lua_ref_count);
static void _ese_map_cell_set_state(EseMapCell *cell, lua_State *state);

/* --- Internal Helpers
 * ------------------------------------------------------------------------- */

// Core helpers
static EseMapCell *_ese_map_cell_make(EseMap *map) {
    EseMapCell *cell = (EseMapCell *)memory_manager.malloc(sizeof(EseMapCell), MMTAG_MAP_CELL);
    cell->tile_ids =
        (int *)memory_manager.malloc(sizeof(int) * INITIAL_LAYER_CAPACITY, MMTAG_MAP_CELL);

    cell->map = map;
    cell->layer_count = 0;
    cell->layer_capacity = INITIAL_LAYER_CAPACITY;
    cell->isDynamic = false;
    cell->flags = 0;
    cell->data = NULL;
    _ese_map_cell_set_state(cell, NULL);
    _ese_map_cell_set_lua_ref(cell, LUA_NOREF);
    _ese_map_cell_set_lua_ref_count(cell, 0);
    cell->watchers = NULL;
    cell->watcher_userdata = NULL;
    cell->watcher_count = 0;
    cell->watcher_capacity = 0;
    return cell;
}

// Private static setters for Lua state management
static void _ese_map_cell_set_lua_ref(EseMapCell *cell, int lua_ref) { cell->lua_ref = lua_ref; }

static void _ese_map_cell_set_lua_ref_count(EseMapCell *cell, int lua_ref_count) {
    cell->lua_ref_count = lua_ref_count;
}

static void _ese_map_cell_set_state(EseMapCell *cell, lua_State *state) { cell->state = state; }

/* --- C API
 * ------------------------------------------------------------------------------------
 */

// Core lifecycle
EseMapCell *ese_map_cell_create(EseLuaEngine *engine, EseMap *map) {
    log_assert("MAPCELL", engine, "ese_map_cell_create called with NULL engine");
    log_assert("MAPCELL", map, "ese_map_cell_create called with NULL map");
    EseMapCell *cell = _ese_map_cell_make(map);
    _ese_map_cell_set_state(cell, engine->runtime);
    return cell;
}

EseMapCell *ese_map_cell_copy(const EseMapCell *source) {
    log_assert("MAPCELL", source, "ese_map_cell_copy called with NULL source");

    EseMapCell *copy = (EseMapCell *)memory_manager.malloc(sizeof(EseMapCell), MMTAG_MAP_CELL);
    if (!copy)
        return NULL;

    copy->tile_ids =
        (int *)memory_manager.malloc(sizeof(int) * source->layer_capacity, MMTAG_MAP_CELL);
    if (!copy->tile_ids) {
        memory_manager.free(copy);
        return NULL;
    }

    memcpy(copy->tile_ids, source->tile_ids, sizeof(int) * source->layer_count);
    copy->layer_count = source->layer_count;
    copy->layer_capacity = source->layer_capacity;
    copy->isDynamic = source->isDynamic;
    copy->flags = source->flags;
    copy->data = source->data; // Shallow copy - caller responsible for deep copy if needed
    _ese_map_cell_set_state(copy, source->state);
    _ese_map_cell_set_lua_ref(copy, LUA_NOREF);
    _ese_map_cell_set_lua_ref_count(copy, 0);
    copy->watchers = NULL;
    copy->watcher_userdata = NULL;
    copy->watcher_count = 0;
    copy->watcher_capacity = 0;
    return copy;
}

void ese_map_cell_destroy(EseMapCell *cell) {
    if (!cell)
        return;

    if (cell->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
        if (cell->tile_ids) {
            memory_manager.free(cell->tile_ids);
        }
        if (cell->watchers) {
            memory_manager.free(cell->watchers);
            cell->watchers = NULL;
        }
        if (cell->watcher_userdata) {
            memory_manager.free(cell->watcher_userdata);
            cell->watcher_userdata = NULL;
        }
        memory_manager.free(cell);
    } else {
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
        ese_map_cell_unref(cell);
    }
}

/* --- Lua Init
 * ---------------------------------------------------------------------------------
 */
void ese_map_cell_lua_init(EseLuaEngine *engine) {
    log_assert("MAPCELL", engine, "ese_map_cell_lua_init called with NULL engine");

    _ese_map_cell_lua_init(engine);
}

/* --- C API
 * ------------------------------------------------------------------------------------
 */
void ese_map_cell_lua_push(EseMapCell *cell) {
    log_assert("MAPCELL", cell, "ese_map_cell_lua_push called with NULL cell");

    if (ese_map_cell_get_lua_ref(cell) == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseMapCell **ud =
            (EseMapCell **)lua_newuserdata(ese_map_cell_get_state(cell), sizeof(EseMapCell *));
        *ud = cell;

        // Attach metatable
        luaL_getmetatable(ese_map_cell_get_state(cell), MAP_CELL_PROXY_META);
        lua_setmetatable(ese_map_cell_get_state(cell), -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(ese_map_cell_get_state(cell), LUA_REGISTRYINDEX,
                    ese_map_cell_get_lua_ref(cell));
    }
}

EseMapCell *ese_map_cell_lua_get(lua_State *L, int idx) {
    log_assert("MAPCELL", L, "ese_map_cell_lua_get called with NULL Lua state");

    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }

    // Get the userdata and check metatable
    EseMapCell **ud = (EseMapCell **)luaL_testudata(L, idx, MAP_CELL_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }

    return *ud;
}

void ese_map_cell_ref(EseMapCell *cell) {
    log_assert("MAPCELL", cell, "ese_map_cell_ref called with NULL cell");

    if (ese_map_cell_get_lua_ref(cell) == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseMapCell **ud =
            (EseMapCell **)lua_newuserdata(ese_map_cell_get_state(cell), sizeof(EseMapCell *));
        *ud = cell;

        // Attach metatable
        luaL_getmetatable(ese_map_cell_get_state(cell), MAP_CELL_PROXY_META);
        lua_setmetatable(ese_map_cell_get_state(cell), -2);

        // Store hard reference to prevent garbage collection
        _ese_map_cell_set_lua_ref(cell, luaL_ref(ese_map_cell_get_state(cell), LUA_REGISTRYINDEX));
        _ese_map_cell_set_lua_ref_count(cell, 1);
    } else {
        // Already referenced - just increment count
        _ese_map_cell_set_lua_ref_count(cell, ese_map_cell_get_lua_ref_count(cell) + 1);
    }

    profile_count_add("ese_map_cell_ref_count");
}

void ese_map_cell_unref(EseMapCell *cell) {
    if (!cell)
        return;

    if (ese_map_cell_get_lua_ref(cell) != LUA_NOREF && ese_map_cell_get_lua_ref_count(cell) > 0) {
        _ese_map_cell_set_lua_ref_count(cell, ese_map_cell_get_lua_ref_count(cell) - 1);

        if (ese_map_cell_get_lua_ref_count(cell) == 0) {
            // No more references - remove from registry
            luaL_unref(ese_map_cell_get_state(cell), LUA_REGISTRYINDEX,
                       ese_map_cell_get_lua_ref(cell));
            _ese_map_cell_set_lua_ref(cell, LUA_NOREF);
        }
    }

    profile_count_add("ese_map_cell_unref_count");
}

// Lua-related access
lua_State *ese_map_cell_get_state(const EseMapCell *cell) {
    log_assert("MAPCELL", cell, "ese_map_cell_get_state called with NULL cell");
    return cell->state;
}

int ese_map_cell_get_lua_ref(const EseMapCell *cell) {
    log_assert("MAPCELL", cell, "ese_map_cell_get_lua_ref called with NULL cell");
    return cell->lua_ref;
}

int ese_map_cell_get_lua_ref_count(const EseMapCell *cell) {
    log_assert("MAPCELL", cell, "ese_map_cell_get_lua_ref_count called with NULL cell");
    return cell->lua_ref_count;
}

size_t ese_map_cell_sizeof(void) { return sizeof(EseMapCell); }

/* --- Tile/Flag API
 * ---------------------------------------------------------------------------
 */
bool ese_map_cell_add_layer(EseMapCell *cell, int tile_id) {
    if (!cell)
        return false;

    if (cell->layer_count >= cell->layer_capacity) {
        size_t new_capacity = cell->layer_capacity * 2;
        int *new_array = (int *)memory_manager.realloc(cell->tile_ids, sizeof(int) * new_capacity,
                                                       MMTAG_MAP_CELL);
        if (!new_array)
            return false;
        cell->tile_ids = new_array;
        cell->layer_capacity = new_capacity;
    }

    cell->tile_ids[cell->layer_count++] = tile_id;
    _ese_map_set_layer_count_dirty(cell->map);
    _ese_map_cell_notify_watchers(cell);
    return true;
}

bool ese_map_cell_remove_layer(EseMapCell *cell, size_t layer_index) {
    if (!cell || layer_index >= cell->layer_count)
        return false;

    for (size_t i = layer_index; i < cell->layer_count - 1; i++) {
        cell->tile_ids[i] = cell->tile_ids[i + 1];
    }
    cell->layer_count--;
    _ese_map_set_layer_count_dirty(cell->map);
    _ese_map_cell_notify_watchers(cell);
    return true;
}

int ese_map_cell_get_layer(const EseMapCell *cell, size_t layer_index) {
    if (!cell || layer_index >= cell->layer_count)
        return 0;
    return cell->tile_ids[layer_index];
}

bool ese_map_cell_set_layer(EseMapCell *cell, size_t layer_index, uint8_t tile_id) {
    if (!cell || layer_index >= cell->layer_count)
        return false;
    cell->tile_ids[layer_index] = tile_id;
    _ese_map_cell_notify_watchers(cell);
    return true;
}

void ese_map_cell_clear_layers(EseMapCell *cell) {
    if (cell) {
        cell->layer_count = 0;
        _ese_map_set_layer_count_dirty(cell->map);
        _ese_map_cell_notify_watchers(cell);
    }
}

bool ese_map_cell_has_layers(const EseMapCell *cell) {
    if (!cell)
        return false;
    return cell->layer_count > 0;
}

size_t ese_map_cell_get_layer_count(const EseMapCell *cell) {
    if (!cell)
        return 0;
    return cell->layer_count;
}

/* --- Watcher API
 * ------------------------------------------------------------------------------
 */
bool ese_map_cell_add_watcher(EseMapCell *cell, EseMapCellWatcherCallback callback,
                              void *userdata) {
    log_assert("MAPCELL", cell, "ese_map_cell_add_watcher called with NULL cell");
    log_assert("MAPCELL", callback, "ese_map_cell_add_watcher called with NULL callback");

    if (cell->watcher_count == 0) {
        cell->watcher_capacity = 4;
        cell->watchers = memory_manager.malloc(
            sizeof(EseMapCellWatcherCallback) * cell->watcher_capacity, MMTAG_MAP_CELL);
        cell->watcher_userdata =
            memory_manager.malloc(sizeof(void *) * cell->watcher_capacity, MMTAG_MAP_CELL);
        cell->watcher_count = 0;
    }

    if (cell->watcher_count >= cell->watcher_capacity) {
        size_t new_capacity = cell->watcher_capacity * 2;
        EseMapCellWatcherCallback *new_watchers = memory_manager.realloc(
            cell->watchers, sizeof(EseMapCellWatcherCallback) * new_capacity, MMTAG_MAP_CELL);
        void **new_userdata = memory_manager.realloc(cell->watcher_userdata,
                                                     sizeof(void *) * new_capacity, MMTAG_MAP_CELL);
        if (!new_watchers || !new_userdata)
            return false;
        cell->watchers = new_watchers;
        cell->watcher_userdata = new_userdata;
        cell->watcher_capacity = new_capacity;
    }

    cell->watchers[cell->watcher_count] = callback;
    cell->watcher_userdata[cell->watcher_count] = userdata;
    cell->watcher_count++;
    return true;
}

bool ese_map_cell_remove_watcher(EseMapCell *cell, EseMapCellWatcherCallback callback,
                                 void *userdata) {
    log_assert("MAPCELL", cell, "ese_map_cell_remove_watcher called with NULL cell");
    log_assert("MAPCELL", callback, "ese_map_cell_remove_watcher called with NULL callback");

    for (size_t i = 0; i < cell->watcher_count; i++) {
        if (cell->watchers[i] == callback && cell->watcher_userdata[i] == userdata) {
            for (size_t j = i; j < cell->watcher_count - 1; j++) {
                cell->watchers[j] = cell->watchers[j + 1];
                cell->watcher_userdata[j] = cell->watcher_userdata[j + 1];
            }
            cell->watcher_count--;
            return true;
        }
    }
    return false;
}

static void _ese_map_cell_notify_watchers(EseMapCell *cell) {
    if (!cell || cell->watcher_count == 0)
        return;
    for (size_t i = 0; i < cell->watcher_count; i++) {
        if (cell->watchers[i]) {
            cell->watchers[i](cell, cell->watcher_userdata[i]);
        }
    }
}

bool ese_map_cell_has_flag(const EseMapCell *cell, uint32_t flag) {
    if (!cell)
        return false;
    return (cell->flags & flag) != 0;
}

void ese_map_cell_set_flag(EseMapCell *cell, uint32_t flag) {
    if (cell) {
        cell->flags |= flag;
        _ese_map_cell_notify_watchers(cell);
    }
}

void ese_map_cell_clear_flag(EseMapCell *cell, uint32_t flag) {
    if (cell) {
        cell->flags &= ~flag;
        _ese_map_cell_notify_watchers(cell);
    }
}

// Property access
void ese_map_cell_set_is_dynamic(EseMapCell *cell, bool isDynamic) {
    if (cell) {
        cell->isDynamic = isDynamic;
        _ese_map_cell_notify_watchers(cell);
    }
}

bool ese_map_cell_get_is_dynamic(const EseMapCell *cell) {
    if (!cell)
        return false;
    return cell->isDynamic;
}

void ese_map_cell_set_flags(EseMapCell *cell, uint32_t flags) {
    if (cell) {
        cell->flags = flags;
        _ese_map_cell_notify_watchers(cell);
    }
}

uint32_t ese_map_cell_get_flags(const EseMapCell *cell) {
    if (!cell)
        return 0;
    return cell->flags;
}