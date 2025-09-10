#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/lua/src/lauxlib.h"
#include "types/map_cell.h"

#define INITIAL_LAYER_CAPACITY 4

// The actual EseMapCell struct definition (private to this file)
typedef struct EseMapCell {
    // Multiple tile layers for this cell position
    uint8_t *tile_ids;       /**< Array of tile IDs for layering */
    size_t layer_count;      /**< Number of layers in this specific cell */
    size_t layer_capacity;   /**< Allocated capacity for tile_ids array */

    // Cell-wide properties
    bool isDynamic;          /**< If false (default), Map component renders all layers; if true, ignored */
    uint32_t flags;          /**< Bitfield for properties (applies to whole cell) */

    // Optional data payload
    void *data;              /**< Game-specific data (JSON, custom struct, etc.) */

    // Lua integration
    lua_State *state;        /**< Lua State this EseMapCell belongs to */
    int lua_ref;             /**< Lua registry reference to its own userdata */
    int lua_ref_count;       /**< Number of times this map cell has been referenced in C */
} EseMapCell;

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseMapCell *_ese_mapcell_make(void);

// Lua metamethods
static int _ese_mapcell_lua_gc(lua_State *L);
static int _ese_mapcell_lua_index(lua_State *L);
static int _ese_mapcell_lua_newindex(lua_State *L);
static int _ese_mapcell_lua_tostring(lua_State *L);

// Lua constructors
static int _ese_mapcell_lua_new(lua_State *L);

