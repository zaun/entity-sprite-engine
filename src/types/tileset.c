#include <string.h>
#include <time.h>
#include <stdio.h>
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "core/memory_manager.h"
#include "graphics/sprite.h"
#include "types/tileset.h"

#define INITIAL_SPRITE_CAPACITY 4

/**
 * @brief Represents a weighted sprite entry for tile mapping.
 */
 typedef struct EseSpriteWeight {
    char *sprite_id;   /** The sprite string (heap-allocated) */
    uint16_t weight;   /** Weight for random selection (must be > 0) */
} EseSpriteWeight;

/**
 * @brief Represents a mapping for a single tile ID to weighted sprites.
 */
 typedef struct EseTileMapping {
    EseSpriteWeight *sprites;  /** Array of weighted sprites */
    size_t sprite_count;       /** Number of sprites in this mapping */
    size_t sprite_capacity;    /** Allocated capacity for sprites array */
    uint32_t total_weight;     /** Sum of all weights for fast selection */
} EseTileMapping;

// The actual EseTileSet struct definition (private to this file)
typedef struct EseTileSet {
    EseTileMapping mappings[256]; /** Mappings indexed by tile_id */
    uint32_t rng_seed;

    // Lua integration
    lua_State *state;        /** Lua State this EseTileSet belongs to */
    int lua_ref;             /** Lua registry reference to its own userdata */
    int lua_ref_count;       /** Number of times this tileset has been referenced in C */
    bool destroyed;          /** Flag to track if tileset has been destroyed */
} EseTileSet;

/* ----------------- RNG ----------------- */

static uint32_t _get_random_weight(EseTileSet *tiles, uint32_t max_weight) {
    if (max_weight == 0) return 0;
    if (tiles->rng_seed == 0) {
        tiles->rng_seed = (uint32_t)time(NULL);
    }
    tiles->rng_seed = (tiles->rng_seed * 1664525 + 1013904223); // LCG
    return tiles->rng_seed % max_weight;
}

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseTileSet *_ese_tileset_make(void);

// Lua metamethods
static int _ese_tileset_lua_gc(lua_State *L);
static int _ese_tileset_lua_index(lua_State *L);
static int _ese_tileset_lua_newindex(lua_State *L);
static int _ese_tileset_lua_tostring(lua_State *L);

