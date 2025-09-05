#include "collision_index.h"
#include "utility/log.h"
#include "utility/array.h"
#include "utility/double_linked_list.h"
#include "types/rect.h"
#include "entity/entity_private.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component_collider.h"
#include "platform/time.h"
#include <math.h>
#include <limits.h>
#include <stdio.h>

#define COLLISION_INDEX_DEFAULT_CELL_SIZE 32.0f
#define COLLISION_INDEX_AUTO_TUNE_THRESHOLD 10
#define COLLISION_INDEX_DBVH_THRESHOLD 15
#define COLLISION_INDEX_AUTO_TUNE_COOLDOWN_SECONDS 5.0

/**
 * @brief Structure for the collision index using a spatial hash grid.
 */
struct EseCollisionIndex {
    float cell_size;            /**< Size of each grid cell in world units */
    EseHashMap *bins;           /**< Hashmap<EseCollisionIndexKey, EseDList* (of EseEntity*)> */
    EseHashMap *dbvh_regions;   /**< Hashmap<EseCollisionIndexKey, DBVHNode*> for DBVH regions */
    EseArray *collision_pairs;  /**< Array of collision pairs (owned by this index) */
    double last_auto_tune_time; /**< Last time auto-tuning was performed (in seconds) */
};

// Forward declarations for internal functions
static void _free_collision_pair(void *ptr);
static void _dbvh_query_pairs_recursive(DBVHNode *root, EseArray *pairs);
static int _dbvh_get_height(DBVHNode *node);
static int _dbvh_get_balance(DBVHNode *node);
static DBVHNode *_dbvh_rotate_right(DBVHNode *y);
static DBVHNode *_dbvh_rotate_left(DBVHNode *x);
static void _dbvh_update_bounds(DBVHNode *node);
static void _dbvh_collect_entities(DBVHNode *root, EseArray *entities);
static void _collision_index_convert_cell_to_dbvh(EseCollisionIndex *index, int center_x, int center_y);

// Helper function to free collision pairs
static void _free_collision_pair(void *ptr) {
    if (ptr) {
        memory_manager.free(ptr);
    }
}

// Internal helper to compute cell key (simple hash combining x and y)
EseCollisionIndexKey _collision_index_compute_key(int x, int y) {
    // Convert to unsigned by adding a large offset to handle negative coordinates
    // Use 0x80000000 (2^31) as offset to ensure all values become positive
    uint64_t key = ((uint64_t)((uint32_t)(x + 0x80000000)) << 32) | (uint64_t)((uint32_t)(y + 0x80000000));
    return key;
}

// Internal helper to convert EseCollisionIndexKey to string for hashmap
char* _collision_index_key_to_string(EseCollisionIndexKey key) {
    static char key_str[64];
    snprintf(key_str, sizeof(key_str), "%llu", (unsigned long long)key);
    return key_str;
}

// Internal helper to calculate average bin count
float _collision_index_calculate_average_bin_count(EseCollisionIndex *index) {
    size_t total_entities = 0;
    size_t non_empty_bins = 0;
    
    EseHashMapIter *iter = hashmap_iter_create(index->bins);
    const char *key;
    void *value;
    while (hashmap_iter_next(iter, &key, &value)) {
        EseDoubleLinkedList *list = (EseDoubleLinkedList *)value;
        size_t bin_size = dlist_size(list);
        if (bin_size > 0) {
            total_entities += bin_size;
            non_empty_bins++;
        }
    }
    hashmap_iter_free(iter);
    
    return non_empty_bins > 0 ? (float)total_entities / (float)non_empty_bins : 0.0f;
}

// ========================================
// DBVH IMPLEMENTATION
// ========================================

DBVHNode *dbvh_node_create(EseEntity *entity) {
    DBVHNode *node = memory_manager.malloc(sizeof(DBVHNode), MMTAG_ENTITY);
    if (!node) return NULL;
    
    node->entity = entity;
    node->left = NULL;
    node->right = NULL;
    node->height = 1;
    
    if (entity && entity->collision_world_bounds) {
        EseRect *bounds = entity->collision_world_bounds;
        node->bounds_x = rect_get_x(bounds);
        node->bounds_y = rect_get_y(bounds);
        node->bounds_width = rect_get_width(bounds);
        node->bounds_height = rect_get_height(bounds);
    } else {
        // Initialize with empty bounds for internal nodes
        node->bounds_x = 0;
        node->bounds_y = 0;
        node->bounds_width = 0;
        node->bounds_height = 0;
    }
    
    return node;
}

