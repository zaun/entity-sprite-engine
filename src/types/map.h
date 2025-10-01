#ifndef ESE_MAP_H
#define ESE_MAP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define MAP_PROXY_META "MapProxyMeta"

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseMapCell EseMapCell;
typedef struct EseTileSet EseTileSet;
typedef struct EseMap EseMap;

/**
 * @brief Callback function type for map change notifications.
 *
 * @param map Pointer to the EseMap that changed
 * @param userdata User-provided data passed when registering the watcher
 */
typedef void (*EseMapWatcherCallback)(EseMap *map, void *userdata);

/**
 * @brief Map type enumeration for different coordinate systems.
 */
typedef enum {
    MAP_TYPE_GRID = 0,          /** Standard grid/square tiles */
    MAP_TYPE_HEX_POINT_UP,      /** Hexagonal tiles with point facing up */
    MAP_TYPE_HEX_FLAT_UP,       /** Hexagonal tiles with flat side facing up */
    MAP_TYPE_ISO                /** Isometric tiles */
} EseMapType;

/* ----------------- Lua API ----------------- */

/**
 * @brief Initializes the EseMap userdata type in the Lua state.
 *
 * @details
 * Creates and registers the "MapProxyMeta" metatable with
 * __index, __newindex, __gc, and __tostring metamethods.
 * This allows EseMap objects to be used naturally from Lua
 * with dot notation and automatic garbage collection.
 *
 * @param engine EseLuaEngine pointer where the EseMap type will be registered
 */
void ese_map_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a registered EseMap proxy table back onto the Lua stack.
 *
 * @param map Pointer to the EseMap object
 */
void ese_map_lua_push(EseMap *map);

/**
 * @brief Extracts a EseMap pointer from a Lua proxy table with type safety.
 *
 * @details
 * Retrieves the C EseMap pointer from the "__ptr" field of a Lua
 * table that was created by ese_map_lua_push(). Performs type checking
 * to ensure the object is a valid EseMap proxy table with the correct
 * metatable and userdata pointer.
 *
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseMap object
 * @return Pointer to the EseMap object, or NULL if extraction fails
 *
 * @warning Returns NULL for invalid objects — always check return value before use.
 */
EseMap *ese_map_lua_get(lua_State *L, int idx);

/**
 * @brief Increments the reference count for a EseMap object.
 *
 * @details
 * When a EseMap is referenced from C code, this function should be called
 * to prevent it from being garbage collected by Lua. Each call to ese_map_ref
 * should be matched with a call to ese_map_unref.
 *
 * @param map Pointer to the EseMap object
 */
void ese_map_ref(EseMap *map);

/**
 * @brief Decrements the reference count for a EseMap object.
 *
 * @details
 * When a EseMap is no longer referenced from C code, this function should
 * be called to allow it to be garbage collected by Lua when appropriate.
 *
 * @param map Pointer to the EseMap object
 */
void ese_map_unref(EseMap *map);

/* ----------------- C API ----------------- */

/**
 * @brief Creates a new EseMap object with specified dimensions.
 *
 * @details
 * Allocates memory for a new EseMap and initializes its cells.
 * Each cell is created with `mapcell_create(engine, false)` and
 * registered with Lua as C-owned.
 *
 * If `c_only` is false, the map itself is also registered with Lua
 * and wrapped in a proxy table. If true, the map exists only in C.
 *
 * @param engine Pointer to a EseLuaEngine
 * @param width Map width in cells
 * @param height Map height in cells
 * @param type Map coordinate type
 * @param c_only True if this object won't be accessible in Lua
 * @return Pointer to newly created EseMap object
 *
 * @warning The returned EseMap must be freed with ese_map_destroy()
 *          to prevent memory leaks.
 */
EseMap *ese_map_create(EseLuaEngine *engine, uint32_t width, uint32_t height,
                   EseMapType type, bool c_only);

/**
 * @brief Destroys a EseMap object and frees its memory.
 *
 * @details
 * Frees the memory allocated by ese_map_create(), including all cells,
 * metadata strings, and Lua registry references.
 *
 * @param map Pointer to the EseMap object to destroy
 */