// Lua constructors
static int _ese_tileset_lua_new(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseTileSet instance with default values
 * 
 * Allocates memory for a new EseTileSet and initializes all fields to safe defaults.
 * The tileset starts with empty mappings and no Lua state.
 * 
 * @return Pointer to the newly created EseTileSet, or NULL on allocation failure
 */
static EseTileSet *_ese_tileset_make() {
    EseTileSet *tiles = (EseTileSet *)memory_manager.malloc(sizeof(EseTileSet), MMTAG_TILESET);
    if (!tiles) return NULL;
    
    for (int i = 0; i < 256; i++) {
        tiles->mappings[i].sprites = NULL;
        tiles->mappings[i].sprite_count = 0;
        tiles->mappings[i].sprite_capacity = 0;
        tiles->mappings[i].total_weight = 0;
    }
    
    tiles->rng_seed = 0;
    tiles->state = NULL;
    tiles->lua_ref = LUA_NOREF;
    tiles->lua_ref_count = 0;
    tiles->destroyed = false;
    return tiles;
}

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseTileSet
 * 
 * Handles cleanup when a Lua userdata for an EseTileSet is garbage collected.
 * Only frees the underlying EseTileSet if it has no C-side references.
 * 
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static int _ese_tileset_lua_gc(lua_State *L) {
    // Get from userdata
    EseTileSet **ud = (EseTileSet **)luaL_testudata(L, 1, TILESET_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseTileSet *tiles = *ud;
    if (tiles && !tiles->destroyed) {
        // If lua_ref == LUA_NOREF, there are no more references to this tileset, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this tileset was referenced from C and should not be freed.
        if (tiles->lua_ref == LUA_NOREF) {
            ese_tileset_destroy(tiles);
        }
    }

    return 0;
}

/* ----------------- Lua Methods ----------------- */

static int _ese_tileset_lua_add_sprite(lua_State *L) {
    EseTileSet *tiles = ese_tileset_lua_get(L, 1);
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

    lua_pushboolean(L, ese_tileset_add_sprite(tiles, tile_id, sprite_str, weight));
    return 1;
}

static int _ese_tileset_lua_remove_sprite(lua_State *L) {
    EseTileSet *tiles = ese_tileset_lua_get(L, 1);
    if (!tiles) return luaL_error(L, "Invalid Tiles in remove_sprite");

    if (!lua_isnumber(L, 2) || !lua_isstring(L, 3))
        return luaL_error(L, "remove_sprite(tile_id, sprite_id) requires number, string");

    uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
    const char *sprite_str = lua_tostring(L, 3);

    if (!sprite_str || strlen(sprite_str) == 0)
        return luaL_error(L, "sprite_id cannot be empty");

    lua_pushboolean(L, ese_tileset_remove_sprite(tiles, tile_id, sprite_str));
    return 1;
}

static int _ese_tileset_lua_get_sprite(lua_State *L) {
    EseTileSet *tiles = ese_tileset_lua_get(L, 1);
    if (!tiles) return luaL_error(L, "Invalid Tiles in get_sprite");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "get_sprite(tile_id) requires a number");

    uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
    const char *sprite = ese_tileset_get_sprite(tiles, tile_id);

    if (!sprite) lua_pushnil(L);
    else lua_pushstring(L, sprite);
    return 1;
}

static int _ese_tileset_lua_clear_mapping(lua_State *L) {
    EseTileSet *tiles = ese_tileset_lua_get(L, 1);
    if (!tiles) return luaL_error(L, "Invalid Tiles in clear_mapping");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "clear_mapping(tile_id) requires a number");

    uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
    ese_tileset_clear_mapping(tiles, tile_id);
    return 0;
}

static int _ese_tileset_lua_get_sprite_count(lua_State *L) {
    EseTileSet *tiles = ese_tileset_lua_get(L, 1);
    if (!tiles) return luaL_error(L, "Invalid Tiles in get_sprite_count");

    if (!lua_isnumber(L, 2))
        return luaL_error(L, "get_sprite_count(tile_id) requires a number");

    uint8_t tile_id = (uint8_t)lua_tonumber(L, 2);
    lua_pushnumber(L, ese_tileset_get_sprite_count(tiles, tile_id));
    return 1;
}

static int _ese_tileset_lua_update_sprite_weight(lua_State *L) {
    EseTileSet *tiles = ese_tileset_lua_get(L, 1);
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

    lua_pushboolean(L, ese_tileset_update_sprite_weight(tiles, tile_id, sprite_str, weight));
    return 1;
}

/**
 * @brief Lua __index metamethod for EseTileSet property access
 * 
 * Provides read access to tileset methods from Lua. When a Lua script
 * accesses tileset.method, this function is called to retrieve the methods.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid methods, 0 for invalid)
 */
