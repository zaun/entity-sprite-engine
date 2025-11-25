
# Problem statement
We want a first-class **Scene** type that describes a set of entities and their components and can be used from Lua to:
* Snapshot the current engine state (entities + components) into a `Scene` value.
* Re-instantiate that description into the engine later via an instance `run` method.
* Provide class-level `clear` and `reset` operations that replace the current free functions `scene_clear` and `scene_reset`.
The design should fit the existing patterns used for engine types like `Point` and `Rect` (C struct, Lua proxy, optional serialization) and integrate cleanly with the entity/component architecture.
# Current architecture (relevant pieces)
## Entities and components
* `EseEntity` is defined in `entity/entity_private.h` and managed via `entity_create`, `entity_copy`, and `entity_destroy`.
* Each entity owns:
    * Core flags: `active`, `visible`, `persistent`, `destroyed`, `draw_order`.
    * A position `EsePoint *position` with a watcher that updates collider bounds when the point changes.
    * Arrays of `EseEntityComponent *components` plus counts/capacity.
    * Collision state: `collision_bounds`, `collision_world_bounds`, `current_collisions`, `previous_collisions`.
    * Lua integration: `EseLuaEngine *lua`, `lua_ref`, `lua_ref_count`, `lua_val_ref` (self `EseLuaValue`), and an environment table (`data` / `__data`).
    * Tags and pub/sub subscription tracking.
* Components are abstracted via `EseEntityComponent` with per-type vtables and C+Lua implementations under `entity/components/`.
* Systems are notified of component lifecycle via `engine_notify_comp_add` / `engine_notify_comp_rem` from `entity_component_add` / `entity_component_remove`.
## Engine-side entity storage and clearing
* `EseEngine` (in `core/engine.c`) holds entities in a double-linked list:
    * `engine->entities` – live entities.
    * `engine->del_entities` – entities queued for deletion.
* `engine_add_entity` appends to `engine->entities`.
* `engine_remove_entity` queues an entity by appending it to `engine->del_entities`.
* `engine_clear_entities(EseEngine *engine, bool include_persistent)` iterates `engine->entities` and queues entities for deletion:
    * When `include_persistent == false`, only non-persistent entities (`!entity_get_persistent(entity)`) are queued.
    * When `include_persistent == true`, all entities are queued.
* Actual destruction happens at the end of `engine_update`, which drains `engine->del_entities` and destroys entities after removing them from `engine->entities`.
## Existing scene_* Lua functions
* `engine_create` registers global Lua functions:
    * `scene_clear` → `_lua_scene_clear`.
    * `scene_reset` → `_lua_scene_reset`.
* Their current implementations in `core/engine_lua.c` are thin wrappers:
    * `_lua_scene_clear`:
        * Validates zero arguments.
        * Fetches the engine from the Lua registry and calls `engine_clear_entities(engine, false)`.
    * `_lua_scene_reset`:
        * Validates zero arguments.
        * Fetches the engine and calls `engine_clear_entities(engine, true)`.
* There is **no** current `Scene` type — only free functions operating on the engine.
## Type patterns: Point and Rect
* `types/point.[ch]` and `types/rect.[ch]` define engine-side math types with a consistent pattern:
    * Opaque typedef in the header (`typedef struct EsePoint EsePoint;`).
    * Private struct definition in the `.c` file containing numerical data + Lua state, registry ref, ref count, and watcher arrays.
    * A C API for lifecycle (`*_create`, `*_copy`, `*_destroy`, `*_sizeof`) and math functions.
    * Lua integration functions (`*_lua_init`, `*_lua_push`, `*_lua_get`, `*_ref`, `*_unref`).
    * JSON serialization via cJSON (`*_serialize`, `*_deserialize`).
