## System architecture overview

- **Systems** are instances of `EseSystemManager` with a vtable:
  - `init(self, eng)`
  - `update(self, eng, dt)`
  - `accepts(self, comp)`
  - `on_component_added(self, eng, comp)`
  - `on_component_removed(self, eng, comp)`
  - `shutdown(self, eng)`
- Systems are grouped into **phases**:
  - `SYS_PHASE_EARLY` – parallel, before Lua/entity update.
  - `SYS_PHASE_LUA` – single-threaded, for Lua-driven systems.
  - `SYS_PHASE_LATE` – parallel, after Lua and entity draw, before GUI/console.
  - `SYS_PHASE_CLEANUP` – single-threaded, after everything else (deferred cleanup).

Systems usually maintain their own **internal collections** of components (e.g. arrays of pointers) populated via `on_component_added` / `on_component_removed`.

---

## Per-frame execution order (high level)

Within `engine_update(engine, delta_time, state)` a single frame proceeds broadly as:

1. **EARLY systems (parallel)**
   - `engine_run_phase(engine, SYS_PHASE_EARLY, dt, parallel = true)`
   - Each matching system runs `vt->update` on a worker thread via the job queue.

2. **Entities – main update (single-threaded)**
   - Iterate all active entities and call `entity_component_update` for each active component.

3. **LUA systems (single-threaded)**
   - `engine_run_phase(engine, SYS_PHASE_LUA, dt, parallel = false)`

4. **Collision pipeline & callback dispatch** (single-threaded)
   - Build spatial pairs, resolve collisions, run Lua callbacks.

5. **Entity draw pass** (single-threaded, non-system)  
   - Clear draw list, iterate entities, call `entity_draw`, which dispatches to `entity_component_draw`.

6. **LATE systems (parallel)**
   - `engine_run_phase(engine, SYS_PHASE_LATE, dt, parallel = true)`
   - Render systems (sprite, shape, map, text, collider debug) run here on worker threads and write directly into `EseDrawList`.

7. **GUI and console drawing** (single-threaded)
   - GUI processes input, populates draw list; console may draw on top.

8. **Renderer flip**
   - Convert draw list → render list, flip active render list; renderer uses the new list.

9. **Async job callbacks**
   - `ese_job_queue_process(engine->job_queue)` on main thread.

10. **Lua GC**

11. **CLEANUP systems (single-threaded)**
    - `engine_run_phase(engine, SYS_PHASE_CLEANUP, dt, parallel = false)`
    - Typically includes the **cleanup system** that processes deferred component removals.

12. **Entity deletion (hard destroy)**
    - Pop `engine->del_entities`, remove each from `engine->entities`, and call `entity_destroy`.

All **parallel system work** (EARLY & LATE) must be finished (via job queue completion waits) before CLEANUP and entity destruction happen.

---

## Job queue and threaded systems

- `engine_run_phase(..., parallel = true)`:
  - For each active system in the requested phase:
    - Allocates a `SystemJobData { sys, eng, dt }`.
    - Pushes a job to the job queue with `_system_job_worker`.
- `_system_job_worker(thread_data, user_data, canceled)`:
  - Calls `sys->vt->update(sys, eng, dt)` on that worker thread.
- After scheduling all jobs, `engine_run_phase` waits for all job IDs to reach `JOB_RESULTS_READY`/`EXECUTED` before returning to the main thread.

**Invariants:**

- No entity or component is destroyed while a system update for that frame is still running.
- All system `update` calls see a consistent snapshot of entity/component lifetime for the duration of that frame’s phase.

---

## Component lifecycle and system notifications

### Component creation and attachment

1. A component is constructed (e.g. `entity_component_text_create(lua_engine)`).
2. It is attached to an entity via `entity_component_add(entity, comp)`:
   - Appends to `entity->components`.
   - Sets `comp->entity = entity`.
   - Calls `comp->vtable->ref(comp)` (Lua ref management).
   - Calls `engine_notify_comp_add(engine, comp)`:
     - For each system with `vt->accepts` returning true:
       - Calls `vt->on_component_added(self, eng, comp)`.
     - Systems use this to populate their internal tracking arrays.

