#include <string.h>
#include <time.h>
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "core/memory_manager.h"
#include "types/types.h"

// Initial capacity for sprite arrays
#define INITIAL_SPRITE_CAPACITY 4

// Forward declarations for static functions
static void _tileset_lua_register(EseTileSet *tiles, bool push_to_stack);
static int _tileset_lua_push(lua_State *L, EseTileSet *tiles);
static int _tileset_lua_new(lua_State *L);
static int _tileset_lua_index(lua_State *L);
static int _tileset_lua_newindex(lua_State *L);
static int _tileset_lua_add_sprite(lua_State *L);
static int _tileset_lua_remove_sprite(lua_State *L);
static int _tileset_lua_get_sprite(lua_State *L);
static int _tileset_lua_clear_mapping(lua_State *L);
static int _tileset_lua_get_sprite_count(lua_State *L);
static int _tileset_lua_update_sprite_weight(lua_State *L);
static uint32_t _get_random_weight(uint32_t max_weight);

/**
 * @brief Simple random number generator for weighted selection.
 */
static uint32_t _get_random_weight(uint32_t max_weight) {
    if (max_weight == 0) return 0;
    static uint32_t seed = 0;
    if (seed == 0) {
        seed = (uint32_t)time(NULL);
    }
    // Simple LCG
    seed = (seed * 1664525 + 1013904223);
    return seed % max_weight;
}

/**
 * @brief Registers an EseTileSet object in the Lua registry and optionally pushes it to stack.
 */
static void _tileset_lua_register(EseTileSet *tiles, bool push_to_stack) {
    lua_State *L = tiles->state;
    
    // Create proxy table
    lua_newtable(L);
    
    // Set metatable
    luaL_getmetatable(L, "TilesProxyMeta");
    lua_setmetatable(L, -2);
    
    // Store pointer to C object
    lua_pushlightuserdata(L, tiles);
    lua_setfield(L, -2, "__ptr");
    
    // Create Lua methods with upvalues
    lua_pushlightuserdata(L, tiles);
    lua_pushcclosure(L, _tileset_lua_add_sprite, 1);
    lua_setfield(L, -2, "add_sprite");
    
    lua_pushlightuserdata(L, tiles);
    lua_pushcclosure(L, _tileset_lua_remove_sprite, 1);
    lua_setfield(L, -2, "remove_sprite");
    
    lua_pushlightuserdata(L, tiles);
    lua_pushcclosure(L, _tileset_lua_get_sprite, 1);
    lua_setfield(L, -2, "get_sprite");
    
    lua_pushlightuserdata(L, tiles);
    lua_pushcclosure(L, _tileset_lua_clear_mapping, 1);
    lua_setfield(L, -2, "clear_mapping");
    
    lua_pushlightuserdata(L, tiles);
    lua_pushcclosure(L, _tileset_lua_get_sprite_count, 1);
    lua_setfield(L, -2, "get_sprite_count");
    
    lua_pushlightuserdata(L, tiles);
    lua_pushcclosure(L, _tileset_lua_update_sprite_weight, 1);
    lua_setfield(L, -2, "update_sprite_weight");
    
    // Store reference in registry
    tiles->lua_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    
    if (push_to_stack) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, tiles->lua_ref);
    }
}

static int _tileset_lua_push(lua_State *L, EseTileSet *tiles) {
    if (!tiles) {
        lua_pushnil(L);
        return 1;
    }
    
    if (tiles->lua_ref != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, tiles->lua_ref);
    } else {
        lua_pushnil(L);
    }
    
    return 1;
}

static int _tileset_lua_new(lua_State *L) {
    EseTileSet *tiles = (EseTileSet *)memory_manager.malloc(sizeof(EseTileSet), MMTAG_GENERAL);
    
    // Initialize all mappings to empty
    for (int i = 0; i < 256; i++) {
        tiles->mappings[i].sprites = NULL;
        tiles->mappings[i].sprite_count = 0;
        tiles->mappings[i].sprite_capacity = 0;
        tiles->mappings[i].total_weight = 0;
    }
    
    tiles->state = L;
    tiles->lua_ref = LUA_NOREF;
    _tileset_lua_register(tiles, true);
    
    return 1;
}

