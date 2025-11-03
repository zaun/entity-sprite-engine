/**
 * SPATIAL INDEX IMPLEMENTATION
 * ============================
 *
 * This file implements a hybrid spatial indexing system that combines a uniform
 * grid with Dynamic Bounding Volume Hierarchies (DBVH) for efficient collision
 * detection. The system automatically adapts to entity density and distribution
 * patterns.
 *
 * ARCHITECTURE OVERVIEW
 * =====================
 *
 * The spatial index uses a two-tier approach:
 * 1. UNIFORM GRID: Fast insertion and basic spatial partitioning
 * 2. DBVH REGIONS: Hierarchical structures for dense entity clusters
 *
 * Key Components:
 * - SpatialIndex: Main container with grid cells and DBVH regions
 * - Grid Cells: Fixed-size spatial bins storing entity lists
 * - DBVH Nodes: Self-balancing binary trees for dense regions
 * - Auto-tuning: Dynamic cell size adjustment based on entity distribution
 *
 * HOW IT WORKS
 * ============
 *
 * 1. ENTITY INSERTION:
 *    - Calculate which grid cells the entity's bounding box overlaps
 *    - Add entity to all relevant cell lists
 *    - Trigger auto-tuning if average cell density exceeds threshold
 *
 * 2. COLLISION DETECTION:
 *    - Convert dense cells (>8 entities) to DBVH regions
 *    - Query DBVH regions for internal entity pairs
 *    - Query remaining grid cells for entity pairs
 *    - Check neighboring cells for cross-boundary collisions
 *    - Apply component-based filtering before expensive AABB tests
 *
 * 3. AUTO-TUNING:
 *    - Calculates average entity diagonal size
 *    - Adjusts cell size to 2x average diagonal
 *    - Prevents excessive entity clustering in single cells
 *
 * STEP-BY-STEP EXAMPLES
 * =====================
 *
 * Example 1: Simple Entity Insertion
 * ----------------------------------
 *
 * Given: Entity at position (100, 50) with size (64, 32)
 *        Cell size: 128x128
 *
 * Step 1: Calculate grid coordinates
 *   - min_cell_x = floor(100 / 128) = 0
 *   - min_cell_y = floor(50 / 128) = 0
 *   - max_cell_x = floor(164 / 128) = 1
 *   - max_cell_y = floor(82 / 128) = 0
 *
 * Step 2: Insert into cells (0,0) and (1,0)
 *   - Cell (0,0): Add entity to list
 *   - Cell (1,0): Add entity to list
 *
 * Example 2: Dense Region Conversion to DBVH
 * -----------------------------------------
 *
 * Given: Cell (5,3) contains 12 entities (exceeds threshold of 8)
 *
 * Step 1: Collect entities from 3x3 region centered at (5,3)
 *   - Gather entities from cells (4,2) through (6,4)
 *   - Total entities: 15 (from 9 cells)
 *
 * Step 2: Build DBVH tree
 *   - Create root node with bounds encompassing all entities
 *   - Insert entities one by one, maintaining AVL balance
 *   - Each leaf node stores one entity
 *   - Internal nodes store bounding boxes of children
 *
 * Step 3: Replace grid cells with DBVH region
 *   - Remove entity lists from 9 grid cells
 *   - Store DBVH root in dbvh_regions map
 *   - Mark region center as (5,3)
 *
 * Example 3: Collision Detection Process
 * -------------------------------------
 *
 * Given: Mixed grid/DBVH spatial index
 *
 * Step 1: Convert dense cells to DBVH
 *   - Scan all grid cells
 *   - Convert cells with >8 entities to DBVH regions
 *
 * Step 2: Query DBVH regions
 *   - For each DBVH region:
 *     - Collect all entities in region
 *     - Check all entity pairs within region
 *     - Check entities against neighboring grid cells
 *
 * Step 3: Query remaining grid cells
 *   - For each cell with 2+ entities:
 *     - Check all entity pairs within cell
 *     - Check against 8 neighboring cells
 *     - Skip already-processed neighbor pairs
 *
 * Step 4: Apply filtering
 *   - Component-based prefilter (collider vs map interactions)
 *   - AABB intersection test
 *   - Deduplicate pairs using entity ID combinations
 *
 * Example 4: Auto-tuning Process
 * -----------------------------
 *
 * Given: Average cell density > 10 entities
 *
 * Step 1: Sample entity sizes
 *   - Iterate through all non-empty cells
 *   - Calculate diagonal of first entity in each cell
 *   - Compute average diagonal size
 *
 * Step 2: Adjust cell size
 *   - new_size = max(32, average_diagonal * 2)
 *   - Update cell_size parameter
 *   - Log adjustment for debugging
 *
 * Step 3: Cooldown period
 *   - Set last_auto_tune_time to current time
 *   - Prevent frequent adjustments (5-second cooldown)
 *
 * PERFORMANCE CHARACTERISTICS
 * ===========================
 *
 * Time Complexity:
 * - Insertion: O(1) per entity (amortized)
 * - Collision detection: O(n + k) where n=entities, k=collision pairs
 * - DBVH operations: O(log n) for balanced trees
 *
 * Space Complexity:
 * - Grid cells: O(n) where n=entities
 * - DBVH regions: O(n) in worst case
 * - Total: O(n) linear with entity count
 *
 * Memory Layout:
 * - SpatialIndex: 40 bytes + hashmap overhead
 * - DBVHNode: 48 bytes per node
 * - Grid cells: Variable based on entity distribution
 *
 * OPTIMIZATION FEATURES
 * =====================
 *
 * 1. Component-based filtering: Skip incompatible entity pairs early
 * 2. AABB prechecking: Avoid expensive collision tests for non-overlapping
 * entities
 * 3. Pair deduplication: Prevent duplicate collision pairs using sorted entity
 * IDs
 * 4. Neighbor querying: Only check adjacent cells for cross-boundary collisions
 * 5. Auto-tuning: Adapt cell size to entity distribution patterns
 * 6. DBVH conversion: Use hierarchical structures for dense regions
 * 7. Profile counting: Track performance metrics for optimization
 *
 * USAGE PATTERNS
 * ==============
 *
 * Typical workflow:
 * 1. Create spatial index with spatial_index_create()
 * 2. Insert entities with spatial_index_insert() as they move
 * 3. Query collision pairs with spatial_index_get_pairs()
 * 4. Process collision pairs in collision resolver
 * 5. Clear index with spatial_index_clear() when needed
 * 6. Destroy index with spatial_index_destroy() when done
 *
 * Thread Safety:
 * - Not thread-safe by design
 * - Single-threaded collision detection
 * - External synchronization required for multi-threaded access
 */

