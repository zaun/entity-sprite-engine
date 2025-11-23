
# Problem statement
The current engine runs the collision pipeline directly in `engine_update`:
* After EARLY and LUA systems, it clears the global `SpatialIndex`,
* Loops all active entities and calls `spatial_index_insert(engine->spatial_index, entity)`,
* Calls `spatial_index_get_pairs` to get broad‑phase pairs,
* Calls `collision_resolver_solve` to run narrow‑phase and state tracking,
* Iterates resulting hits and calls `entity_process_collision_callbacks`.
This is all single‑threaded on the main thread, and insertion is driven by an explicit per‑frame loop over `engine->entities`.
Goals:
* Move spatial index usage and collision resolution into a dedicated **collision system**.
* Drive which entities participate in collisions via `on_component_added` / `on_component_removed` (system manager), not a manual `for` loop in `engine_update`.
* “Fully parallelize” the heavy parts of the collision pipeline using the existing job queue, while keeping Lua callbacks on the main thread.
* Preserve current collision behaviour (collider↔collider and collider↔map, enter/stay/leave, state tracking, etc.).
# Current architecture (relevant pieces)
## Engine update and collision pipeline
* `engine_update` (in `core/engine.c`) currently:
    * Runs systems in phases:
        * `engine_run_phase(engine, SYS_PHASE_EARLY, dt, parallel = true)`
        * `engine_run_phase(engine, SYS_PHASE_LUA, dt, parallel = false)`
    * Then runs the collision pipeline inline:
        * `spatial_index_clear(engine->spatial_index);`
        * Iterate `engine->entities` and call `spatial_index_insert` for each active entity.
        * `spatial_index_get_pairs` → `EseArray *spatial_pairs`.
        * `collision_resolver_solve(engine->collision_resolver, spatial_pairs, engine->lua_engine)`.
        * Loop collision hits and call `entity_process_collision_callbacks(hit)`.
    * Then runs LATE systems and the rest of rendering/GUI.
* Systems phases are documented in `docs/Systems Architecture Overview.md`:
    * `SYS_PHASE_EARLY` and `SYS_PHASE_LATE` are run in parallel via the job queue.
    * `SYS_PHASE_LUA` and `SYS_PHASE_CLEANUP` are single‑threaded.
## Spatial index
* Implemented in `utility/spatial_index.[ch]`.
* Holds:
    * A grid of bins in an `EseIntHashMap` keyed by `(cell_x, cell_y)` → `EseDoubleLinkedList*` of `EseEntity*`.
    * Optional DBVH regions for dense areas.
    * An internal `EseArray *pairs` of `SpatialPair* { EseEntity *a, *b; }`.
* Key APIs:
    * `spatial_index_clear(SpatialIndex *index)` – clears bins, DBVH regions, and pairs.
    * `spatial_index_insert(SpatialIndex *index, EseEntity *entity)` – inserts an entity into the overlapping grid cells (if active and with `collision_world_bounds`).
    * `spatial_index_get_pairs(SpatialIndex *index)` – converts dense cells to DBVH regions, then generates deduplicated candidate pairs using component‑based filtering and AABB tests.
* Threading: explicitly **not thread‑safe**; designed to be used from a single thread per frame.
## Collision resolver
* Implemented in `core/collision_resolver.[ch]`.
* Holds:
    * `EseArray *hits` – array of `EseCollisionHit*`.
    * `EseHashMap *previous_collisions` – canonical collision keys (`"idA|idB"`) for state tracking.
* `collision_resolver_solve(CollisionResolver *resolver, EseArray *pairs, EseLuaEngine *engine)`:
    * Iterates `SpatialPair*` from the spatial index.
    * Uses entity AABBs for broad‑phase filtering.
    * Uses `entity_test_collision(a, b, tmp_hits)` for narrow‑phase (collider↔collider, collider↔map).
    * Computes state (ENTER, STAY, LEAVE, NONE) using `previous_collisions`.
    * Populates `resolver->hits` with `EseCollisionHit*` and updates `previous_collisions` for the next frame.
    * Returns `resolver->hits`.
