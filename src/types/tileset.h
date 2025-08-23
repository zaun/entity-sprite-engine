#ifndef ESE_TILE_SET_H
#define ESE_TILE_SET_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseSprite EseSprite;

/**
 * @brief Represents a weighted sprite entry for tile mapping.
 */
typedef struct EseSpriteWeight {
    char *sprite_id;   /**< The sprite string (heap-allocated) */
    uint16_t weight;   /**< Weight for random selection (must be > 0) */
} EseSpriteWeight;

/**
 * @brief Represents a mapping for a single tile ID to weighted sprites.
 */
typedef struct EseTileMapping {
    EseSpriteWeight *sprites;  /**< Array of weighted sprites */
    size_t sprite_count;       /**< Number of sprites in this mapping */
    size_t sprite_capacity;    /**< Allocated capacity for sprites array */
    uint32_t total_weight;     /**< Sum of all weights for fast selection */
} EseTileMapping;

/**
 * @brief Tile system that maps tile IDs to weighted sprite lists.
 *
 * @details
 * This structure provides a mapping from tile_id (uint8_t) to weighted
 * lists of sprite_ids (strings). Supports weighted random selection.
 * Integrated with Lua via proxy tables.
 */
typedef struct EseTileSet {
    EseTileMapping mappings[256]; /**< Mappings indexed by tile_id */
    uint32_t rng_seed;

    // Lua integration
    lua_State *state;  /**< Lua State this EseTileSet belongs to */
    int lua_ref;       /**< Lua registry reference to its own proxy table */
} EseTileSet;

/* ----------------- Lua API ----------------- */

/**
 * @brief Initializes the EseTileSet userdata type in the Lua state.
 *
 * @details
 * Creates and registers the "TilesProxyMeta" metatable with
 * __index, __newindex, __gc, and __tostring metamethods.
 * This allows EseTileSet objects to be used naturally from Lua
 * with dot notation and automatic garbage collection.
 *
 * @param engine EseLuaEngine pointer where the EseTileSet type will be registered
 */
void tileset_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a registered EseTileSet proxy table back onto the Lua stack.
 *
 * @param tiles Pointer to the EseTileSet object
 */
void tileset_lua_push(EseTileSet *tiles);

/**
 * @brief Extracts a EseTileSet pointer from a Lua proxy table with type safety.
 *
 * @details
 * Retrieves the C EseTileSet pointer from the "__ptr" field of a Lua
 * table that was created by tileset_lua_push(). Performs type checking
 * to ensure the object is a valid EseTileSet proxy table with the correct
 * metatable and userdata pointer.
 *
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseTileSet object
 * @return Pointer to the EseTileSet object, or NULL if extraction fails
 *
 * @warning Returns NULL for invalid objects — always check return value before use.
 */
EseTileSet *tileset_lua_get(lua_State *L, int idx);

/* ----------------- C API ----------------- */

/**
 * @brief Creates a new EseTileSet object.
 *
 * @details
 * Allocates memory for a new EseTileSet and initializes all mappings to empty.
 * If `c_only` is false, the object is also registered with Lua and
 * wrapped in a proxy table. If true, the object exists only in C.
 *
 * @param engine Pointer to a EseLuaEngine
 * @param c_only True if this object won't be accessible in Lua
 * @return Pointer to newly created EseTileSet object
 *
 * @warning The returned EseTileSet must be freed with tileset_destroy()
 *          to prevent memory leaks.
 */
EseTileSet *tileset_create(EseLuaEngine *engine, bool c_only);

/**
 * @brief Destroys a EseTileSet object and frees its memory.
 *
 * @details
 * Frees the memory allocated by tileset_create(), including all sprite strings
 * and arrays. If the object was registered with Lua, its registry reference
 * is also released.
 *
 * @param tiles Pointer to the EseTileSet object to destroy
 */
void tileset_destroy(EseTileSet *tiles);

/* ----------------- Sprite Management ----------------- */

/**
 * @brief Adds a sprite with weight to a tile mapping.
 *
 * @param tiles Pointer to the EseTileSet object
 * @param tile_id The tile ID to add the sprite to
 * @param sprite_id The sprite string to add (copied internally)
 * @param weight The weight for random selection (must be > 0)
 * @return true if successful, false if memory allocation fails
 */
bool tileset_add_sprite(EseTileSet *tiles, uint8_t tile_id,
                        const char *sprite_id, uint16_t weight);

/**
 * @brief Removes a sprite from a tile mapping.
 *
 * @param tiles Pointer to the EseTileSet object
 * @param tile_id The tile ID to remove the sprite from
 * @param sprite_id The sprite string to remove
 * @return true if successful, false if sprite not found
 */
bool tileset_remove_sprite(EseTileSet *tiles, uint8_t tile_id,
                           const char *sprite_id);

/**
 * @brief Gets a random sprite based on weights for a tile.
 *
 * @param tiles Pointer to the EseTileSet object
 * @param tile_id The tile ID to get a sprite for
 * @return A EseSprite based on weights, or NULL if no mapping exists
 */
const char *tileset_get_sprite(EseTileSet *tiles, uint8_t tile_id);

/**
 * @brief Clears all sprites from a tile mapping.
 *
 * @param tiles Pointer to the EseTileSet object
 * @param tile_id The tile ID to clear
 */
void tileset_clear_mapping(EseTileSet *tiles, uint8_t tile_id);

/**
 * @brief Gets the number of sprites for a tile mapping.
 *
 * @param tiles Pointer to the EseTileSet object
 * @param tile_id The tile ID to check
 * @return Number of sprites in the mapping
 */
size_t tileset_get_sprite_count(const EseTileSet *tiles, uint8_t tile_id);

/**
 * @brief Updates the weight of an existing sprite in a tile mapping.
 *
 * @param tiles Pointer to the EseTileSet object
 * @param tile_id The tile ID containing the sprite
 * @param sprite_id The sprite string to update
 * @param new_weight The new weight value (must be > 0)
 * @return true if successful, false if sprite not found
 */
bool tileset_update_sprite_weight(EseTileSet *tiles, uint8_t tile_id,
                                  const char *sprite_id, uint16_t new_weight);

void tileset_set_seed(EseTileSet *tiles, uint32_t seed);

#endif // ESE_TILE_SET_H