#include "spatial_index.h"
#include "core/memory_manager.h"
#include "entity/components/entity_component_collider.h"
#include "entity/components/entity_component_private.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "platform/time.h"
#include "types/rect.h"
#include "utility/array.h"
#include "utility/double_linked_list.h"
#include "utility/hashmap.h"
#include "utility/int_hashmap.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <limits.h>
#include <math.h>
#include <string.h>

#define SPATIAL_INDEX_DEFAULT_CELL_SIZE 128.0f
#define SPATIAL_INDEX_AUTO_TUNE_THRESHOLD 10
#define SPATIAL_INDEX_DBVH_THRESHOLD 8
#define SPATIAL_INDEX_AUTO_TUNE_COOLDOWN_SECONDS 5.0

typedef uint64_t SpatialIndexKey;

typedef struct DBVHNode {
    float bounds_x;
    float bounds_y;
    float bounds_width;
    float bounds_height;
    EseEntity *entity;
    struct DBVHNode *left;
    struct DBVHNode *right;
    int height;
    int region_center_x;
    int region_center_y;
} DBVHNode;

struct SpatialIndex {
    float cell_size;
    EseIntHashMap *bins;         // IntHashmap<SpatialIndexKey, EseDoubleLinkedList*>
    EseIntHashMap *dbvh_regions; // IntHashmap<SpatialIndexKey, DBVHNode*>
    EseArray *pairs;             // Array of SpatialPair*
    double last_auto_tune_time;
};

static SpatialIndexKey _spatial_index_compute_key(int x, int y) {
    uint32_t ux = (uint32_t)(int32_t)x;
    uint32_t uy = (uint32_t)(int32_t)y;
    uint64_t key = ((uint64_t)ux << 32) | (uint64_t)uy;
    return (SpatialIndexKey)key;
}