static int _ese_tileset_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_TILESET_INDEX);
    EseTileSet *tiles = ese_tileset_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!tiles || !key) {
        profile_cancel(PROFILE_LUA_TILESET_INDEX);
        return 0;
    }

    if (strcmp(key, "add_sprite") == 0) {
        lua_pushcfunction(L, _ese_tileset_lua_add_sprite);
        profile_stop(PROFILE_LUA_TILESET_INDEX, "ese_tileset_lua_index (method)");
        return 1;
    } else if (strcmp(key, "remove_sprite") == 0) {
        lua_pushcfunction(L, _ese_tileset_lua_remove_sprite);
        profile_stop(PROFILE_LUA_TILESET_INDEX, "ese_tileset_lua_index (method)");
        return 1;
    } else if (strcmp(key, "get_sprite") == 0) {
        lua_pushcfunction(L, _ese_tileset_lua_get_sprite);
        profile_stop(PROFILE_LUA_TILESET_INDEX, "ese_tileset_lua_index (method)");
        return 1;
    } else if (strcmp(key, "clear_mapping") == 0) {
        lua_pushcfunction(L, _ese_tileset_lua_clear_mapping);
        profile_stop(PROFILE_LUA_TILESET_INDEX, "ese_tileset_lua_index (method)");
        return 1;
    } else if (strcmp(key, "get_sprite_count") == 0) {
        lua_pushcfunction(L, _ese_tileset_lua_get_sprite_count);
        profile_stop(PROFILE_LUA_TILESET_INDEX, "ese_tileset_lua_index (method)");
        return 1;
    } else if (strcmp(key, "update_sprite_weight") == 0) {
        lua_pushcfunction(L, _ese_tileset_lua_update_sprite_weight);
        profile_stop(PROFILE_LUA_TILESET_INDEX, "ese_tileset_lua_index (method)");
        return 1;
    }
    profile_stop(PROFILE_LUA_TILESET_INDEX, "ese_tileset_lua_index (invalid)");
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseTileSet property assignment
 * 
 * Provides write access to tileset properties from Lua. Currently returns
 * an error as direct assignment is not supported - use methods instead.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _ese_tileset_lua_newindex(lua_State *L) {
    return luaL_error(L, "Direct assignment not supported - use methods");
}

/**
 * @brief Lua __tostring metamethod for EseTileSet string representation
 * 
 * Converts an EseTileSet to a human-readable string for debugging and display.
 * The format includes the memory address and total sprite count.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
static int _ese_tileset_lua_tostring(lua_State *L) {
    EseTileSet *tiles = ese_tileset_lua_get(L, 1);

    if (!tiles) {
        lua_pushstring(L, "Tileset: (invalid)");
        return 1;
    }

    size_t total = 0;
    for (int i = 0; i < 256; i++) {
        total += tiles->mappings[i].sprite_count;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Tileset: %p (total_sprites=%zu)", (void *)tiles, total);
    lua_pushstring(L, buf);
    return 1;
}

// Lua constructors
/**
 * @brief Lua constructor function for creating new EseTileSet instances
 * 
 * Creates a new EseTileSet from Lua. This function is called when Lua code 
 * executes `Tileset.new()`. It creates the underlying EseTileSet and returns 
 * a userdata that provides access to the tileset's methods.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the userdata)
 */
static int _ese_tileset_lua_new(lua_State *L) {
    profile_start(PROFILE_LUA_TILESET_NEW);

    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 0) {
        profile_cancel(PROFILE_LUA_TILESET_NEW);
        return luaL_error(L, "Tileset.new() takes 0 arguments");
    }

    // Create the tileset
    EseTileSet *tiles = _ese_tileset_make();
    if (!tiles) {
        profile_cancel(PROFILE_LUA_TILESET_NEW);
        return luaL_error(L, "Failed to create Tileset");
    }
    tiles->state = L;

    // Create userdata directly
    EseTileSet **ud = (EseTileSet **)lua_newuserdata(L, sizeof(EseTileSet *));
    *ud = tiles;

    // Attach metatable
    luaL_getmetatable(L, TILESET_PROXY_META);
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_TILESET_NEW, "ese_tileset_lua_new");
    return 1;
}

/* ----------------- Lua Methods ----------------- */

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseTileSet *ese_tileset_create(EseLuaEngine *engine) {
    log_assert("TILESET", engine, "ese_tileset_create called with NULL engine");
    EseTileSet *tiles = _ese_tileset_make();
    if (!tiles) return NULL;
    tiles->state = engine->runtime;
    return tiles;
}

