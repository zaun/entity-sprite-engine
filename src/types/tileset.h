/**
 * @file tileset.h
 * @brief Tile system that maps tile IDs to weighted sprite lists
 * @details Provides weighted random selection of sprites for tile mapping
 * 
 * @copyright Copyright (c) 2024 ESE Project
 * @license See LICENSE.md for license information
 */

#ifndef ESE_TILESET_H
#define ESE_TILESET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ========================================
// DEFINES AND STRUCTS
// ========================================

#define TILESET_PROXY_META "TilesetProxyMeta"
#define TILESET_META "TilesetMeta"

/**
 * @brief Tile system that maps tile IDs to weighted sprite lists.
 *
 * @details
 * This structure provides a mapping from tile_id (uint8_t) to weighted
 * lists of sprite_ids (strings). Supports weighted random selection.
 * Integrated with Lua via userdata with reference counting.
 */
typedef struct EseTileSet EseTileSet;

// ========================================
// FORWARD DECLARATIONS
// ========================================

typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseSprite EseSprite;

// ========================================
// LUA API
// ========================================

/**
 * @brief Initializes the EseTileSet userdata type in the Lua state.
 *
 * @details
 * Creates and registers the "TilesetProxyMeta" metatable with
 * __index, __newindex, __gc, and __tostring metamethods.
 * This allows EseTileSet objects to be used naturally from Lua
 * with dot notation and automatic garbage collection.
 *
 * @param engine EseLuaEngine pointer where the EseTileSet type will be registered
 */
void ese_tileset_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EseTileSet object to the Lua stack.
 * 
 * @details If the tileset has no Lua references (lua_ref == LUA_NOREF), creates a new
 *          userdata. If the tileset has Lua references, retrieves the existing
 *          userdata from the registry.
 * 
 * @param tiles Pointer to the EseTileSet object to push to Lua
 */
void ese_tileset_lua_push(EseTileSet *tiles);

/**
 * @brief Extracts a EseTileSet pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EseTileSet pointer from the userdata
 *          that was created by tileset_lua_push(). Performs
 *          type checking to ensure the object is a valid EseTileSet userdata
 *          with the correct metatable.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseTileSet object
 * @return Pointer to the EseTileSet object, or NULL if extraction fails or type check fails
 * 
 * @warning Returns NULL for invalid objects - always check return value before use
 */
EseTileSet *ese_tileset_lua_get(lua_State *L, int idx);

/**
 * @brief References a EseTileSet object for Lua access with reference counting.
 * 
 * @details If tiles->lua_ref is LUA_NOREF, pushes the tileset to Lua and references it,
 *          setting lua_ref_count to 1. If tiles->lua_ref is already set, increments
 *          the reference count by 1. This prevents the tileset from being garbage
 *          collected while C code holds references to it.
 * 
 * @param tiles Pointer to the EseTileSet object to reference
 */
void ese_tileset_ref(EseTileSet *tiles);

/**
 * @brief Unreferences a EseTileSet object, decrementing the reference count.
 * 
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua reference
 *          is removed from the registry. This function does NOT free memory.
 * 
 * @param tiles Pointer to the EseTileSet object to unreference
 */
void ese_tileset_unref(EseTileSet *tiles);

// ========================================
// C API
// ========================================

/**
 * @brief Creates a new EseTileSet object.
 * 
 * @details Allocates memory for a new EseTileSet and initializes all mappings to empty.
 *          The tileset is created without Lua references and must be explicitly
 *          referenced with tileset_ref() if Lua access is desired.
 * 
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseTileSet object
 * 
 * @warning The returned EseTileSet must be freed with tileset_destroy() to prevent memory leaks
 */
EseTileSet *ese_tileset_create(EseLuaEngine *engine);

/**
 * @brief Copies a source EseTileSet into a new EseTileSet object.
 * 
 * @details This function creates a deep copy of an EseTileSet object. It allocates a new EseTileSet
 *          struct and copies all members, including the sprite mappings. The copy is created without 
 *          Lua references and must be explicitly referenced with tileset_ref() if Lua access is desired.
 * 
 * @param source Pointer to the source EseTileSet to copy.
 * @return A new, distinct EseTileSet object that is a copy of the source.
 * 
 * @warning The returned EseTileSet must be freed with tileset_destroy() to prevent memory leaks.
 */
EseTileSet *ese_tileset_copy(const EseTileSet *source);

/**
 * @brief Destroys a EseTileSet object, managing memory based on Lua references.
 * 
 * @details If the tileset has no Lua references (lua_ref == LUA_NOREF), frees memory immediately.
 *          If the tileset has Lua references, decrements the reference counter.
 *          When the counter reaches 0, removes the Lua reference and lets
 *          Lua's garbage collector handle final cleanup.
 * 
 * @note If the tileset is Lua-owned, memory may not be freed immediately.
 *       Lua's garbage collector will finalize it once no references remain.
 * 
 * @param tiles Pointer to the EseTileSet object to destroy
 */