void dbvh_node_destroy(DBVHNode *node) {
    if (!node) return;
    
    dbvh_node_destroy(node->left);
    dbvh_node_destroy(node->right);
    memory_manager.free(node);
}

static int _dbvh_get_height(DBVHNode *node) {
    return node ? node->height : 0;
}

static int _dbvh_get_balance(DBVHNode *node) {
    return node ? _dbvh_get_height(node->left) - _dbvh_get_height(node->right) : 0;
}

static void _dbvh_update_bounds(DBVHNode *node) {
    if (!node) return;
    
    if (node->entity && node->entity->collision_world_bounds) {
        EseRect *bounds = node->entity->collision_world_bounds;
        node->bounds_x = rect_get_x(bounds);
        node->bounds_y = rect_get_y(bounds);
        node->bounds_width = rect_get_width(bounds);
        node->bounds_height = rect_get_height(bounds);
    } else if (node->left && node->right) {
        // Internal node - union of children bounds
        float min_x = fminf(node->left->bounds_x, node->right->bounds_x);
        float min_y = fminf(node->left->bounds_y, node->right->bounds_y);
        float max_x = fmaxf(node->left->bounds_x + node->left->bounds_width, 
                           node->right->bounds_x + node->right->bounds_width);
        float max_y = fmaxf(node->left->bounds_y + node->left->bounds_height, 
                           node->right->bounds_y + node->right->bounds_height);
        
        node->bounds_x = min_x;
        node->bounds_y = min_y;
        node->bounds_width = max_x - min_x;
        node->bounds_height = max_y - min_y;
    } else if (node->left) {
        node->bounds_x = node->left->bounds_x;
        node->bounds_y = node->left->bounds_y;
        node->bounds_width = node->left->bounds_width;
        node->bounds_height = node->left->bounds_height;
    } else if (node->right) {
        node->bounds_x = node->right->bounds_x;
        node->bounds_y = node->right->bounds_y;
        node->bounds_width = node->right->bounds_width;
        node->bounds_height = node->right->bounds_height;
    }
}

static DBVHNode *_dbvh_rotate_right(DBVHNode *y) {
    DBVHNode *x = y->left;
    DBVHNode *T2 = x->right;
    
    x->right = y;
    y->left = T2;
    
    y->height = 1 + fmax(_dbvh_get_height(y->left), _dbvh_get_height(y->right));
    x->height = 1 + fmax(_dbvh_get_height(x->left), _dbvh_get_height(x->right));
    
    _dbvh_update_bounds(y);
    _dbvh_update_bounds(x);
    
    return x;
}

static DBVHNode *_dbvh_rotate_left(DBVHNode *x) {
    DBVHNode *y = x->right;
    DBVHNode *T2 = y->left;
    
    y->left = x;
    x->right = T2;
    
    x->height = 1 + fmax(_dbvh_get_height(x->left), _dbvh_get_height(x->right));
    y->height = 1 + fmax(_dbvh_get_height(y->left), _dbvh_get_height(y->right));
    
    _dbvh_update_bounds(x);
    _dbvh_update_bounds(y);
    
    return y;
}