// Lua method implementations
static int _ese_mapcell_lua_add_layer(lua_State *L);
static int _ese_mapcell_lua_remove_layer(lua_State *L);
static int _ese_mapcell_lua_get_layer(lua_State *L);
static int _ese_mapcell_lua_set_layer(lua_State *L);
static int _ese_mapcell_lua_clear_layers(lua_State *L);
static int _ese_mapcell_lua_has_flag(lua_State *L);
static int _ese_mapcell_lua_set_flag(lua_State *L);
static int _ese_mapcell_lua_clear_flag(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseMapCell instance with default values
 * 
 * Allocates memory for a new EseMapCell and initializes all fields to safe defaults.
 * The map cell starts with empty layers, no flags, and no Lua state.
 * 
 * @return Pointer to the newly created EseMapCell, or NULL on allocation failure
 */
static EseMapCell *_ese_mapcell_make() {
    EseMapCell *cell = (EseMapCell *)memory_manager.malloc(sizeof(EseMapCell), MMTAG_MAP_CELL);    
    cell->tile_ids = (uint8_t *)memory_manager.malloc(sizeof(uint8_t) * INITIAL_LAYER_CAPACITY, MMTAG_MAP_CELL);
    
    cell->layer_count = 0;
    cell->layer_capacity = INITIAL_LAYER_CAPACITY;
    cell->isDynamic = false;
    cell->flags = 0;
    cell->data = NULL;
    cell->state = NULL;
    cell->lua_ref = LUA_NOREF;
    cell->lua_ref_count = 0;
    return cell;
}

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseMapCell
 * 
 * Handles cleanup when a Lua userdata for an EseMapCell is garbage collected.
 * Only frees the underlying EseMapCell if it has no C-side references.
 * 
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static int _ese_mapcell_lua_gc(lua_State *L) {
    // Get from userdata
    EseMapCell **ud = (EseMapCell **)luaL_testudata(L, 1, MAP_CELL_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseMapCell *cell = *ud;
    if (cell) {
        // If lua_ref == LUA_NOREF, there are no more references to this cell, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this cell was referenced from C and should not be freed.
        if (cell->lua_ref == LUA_NOREF) {
            ese_mapcell_destroy(cell);
        }
    }

    return 0;
}

/**
 * @brief Lua __index metamethod for EseMapCell property access
 * 
 * Provides read access to map cell properties from Lua. When a Lua script
 * accesses cell.property, this function is called to retrieve the values.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid properties, 0 for invalid)
 */
static int _ese_mapcell_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_MAPCELL_INDEX);
    EseMapCell *cell = ese_mapcell_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!cell || !key) {
        profile_cancel(PROFILE_LUA_MAPCELL_INDEX);
        return 0;
    }

    if (strcmp(key, "isDynamic") == 0) {
        lua_pushboolean(L, cell->isDynamic);
        profile_stop(PROFILE_LUA_MAPCELL_INDEX, "mapcell_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "flags") == 0) {
        lua_pushnumber(L, cell->flags);
        profile_stop(PROFILE_LUA_MAPCELL_INDEX, "mapcell_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "layer_count") == 0) {
        lua_pushnumber(L, cell->layer_count);
        profile_stop(PROFILE_LUA_MAPCELL_INDEX, "mapcell_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "add_layer") == 0) {
        lua_pushcfunction(L, _ese_mapcell_lua_add_layer);
        profile_stop(PROFILE_LUA_MAPCELL_INDEX, "mapcell_lua_index (method)");
        return 1;
    } else if (strcmp(key, "remove_layer") == 0) {
        lua_pushcfunction(L, _ese_mapcell_lua_remove_layer);
        profile_stop(PROFILE_LUA_MAPCELL_INDEX, "mapcell_lua_index (method)");
        return 1;
    } else if (strcmp(key, "get_layer") == 0) {
        lua_pushcfunction(L, _ese_mapcell_lua_get_layer);
        profile_stop(PROFILE_LUA_MAPCELL_INDEX, "mapcell_lua_index (method)");
        return 1;
    } else if (strcmp(key, "set_layer") == 0) {
        lua_pushcfunction(L, _ese_mapcell_lua_set_layer);
        profile_stop(PROFILE_LUA_MAPCELL_INDEX, "mapcell_lua_index (method)");
        return 1;
    } else if (strcmp(key, "clear_layers") == 0) {
        lua_pushcfunction(L, _ese_mapcell_lua_clear_layers);
        profile_stop(PROFILE_LUA_MAPCELL_INDEX, "mapcell_lua_index (method)");
        return 1;
    } else if (strcmp(key, "has_flag") == 0) {
        lua_pushcfunction(L, _ese_mapcell_lua_has_flag);
        profile_stop(PROFILE_LUA_MAPCELL_INDEX, "mapcell_lua_index (method)");
        return 1;
    } else if (strcmp(key, "set_flag") == 0) {
        lua_pushcfunction(L, _ese_mapcell_lua_set_flag);
        profile_stop(PROFILE_LUA_MAPCELL_INDEX, "mapcell_lua_index (method)");
        return 1;
    } else if (strcmp(key, "clear_flag") == 0) {
        lua_pushcfunction(L, _ese_mapcell_lua_clear_flag);
        profile_stop(PROFILE_LUA_MAPCELL_INDEX, "mapcell_lua_index (method)");
        return 1;
    }
    profile_stop(PROFILE_LUA_MAPCELL_INDEX, "mapcell_lua_index (invalid)");
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseMapCell property assignment
 * 
 * Provides write access to map cell properties from Lua. When a Lua script
 * assigns to cell.property, this function is called to update the values.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_mapcell_lua_newindex(lua_State *L) {
    profile_start(PROFILE_LUA_MAPCELL_NEWINDEX);
    EseMapCell *cell = ese_mapcell_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!cell || !key) {
        profile_cancel(PROFILE_LUA_MAPCELL_NEWINDEX);
        return 0;
    }

    if (strcmp(key, "isDynamic") == 0) {
        if (lua_type(L, 3) != LUA_TBOOLEAN) {
            profile_cancel(PROFILE_LUA_MAPCELL_NEWINDEX);
            return luaL_error(L, "mapcell.isDynamic must be a boolean");
        }
        cell->isDynamic = lua_toboolean(L, 3);
        profile_stop(PROFILE_LUA_MAPCELL_NEWINDEX, "mapcell_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "flags") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_MAPCELL_NEWINDEX);
            return luaL_error(L, "mapcell.flags must be a number");
        }
        cell->flags = (uint32_t)lua_tonumber(L, 3);
        profile_stop(PROFILE_LUA_MAPCELL_NEWINDEX, "mapcell_lua_newindex (setter)");
        return 0;
    }
    profile_stop(PROFILE_LUA_MAPCELL_NEWINDEX, "mapcell_lua_newindex (invalid)");
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __tostring metamethod for EseMapCell string representation
 * 
 * Converts an EseMapCell to a human-readable string for debugging and display.
 * The format includes the memory address and current properties.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
static int _ese_mapcell_lua_tostring(lua_State *L) {
    EseMapCell *cell = ese_mapcell_lua_get(L, 1);

    if (!cell) {
        lua_pushstring(L, "MapCell: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "MapCell: %p (layers=%zu, flags=%u, dynamic=%d)",
             (void*)cell, cell->layer_count, cell->flags, cell->isDynamic);
    lua_pushstring(L, buf);

    return 1;
}

// Lua constructors
/**
 * @brief Lua constructor function for creating new EseMapCell instances
 * 
 * Creates a new EseMapCell from Lua. This function is called when Lua code 
 * executes `MapCell.new()`. It creates the underlying EseMapCell and returns 
 * a userdata that provides access to the map cell's properties and methods.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the userdata)
 */
static int _ese_mapcell_lua_new(lua_State *L) {
    profile_start(PROFILE_LUA_MAPCELL_NEW);

    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 0) {
        profile_cancel(PROFILE_LUA_MAPCELL_NEW);
        return luaL_error(L, "MapCell.new() takes 0 arguments");
    }

    // Create the map cell
    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    EseMapCell *cell = ese_mapcell_create(engine);
    cell->state = L;

    // Create userdata directly
    EseMapCell **ud = (EseMapCell **)lua_newuserdata(L, sizeof(EseMapCell *));
    *ud = cell;

    // Attach metatable
    luaL_getmetatable(L, MAP_CELL_PROXY_META);
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_MAPCELL_NEW, "mapcell_lua_new");
    return 1;
}

// Lua method implementations
static int _ese_mapcell_lua_add_layer(lua_State *L) {
    EseMapCell *cell = ese_mapcell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in add_layer");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "add_layer(tile_id) requires a number");

    uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
    lua_pushboolean(L, ese_mapcell_add_layer(cell, tile_id));
    return 1;
}

static int _ese_mapcell_lua_remove_layer(lua_State *L) {
    EseMapCell *cell = ese_mapcell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in remove_layer");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "remove_layer(index) requires a number");

    size_t idx = (size_t)lua_tonumber(L, 2);
    lua_pushboolean(L, ese_mapcell_remove_layer(cell, idx));
    return 1;
}

static int _ese_mapcell_lua_get_layer(lua_State *L) {
    EseMapCell *cell = ese_mapcell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in get_layer");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "get_layer(index) requires a number");

    size_t idx = (size_t)lua_tonumber(L, 2);
    lua_pushnumber(L, ese_mapcell_get_layer(cell, idx));
    return 1;
}

static int _ese_mapcell_lua_set_layer(lua_State *L) {
    EseMapCell *cell = ese_mapcell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in set_layer");

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3))
        return luaL_error(L, "set_layer(index, tile_id) requires two numbers");

    size_t idx = (size_t)lua_tonumber(L, 2);
    uint8_t tile_id = (uint8_t)lua_tonumber(L, 3);
    lua_pushboolean(L, ese_mapcell_set_layer(cell, idx, tile_id));
    return 1;
}

