#include <string.h>
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "core/memory_manager.h"
#include "types/map_cell.h"

// Initial capacity for tile layers
#define INITIAL_LAYER_CAPACITY 4

// Forward declarations for static functions
static void _mapcell_lua_register(EseMapCell *cell, bool push_to_stack);
static int _mapcell_lua_push(lua_State *L, EseMapCell *cell);
static int _mapcell_lua_new(lua_State *L);
static int _mapcell_lua_index(lua_State *L);
static int _mapcell_lua_newindex(lua_State *L);
static int _mapcell_lua_add_layer(lua_State *L);
static int _mapcell_lua_remove_layer(lua_State *L);
static int _mapcell_lua_get_layer(lua_State *L);
static int _mapcell_lua_set_layer(lua_State *L);
static int _mapcell_lua_clear_layers(lua_State *L);
static int _mapcell_lua_has_flag(lua_State *L);
static int _mapcell_lua_set_flag(lua_State *L);
static int _mapcell_lua_clear_flag(lua_State *L);
static int _mapcell_lua_get_layer_count(lua_State *L);

/**
 * @brief Registers an EseMapCell object in the Lua registry and optionally pushes it to stack.
 * 
 * @param cell Pointer to the EseMapCell to register
 * @param push_to_stack If true, pushes the proxy table to the Lua stack
 */
static void _mapcell_lua_register(EseMapCell *cell, bool push_to_stack) {
    lua_State *L = cell->state;
    
    // Create proxy table
    lua_newtable(L);
    
    // Set metatable
    luaL_getmetatable(L, "MapCellProxyMeta");
    lua_setmetatable(L, -2);
    
    // Store pointer to C object
    lua_pushlightuserdata(L, cell);
    lua_setfield(L, -2, "__ptr");
    
    // Create Lua methods with upvalues
    lua_pushlightuserdata(L, cell);
    lua_pushcclosure(L, _mapcell_lua_add_layer, 1);
    lua_setfield(L, -2, "add_layer");
    
    lua_pushlightuserdata(L, cell);
    lua_pushcclosure(L, _mapcell_lua_remove_layer, 1);
    lua_setfield(L, -2, "remove_layer");
    
    lua_pushlightuserdata(L, cell);
    lua_pushcclosure(L, _mapcell_lua_get_layer, 1);
    lua_setfield(L, -2, "get_layer");
    
    lua_pushlightuserdata(L, cell);
    lua_pushcclosure(L, _mapcell_lua_set_layer, 1);
    lua_setfield(L, -2, "set_layer");
    
    lua_pushlightuserdata(L, cell);
    lua_pushcclosure(L, _mapcell_lua_clear_layers, 1);
    lua_setfield(L, -2, "clear_layers");
    
    lua_pushlightuserdata(L, cell);
    lua_pushcclosure(L, _mapcell_lua_has_flag, 1);
    lua_setfield(L, -2, "has_flag");
    
    lua_pushlightuserdata(L, cell);
    lua_pushcclosure(L, _mapcell_lua_set_flag, 1);
    lua_setfield(L, -2, "set_flag");
    
    lua_pushlightuserdata(L, cell);
    lua_pushcclosure(L, _mapcell_lua_clear_flag, 1);
    lua_setfield(L, -2, "clear_flag");
    
    lua_pushlightuserdata(L, cell);
    lua_pushcclosure(L, _mapcell_lua_get_layer_count, 1);
    lua_setfield(L, -2, "get_layer_count");
    
    // Store reference in registry
    cell->lua_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    
    if (push_to_stack) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, cell->lua_ref);
    }
}

/**
 * @brief Pushes an EseMapCell object to the Lua stack.
 * 
 * @param L Lua state pointer
 * @param cell Pointer to the EseMapCell object
 * @return Number of values pushed to stack (always 1)
 */
static int _mapcell_lua_push(lua_State *L, EseMapCell *cell) {
    if (!cell) {
        lua_pushnil(L);
        return 1;
    }
    
    if (cell->lua_ref != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, cell->lua_ref);
    } else {
        lua_pushnil(L);
    }
    
    return 1;
}