void ese_tileset_destroy(EseTileSet *tiles);

/**
 * @brief Gets the size of the EseTileSet structure in bytes.
 * 
 * @return The size of the EseTileSet structure in bytes
 */
size_t ese_tileset_sizeof(void);

// Lua-related access
/**
 * @brief Gets the Lua state associated with this tileset.
 * 
 * @param tiles Pointer to the EseTileSet object
 * @return Pointer to the Lua state, or NULL if none
 */
lua_State *ese_tileset_get_state(const EseTileSet *tiles);

/**
 * @brief Gets the Lua registry reference for this tileset.
 * 
 * @param tiles Pointer to the EseTileSet object
 * @return The Lua registry reference value
 */
int ese_tileset_get_lua_ref(const EseTileSet *tiles);

/**
 * @brief Gets the Lua reference count for this tileset.
 * 
 * @param tiles Pointer to the EseTileSet object
 * @return The current reference count
 */
int ese_tileset_get_lua_ref_count(const EseTileSet *tiles);

/**
 * @brief Gets the RNG seed of the tileset
 * 
 * @param tiles Pointer to the EseTileSet
 * @return The current RNG seed value
 */
uint32_t ese_tileset_get_rng_seed(const EseTileSet *tiles);

// ========================================
// SPRITE MANAGEMENT
// ========================================

/**
 * @brief Adds a sprite with weight to a tile mapping.
 *
 * @param tiles Pointer to the EseTileSet object
 * @param tile_id The tile ID to add the sprite to
 * @param sprite_id The sprite string to add (copied internally)
 * @param weight The weight for random selection (must be > 0)
 * @return true if successful, false if memory allocation fails
 */
bool ese_tileset_add_sprite(EseTileSet *tiles, uint8_t tile_id,
                        const char *sprite_id, uint16_t weight);

/**
 * @brief Removes a sprite from a tile mapping.
 *
 * @param tiles Pointer to the EseTileSet object
 * @param tile_id The tile ID to remove the sprite from
 * @param sprite_id The sprite string to remove
 * @return true if successful, false if sprite not found
 */
bool ese_tileset_remove_sprite(EseTileSet *tiles, uint8_t tile_id,
                           const char *sprite_id);

/**
 * @brief Gets a random sprite based on weights for a tile.
 *
 * @param tiles Pointer to the EseTileSet object
 * @param tile_id The tile ID to get a sprite for
 * @return A EseSprite based on weights, or NULL if no mapping exists
 */
const char *ese_tileset_get_sprite(EseTileSet *tiles, uint8_t tile_id);

/**
 * @brief Clears all sprites from a tile mapping.
 *
 * @param tiles Pointer to the EseTileSet object
 * @param tile_id The tile ID to clear
 */
void ese_tileset_clear_mapping(EseTileSet *tiles, uint8_t tile_id);

/**
 * @brief Gets the number of sprites for a tile mapping.
 *
 * @param tiles Pointer to the EseTileSet object
 * @param tile_id The tile ID to check
 * @return Number of sprites in the mapping
 */
size_t ese_tileset_get_sprite_count(const EseTileSet *tiles, uint8_t tile_id);

/**
 * @brief Updates the weight of an existing sprite in a tile mapping.
 *
 * @param tiles Pointer to the EseTileSet object
 * @param tile_id The tile ID containing the sprite
 * @param sprite_id The sprite string to update
 * @param new_weight The new weight value (must be > 0)
 * @return true if successful, false if sprite not found
 */
bool ese_tileset_update_sprite_weight(EseTileSet *tiles, uint8_t tile_id,
                                  const char *sprite_id, uint16_t new_weight);

/**
 * @brief Sets the RNG seed for the tileset
 * 
 * @param tiles Pointer to the EseTileSet object
 * @param seed The new RNG seed value
 */
void ese_tileset_set_seed(EseTileSet *tiles, uint32_t seed);

/**
 * @brief Sets the Lua state associated with this tileset.
 * 
 * @param tiles Pointer to the EseTileSet object
 * @param state Pointer to the Lua state
 */
void ese_tileset_set_state(EseTileSet *tiles, lua_State *state);

/**
 * @brief Creates a new EseTileSet instance with default values
 * 
 * @details Internal function used by Lua constructors and other internal functions.
 * Allocates memory for a new EseTileSet and initializes all fields to safe defaults.
 * The tileset starts with empty mappings and no Lua state.
 * 
 * @return Pointer to the newly created EseTileSet, or NULL on allocation failure
 */
EseTileSet *_ese_tileset_make(void);

#endif // ESE_TILESET_H
