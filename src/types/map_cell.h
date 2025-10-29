#ifndef ESE_MAP_CELL_H
#define ESE_MAP_CELL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAP_CELL_PROXY_META "MapCellProxyMeta"
#define MAP_CELL_META "MapCellMeta"

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseSprite EseSprite;
typedef struct EseMap EseMap;
typedef struct EseMapCell EseMapCell;

// Map cell flags
// Marks a cell as collidable/solid for entity↔map collisions
#define MAP_CELL_FLAG_SOLID (1u << 0)

/**
 * @brief Callback function type for map cell change notifications.
 *
 * @param cell Pointer to the EseMapCell that changed
 * @param userdata User-provided data passed when registering the watcher
 */
typedef void (*EseMapCellWatcherCallback)(EseMapCell *cell, void *userdata);

/**
 * @brief Represents a map cell with multiple tile layers and properties.
 *
 * @details
 * Each map cell can contain multiple tile layers (stacked tile IDs),
 * cell-wide flags, and optional game-specific data. It is also
 * integrated with Lua via userdata, allowing natural access
 * from Lua scripts.
 */
typedef struct EseMapCell EseMapCell;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
/**
 * @brief Creates a new EseMapCell object.
 *
 * @details Allocates memory for a new EseMapCell and initializes with empty
 * layers. The map cell is created without Lua references and must be explicitly
 *          referenced with ese_map_cell_ref() if Lua access is desired.
 *
 * @param engine Pointer to a EseLuaEngine
 * @param map Pointer to a EseMap this cell belongs to
 * @return Pointer to newly created EseMapCell object
 *
 * @warning The returned EseMapCell must be freed with ese_map_cell_destroy() to
 * prevent memory leaks
 */
EseMapCell *ese_map_cell_create(EseLuaEngine *engine, EseMap *map);

/**
 * @brief Copies a source EseMapCell into a new EseMapCell object.
 *
 * @details This function creates a deep copy of an EseMapCell object. It
 * allocates a new EseMapCell struct and copies all members, including the
 * tile_ids array. The copy is created without Lua references and must be
 * explicitly referenced with ese_map_cell_ref() if Lua access is desired.
 *
 * @param source Pointer to the source EseMapCell to copy.
 * @return A new, distinct EseMapCell object that is a copy of the source.
 *
 * @warning The returned EseMapCell must be freed with ese_map_cell_destroy() to
 * prevent memory leaks.
 */
EseMapCell *ese_map_cell_copy(const EseMapCell *source);

/**
 * @brief Destroys a EseMapCell object, managing memory based on Lua references.
 *
 * @details If the map cell has no Lua references (lua_ref == LUA_NOREF), frees
 * memory immediately. If the map cell has Lua references, decrements the
 * reference counter. When the counter reaches 0, removes the Lua reference and
 * lets Lua's garbage collector handle final cleanup.
 *
 * @note If the map cell is Lua-owned, memory may not be freed immediately.
 *       Lua's garbage collector will finalize it once no references remain.
 *
 * @param cell Pointer to the EseMapCell object to destroy
 */
void ese_map_cell_destroy(EseMapCell *cell);

/**
 * @brief Gets the size of the EseMapCell structure in bytes.
 *
 * @return The size of the EseMapCell structure in bytes
 */
size_t ese_map_cell_sizeof(void);

// Lua-related access
/**
 * @brief Gets the Lua state associated with this map cell.
 *
 * @param cell Pointer to the EseMapCell object
 * @return Pointer to the Lua state, or NULL if none
 */
lua_State *ese_map_cell_get_state(const EseMapCell *cell);

/**
 * @brief Gets the Lua registry reference for this map cell.
 *
 * @param cell Pointer to the EseMapCell object
 * @return The Lua registry reference value
 */
int ese_map_cell_get_lua_ref(const EseMapCell *cell);

/**
 * @brief Gets the Lua reference count for this map cell.
 *
 * @param cell Pointer to the EseMapCell object
 * @return The current reference count
 */
int ese_map_cell_get_lua_ref_count(const EseMapCell *cell);

// Lua integration
/**
 * @brief Initializes the EseMapCell userdata type in the Lua state.
 *
 * @details Creates and registers the "MapCellProxyMeta" metatable with __index,
 * __newindex,
 *          __gc, __tostring metamethods for property access and garbage
 * collection. This allows EseMapCell objects to be used naturally from Lua with
 * dot notation. Also creates the global "MapCell" table with "new" constructor.
 *
 * @param engine EseLuaEngine pointer where the EseMapCell type will be
 * registered
 */
void ese_map_cell_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EseMapCell object to the Lua stack.
 *
 * @details If the map cell has no Lua references (lua_ref == LUA_NOREF),
 * creates a new userdata. If the map cell has Lua references, retrieves the
 * existing userdata from the registry.
 *
 * @param cell Pointer to the EseMapCell object to push to Lua
 */
void ese_map_cell_lua_push(EseMapCell *cell);

/**
 * @brief Extracts a EseMapCell pointer from a Lua userdata object with type
 * safety.
 *
 * @details Retrieves the C EseMapCell pointer from the userdata
 *          that was created by ese_map_cell_lua_push(). Performs
 *          type checking to ensure the object is a valid EseMapCell userdata
 *          with the correct metatable.
 *
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseMapCell object
 * @return Pointer to the EseMapCell object, or NULL if extraction fails or type
 * check fails
 *
 * @warning Returns NULL for invalid objects - always check return value before
 * use
 */