* Lua registration functions create a proxy metatable (`PointProxyMeta`, `RectProxyMeta`) and a global table (`Point`, `Rect`) with constructors.
This pattern is a good model for a new `Scene` type: C-owned data, Lua proxies, optional JSON support later.
# Goals and requirements for Scene
1. **Scene data model**
    * Represent a **description** of a set of entities and their components, not just a live view.
    * The description should be usable to create *new* entities with equivalent state (position, tags, components, and component configuration) when `Scene:run()` is called.
    * Entity UUIDs/IDs are **not** serialized or reused; each `scene:run()` call creates brand-new entities with new IDs, and scripts should use tags to find entities rather than relying on stable IDs.
2. **Instance method: run**
    * `scene:run()` should:
        * Iterate all described entities in the scene.
        * For each description, construct a new `EseEntity` and attach components according to the description.
        * Register each new entity with the engine via `engine_add_entity`.
        * Ensure systems are notified of component additions via `engine_notify_comp_add` indirectly through `entity_component_add`.
3. **Class method: create**
    * `Scene.create(...)` (a class method on Lua `Scene`) should:
        * Inspect the current engine entities (`engine->entities`).
        * For each, construct a description capturing at least:
        * Core entity flags (`active`, `visible`, `persistent`, `draw_order`).
        * Position (`x`, `y`).
        * Tags.
        * Components and their configuration.
        * Return a new `Scene` instance that holds these descriptions.
    * We want this to be callable from Lua without explicitly passing the engine (`Scene.create()` will look up the engine from the registry like the existing scene_* functions).
4. **Class methods: clear and reset**
    * Move behavior of `_lua_scene_clear` and `_lua_scene_reset` into `Scene` as **class methods**:
        * `Scene.clear()` → `engine_clear_entities(engine, false)`.
        * `Scene.reset()` → `engine_clear_entities(engine, true)`.
    * Decide how to handle the *old* `scene_clear` and `scene_reset` globals (backwards compatibility vs breaking change).
5. **Fit with existing patterns**
    * Follow patterns used by `Point`/`Rect`:
        * `types/scene.h` / `types/scene.c` for the C struct and API.
        * `types/scene_lua.[ch]` for Lua bindings and registration of the `Scene` global.
        * Use `EseLuaEngine` and `EseLuaValue` for bridging between C scene data and Lua.
# Proposed design
## 1. C data model for Scene
### 1.1 Core types and headers
* Add a new type under `src/types/`:
    * Header: `types/scene.h`.
    * Implementation: `types/scene.c`.
    * Lua bindings: `types/scene_lua.h` / `types/scene_lua.c`.
* Public opaque type:
    * `typedef struct EseScene EseScene;`
### 1.2 Internal structure
Inside `scene.c`, define something along these lines (conceptually):
* `EseScene` owns:
    * `EseLuaEngine *lua;` – pointer to the single engine's Lua instance; scenes are always used with this engine and are only created/run from the main thread via Lua bindings.
    * `EseArray *entities;` – dynamic array of `EseSceneEntityDesc*`.
* `EseSceneEntityDesc` represents one entity blueprint:
    * Core fields copied from an entity:
        * `bool active;`
        * `bool visible;`
        * `bool persistent;`
        * `uint64_t draw_order;`
        * `float x, y;` – position at time of snapshot.
        * A list of tags: `char **tags; size_t tag_count;`.
    * Component blueprints captured in a C-driven, JSON-based format:
        * `EseArray *components;` – dynamic array of `EseSceneComponentBlueprint*`.
* `EseSceneComponentBlueprint` conceptually contains:
    * `EntityComponentType type;` – which component this blueprint is for.
    * `cJSON *json;` – configuration produced by that component's existing `*_serialize` / `*_to_json` function, representing its **current state** at snapshot time.
    * Optional extra metadata if a component needs it (e.g., versioning tags) – the exact shape can evolve with components.
