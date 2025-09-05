// core/collision_index.c
#include "collision_index.h"
#include "utility/log.h"
#include "utility/array.h"
#include "utility/double_linked_list.h"
#include "utility/int_hashmap.h"
#include "utility/hashmap.h"
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

struct EseCollisionIndex {
    float cell_size;
    EseIntHashMap *bins;        // IntHashmap<EseCollisionIndexKey, EseDoubleLinkedList*>
    EseIntHashMap *dbvh_regions; // IntHashmap<EseCollisionIndexKey, DBVHNode*>
    EseArray *collision_pairs;
    double last_auto_tune_time;
};

// Forward declarations
static void _free_collision_pair(void *ptr);
static int _dbvh_get_height(DBVHNode *node);
static int _dbvh_get_balance(DBVHNode *node);
static DBVHNode *_dbvh_rotate_right(DBVHNode *y);
static DBVHNode *_dbvh_rotate_left(DBVHNode *x);
static void _dbvh_update_bounds(DBVHNode *node);
static void _dbvh_collect_entities(DBVHNode *root, EseArray *entities);
static void _collision_index_convert_cell_to_dbvh(EseCollisionIndex *index, int center_x, int center_y);
static void _collision_index_emit_pair_if_new(EseCollisionIndex *index, EseHashMap *seen, EseArray *pairs, EseEntity *a, EseEntity *b, int state);

// DBVH function forward declarations
static DBVHNode *_dbvh_node_create(EseEntity *entity);
static void _dbvh_node_destroy(DBVHNode *node);
static DBVHNode *_dbvh_insert(DBVHNode *root, EseEntity *entity);
static void _dbvh_query_pairs(DBVHNode *root, EseArray *pairs, EseCollisionIndex *index, EseHashMap *seen);

// Helper to free collision pairs in the array
static void _free_collision_pair(void *ptr) {
    if (ptr) memory_manager.free(ptr);
}

// Pack two signed 32-bit into uint64 key
EseCollisionIndexKey _collision_index_compute_key(int x, int y) {
    uint32_t ux = (uint32_t)(int32_t)x;
    uint32_t uy = (uint32_t)(int32_t)y;
    uint64_t key = ((uint64_t)ux << 32) | (uint64_t)uy;
    return (EseCollisionIndexKey)key;
}

