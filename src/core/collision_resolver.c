/**
 * COLLISION RESOLVER IMPLEMENTATION
 * =================================
 *
 * This file implements a collision resolution system that processes spatial
 * pairs from the spatial index and generates detailed collision events with
 * state tracking. The resolver handles both entity-to-entity collisions and
 * entity-to-map collisions, providing enter/leave/stay state transitions for
 * game logic.
 *
 * ARCHITECTURE OVERVIEW
 * =====================
 *
 * The collision resolver operates on spatial pairs and produces collision hits:
 * - Input: Spatial pairs from spatial index (broad-phase candidates)
 * - Processing: State tracking, component dispatch, hit generation
 * - Output: Array of collision hits with detailed information
 *
 * Key Components:
 * - CollisionResolver: Main container with hit storage and state tracking
 * - State Machine: Tracks enter/leave/stay transitions between frames
 * - Component Dispatch: Routes collision testing to appropriate components
 * - Hit Generation: Creates detailed collision information for game logic
 *
 * HOW IT WORKS
 * ============
 *
 * 1. PAIR PROCESSING:
 *    - Process each spatial pair from the spatial index
 *    - Generate canonical collision key for state tracking
 *    - Check previous collision state for transitions
 *
 * 2. BROAD-PHASE FILTERING:
 *    - Use AABB intersection for cheap overlap detection
 *    - Skip expensive component testing for non-overlapping pairs
 *    - Determine collision state based on overlap and history
 *
 * 3. NARROW-PHASE TESTING:
 *    - Dispatch to entity collision testing for overlapping pairs
 *    - Test specific component combinations (collider vs collider, collider vs
 * map)
 *    - Generate detailed collision hits with contact information
 *
 * 4. STATE TRACKING:
 *    - Track collision states between frames
 *    - Generate enter/leave/stay events for game logic
 *    - Maintain collision history for next frame processing
 *
 * STEP-BY-STEP EXAMPLES
 * =====================
 *
 * Example 1: Entity Collision Detection
 * -------------------------------------
 *
 * Given: Two entities A and B with collider components
 *        Previous state: Not colliding
 *
 * Step 1: Generate collision key
 *   - Entity A ID: "entity-123"
 *   - Entity B ID: "entity-456"
 *   - Canonical key: "entity-123|entity-456" (sorted alphabetically)
 *
 * Step 2: Check previous state
 *   - Look up key in previous_collisions hashmap
 *   - Result: NULL (not colliding in previous frame)
 *
 * Step 3: AABB overlap test
 *   - Entity A bounds: (100, 50, 64, 32)
 *   - Entity B bounds: (120, 60, 48, 24)
 *   - Overlap: true (rectangles intersect)
 *
 * Step 4: Component dispatch
 *   - Call entity_test_collision(A, B, hits_array)
 *   - Collider components test detailed collision
 *   - Result: Collision detected with contact point (140, 70)
 *
 * Step 5: State determination
 *   - Currently colliding: true
 *   - Was colliding: false
 *   - State: COLLISION_STATE_ENTER
 *
 * Step 6: Hit generation
 *   - Create EseCollisionHit with:
 *     - Kind: COLLISION_KIND_COLLIDER
 *     - State: COLLISION_STATE_ENTER
 *     - Entity: A, Target: B
 *     - Contact point: (140, 70)
 *
 * Example 2: Map Collision Detection
 * ---------------------------------
 *
 * Given: Entity with collider + Map entity
 *        Previous state: Colliding
 *
 * Step 1: Component analysis
 *   - Entity A: Has collider component with map_interaction=true
 *   - Entity B: Has map component
 *   - Collision type: Map collision
 *
 * Step 2: AABB overlap test
 *   - Entity bounds: (200, 100, 32, 32)
 *   - Map bounds: (180, 80, 128, 128)
 *   - Overlap: true
 *
 * Step 3: Map collision testing
 *   - Call entity_test_collision with map-specific logic
 *   - Test against map tiles and collision layers
 *   - Result: Collision with tile at (192, 96)
 *
 * Step 4: State determination
 *   - Currently colliding: true
 *   - Was colliding: true
 *   - State: COLLISION_STATE_STAY
 *
 * Step 5: Hit generation
 *   - Create EseCollisionHit with:
 *     - Kind: COLLISION_KIND_MAP
 *     - State: COLLISION_STATE_STAY
 *     - Entity: collider, Target: map
 *     - Tile coordinates: (192, 96)
 *
 * Example 3: Collision Leave Event
 * --------------------------------
 *
 * Given: Two entities that were colliding, now separated
 *        Previous state: Colliding
 *
 * Step 1: AABB overlap test
 *   - Entity A bounds: (100, 50, 64, 32)
 *   - Entity B bounds: (200, 150, 48, 24)
 *   - Overlap: false (entities moved apart)
 *
 * Step 2: State determination
 *   - Currently colliding: false
 *   - Was colliding: true
 *   - State: COLLISION_STATE_LEAVE
 *
 * Step 3: Leave hit generation
 *   - Create minimal EseCollisionHit with:
 *     - Kind: Based on component types
 *     - State: COLLISION_STATE_LEAVE
 *     - Entity/Target: Original pair
 *     - No detailed contact information needed
 *
 * Example 4: State Transition Matrix
 * ---------------------------------
 *
 * Previous | Current | State Transition | Hit Generated
 * ---------|---------|------------------|---------------
 * None     | None    | NONE            | No
 * None     | Collide | ENTER           | Yes (detailed)
 * Collide  | Collide | STAY            | Yes (detailed)
 * Collide  | None    | LEAVE           | Yes (minimal)
 *
 * PERFORMANCE CHARACTERISTICS
 * ===========================
 *
 * Time Complexity:
 * - Per pair: O(1) for AABB test + O(k) for component testing
 * - Total: O(n + k) where n=pairs, k=detailed collision tests
 * - State lookup: O(1) average case with hashmap
 *
 * Space Complexity:
 * - Hit storage: O(k) where k=active collision pairs
 * - State tracking: O(n) where n=unique entity pairs
 * - Temporary arrays: O(1) per pair (reused)
 *
 * Memory Layout:
 * - CollisionResolver: 24 bytes + hashmap overhead
 * - EseCollisionHit: Variable based on collision type
 * - State tracking: ~16 bytes per unique pair
 *
 * OPTIMIZATION FEATURES
 * =====================
 *
 * 1. AABB prefiltering: Skip expensive component tests for non-overlapping
 * pairs
 * 2. State caching: Track collision history to avoid redundant testing
 * 3. Canonical keys: Prevent duplicate collision pairs using sorted entity IDs
 * 4. Component dispatch: Route to appropriate collision testing based on
 * component types
 * 5. Hit reuse: Transfer ownership of detailed hits to avoid copying
 * 6. Minimal leave events: Generate lightweight hits for collision exit events
 * 7. Profile counting: Track performance metrics for optimization
 *
 * COLLISION TYPES
 * ===============
 *
 * 1. COLLIDER vs COLLIDER:
 *    - Both entities have collider components
 *    - Test detailed shape intersection
 *    - Generate contact points and normals
 *
 * 2. COLLIDER vs MAP:
 *    - One entity has collider with map_interaction=true
 *    - Other entity has map component
 *    - Test against map tiles and collision layers
 *    - Generate tile-based collision information
 *
 * 3. MAP vs MAP:
 *    - Not supported (no collision between map entities)
 *    - Filtered out during pair generation
 *
 * USAGE PATTERNS
 * ==============
 *
 * Typical workflow:
 * 1. Create resolver with collision_resolver_create()
 * 2. Get spatial pairs from spatial index
 * 3. Process pairs with collision_resolver_solve()
 * 4. Iterate through returned collision hits
 * 5. Handle enter/leave/stay events in game logic
 * 6. Clear resolver with collision_resolver_clear() when needed
 * 7. Destroy resolver with collision_resolver_destroy() when done
 *
 * State Management:
 * - Resolver maintains collision state between frames
 * - State transitions are automatically detected
 * - Previous collision state is updated after each solve
 *
 * Thread Safety:
 * - Not thread-safe by design
 * - Single-threaded collision resolution
 * - External synchronization required for multi-threaded access
 */

