/**
 * @file collision_index.h
 *
 * @brief This file contains the collision index system for efficient spatial
 * collision detection.
 *
 * @details The collision index uses a spatial hash grid to partition entities
 * for fast collision detection. It automatically manages collision pair
 * collection and provides efficient neighbor lookups for collision checking.
 */
#ifndef ESE_COLLISION_INDEX_H
#define ESE_COLLISION_INDEX_H

#include "core/memory_manager.h"
#include "entity/entity.h"
#include "types/point.h"
#include "types/rect.h"
#include "utility/array.h"
#include "utility/double_linked_list.h"
#include "utility/int_hashmap.h"

/**
 * @brief Hashed key for grid cells (combines x and y coordinates into a
 * uint64_t)
 */
typedef uint64_t EseCollisionIndexKey;

/**
 * @brief Structure representing a collision pair between two entities.
 */
typedef struct CollisionPair {
  EseEntity *entity_a; /** First entity in the collision pair */
  EseEntity *entity_b; /** Second entity in the collision pair */
} CollisionPair;

/**
 * @brief DBVH node structure for dynamic bounding volume hierarchy.
 */
typedef struct DBVHNode {
  float bounds_x;         /** Bounding box x coordinate */
  float bounds_y;         /** Bounding box y coordinate */
  float bounds_width;     /** Bounding box width */
  float bounds_height;    /** Bounding box height */
  EseEntity *entity;      /** Entity (NULL for internal nodes) */
  struct DBVHNode *left;  /** Left child node */
  struct DBVHNode *right; /** Right child node */
  int height;             /** Height of this subtree */
  int region_center_x;
  int region_center_y;
} DBVHNode;

/**
 * @brief Opaque structure for the collision index using a spatial hash grid.
 */
typedef struct EseCollisionIndex EseCollisionIndex;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
/**
 * @brief Creates a new collision index with default cell size.
 *
 * @return A pointer to the newly created EseCollisionIndex, or NULL on failure
 */
EseCollisionIndex *collision_index_create(void);

/**
 * @brief Destroys the collision index and frees all resources.
 *
 * @param index Pointer to the collision index to destroy
 */
void collision_index_destroy(EseCollisionIndex *index);

/**
 * @brief Clears all bins (removes entities but keeps structure).
 *
 * @param index Pointer to the collision index to clear
 */
void collision_index_clear(EseCollisionIndex *index);

// Entity management
/**
 * @brief Inserts an entity into the collision index based on its position and
 * collider rects.
 *
 * @param index Pointer to the collision index
 * @param entity Pointer to the entity to insert
 */
void collision_index_insert(EseCollisionIndex *index, EseEntity *entity);

// Spatial queries
/**
 * @brief Gets the list of entities in a specific cell.
 *
 * @param index Pointer to the collision index
 * @param cell_x X coordinate of the cell
 * @param cell_y Y coordinate of the cell
 * @return Pointer to the list of entities in the cell, or NULL if empty
 */
EseDoubleLinkedList *collision_index_get_cell(EseCollisionIndex *index,
                                              int cell_x, int cell_y);

/**
 * @brief Gets neighboring cells' entities for collision checking.
 *
 * @param index Pointer to the collision index
 * @param cell_x X coordinate of the center cell
 * @param cell_y Y coordinate of the center cell
 * @param neighbors Array to store neighbor cell lists
 * @param neighbor_count Pointer to store the number of neighbors found
 */
void collision_index_get_neighbors(EseCollisionIndex *index, int cell_x,
                                   int cell_y, EseDoubleLinkedList **neighbors,
                                   size_t *neighbor_count);

// Collision detection
/**
 * @brief Gets all collision pairs from the spatial index.
 *
 * @param index Pointer to the collision index
 * @return Pointer to the array of collision pairs
 *
 * @note The returned EseArray is owned by the collision index and should NOT be
 * freed by the caller. The array is automatically cleared internally before
 * collecting new pairs.
 */
EseArray *collision_index_get_pairs(EseCollisionIndex *index);

// Optimization
/**
 * @brief Auto-tunes the cell size based on current entities' sizes.
 *
 * @param index Pointer to the collision index to optimize
 */
void collision_index_auto_tune(EseCollisionIndex *index);

#endif // ESE_COLLISION_INDEX_H