float _collision_index_calculate_average_bin_count(EseCollisionIndex *index) {
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

// ==================== DBVH ====================

static DBVHNode *_dbvh_node_create(EseEntity *entity) {
    DBVHNode *node = memory_manager.malloc(sizeof(DBVHNode), MMTAG_ENTITY);
    if (!node) return NULL;
    node->entity = entity;
    node->left = NULL;
    node->right = NULL;
    node->height = 1;
    node->region_center_x = INT_MIN;
    node->region_center_y = INT_MIN;
    if (entity && entity->collision_world_bounds) {
        EseRect *r = entity->collision_world_bounds;
        node->bounds_x = rect_get_x(r);
        node->bounds_y = rect_get_y(r);
        node->bounds_width = rect_get_width(r);
        node->bounds_height = rect_get_height(r);
    } else {
        node->bounds_x = node->bounds_y = node->bounds_width = node->bounds_height = 0;
    }
    return node;
}

static void _dbvh_node_destroy(DBVHNode *node) {
    if (!node) return;
    _dbvh_node_destroy(node->left);
    _dbvh_node_destroy(node->right);
    memory_manager.free(node);
}

static int _dbvh_get_height(DBVHNode *node) { return node ? node->height : 0; }
static int _dbvh_get_balance(DBVHNode *node) { return node ? _dbvh_get_height(node->left) - _dbvh_get_height(node->right) : 0; }

static void _dbvh_update_bounds(DBVHNode *node) {
    if (!node) return;
    if (node->entity && node->entity->collision_world_bounds) {
        EseRect *b = node->entity->collision_world_bounds;
        node->bounds_x = rect_get_x(b);
        node->bounds_y = rect_get_y(b);
        node->bounds_width = rect_get_width(b);
        node->bounds_height = rect_get_height(b);
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
    if (!entity || !entity->collision_world_bounds) return root;
    if (!root) return _dbvh_node_create(entity);
    DBVHNode *new_node = _dbvh_node_create(entity);
    if (!new_node) return root;
    if (!root->right) {
        root->right = new_node;
    } else {
        DBVHNode *internal = _dbvh_node_create(NULL);
        if (!internal) { _dbvh_node_destroy(new_node); return root; }
        internal->left = root;
        internal->right = new_node;
        root = internal;
    }
    root->height = 1 + fmax(_dbvh_get_height(root->left), _dbvh_get_height(root->right));
    _dbvh_update_bounds(root);
    int balance = _dbvh_get_balance(root);
    if (balance > 1) {
        if (_dbvh_get_balance(root->left) < 0) root->left = _dbvh_rotate_left(root->left);
        return _dbvh_rotate_right(root);
    }
    if (balance < -1) {
        if (_dbvh_get_balance(root->right) > 0) root->right = _dbvh_rotate_right(root->right);
        return _dbvh_rotate_left(root);
    }
    return root;
}

static void _dbvh_collect_entities(DBVHNode *root, EseArray *entities) {
    if (!root) return;
    if (root->entity) {
        array_push(entities, root->entity);
    } else {
        _dbvh_collect_entities(root->left, entities);
        _dbvh_collect_entities(root->right, entities);
    }
}

// Emit helper: canonicalize unordered pair, check seen, push to pairs.
static void _collision_index_emit_pair_if_new(EseCollisionIndex *index, EseHashMap *seen, EseArray *pairs, EseEntity *a, EseEntity *b, int state) {
    if (!a || !b || !seen || !pairs) return;

    // Skip if the same entity
    if (a == b) return;
    if (a->id && b->id && strcmp(a->id->value, b->id->value) == 0) return;

    const char *ida = a->id->value;
    const char *idb = b->id->value;
    const char *first = ida;
    const char *second = idb;
    if (strcmp(ida, idb) > 0) { first = idb; second = ida; }
    size_t keylen = strlen(first) + 1 + strlen(second) + 1;
    char *key = memory_manager.malloc(keylen, MMTAG_ENGINE);
    if (!key) return;
    snprintf(key, keylen, "%s|%s", first, second);
    if (hashmap_get(seen, key) != NULL) { memory_manager.free(key); return; }
    // Mark seen (hashmap will own the key and free later)
    hashmap_set(seen, key, (void*)1);
    CollisionPair *pair = (CollisionPair*)memory_manager.malloc(sizeof(CollisionPair), MMTAG_ENGINE);
    pair->entity_a = a;
    pair->entity_b = b;
    pair->state = state;
    if (!array_push(pairs, pair)) {
        log_warn("COLLISION_INDEX", "Failed to add collision pair to array");
        memory_manager.free(pair);
    }
}

// DBVH query: internal pairs + DBVH entities vs neighboring grid bins
static void _dbvh_query_pairs(DBVHNode *root, EseArray *pairs, EseCollisionIndex *index, EseHashMap *seen) {
    if (!root || !index || !pairs) return;
    EseArray *entities = array_create(64, NULL);
    _dbvh_collect_entities(root, entities);
    // internal pairs
    for (size_t i = 0; i < array_size(entities); i++) {
        EseEntity *a = (EseEntity*)array_get(entities, i);
        for (size_t j = i + 1; j < array_size(entities); j++) {
            EseEntity *b = (EseEntity*)array_get(entities, j);
            int state = entity_check_collision_state(a, b);
            if (state != 0) _collision_index_emit_pair_if_new(index, seen, pairs, a, b, state);
        }
    }
    // cross-boundary: test DBVH entities against neighboring grid bins (outside 3x3)
    int cx = root->region_center_x;
    int cy = root->region_center_y;
    if (cx != INT_MIN && cy != INT_MIN) {
        for (int nx = cx - 2; nx <= cx + 2; nx++) {
            for (int ny = cy - 2; ny <= cy + 2; ny++) {
                // skip inner 3x3 (owned)
                if (nx >= cx - 1 && nx <= cx + 1 && ny >= cy - 1 && ny <= cy + 1) continue;
                EseCollisionIndexKey nkey = _collision_index_compute_key(nx, ny);
                // skip neighbor if owned by another DBVH
                if (int_hashmap_get(index->dbvh_regions, nkey)) continue;
                EseDoubleLinkedList *neighbor_list = (EseDoubleLinkedList*)int_hashmap_get(index->bins, nkey);
                if (!neighbor_list || dlist_size(neighbor_list) == 0) continue;
                for (size_t i = 0; i < array_size(entities); i++) {
                    EseEntity *a = (EseEntity*)array_get(entities, i);
                    EseDListIter *it = dlist_iter_create(neighbor_list);
                    void *val;
                    while (dlist_iter_next(it, &val)) {
                        EseEntity *b = (EseEntity*)val;
                        int state = entity_check_collision_state(a, b);
                        if (state != 0) _collision_index_emit_pair_if_new(index, seen, pairs, a, b, state);
                    }
                    dlist_iter_free(it);
                }
            }
        }
    }
    array_destroy(entities);
}

// Convert center+8 neighbors to DBVH and take ownership of those bins
static void _collision_index_convert_cell_to_dbvh(EseCollisionIndex *index, int center_x, int center_y) {
    EseCollisionIndexKey center_key = _collision_index_compute_key(center_x, center_y);
    // already converted?
    if (int_hashmap_get(index->dbvh_regions, center_key)) return;
    // prevent overlapping DBVHs: ensure all 3x3 bins still exist in bins map
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            EseCollisionIndexKey k = _collision_index_compute_key(center_x + dx, center_y + dy);
            if (!int_hashmap_get(index->bins, k)) return; // missing -> skip conversion
        }
    }
    // collect entities
    EseArray *entities = array_create(64, NULL);
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            int cell_x = center_x + dx;
            int cell_y = center_y + dy;
            EseCollisionIndexKey key = _collision_index_compute_key(cell_x, cell_y);
            EseDoubleLinkedList *list = (EseDoubleLinkedList*)int_hashmap_get(index->bins, key);
            if (!list) continue;
            EseDListIter *it = dlist_iter_create(list);
            void *val;
            while (dlist_iter_next(it, &val)) array_push(entities, val);
            dlist_iter_free(it);
        }
    }
    if (array_size(entities) == 0) { array_destroy(entities); return; }
    // build DBVH
    DBVHNode *root = NULL;
    for (size_t i = 0; i < array_size(entities); i++) {
        EseEntity *e = (EseEntity*)array_get(entities, i);
        root = _dbvh_insert(root, e);
    }
    if (root) {
        // set region metadata
        root->region_center_x = center_x;
        root->region_center_y = center_y;
        // Remove the 3x3 bins from the grid so grid-phase won't touch them.
        // NOTE: int_hashmap_remove will call the map's freefn, which in your
        // setup is dlist_free. This implementation assumes dlist_free only
        // frees the list structure and NOT the entity pointers inside.
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                EseCollisionIndexKey k = _collision_index_compute_key(center_x + dx, center_y + dy);
                int_hashmap_remove(index->bins, k);
            }
        }
        int_hashmap_set(index->dbvh_regions, center_key, root);
        log_debug("COLLISION_INDEX", "Converted 3x3 centered (%d,%d) to DBVH with %zu entities", center_x, center_y, array_size(entities));
    }
    array_destroy(entities);
}

