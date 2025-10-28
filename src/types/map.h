#ifndef ESE_MAP_H
#define ESE_MAP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define MAP_PROXY_META "MapProxyMeta"
#define MAP_META "MapMeta"

/* --- Forward declarations
 * --------------------------------------------------------------------- */

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
  MAP_TYPE_GRID = 0,     /** Standard grid/square tiles */
  MAP_TYPE_HEX_POINT_UP, /** Hexagonal tiles with point facing up */
  MAP_TYPE_HEX_FLAT_UP,  /** Hexagonal tiles with flat side facing up */
  MAP_TYPE_ISO           /** Isometric tiles */
} EseMapType;

/* --- Lua API
 * ----------------------------------------------------------------------------------
 */

/**
 * @brief Initializes the EseMap userdata type in the Lua state.
 *
 * @details
 * Creates and registers the "MapProxyMeta" metatable with __index, __newindex,
 * __gc, and __tostring metamethods. Also ensures a global `Map` table exists
 * and assigns `Map.new` to the constructor. This enables natural usage from
 * Lua with dot notation and automatic garbage collection.
 *
 * @param engine EseLuaEngine pointer where the EseMap type will be registered
 */
void ese_map_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes this map's userdata onto its Lua state's stack.
 *
 * @details
 * If the map has not yet been referenced from Lua, a userdata is created and
 * assigned the `MapProxyMeta` metatable; otherwise the existing registry ref
 * is pushed.
 *
 * @param map Pointer to the EseMap object
 */
void ese_map_lua_push(EseMap *map);

/**
 * @brief Extracts an EseMap pointer from a Lua userdata with type safety.
 *
 * @details
 * Validates that the value at `idx` is a userdata with the `MapProxyMeta`
 * metatable and returns the embedded EseMap pointer.
 *
 * @param L Lua state pointer
 * @param idx Stack index of the Lua object
 * @return Pointer to the EseMap object, or NULL if validation fails
 *
 * @warning Returns NULL for invalid objects — always check before use.
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

/* --- C API
 * ------------------------------------------------------------------------------------
 */

/**
 * @brief Creates a new EseMap object with specified dimensions.
 *
 * @details
 * Allocates and initializes an EseMap, sets its engine/state, and allocates a
 * 2D array of cells. Newly created cells are initialized and hooked so that the
 * map is notified when a cell changes. This function does not register the map
 * with the Lua registry.
 *
 * Note: The `c_only` flag is currently ignored.
 *
 * @param engine Pointer to a EseLuaEngine
 * @param width Map width in cells
 * @param height Map height in cells
 * @param type Map coordinate type
 * @param c_only Present for compatibility; currently unused
 * @return Pointer to newly created EseMap object
 *
 * @warning The returned EseMap must be freed with ese_map_destroy().
 */
EseMap *ese_map_create(EseLuaEngine *engine, uint32_t width, uint32_t height,
                       EseMapType type, bool c_only);

/**
 * @brief Destroys an EseMap object and frees its memory.
 *
 * @details
 * If the map is still referenced from Lua, this decrements the reference count
 * and defers destruction. When no Lua references remain, all cells are
 * destroyed, internal arrays and metadata strings are freed, any attached
 * tileset is destroyed, and the map itself is freed.
 *
 * @param map Pointer to the EseMap object to destroy
 */
void ese_map_destroy(EseMap *map);

/* --- Map Operations
 * ---------------------------------------------------------------------------
 */

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
 * Allocates a new cell grid sized to the requested dimensions. Cells that fall
 * within both the old and new bounds are deep-copied into the new grid; other
 * old cells are destroyed. Newly uncovered positions are populated with newly
 * created cells. Watchers are reattached for copied/new cells. On allocation
 * failure, the map is restored to its previous state and false is returned.
 *
 * @param map Pointer to the EseMap object
 * @param new_width New width in cells
 * @param new_height New height in cells
 * @return true if successful, false if allocation fails or inputs are invalid
 */
bool ese_map_resize(EseMap *map, uint32_t new_width, uint32_t new_height);

/**
 * @brief Gets the width of the map.
 *
 * @param map Pointer to the EseMap object
 * @return Width of the map
 */