static int _tileset_lua_index(lua_State *L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "Expected Tiles object");
    }
    
    lua_getfield(L, 1, "__ptr");
    EseTileSet *tiles = (EseTileSet *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    if (!tiles) {
        return luaL_error(L, "Invalid Tiles object");
    }
    
    // Check if it's a number (tile_id access)
    if (lua_isnumber(L, 2)) {
        uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
        char sprite = tileset_get_sprite(tiles, tile_id);
        if (sprite == 0) {
            lua_pushnil(L);
        } else {
            lua_pushlstring(L, &sprite, 1);
        }
        return 1;
    }
    
    // Check if method exists in the table
    lua_rawget(L, 1);
    return 1;
}

static int _tileset_lua_newindex(lua_State *L) {
    // For now, prevent direct assignment - use methods instead
    return luaL_error(L, "Direct assignment not supported - use add_sprite method");
}

static int _tileset_lua_add_sprite(lua_State *L) {
    EseTileSet *tiles = (EseTileSet *)lua_touserdata(L, lua_upvalueindex(1));
    if (!tiles) {
        return luaL_error(L, "Invalid EseTileSet object in add_sprite method");
    }
    
    if (!lua_isnumber(L, 1) || !lua_isstring(L, 2)) {
        return luaL_error(L, "add_sprite(tile_id, sprite_id, [weight]) requires number, string, [number]");
    }
    
    uint8_t tile_id = (uint8_t)lua_tonumber(L, 1);
    const char *sprite_str = lua_tostring(L, 2);
    uint16_t weight = lua_isnumber(L, 3) ? (uint16_t)lua_tonumber(L, 3) : 1;
    
    if (strlen(sprite_str) == 0) {
        return luaL_error(L, "sprite_id cannot be empty string");
    }
    
    char sprite_id = sprite_str[0];
    lua_pushboolean(L, tileset_add_sprite(tiles, tile_id, sprite_id, weight));
    return 1;
}

static int _tileset_lua_remove_sprite(lua_State *L) {
    EseTileSet *tiles = (EseTileSet *)lua_touserdata(L, lua_upvalueindex(1));
    if (!tiles) {
        return luaL_error(L, "Invalid EseTileSet object in remove_sprite method");
    }
    
    if (!lua_isnumber(L, 1) || !lua_isstring(L, 2)) {
        return luaL_error(L, "remove_sprite(tile_id, sprite_id) requires number, string");
    }
    
    uint8_t tile_id = (uint8_t)lua_tonumber(L, 1);
    const char *sprite_str = lua_tostring(L, 2);
    
    if (strlen(sprite_str) == 0) {
        return luaL_error(L, "sprite_id cannot be empty string");
    }
    
    char sprite_id = sprite_str[0];
    lua_pushboolean(L, tileset_remove_sprite(tiles, tile_id, sprite_id));
    return 1;
}

static int _tileset_lua_get_sprite(lua_State *L) {
    EseTileSet *tiles = (EseTileSet *)lua_touserdata(L, lua_upvalueindex(1));
    if (!tiles) {
        return luaL_error(L, "Invalid EseTileSet object in get_sprite method");
    }
    
    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "get_sprite(tile_id) requires a number");
    }
    
    uint8_t tile_id = (uint8_t)lua_tonumber(L, 1);
    char sprite = tileset_get_sprite(tiles, tile_id);
    
    if (sprite == 0) {
        lua_pushnil(L);
    } else {
        lua_pushlstring(L, &sprite, 1);
    }
    return 1;
}

static int _tileset_lua_clear_mapping(lua_State *L) {
    EseTileSet *tiles = (EseTileSet *)lua_touserdata(L, lua_upvalueindex(1));
    if (!tiles) {
        return luaL_error(L, "Invalid EseTileSet object in clear_mapping method");
    }
    
    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "clear_mapping(tile_id) requires a number");
    }
    
    uint8_t tile_id = (uint8_t)lua_tonumber(L, 1);
    tileset_clear_mapping(tiles, tile_id);
    return 0;
}