static int _ese_mapcell_lua_clear_layers(lua_State *L) {
    EseMapCell *cell = ese_mapcell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in clear_layers");

    ese_mapcell_clear_layers(cell);
    return 0;
}

static int _ese_mapcell_lua_has_flag(lua_State *L) {
    EseMapCell *cell = ese_mapcell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in has_flag");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "has_flag(flag) requires a number");

    uint32_t flag = (uint32_t)lua_tonumber(L, 2);
    lua_pushboolean(L, ese_mapcell_has_flag(cell, flag));
    return 1;
}

static int _ese_mapcell_lua_set_flag(lua_State *L) {
    EseMapCell *cell = ese_mapcell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in set_flag");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "set_flag(flag) requires a number");

    uint32_t flag = (uint32_t)lua_tonumber(L, 2);
    ese_mapcell_set_flag(cell, flag);
    return 0;
}

static int _ese_mapcell_lua_clear_flag(lua_State *L) {
    EseMapCell *cell = ese_mapcell_lua_get(L, 1);
    if (!cell) return luaL_error(L, "Invalid MapCell in clear_flag");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "clear_flag(flag) requires a number");

    uint32_t flag = (uint32_t)lua_tonumber(L, 2);
    ese_mapcell_clear_flag(cell, flag);
    return 0;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseMapCell *ese_mapcell_create(EseLuaEngine *engine) {
    log_assert("MAPCELL", engine, "ese_mapcell_create called with NULL engine");
    EseMapCell *cell = _ese_mapcell_make();
    cell->state = engine->runtime;
    return cell;
}

