#include <string.h>
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "core/memory_manager.h"
#include "types/types.h"

// Forward declarations for static functions
static void _map_lua_register(EseMap *map, bool push_to_stack);
static int _map_lua_push(lua_State *L, EseMap *map);
static int _map_lua_new(lua_State *L);
static int _map_lua_index(lua_State *L);
static int _map_lua_newindex(lua_State *L);
static int _map_lua_get_cell(lua_State *L);
static int _map_lua_set_cell(lua_State *L);
static int _map_lua_resize(lua_State *L);
static int _map_lua_set_tileset(lua_State *L);
static bool _allocate_cells_array(EseMap *map);
static void _free_cells_array(EseMap *map);
static char *_strdup_safe(const char *src);

/**
 * @brief Safe string duplication with memory manager.
 */
static char *_strdup_safe(const char *src) {
    if (!src) return NULL;
    
    size_t len = strlen(src) + 1;
    char *dst = (char *)memory_manager.malloc(len, MMTAG_GENERAL);
    if (dst) {
        memcpy(dst, src, len);
    }
    return dst;
}

/**
 * @brief Allocates the 2D cells array for the map.
 */
static bool _allocate_cells_array(EseMap *map) {
    if (!map || map->width == 0 || map->height == 0) {
        return false;
    }
    
    // Allocate row pointers
    map->cells = (EseMapCell **)memory_manager.malloc(
        sizeof(EseMapCell *) * map->height, 
        MMTAG_GENERAL
    );
    if (!map->cells) {
        return false;
    }
    
    // Allocate and initialize cells
    for (uint32_t y = 0; y < map->height; y++) {
        map->cells[y] = (EseMapCell *)memory_manager.malloc(
            sizeof(EseMapCell) * map->width, 
            MMTAG_GENERAL
        );
        if (!map->cells[y]) {
            // Clean up partial allocation
            for (uint32_t cleanup_y = 0; cleanup_y < y; cleanup_y++) {
                memory_manager.free(map->cells[cleanup_y]);
            }
            memory_manager.free(map->cells);
            map->cells = NULL;
            return false;
        }
        
        // Initialize each cell
        for (uint32_t x = 0; x < map->width; x++) {
            EseMapCell *cell = &map->cells[y][x];
            cell->tile_ids = (uint8_t *)memory_manager.malloc(
                sizeof(uint8_t) * 4, 
                MMTAG_GENERAL
            );
            cell->layer_count = 0;
            cell->layer_capacity = 4;
            cell->isDynamic = false;
            cell->flags = 0;
            cell->data = NULL;
            cell->state = map->state;
            cell->lua_ref = LUA_NOREF;
        }
    }
    
    return true;
}

/**
 * @brief Frees the 2D cells array.
 */
static void _free_cells_array(EseMap *map) {
    if (!map || !map->cells) {
        return;
    }
    
    for (uint32_t y = 0; y < map->height; y++) {
        if (map->cells[y]) {
            // Free each cell's tile_ids array
            for (uint32_t x = 0; x < map->width; x++) {
                if (map->cells[y][x].tile_ids) {
                    memory_manager.free(map->cells[y][x].tile_ids);
                }
                if (map->cells[y][x].lua_ref != LUA_NOREF) {
                    luaL_unref(map->state, LUA_REGISTRYINDEX, map->cells[y][x].lua_ref);
                }
            }
            memory_manager.free(map->cells[y]);
        }
    }
    
    memory_manager.free(map->cells);
    map->cells = NULL;
}

/**
 * @brief Registers an EseMap object in the Lua registry and optionally pushes it to stack.
 */
static void _map_lua_register(EseMap *map, bool push_to_stack) {
    lua_State *L = map->state;
    
    // Create proxy table
    lua_newtable(L);
    
    // Set metatable
    luaL_getmetatable(L, "MapProxyMeta");
    lua_setmetatable(L, -2);
    
    // Store pointer to C object
    lua_pushlightuserdata(L, map);
    lua_setfield(L, -2, "__ptr");
    
    // Create Lua methods with upvalues
    lua_pushlightuserdata(L, map);
    lua_pushcclosure(L, _map_lua_get_cell, 1);
    lua_setfield(L, -2, "get_cell");
    
    lua_pushlightuserdata(L, map);
    lua_pushcclosure(L, _map_lua_set_cell, 1);
    lua_setfield(L, -2, "set_cell");
    
    lua_pushlightuserdata(L, map);
    lua_pushcclosure(L, _map_lua_resize, 1);
    lua_setfield(L, -2, "resize");
    
    lua_pushlightuserdata(L, map);
    lua_pushcclosure(L, _map_lua_set_tileset, 1);
    lua_setfield(L, -2, "set_tileset");
    
    // Store reference in registry
    map->lua_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    
    if (push_to_stack) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, map->lua_ref);
    }
}

