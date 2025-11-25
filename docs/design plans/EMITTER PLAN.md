# EntityComponentEmitter and Particle System Design
## Problem statement
The engine currently has a sprite component (`EseEntityComponentSprite`) and corresponding systems (`sprite_system`, `sprite_render_system`) that animate and render one sprite per entity. There is no first-class particle/emitter system for efficiently handling many short-lived sprite-based particles with configurable emission behavior. We want to introduce an `EntityComponentEmitter` component and particle systems that integrate with the existing ECS, system manager, renderer, and Lua bindings, and that support standard 2D particle emitter options.
## Current architecture (relevant pieces)
### Entity and component model
Entities (`EseEntity`) own an array of `EseEntityComponent*` with a shared `ComponentVTable` defined in `entity_component_private.h`. The base struct includes:
* `EntityComponentType type` enum (sprite, collider, text, sound, etc.).
* `void *data` pointing to the concrete component type.
* `EseEntity *entity`, `EseLuaEngine *lua`, UUID, active flag, and Lua registry ref/count.
System registration and lifecycle notifications use `engine_notify_comp_add` / `engine_notify_comp_rem` which are called from `entity_component_add` / `entity_component_remove` in `entity.c`.
### Sprite component
`EseEntityComponentSprite` in `entity_component_sprite.[ch]` wraps the base component and holds minimal animation state:
* `char *sprite_name` resolved via `engine_get_sprite(engine, name)`.
* `size_t current_frame` and `float sprite_ellapse_time` for simple frame-based animation.
The component uses a `ComponentVTable` with copy/destroy/ref/unref implementations, and a userdata-based Lua proxy (`ENTITY_COMPONENT_SPRITE_PROXY_META`) with properties `active`, `id`, and `sprite` exposed via `__index` / `__newindex`.
Lua-side construction uses `EntityComponentSprite.new([sprite_name])`, while engine/engine-side construction uses `entity_component_sprite_create(engine, sprite_name)` which also registers the component with Lua via `vtable->ref`.
### Sprite systems
There are two sprite-related systems:
* `sprite_system.[ch]` (phase `SYS_PHASE_EARLY`): maintains a dense array of `EseEntityComponentSprite*`, tracks additions/removals, and in `update` advances `sprite_ellapse_time` and `current_frame` for active and visible entities using `sprite_get_speed` / `sprite_get_frame_count`.
* `sprite_render_system.[ch]` (phase `SYS_PHASE_LATE`): maintains its own dense array of `EseEntityComponentSprite*` and, in `update`, looks up the current frame via `sprite_get_frame`, converts entity world position to screen space via `engine_get_camera` and `engine_get_display`, and submits each sprite to the draw list via `_engine_add_texture_to_draw_list`.
This split between an animation system and a render system, both keyed on the same component type, is the core pattern we should follow for emitters.
### Sprite assets
`graphics/sprite.h` defines `EseSprite` as a sequence of frames and animation speed:
* Frames added via `sprite_add_frame`, retrieved via `sprite_get_frame(sprite, frame, ...)`.
* Animation speed accessed via `sprite_get_speed` / `sprite_set_speed`.
Sprites are retrieved from the engine via `engine_get_sprite(engine, sprite_id)`.
## Goals and requirements
1. Add a new emitter component type, `EntityComponentEmitter`, that can be attached to entities and controlled from Lua, following the same patterns as existing components.
2. Implement emitter and particle systems that simulate and render potentially many particles efficiently without requiring one entity per particle.
3. Use sprites as particles: each particle should be drawn using either a fixed `EseSprite` or frames from a configured sprite resource.
4. Support standard 2D particle emitter options, including (at minimum):
    * Emission: rate (particles/sec), bursts, maximum live particles, looping vs one-shot, start delay.
    * Lifetime: per-particle lifetime range and optional emitter lifetime.
    * Initial position: emitter offset, basic shapes (point, circle radius, axis-aligned box) in either local or world space.
    * Motion: initial speed range, emission angle and spread, gravity (constant acceleration), optional radial/ tangential acceleration.
    * Appearance: start/end size (scale), start/end color (RGBA) over lifetime, optional random start frame or animated frames.
    * Sorting: use entity `draw_order` plus optional per-particle offset / randomness.