static void _free_spatial_pair(void *ptr) {
    if (ptr)
        memory_manager.shared.free(ptr);
}

static int _dbvh_get_height(DBVHNode *node) { return node ? node->height : 0; }
static int _dbvh_get_balance(DBVHNode *node) {
    return node ? _dbvh_get_height(node->left) - _dbvh_get_height(node->right) : 0;
}

static DBVHNode *_dbvh_node_create(EseEntity *entity) {
    DBVHNode *node = memory_manager.malloc(sizeof(DBVHNode), MMTAG_COLLISION_INDEX);
    if (!node)
        return NULL;
    node->entity = entity;
    node->left = NULL;
    node->right = NULL;
    node->height = 1;
    node->region_center_x = INT_MIN;
    node->region_center_y = INT_MIN;
    if (entity && entity->collision_world_bounds) {
        EseRect *r = entity->collision_world_bounds;
        node->bounds_x = ese_rect_get_x(r);
        node->bounds_y = ese_rect_get_y(r);
        node->bounds_width = ese_rect_get_width(r);
        node->bounds_height = ese_rect_get_height(r);
    } else {
        node->bounds_x = node->bounds_y = node->bounds_width = node->bounds_height = 0;
    }
    return node;
}

static void _dbvh_node_destroy(DBVHNode *node) {
    if (!node)
        return;
    _dbvh_node_destroy(node->left);
    _dbvh_node_destroy(node->right);
    memory_manager.free(node);
}