static int _map_lua_push(lua_State *L, EseMap *map) {
    if (!map) {
        lua_pushnil(L);
        return 1;
    }
    
    if (map->lua_ref != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, map->lua_ref);
    } else {
        lua_pushnil(L);
    }
    
    return 1;
}

static int _map_lua_new(lua_State *L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        return luaL_error(L, "Map.new(width, height, [type]) requires at least two numbers");
    }
    
    uint32_t width = (uint32_t)lua_tonumber(L, 1);
    uint32_t height = (uint32_t)lua_tonumber(L, 2);
    EseMapType type = MAP_TYPE_GRID;
    
    if (lua_isstring(L, 3)) {
        type = map_type_from_string(lua_tostring(L, 3));
    }
    
    if (width == 0 || height == 0) {
        return luaL_error(L, "Map dimensions must be greater than 0");
    }
    
    EseMap *map = (EseMap *)memory_manager.malloc(sizeof(EseMap), MMTAG_GENERAL);
    map->title = _strdup_safe("Untitled Map");
    map->author = _strdup_safe("Unknown");
    map->version = _strdup_safe("1.0");
    map->type = type;
    map->tileset = NULL;
    map->width = width;
    map->height = height;
    map->cells = NULL;
    map->state = L;
    map->lua_ref = LUA_NOREF;
    
    if (!_allocate_cells_array(map)) {
        if (map->title) memory_manager.free(map->title);
        if (map->author) memory_manager.free(map->author);
        if (map->version) memory_manager.free(map->version);
        memory_manager.free(map);
        return luaL_error(L, "Failed to allocate cell array");
    }
    
    _map_lua_register(map, true);
    return 1;
}

static int _map_lua_index(lua_State *L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "Expected Map object");
    }
    
    lua_getfield(L, 1, "__ptr");
    EseMap *map = (EseMap *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    if (!map) {
        return luaL_error(L, "Invalid Map object");
    }
    
    const char *key = lua_tostring(L, 2);
    if (!key) {
        return 0;
    }
    
    if (strcmp(key, "title") == 0) {
        lua_pushstring(L, map->title ? map->title : "");
        return 1;
    } else if (strcmp(key, "author") == 0) {
        lua_pushstring(L, map->author ? map->author : "");
        return 1;
    } else if (strcmp(key, "version") == 0) {
        lua_pushstring(L, map->version ? map->version : "");
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
            lua_rawgeti(L, LUA_REGISTRYINDEX, map->tileset->lua_ref);
        } else {
            lua_pushnil(L);
        }
        return 1;
    }
    
    // Check if method exists in the table
    lua_rawget(L, 1);
    return 1;
}

static int _map_lua_newindex(lua_State *L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "Expected Map object");
    }
    
    lua_getfield(L, 1, "__ptr");
    EseMap *map = (EseMap *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    if (!map) {
        return luaL_error(L, "Invalid Map object");
    }
    
    const char *key = lua_tostring(L, 2);
    if (!key) {
        return 0;
    }
    
    if (strcmp(key, "title") == 0) {
        const char *title = lua_tostring(L, 3);
        map_set_title(map, title);
    } else if (strcmp(key, "author") == 0) {
        const char *author = lua_tostring(L, 3);
        map_set_author(map, author);
    } else if (strcmp(key, "version") == 0) {
        const char *version = lua_tostring(L, 3);
        map_set_version(map, version);
    } else if (strcmp(key, "type") == 0) {
        const char *type_str = lua_tostring(L, 3);
        if (type_str) {
            map->type = map_type_from_string(type_str);
        }
    } else {
        // For other fields, store in the table
        lua_rawset(L, 1);
    }
    
    return 0;
}