* Not thread‑safe; designed to run on a single thread.
## Collider component and system
* Collider component: `entity_component_collider.[ch]`.
    * Manages one or more `EseRect*` shapes plus an offset and flags (e.g., `map_interaction`).
    * Computes `entity->collision_bounds` and `entity->collision_world_bounds` via `entity_component_collider_update_bounds`, which is called when rects or positions change.
* Collider system: `entity/systems/collider_system.c`.
    * System with `accepts = ENTITY_COMPONENT_COLLIDER`.
    * `on_component_added`/`on_component_removed` maintain an internal array of `EseEntityComponentCollider*`.
    * `update` walks that array and recomputes `collision_world_bounds` from `collision_bounds` and `entity->position` for active entities.
## System manager and job queue
* `core/system_manager.[ch]`:
    * Systems provide a vtable: `init`, `update`, `accepts`, `on_component_added`, `on_component_removed`, `shutdown`, `apply_result`.
    * `engine_run_phase(..., parallel = true)` pushes one job per system to the engine’s `job_queue`.
    * `update` runs on worker threads; `apply_result` (if non‑NULL) runs on the main thread when the job completes.
    * `update` returns a `JobResult` (`EseSystemJobResult` alias). If `result` is non‑NULL, the framework passes it to `apply_result` then frees it.
* `utility/job_queue.[ch]` implements the worker pool and callback flow.
# High‑level target design
Introduce a dedicated **CollisionSystem** that owns the entire collision pipeline from “collect candidate entities” through “produce collision hits”, and uses the system manager + job queue for parallel execution.
Key properties:
* **Component‑driven membership**:
    * The system tracks only entities that have collision‑relevant components (collider and/or map) via `on_component_added`/`on_component_removed`.
    * No manual iteration over `engine->entities` in `engine_update`.
* **System‑based lifecycle**:
    * Collision work is performed in a system’s `update` callback, scheduled in a **parallel phase** via the job queue.
    * Lua callbacks stay on the main thread via `apply_result`.
* **Incremental parallelization**:
    * Phase 1: Run the whole collision pipeline on a single worker thread (parallel with respect to the main thread and other systems).
    * Phase 2: Internally split the work across multiple jobs (e.g., pair ranges) using the job queue, while preserving resolver state semantics.
# CollisionSystem API and placement
## System registration
* New files under `src/entity/systems/`:
    * `collision_system.h`
    * `collision_system.c`
* Public API:
    * `EseSystemManager *collision_system_create(void);`
    * `void engine_register_collision_system(EseEngine *eng);`
* `engine_create` will register this system along with existing systems, ideally after collider and map systems:
    * `engine_register_collider_system(engine);`
    * `engine_register_map_system(engine);`
    * **`engine_register_collision_system(engine);`**
## Phase choice
Two viable options:
1. **New dedicated phase (preferred for clarity)**
    * Extend `EseSystemPhase` with `SYS_PHASE_COLLISION` between LUA and LATE:
        * Ordering in `engine_update`:
        1. EARLY (parallel)
        2. LUA (single‑threaded)
        3. **COLLISION (parallel)** – `engine_run_phase(engine, SYS_PHASE_COLLISION, dt, true)`
        4. LATE (parallel)
        5. CLEANUP (single‑threaded)
    * Only `CollisionSystem` (and any future collision‑adjacent systems) live in this phase.
2. **Reuse an existing phase**
    * Put `CollisionSystem` into `SYS_PHASE_LATE` and rely on ordering of LATE systems to ensure it runs **before** render systems.
    * This requires either:
        * A convention that collision system is registered before render systems and that LATE systems are processed in registration order, or
        * A more explicit priority mechanism.