/**
 * @brief Lua constructor function for creating new MapCell objects.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new mapcell object)
 */
static int _mapcell_lua_new(lua_State *L) {
    EseMapCell *cell = (EseMapCell *)memory_manager.malloc(sizeof(EseMapCell), MMTAG_GENERAL);
    cell->tile_ids = (uint8_t *)memory_manager.malloc(sizeof(uint8_t) * INITIAL_LAYER_CAPACITY, MMTAG_GENERAL);
    cell->layer_count = 0;
    cell->layer_capacity = INITIAL_LAYER_CAPACITY;
    cell->isDynamic = false;
    cell->flags = 0;
    cell->data = NULL;
    cell->state = L;
    cell->lua_ref = LUA_NOREF;
    _mapcell_lua_register(cell, true);
    
    return 1;
}

/**
 * @brief Lua __index metamethod for EseMapCell objects (getter).
 * 
 * @param L Lua state pointer
 * @return Number of return values (0 or 1)
 */
static int _mapcell_lua_index(lua_State *L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "Expected MapCell object");
    }
    
    lua_getfield(L, 1, "__ptr");
    EseMapCell *cell = (EseMapCell *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    if (!cell) {
        return luaL_error(L, "Invalid MapCell object");
    }
    
    const char *key = lua_tostring(L, 2);
    if (!key) {
        return 0;
    }
    
    if (strcmp(key, "isDynamic") == 0) {
        lua_pushboolean(L, cell->isDynamic);
        return 1;
    } else if (strcmp(key, "flags") == 0) {
        lua_pushnumber(L, cell->flags);
        return 1;
    } else if (strcmp(key, "layer_count") == 0) {
        lua_pushnumber(L, cell->layer_count);
        return 1;
    }
    
    // Check if method exists in the table
    lua_rawget(L, 1);
    return 1;
}

/**
 * @brief Lua __newindex metamethod for EseMapCell objects (setter).
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 0)
 */
static int _mapcell_lua_newindex(lua_State *L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "Expected MapCell object");
    }
    
    lua_getfield(L, 1, "__ptr");
    EseMapCell *cell = (EseMapCell *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    if (!cell) {
        return luaL_error(L, "Invalid MapCell object");
    }
    
    const char *key = lua_tostring(L, 2);
    if (!key) {
        return 0;
    }
    
    if (strcmp(key, "isDynamic") == 0) {
        cell->isDynamic = lua_toboolean(L, 3);
    } else if (strcmp(key, "flags") == 0) {
        if (lua_isnumber(L, 3)) {
            cell->flags = (uint32_t)lua_tonumber(L, 3);
        }
    } else {
        // For other fields, store in the table
        lua_rawset(L, 1);
    }
    
    return 0;
}

/**
 * @brief Lua method to add a tile layer.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - boolean success)
 */
static int _mapcell_lua_add_layer(lua_State *L) {
    EseMapCell *cell = (EseMapCell *)lua_touserdata(L, lua_upvalueindex(1));
    if (!cell) {
        return luaL_error(L, "Invalid EseMapCell object in add_layer method");
    }
    
    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "add_layer(tile_id) requires a number");
    }
    
    uint8_t tile_id = (uint8_t)lua_tonumber(L, 1);
    lua_pushboolean(L, mapcell_add_layer(cell, tile_id));
    return 1;
}

/**
 * @brief Lua method to remove a tile layer by index.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - boolean success)
 */
static int _mapcell_lua_remove_layer(lua_State *L) {
    EseMapCell *cell = (EseMapCell *)lua_touserdata(L, lua_upvalueindex(1));
    if (!cell) {
        return luaL_error(L, "Invalid EseMapCell object in remove_layer method");
    }
    
    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "remove_layer(index) requires a number");
    }
    
    size_t layer_index = (size_t)lua_tonumber(L, 1);
    lua_pushboolean(L, mapcell_remove_layer(cell, layer_index));
    return 1;
}

/**
 * @brief Lua method to get a tile ID from a layer.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the tile ID)
 */