static int _map_lua_get_cell(lua_State *L) {
    EseMap *map = (EseMap *)lua_touserdata(L, lua_upvalueindex(1));
    if (!map) {
        return luaL_error(L, "Invalid EseMap object in get_cell method");
    }
    
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        return luaL_error(L, "get_cell(x, y) requires two numbers");
    }
    
    uint32_t x = (uint32_t)lua_tonumber(L, 1);
    uint32_t y = (uint32_t)lua_tonumber(L, 2);
    
    EseMapCell *cell = map_get_cell(map, x, y);
    lua_rawgeti(L, LUA_REGISTRYINDEX, cell->lua_ref);
    
    return 1;
}

static int _map_lua_set_cell(lua_State *L) {
    EseMap *map = (EseMap *)lua_touserdata(L, lua_upvalueindex(1));
    if (!map) {
        return luaL_error(L, "Invalid EseMap object in set_cell method");
    }
    
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        return luaL_error(L, "set_cell(x, y, cell) requires coordinates as numbers");
    }
    
    uint32_t x = (uint32_t)lua_tonumber(L, 1);
    uint32_t y = (uint32_t)lua_tonumber(L, 2);
    
    EseMapCell *cell = mapcell_lua_get(L, 3);
    if (!cell) {
        return luaL_error(L, "set_cell(x, y, cell) requires a valid MapCell object");
    }
    
    lua_pushboolean(L, map_set_cell(map, x, y, cell));
    return 1;
}

static int _map_lua_resize(lua_State *L) {
    EseMap *map = (EseMap *)lua_touserdata(L, lua_upvalueindex(1));
    if (!map) {
        return luaL_error(L, "Invalid EseMap object in resize method");
    }
    
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        return luaL_error(L, "resize(width, height) requires two numbers");
    }
    
    uint32_t new_width = (uint32_t)lua_tonumber(L, 1);
    uint32_t new_height = (uint32_t)lua_tonumber(L, 2);
    
    lua_pushboolean(L, map_resize(map, new_width, new_height));
    return 1;
}

static int _map_lua_set_tileset(lua_State *L) {
    EseMap *map = (EseMap *)lua_touserdata(L, lua_upvalueindex(1));
    if (!map) {
        return luaL_error(L, "Invalid EseMap object in set_tileset method");
    }
    
    EseTileSet *tileset = tileset_lua_get(L, 1);
    map_set_tileset(map, tileset);
    return 0;
}

// Public API implementations

void map_lua_init(EseLuaEngine *engine) {
    log_debug("LUA", "Creating EseMap metatable");
    
    // Create metatable
    luaL_newmetatable(engine->runtime, "MapProxyMeta");
    lua_pushcfunction(engine->runtime, _map_lua_index);
    lua_setfield(engine->runtime, -2, "__index");
    lua_pushcfunction(engine->runtime, _map_lua_newindex);
    lua_setfield(engine->runtime, -2, "__newindex");
    lua_pop(engine->runtime, 1);
    
    // Create global Map table
    lua_getglobal(engine->runtime, "Map");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);
        log_debug("LUA", "Creating global EseMap table");
        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _map_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_setglobal(engine->runtime, "Map");
    } else {
        lua_pop(engine->runtime, 1);
    }
}

EseMap *map_create(EseLuaEngine *engine, uint32_t width, uint32_t height, EseMapType type, bool c_only) {
    if (width == 0 || height == 0) {
        return NULL;
    }
    
    EseMap *map = (EseMap *)memory_manager.malloc(sizeof(EseMap), MMTAG_GENERAL);
    map->title = _strdup_safe("Untitled Map");
    map->author = _strdup_safe("Unknown");
    map->version = _strdup_safe("1.0");
    map->type = type;
    map->tileset = NULL;
    map->width = width;
    map->height = height;
    map->cells = NULL;
    map->state = engine->runtime;
    map->lua_ref = LUA_NOREF;
    
    if (!_allocate_cells_array(map)) {
        if (map->title) memory_manager.free(map->title);
        if (map->author) memory_manager.free(map->author);
        if (map->version) memory_manager.free(map->version);
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
        if (map->version) memory_manager.free(map->version);
        
        memory_manager.free(map);
    }
}

EseMap *map_lua_get(lua_State *L, int idx) {
    if (!lua_istable(L, idx)) {
        return NULL;
    }
    
    if (!lua_getmetatable(L, idx)) {
        return NULL;
    }
    
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
    
    void *map = lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    return (EseMap *)map;
}

EseMapCell *map_get_cell(const EseMap *map, uint32_t x, uint32_t y) {
    if (!map || !map->cells || x >= map->width || y >= map->height) {
        return NULL;
    }
    
    return &map->cells[y][x];
}