static int _tileset_lua_get_sprite_count(lua_State *L) {
    EseTileSet *tiles = (EseTileSet *)lua_touserdata(L, lua_upvalueindex(1));
    if (!tiles) {
        return luaL_error(L, "Invalid EseTileSet object in get_sprite_count method");
    }
    
    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "get_sprite_count(tile_id) requires a number");
    }
    
    uint8_t tile_id = (uint8_t)lua_tonumber(L, 1);
    lua_pushnumber(L, tileset_get_sprite_count(tiles, tile_id));
    return 1;
}

static int _tileset_lua_update_sprite_weight(lua_State *L) {
    EseTileSet *tiles = (EseTileSet *)lua_touserdata(L, lua_upvalueindex(1));
    if (!tiles) {
        return luaL_error(L, "Invalid EseTileSet object in update_sprite_weight method");
    }
    
    if (!lua_isnumber(L, 1) || !lua_isstring(L, 2) || !lua_isnumber(L, 3)) {
        return luaL_error(L, "update_sprite_weight(tile_id, sprite_id, weight) requires number, string, number");
    }
    
    uint8_t tile_id = (uint8_t)lua_tonumber(L, 1);
    const char *sprite_str = lua_tostring(L, 2);
    uint16_t weight = (uint16_t)lua_tonumber(L, 3);
    
    if (strlen(sprite_str) == 0) {
        return luaL_error(L, "sprite_id cannot be empty string");
    }
    
    char sprite_id = sprite_str[0];
    lua_pushboolean(L, tileset_update_sprite_weight(tiles, tile_id, sprite_id, weight));
    return 1;
}

// Public API implementations

void tileset_lua_init(EseLuaEngine *engine) {
    log_debug("LUA", "Creating EseTileSet metatable");
    
    // Create metatable
    luaL_newmetatable(engine->runtime, "TilesProxyMeta");
    lua_pushcfunction(engine->runtime, _tileset_lua_index);
    lua_setfield(engine->runtime, -2, "__index");
    lua_pushcfunction(engine->runtime, _tileset_lua_newindex);
    lua_setfield(engine->runtime, -2, "__newindex");
    lua_pop(engine->runtime, 1);
    
    // Create global Tiles table
    lua_getglobal(engine->runtime, "Tiles");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);
        log_debug("LUA", "Creating global EseTileSet table");
        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _tileset_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_setglobal(engine->runtime, "Tiles");
    } else {
        lua_pop(engine->runtime, 1);
    }
}

EseTileSet *tileset_create(EseLuaEngine *engine, bool c_only) {
    EseTileSet *tiles = (EseTileSet *)memory_manager.malloc(sizeof(EseTileSet), MMTAG_GENERAL);
    
    // Initialize all mappings to empty
    for (int i = 0; i < 256; i++) {
        tiles->mappings[i].sprites = NULL;
        tiles->mappings[i].sprite_count = 0;
        tiles->mappings[i].sprite_capacity = 0;
        tiles->mappings[i].total_weight = 0;
    }
    
    tiles->state = engine->runtime;
    tiles->lua_ref = LUA_NOREF;
    
    if (!c_only) {
        _tileset_lua_register(tiles, false);
    }
    
    return tiles;
}

void tileset_destroy(EseTileSet *tiles) {
    if (tiles) {
        if (tiles->lua_ref != LUA_NOREF) {
            luaL_unref(tiles->state, LUA_REGISTRYINDEX, tiles->lua_ref);
        }
        
        // Free all sprite arrays
        for (int i = 0; i < 256; i++) {
            if (tiles->mappings[i].sprites) {
                memory_manager.free(tiles->mappings[i].sprites);
            }
        }
        
        memory_manager.free(tiles);
    }
}

EseTileSet *tileset_lua_get(lua_State *L, int idx) {
    if (!lua_istable(L, idx)) {
        return NULL;
    }
    
    if (!lua_getmetatable(L, idx)) {
        return NULL;
    }
    
    luaL_getmetatable(L, "TilesProxyMeta");
    
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
    
    void *tiles = lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    return (EseTileSet *)tiles;
}