// ==================== Public API ====================

EseCollisionIndex *collision_index_create(void) {
    EseCollisionIndex *index = memory_manager.malloc(sizeof(EseCollisionIndex), MMTAG_ENTITY);
    index->cell_size = COLLISION_INDEX_DEFAULT_CELL_SIZE;
    index->bins = int_hashmap_create((EseIntHashMapFreeFn)dlist_free);
    index->dbvh_regions = int_hashmap_create((EseIntHashMapFreeFn)_dbvh_node_destroy);
    index->collision_pairs = array_create(128, _free_collision_pair);
    index->last_auto_tune_time = 0.0;
    return index;
}

void collision_index_destroy(EseCollisionIndex *index) {
    log_assert("COLLISION_INDEX", index, "destroy called with NULL index");
    int_hashmap_free(index->bins);
    int_hashmap_free(index->dbvh_regions);
    array_destroy(index->collision_pairs);
    memory_manager.free(index);
}

void collision_index_clear(EseCollisionIndex *index) {
    log_assert("COLLISION_INDEX", index, "clear called with NULL index");
    int_hashmap_clear(index->bins);
    int_hashmap_clear(index->dbvh_regions);
    array_clear(index->collision_pairs);
}

void collision_index_insert(EseCollisionIndex *index, EseEntity *entity) {
    log_assert("COLLISION_INDEX", index, "insert called with NULL index");
    log_assert("COLLISION_INDEX", entity, "insert called with NULL entity");
    if (!entity->active) return;
    if (!entity->collision_world_bounds) return;
    EseRect *bounds = entity->collision_world_bounds;
    float x0 = rect_get_x(bounds);
    float y0 = rect_get_y(bounds);
    float x1 = x0 + rect_get_width(bounds);
    float y1 = y0 + rect_get_height(bounds);
    int min_cell_x = (int)floorf(x0 / index->cell_size);
    int min_cell_y = (int)floorf(y0 / index->cell_size);
    int max_cell_x = (int)floorf((x1) / index->cell_size);
    int max_cell_y = (int)floorf((y1) / index->cell_size);
    for (int cx = min_cell_x; cx <= max_cell_x; cx++) {
        for (int cy = min_cell_y; cy <= max_cell_y; cy++) {
            EseCollisionIndexKey key = _collision_index_compute_key(cx, cy);
            // If this cell is owned by a DBVH, skip (DBVH owns it)
            if (int_hashmap_get(index->dbvh_regions, key)) continue;
            EseDoubleLinkedList *list = (EseDoubleLinkedList*)int_hashmap_get(index->bins, key);
            if (!list) {
                list = dlist_create(NULL);
                int_hashmap_set(index->bins, key, list);
            }
            dlist_append(list, entity);
        }
    }
    double now = time_now_seconds();
    if (now - index->last_auto_tune_time >= COLLISION_INDEX_AUTO_TUNE_COOLDOWN_SECONDS) {
        float avg = _collision_index_calculate_average_bin_count(index);
        if (avg > COLLISION_INDEX_AUTO_TUNE_THRESHOLD) {
            collision_index_auto_tune(index);
            index->last_auto_tune_time = now;
        }
    }
}