Rationale:
* Scenes are pure blueprints of the "current state" when `Scene.create` is called; they are not live views of entities.
* By delegating serialization to each component's existing C-level to/from JSON functions, scene creation and re-instantiation stay C-driven and do not depend on Lua to define the configuration schema.
* Because IDs are not serialized, running a scene always produces a fresh set of entities; cross-entity references should use tags or other higher-level mechanisms rather than UUIDs.
### 1.3 Scene lifecycle API
In `scene.h`:
* Creation and destruction:
    * `EseScene *ese_scene_create(EseLuaEngine *engine);`
    * `void ese_scene_destroy(EseScene *scene);`
* Query:
    * `size_t ese_scene_entity_count(const EseScene *scene);`
* Snapshot from engine:
    * `EseScene *ese_scene_create_from_engine(EseEngine *engine, bool include_persistent);`
        * This will implement the core of `Scene.create()`.
* Run/instantiate:
    * `void ese_scene_run(EseScene *scene, EseEngine *engine);`
Optionally (future work, not mandatory initially):
* JSON serialization:
    * `cJSON *ese_scene_serialize(const EseScene *scene);`
    * `EseScene *ese_scene_deserialize(EseLuaEngine *engine, const cJSON *json);`
## 2. Building a Scene from engine entities (Scene.create)
### 2.1 High-level flow
Implement `ese_scene_create_from_engine(engine, include_persistent)` roughly as:
1. Allocate a new `EseScene` bound to `engine->lua_engine`.
2. Iterate over `engine->entities` using a `EseDListIter`.
3. For each `EseEntity *entity`:
    * Skip destroyed entities.
    * If `include_persistent == false` and `entity->persistent == true`, skip.
    * Construct an `EseSceneEntityDesc`:
        * Copy simple fields (`active`, `visible`, `persistent`, `draw_order`).
        * Copy `x`, `y` from `entity->position` via `ese_point_get_x/y`.
        * Deep-copy tags: allocate `char*` per tag and copy the strings.
    * Capture component blueprints as described below.
`ese_scene_create_from_engine` is only called from Lua bindings on the main thread; there is a single `EseEngine`/`EseLuaEngine` per application, and scenes are not intended to be used across multiple engines.
### 2.2 Capturing entity configuration via component JSON
Instead of going through Lua tables, scene blueprints use each component's existing JSON serialization:
* Introduce an internal helper (not Lua-visible):
    * `EseSceneEntityDesc *scene_build_entity_desc(EseEntity *entity);`
        * Implementation outline:
        1. Allocate and populate the core fields and tags as above.
        2. For each `EseEntityComponent *comp` in `entity->components`:
            * Call that component's existing C-level `*_serialize` / `*_to_json` function to obtain a `cJSON*` representing its current configuration.
            * Allocate an `EseSceneComponentBlueprint`, store `comp->type` and the `cJSON*` into it, and append it to `desc->components`.
        3. If we later introduce entity-level JSON (for the `data` environment or other script state), we can store that alongside the component blueprints using the same pattern.
* `EseScene` owns all `EseSceneEntityDesc` instances and their `EseSceneComponentBlueprint` arrays, including the `cJSON*` nodes. `ese_scene_destroy` is responsible for freeing them.
This keeps the blueprint format purely C-driven and reuses the existing per-component JSON schema, while still capturing the full "current" configuration of each component at the moment `Scene.create` is called.
## 3. Instantiating a Scene (scene:run)
### 3.1 High-level flow
`ese_scene_run(EseScene *scene, EseEngine *engine)` should:
1. Iterate `scene->entities`.
2. For each `EseSceneEntityDesc *desc`:
    * Construct a new entity: `EseEntity *entity = entity_create(engine->lua_engine);`
    * Set base fields:
        * `entity->active = desc->active;`
        * `entity->visible = desc->visible;`
        * `entity->persistent = desc->persistent;`
        * `entity->draw_order = desc->draw_order;`
        * `entity_set_position(entity, desc->x, desc->y);`
        * Re-add tags via `entity_add_tag`.
    * Run a helper that applies the `spec` blueprint to attach components and `data`.
    * Register the entity with the engine via `engine_add_entity(engine, entity);`.