bool tileset_add_sprite(EseTileSet *tiles, uint8_t tile_id, char sprite_id, uint16_t weight) {
    if (!tiles || weight == 0) return false;
    
    EseTileMapping *mapping = &tiles->mappings[tile_id];
    
    // Check if sprite already exists and update weight
    for (size_t i = 0; i < mapping->sprite_count; i++) {
        if (mapping->sprites[i].sprite_id == sprite_id) {
            mapping->total_weight -= mapping->sprites[i].weight;
            mapping->sprites[i].weight = weight;
            mapping->total_weight += weight;
            return true;
        }
    }
    
    // Resize array if needed
    if (mapping->sprite_count >= mapping->sprite_capacity) {
        size_t new_capacity = mapping->sprite_capacity == 0 ? INITIAL_SPRITE_CAPACITY : mapping->sprite_capacity * 2;
        EseSpriteWeight *new_array = (EseSpriteWeight *)memory_manager.realloc(
            mapping->sprites, 
            sizeof(EseSpriteWeight) * new_capacity, 
            MMTAG_GENERAL
        );
        if (!new_array) {
            return false;
        }
        mapping->sprites = new_array;
        mapping->sprite_capacity = new_capacity;
    }
    
    // Add new sprite
    mapping->sprites[mapping->sprite_count].sprite_id = sprite_id;
    mapping->sprites[mapping->sprite_count].weight = weight;
    mapping->sprite_count++;
    mapping->total_weight += weight;
    
    return true;
}

bool tileset_remove_sprite(EseTileSet *tiles, uint8_t tile_id, char sprite_id) {
    if (!tiles) return false;
    
    EseTileMapping *mapping = &tiles->mappings[tile_id];
    
    for (size_t i = 0; i < mapping->sprite_count; i++) {
        if (mapping->sprites[i].sprite_id == sprite_id) {
            mapping->total_weight -= mapping->sprites[i].weight;
            
            // Shift remaining elements down
            for (size_t j = i; j < mapping->sprite_count - 1; j++) {
                mapping->sprites[j] = mapping->sprites[j + 1];
            }
            
            mapping->sprite_count--;
            return true;
        }
    }
    
    return false;
}

char tileset_get_sprite(const EseTileSet *tiles, uint8_t tile_id) {
    if (!tiles) return 0;
    
    const EseTileMapping *mapping = &tiles->mappings[tile_id];
    
    if (mapping->sprite_count == 0 || mapping->total_weight == 0) {
        return 0;
    }
    
    uint32_t random_weight = _get_random_weight(mapping->total_weight);
    uint32_t accumulated_weight = 0;
    
    for (size_t i = 0; i < mapping->sprite_count; i++) {
        accumulated_weight += mapping->sprites[i].weight;
        if (random_weight < accumulated_weight) {
            return mapping->sprites[i].sprite_id;
        }
    }
    
    // Fallback to last sprite (shouldn't happen)
    return mapping->sprites[mapping->sprite_count - 1].sprite_id;
}

void tileset_clear_mapping(EseTileSet *tiles, uint8_t tile_id) {
    if (!tiles) return;
    
    EseTileMapping *mapping = &tiles->mappings[tile_id];
    mapping->sprite_count = 0;
    mapping->total_weight = 0;
}

size_t tileset_get_sprite_count(const EseTileSet *tiles, uint8_t tile_id) {
    if (!tiles) return 0;
    return tiles->mappings[tile_id].sprite_count;
}

bool tileset_update_sprite_weight(EseTileSet *tiles, uint8_t tile_id, char sprite_id, uint16_t new_weight) {
    if (!tiles || new_weight == 0) return false;
    
    EseTileMapping *mapping = &tiles->mappings[tile_id];
    
    for (size_t i = 0; i < mapping->sprite_count; i++) {
        if (mapping->sprites[i].sprite_id == sprite_id) {
            mapping->total_weight -= mapping->sprites[i].weight;
            mapping->sprites[i].weight = new_weight;
            mapping->total_weight += new_weight;
            return true;
        }
    }
    
    return false;
}