static int _mapcell_lua_get_layer(lua_State *L) {
    EseMapCell *cell = (EseMapCell *)lua_touserdata(L, lua_upvalueindex(1));
    if (!cell) {
        return luaL_error(L, "Invalid EseMapCell object in get_layer method");
    }
    
    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "get_layer(index) requires a number");
    }
    
    size_t layer_index = (size_t)lua_tonumber(L, 1);
    lua_pushnumber(L, mapcell_get_layer(cell, layer_index));
    return 1;
}

/**
 * @brief Lua method to set a tile ID for a layer.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - boolean success)
 */
static int _mapcell_lua_set_layer(lua_State *L) {
    EseMapCell *cell = (EseMapCell *)lua_touserdata(L, lua_upvalueindex(1));
    if (!cell) {
        return luaL_error(L, "Invalid EseMapCell object in set_layer method");
    }
    
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        return luaL_error(L, "set_layer(index, tile_id) requires two numbers");
    }
    
    size_t layer_index = (size_t)lua_tonumber(L, 1);
    uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
    lua_pushboolean(L, mapcell_set_layer(cell, layer_index, tile_id));
    return 1;
}

/**
 * @brief Lua method to clear all layers.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 0)
 */
static int _mapcell_lua_clear_layers(lua_State *L) {
    EseMapCell *cell = (EseMapCell *)lua_touserdata(L, lua_upvalueindex(1));
    if (!cell) {
        return luaL_error(L, "Invalid EseMapCell object in clear_layers method");
    }
    
    mapcell_clear_layers(cell);
    return 0;
}

/**
 * @brief Lua method to check if a flag is set.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - boolean result)
 */
static int _mapcell_lua_has_flag(lua_State *L) {
    EseMapCell *cell = (EseMapCell *)lua_touserdata(L, lua_upvalueindex(1));
    if (!cell) {
        return luaL_error(L, "Invalid EseMapCell object in has_flag method");
    }
    
    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "has_flag(flag) requires a number");
    }
    
    uint32_t flag = (uint32_t)lua_tonumber(L, 1);
    lua_pushboolean(L, mapcell_has_flag(cell, flag));
    return 1;
}

/**
 * @brief Lua method to set a flag.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 0)
 */
static int _mapcell_lua_set_flag(lua_State *L) {
    EseMapCell *cell = (EseMapCell *)lua_touserdata(L, lua_upvalueindex(1));
    if (!cell) {
        return luaL_error(L, "Invalid EseMapCell object in set_flag method");
    }
    
    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "set_flag(flag) requires a number");
    }
    
    uint32_t flag = (uint32_t)lua_tonumber(L, 1);
    mapcell_set_flag(cell, flag);
    return 0;
}

/**
 * @brief Lua method to clear a flag.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 0)
 */
static int _mapcell_lua_clear_flag(lua_State *L) {
    EseMapCell *cell = (EseMapCell *)lua_touserdata(L, lua_upvalueindex(1));
    if (!cell) {
        return luaL_error(L, "Invalid EseMapCell object in clear_flag method");
    }
    
    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "clear_flag(flag) requires a number");
    }
    
    uint32_t flag = (uint32_t)lua_tonumber(L, 1);
    mapcell_clear_flag(cell, flag);
    return 0;
}

/**
 * @brief Lua method to get layer count.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the layer count)
 */
static int _mapcell_lua_get_layer_count(lua_State *L) {
    EseMapCell *cell = (EseMapCell *)lua_touserdata(L, lua_upvalueindex(1));
    if (!cell) {
        return luaL_error(L, "Invalid EseMapCell object in get_layer_count method");
    }
    
    lua_pushnumber(L, cell->layer_count);
    return 1;
}

// Public API implementations

void mapcell_lua_init(EseLuaEngine *engine) {
    log_debug("LUA", "Creating EseMapCell metatable");
    
    // Create metatable
    luaL_newmetatable(engine->runtime, "MapCellProxyMeta");
    lua_pushcfunction(engine->runtime, _mapcell_lua_index);
    lua_setfield(engine->runtime, -2, "__index");
    lua_pushcfunction(engine->runtime, _mapcell_lua_newindex);
    lua_setfield(engine->runtime, -2, "__newindex");
    lua_pop(engine->runtime, 1);
    
    // Create global MapCell table
    lua_getglobal(engine->runtime, "MapCell");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);
        log_debug("LUA", "Creating global EseMapCell table");
        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _mapcell_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_setglobal(engine->runtime, "MapCell");
    } else {
        lua_pop(engine->runtime, 1);
    }
}