static void _dbvh_update_bounds(DBVHNode *node) {
    if (!node)
        return;
    if (node->entity && node->entity->collision_world_bounds) {
        EseRect *b = node->entity->collision_world_bounds;
        node->bounds_x = ese_rect_get_x(b);
        node->bounds_y = ese_rect_get_y(b);
        node->bounds_width = ese_rect_get_width(b);
        node->bounds_height = ese_rect_get_height(b);
        return;
    }
    if (node->left && node->right) {
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

static DBVHNode *_dbvh_insert(DBVHNode *root, EseEntity *entity) {
    if (!entity || !entity->collision_world_bounds)
        return root;
    if (!root)
        return _dbvh_node_create(entity);
    DBVHNode *new_node = _dbvh_node_create(entity);
    if (!new_node)
        return root;
    if (!root->right) {
        root->right = new_node;
    } else {
        DBVHNode *internal = _dbvh_node_create(NULL);
        if (!internal) {
            _dbvh_node_destroy(new_node);
            return root;
        }
        internal->left = root;
        internal->right = new_node;
        root = internal;
    }
    root->height = 1 + fmax(_dbvh_get_height(root->left), _dbvh_get_height(root->right));
    _dbvh_update_bounds(root);
    int balance = _dbvh_get_balance(root);
    if (balance > 1) {
        if (_dbvh_get_balance(root->left) < 0)
            root->left = _dbvh_rotate_left(root->left);
        return _dbvh_rotate_right(root);
    }
    if (balance < -1) {
        if (_dbvh_get_balance(root->right) > 0)
            root->right = _dbvh_rotate_right(root->right);
        return _dbvh_rotate_left(root);
    }
    return root;
}

static void _dbvh_collect_entities(DBVHNode *root, EseArray *entities) {
    if (!root)
        return;
    if (root->entity) {
        array_push(entities, root->entity);
    } else {
        _dbvh_collect_entities(root->left, entities);
        _dbvh_collect_entities(root->right, entities);
    }
}

static bool _pair_is_potential_collision(EseEntity *a, EseEntity *b) {
    if (!a || !b)
        return false;
    bool a_has_map = false, b_has_map = false;
    bool a_has_collider = false, b_has_collider = false;
    bool a_collider_map_interaction = false, b_collider_map_interaction = false;

    for (size_t i = 0; i < a->component_count; i++) {
        EseEntityComponent *comp = a->components[i];
        if (!comp || !comp->active)
            continue;
        if (comp->type == ENTITY_COMPONENT_MAP)
            a_has_map = true;
        if (comp->type == ENTITY_COMPONENT_COLLIDER) {
            a_has_collider = true;
            EseEntityComponentCollider *col = (EseEntityComponentCollider *)comp->data;
            if (col)
                a_collider_map_interaction = col->map_interaction;
        }
    }
    for (size_t j = 0; j < b->component_count; j++) {
        EseEntityComponent *comp = b->components[j];
        if (!comp || !comp->active)
            continue;
        if (comp->type == ENTITY_COMPONENT_MAP)
            b_has_map = true;
        if (comp->type == ENTITY_COMPONENT_COLLIDER) {
            b_has_collider = true;
            EseEntityComponentCollider *col = (EseEntityComponentCollider *)comp->data;
            if (col)
                b_collider_map_interaction = col->map_interaction;
        }
    }

    if (a_has_collider && b_has_collider)
        return true;
    if (a_has_map && b_has_collider && b_collider_map_interaction)
        return true;
    if (b_has_map && a_has_collider && a_collider_map_interaction)
        return true;
    return false;
}

static float _calculate_average_bin_count(SpatialIndex *index) {
    size_t total_entities = 0;
    size_t non_empty_bins = 0;
    EseIntHashMapIter *iter = int_hashmap_iter_create(index->bins);
    uint64_t key;
    void *value;
    while (int_hashmap_iter_next(iter, &key, &value)) {
        EseDoubleLinkedList *list = (EseDoubleLinkedList *)value;
        size_t sz = dlist_size(list);
        if (sz > 0) {
            total_entities += sz;
            non_empty_bins++;
        }
    }
    int_hashmap_iter_free(iter);
    return non_empty_bins > 0 ? (float)total_entities / (float)non_empty_bins : 0.0f;
}

static void _emit_pair_if_new(SpatialIndex *index, EseHashMap *seen, EseArray *pairs, EseEntity *a,
                              EseEntity *b) {
    if (!a || !b || !seen || !pairs)
        return;
    if (a == b)
        return;
    if (a->id && b->id && strcmp(ese_uuid_get_value(a->id), ese_uuid_get_value(b->id)) == 0)
        return;

    const char *ida = ese_uuid_get_value(a->id);
    const char *idb = ese_uuid_get_value(b->id);
    const char *first = ida;
    const char *second = idb;

    if (strcmp(ida, idb) > 0) {
        first = idb;
        second = ida;
    }

    size_t keylen = strlen(first) + 1 + strlen(second) + 1;
    char *key = memory_manager.malloc(keylen, MMTAG_COLLISION_INDEX);
    snprintf(key, keylen, "%s|%s", first, second);

    if (hashmap_get(seen, key) != NULL) {
        memory_manager.free(key);
        return;
    }

    hashmap_set(seen, key, (void *)1);
    memory_manager.free(key);

    // Use shared allocator so SpatialPair can be freed from any thread
    SpatialPair *pair =
        (SpatialPair *)memory_manager.shared.malloc(sizeof(SpatialPair), MMTAG_COLLISION_INDEX);
    pair->a = a;
    pair->b = b;

    if (!array_push(pairs, pair)) {
        log_warn("SPATIAL_INDEX", "Failed to add pair to array");
        memory_manager.shared.free(pair);
    }
}

static void _dbvh_query_pairs(DBVHNode *root, EseArray *pairs, SpatialIndex *index,
                              EseHashMap *seen) {
    if (!root || !index || !pairs)
        return;
    EseArray *entities = array_create(64, NULL);
    _dbvh_collect_entities(root, entities);
    for (size_t i = 0; i < array_size(entities); i++) {
        EseEntity *a = (EseEntity *)array_get(entities, i);
        for (size_t j = i + 1; j < array_size(entities); j++) {
            EseEntity *b = (EseEntity *)array_get(entities, j);
            // Component-kind prefilter and AABB precheck to prune early
            if (_pair_is_potential_collision(a, b) && a->collision_world_bounds &&
                b->collision_world_bounds &&
                ese_rect_intersects(a->collision_world_bounds, b->collision_world_bounds)) {
                _emit_pair_if_new(index, seen, pairs, a, b);
            }
        }
    }
    int cx = root->region_center_x;
    int cy = root->region_center_y;
    if (cx != INT_MIN && cy != INT_MIN) {
        for (int nx = cx - 2; nx <= cx + 2; nx++) {
            for (int ny = cy - 2; ny <= cy + 2; ny++) {
                if (nx >= cx - 1 && nx <= cx + 1 && ny >= cy - 1 && ny <= cy + 1)
                    continue;
                SpatialIndexKey nkey = _spatial_index_compute_key(nx, ny);
                if (int_hashmap_get(index->dbvh_regions, nkey))
                    continue;
                EseDoubleLinkedList *neighbor_list =
                    (EseDoubleLinkedList *)int_hashmap_get(index->bins, nkey);
                if (!neighbor_list || dlist_size(neighbor_list) == 0)
                    continue;
                for (size_t i = 0; i < array_size(entities); i++) {
                    EseEntity *a = (EseEntity *)array_get(entities, i);
                    EseDListIter *it = dlist_iter_create(neighbor_list);
                    void *val;
                    while (dlist_iter_next(it, &val)) {
                        EseEntity *b = (EseEntity *)val;
                        // Component-kind prefilter and AABB precheck
                        if (_pair_is_potential_collision(a, b) && a->collision_world_bounds &&
                            b->collision_world_bounds &&
                            ese_rect_intersects(a->collision_world_bounds,
                                                b->collision_world_bounds)) {
                            _emit_pair_if_new(index, seen, pairs, a, b);
                        }
                    }
                    dlist_iter_free(it);
                }
            }
        }
    }
    array_destroy(entities);
}

static void _convert_cell_to_dbvh(SpatialIndex *index, int center_x, int center_y) {
    SpatialIndexKey center_key = _spatial_index_compute_key(center_x, center_y);
    if (int_hashmap_get(index->dbvh_regions, center_key))
        return;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            SpatialIndexKey k = _spatial_index_compute_key(center_x + dx, center_y + dy);
            if (!int_hashmap_get(index->bins, k))
                return;
        }
    }
    EseArray *entities = array_create(64, NULL);
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            int cell_x = center_x + dx;
            int cell_y = center_y + dy;
            SpatialIndexKey key = _spatial_index_compute_key(cell_x, cell_y);
            EseDoubleLinkedList *list = (EseDoubleLinkedList *)int_hashmap_get(index->bins, key);
            if (!list)
                continue;
            EseDListIter *it = dlist_iter_create(list);
            void *val;
            while (dlist_iter_next(it, &val))
                array_push(entities, val);
            dlist_iter_free(it);
        }
    }
    if (array_size(entities) == 0) {
        array_destroy(entities);
        return;
    }
    DBVHNode *root = NULL;
    for (size_t i = 0; i < array_size(entities); i++) {
        EseEntity *e = (EseEntity *)array_get(entities, i);
        root = _dbvh_insert(root, e);
    }
    if (root) {
        root->region_center_x = center_x;
        root->region_center_y = center_y;
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                SpatialIndexKey k = _spatial_index_compute_key(center_x + dx, center_y + dy);
                EseDoubleLinkedList *removed =
                    (EseDoubleLinkedList *)int_hashmap_remove(index->bins, k);
                if (removed) {
                    dlist_free(removed);
                }
            }
        }
        int_hashmap_set(index->dbvh_regions, center_key, root);
        log_debug("SPATIAL_INDEX", "Converted 3x3 centered (%d,%d) to DBVH with %zu entities",
                  center_x, center_y, array_size(entities));
    }
    array_destroy(entities);
}

