#include <string.h>
#include <time.h>
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "core/memory_manager.h"
#include "graphics/sprite.h"
#include "types/tileset.h"

#define INITIAL_SPRITE_CAPACITY 4

/* ----------------- RNG ----------------- */

static uint32_t _get_random_weight(EseTileSet *tiles, uint32_t max_weight) {
    if (max_weight == 0) return 0;
    if (tiles->rng_seed == 0) {
        tiles->rng_seed = (uint32_t)time(NULL);
    }
    tiles->rng_seed = (tiles->rng_seed * 1664525 + 1013904223); // LCG
    return tiles->rng_seed % max_weight;
}

/* ----------------- Internal Helpers ----------------- */

static void _tileset_lua_register(EseTileSet *tiles, bool is_lua_owned) {
    log_assert("TILESET", tiles, "_tileset_lua_register called with NULL tiles");
    log_assert("TILESET", tiles->lua_ref == LUA_NOREF,
               "_tileset_lua_register tiles already registered");

    lua_State *L = tiles->state;

    lua_newtable(L);

    // Store pointer
    lua_pushlightuserdata(L, tiles);
    lua_setfield(L, -2, "__ptr");

    // Store ownership flag
    lua_pushboolean(L, is_lua_owned);
    lua_setfield(L, -2, "__is_lua_owned");

    // Set metatable
    luaL_getmetatable(L, "TilesProxyMeta");
    lua_setmetatable(L, -2);

    // Store registry reference
    tiles->lua_ref = luaL_ref(L, LUA_REGISTRYINDEX);
}

void tileset_lua_push(EseTileSet *tiles) {
    log_assert("TILESET", tiles, "tileset_lua_push called with NULL tiles");
    log_assert("TILESET", tiles->lua_ref != LUA_NOREF,
               "tileset_lua_push tiles not registered with lua");

    lua_rawgeti(tiles->state, LUA_REGISTRYINDEX, tiles->lua_ref);
}

/* ----------------- Lua Methods ----------------- */

static int _tileset_lua_add_sprite(lua_State *L) {
    EseTileSet *tiles = tileset_lua_get(L, 1);
    if (!tiles) return luaL_error(L, "Invalid Tiles in add_sprite");

    if (!lua_isnumber(L, 2) || !lua_isstring(L, 3))
        return luaL_error(L, "add_sprite(tile_id, sprite_id, [weight]) requires number, string, [number]");

    uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
    const char *sprite_str = lua_tostring(L, 3);
    uint16_t weight = lua_isnumber(L, 4) ? (uint16_t)lua_tonumber(L, 4) : 1;

    if (!sprite_str || strlen(sprite_str) == 0)
        return luaL_error(L, "sprite_id cannot be empty");
    if (weight == 0)
        return luaL_error(L, "weight must be > 0");

    lua_pushboolean(L, tileset_add_sprite(tiles, tile_id, sprite_str, weight));
    return 1;
}

static int _tileset_lua_remove_sprite(lua_State *L) {
    EseTileSet *tiles = tileset_lua_get(L, 1);
    if (!tiles) return luaL_error(L, "Invalid Tiles in remove_sprite");

    if (!lua_isnumber(L, 2) || !lua_isstring(L, 3))
        return luaL_error(L, "remove_sprite(tile_id, sprite_id) requires number, string");

    uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
    const char *sprite_str = lua_tostring(L, 3);

    if (!sprite_str || strlen(sprite_str) == 0)
        return luaL_error(L, "sprite_id cannot be empty");

    lua_pushboolean(L, tileset_remove_sprite(tiles, tile_id, sprite_str));
    return 1;
}

static int _tileset_lua_get_sprite(lua_State *L) {
    EseTileSet *tiles = tileset_lua_get(L, 1);
    if (!tiles) return luaL_error(L, "Invalid Tiles in get_sprite");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "get_sprite(tile_id) requires a number");

    uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
    const char *sprite = tileset_get_sprite(tiles, tile_id);

    if (!sprite) lua_pushnil(L);
    else lua_pushstring(L, sprite);
    return 1;
}