EseMapCell *ese_map_cell_lua_get(lua_State *L, int idx);

/**
 * @brief References a EseMapCell object for Lua access with reference counting.
 *
 * @details If cell->lua_ref is LUA_NOREF, pushes the map cell to Lua and
 * references it, setting lua_ref_count to 1. If cell->lua_ref is already set,
 * increments the reference count by 1. This prevents the map cell from being
 * garbage collected while C code holds references to it.
 *
 * @param cell Pointer to the EseMapCell object to reference
 */
void ese_map_cell_ref(EseMapCell *cell);

/**
 * @brief Unreferences a EseMapCell object, decrementing the reference count.
 *
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua
 * reference is removed from the registry. This function does NOT free memory.
 *
 * @param cell Pointer to the EseMapCell object to unreference
 */
void ese_map_cell_unref(EseMapCell *cell);

// Tile Layer API
/**
 * @brief Adds a tile layer to the map cell.
 *
 * @param cell Pointer to the EseMapCell object
 * @param tile_id The tile ID to add as a new layer
 * @return true if successful, false if memory allocation fails
 */
bool ese_map_cell_add_layer(EseMapCell *cell, int tile_id);

/**
 * @brief Removes a tile layer from the map cell by index.
 *
 * @param cell Pointer to the EseMapCell object
 * @param layer_index Index of the layer to remove (0-based)
 * @return true if successful, false if index is out of bounds
 */
bool ese_map_cell_remove_layer(EseMapCell *cell, size_t layer_index);

/**
 * @brief Gets a tile ID from a specific layer.
 *
 * @param cell Pointer to the EseMapCell object
 * @param layer_index Index of the layer to get (0-based)
 * @return The tile ID, or 0 if index is out of bounds
 */
int ese_map_cell_get_layer(const EseMapCell *cell, size_t layer_index);

/**
 * @brief Sets a tile ID for a specific layer.
 *
 * @param cell Pointer to the EseMapCell object
 * @param layer_index Index of the layer to set (0-based)
 * @param tile_id The new tile ID
 * @return true if successful, false if index is out of bounds
 */
bool ese_map_cell_set_layer(EseMapCell *cell, size_t layer_index, uint8_t tile_id);

/**
 * @brief Clears all layers from the map cell.
 *
 * @param cell Pointer to the EseMapCell object
 */
void ese_map_cell_clear_layers(EseMapCell *cell);

/**
 * @brief Checks if the map cell has any layers.
 *
 * @param cell Pointer to the EseMapCell object
 * @return true if the cell has at least one layer, false otherwise
 */
bool ese_map_cell_has_layers(const EseMapCell *cell);

/**
 * @brief Gets the number of layers in the map cell.
 *
 * @param cell Pointer to the EseMapCell object
 * @return The number of layers
 */
size_t ese_map_cell_get_layer_count(const EseMapCell *cell);

// Property access
/**
 * @brief Sets the isDynamic property of the map cell.
 *
 * @param cell Pointer to the EseMapCell object
 * @param isDynamic The isDynamic value
 */
void ese_map_cell_set_is_dynamic(EseMapCell *cell, bool isDynamic);

/**
 * @brief Gets the isDynamic property of the map cell.
 *
 * @param cell Pointer to the EseMapCell object
 * @return The isDynamic value
 */
bool ese_map_cell_get_is_dynamic(const EseMapCell *cell);

/**
 * @brief Sets the flags property of the map cell.
 *
 * @param cell Pointer to the EseMapCell object
 * @param flags The flags value
 */
void ese_map_cell_set_flags(EseMapCell *cell, uint32_t flags);

/**
 * @brief Gets the flags property of the map cell.
 *
 * @param cell Pointer to the EseMapCell object
 * @return The flags value
 */
uint32_t ese_map_cell_get_flags(const EseMapCell *cell);

// Flag API
/**
 * @brief Checks if a specific flag is set.
 *
 * @param cell Pointer to the EseMapCell object
 * @param flag The flag bit to check
 * @return true if flag is set, false otherwise
 */
bool ese_map_cell_has_flag(const EseMapCell *cell, uint32_t flag);

/**
 * @brief Sets a specific flag.
 *
 * @param cell Pointer to the EseMapCell object
 * @param flag The flag bit to set
 */
void ese_map_cell_set_flag(EseMapCell *cell, uint32_t flag);

/**
 * @brief Clears a specific flag.
 *
 * @param cell Pointer to the EseMapCell object
 * @param flag The flag bit to clear
 */
void ese_map_cell_clear_flag(EseMapCell *cell, uint32_t flag);

// Watcher API
/**
 * @brief Adds a watcher callback to be notified when any map cell property
 * changes.
 *
 * @param cell Pointer to the EseMapCell object to watch
 * @param callback Function to call when properties change
 * @param userdata User-provided data to pass to the callback
 * @return true if watcher was added successfully, false otherwise
 */
bool ese_map_cell_add_watcher(EseMapCell *cell, EseMapCellWatcherCallback callback, void *userdata);

/**
 * @brief Removes a previously registered watcher callback.
 *
 * @param cell Pointer to the EseMapCell object
 * @param callback Function to remove
 * @param userdata User data that was used when registering
 * @return true if watcher was removed, false if not found
 */
bool ese_map_cell_remove_watcher(EseMapCell *cell, EseMapCellWatcherCallback callback,
                                 void *userdata);

#endif // ESE_MAP_CELL_H