#include "collision_resolver.h"
#include "core/memory_manager.h"
#include "entity/components/entity_component.h"
#include "entity/components/entity_component_collider.h"
#include "entity/components/entity_component_map.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "spatial_index.h"
#include "types/rect.h"
#include "utility/array.h"
#include "utility/hashmap.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Forward declarations to avoid pulling component headers into the public API
typedef struct EseRect EseRect;
typedef struct EseMap EseMap;

// CollisionKind and EseCollisionHit are now declared in the public header

struct CollisionResolver {
    EseArray *hits;                  // EseCollisionHit*
    EseHashMap *previous_collisions; // canonical keys seen colliding in previous solve
};

static bool _pair_involves_map_collision(EseEntity *a, EseEntity *b) {
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

    // Map collision if one side is map and the other is collider with map
    // interaction enabled
    if (a_has_map && b_has_collider && b_collider_map_interaction)
        return true;
    if (b_has_map && a_has_collider && a_collider_map_interaction)
        return true;
    return false;
}

static bool _get_map_pair_entities(EseEntity *a, EseEntity *b, EseEntity **out_collider,
                                   EseEntity **out_map) {
    if (!a || !b || !out_collider || !out_map)
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

    if (a_has_map && b_has_collider && b_collider_map_interaction) {
        *out_map = a;
        *out_collider = b;
        return true;
    }
    if (b_has_map && a_has_collider && a_collider_map_interaction) {
        *out_map = b;
        *out_collider = a;
        return true;
    }
    return false;
}