For a clean design that closely matches the current high‑level flow (collisions between LUA and rendering), the **new `SYS_PHASE_COLLISION` phase** is the better conceptual fit.
# CollisionSystem internal state
Define an internal data struct similar to `ColliderSystemData` but focused on entities rather than individual components.
Conceptually:
* `CollisionSystemData` contains:
*     * `EseEntityComponentCollider **colliders; size_t collider_count, collider_cap;`
*     * `EseEntityComponentMap **maps; size_t map_count, map_cap;`
*     * Optional per‑entity cached flags and ref‑counts so we never need a per‑frame scratch set:
*        * e.g. a small map `EseHashMap *entity_flags` keyed by `EseEntity*` or `EseUUID*` → struct `{ uint32_t ref_count; bool has_collider; bool has_map; bool collider_map_interaction; }`.
*     * A persistent `EseArray *candidate_entities` of `EseEntity*` that participate in collisions. This array is updated incrementally when components are added/removed:
*        * When the collision‑relevant `ref_count` for an entity goes from 0 → 1, we append the entity to `candidate_entities`.
*        * When `ref_count` goes from 1 → 0, we remove the entity from `candidate_entities` by swap‑with‑last.
*        * When `ref_count` is >1, we leave `candidate_entities` unchanged (the entity is already present).
*     * A pointer to the engine’s shared `SpatialIndex` and `CollisionResolver` (these remain owned by `EseEngine` but used by the system).
Tracking by component rather than entity aligns with how other systems work and plugs into `on_component_added` / `on_component_removed` naturally. The per‑entity `ref_count` ensures each entity appears at most once in `candidate_entities` without rebuilding that array every frame.
# Component tracking via accepts/add/remove
## accepts
* `collision_sys_accepts(self, comp)` returns `true` for:
    * `ENTITY_COMPONENT_COLLIDER`
    * `ENTITY_COMPONENT_MAP`
* Everything else is ignored.
## on_component_added
* If `comp->type == ENTITY_COMPONENT_COLLIDER`:
    * Append `comp->data` (an `EseEntityComponentCollider*`) to `d->colliders`.
    * If caching per‑entity flags, update the entry for `comp->entity`:
        * `has_collider = true`.
        * `collider_map_interaction = col->map_interaction`.
    * Ensure any collider‑dependent world bounds are up‑to‑date:
        * Typically already handled by collider component + collider system; here we only rely on `entity->collision_world_bounds`.
* If `comp->type == ENTITY_COMPONENT_MAP`:
    * Append `comp->data` (e.g. `EseEntityComponentMap*`) to `d->maps`.
    * Set `has_map = true` for the owning entity in `entity_flags`.
## on_component_removed
* Symmetric to `on_component_added`:
    * Find the component pointer in the relevant array and remove it by swap‑with‑last.
    * Update per‑entity flags:
        * If the entity has no remaining collider components, clear `has_collider`/`collider_map_interaction`.
        * If the entity has no remaining map components, clear `has_map`.
    * On entity destruction, the cleanup system already calls `engine_notify_comp_rem` for each component, so `CollisionSystem` will drop references before the entity is freed.
