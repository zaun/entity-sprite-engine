#ifndef ESE_COLLISION_RESOLVER_H
#define ESE_COLLISION_RESOLVER_H

#include <stddef.h>
#include "utility/array.h"
#include "types/collision_hit.h"

// Forward declarations (avoid heavy includes)
typedef struct EseEntity EseEntity;
typedef struct EseRect EseRect;
typedef struct EseMap EseMap;
typedef struct EseLuaEngine EseLuaEngine;

// Pair produced by spatial_index
typedef struct SpatialPair SpatialPair;


// Resolve spatial pairs into collision hits
// Returns an EseArray of EseCollisionHit* allocated via memory_manager and owned by the resolver instance.
// The caller must not free the returned array or its elements.
typedef struct CollisionResolver CollisionResolver;

CollisionResolver *collision_resolver_create(void);
void collision_resolver_destroy(CollisionResolver *resolver);
void collision_resolver_clear(CollisionResolver *resolver);

// Input is an array of SpatialPair* (from spatial_index_get_pairs)
EseArray *collision_resolver_solve(CollisionResolver *resolver, EseArray *pairs, EseLuaEngine *engine);

#endif // ESE_COLLISION_RESOLVER_H