EseTileSet *ese_tileset_copy(const EseTileSet *source) {
    log_assert("TILESET", source, "ese_tileset_copy called with NULL source");
    
    EseTileSet *copy = (EseTileSet *)memory_manager.malloc(sizeof(EseTileSet), MMTAG_TILESET);
    if (!copy) return NULL;
    
    // Copy basic fields
    copy->rng_seed = source->rng_seed;
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
    copy->lua_ref_count = 0;
    copy->destroyed = false;
    
    // Deep copy mappings
    for (int i = 0; i < 256; i++) {
        const EseTileMapping *src_mapping = &source->mappings[i];
        EseTileMapping *dst_mapping = &copy->mappings[i];
        
        if (src_mapping->sprite_count > 0) {
            dst_mapping->sprite_capacity = src_mapping->sprite_capacity;
            dst_mapping->sprite_count = src_mapping->sprite_count;
            dst_mapping->total_weight = src_mapping->total_weight;
            
            dst_mapping->sprites = (EseSpriteWeight *)memory_manager.malloc(
                sizeof(EseSpriteWeight) * dst_mapping->sprite_capacity, MMTAG_TILESET);
            if (!dst_mapping->sprites) {
                // Clean up previous allocations
                for (int j = 0; j < i; j++) {
                    if (copy->mappings[j].sprites) {
                        for (size_t k = 0; k < copy->mappings[j].sprite_count; k++) {
                            if (copy->mappings[j].sprites[k].sprite_id) {
                                memory_manager.free(copy->mappings[j].sprites[k].sprite_id);
                            }
                        }
                        memory_manager.free(copy->mappings[j].sprites);
                    }
                }
                memory_manager.free(copy);
                return NULL;
            }
            
            // Copy sprite data
            for (size_t j = 0; j < src_mapping->sprite_count; j++) {
                size_t len = strlen(src_mapping->sprites[j].sprite_id);
                dst_mapping->sprites[j].sprite_id = (char *)memory_manager.malloc(len + 1, MMTAG_TILESET);
                if (!dst_mapping->sprites[j].sprite_id) {
                    // Clean up
                    for (size_t k = 0; k < j; k++) {
                        memory_manager.free(dst_mapping->sprites[k].sprite_id);
                    }
                    memory_manager.free(dst_mapping->sprites);
                    for (int k = 0; k < i; k++) {
                        if (copy->mappings[k].sprites) {
                            for (size_t l = 0; l < copy->mappings[k].sprite_count; l++) {
                                memory_manager.free(copy->mappings[k].sprites[l].sprite_id);
                            }
                            memory_manager.free(copy->mappings[k].sprites);
                        }
                    }
                    memory_manager.free(copy);
                    return NULL;
                }
                strcpy(dst_mapping->sprites[j].sprite_id, src_mapping->sprites[j].sprite_id);
                dst_mapping->sprites[j].weight = src_mapping->sprites[j].weight;
            }
        } else {
            dst_mapping->sprites = NULL;
            dst_mapping->sprite_count = 0;
            dst_mapping->sprite_capacity = 0;
            dst_mapping->total_weight = 0;
        }
    }
    
    return copy;
}