EseDoubleLinkedList *collision_index_get_cell(EseCollisionIndex *index, int cell_x, int cell_y) {
    log_assert("COLLISION_INDEX", index, "get_cell called with NULL index");
    EseCollisionIndexKey key = _collision_index_compute_key(cell_x, cell_y);
    return (EseDoubleLinkedList*)int_hashmap_get(index->bins, key);
}

void collision_index_get_neighbors(EseCollisionIndex *index, int cell_x, int cell_y, EseDoubleLinkedList **neighbors, size_t *neighbor_count) {
    log_assert("COLLISION_INDEX", index, "get_neighbors called with NULL index");
    log_assert("COLLISION_INDEX", neighbors, "get_neighbors called with NULL neighbors");
    log_assert("COLLISION_INDEX", neighbor_count, "get_neighbors called with NULL neighbor_count");
    *neighbor_count = 0;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;
            EseDoubleLinkedList *l = collision_index_get_cell(index, cell_x + dx, cell_y + dy);
            if (l && dlist_size(l) > 0) {
                neighbors[*neighbor_count] = l;
                (*neighbor_count)++;
            }
        }
    }
}

void collision_index_auto_tune(EseCollisionIndex *index) {
    log_assert("COLLISION_INDEX", index, "auto_tune called with NULL index");
    float total = 0.0f;
    size_t samples = 0;
    EseIntHashMapIter *iter = int_hashmap_iter_create(index->bins);
    uint64_t key;
    void *value;
    while (int_hashmap_iter_next(iter, &key, &value)) {
        EseDoubleLinkedList *list = (EseDoubleLinkedList*)value;
        if (dlist_size(list) > 0) {
            EseDListIter *it = dlist_iter_create(list);
            void *val;
            if (dlist_iter_next(it, &val)) {
                EseEntity *e = (EseEntity*)val;
                if (e->collision_world_bounds) {
                    EseRect *r = e->collision_world_bounds;
                    float diag = sqrtf(rect_get_width(r)*rect_get_width(r) + rect_get_height(r)*rect_get_height(r));
                    total += diag;
                    samples++;
                }
            }
            dlist_iter_free(it);
        }
    }
    int_hashmap_iter_free(iter);
    if (samples == 0) { index->cell_size = COLLISION_INDEX_DEFAULT_CELL_SIZE; return; }
    float avg = total / (float)samples;
    float new_size = fmaxf(32.0f, avg * 2.0f);
    index->cell_size = new_size;
    log_debug("COLLISION_INDEX", "Auto-tuned cell_size to %f based on %zu samples (avg diag: %f)", new_size, samples, avg);
}

