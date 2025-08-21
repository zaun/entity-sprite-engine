#ifndef ESE_MAP_H
#define ESE_MAP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseMapCell EseMapCell;
typedef struct EseTileSet EseTileSet;

/**
 * @brief Map type enumeration for different coordinate systems.
 */
typedef enum {
    MAP_TYPE_GRID = 0,          /**< Standard grid/square tiles */
    MAP_TYPE_HEX_POINT_UP,      /**< Hexagonal tiles with point facing up */
    MAP_TYPE_HEX_FLAT_UP,       /**< Hexagonal tiles with flat side facing up */
    MAP_TYPE_ISO                /**< Isometric tiles */
} EseMapType;

/**
 * @brief Represents a complete map with metadata, tileset, and cell grid.
 * 
 * @details This structure contains all map data including metadata,
 *          associated tileset, dimensions, and a 2D array of map cells.
 */
typedef struct EseMap {
    // Metadata
    char *title;                     /**< Map title */
    char *author;                    /**< Map author */
    uint32_t version;                /**< Map version number */
    EseMapType type;                 /**< Map coordinate type */
    
    // Tileset reference
    EseTileSet *tileset;             /**< Associated tileset for this map */
    
    // Dimensions
    uint32_t width;                  /**< Map width in cells */
    uint32_t height;                 /**< Map height in cells */
    
    // Cell data
    EseMapCell **cells;              /**< 2D array of map cells [y][x] */
    
    // Lua integration
    lua_State *state;                /**< Lua State this EseMap belongs to */
    int lua_ref;                     /**< Lua registry reference to its own proxy table */
} EseMap;

/**
 * @brief Initializes the EseMap userdata type in the Lua state.
 * 
 * @param engine EseLuaEngine pointer where the EseMap type will be registered
 */
void map_lua_init(EseLuaEngine *engine);

/**
 * @brief Creates a new EseMap object with specified dimensions.
 * 
 * @param engine Pointer to a EseLuaEngine
 * @param width Map width in cells
 * @param height Map height in cells
 * @param type Map coordinate type
 * @param c_only True if this object won't be accessible in LUA
 * @return Pointer to newly created EseMap object
 * 
 * @warning The returned EseMap must be freed with map_destroy()
 */
EseMap *map_create(EseLuaEngine *engine, uint32_t width, uint32_t height, EseMapType type, bool c_only);

/**
 * @brief Destroys a EseMap object and frees its memory.
 * 
 * @param map Pointer to the EseMap object to destroy
 */
void map_destroy(EseMap *map);

/**
 * @brief Extracts a EseMap pointer from a Lua userdata object.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseMap object
 * @return Pointer to the EseMap object, or NULL if extraction fails
 */
EseMap *map_lua_get(lua_State *L, int idx);

/**
 * @brief Gets a map cell at the specified coordinates.
 * 
 * @param map Pointer to the EseMap object
 * @param x X coordinate
 * @param y Y coordinate
 * @return Pointer to the EseMapCell, or NULL if coordinates are out of bounds
 */
EseMapCell *map_get_cell(const EseMap *map, uint32_t x, uint32_t y);

/**
 * @brief Sets a map cell at the specified coordinates.
 * 
 * @param map Pointer to the EseMap object
 * @param x X coordinate
 * @param y Y coordinate
 * @param cell Pointer to the EseMapCell to set
 * @return true if successful, false if coordinates are out of bounds
 */
bool map_set_cell(EseMap *map, uint32_t x, uint32_t y, EseMapCell *cell);

/**
 * @brief Sets the map title.
 * 
 * @param map Pointer to the EseMap object
 * @param title New title string (will be copied)
 * @return true if successful, false if memory allocation fails
 */
bool map_set_title(EseMap *map, const char *title);

/**
 * @brief Sets the map author.
 * 
 * @param map Pointer to the EseMap object
 * @param author New author string (will be copied)
 * @return true if successful, false if memory allocation fails
 */
bool map_set_author(EseMap *map, const char *author);

/**
 * @brief Sets the map version.
 * 
 * @param map Pointer to the EseMap object
 * @param version New version number
 */
void map_set_version(EseMap *map, int version);

/**
 * @brief Sets the map tileset.
 * 
 * @param map Pointer to the EseMap object
 * @param tileset Pointer to the EseTileSet to associate
 */
void map_set_tileset(EseMap *map, EseTileSet *tileset);

/**
 * @brief Resizes the map to new dimensions.
 * 
 * @param map Pointer to the EseMap object
 * @param new_width New width in cells
 * @param new_height New height in cells
 * @return true if successful, false if memory allocation fails
 * 
 * @warning This will destroy existing cells that are outside the new bounds
 */
bool map_resize(EseMap *map, uint32_t new_width, uint32_t new_height);

/**
 * @brief Converts map type enum to string representation.
 * 
 * @param type EseMapType value
 * @return String representation of the map type
 */
const char *map_type_to_string(EseMapType type);

/**
 * @brief Converts string to map type enum.
 * 
 * @param type_str String representation of map type
 * @return EseMapType value, or MAP_TYPE_GRID if string is invalid
 */
EseMapType map_type_from_string(const char *type_str);

#endif // ESE_MAP_H