### 3.2 Applying the blueprint in C using component JSON
Reconstruction is fully C-driven and uses each component's `*_deserialize` / `*_from_json` implementation:
* Add an internal helper:
    * `bool entity_apply_blueprint(EseEntity *entity, const EseSceneEntityDesc *desc);`
        * Implementation outline:
        1. For each `EseSceneComponentBlueprint *bp` in `desc->components`:
            * Dispatch on `bp->type` and call the corresponding component's C-level JSON deserializer to construct a new `EseEntityComponent*` from `bp->json`.
            * Attach the component to the entity via the normal `entity_component_add` path so that systems see the new component.
        2. If entity-level JSON is added later, apply it here as well.
        3. Return `true` on success; on failure, log an error and either partially construct the entity or destroy it, depending on what is most appropriate for the engine.
Scenes themselves never call into Lua to rebuild entities; Lua is used only to *request* `Scene.create()` and `scene:run()`, with all work performed in C.
### 3.3 Ensuring systems are notified
* Because we will use normal entity/component APIs, systems will be notified automatically:
    * Creating components goes through `entity_component_add`.
    * That leads to `engine_notify_comp_add` being invoked and all relevant systems updating their internal collections.
* Multiple calls to `ese_scene_run` / `scene:run()` are allowed and will create additional entities each time; scenes do not track or replace previously instantiated entities.
## 4. Lua API surface for Scene
### 4.1 Lua metatable and global table
* Add `scene_lua_init(EseLuaEngine *engine)` in `types/scene_lua.c`.
* This will:
    * Create a `SceneProxyMeta` metatable for Scene userdata:
        * `__index` should dispatch instance methods: `run`, `entity_count`.
    * Create a global `Scene` table with class methods:
        * `Scene.create([include_persistent])`
        * `Scene.clear()`
        * `Scene.reset()`
* Register `Scene` in `engine_create` after other types:
    * Call `ese_scene_lua_init(engine->lua_engine);` similarly to `ese_point_lua_init`, `ese_rect_lua_init`, etc.
### 4.2 Class methods
#### `Scene.create([include_persistent])`
* Bound to a C function `_lua_scene_create(lua_State *L)` in `scene_lua.c`:
    * Read optional boolean `include_persistent` (default false) from argument 1.
    * Get `EseEngine *engine` from the registry.
    * Call `EseScene *scene = ese_scene_create_from_engine(engine, include_persistent);`
    * Wrap `scene` in a Lua userdata with metatable `SceneProxyMeta` and return it.
#### `Scene.clear()`
* C function `_lua_scene_class_clear(lua_State *L)`:
    * No arguments; always preserves the existing `scene_clear` semantics of clearing only non-persistent entities.
    * Get engine from registry.
    * Call `engine_clear_entities(engine, false);`
    * Return `true`.
#### `Scene.reset()`
* C function `_lua_scene_class_reset(lua_State *L)`:
    * No arguments.
    * Get engine from registry.
    * Call `engine_clear_entities(engine, true);`.
    * Return `true`.
### 4.3 Instance methods: `scene:run()` and `scene:entity_count()`
* C binding `_lua_scene_run(lua_State *L)` in `scene_lua.c`:
    * Ensure `self` is a `Scene` userdata and fetch `EseScene *scene`.
    * Get engine from registry.
    * Call `ese_scene_run(scene, engine);`
    * Return nothing (or a boolean success).
* C binding `_lua_scene_entity_count(lua_State *L)` in `scene_lua.c`:
    * Ensure `self` is a `Scene` userdata and fetch `EseScene *scene`.
    * Call `ese_scene_entity_count(scene)` and push the resulting count as an integer.
    * Return 1 result.