5. Integrate cleanly with system phases: simulation must run before rendering, and rendering should coexist with the existing sprite render system and draw list.
6. Provide a Lua-facing API that makes it easy to configure emitters from scripts, including constructing components, adjusting parameters at runtime, and triggering bursts.
7. Follow existing memory, logging, and Lua binding conventions (memory_manager tags, `log_*`, userdata + registry ref model).
## Proposed C types and files
### New component type and files
Add a new component type to `EntityComponentType` in `entity_component_private.h`:
* `ENTITY_COMPONENT_EMITTER` (appended after existing entries).
Add new component files under `src/entity/components/`:
* `entity_component_emitter.h`
* `entity_component_emitter.c`
These files will define the component struct, vtable, Lua bindings, and C-side creation helpers, following the sprite component's structure.
### Emitter component struct
Define `EseEntityComponentEmitter` in `entity_component_emitter.h`:
* `EseEntityComponent base;` (must be first and point `base.data` back to the struct).
* Configuration fields (emitter-level):
    * `char *sprite_name;` – ID of the sprite asset used for particles.
    * `bool emitting;` – whether the emitter is currently active.
    * `bool one_shot;` – emit until a fixed particle count then stop.
    * `bool local_space;` – particles follow the entity transform when true.
    * `float emission_rate;` – particles per second.
    * `float burst_count;` and `float burst_interval;` or a simple burst helper.
    * `int max_particles;` – pool size / cap on active particles.
    * Lifetime and timing: `float particle_lifetime_min, particle_lifetime_max;`, `float emitter_lifetime;` and `float emitter_age;` (0 for infinite), `float emit_accumulator;`
    * Shape parameters: `float shape_radius;`, `float shape_half_width, shape_half_height;`, `int shape_type;` (point, circle, box).
    * Motion parameters: `float speed_min, speed_max;`, `float direction;` (base angle) and `float spread;`, `float gravity_x, gravity_y;`, `float radial_accel, tangential_accel;`
    * Appearance parameters: `float start_size, end_size;`, `unsigned char start_r, start_g, start_b, start_a;`, `unsigned char end_r, end_g, end_b, end_a;`, `bool random_start_frame;`, `bool animate_frames;`
* Particle pool: `struct EseEmitterParticle *particles;`, `int particle_count;`, additional bookkeeping indices.
### Particle struct
Define a private `EseEmitterParticle` in `entity_component_emitter.c`:
* `bool alive;`
* `float x, y;` – position in world or local coordinates.
* `float vx, vy;` – velocity.
* `float age;` and `float lifetime;`
* `float size;` – current scale.
* `unsigned char r, g, b, a;` – current color.
* Optional: `float rotation;` and `float angular_velocity;`, `float frame;` or integer `frame_index;`.
The particle array will be a fixed-size pool of `max_particles`. Spawning finds a dead slot (or overwrites the oldest particle).
### New systems for emitters
Add new systems under `src/entity/systems/`:
* `emitter_system.h` / `emitter_system.c` – simulation/update of particles.
* `emitter_render_system.h` / `emitter_render_system.c` – rendering of particles.
System APIs will mirror the sprite systems:
* `EseSystemManager *emitter_system_create(void);`
* `void engine_register_emitter_system(EseEngine *eng);`
* `EseSystemManager *emitter_render_system_create(void);`
* `void engine_register_emitter_render_system(EseEngine *eng);`
The emitter system should run in `SYS_PHASE_EARLY` or `SYS_PHASE_LATE` depending on when particles should update relative to other game logic; rendering must run in `SYS_PHASE_LATE` alongside other render systems.
## Emitter simulation system design
### System membership
The emitter system will follow the sprite system pattern:
* Internal data struct `EmitterSystemData`: `EseEntityComponentEmitter **emitters;`, `size_t count, capacity;`
* `accepts`: Returns true when `comp->type == ENTITY_COMPONENT_EMITTER`.
* `on_component_added`: Append `(EseEntityComponentEmitter *)comp->data` to the `emitters` array, growing capacity as needed.
* `on_component_removed`: Find the pointer and remove it via swap-with-last, then decrement `count`.
* `init` / `shutdown`: Allocate or free the `EmitterSystemData` and internal array using `memory_manager`.
### Update algorithm
`emitter_sys_update(self, eng, dt)` will:
1. Iterate all tracked emitters.
2. Skip emitters whose owning entity is inactive, invisible, or destroyed.
3. Update emitter age and disable `emitting` when `one_shot` or `emitter_lifetime` conditions are met.
4. Spawn new particles if `emitting`:
    * Accumulate `emit_accumulator += emission_rate * dt`.
    * While `emit_accumulator >= 1.0f` and `particle_count < max_particles`, emit one particle and decrement accumulator.
    * For each new particle: Sample spawn position according to `shape_type`, transform to world space if needed, sample lifetime, speed, and direction, set velocity, initialize `age = 0`, `size = start_size`, and color to start RGBA.