SpatialIndex *spatial_index_create(void) {
    SpatialIndex *index = memory_manager.malloc(sizeof(SpatialIndex), MMTAG_COLLISION_INDEX);
    index->cell_size = SPATIAL_INDEX_DEFAULT_CELL_SIZE;
    index->bins = int_hashmap_create((EseIntHashMapFreeFn)dlist_free);
    index->dbvh_regions = int_hashmap_create((EseIntHashMapFreeFn)_dbvh_node_destroy);
    index->pairs = array_create(128, _free_spatial_pair);
    index->last_auto_tune_time = 0.0;
    return index;
}

void spatial_index_destroy(SpatialIndex *index) {
    log_assert("SPATIAL_INDEX", index, "destroy called with NULL index");
    int_hashmap_destroy(index->bins);
    int_hashmap_destroy(index->dbvh_regions);
    array_destroy(index->pairs);
    memory_manager.free(index);
}

void spatial_index_clear(SpatialIndex *index) {
    log_assert("SPATIAL_INDEX", index, "clear called with NULL index");
    int_hashmap_clear(index->bins);
    int_hashmap_clear(index->dbvh_regions);
    array_clear(index->pairs);
}

void spatial_index_insert(SpatialIndex *index, EseEntity *entity) {
    log_assert("SPATIAL_INDEX", index, "insert called with NULL index");
    log_assert("SPATIAL_INDEX", entity, "insert called with NULL entity");
    if (!entity->active)
        return;
    if (!entity->collision_world_bounds)
        return;
    EseRect *bounds = entity->collision_world_bounds;
    float x0 = ese_rect_get_x(bounds);
    float y0 = ese_rect_get_y(bounds);
    float x1 = x0 + ese_rect_get_width(bounds);
    float y1 = y0 + ese_rect_get_height(bounds);
    int min_cell_x = (int)floorf(x0 / index->cell_size);
    int min_cell_y = (int)floorf(y0 / index->cell_size);
    int max_cell_x = (int)floorf((x1) / index->cell_size);
    int max_cell_y = (int)floorf((y1) / index->cell_size);
    for (int cx = min_cell_x; cx <= max_cell_x; cx++) {
        for (int cy = min_cell_y; cy <= max_cell_y; cy++) {
            SpatialIndexKey key = _spatial_index_compute_key(cx, cy);
            if (int_hashmap_get(index->dbvh_regions, key))
                continue;
            EseDoubleLinkedList *list = (EseDoubleLinkedList *)int_hashmap_get(index->bins, key);
            if (!list) {
                list = dlist_create(NULL);
                int_hashmap_set(index->bins, key, list);
            }
            dlist_append(list, entity);
            profile_count_add("spatial_index_entity_cell_insert");
        }
    }
    double now = time_now_seconds();
    if (now - index->last_auto_tune_time >= SPATIAL_INDEX_AUTO_TUNE_COOLDOWN_SECONDS) {
        float avg = _calculate_average_bin_count(index);
        if (avg > SPATIAL_INDEX_AUTO_TUNE_THRESHOLD) {
            spatial_index_auto_tune(index);
            index->last_auto_tune_time = now;
        }
    }
}