DBVHNode *dbvh_insert(DBVHNode *root, EseEntity *entity) {
    if (!entity || !entity->collision_world_bounds) return root;
    
    // Create leaf node
    if (!root) {
        return dbvh_node_create(entity);
    }
    
    // Insert as leaf (simple approach for now)
    DBVHNode *new_node = dbvh_node_create(entity);
    if (!new_node) return root;
    
    // For simplicity, just add as right child of current root
    // In a more sophisticated implementation, we'd choose insertion point based on bounds
    if (!root->right) {
        root->right = new_node;
    } else {
        // Create new internal node
        DBVHNode *internal = dbvh_node_create(NULL);
        if (!internal) {
            dbvh_node_destroy(new_node);
            return root;
        }
        internal->left = root;
        internal->right = new_node;
        root = internal;
    }
    
    // Update height and bounds
    root->height = 1 + fmax(_dbvh_get_height(root->left), _dbvh_get_height(root->right));
    _dbvh_update_bounds(root);
    
    // Simple balancing (not perfect, but functional)
    int balance = _dbvh_get_balance(root);
    
    if (balance > 1) {
        if (_dbvh_get_balance(root->left) < 0) {
            root->left = _dbvh_rotate_left(root->left);
        }
        return _dbvh_rotate_right(root);
    }
    
    if (balance < -1) {
        if (_dbvh_get_balance(root->right) > 0) {
            root->right = _dbvh_rotate_right(root->right);
        }
        return _dbvh_rotate_left(root);
    }
    
    return root;
}

static void _dbvh_query_pairs_recursive(DBVHNode *root, EseArray *pairs) {
    if (!root) return;
    
    // If this is a leaf node with an entity, we need to check it against all other entities
    if (root->entity) {
        // For now, we'll do a simple O(n²) check within the DBVH
        // In a more sophisticated implementation, we'd use the tree structure more efficiently
        _dbvh_query_pairs_recursive(root->left, pairs);
        _dbvh_query_pairs_recursive(root->right, pairs);
        return;
    }
    
    // Internal node - check if bounds overlap (simplified)
    if (root->left && root->right) {
        _dbvh_query_pairs_recursive(root->left, pairs);
        _dbvh_query_pairs_recursive(root->right, pairs);
        
        // Check pairs between left and right subtrees
        if (root->left->entity && root->right->entity) {
            int state = entity_check_collision_state(root->left->entity, root->right->entity);
            if (state != 0) {
                CollisionPair *pair = (CollisionPair *)memory_manager.malloc(sizeof(CollisionPair), MMTAG_ENGINE);
                pair->entity_a = root->left->entity;
                pair->entity_b = root->right->entity;
                pair->state = state;
                if (!array_push(pairs, pair)) {
                    log_warn("COLLISION_INDEX", "Failed to add collision pair to array");
                    memory_manager.free(pair);
                }
            }
        }
    } else {
        _dbvh_query_pairs_recursive(root->left, pairs);
        _dbvh_query_pairs_recursive(root->right, pairs);
    }
}

void dbvh_query_pairs(DBVHNode *root, EseArray *pairs) {
    if (!root) return;
    
    // Collect all entities first
    EseArray *entities = array_create(64, NULL);
    _dbvh_collect_entities(root, entities);
    
    // Check all pairs (O(n²) for now)
    for (size_t i = 0; i < array_size(entities); i++) {
        EseEntity *a = (EseEntity *)array_get(entities, i);
        for (size_t j = i + 1; j < array_size(entities); j++) {
            EseEntity *b = (EseEntity *)array_get(entities, j);
            int state = entity_check_collision_state(a, b);
            if (state != 0) {
                CollisionPair *pair = (CollisionPair *)memory_manager.malloc(sizeof(CollisionPair), MMTAG_ENGINE);
                pair->entity_a = a;
                pair->entity_b = b;
                pair->state = state;
                if (!array_push(pairs, pair)) {
                    log_warn("COLLISION_INDEX", "Failed to add collision pair to array");
                    memory_manager.free(pair);
                }
            }
        }
    }
    
    array_destroy(entities);
}

// Helper to collect all entities from DBVH tree
static void _dbvh_collect_entities(DBVHNode *root, EseArray *entities) {
    if (!root) return;
    
    if (root->entity) {
        array_push(entities, root->entity);
    } else {
        _dbvh_collect_entities(root->left, entities);
        _dbvh_collect_entities(root->right, entities);
    }
}