5. Integrate existing particles: For each alive particle, increase `age += dt`; kill if `age >= lifetime`, apply acceleration, update position, interpolate appearance over normalized life.
6. Return an empty `EseSystemJobResult`.
### Coordinate space handling
* If `local_space == true`: particles store positions relative to the emitter entity. Rendering converts them to world coordinates by adding `entity->position` at draw time.
* If `local_space == false`: particles store world-space positions computed at spawn time; they stay where they are even if the emitter entity moves.
## Emitter render system design
### System membership and lifecycle
`EmitterRenderSystemData` will mirror `SpriteRenderSystemData`:
* `EseEntityComponentEmitter **emitters;`, `size_t count, capacity;`
`accepts`, `on_component_added`, `on_component_removed`, `init`, and `shutdown` will follow the same patterns as the sprite render system, but with the emitter component type.
### Rendering algorithm
`emitter_render_sys_update(self, eng, dt)` will:
1. For each emitter: Skip if `sprite_name` is NULL or if the owning entity is inactive, invisible, or destroyed.
2. Resolve `EseSprite *sprite = engine_get_sprite(eng, emitter->sprite_name);` and skip if NULL.
3. Fetch camera, display, and draw list via `engine_get_camera`, `engine_get_display`, and `engine_get_draw_list`.
4. Iterate the emitter's particle array: Skip dead particles, compute particle world-space position, convert to screen-space, compute sprite frame, apply particle size and color, submit to draw list using `_engine_add_texture_to_draw_list`.
5. Return an empty `EseSystemJobResult`.
Rendering code should be careful to avoid large allocations per frame; only use the pre-allocated particle pool.
## Lua API design
### Emitter component Lua type
Add a new Lua-visible component table similar to `EntityComponentSprite`:
* Global table: `EntityComponentEmitter` with `EntityComponentEmitter.new([config_table])`.
* Userdata metatable: `ENTITY_COMPONENT_EMITTER_PROXY_META` with `__index`, `__newindex`, `__gc`, `__tostring`.
* Helper Lua methods: `emitter:start()` / `emitter:stop()`, `emitter:burst(count)`.
### Engine-side creation helper
Add a C-side helper analogous to `entity_component_sprite_create`:
* `EseEntityComponent *entity_component_emitter_create(EseLuaEngine *engine, const EmitterConfig *cfg);`
### Registration
In `entity_component_lua_init` or equivalent initialization code:
* Call a new `_entity_component_emitter_init(EseLuaEngine *engine)` to register the emitter metatable and the `EntityComponentEmitter` global table.
## Engine integration
### Component type and system registration
Update `entity_component_private.h` to include `ENTITY_COMPONENT_EMITTER` in `EntityComponentType`.
Hook emitter initialization into engine startup:
* Call `_entity_component_emitter_init(engine->lua_engine);` where other component Lua bindings are initialized.
Register systems in `engine_create` or wherever systems are wired:
* After registering sprite systems: `engine_register_emitter_system(engine);` and `engine_register_emitter_render_system(engine);`
### Memory management and tags
Define new memory tags if desired (e.g. `MMTAG_S_EMITTER`, `MMTAG_RS_EMITTER`, `MMTAG_ENTITY_PARTICLE`) or reuse existing sprite/system tags.
All allocations for emitter components and particles must use the `memory_manager` API; no direct `malloc`/`free`.
## Testing strategy
### C unit tests
Add a new test file under `tests/`:
* `test_entity_component_emitter.c` to cover: Creation and destruction of emitters, basic configuration, simulation behavior, local vs world space, and interaction with engine.
### Lua-facing tests
Add Lua-based tests or example scripts that: Create an entity and add an `EntityComponentEmitter` via Lua, configure sprite, emission rate, lifetime, and shape, run the engine for a few frames and verify particle spawning.
## Implementation phases
### Phase 1 – Scaffolding and component API
* Extend `EntityComponentType` with `ENTITY_COMPONENT_EMITTER`.
* Implement `EseEntityComponentEmitter` struct, vtable, creation, copy, destroy, ref/unref, and Lua bindings without actual particle simulation.
* Wire `_entity_component_emitter_init` into component initialization and add basic tests.
### Phase 2 – Particle data model and simulation system
* Implement `EseEmitterParticle` and the internal particle pool.
* Implement `emitter_system.[ch]` with `EmitterSystemData`, membership management, and `emitter_sys_update`.
* Support core options: emission rate, lifetime range, speed and direction with spread, shape, max particles, and gravity.
### Phase 3 – Rendering system and visual tuning
* Implement `emitter_render_system.[ch]` to render particles using the configured sprite.
* Add size and color over lifetime interpolation, local vs world-space handling, and optional frame animation.
* Integrate systems into the engine and verify ordering.
### Phase 4 – Polishing and documentation
* Extend Lua API with helper methods and any additional properties.
* Add or update documentation under `docs/`.
* Add more exhaustive tests and example scenes demonstrating emitters with different configurations.