This eliminates the need to scan `engine->entities` to discover potential collision participants; the system maintains a compact, collision‑relevant subset.
# Per‑frame update flow inside CollisionSystem
## Overview
`collision_sys_update(self, eng, dt)` (running on a worker thread in the COLLISION phase) should:
1. Build the per‑frame candidate entity list from tracked components.
2. Clear and repopulate the spatial index from those candidates.
3. Generate broad‑phase pairs via `spatial_index_get_pairs`.
4. Run `collision_resolver_solve` to produce collision hits.
5. Package the resulting hits into a `JobResult` so they can be applied on the main thread.
### 1. Build candidate_entities
* Rely on the persistent `candidate_entities` array maintained by `on_component_added` / `on_component_removed` and the per‑entity `ref_count`:
*    * `ref_count` is the number of collision‑relevant components (colliders + maps) on that entity.
*    * When a relevant component is added and `ref_count` transitions 0 → 1, we append the entity to `candidate_entities`.
*    * When a relevant component is removed and `ref_count` transitions 1 → 0, we remove the entity from `candidate_entities` via swap‑with‑last.
*    * For transitions 1 → 2, 2 → 3, etc., `candidate_entities` is unchanged; the entity is already present.
* In `collision_sys_update` we simply iterate `candidate_entities` directly; no per‑frame scratch hash map or pointer set is needed.
Notes:
* Because `ref_count` is maintained incrementally with component lifecycle, each entity appears at most once in `candidate_entities` across frames.
* This guarantees we insert each entity into the spatial index at most once per frame, even if it has multiple colliders/maps.
### 2. Clear and insert into SpatialIndex
* Call `spatial_index_clear(eng->spatial_index)`.
* For each `e` in `candidate_entities`:
*    * Skip if `!e->active` or `e->destroyed` (matches the existing engine semantics).
*    * Require `e->collision_world_bounds` to be non‑NULL; if NULL, skip.
*    * Call `spatial_index_insert(eng->spatial_index, e)`.
This replaces the current `engine_update` loop that iterates `engine->entities`. The main difference is that we only touch entities that are known to be collision‑relevant, while still honoring `active`/`destroyed` flags.
### 3. Generate broad‑phase pairs
* Call `EseArray *pairs = spatial_index_get_pairs(eng->spatial_index);`
    * This returns `SpatialPair*` owned by the index.
    * No deallocation needed by the system.
### 4. Run collision resolver
* Call `EseArray *hits = collision_resolver_solve(eng->collision_resolver, pairs, eng->lua_engine);`
    * This returns `EseCollisionHit*` owned by the resolver instance.
    * `hits` remains valid until the next `collision_resolver_clear` / `collision_resolver_solve` call.
### 5. Prepare JobResult
* The job queue will free any `result` buffer given in `JobResult`. We should not directly hand out the resolver’s internal pointers.
* Instead, in `collision_sys_update`:
    * Allocate a small POD struct, e.g.:
        * `typedef struct { EseCollisionHit **hits; size_t count; } CollisionJobResult;`
    * Clone only the **pointers to hits** into a new `hits` array (not copying the hits themselves):
        * `hits_out[i] = (EseCollisionHit *)array_get(hits, i);`
    * Fill `JobResult` with:
        * `result = collision_job_result_ptr;`
        * `size = sizeof(*collision_job_result) + count * sizeof(EseCollisionHit*);` (or fixed struct + a second allocation for the pointer array).
        * `copy_fn = NULL` (we already allocated in worker; main thread can use it as‑is).
        * `free_fn = NULL` (we want the system’s `apply_result` to free `result` and the pointer array explicitly).
* Return this `JobResult` from `collision_sys_update`.
# Main‑thread application of results
Implement `collision_sys_apply_result(self, eng, void *result)` to run on the main thread after the job completes.
Steps:
1. Cast `result` back to `CollisionJobResult *`.
2. Iterate `hits[0..count)` and call `entity_process_collision_callbacks(hit)`.
    * This preserves the current semantics where Lua callbacks run on the main thread.
    * Since `collision_resolver_solve` has already set the correct `CollisionKind` and `CollisionState` on each hit, callbacks behave the same as today.
3. Free the `hits` pointer array and the `CollisionJobResult` struct.
This use of `apply_result` keeps **all** potentially unsafe main‑thread work (Lua calls, entity mutation via scripts) off the worker threads while moving the heavy math (broad‑phase + resolver) onto a worker.
# Removing collision code from engine_update
Once `CollisionSystem` is in place, `engine_update` can be simplified:
* Remove the entire block that:
    * Clears the spatial index.
    * Iterates entities and inserts them.
    * Calls `spatial_index_get_pairs`.
    * Calls `collision_resolver_solve`.
    * Iterates hits and calls `entity_process_collision_callbacks`.
* Instead, insert a phase run call:
    * If introducing `SYS_PHASE_COLLISION`:
        * `engine_run_phase(engine, SYS_PHASE_COLLISION, delta_time, true);`
    * The rest of the pipeline (LATE systems, GUI, renderer flip, CLEANUP, entity deletion) remains unchanged.
