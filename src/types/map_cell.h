#ifndef ESE_MAP_CELL_H
#define ESE_MAP_CELL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseSprite EseSprite;

/**
 * @brief Represents a map cell with multiple tile layers and properties.
 *
 * @details
 * Each map cell can contain multiple tile layers (stacked tile IDs),
 * cell-wide flags, and optional game-specific data. It is also
 * integrated with Lua via a proxy table, allowing natural access
 * from Lua scripts.
 */
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
    int lua_ref;             /**< Lua registry reference to its own proxy table */
} EseMapCell;

/**
 * @brief Initializes the EseMapCell userdata type in the Lua state.
 *
 * @details
 * Creates and registers the "MapCellProxyMeta" metatable with
 * __index, __newindex, __gc, and __tostring metamethods.
 * This allows EseMapCell objects to be used naturally from Lua
 * with dot notation and automatic garbage collection.
 *
 * @param engine EseLuaEngine pointer where the EseMapCell type will be registered
 */
void mapcell_lua_init(EseLuaEngine *engine);

/**
 * @brief Creates a new EseMapCell object.
 *
 * @details
 * Allocates memory for a new EseMapCell and initializes with empty layers.
 * If `c_only` is false, the object is also registered with Lua and
 * wrapped in a proxy table. If true, the object exists only in C.
 *
 * @param engine Pointer to a EseLuaEngine
 * @param c_only True if this object won't be accessible in Lua
 * @return Pointer to newly created EseMapCell object
 *
 * @warning The returned EseMapCell must be freed with mapcell_destroy()
 *          to prevent memory leaks.
 */
EseMapCell *mapcell_create(EseLuaEngine *engine, bool c_only);

/**
 * @brief Copies a source EseMapCell into a new EseMapCell object.
 *
 * @details
 * This function creates a deep copy of an EseMapCell object.
 * It allocates a new EseMapCell struct and copies all members,
 * including the tile_ids array. The `data` pointer is shallow-copied
 * (caller is responsible for deep-copying if needed).
 *
 * @param source Pointer to the source EseMapCell to copy
 * @param c_only True if the copied object won't be accessible in Lua
 * @return A new, distinct EseMapCell object that is a copy of the source
 *
 * @warning The returned EseMapCell must be freed with mapcell_destroy()
 *          to prevent memory leaks.
 */
EseMapCell *mapcell_copy(const EseMapCell *source, bool c_only);

/**
 * @brief Destroys a EseMapCell object and frees its memory.
 *
 * @details
 * Frees the memory allocated by mapcell_create(), including the tile_ids array.
 * If the object was registered with Lua, its registry reference is also released.
 *
 * @param cell Pointer to the EseMapCell object to destroy
 */
void mapcell_destroy(EseMapCell *cell);

/**
 * @brief Extracts a EseMapCell pointer from a Lua proxy table with type safety.
 *
 * @details
 * Retrieves the C EseMapCell pointer from the "__ptr" field of a Lua
 * table that was created by mapcell_lua_push(). Performs type checking
 * to ensure the object is a valid EseMapCell proxy table with the correct
 * metatable and userdata pointer.
 *
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseMapCell object
 * @return Pointer to the EseMapCell object, or NULL if extraction fails
 *
 * @warning Returns NULL for invalid objects — always check return value before use.
 */
EseMapCell *mapcell_lua_get(lua_State *L, int idx);

/**
 * @brief Pushes a registered EseMapCell proxy table back onto the Lua stack.
 *
 * @param cell Pointer to the EseMapCell object
 */
void mapcell_lua_push(EseMapCell *cell);

/* ----------------- Tile Layer API ----------------- */

/**
 * @brief Adds a tile layer to the map cell.
 *
 * @param cell Pointer to the EseMapCell object
 * @param tile_id The tile ID to add as a new layer
 * @return true if successful, false if memory allocation fails
 */
bool mapcell_add_layer(EseMapCell *cell, uint8_t tile_id);

/**
 * @brief Removes a tile layer from the map cell by index.
 *
 * @param cell Pointer to the EseMapCell object
 * @param layer_index Index of the layer to remove (0-based)
 * @return true if successful, false if index is out of bounds
 */
bool mapcell_remove_layer(EseMapCell *cell, size_t layer_index);

/**
 * @brief Gets a tile ID from a specific layer.
 *
 * @param cell Pointer to the EseMapCell object
 * @param layer_index Index of the layer to get (0-based)
 * @return The tile ID, or 0 if index is out of bounds
 */
uint8_t mapcell_get_layer(const EseMapCell *cell, size_t layer_index);

/**
 * @brief Sets a tile ID for a specific layer.
 *
 * @param cell Pointer to the EseMapCell object
 * @param layer_index Index of the layer to set (0-based)
 * @param tile_id The new tile ID
 * @return true if successful, false if index is out of bounds
 */
bool mapcell_set_layer(EseMapCell *cell, size_t layer_index, uint8_t tile_id);

/**
 * @brief Clears all layers from the map cell.
 *
 * @param cell Pointer to the EseMapCell object
 */
void mapcell_clear_layers(EseMapCell *cell);

/**
 * @brief Checks if the map cell has any layers.
 *
 * @param cell Pointer to the EseMapCell object
 * @return true if the cell has at least one layer, false otherwise
 */
/* ----------------- Flag API ----------------- */

/**
 * @brief Checks if a specific flag is set.
 *
 * @param cell Pointer to the EseMapCell object
 * @param flag The flag bit to check
 * @return true if flag is set, false otherwise
 */
bool mapcell_has_flag(const EseMapCell *cell, uint32_t flag);

/**
 * @brief Sets a specific flag.
 *
 * @param cell Pointer to the EseMapCell object
 * @param flag The flag bit to set
 */
void mapcell_set_flag(EseMapCell *cell, uint32_t flag);

/**
 * @brief Clears a specific flag.
 *
 * @param cell Pointer to the EseMapCell object
 * @param flag The flag bit to clear
 */
void mapcell_clear_flag(EseMapCell *cell, uint32_t flag);

#endif // ESE_MAP_CELL_H