EseMapCell *ese_mapcell_copy(const EseMapCell *source) {
    log_assert("MAPCELL", source, "ese_mapcell_copy called with NULL source");
    
    EseMapCell *copy = (EseMapCell *)memory_manager.malloc(sizeof(EseMapCell), MMTAG_MAP_CELL);
    if (!copy) return NULL;
    
    copy->tile_ids = (uint8_t *)memory_manager.malloc(
        sizeof(uint8_t) * source->layer_capacity, MMTAG_MAP_CELL);
    if (!copy->tile_ids) {
        memory_manager.free(copy);
        return NULL;
    }
    
    memcpy(copy->tile_ids, source->tile_ids, sizeof(uint8_t) * source->layer_count);
    copy->layer_count = source->layer_count;
    copy->layer_capacity = source->layer_capacity;
    copy->isDynamic = source->isDynamic;
    copy->flags = source->flags;
    copy->data = source->data; // Shallow copy - caller responsible for deep copy if needed
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
    copy->lua_ref_count = 0;
    return copy;
}

void ese_mapcell_destroy(EseMapCell *cell) {
    if (!cell) return;
    
    if (cell->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
        if (cell->tile_ids) {
            memory_manager.free(cell->tile_ids);
        }
        memory_manager.free(cell);
    } else {
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
        ese_mapcell_unref(cell);
    }
}