void spatial_index_auto_tune(SpatialIndex *index) {
    log_assert("SPATIAL_INDEX", index, "auto_tune called with NULL index");
    float total = 0.0f;
    size_t samples = 0;
    EseIntHashMapIter *iter = int_hashmap_iter_create(index->bins);
    uint64_t key;
    void *value;
    while (int_hashmap_iter_next(iter, &key, &value)) {
        EseDoubleLinkedList *list = (EseDoubleLinkedList *)value;
        if (dlist_size(list) > 0) {
            EseDListIter *it = dlist_iter_create(list);
            void *val;
            if (dlist_iter_next(it, &val)) {
                EseEntity *e = (EseEntity *)val;
                if (e->collision_world_bounds) {
                    EseRect *r = e->collision_world_bounds;
                    float diag = sqrtf(ese_rect_get_width(r) * ese_rect_get_width(r) +
                                       ese_rect_get_height(r) * ese_rect_get_height(r));
                    total += diag;
                    samples++;
                }
            }
            dlist_iter_free(it);
        }
    }
    int_hashmap_iter_free(iter);
    if (samples == 0) {
        index->cell_size = SPATIAL_INDEX_DEFAULT_CELL_SIZE;
        return;
    }
    float avg = total / (float)samples;
    float new_size = fmaxf(32.0f, avg * 2.0f);
    index->cell_size = new_size;
    log_debug("SPATIAL_INDEX", "Auto-tuned cell_size to %f based on %zu samples (avg diag: %f)",
              new_size, samples, avg);
}