static int _tileset_lua_clear_mapping(lua_State *L) {
    EseTileSet *tiles = tileset_lua_get(L, 1);
    if (!tiles) return luaL_error(L, "Invalid Tiles in clear_mapping");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "clear_mapping(tile_id) requires a number");

    uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
    tileset_clear_mapping(tiles, tile_id);
    return 0;
}

static int _tileset_lua_get_sprite_count(lua_State *L) {
    EseTileSet *tiles = tileset_lua_get(L, 1);
    if (!tiles) return luaL_error(L, "Invalid Tiles in get_sprite_count");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "get_sprite_count(tile_id) requires a number");

    uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
    lua_pushnumber(L, tileset_get_sprite_count(tiles, tile_id));
    return 1;
}

static int _tileset_lua_update_sprite_weight(lua_State *L) {
    EseTileSet *tiles = tileset_lua_get(L, 1);
    if (!tiles) return luaL_error(L, "Invalid Tiles in update_sprite_weight");

    if (!lua_isnumber(L, 2) || !lua_isstring(L, 3) || !lua_isnumber(L, 4))
        return luaL_error(L, "update_sprite_weight(tile_id, sprite_id, weight) requires number, string, number");

    uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
    const char *sprite_str = lua_tostring(L, 3);
    uint16_t weight = (uint16_t)lua_tonumber(L, 4);

    if (!sprite_str || strlen(sprite_str) == 0)
        return luaL_error(L, "sprite_id cannot be empty");
    if (weight == 0)
        return luaL_error(L, "weight must be > 0");

    lua_pushboolean(L, tileset_update_sprite_weight(tiles, tile_id, sprite_str, weight));
    return 1;
}

/* ----------------- Metamethods ----------------- */

static int _tileset_lua_index(lua_State *L) {
    EseTileSet *tiles = tileset_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!tiles || !key) return 0;

    if (strcmp(key, "add_sprite") == 0) {
        lua_pushcfunction(L, _tileset_lua_add_sprite);
        return 1;
    } else if (strcmp(key, "remove_sprite") == 0) {
        lua_pushcfunction(L, _tileset_lua_remove_sprite);
        return 1;
    } else if (strcmp(key, "get_sprite") == 0) {
        lua_pushcfunction(L, _tileset_lua_get_sprite);
        return 1;
    } else if (strcmp(key, "clear_mapping") == 0) {
        lua_pushcfunction(L, _tileset_lua_clear_mapping);
        return 1;
    } else if (strcmp(key, "get_sprite_count") == 0) {
        lua_pushcfunction(L, _tileset_lua_get_sprite_count);
        return 1;
    } else if (strcmp(key, "update_sprite_weight") == 0) {
        lua_pushcfunction(L, _tileset_lua_update_sprite_weight);
        return 1;
    }

    return 0;
}

static int _tileset_lua_newindex(lua_State *L) {
    return luaL_error(L, "Direct assignment not supported - use methods");
}

static int _tileset_lua_gc(lua_State *L) {
    EseTileSet *tiles = tileset_lua_get(L, 1);
    if (tiles) {
        lua_getfield(L, 1, "__is_lua_owned");
        bool is_lua_owned = lua_toboolean(L, -1);
        lua_pop(L, 1);

        if (is_lua_owned) {
            tileset_destroy(tiles);
            log_debug("LUA_GC", "Tileset (Lua-owned) freed.");
        } else {
            log_debug("LUA_GC", "Tileset (C-owned) collected, not freed.");
        }
    }
    return 0;
}