// Lua integration
void ese_mapcell_lua_init(EseLuaEngine *engine) {
    log_assert("MAPCELL", engine, "ese_mapcell_lua_init called with NULL engine");
    if (luaL_newmetatable(engine->runtime, MAP_CELL_PROXY_META)) {
        log_debug("LUA", "Adding MapCellProxyMeta to engine");
        lua_pushstring(engine->runtime, MAP_CELL_PROXY_META);
        lua_setfield(engine->runtime, -2, "__name");
        lua_pushcfunction(engine->runtime, _ese_mapcell_lua_index);
        lua_setfield(engine->runtime, -2, "__index");               // For property getters
        lua_pushcfunction(engine->runtime, _ese_mapcell_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");            // For property setters
        lua_pushcfunction(engine->runtime, _ese_mapcell_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");                  // For garbage collection
        lua_pushcfunction(engine->runtime, _ese_mapcell_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");            // For printing/debugging
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
    
    // Create global MapCell table with constructor
    lua_getglobal(engine->runtime, "MapCell");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1); // Pop the nil value
        log_debug("LUA", "Creating global MapCell table");
        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _ese_mapcell_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_setglobal(engine->runtime, "MapCell");
    } else {
        lua_pop(engine->runtime, 1); // Pop the existing MapCell table
    }
}

void ese_mapcell_lua_push(EseMapCell *cell) {
    log_assert("MAPCELL", cell, "ese_mapcell_lua_push called with NULL cell");

    if (cell->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseMapCell **ud = (EseMapCell **)lua_newuserdata(cell->state, sizeof(EseMapCell *));
        *ud = cell;

        // Attach metatable
        luaL_getmetatable(cell->state, MAP_CELL_PROXY_META);
        lua_setmetatable(cell->state, -2);
    } else {
        printf("C-owned: getting from registry\n");
        // C-owned: get from registry
        lua_rawgeti(cell->state, LUA_REGISTRYINDEX, cell->lua_ref);
    }
}

EseMapCell *ese_mapcell_lua_get(lua_State *L, int idx) {
    log_assert("MAPCELL", L, "ese_mapcell_lua_get called with NULL Lua state");
    
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

void ese_mapcell_ref(EseMapCell *cell) {
    log_assert("MAPCELL", cell, "ese_mapcell_ref called with NULL cell");
    
    if (cell->lua_ref == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseMapCell **ud = (EseMapCell **)lua_newuserdata(cell->state, sizeof(EseMapCell *));
        *ud = cell;

        // Attach metatable
        luaL_getmetatable(cell->state, MAP_CELL_PROXY_META);
        lua_setmetatable(cell->state, -2);

        // Store hard reference to prevent garbage collection
        cell->lua_ref = luaL_ref(cell->state, LUA_REGISTRYINDEX);
        cell->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        cell->lua_ref_count++;
    }

    profile_count_add("ese_mapcell_ref_count");
}

void ese_mapcell_unref(EseMapCell *cell) {
    if (!cell) return;
    
    if (cell->lua_ref != LUA_NOREF && cell->lua_ref_count > 0) {
        cell->lua_ref_count--;
        
        if (cell->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(cell->state, LUA_REGISTRYINDEX, cell->lua_ref);
            cell->lua_ref = LUA_NOREF;
        }
    }

    profile_count_add("ese_mapcell_unref_count");
}

// Lua-related access
lua_State *ese_mapcell_get_state(const EseMapCell *cell) {
    log_assert("MAPCELL", cell, "ese_mapcell_get_state called with NULL cell");
    return cell->state;
}

int ese_mapcell_get_lua_ref(const EseMapCell *cell) {
    log_assert("MAPCELL", cell, "ese_mapcell_get_lua_ref called with NULL cell");
    return cell->lua_ref;
}

int ese_mapcell_get_lua_ref_count(const EseMapCell *cell) {
    log_assert("MAPCELL", cell, "ese_mapcell_get_lua_ref_count called with NULL cell");
    return cell->lua_ref_count;
}

size_t ese_mapcell_sizeof(void) {
    return sizeof(EseMapCell);
}

// Tile/Flag API
bool ese_mapcell_add_layer(EseMapCell *cell, uint8_t tile_id) {
    if (!cell) return false;

    if (cell->layer_count >= cell->layer_capacity) {
        size_t new_capacity = cell->layer_capacity * 2;
        uint8_t *new_array = (uint8_t *)memory_manager.realloc(
            cell->tile_ids, sizeof(uint8_t) * new_capacity, MMTAG_MAP_CELL);
        if (!new_array) return false;
        cell->tile_ids = new_array;
        cell->layer_capacity = new_capacity;
    }

    cell->tile_ids[cell->layer_count++] = tile_id;
    return true;
}

bool ese_mapcell_remove_layer(EseMapCell *cell, size_t layer_index) {
    if (!cell || layer_index >= cell->layer_count) return false;

    for (size_t i = layer_index; i < cell->layer_count - 1; i++) {
        cell->tile_ids[i] = cell->tile_ids[i + 1];
    }
    cell->layer_count--;
    return true;
}

uint8_t ese_mapcell_get_layer(const EseMapCell *cell, size_t layer_index) {
    if (!cell || layer_index >= cell->layer_count) return 0;
    return cell->tile_ids[layer_index];
}

bool ese_mapcell_set_layer(EseMapCell *cell, size_t layer_index, uint8_t tile_id) {
    if (!cell || layer_index >= cell->layer_count) return false;
    cell->tile_ids[layer_index] = tile_id;
    return true;
}

void ese_mapcell_clear_layers(EseMapCell *cell) {
    if (cell) cell->layer_count = 0;
}

bool ese_mapcell_has_layers(const EseMapCell *cell) {
    if (!cell) return false;
    return cell->layer_count > 0;
}

size_t ese_mapcell_get_layer_count(const EseMapCell *cell) {
    if (!cell) return 0;
    return cell->layer_count;
}

bool ese_mapcell_has_flag(const EseMapCell *cell, uint32_t flag) {
    if (!cell) return false;
    return (cell->flags & flag) != 0;
}

void ese_mapcell_set_flag(EseMapCell *cell, uint32_t flag) {
    if (cell) cell->flags |= flag;
}

void ese_mapcell_clear_flag(EseMapCell *cell, uint32_t flag) {
    if (cell) cell->flags &= ~flag;
}

// Property access
void ese_mapcell_set_is_dynamic(EseMapCell *cell, bool isDynamic) {
    if (cell) cell->isDynamic = isDynamic;
}

bool ese_mapcell_get_is_dynamic(const EseMapCell *cell) {
    if (!cell) return false;
    return cell->isDynamic;
}

void ese_mapcell_set_flags(EseMapCell *cell, uint32_t flags) {
    if (cell) cell->flags = flags;
}

uint32_t ese_mapcell_get_flags(const EseMapCell *cell) {
    if (!cell) return 0;
    return cell->flags;
}