const char *_get_collision_key(EseUUID *a, EseUUID *b) {
    const char *ida = ese_uuid_get_value(a);
    const char *idb = ese_uuid_get_value(b);
    const char *first = ida;
    const char *second = idb;
    if (strcmp(ida, idb) > 0) {
        first = idb;
        second = ida;
    }
    size_t keylen = strlen(first) + 1 + strlen(second) + 1;
    char *key = memory_manager.malloc(keylen, MMTAG_ENGINE);
    snprintf(key, keylen, "%s|%s", first, second);
    return key; // NOTE: caller (hashmap_set) MUST take ownership and free later
}

CollisionResolver *collision_resolver_create(void) {
    CollisionResolver *resolver =
        memory_manager.malloc(sizeof(CollisionResolver), MMTAG_COLLISION_INDEX);
    resolver->hits = array_create(128, (void (*)(void *))ese_collision_hit_destroy);
    resolver->previous_collisions = hashmap_create((void (*)(void *))memory_manager.free);
    return resolver;
}

void collision_resolver_destroy(CollisionResolver *resolver) {
    log_assert("COLLISION_RESOLVER", resolver, "destroy called with NULL resolver");
    array_destroy(resolver->hits);
    hashmap_destroy(resolver->previous_collisions);
    memory_manager.free(resolver);
}

void collision_resolver_clear(CollisionResolver *resolver) {
    log_assert("COLLISION_RESOLVER", resolver, "clear called with NULL resolver");
    array_clear(resolver->hits);
    hashmap_clear(resolver->previous_collisions);
}