EseArray *collision_index_get_pairs(EseCollisionIndex *index) {
    log_assert("COLLISION_INDEX", index, "get_pairs called with NULL index");
    array_clear(index->collision_pairs);

    // PHASE 1: convert dense cells -> DBVH (3x3) and remove owned bins
    EseIntHashMapIter *bin_iter = int_hashmap_iter_create(index->bins);
    uint64_t bin_key;
    void *bin_value;
    while (int_hashmap_iter_next(bin_iter, &bin_key, &bin_value)) {
        EseDoubleLinkedList *cell_list = (EseDoubleLinkedList*)bin_value;
        size_t sz = dlist_size(cell_list);
        if (sz > COLLISION_INDEX_DBVH_THRESHOLD) {
            uint32_t ux = (uint32_t)(bin_key >> 32);
            uint32_t uy = (uint32_t)(bin_key & 0xFFFFFFFFu);
            int cell_x = (int)(int32_t)ux;
            int cell_y = (int)(int32_t)uy;
            _collision_index_convert_cell_to_dbvh(index, cell_x, cell_y);
        }
    }
    int_hashmap_iter_free(bin_iter);

    // PHASE 2: DBVH regions
    EseHashMap *seen = hashmap_create(NULL); // defensive dedupe across DBVHs
    EseIntHashMapIter *dbvh_iter = int_hashmap_iter_create(index->dbvh_regions);
    uint64_t dbvh_key;
    void *dbvh_value;
    while (int_hashmap_iter_next(dbvh_iter, &dbvh_key, &dbvh_value)) {
        DBVHNode *root = (DBVHNode*)dbvh_value;
        _dbvh_query_pairs(root, index->collision_pairs, index, seen);
    }
    int_hashmap_iter_free(dbvh_iter);

    // PHASE 3: grid cells (DBVH-owned bins were removed)
    bin_iter = int_hashmap_iter_create(index->bins);
    while (int_hashmap_iter_next(bin_iter, &bin_key, &bin_value)) {
        EseDoubleLinkedList *cell_list = (EseDoubleLinkedList*)bin_value;
        if (!cell_list || dlist_size(cell_list) == 0) continue;

        // decode coords
        uint32_t ux = (uint32_t)(bin_key >> 32);
        uint32_t uy = (uint32_t)(bin_key & 0xFFFFFFFFu);
        int cell_x = (int)(int32_t)ux;
        int cell_y = (int)(int32_t)uy;

        // intra-cell
        if (dlist_size(cell_list) >= 2) {
            void *a_val;
            EseDListIter *outer = dlist_iter_create(cell_list);
            while (dlist_iter_next(outer, &a_val)) {
                EseEntity *a = (EseEntity*)a_val;
                EseDListIter *inner = dlist_iter_create_from(outer);
                void *b_val;
                while (dlist_iter_next(inner, &b_val)) {
                    EseEntity *b = (EseEntity*)b_val;
                    int state = entity_check_collision_state(a, b);
                    if (state != 0) _collision_index_emit_pair_if_new(index, seen, index->collision_pairs, a, b, state);
                }
                dlist_iter_free(inner);
            }
            dlist_iter_free(outer);
        }

        // neighbors: use stable ordering on keys to avoid duplicating checks
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                if (dx == 0 && dy == 0) continue;
                int nx = cell_x + dx;
                int ny = cell_y + dy;
                EseCollisionIndexKey nkey = _collision_index_compute_key(nx, ny);
                if (nkey <= bin_key) continue; // ensures each unordered cell-pair handled once
                // skip neighbor if it's a DBVH center (DBVH handled cross-boundary checks)
                if (int_hashmap_get(index->dbvh_regions, nkey)) continue;
                EseDoubleLinkedList *neighbor_list = (EseDoubleLinkedList*)int_hashmap_get(index->bins, nkey);
                if (!neighbor_list || dlist_size(neighbor_list) == 0) continue;
                // iterate both lists
                EseDListIter *cell_it = dlist_iter_create(cell_list);
                void *c_val;
                while (dlist_iter_next(cell_it, &c_val)) {
                    EseEntity *c = (EseEntity*)c_val;
                    EseDListIter *neigh_it = dlist_iter_create(neighbor_list);
                    void *n_val;
                    while (dlist_iter_next(neigh_it, &n_val)) {
                        EseEntity *n = (EseEntity*)n_val;
                        int state = entity_check_collision_state(c, n);
                        if (state != 0) _collision_index_emit_pair_if_new(index, seen, index->collision_pairs, c, n, state);
                    }
                    dlist_iter_free(neigh_it);
                }
                dlist_iter_free(cell_it);
            }
        }
    }
    int_hashmap_iter_free(bin_iter);

    // cleanup seen
    hashmap_free(seen);
    return index->collision_pairs;
}