Behaviourally, the main difference is **where** and **on which thread** the collision work happens, not *when* in the frame it happens.
# Parallelization strategy beyond a single system job
The above design already:
* Moves the collision heavy work to a worker thread (via the system manager / job queue), and
* Runs it in parallel with no other main‑thread tasks during that phase (engine waits for the job to finish before proceeding),
* While keeping Lua callbacks on the main thread.
If you want to further exploit multiple cores **within** the collision phase, you can extend the design in stages:
## Stage 1: Parallelize narrow‑phase over pairs
1. Keep spatial index insertion and `spatial_index_get_pairs` single‑threaded inside `collision_sys_update` (avoids making `SpatialIndex` thread‑safe).
2. Once you have `pairs` and before calling `collision_resolver_solve`, split the `SpatialPair*` array into N ranges.
3. Use the engine’s `job_queue` directly from inside `collision_sys_update` to schedule N worker jobs, each processing a slice of pairs.
4. Each job uses a **thread‑local resolver shard** that:
    * Reads from a shared `previous_collisions` snapshot (taken at the start of the frame).
    * Emits hits and a “currently colliding” set for its slice.
5. After all jobs finish:
    * Merge hit arrays into a single array for the main `CollisionResolver`.
    * Merge “currently colliding” sets into one map, then compute final states vs the old `previous_collisions`.
This likely requires refactoring `collision_resolver.c` to:
* Separate “stateful previous‑collision tracking” from “stateless collision testing for a pair slice”.
* Make the stateless part reusable by worker jobs.
## Stage 2: Parallelize spatial pair generation
If profiling shows `spatial_index_get_pairs` is a bottleneck:
1. Introduce **per‑thread local pair buffers** and a final merge step:
    * Each job processes a disjoint subset of bins or DBVH regions.
    * Each job uses a thread‑local `seen` map for deduplication within its subset.
    * After jobs finish, merge local pair arrays and reconcile duplicates (e.g., canonicalize by entity ID and deduplicate in a final pass).
2. Carefully partition work so that neighbor‑cell checks do not overlap between jobs, or ensure that deduplication at the merge stage is correct.
3. Keep all writes to the underlying `SpatialIndex` (bins, DBVH trees) single‑threaded; only **reading** from it in parallel when generating pairs.
This keeps `SpatialIndex` structurally single‑writer, multi‑reader, but lets you fan out pair generation.
# Interaction with existing systems and invariants
* **ColliderSystem**:
    * Already maintains world bounds for colliders based on entity position.
    * CollisionSystem relies on accurate `entity->collision_world_bounds` and does not recompute them.
    * The ordering should ensure ColliderSystem (EARLY) runs before CollisionSystem (COLLISION).
* **MapSystem**:
    * Responsible for creating/updating map‑related collision geometry and bounds.
    * CollisionSystem only cares that map entities expose `collision_world_bounds` and have `ENTITY_COMPONENT_MAP`.
* **CleanupSystem and entity destruction**:
    * CleanupSystem still defers component destroy to CLEANUP.
    * CollisionSystem drops component references on `on_component_removed`, so it will not hold stale pointers after CLEANUP.
    * The documented invariants (no component destruction while EARLY / LATE / COLLISION phase jobs are running) remain intact.
* **Lua and thread safety**:
    * All Lua interactions remain on the main thread inside `entity_process_collision_callbacks`.
    * Worker threads in CollisionSystem only read `EseEntity` fields (positions, bounds, component pointers) and write to `SpatialIndex` / `CollisionResolver` internals; no direct Lua API calls.
# Summary
Designing a dedicated CollisionSystem built on the existing system manager and job queue lets you:
* Remove the explicit per‑frame "Insert active entities" loop and other collision steps from `engine_update`.
* Base collision participation purely on `on_component_added` / `on_component_removed` for collider and map components.
* Move the heavy collision work onto a worker thread (via a parallel system phase), preserving the current behaviour of Lua collision callbacks.
* Incrementally evolve toward finer‑grained parallelization of narrow‑phase and pair generation if profiling justifies the extra complexity.
