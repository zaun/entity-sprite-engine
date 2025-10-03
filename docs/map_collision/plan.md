### Map ↔ Entity collision: implementation plan

This plan documents how to add entity–map collision detection using existing collider rectangles and map cells, with optional ENTER/STAY/EXIT state and Lua callbacks.

### Goals
- Detect overlaps between any entity collider rect and cells of any `EntityComponentMap` in `engine->map_components`.
- Support an opt-in switch on each collider (`map_interaction`) to include/exclude it from map collisions.
- Optionally track per-frame state (ENTER/STAY/EXIT) similar to entity↔entity collisions.
- Dispatch Lua callbacks on entities for map collisions.
- Keep runtime cost reasonable and avoid per-frame churn.

### Terminology and assumptions
- `engine->map_components` is maintained in real time (single source of truth). Do NOT clear/rebuild per frame.
- `entity.position` is world-space; collider rects are local to the entity and offset by `collider.offset`.
- Map cell world rectangles come from `entity_component_map_get_cell_rect(map_comp, x, y)`; `entity_component_map_cell_intersect(map_comp, rect, cap, out_cells)` returns any intersecting cells.
- A “solid” cell is determined by a flag on `EseMapCell` (see Constants).

### Constants
- Define a bit for solidity (choose any available bit that fits your usage):
  - Suggested: `#define MAP_CELL_FLAG_SOLID (1u << 0)`
  - Placement: add near flag APIs in `types/map_cell.h` (keeps it close to cell semantics).

### Public API surface
- Add a small public wrapper so the core can call a stable symbol without reaching into internals:
  - In `entity.h`:
    - `void entity_update_map_collisions(EseEntity *entity, const EseArray *map_components);`
  - In `entity.c`:
    - Implement the wrapper and delegate to an internal helper (below).

### Internal helper (bulk of the work)
- Implement in `entity_private.c` as an internal function (underscore prefix):
  - `_entity_test_map_collision(EseEntity *entity, const EseArray *map_components)`
  - Responsibilities:
    - Iterate active components; filter to `ENTITY_COMPONENT_COLLIDER` with `map_interaction == true`.
    - For each collider rect:
      - Compute world rect = entity position + collider offset + rect local.
      - For each `EseEntityComponentMap *` in `map_components` (skip inactive ones):
        - Call `entity_component_map_cell_intersect(map_comp, world_rect, cap, out_cells)`.
        - Filter hits by solidity rule: `ese_map_cell_has_flag(cell, MAP_CELL_FLAG_SOLID)` (or game-specific predicate).
        - Emit callbacks or update state (see below).

### Engine loop integration
- In `engine_update` PASS TWO, after entity↔entity collision callbacks and before PASS THREE (draw):
  - Iterate active entities and call the public wrapper:
    - `entity_update_map_collisions(entity, engine->map_components);`
  - Rationale: ordering mirrors entity↔entity collisions and ensures gameplay responses happen before draw.

### Optional: ENTER/STAY/EXIT state for map collisions
- To mirror entity-pair state, maintain per-entity sets for map cells collided with:
  - Data structures on `EseEntity` (suggestion):
    - `EseIntHashMap *current_map_collisions;`
    - `EseIntHashMap *previous_map_collisions;`
  - Keying approach:
    - Use a 64-bit key that combines map pointer and cell pointer for uniqueness across multiple maps:
      - `uint64_t key = ((uint64_t)(uintptr_t)map_ptr << 32) ^ (uint64_t)(uintptr_t)cell_ptr;`
  - Per-frame lifecycle:
    - At frame start (with the existing collision-state clear pass), swap `current_map_collisions` and `previous_map_collisions`, then clear the new `current_map_collisions`.
    - During `_entity_test_map_collision`, when a solid cell is detected:
      - Compute key and check membership in `previous_map_collisions`.
      - Determine state:
        - Not in previous → ENTER
        - In previous → STAY
      - Set key in `current_map_collisions`.
    - After all checks, any keys present in `previous_map_collisions` but not in `current_map_collisions` imply EXIT; dispatch exits accordingly or process exits as you discover absences.

