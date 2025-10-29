#ifndef ESE_SPATIAL_INDEX_H
#define ESE_SPATIAL_INDEX_H

#include "utility/array.h"
#include <stddef.h>

// Forward declarations to avoid pulling heavy headers into the public API
typedef struct EseEntity EseEntity;

// Opaque spatial index type
typedef struct SpatialIndex SpatialIndex;

// Canonical, unordered pair of entities produced by the spatial phase
typedef struct SpatialPair {
    EseEntity *a;
    EseEntity *b;
} SpatialPair;

// Lifecycle
SpatialIndex *spatial_index_create(void);
void spatial_index_destroy(SpatialIndex *index);
void spatial_index_clear(SpatialIndex *index);

// Insert entities before pair generation
void spatial_index_insert(SpatialIndex *index, EseEntity *entity);

// Optional tuning hook
void spatial_index_auto_tune(SpatialIndex *index);

// Generate canonical, deduplicated unordered pairs (SpatialPair*)
// The returned EseArray is owned by the index and must not be freed by the
// caller.
EseArray *spatial_index_get_pairs(SpatialIndex *index);

#endif // ESE_SPATIAL_INDEX_H