void ese_tileset_destroy(EseTileSet *tiles) {
    if (!tiles || tiles->destroyed) return;
    
    tiles->destroyed = true;
    
    if (tiles->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
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
    } else {
        ese_tileset_unref(tiles);
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

size_t ese_tileset_sizeof(void) {
    return sizeof(EseTileSet);
}

// Lua integration
void ese_tileset_lua_init(EseLuaEngine *engine) {
    log_assert("TILESET", engine, "ese_tileset_lua_init called with NULL engine");
    
    // Create metatable
    lua_engine_new_object_meta(engine, TILESET_PROXY_META, 
        _ese_tileset_lua_index, 
        _ese_tileset_lua_newindex, 
        _ese_tileset_lua_gc, 
        _ese_tileset_lua_tostring);

    // Create global Tileset table with functions
    const char *keys[] = {"new"};
    lua_CFunction functions[] = {_ese_tileset_lua_new};
    lua_engine_new_object(engine, "Tileset", 1, keys, functions);
}

void ese_tileset_lua_push(EseTileSet *tiles) {
    log_assert("TILESET", tiles, "ese_tileset_lua_push called with NULL tiles");

    if (tiles->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseTileSet **ud = (EseTileSet **)lua_newuserdata(tiles->state, sizeof(EseTileSet *));
        *ud = tiles;

        // Attach metatable
        luaL_getmetatable(tiles->state, TILESET_PROXY_META);
        lua_setmetatable(tiles->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(tiles->state, LUA_REGISTRYINDEX, tiles->lua_ref);
    }
}

EseTileSet *ese_tileset_lua_get(lua_State *L, int idx) {
    log_assert("TILESET", L, "ese_tileset_lua_get called with NULL Lua state");
    
    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseTileSet **ud = (EseTileSet **)luaL_testudata(L, idx, TILESET_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_tileset_ref(EseTileSet *tiles) {
    log_assert("TILESET", tiles, "ese_tileset_ref called with NULL tiles");
    
    if (tiles->lua_ref == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseTileSet **ud = (EseTileSet **)lua_newuserdata(tiles->state, sizeof(EseTileSet *));
        *ud = tiles;

        // Attach metatable
        luaL_getmetatable(tiles->state, TILESET_PROXY_META);
        lua_setmetatable(tiles->state, -2);

        // Store hard reference to prevent garbage collection
        tiles->lua_ref = luaL_ref(tiles->state, LUA_REGISTRYINDEX);
        tiles->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        tiles->lua_ref_count++;
    }

    profile_count_add("ese_tileset_ref_count");
}

void ese_tileset_unref(EseTileSet *tiles) {
    if (!tiles) return;
    
    if (tiles->lua_ref != LUA_NOREF && tiles->lua_ref_count > 0) {
        tiles->lua_ref_count--;
        
        if (tiles->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(tiles->state, LUA_REGISTRYINDEX, tiles->lua_ref);
            tiles->lua_ref = LUA_NOREF;
        }
    }

    profile_count_add("ese_tileset_unref_count");
}

// Lua-related access
lua_State *ese_tileset_get_state(const EseTileSet *tiles) {
    log_assert("TILESET", tiles, "ese_tileset_get_state called with NULL tiles");
    return tiles->state;
}

int ese_tileset_get_lua_ref(const EseTileSet *tiles) {
    log_assert("TILESET", tiles, "ese_tileset_get_lua_ref called with NULL tiles");
    return tiles->lua_ref;
}

int ese_tileset_get_lua_ref_count(const EseTileSet *tiles) {
    log_assert("TILESET", tiles, "ese_tileset_get_lua_ref_count called with NULL tiles");
    return tiles->lua_ref_count;
}

uint32_t ese_tileset_get_rng_seed(const EseTileSet *tiles) {
    log_assert("TILESET", tiles, "ese_tileset_get_rng_seed called with NULL tiles");
    return tiles->rng_seed;
}

/* ----------------- Sprite Management ----------------- */

bool ese_tileset_add_sprite(EseTileSet *tiles, uint8_t tile_id, const char *sprite_id, uint16_t weight) {
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
            mapping->sprites, sizeof(EseSpriteWeight) * new_capacity, MMTAG_TILESET);
        if (!new_array) return false;
        mapping->sprites = new_array;
        mapping->sprite_capacity = new_capacity;
    }

    // Copy string
    size_t len = strlen(sprite_id);
    char *copy = (char *)memory_manager.malloc(len + 1, MMTAG_TILESET);
    if (!copy) return false;
    memcpy(copy, sprite_id, len + 1);

    mapping->sprites[mapping->sprite_count].sprite_id = copy;
    mapping->sprites[mapping->sprite_count].weight = weight;
    mapping->sprite_count++;
    mapping->total_weight += weight;

    return true;
}

bool ese_tileset_remove_sprite(EseTileSet *tiles, uint8_t tile_id, const char *sprite_id) {
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

const char *ese_tileset_get_sprite(EseTileSet *tiles, uint8_t tile_id) {
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

void ese_tileset_clear_mapping(EseTileSet *tiles, uint8_t tile_id) {
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

size_t ese_tileset_get_sprite_count(const EseTileSet *tiles, uint8_t tile_id) {
    if (!tiles) return 0;
    return tiles->mappings[tile_id].sprite_count;
}

bool ese_tileset_update_sprite_weight(EseTileSet *tiles, uint8_t tile_id, const char *sprite_id, uint16_t new_weight) {
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

void ese_tileset_set_seed(EseTileSet *tiles, uint32_t seed) {
    if (tiles) {
        tiles->rng_seed = seed;
    }
}