## 5. Moving scene_clear / scene_reset into Scene
### 5.1 Engine changes
* In `engine_create` (`core/engine.c`), **stop registering** the global functions:
    * Remove or comment:
        * `lua_engine_add_function(engine->lua_engine, "scene_clear", _lua_scene_clear);`
        * `lua_engine_add_function(engine->lua_engine, "scene_reset", _lua_scene_reset);`
* Instead, `ese_scene_lua_init` will register `Scene.clear` and `Scene.reset` as described above.
### 5.2 Backwards compatibility options
* **Strict (breaking) change**:
    * Do not define global `scene_clear` / `scene_reset` names at all; users must call `Scene.clear()` and `Scene.reset()`.
* **Soft migration** (recommended if many scripts exist):
    * Keep tiny shim functions in `engine_lua.c`:
        * `scene_clear()` implemented in Lua or C as `return Scene.clear(false)`.
        * `scene_reset()` implemented as `return Scene.reset()`.
    * Mark them as deprecated in docs (`docs/global.md`) and eventually remove them.
## 6. Testing strategy
* **Unit tests for Scene C API** (new `tests/test_scene.c`):
    * Create a fake engine with a few entities and components (via existing testing helpers).
    * Call `ese_scene_create_from_engine(engine, false)` and verify:
        * Entity count in scene equals the number of non-persistent entities.
        * `EseSceneEntityDesc` fields match the original entities.
    * Call `engine_clear_entities` and then `ese_scene_run(scene, engine)`.
        * Verify entities were recreated (correct count, tags, persistence flags, positions).
* **Lua tests**:
    * In a Lua-visible test or example, exercise:
        * `local scene = Scene.create()`.
        * `Scene.clear()`; assert in Lua that `Entity.count()` is 0 or only persistent entities remain.
        * `scene:run()`; assert the entity count and basic properties.
        * `Scene.reset()` to ensure all entities are cleared.
* **Regression tests for existing behavior**:
    * If shims for `scene_clear` and `scene_reset` are kept, ensure they still clear/reset entities exactly as before.
## 7. Implementation phases
To keep the work manageable and incremental:
1. **Phase 1 – Infrastructure and clear/reset migration**
    * Implement `EseScene` with minimal internals (maybe just an empty struct initially).
    * Implement Lua `Scene` table with `Scene.clear` and `Scene.reset` wired to `engine_clear_entities`.
    * Remove direct registration of `scene_clear` / `scene_reset` from `engine_create` (or rewire them as shims).
    * Add basic tests verifying `Scene.clear` / `Scene.reset` semantics.
2. **Phase 2 – Snapshot and run scaffolding**
    * Flesh out `EseScene` internals and `ese_scene_create_from_engine` / `ese_scene_run` signatures.
    * Implement the iteration over `engine->entities` and creation of `EseSceneEntityDesc` with basic fields (no components yet), and ensure `run` can respawn entities with position, tags, and simple flags.
    * Expose `Scene.create([include_persistent])` and `scene:run()` to Lua.
3. **Phase 3 – Component and data blueprints**
    * Implement `entity_serialize_blueprint` and `entity_apply_blueprint` using `EseLuaValue` and Lua helpers.
    * Extend `EseSceneEntityDesc` to capture component configuration and entity `data` and to rebuild them on `run`.
    * Update tests to assert component re-creation as well.
4. **Phase 4 – Optional JSON serialization and docs**
    * If desired, add `ese_scene_serialize` / `ese_scene_deserialize` for saving/loading scenes to disk (parallel to `Point`/`Rect`).
    * Add `docs/scene.md` and update `docs/global.md` with the new `Scene` API and deprecation note for global `scene_clear`/`scene_reset` if applicable.
This plan yields a `Scene` type that follows your existing type and Lua binding conventions, encapsulates a description of entities and their components, allows capturing from a running engine, re-instantiates those entities via `run`, and moves scene management operations into `Scene` class methods.