EseArray *collision_resolver_solve(CollisionResolver *resolver, EseArray *pairs,
                                   EseLuaEngine *engine) {
    log_assert("COLLISION_RESOLVER", resolver, "solve called with NULL resolver");
    log_assert("COLLISION_RESOLVER", pairs, "solve called with NULL pairs array");
    log_assert("COLLISION_RESOLVER", engine, "solve called with NULL engine");

    profile_start(PROFILE_COLLISION_RESOLVER_SECTION);
    array_clear(resolver->hits);
    EseHashMap *current_collisions = hashmap_create((void (*)(void *))memory_manager.free);
    for (size_t i = 0; i < array_size(pairs); i++) {
        SpatialPair *pair = (SpatialPair *)array_get(pairs, i);
        EseEntity *a = pair->a;
        EseEntity *b = pair->b;
        profile_start(PROFILE_ENTITY_COLLISION_DETECT);
        const char *canonical_key = _get_collision_key(a->id, b->id);
        bool was_colliding = hashmap_get(resolver->previous_collisions, canonical_key) != NULL;

        // Cheap broadphase overlap using entity world AABBs
        bool has_bounds = a->collision_world_bounds && b->collision_world_bounds;
        bool aabb_overlap =
            has_bounds ? ese_rect_intersects(a->collision_world_bounds, b->collision_world_bounds)
                       : false;

        // Prepare detailed hits only when we might need them
        EseArray *tmp_hits = NULL;
        bool currently_colliding = false;
        EseCollisionState state = COLLISION_STATE_NONE;

        if (!aabb_overlap) {
            // No overlap -> either NONE or LEAVE
            state = was_colliding ? COLLISION_STATE_LEAVE : COLLISION_STATE_NONE;

            // If it's a LEAVE state, we still need to create a hit for the exit
            // callback
            if (state == COLLISION_STATE_LEAVE) {
                EseCollisionHit *exit_hit = ese_collision_hit_create(engine);
                EseCollisionKind kind = _pair_involves_map_collision(a, b)
                                            ? COLLISION_KIND_MAP
                                            : COLLISION_KIND_COLLIDER;
                ese_collision_hit_set_kind(exit_hit, kind);
                if (kind == COLLISION_KIND_MAP) {
                    EseEntity *collider_entity = a, *map_entity = b;
                    if (_get_map_pair_entities(a, b, &collider_entity, &map_entity)) {
                        ese_collision_hit_set_entity(exit_hit, collider_entity);
                        ese_collision_hit_set_target(exit_hit, map_entity);
                    } else {
                        ese_collision_hit_set_entity(exit_hit, a);
                        ese_collision_hit_set_target(exit_hit, b);
                    }
                } else {
                    ese_collision_hit_set_entity(exit_hit, a);
                    ese_collision_hit_set_target(exit_hit, b);
                    ese_collision_hit_set_rect(exit_hit, NULL);
                }
                ese_collision_hit_set_state(exit_hit, COLLISION_STATE_LEAVE);
                if (!array_push(resolver->hits, exit_hit)) {
                    memory_manager.free(exit_hit);
                }
            }
        } else {
            // Only do expensive component-level test when AABBs overlap
            tmp_hits = array_create(4, NULL); // take ownership of hits transferred
            // Classify the pair to count map vs collider paths
            if (_pair_involves_map_collision(a, b)) {
                profile_count_add("resolver_pair_map_candidate");
            } else {
                profile_count_add("resolver_pair_collider_candidate");
            }

            profile_start(PROFILE_ENTITY_COMPONENT_DISPATCH);
            currently_colliding = entity_test_collision(a, b, tmp_hits);
            profile_stop(PROFILE_ENTITY_COMPONENT_DISPATCH, "entity_component_pair_dispatch");

            if (currently_colliding && !was_colliding) {
                state = COLLISION_STATE_ENTER;
            } else if (currently_colliding && was_colliding) {
                state = COLLISION_STATE_STAY;
            } else if (!currently_colliding && was_colliding) {
                state = COLLISION_STATE_LEAVE;
            } else {
                state = COLLISION_STATE_NONE;
            }
        }
        profile_stop(PROFILE_ENTITY_COLLISION_DETECT, "collision_resolver_detect");

        if (currently_colliding) {
            // Track for next frame - store entity pointers
            SpatialPair *pair_data =
                memory_manager.malloc(sizeof(SpatialPair), MMTAG_COLLISION_INDEX);
            pair_data->a = a;
            pair_data->b = b;
            hashmap_set(current_collisions, canonical_key, pair_data);
        }

        // Emit collision hits with computed state
        if (state != COLLISION_STATE_NONE) {
            size_t count_hits = tmp_hits ? array_size(tmp_hits) : 0;
            if (count_hits > 0) {
                // Propagate state to all detailed hits
                for (size_t hi = 0; hi < count_hits; hi++) {
                    EseCollisionHit *hit = (EseCollisionHit *)array_get(tmp_hits, hi);
                    if (ese_collision_hit_get_kind(hit) == COLLISION_KIND_MAP) {
                        profile_count_add("resolver_hits_map");
                    } else {
                        profile_count_add("resolver_hits_collider");
                    }
                    ese_collision_hit_set_state(hit, state);
                    if (!array_push(resolver->hits, hit)) {
                        memory_manager.free(hit);
                    }
                }
            } else if (state == COLLISION_STATE_LEAVE) {
                // EXIT with no detailed hits still requires a callback
                EseCollisionHit *exit_hit = ese_collision_hit_create(engine);
                EseCollisionKind kind = _pair_involves_map_collision(a, b)
                                            ? COLLISION_KIND_MAP
                                            : COLLISION_KIND_COLLIDER;
                ese_collision_hit_set_kind(exit_hit, kind);
                if (kind == COLLISION_KIND_MAP) {
                    EseEntity *collider_entity = a, *map_entity = b;
                    if (_get_map_pair_entities(a, b, &collider_entity, &map_entity)) {
                        ese_collision_hit_set_entity(exit_hit, collider_entity);
                        ese_collision_hit_set_target(exit_hit, map_entity);
                    } else {
                        ese_collision_hit_set_entity(exit_hit, a);
                        ese_collision_hit_set_target(exit_hit, b);
                    }
                } else {
                    ese_collision_hit_set_entity(exit_hit, a);
                    ese_collision_hit_set_target(exit_hit, b);
                    ese_collision_hit_set_rect(exit_hit, NULL);
                }
                ese_collision_hit_set_state(exit_hit, COLLISION_STATE_LEAVE);
                if (!array_push(resolver->hits, exit_hit)) {
                    memory_manager.free(exit_hit);
                }
            }
        } else {
            // No collision state -> free collected hits we won't return
            if (tmp_hits) {
                for (size_t hi = 0; hi < array_size(tmp_hits); hi++) {
                    EseCollisionHit *hit = (EseCollisionHit *)array_get(tmp_hits, hi);
                    memory_manager.free(hit);
                }
            }
        }

        // Destroy temporary container (elements already transferred or freed)
        if (tmp_hits)
            array_destroy(tmp_hits);
        memory_manager.free((void *)canonical_key);
    }

    // Process exit collisions for entities that were colliding but are no
    // longer in pairs
    EseHashMapIter *prev_iter = hashmap_iter_create(resolver->previous_collisions);
    const char *key;
    void *value;
    while (hashmap_iter_next(prev_iter, &key, &value)) {
        if (hashmap_get(current_collisions, key) == NULL) {
            // This pair was colliding before but is not in current pairs ->
            // EXIT
            SpatialPair *pair_data = (SpatialPair *)value;
            EseEntity *a = pair_data->a;
            EseEntity *b = pair_data->b;

            EseCollisionKind kind =
                _pair_involves_map_collision(a, b) ? COLLISION_KIND_MAP : COLLISION_KIND_COLLIDER;
            EseCollisionHit *exit_hit = ese_collision_hit_create(engine);
            ese_collision_hit_set_kind(exit_hit, kind);
            if (kind == COLLISION_KIND_MAP) {
                EseEntity *collider_entity = a, *map_entity = b;
                if (_get_map_pair_entities(a, b, &collider_entity, &map_entity)) {
                    ese_collision_hit_set_entity(exit_hit, collider_entity);
                    ese_collision_hit_set_target(exit_hit, map_entity);
                } else {
                    ese_collision_hit_set_entity(exit_hit, a);
                    ese_collision_hit_set_target(exit_hit, b);
                }
            } else {
                ese_collision_hit_set_entity(exit_hit, a);
                ese_collision_hit_set_target(exit_hit, b);
                ese_collision_hit_set_rect(exit_hit, NULL);
            }
            ese_collision_hit_set_state(exit_hit, COLLISION_STATE_LEAVE);
            if (!array_push(resolver->hits, exit_hit)) {
                memory_manager.free(exit_hit);
            }
        }
    }
    hashmap_iter_free(prev_iter);

    // Roll current into previous for next call
    EseHashMap *old_prev = resolver->previous_collisions;
    resolver->previous_collisions = current_collisions;
    hashmap_destroy(old_prev);
    profile_stop(PROFILE_COLLISION_RESOLVER_SECTION, "collision_resolver_solve");
    return resolver->hits;
}