### Lua callbacks
- On the colliding entity, support any/all of:
  - `ENTITY:entity_map_collision_enter(cell)`
  - `ENTITY:entity_map_collision_stay(cell)`
  - `ENTITY:entity_map_collision_exit(cell)`
  - Optional simpler path: `ENTITY:entity_map_collision(cell)` (fire on any hit without state).
- Passing the cell to Lua:
  - Push the `EseMapCell` as Lua userdata using the existing MapCell Lua helpers, or wrap into an `EseLuaValue` via the project’s value helpers.
  - Prefer reusing existing `EseLuaValue` creation helpers (no manual `EseLuaValue argv[1]`).

### Performance considerations
- v1 can call `entity_component_map_cell_intersect` (full scan); acceptable for small maps or low entity counts.
- Fast-path improvements (later):
  - Compute the cell range from world rect and only test those indices.
  - Add a simple spatial hash for dynamic cells.
  - Early-out once a collision is found if only a boolean is needed.
- Minimize transient allocations (reuse a stack or preallocated rect if style permits; otherwise, use designated API and ensure destruction via memory manager).

### Edge cases
- Entities without collider components or with all colliders `map_interaction == false` should be skipped.
- Maps with no active layers/hidden tiles still have cells; solidity should rely on `flags`, not presence of graphics.
- Multiple maps can overlap in world-space; treat each map independently (keys include the map pointer).
- Destroyed entities/maps mid-frame: guard with activity checks before processing.

### Testing outline (when requested)
- Minimal C tests (follow existing style):
  - Entity with a single collider rect overlapping a solid cell returns a hit.
  - Non-solid cell: no hit.
  - ENTER → STAY → EXIT across frames with key bookkeeping.
  - Multiple colliders & multiple maps.
- Lua callback smoke test: define `entity_map_collision_enter` and verify it runs.

### Step-by-step implementation checklist
1) Define `MAP_CELL_FLAG_SOLID` in `types/map_cell.h` (or your preferred constants header).
2) Add public wrapper declaration to `entity.h`: `entity_update_map_collisions`.
3) Implement public wrapper in `entity.c` to call `_entity_test_map_collision`.
4) Implement `_entity_test_map_collision` in `entity_private.c`.
5) Hook the wrapper in `engine_update` PASS TWO (after entity↔entity callbacks).
6) (Optional) Add `previous_map_collisions` / `current_map_collisions` to `EseEntity` and swap/clear alongside existing collision-state pass.
7) (Optional) Implement ENTER/STAY/EXIT and Lua dispatch for map collisions.
8) (Optional) Add a debug toggle to draw overlapped cells.
9) (Optional) Optimize cell intersection.

### Pseudocode reference
```c
// engine_update, PASS TWO (after entity-entity callbacks)
EseDListIter *it = dlist_iter_create(engine->entities);
void *v;
while (dlist_iter_next(it, &v)) {
    EseEntity *e = (EseEntity*)v;
    if (!e->active) continue;
    entity_update_map_collisions(e, engine->map_components);
}
dlist_iter_free(it);
```

```c
// entity.c
void entity_update_map_collisions(EseEntity *entity, const EseArray *map_components) {
    _entity_test_map_collision(entity, map_components);
}
```

```c
// entity_private.c (sketch)
static void _entity_test_map_collision(EseEntity *entity, const EseArray *map_components) {
    // for each collider rect (world) and for each map component
    // call entity_component_map_cell_intersect(...)
    // filter by MAP_CELL_FLAG_SOLID
    // update ENTER/STAY/EXIT or call a simple Lua callback
}
```

### Rollback / safety
- The integration is additive; if needed, the engine call site can be toggled behind a feature flag while keeping the helper compiled.