bool map_set_cell(EseMap *map, uint32_t x, uint32_t y, EseMapCell *cell) {
    if (!map || !map->cells || !cell || x >= map->width || y >= map->height) {
        return false;
    }
    
    // Copy the cell data
    EseMapCell *target = &map->cells[y][x];
    
    // Free existing tile_ids if any
    if (target->tile_ids) {
        memory_manager.free(target->tile_ids);
    }
    
    // Copy tile_ids array
    target->tile_ids = (uint8_t *)memory_manager.malloc(
        sizeof(uint8_t) * cell->layer_capacity, 
        MMTAG_GENERAL
    );
    if (!target->tile_ids) {
        return false;
    }
    
    memcpy(target->tile_ids, cell->tile_ids, sizeof(uint8_t) * cell->layer_count);
    target->layer_count = cell->layer_count;
    target->layer_capacity = cell->layer_capacity;
    target->isDynamic = cell->isDynamic;
    target->flags = cell->flags;
    target->data = cell->data;
    
    return true;
}

bool map_set_title(EseMap *map, const char *title) {
    if (!map) return false;
    
    if (map->title) {
        memory_manager.free(map->title);
    }
    
    map->title = _strdup_safe(title);
    return map->title != NULL;
}

bool map_set_author(EseMap *map, const char *author) {
    if (!map) return false;
    
    if (map->author) {
        memory_manager.free(map->author);
    }
    
    map->author = _strdup_safe(author);
    return map->author != NULL;
}

bool map_set_version(EseMap *map, const char *version) {
    if (!map) return false;
    
    if (map->version) {
        memory_manager.free(map->version);
    }
    
    map->version = _strdup_safe(version);
    return map->version != NULL;
}

void map_set_tileset(EseMap *map, EseTileSet *tileset) {
    if (map) {
        map->tileset = tileset;
    }
}

bool map_resize(EseMap *map, uint32_t new_width, uint32_t new_height) {
    if (!map || new_width == 0 || new_height == 0) {
        return false;
    }
    
    if (new_width == map->width && new_height == map->height) {
        return true; // No change needed
    }
    
    // Store old dimensions
    uint32_t old_width = map->width;
    uint32_t old_height = map->height;
    EseMapCell **old_cells = map->cells;
    
    // Update dimensions
    map->width = new_width;
    map->height = new_height;
    map->cells = NULL;
    
    // Allocate new cells array
    if (!_allocate_cells_array(map)) {
        // Restore old state on failure
        map->width = old_width;
        map->height = old_height;
        map->cells = old_cells;
        return false;
    }
    
    // Copy existing cells that fit in new dimensions
    if (old_cells) {
        uint32_t copy_width = (old_width < new_width) ? old_width : new_width;
        uint32_t copy_height = (old_height < new_height) ? old_height : new_height;
        
        for (uint32_t y = 0; y < copy_height; y++) {
            for (uint32_t x = 0; x < copy_width; x++) {
                EseMapCell *src = &old_cells[y][x];
                EseMapCell *dst = &map->cells[y][x];
                
                // Free the default allocated tile_ids
                memory_manager.free(dst->tile_ids);
                
                // Copy from source
                dst->tile_ids = src->tile_ids; // Transfer ownership
                dst->layer_count = src->layer_count;
                dst->layer_capacity = src->layer_capacity;
                dst->isDynamic = src->isDynamic;
                dst->flags = src->flags;
                dst->data = src->data;
                dst->lua_ref = src->lua_ref; // Transfer Lua reference
                
                // Clear source to prevent double-free
                src->tile_ids = NULL;
                src->lua_ref = LUA_NOREF;
            }
        }
        
        // Free old array structure (cells were transferred or freed)
        for (uint32_t y = 0; y < old_height; y++) {
            if (old_cells[y]) {
                // Free any cells that weren't transferred
                for (uint32_t x = copy_width; x < old_width; x++) {
                    if (old_cells[y][x].tile_ids) {
                        memory_manager.free(old_cells[y][x].tile_ids);
                    }
                    if (old_cells[y][x].lua_ref != LUA_NOREF) {
                        luaL_unref(map->state, LUA_REGISTRYINDEX, old_cells[y][x].lua_ref);
                    }
                }
                memory_manager.free(old_cells[y]);
            }
        }
        memory_manager.free(old_cells);
    }
    
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
    
    return MAP_TYPE_GRID; // Default fallback
}
