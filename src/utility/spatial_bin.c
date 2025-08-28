#include "spatial_bin.h"
#include "utility/log.h"
#include "types/rect.h"
#include "entity/entity_private.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component_collider.h"
#include <math.h>
#include <limits.h>
#include <stdio.h>

#define SPATIAL_BIN_DEFAULT_CELL_SIZE 128.0f

// Internal helper to compute cell key (simple hash combining x and y)
EseSpatialBinKey _spatial_bin_compute_key(int x, int y) {
    // Use a simple hash: treat x,y as 32-bit ints, combine into 64-bit
    uint64_t key = ((uint64_t)(x + INT_MAX) << 32) | (uint64_t)(y + INT_MAX);
    return key;
}

// Internal helper to convert EseSpatialBinKey to string for hashmap
char* _spatial_bin_key_to_string(EseSpatialBinKey key) {
    static char key_str[64];
    snprintf(key_str, sizeof(key_str), "%llu", (unsigned long long)key);
    return key_str;
}


EseSpatialBin *spatial_bin_create(void) {
    EseSpatialBin *bin = memory_manager.malloc(sizeof(EseSpatialBin), MMTAG_ENTITY);
    bin->cell_size = SPATIAL_BIN_DEFAULT_CELL_SIZE;
    bin->bins = hashmap_create((EseHashMapFreeFn)dlist_free);
    return bin;
}

void spatial_bin_destroy(EseSpatialBin *bin) {
    log_assert("SPATIAL_BIN", bin, "destroy called with NULL bin");
    
    hashmap_free(bin->bins);
    memory_manager.free(bin);
}

void spatial_bin_clear(EseSpatialBin *bin) {
    log_assert("SPATIAL_BIN", bin, "clear called with NULL bin");

    // The hashmap will automatically free all linked list objects when cleared
    hashmap_clear(bin->bins);
}

void spatial_bin_insert(EseSpatialBin *bin, EseEntity *entity) {
    log_assert("SPATIAL_BIN", bin, "insert called with NULL bin");
    log_assert("SPATIAL_BIN", entity, "insert called with NULL entity");
    if (!entity->active) return;

    // Use the entity's pre-computed world bounds
    if (!entity->collision_world_bounds) return;  // No collision bounds, skip

    EseRect *bounds = entity->collision_world_bounds;

    // Compute min/max cell indices
    int min_x = (int)floorf(rect_get_x(bounds) / bin->cell_size);
    int min_y = (int)floorf(rect_get_y(bounds) / bin->cell_size);
    int max_x = (int)floorf((rect_get_x(bounds) + rect_get_width(bounds)) / bin->cell_size);
    int max_y = (int)floorf((rect_get_y(bounds) + rect_get_height(bounds)) / bin->cell_size);

    for (int x = min_x; x <= max_x; x++) {
        for (int y = min_y; y <= max_y; y++) {
            EseSpatialBinKey key = _spatial_bin_compute_key(x, y);
            char *key_str = _spatial_bin_key_to_string(key);
            EseDoubleLinkedList *list = (EseDoubleLinkedList *)hashmap_get(bin->bins, key_str);
            if (!list) {
                list = dlist_create(NULL);  // No free function needed for entity pointers
                hashmap_set(bin->bins, key_str, list);
            }
            dlist_append(list, entity);
        }
    }
}

EseDoubleLinkedList *spatial_bin_get_cell(EseSpatialBin *bin, int cell_x, int cell_y) {
    log_assert("SPATIAL_BIN", bin, "get_cell called with NULL bin");

    EseSpatialBinKey key = _spatial_bin_compute_key(cell_x, cell_y);
    char *key_str = _spatial_bin_key_to_string(key);
    return (EseDoubleLinkedList *)hashmap_get(bin->bins, key_str);
}

void spatial_bin_get_neighbors(EseSpatialBin *bin, int cell_x, int cell_y, EseDoubleLinkedList **neighbors, size_t *neighbor_count) {
    log_assert("SPATIAL_BIN", bin, "get_neighbors called with NULL bin");
    log_assert("SPATIAL_BIN", neighbors, "get_neighbors called with NULL neighbors");
    log_assert("SPATIAL_BIN", neighbor_count, "get_neighbors called with NULL neighbor_count");

    *neighbor_count = 0;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;  // Skip self
            EseDoubleLinkedList *list = spatial_bin_get_cell(bin, cell_x + dx, cell_y + dy);
            if (list && dlist_size(list) > 0) {
                neighbors[*neighbor_count] = list;
                (*neighbor_count)++;
            }
        }
    }
}

void spatial_bin_auto_tune(EseSpatialBin *bin) {
    log_assert("SPATIAL_BIN", bin, "auto_tune called with NULL bin");

    float total_size = 0.0f;
    size_t sample_count = 0;

    EseHashMapIter *iter = hashmap_iter_create(bin->bins);
    const char *key;
    void *value;
    while (hashmap_iter_next(iter, &key, &value)) {
        EseDoubleLinkedList *list = (EseDoubleLinkedList *)value;
        if (dlist_size(list) > 0) {
            // Sample first entity in bin using iterator
            EseDListIter *entity_iter = dlist_iter_create(list);
            void *entity_value;
            if (dlist_iter_next(entity_iter, &entity_value)) {
                EseEntity *entity = (EseEntity *)entity_value;
                if (entity->collision_world_bounds) {
                    EseRect *bounds = entity->collision_world_bounds;
                    float diag = sqrtf(rect_get_width(bounds) * rect_get_width(bounds) + rect_get_height(bounds) * rect_get_height(bounds));
                    total_size += diag;
                    sample_count++;
                }
            }
            dlist_iter_free(entity_iter);
        }
    }
    hashmap_iter_free(iter);

    if (sample_count == 0) {
        bin->cell_size = SPATIAL_BIN_DEFAULT_CELL_SIZE;
        return;
    }

    float avg_size = total_size / sample_count;
    float new_size = fmaxf(32.0f, avg_size * 2.0f);  // 2x average, min 32
    bin->cell_size = new_size;
    log_debug("SPATIAL_BIN", "Auto-tuned cell_size to %f based on %zu samples (avg diag: %f)", new_size, sample_count, avg_size);
}