EseMapCell *mapcell_create(EseLuaEngine *engine, bool c_only) {
    EseMapCell *cell = (EseMapCell *)memory_manager.malloc(sizeof(EseMapCell), MMTAG_GENERAL);
    cell->tile_ids = (uint8_t *)memory_manager.malloc(sizeof(uint8_t) * INITIAL_LAYER_CAPACITY, MMTAG_GENERAL);
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

EseMapCell *mapcell_copy(const EseMapCell *source, bool c_only) {
    if (source == NULL) {
        return NULL;
    }
    
    EseMapCell *copy = (EseMapCell *)memory_manager.malloc(sizeof(EseMapCell), MMTAG_GENERAL);
    copy->tile_ids = (uint8_t *)memory_manager.malloc(sizeof(uint8_t) * source->layer_capacity, MMTAG_GENERAL);
    memcpy(copy->tile_ids, source->tile_ids, sizeof(uint8_t) * source->layer_count);
    copy->layer_count = source->layer_count;
    copy->layer_capacity = source->layer_capacity;
    copy->isDynamic = source->isDynamic;
    copy->flags = source->flags;
    copy->data = source->data; // Shallow copy - caller responsible for deep copy if needed
    copy->state = source->state;
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
        // Note: We don't free cell->data as it's managed by the caller
        memory_manager.free(cell);
    }
}

EseMapCell *mapcell_lua_get(lua_State *L, int idx) {
    if (!lua_istable(L, idx)) {
        return NULL;
    }
    
    if (!lua_getmetatable(L, idx)) {
        return NULL;
    }
    
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
    
    void *cell = lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    return (EseMapCell *)cell;
}

bool mapcell_add_layer(EseMapCell *cell, uint8_t tile_id) {
    if (!cell) return false;
    
    // Resize array if needed
    if (cell->layer_count >= cell->layer_capacity) {
        size_t new_capacity = cell->layer_capacity * 2;
        uint8_t *new_array = (uint8_t *)memory_manager.realloc(
            cell->tile_ids, 
            sizeof(uint8_t) * new_capacity, 
            MMTAG_GENERAL
        );
        if (!new_array) {
            return false;
        }
        cell->tile_ids = new_array;
        cell->layer_capacity = new_capacity;
    }
    
    cell->tile_ids[cell->layer_count++] = tile_id;
    return true;
}

bool mapcell_remove_layer(EseMapCell *cell, size_t layer_index) {
    if (!cell || layer_index >= cell->layer_count) {
        return false;
    }
    
    // Shift remaining elements down
    for (size_t i = layer_index; i < cell->layer_count - 1; i++) {
        cell->tile_ids[i] = cell->tile_ids[i + 1];
    }
    
    cell->layer_count--;
    return true;
}

uint8_t mapcell_get_layer(const EseMapCell *cell, size_t layer_index) {
    if (!cell || layer_index >= cell->layer_count) {
        return 0;
    }
    return cell->tile_ids[layer_index];
}

bool mapcell_set_layer(EseMapCell *cell, size_t layer_index, uint8_t tile_id) {
    if (!cell || layer_index >= cell->layer_count) {
        return false;
    }
    cell->tile_ids[layer_index] = tile_id;
    return true;
}

void mapcell_clear_layers(EseMapCell *cell) {
    if (cell) {
        cell->layer_count = 0;
    }
}

bool mapcell_has_flag(const EseMapCell *cell, uint32_t flag) {
    if (!cell) return false;
    return (cell->flags & flag) != 0;
}

void mapcell_set_flag(EseMapCell *cell, uint32_t flag) {
    if (cell) {
        cell->flags |= flag;
    }
}

void mapcell_clear_flag(EseMapCell *cell, uint32_t flag) {
    if (cell) {
        cell->flags &= ~flag;
    }
}