EseArray *spatial_index_get_pairs(SpatialIndex *index) {
    log_assert("SPATIAL_INDEX", index, "get_pairs called with NULL index");
    profile_start(PROFILE_SPATIAL_INDEX_SECTION);
    array_clear(index->pairs);

    EseIntHashMapIter *bin_iter = int_hashmap_iter_create(index->bins);
    uint64_t bin_key;
    void *bin_value;
    while (int_hashmap_iter_next(bin_iter, &bin_key, &bin_value)) {
        EseDoubleLinkedList *cell_list = (EseDoubleLinkedList *)bin_value;
        size_t sz = dlist_size(cell_list);
        if (sz > SPATIAL_INDEX_DBVH_THRESHOLD) {
            uint32_t ux = (uint32_t)(bin_key >> 32);
            uint32_t uy = (uint32_t)(bin_key & 0xFFFFFFFFu);
            int cell_x = (int)(int32_t)ux;
            int cell_y = (int)(int32_t)uy;
            _convert_cell_to_dbvh(index, cell_x, cell_y);
        }
    }
    int_hashmap_iter_free(bin_iter);

    EseHashMap *seen = hashmap_create(NULL);
    EseIntHashMapIter *dbvh_iter = int_hashmap_iter_create(index->dbvh_regions);
    uint64_t dbvh_key;
    void *dbvh_value;
    while (int_hashmap_iter_next(dbvh_iter, &dbvh_key, &dbvh_value)) {
        DBVHNode *root = (DBVHNode *)dbvh_value;
        _dbvh_query_pairs(root, index->pairs, index, seen);
    }
    int_hashmap_iter_free(dbvh_iter);

    bin_iter = int_hashmap_iter_create(index->bins);
    while (int_hashmap_iter_next(bin_iter, &bin_key, &bin_value)) {
        EseDoubleLinkedList *cell_list = (EseDoubleLinkedList *)bin_value;
        if (!cell_list || dlist_size(cell_list) == 0)
            continue;
        uint32_t ux = (uint32_t)(bin_key >> 32);
        uint32_t uy = (uint32_t)(bin_key & 0xFFFFFFFFu);
        int cell_x = (int)(int32_t)ux;
        int cell_y = (int)(int32_t)uy;
        if (dlist_size(cell_list) >= 2) {
            void *a_val;
            EseDListIter *outer = dlist_iter_create(cell_list);
            while (dlist_iter_next(outer, &a_val)) {
                EseEntity *a = (EseEntity *)a_val;
                EseDListIter *inner = dlist_iter_create_from(outer);
                void *b_val;
                while (dlist_iter_next(inner, &b_val)) {
                    EseEntity *b = (EseEntity *)b_val;
                    // AABB precheck to prune non-overlapping pairs early
                    if (a->collision_world_bounds && b->collision_world_bounds &&
                        ese_rect_intersects(a->collision_world_bounds, b->collision_world_bounds)) {
                        _emit_pair_if_new(index, seen, index->pairs, a, b);
                    }
                }
                dlist_iter_free(inner);
            }
            dlist_iter_free(outer);
        }
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                if (dx == 0 && dy == 0)
                    continue;
                int nx = cell_x + dx;
                int ny = cell_y + dy;
                SpatialIndexKey nkey = _spatial_index_compute_key(nx, ny);
                if (nkey <= bin_key)
                    continue;
                if (int_hashmap_get(index->dbvh_regions, nkey))
                    continue;
                EseDoubleLinkedList *neighbor_list =
                    (EseDoubleLinkedList *)int_hashmap_get(index->bins, nkey);
                if (!neighbor_list || dlist_size(neighbor_list) == 0)
                    continue;
                EseDListIter *cell_it = dlist_iter_create(cell_list);
                void *c_val;
                while (dlist_iter_next(cell_it, &c_val)) {
                    EseEntity *c = (EseEntity *)c_val;
                    EseDListIter *neigh_it = dlist_iter_create(neighbor_list);
                    void *n_val;
                    while (dlist_iter_next(neigh_it, &n_val)) {
                        EseEntity *n = (EseEntity *)n_val;
                        // Component-kind prefilter and AABB precheck
                        if (_pair_is_potential_collision(c, n) && c->collision_world_bounds &&
                            n->collision_world_bounds &&
                            ese_rect_intersects(c->collision_world_bounds,
                                                n->collision_world_bounds)) {
                            _emit_pair_if_new(index, seen, index->pairs, c, n);
                            profile_count_add("spatial_index_pair_emitted");
                        }
                    }
                    dlist_iter_free(neigh_it);
                }
                dlist_iter_free(cell_it);
            }
        }
    }
    int_hashmap_iter_free(bin_iter);
    hashmap_destroy(seen);
    profile_stop(PROFILE_SPATIAL_INDEX_SECTION, "spatial_index_get_pairs");
    return index->pairs;
}