void ese_map_destroy(EseMap *map);

/* ----------------- Map Operations ----------------- */

/**
 * @brief Gets a map cell at the specified coordinates.
 *
 * @param map Pointer to the EseMap object
 * @param x X coordinate
 * @param y Y coordinate
 * @return Pointer to the EseMapCell, or NULL if coordinates are out of bounds
 */
EseMapCell *ese_map_get_cell(const EseMap *map, uint32_t x, uint32_t y);

/**
 * @brief Sets the map title.
 *
 * @param map Pointer to the EseMap object
 * @param title New title string (will be copied)
 * @return true if successful, false if memory allocation fails
 */
bool ese_map_set_title(EseMap *map, const char *title);

/**
 * @brief Sets the map author.
 *
 * @param map Pointer to the EseMap object
 * @param author New author string (will be copied)
 * @return true if successful, false if memory allocation fails
 */
bool ese_map_set_author(EseMap *map, const char *author);

/**
 * @brief Sets the map version.
 *
 * @param map Pointer to the EseMap object
 * @param version New version number
 */
void ese_map_set_version(EseMap *map, int version);

/**
 * @brief Sets the map tileset.
 *
 * @param map Pointer to the EseMap object
 * @param tileset Pointer to the EseTileSet to associate
 */
void ese_map_set_tileset(EseMap *map, EseTileSet *tileset);

/**
 * @brief Resizes the map to new dimensions.
 *
 * @details
 * Allocates a new cell array and copies existing cells that fit
 * within the new dimensions. Cells outside the new bounds are destroyed.
 * New cells are created with `mapcell_create(engine, false)`.
 *
 * @param map Pointer to the EseMap object
 * @param new_width New width in cells
 * @param new_height New height in cells
 * @return true if successful, false if memory allocation fails
 *
 * @warning This will destroy existing cells that are outside the new bounds.
 */
bool ese_map_resize(EseMap *map, uint32_t new_width, uint32_t new_height);

/**
 * @brief Gets the width of the map.
 *
 * @param map Pointer to the EseMap object
 * @return Width of the map
 */
int ese_map_get_width(EseMap *map);

/**
 * @brief Gets the height of the map.
 *
 * @param map Pointer to the EseMap object
 * @return Height of the map
 */
int ese_map_get_height(EseMap *map);

/**
 * @brief Gets the type of the map.
 *
 * @param map Pointer to the EseMap object
 * @return Type of the map
 */
EseMapType ese_map_get_type(EseMap *map);

/**
 * @brief Gets the tileset of the map.
 *
 * @param map Pointer to the EseMap object
 * @return Tileset of the map
 */
EseTileSet *ese_map_get_tileset(EseMap *map);

/**
 * @brief Gets the number of layers in the map.
 *
 * @param map Pointer to the EseMap object
 * @return Number of layers in the map
 */
size_t ese_map_get_layer_count(EseMap *map);

/**
 * @brief Adds a watcher callback to be notified when map properties change.
 *
 * @param map Pointer to the EseMap object to watch
 * @param callback Function to call when properties change
 * @param userdata User-provided data to pass to the callback
 * @return true if watcher was added successfully, false otherwise
 */
bool ese_map_add_watcher(EseMap *map, EseMapWatcherCallback callback, void *userdata);

/**
 * @brief Removes a previously registered watcher callback.
 *
 * @param map Pointer to the EseMap object
 * @param callback Function to remove
 * @param userdata User data that was used when registering
 * @return true if watcher was removed, false if not found
 */
bool ese_map_remove_watcher(EseMap *map, EseMapWatcherCallback callback, void *userdata);

/* ----------------- Map Type Conversion ----------------- */

/**
 * @brief Converts map type enum to string representation.
 *
 * @param type EseMapType value
 * @return String representation of the map type
 */
const char *ese_map_type_to_string(EseMapType type);

/**
 * @brief Converts string to map type enum.
 *
 * @param type_str String representation of map type
 * @return EseMapType value, or MAP_TYPE_GRID if string is invalid
 */
EseMapType ese_map_type_from_string(const char *type_str);

#endif // ESE_MAP_H