// Helper function to convert a cell and its 8 neighbors to DBVH
static void _collision_index_convert_cell_to_dbvh(EseCollisionIndex *index, int center_x, int center_y) {
    EseCollisionIndexKey center_key = _collision_index_compute_key(center_x, center_y);
    char *center_key_str = _collision_index_key_to_string(center_key);
    
    // Check if already converted
    if (hashmap_get(index->dbvh_regions, center_key_str)) {
        return;
    }
    
    // Collect all entities from center cell + 8 neighbors
    EseArray *entities = array_create(64, NULL);
    
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            int cell_x = center_x + dx;
            int cell_y = center_y + dy;
            EseCollisionIndexKey key = _collision_index_compute_key(cell_x, cell_y);
            char *key_str = _collision_index_key_to_string(key);
            EseDoubleLinkedList *list = (EseDoubleLinkedList *)hashmap_get(index->bins, key_str);
            
            if (list) {
                EseDListIter *iter = dlist_iter_create(list);
                void *entity_value;
                while (dlist_iter_next(iter, &entity_value)) {
                    EseEntity *entity = (EseEntity *)entity_value;
                    array_push(entities, entity);
                }
                dlist_iter_free(iter);
            }
        }
    }
    
    if (array_size(entities) == 0) {
        array_destroy(entities);
        return;
    }
    
    // Build DBVH from collected entities
    DBVHNode *root = NULL;
    for (size_t i = 0; i < array_size(entities); i++) {
        EseEntity *entity = (EseEntity *)array_get(entities, i);
        root = dbvh_insert(root, entity);
    }
    
    if (root) {
        // Store DBVH in regions hashmap
        hashmap_set(index->dbvh_regions, center_key_str, root);
        
        // Clear the center cell (neighbors remain in grid for other cells' neighbor checks)
        EseDoubleLinkedList *center_list = (EseDoubleLinkedList *)hashmap_get(index->bins, center_key_str);
        if (center_list) {
            // Replace with a new empty list
            dlist_free(center_list);
            EseDoubleLinkedList *new_list = dlist_create(NULL);
            hashmap_set(index->bins, center_key_str, new_list);
        }
        
        log_debug("COLLISION_INDEX", "Converted cell (%d,%d) to DBVH with %zu entities", center_x, center_y, array_size(entities));
    }
    
    array_destroy(entities);
}


EseCollisionIndex *collision_index_create(void) {
    EseCollisionIndex *index = memory_manager.malloc(sizeof(EseCollisionIndex), MMTAG_ENTITY);
    index->cell_size = COLLISION_INDEX_DEFAULT_CELL_SIZE;
    index->bins = hashmap_create((EseHashMapFreeFn)dlist_free);
    index->dbvh_regions = hashmap_create((EseHashMapFreeFn)dbvh_node_destroy);
    index->collision_pairs = array_create(128, _free_collision_pair);
    index->last_auto_tune_time = 0.0;
    return index;
}

void collision_index_destroy(EseCollisionIndex *index) {
    log_assert("COLLISION_INDEX", index, "destroy called with NULL index");
    
    hashmap_free(index->bins);
    hashmap_free(index->dbvh_regions);
    array_destroy(index->collision_pairs);
    memory_manager.free(index);
}

void collision_index_clear(EseCollisionIndex *index) {
    log_assert("COLLISION_INDEX", index, "clear called with NULL index");

    // The hashmap will automatically free all linked list objects when cleared
    hashmap_clear(index->bins);
    hashmap_clear(index->dbvh_regions);
    array_clear(index->collision_pairs);
}

void collision_index_insert(EseCollisionIndex *index, EseEntity *entity) {
    log_assert("COLLISION_INDEX", index, "insert called with NULL index");
    log_assert("COLLISION_INDEX", entity, "insert called with NULL entity");
    if (!entity->active) return;

    // Use the entity's pre-computed world bounds
    if (!entity->collision_world_bounds) return;  // No collision bounds, skip

    EseRect *bounds = entity->collision_world_bounds;

    // Compute primary cell (top-left cell the entity occupies)
    int cell_x = (int)floorf(rect_get_x(bounds) / index->cell_size);
    int cell_y = (int)floorf(rect_get_y(bounds) / index->cell_size);

    // Insert entity into only its primary cell
    EseCollisionIndexKey key = _collision_index_compute_key(cell_x, cell_y);
    char *key_str = _collision_index_key_to_string(key);
    EseDoubleLinkedList *list = (EseDoubleLinkedList *)hashmap_get(index->bins, key_str);
    if (!list) {
        list = dlist_create(NULL);  // No free function needed for entity pointers
        hashmap_set(index->bins, key_str, list);
    }
    dlist_append(list, entity);
    
    // Check if auto-tuning should be triggered
    double current_time = time_now_seconds();
    if (current_time - index->last_auto_tune_time >= COLLISION_INDEX_AUTO_TUNE_COOLDOWN_SECONDS) {
        float avg_bin_count = _collision_index_calculate_average_bin_count(index);
        if (avg_bin_count > COLLISION_INDEX_AUTO_TUNE_THRESHOLD) {
            collision_index_auto_tune(index);
            index->last_auto_tune_time = current_time;
        }
    }
}