### Component removal (deferred)

When a component is removed explicitly:

1. `entity_component_remove(entity, id)`:
   - Locates `comp` in `entity->components`.
   - Calls `engine_notify_comp_rem(engine, comp)`:
     - For each accepting system:
       - Calls `vt->on_component_removed(self, eng, comp)` to drop internal references.
   - For most components, **actual destruction is deferred**:
     - A dedicated **cleanup system** (`cleanup_system`) has `accepts = true` for all components and `on_component_removed` that enqueues `(entity, comp)` into its internal `removal_queue`.
2. In the CLEANUP phase (`cleanup_sys_update`):
   - The cleanup system iterates its `removal_queue`:
     - Verifies the component is still present on the entity (hasn’t been removed in other ways).
     - Calls `comp->vtable->unref(comp)` and `entity_component_destroy(comp)`.
     - Removes `comp` from `entity->components` by swap-with-last and decrementing count.

This **guarantees** that during EARLY/LUA/LATE phases of a frame:

- Systems still see the component attached to its entity.
- The actual removal/destruction happens only after all threaded system work is complete.

---

## Entity lifecycle

### Creation

- `entity_create(lua_engine)`:
  - Calls `_entity_make(engine)`:
    - Allocates `EseEntity`.
    - Creates and refs `entity->position` (`EsePoint`) with a watcher to update colliders on movement.
    - Initializes flags (`active = true`, `visible = true`, `persistent = false`, `destroyed = false`).
    - Allocates collision maps, tag arrays, etc. lazily.
  - Adds C-side reference (`entity_ref`).
- `engine_add_entity(engine, entity)`:
  - Appends `entity` to `engine->entities`.

Components can then be added via `entity_component_add`, triggering system registration via `engine_notify_comp_add`.

### Destruction

- From Lua, typical pattern: `entity:destroy()`:
  - `entity->destroyed = true; entity->active = false`.
  - `engine_remove_entity(engine, entity)`:
    - Append to `engine->del_entities`.
- At the end of a frame (`engine_update` step 12):
  - For each entity in `del_entities`:
    - Remove from `engine->entities`.
    - Call `entity_destroy(entity)`:
      - If Lua-refcount reaches 0, call `_entity_cleanup(entity)`:
        - (Best practice) first notify systems of each component’s removal:

```c
          EseEngine *engine =
              (EseEngine *)lua_engine_get_registry_key(entity->lua->runtime, ENGINE_KEY);

          if (engine) {
              for (size_t i = 0; i < entity->component_count; ++i) {
                  EseEntityComponent *comp = entity->components[i];
                  engine_notify_comp_rem(engine, comp);
              }
          }
```
        - Then:
          - Destroy UUID / position point (`ese_point_unref` + `ese_point_destroy`).
          - For each component:
            - `comp->vtable->unref(comp);`
            - `entity_component_destroy(comp);`
          - Destroy collisions, tags, subscriptions, Lua value refs, etc.
          - Free `entity`.

By notifying systems before running `entity_component_destroy`, systems can drop all component references and avoid stale pointers across frames.

---

## Threading / safety invariants to remember

When reasoning about bugs related to systems, threads, and lifetime, the key invariants are:

- **Per-frame ordering**:
  - All system `update` calls (including parallel ones) for a phase finish before the engine moves to the next major step (collision, entity draw, late systems, cleanup, entity deletion).
- **Destruction deferral**:
  - No entity or component is destroyed while any EARLY/LATE system job for that frame is still running.
  - Deferred component removal is handled in CLEANUP; entity destruction is at the very end.
- **System state must match lifecycle**:
  - Systems that cache component pointers *must* drop them in response to `engine_notify_comp_rem` (both for explicit component removal and for entity-wide destruction).
  - If entity destruction bypasses `engine_notify_comp_rem`, systems will retain stale pointers into freed memory and can fault in subsequent frames.

You can reuse this summary whenever you need to analyze:

- Whether a system is allowed to touch a particular entity/component field in a given phase.
- How to safely add a new system that caches pointers.
- Why a system might be seeing a freed or NULL pointer even though destruction is “deferred until after threads.”