static int _tileset_lua_tostring(lua_State *L) {
    EseTileSet *tiles = tileset_lua_get(L, 1);
    if (!tiles) {
        lua_pushstring(L, "Tiles: (invalid)");
        return 1;
    }

    size_t total = 0;
    for (int i = 0; i < 256; i++) {
        total += tiles->mappings[i].sprite_count;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Tiles: %p (total_sprites=%zu)", (void *)tiles, total);
    lua_pushstring(L, buf);
    return 1;
}

/* ----------------- Lua Init ----------------- */

static int _tileset_lua_new(lua_State *L) {
    EseTileSet *tiles = (EseTileSet *)memory_manager.malloc(sizeof(EseTileSet), MMTAG_GENERAL);

    for (int i = 0; i < 256; i++) {
        tiles->mappings[i].sprites = NULL;
        tiles->mappings[i].sprite_count = 0;
        tiles->mappings[i].sprite_capacity = 0;
        tiles->mappings[i].total_weight = 0;
    }

    tiles->state = L;
    tiles->lua_ref = LUA_NOREF;

    _tileset_lua_register(tiles, true);
    tileset_lua_push(tiles);
    return 1;
}

void tileset_lua_init(EseLuaEngine *engine) {
    if (luaL_newmetatable(engine->runtime, "TilesProxyMeta")) {
        log_debug("LUA", "Adding TilesProxyMeta");
        lua_pushcfunction(engine->runtime, _tileset_lua_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _tileset_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushcfunction(engine->runtime, _tileset_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");
        lua_pushcfunction(engine->runtime, _tileset_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);

    // Global Tiles table
    lua_getglobal(engine->runtime, "Tiles");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);
        log_debug("LUA", "Creating global Tiles table");
        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _tileset_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_setglobal(engine->runtime, "Tiles");
    } else {
        lua_pop(engine->runtime, 1);
    }
}

/* ----------------- C API ----------------- */

EseTileSet *tileset_create(EseLuaEngine *engine, bool c_only) {
    EseTileSet *tiles = (EseTileSet *)memory_manager.malloc(sizeof(EseTileSet), MMTAG_GENERAL);

    for (int i = 0; i < 256; i++) {
        tiles->mappings[i].sprites = NULL;
        tiles->mappings[i].sprite_count = 0;
        tiles->mappings[i].sprite_capacity = 0;
        tiles->mappings[i].total_weight = 0;
    }

    tiles->rng_seed = 0;

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

        for (int i = 0; i < 256; i++) {
            EseTileMapping *m = &tiles->mappings[i];
            if (m->sprites) {
                for (size_t j = 0; j < m->sprite_count; j++) {
                    if (m->sprites[j].sprite_id) {
                        memory_manager.free(m->sprites[j].sprite_id);
                    }
                }
                memory_manager.free(m->sprites);
            }
        }

        memory_manager.free(tiles);
    }
}

EseTileSet *tileset_lua_get(lua_State *L, int idx) {
    if (!lua_istable(L, idx)) return NULL;
    if (!lua_getmetatable(L, idx)) return NULL;
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
    void *ptr = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return (EseTileSet *)ptr;
}

/* ----------------- Sprite Management ----------------- */

bool tileset_add_sprite(EseTileSet *tiles, uint8_t tile_id, const char *sprite_id, uint16_t weight) {
    if (!tiles || !sprite_id || weight == 0) return false;

    EseTileMapping *mapping = &tiles->mappings[tile_id];

    // Update if exists
    for (size_t i = 0; i < mapping->sprite_count; i++) {
        if (strcmp(mapping->sprites[i].sprite_id, sprite_id) == 0) {
            mapping->total_weight -= mapping->sprites[i].weight;
            mapping->sprites[i].weight = weight;
            mapping->total_weight += weight;
            return true;
        }
    }

    // Resize if needed
    if (mapping->sprite_count >= mapping->sprite_capacity) {
        size_t new_capacity = mapping->sprite_capacity == 0 ? INITIAL_SPRITE_CAPACITY : mapping->sprite_capacity * 2;
        EseSpriteWeight *new_array = (EseSpriteWeight *)memory_manager.realloc(
            mapping->sprites, sizeof(EseSpriteWeight) * new_capacity, MMTAG_GENERAL);
        if (!new_array) return false;
        mapping->sprites = new_array;
        mapping->sprite_capacity = new_capacity;
    }

    // Copy string
    size_t len = strlen(sprite_id);
    char *copy = (char *)memory_manager.malloc(len + 1, MMTAG_GENERAL);
    if (!copy) return false;
    memcpy(copy, sprite_id, len + 1);

    mapping->sprites[mapping->sprite_count].sprite_id = copy;
    mapping->sprites[mapping->sprite_count].weight = weight;
    mapping->sprite_count++;
    mapping->total_weight += weight;

    return true;
}

bool tileset_remove_sprite(EseTileSet *tiles, uint8_t tile_id, const char *sprite_id) {
    if (!tiles || !sprite_id) return false;

    EseTileMapping *mapping = &tiles->mappings[tile_id];

    for (size_t i = 0; i < mapping->sprite_count; i++) {
        if (strcmp(mapping->sprites[i].sprite_id, sprite_id) == 0) {
            mapping->total_weight -= mapping->sprites[i].weight;
            memory_manager.free(mapping->sprites[i].sprite_id);

            for (size_t j = i; j < mapping->sprite_count - 1; j++) {
                mapping->sprites[j] = mapping->sprites[j + 1];
            }

            mapping->sprite_count--;
            return true;
        }
    }

    return false;
}

const char *tileset_get_sprite(EseTileSet *tiles, uint8_t tile_id) {
    if (!tiles) return NULL;

    const EseTileMapping *mapping = &tiles->mappings[tile_id];
    if (mapping->sprite_count == 0 || mapping->total_weight == 0) return NULL;

    uint32_t random_weight = _get_random_weight(tiles, mapping->total_weight);
    uint32_t accumulated = 0;

    for (size_t i = 0; i < mapping->sprite_count; i++) {
        accumulated += mapping->sprites[i].weight;
        if (random_weight < accumulated) {
            return mapping->sprites[i].sprite_id;
        }
    }

    return mapping->sprites[mapping->sprite_count - 1].sprite_id;
}

void tileset_clear_mapping(EseTileSet *tiles, uint8_t tile_id) {
    if (!tiles) return;

    EseTileMapping *mapping = &tiles->mappings[tile_id];
    for (size_t i = 0; i < mapping->sprite_count; i++) {
        if (mapping->sprites[i].sprite_id) {
            memory_manager.free(mapping->sprites[i].sprite_id);
        }
    }
    if (mapping->sprites) {
        memory_manager.free(mapping->sprites);
    }
    mapping->sprites = NULL;
    mapping->sprite_count = 0;
    mapping->sprite_capacity = 0;
    mapping->total_weight = 0;
}

size_t tileset_get_sprite_count(const EseTileSet *tiles, uint8_t tile_id) {
    if (!tiles) return 0;
    return tiles->mappings[tile_id].sprite_count;
}

bool tileset_update_sprite_weight(EseTileSet *tiles, uint8_t tile_id, const char *sprite_id, uint16_t new_weight) {
    if (!tiles || !sprite_id || new_weight == 0) return false;

    EseTileMapping *mapping = &tiles->mappings[tile_id];
    for (size_t i = 0; i < mapping->sprite_count; i++) {
        if (strcmp(mapping->sprites[i].sprite_id, sprite_id) == 0) {
            mapping->total_weight -= mapping->sprites[i].weight;
            mapping->sprites[i].weight = new_weight;
            mapping->total_weight += new_weight;
            return true;
        }
    }

    return false;
}

void tileset_set_seed(EseTileSet *tiles, uint32_t seed) {
    if (tiles) {
        tiles->rng_seed = seed;
    }
}