EseDoubleLinkedList *collision_index_get_cell(EseCollisionIndex *index, int cell_x, int cell_y) {
    log_assert("COLLISION_INDEX", index, "get_cell called with NULL index");

    EseCollisionIndexKey key = _collision_index_compute_key(cell_x, cell_y);
    char *key_str = _collision_index_key_to_string(key);
    return (EseDoubleLinkedList *)hashmap_get(index->bins, key_str);
}

void collision_index_get_neighbors(EseCollisionIndex *index, int cell_x, int cell_y, EseDoubleLinkedList **neighbors, size_t *neighbor_count) {
    log_assert("COLLISION_INDEX", index, "get_neighbors called with NULL index");
    log_assert("COLLISION_INDEX", neighbors, "get_neighbors called with NULL neighbors");
    log_assert("COLLISION_INDEX", neighbor_count, "get_neighbors called with NULL neighbor_count");

    *neighbor_count = 0;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;  // Skip self
            EseDoubleLinkedList *list = collision_index_get_cell(index, cell_x + dx, cell_y + dy);
            if (list && dlist_size(list) > 0) {
                neighbors[*neighbor_count] = list;
                (*neighbor_count)++;
            }
        }
    }
}

void collision_index_auto_tune(EseCollisionIndex *index) {
    log_assert("COLLISION_INDEX", index, "auto_tune called with NULL index");

    float total_size = 0.0f;
    size_t sample_count = 0;

    EseHashMapIter *iter = hashmap_iter_create(index->bins);
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
        index->cell_size = COLLISION_INDEX_DEFAULT_CELL_SIZE;
        return;
    }

    float avg_size = total_size / sample_count;
    float new_size = fmaxf(32.0f, avg_size * 2.0f);  // 2x average, min 32
    index->cell_size = new_size;
    log_debug("COLLISION_INDEX", "Auto-tuned cell_size to %f based on %zu samples (avg diag: %f)", new_size, sample_count, avg_size);
}