size_t ese_map_get_width(EseMap *map);

/**
 * @brief Gets the height of the map.
 *
 * @param map Pointer to the EseMap object
 * @return Height of the map
 */
size_t ese_map_get_height(EseMap *map);

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
 * @details
 * Returns the maximum layer count across all cells. The value is cached and
 * recomputed when the map is marked dirty by internal changes.
 *
 * @param map Pointer to the EseMap object
 * @return Maximum number of layers across all cells
 */
size_t ese_map_get_layer_count(EseMap *map);

/**
 * @brief Gets the Lua state associated with this map.
 *
 * @param map Pointer to the EseMap object
 * @return Pointer to the Lua state, or NULL if none
 */
lua_State *ese_map_get_state(const EseMap *map);

/**
 * @brief Gets the Lua registry reference for this map.
 *
 * @param map Pointer to the EseMap object
 * @return The Lua registry reference value
 */
int ese_map_get_lua_ref(const EseMap *map);

/**
 * @brief Gets the Lua reference count for this map.
 *
 * @param map Pointer to the EseMap object
 * @return The current reference count
 */
int ese_map_get_lua_ref_count(const EseMap *map);

/**
 * @brief Gets the engine associated with this map.
 *
 * @param map Pointer to the EseMap object
 * @return Pointer to the EseLuaEngine, or NULL if none
 */
EseLuaEngine *ese_map_get_engine(const EseMap *map);

/**
 * @brief Gets the title of the map.
 *
 * @param map Pointer to the EseMap object
 * @return The title string, or NULL if none
 */
const char *ese_map_get_title(const EseMap *map);

/**
 * @brief Gets the author of the map.
 *
 * @param map Pointer to the EseMap object
 * @return The author string, or NULL if none
 */
const char *ese_map_get_author(const EseMap *map);

/**
 * @brief Gets the version of the map.
 *
 * @param map Pointer to the EseMap object
 * @return The version number
 */
int ese_map_get_version(const EseMap *map);

/**
 * @brief Sets the type of the map.
 *
 * @param map Pointer to the EseMap object
 * @param type The new map type
 */
void ese_map_set_type(EseMap *map, EseMapType type);

/**
 * @brief Sets the engine associated with this map.
 *
 * @param map Pointer to the EseMap object
 * @param engine Pointer to the EseLuaEngine
 */
void ese_map_set_engine(EseMap *map, EseLuaEngine *engine);

/**
 * @brief Sets the Lua state associated with this map.
 *
 * @param map Pointer to the EseMap object
 * @param state Pointer to the Lua state
 */
void ese_map_set_state(EseMap *map, lua_State *state);

/**
 * @brief Internal function to create a new map (used by Lua constructor).
 *
 * @param width Width of the map
 * @param height Height of the map
 * @param type Type of the map
 * @return Pointer to the new EseMap object
 */
EseMap *_ese_map_make(uint32_t width, uint32_t height, EseMapType type);

/**
 * @brief Internal function to allocate the cells array (used by Lua
 * constructor).
 *
 * @param map Pointer to the EseMap object
 * @return true if successful, false otherwise
 */
bool _allocate_cells_array(EseMap *map);

/**
 * @brief Internal function to notify watchers (used by Lua functions).
 *
 * @param map Pointer to the EseMap object
 */
void _ese_map_notify_watchers(EseMap *map);

/**
 * @brief Adds a watcher callback notified when the map or its cells change.
 *
 * @param map Pointer to the EseMap object to watch
 * @param callback Function to call on change
 * @param userdata User-provided data to pass to the callback
 * @return true if watcher was added successfully, false otherwise
 */
bool ese_map_add_watcher(EseMap *map, EseMapWatcherCallback callback,
                         void *userdata);

/**
 * @brief Removes a previously registered watcher callback.
 *
 * @param map Pointer to the EseMap object
 * @param callback Function to remove
 * @param userdata User data that was used when registering
 * @return true if watcher was removed, false if not found
 */
bool ese_map_remove_watcher(EseMap *map, EseMapWatcherCallback callback,
                            void *userdata);

/* --- Map Type Conversion
 * ---------------------------------------------------------------------- */

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