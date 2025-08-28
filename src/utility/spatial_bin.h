#ifndef ESE_SPATIAL_BIN_H
#define ESE_SPATIAL_BIN_H

#include "types/rect.h"
#include "types/point.h"
#include "entity/entity.h"
#include "utility/hashmap.h"
#include "utility/double_linked_list.h"
#include "core/memory_manager.h"

// Hashed key for grid cells (combines x and y into a uint64_t)
typedef uint64_t EseSpatialBinKey;

// Structure for the spatial bin (hash grid)
typedef struct EseSpatialBin {
    float cell_size;          // Size of each grid cell in world units
    EseHashMap *bins;         // Hashmap<EseSpatialBinKey, EseDList* (of EseEntity*)>
} EseSpatialBin;

// Create a new spatial bin with default cell size
EseSpatialBin *spatial_bin_create(void);

// Destroy the spatial bin and free all resources
void spatial_bin_destroy(EseSpatialBin *bin);

// Clear all bins (remove entities but keep structure)
void spatial_bin_clear(EseSpatialBin *bin);

// Insert an entity into the spatial bin based on its position and collider rects
void spatial_bin_insert(EseSpatialBin *bin, EseEntity *entity);

// Get the list of entities in a specific cell (returns NULL if empty)
EseDoubleLinkedList *spatial_bin_get_cell(EseSpatialBin *bin, int cell_x, int cell_y);

// Helper to get neighboring cells' entities (user must handle duplicates/logic)
void spatial_bin_get_neighbors(EseSpatialBin *bin, int cell_x, int cell_y, EseDoubleLinkedList **neighbors, size_t *neighbor_count);

// Auto-tune the cell size based on current entities' sizes
void spatial_bin_auto_tune(EseSpatialBin *bin);

// Internal helper to compute cell key
EseSpatialBinKey _spatial_bin_compute_key(int x, int y);

// Internal helper to compute entity's bounding rect (aggregated from colliders)
EseRect *_spatial_bin_compute_bounds(EseEntity *entity);

#endif // ESE_SPATIAL_BIN_H
