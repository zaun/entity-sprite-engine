#ifndef ESE_MAP_PRIVATE_H
#define ESE_MAP_PRIVATE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "types/map.h"

/**
 * @brief Represents a complete map with metadata, tileset, and cell grid.
 *
 * @details
 * This structure contains all map data including metadata,
 * associated tileset, dimensions, and a 2D array of map cells.
 * Each cell is a pointer to a `EseMapCell` object that is
 * created with `mapcell_create` and destroyed with `mapcell_destroy`.
 */
 typedef struct EseMap {
    // Metadata
    char *title;                     /** Map title */
    char *author;                    /** Map author */
    uint32_t version;                /** Map version number */
    EseMapType type;                 /** Map coordinate type */

    // Tileset reference
    EseTileSet *tileset;             /** Associated tileset for this map */

    // Dimensions
    uint32_t width;                  /** Map width in cells */
    uint32_t height;                 /** Map height in cells */

    // Cell data
    EseMapCell ***cells;             /** 2D array of pointers to map cells [y][x] */

    size_t layer_count;              /** Number of layers in the map */
    bool layer_count_dirty;          /** Flag to track if layer count is dirty */

    // Lua integration
    lua_State *state;                /** Lua State this EseMap belongs to */
    EseLuaEngine *engine;            /** Engine reference for creating cells */
    int lua_ref;                     /** Lua registry reference to its own userdata */
    int lua_ref_count;               /** Number of times this map has been referenced in C */
    bool destroyed;                  /** Flag to track if map has been destroyed */

    // Watcher system
    EseMapWatcherCallback *watchers; /** Array of watcher callbacks */
    void **watcher_userdata;         /** Array of userdata for each watcher */
    size_t watcher_count;            /** Number of registered watchers */
    size_t watcher_capacity;         /** Capacity of the watcher arrays */
} EseMap;

/**
 * @brief Gets the number of layers in the map.
 *
 * @param map Pointer to the EseMap object
 * @return Number of layers in the map
 */
 void _ese_map_set_layer_count_dirty(EseMap *map);

#endif // ESE_MAP_PRIVATE_H