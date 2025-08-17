#ifndef ESE_TILE_SET_H
#define ESE_TILE_SET_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Represents a weighted sprite entry for tile mapping.
 */
typedef struct EseSpriteWeight {
    char sprite_id;                  /**< The sprite character/ID */
    uint16_t weight;                 /**< Weight for random selection */
} EseSpriteWeight;

/**
 * @brief Represents a mapping for a single tile ID to weighted sprites.
 */
typedef struct EseTileMapping {
    EseSpriteWeight *sprites;        /**< Array of weighted sprites */
    size_t sprite_count;             /**< Number of sprites in this mapping */
    size_t sprite_capacity;          /**< Allocated capacity for sprites array */
    uint32_t total_weight;           /**< Sum of all weights for fast selection */
} EseTileMapping;

/**
 * @brief Tile system that maps tile IDs to weighted sprite lists.
 * 
 * @details This structure provides a mapping from tile_id (uint8_t) to weighted
 *          lists of sprite_ids (chars). Supports weighted random selection.
 */
typedef struct EseTileSet {
    EseTileMapping mappings[256];    /**< Mappings indexed by tile_id */
    
    // Lua integration
    lua_State *state;                /**< Lua State this EseTileSet belongs to */
    int lua_ref;                     /**< Lua registry reference to its own proxy table */
} EseTileSet;

/**
 * @brief Initializes the EseTileSet userdata type in the Lua state.
 * 
 * @param engine EseLuaEngine pointer where the EseTileSet type will be registered
 */
void tileset_lua_init(EseLuaEngine *engine);

/**
 * @brief Creates a new EseTileSet object.
 * 
 * @param engine Pointer to a EseLuaEngine
 * @param c_only True if this object won't be accessible in LUA
 * @return Pointer to newly created EseTileSet object
 * 
 * @warning The returned EseTileSet must be freed with tileset_destroy()
 */
EseTileSet *tileset_create(EseLuaEngine *engine, bool c_only);

/**
 * @brief Destroys a EseTileSet object and frees its memory.
 * 
 * @param tiles Pointer to the EseTileSet object to destroy
 */
void tileset_destroy(EseTileSet *tiles);

/**
 * @brief Extracts a EseTileSet pointer from a Lua userdata object.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseTileSet object
 * @return Pointer to the EseTileSet object, or NULL if extraction fails
 */
EseTileSet *tileset_lua_get(lua_State *L, int idx);

/**
 * @brief Adds a sprite with weight to a tile mapping.
 * 
 * @param tiles Pointer to the EseTileSet object
 * @param tile_id The tile ID to add the sprite to
 * @param sprite_id The sprite character to add
 * @param weight The weight for random selection (default 1 if 0)
 * @return true if successful, false if memory allocation fails
 */
bool tileset_add_sprite(EseTileSet *tiles, uint8_t tile_id, char sprite_id, uint16_t weight);

/**
 * @brief Removes a sprite from a tile mapping.
 * 
 * @param tiles Pointer to the EseTileSet object
 * @param tile_id The tile ID to remove the sprite from
 * @param sprite_id The sprite character to remove
 * @return true if successful, false if sprite not found
 */
bool tileset_remove_sprite(EseTileSet *tiles, uint8_t tile_id, char sprite_id);

/**
 * @brief Gets a random sprite based on weights for a tile.
 * 
 * @param tiles Pointer to the EseTileSet object
 * @param tile_id The tile ID to get a sprite for
 * @return A sprite character based on weights, or 0 if no mapping exists
 */
char tileset_get_sprite(const EseTileSet *tiles, uint8_t tile_id);

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
 * @param sprite_id The sprite character to update
 * @param new_weight The new weight value
 * @return true if successful, false if sprite not found
 */
bool tileset_update_sprite_weight(EseTileSet *tiles, uint8_t tile_id, char sprite_id, uint16_t new_weight);

#endif // ESE_TILE_SET_H