EseArray *collision_index_get_pairs(EseCollisionIndex *index) {
    log_assert("COLLISION_INDEX", index, "get_pairs called with NULL index");
    
    // Clear the existing pairs array for reuse
    array_clear(index->collision_pairs);
    
    // PHASE 1: Convert cells to DBVH if they exceed threshold
    EseHashMapIter *bin_iter = hashmap_iter_create(index->bins);
    const char *bin_key_str;
    void *bin_value;
    while (hashmap_iter_next(bin_iter, &bin_key_str, &bin_value)) {
        EseDoubleLinkedList *cell_list = (EseDoubleLinkedList *)bin_value;
        size_t cell_size = dlist_size(cell_list);
        
        // Check if cell exceeds threshold for DBVH conversion
        if (cell_size > COLLISION_INDEX_DBVH_THRESHOLD) {
            // Extract cell coordinates
            EseCollisionIndexKey key_val;
            if (sscanf(bin_key_str, "%llu", (unsigned long long*)&key_val) == 1) {
                int cell_x = (int)((key_val >> 32) - 0x80000000);
                int cell_y = (int)((key_val & 0xFFFFFFFF) - 0x80000000);
                _collision_index_convert_cell_to_dbvh(index, cell_x, cell_y);
            }
        }
    }
    hashmap_iter_free(bin_iter);
    
    // PHASE 2: Query DBVH regions for collision pairs
    EseHashMapIter *dbvh_iter = hashmap_iter_create(index->dbvh_regions);
    const char *dbvh_key_str;
    void *dbvh_value;
    while (hashmap_iter_next(dbvh_iter, &dbvh_key_str, &dbvh_value)) {
        DBVHNode *root = (DBVHNode *)dbvh_value;
        dbvh_query_pairs(root, index->collision_pairs);
    }
    hashmap_iter_free(dbvh_iter);
    
    // PHASE 3: Process regular grid cells (excluding DBVH centers)
    bin_iter = hashmap_iter_create(index->bins);
    while (hashmap_iter_next(bin_iter, &bin_key_str, &bin_value)) {
        EseDoubleLinkedList *cell_list = (EseDoubleLinkedList *)bin_value;
        if (dlist_size(cell_list) < 2) continue;

        // Check if this cell is a DBVH center (skip if so)
        EseCollisionIndexKey key_val;
        if (sscanf(bin_key_str, "%llu", (unsigned long long*)&key_val) == 1) {
            if (hashmap_get(index->dbvh_regions, bin_key_str)) {
                continue; // Skip DBVH center cells
            }
        }

        // Check within cell using nested iterators
        void *a_value;
        EseDListIter *outer = dlist_iter_create(cell_list);
        while (dlist_iter_next(outer, &a_value)) {
            EseEntity *a = (EseEntity *)a_value;

            EseDListIter *inner = dlist_iter_create_from(outer);
            void *b_value;
            while (dlist_iter_next(inner, &b_value)) {
                EseEntity *b = (EseEntity *)b_value;
                int state = entity_check_collision_state(a, b);
                if (state != 0) {
                    // Create heap-allocated collision pair and add to array
                    CollisionPair *pair = (CollisionPair *)memory_manager.malloc(sizeof(CollisionPair), MMTAG_ENGINE);
                    pair->entity_a = a;
                    pair->entity_b = b;
                    pair->state = state;
                    if (!array_push(index->collision_pairs, pair)) {
                        log_warn("COLLISION_INDEX", "Failed to add collision pair to array");
                        memory_manager.free(pair);
                    }
                }
            }
            dlist_iter_free(inner);
        }
        dlist_iter_free(outer);

        // Neighbor checks
        // Extract cell_x, cell_y from bin_key_str (format is "%llu" from EseCollisionIndexKey)
        // We need to reverse the key computation to get x,y coordinates
        if (sscanf(bin_key_str, "%llu", (unsigned long long*)&key_val) == 1) {
            // Reverse the key computation: key = ((x + 0x80000000) << 32) | (y + 0x80000000)
            int cell_x = (int)((key_val >> 32) - 0x80000000);
            int cell_y = (int)((key_val & 0xFFFFFFFF) - 0x80000000);
            
            EseDoubleLinkedList *neighbors[8];
            size_t neighbor_count = 0;
            collision_index_get_neighbors(index, cell_x, cell_y, neighbors, &neighbor_count);

            for (size_t n = 0; n < neighbor_count; n++) {
                EseDoubleLinkedList *neighbor_list = neighbors[n];

                // Check pairs between current cell and neighbor cell
                EseDListIter *cell_iter = dlist_iter_create(cell_list);
                void *c_value;
                while (dlist_iter_next(cell_iter, &c_value)) {
                    EseEntity *c = (EseEntity *)c_value;
                    EseDListIter *neigh_iter = dlist_iter_create(neighbor_list);
                    void *n_value;
                    while (dlist_iter_next(neigh_iter, &n_value)) {
                        EseEntity *n = (EseEntity *)n_value;
                        int state = entity_check_collision_state(c, n);
                        if (state != 0) {
                            // Create heap-allocated collision pair and add to array
                            CollisionPair *pair = (CollisionPair *)memory_manager.malloc(sizeof(CollisionPair), MMTAG_ENGINE);
                            pair->entity_a = c;
                            pair->entity_b = n;
                            pair->state = state;
                            if (!array_push(index->collision_pairs, pair)) {
                                log_warn("COLLISION_INDEX", "Failed to add collision pair to array");
                                memory_manager.free(pair);
                            }
                        }
                    }
                    dlist_iter_free(neigh_iter);
                }
                dlist_iter_free(cell_iter);
            }
        }
    }

    hashmap_iter_free(bin_iter);
    return index->collision_pairs;
}
