# Audio System Review

This document summarizes the current state of the engine's sound system (SFX only, not music), highlights strengths and issues, and lists recommended fixes and future enhancements.

## Scope

Reviewed files:
- `src/entity/systems/sound_system.c`
- `src/entity/systems/sound_system_lua.c`
- `src/entity/systems/sound_system_private.h`
- `src/entity/components/entity_component_sound.c`
- `src/entity/components/entity_component_listener.c`
- `src/entity/entity_private.h`
- `src/audio/pcm.c`

The focus is on the SFX path driven by ECS components and mixed via miniaudio.

## High-Level Architecture

- **Core mixer**
  - `SoundSystemData` owns the miniaudio context, playback device, and arrays of tracked sound and listener components.
  - A global pointer `g_sound_system_data` exposes this state to the Lua bindings and the miniaudio data callback.
  - The miniaudio callback (`sound_sys_data_callback`) mixes all active sounds into an interleaved float32 output buffer.

- **ECS components**
  - `EseEntityComponentSound`
    - Holds asset ID (`sound_name`), playback state (`frame_count`, `current_frame`, `playing`, `repeat`, `is_spatial`).
    - Exposed to Lua via the `EntityComponentSound` proxy type.
  - `EseEntityComponentListener`
    - Holds listener state (`volume`, `spatial`, `max_distance`) and uses the entity's position.
    - Exposed to Lua via the `EntityComponentListener` proxy type.

- **Lua bindings**
  - `Sound` global
    - `Sound.devices`: read-only array of playback device names.
    - `Sound.selected_device`: name of the currently selected device (or `nil`).
    - `Sound.select(idx)`: selects a playback device by 1-based index.
  - Component proxies
    - `EntityComponentSound` exposes `active`, `id`, `sound`, `frame_count`, `current_frame`, `is_playing`, `is_repeat`, `is_spatial`, and methods `play`, `pause`, `stop`, `seek`.
    - `EntityComponentListener` exposes `active`, `id`, `volume`, `spatial`, `max_distance`.

- **PCM abstraction**
  - `EsePcm` wraps raw float32 PCM data plus `frame_count`, `channels`, and `sample_rate`.
  - Accessed via `engine_get_sound` in the mixer; samples and metadata are exposed via simple getters.

## Strengths

- **Clear separation of concerns**
  - Mixing, ECS components, and Lua bindings are cleanly separated and easy to reason about.

- **Simple but effective spatial model**
  - 2D distance-based attenuation with optional stereo panning from listener–sound `dx`.
  - Attenuation clamped to `[0, 1]` and panning clamped to `[-1, 1]`.

- **Safe defaults in the mixer**
  - Output buffer is always zeroed before mixing.
  - If anything critical is missing (no engine, no system data, unsupported format), the callback writes silence and returns.
  - A final hard-clipping pass keeps samples in `[-1, 1]` after summing multiple sounds.

- **ECS integration**
  - `sound_sys_accepts` filters sound and listener components only.
  - `on_component_added` / `on_component_removed` maintain dense arrays of component pointers with simple capacity growth and swap-with-last removal.
  - The system is registered in the early phase and exposes a small, focused Lua API.

- **Lua ergonomics**
  - Component methods work with both `comp:play()` and `comp.play()` via upvalues.
  - Read-only fields are enforced in setters via clear Lua errors.
  - `Sound.devices` is exposed as a normal array so `#Sound.devices` works as expected.

## Issues and Recommended Fixes

### 1. Thread-Safety and Data Races

1. **Listener properties are updated without locking**
   - The mixer reads `listener->volume`, `listener->spatial`, and `listener->max_distance` while holding the sound-system mutex.
   - The Lua setters in `entity_component_listener.c` modify these fields without taking that mutex.
   - This creates a data race between the audio thread and the Lua/main thread.

   **Recommendation:**
   - Wrap writes to `volume`, `spatial`, and `max_distance` in the same mutex used in the sound system (i.e., `g_sound_system_data->mutex` if available).
   - This should mirror the locking strategy already used in the sound component's Lua methods (`play`, `pause`, `stop`, `seek`).

2. **Device selection fields are not consistently protected**
   - `sound_system_select_device_index` writes `output_device_id` and `device_initialized` without holding the mutex.
   - `sound_system_selected_device_name` reads `output_device_id` and scans the device list under the mutex.
   - This can race when switching devices concurrently with Lua querying `Sound.selected_device`.

   **Recommendation:**
   - Guard both reads and writes of `output_device_id`, `device_initialized`, and related device-selection state with the sound-system mutex.

3. **Audio callback calls general engine APIs under a mutex**
   - `sound_sys_data_callback` calls `engine_get_sound` for each playing sound while holding `g_sound_system_data->mutex`.
   - Depending on the implementation of `engine_get_sound`, this may involve non-trivial work (caches, maps, or even I/O in edge cases), which is not ideal on the audio thread.

   **Recommendation:**
   - Introduce a cached `EsePcm *` pointer in `EseEntityComponentSound` (or a similar structure) and resolve it on the main thread when `sound` is set or when assets are (re)loaded.
   - In the callback, only dereference the cached pointer (no asset lookups).
   - Long term, consider moving to a double-buffered or command-queue model for audio state to minimize locking in the callback.

4. **Locking and Lua error paths**
   - In `_entity_component_sound_seek`, the code acquires the sound-system mutex and then calls `luaL_checkinteger`, which can raise a Lua error while the mutex is held.

   **Recommendation:**
   - Read and validate Lua arguments before acquiring the mutex.
   - Only hold the mutex for the minimal critical section that mutates component state.

### 2. Behavior and Defaults

5. **Default listener volume mutes all sounds**
   - With no listener, `base_gain` in the mixer is `1.0f`.
   - With a listener, `base_gain` is derived from `listener_volume` in `[0, 100]`.
   - The listener's default `volume` is `0.0f`, so adding a listener without setting its volume will mute all sounds.

   **Recommendation:**
   - Change the listener default volume to `100.0f` so a newly-added listener preserves previous loudness until explicitly changed from Lua.

6. **Spatialization defaults are slightly counter-intuitive**
   - Sound components default `is_spatial = true`.
   - Listener components default `spatial = false`, so spatialization is effectively disabled until explicitly enabled on the listener.

   **Recommendation:**
   - Either:
     - Default both to spatial enabled, or
     - Default both to non-spatial and require explicit opt-in from Lua.
   - Whichever choice is made, document it clearly in the Lua-facing audio docs.

### 3. Initialization and Device Listing

7. **Potential ordering issue between system init and Lua `Sound` global**
   - `engine_register_sound_system` registers the system and then calls `sound_system_lua_init`.
   - If the engine defers calling the system's `init` until later (e.g., on the first update), `sound_system_lua_init` may see `g_sound_system_data == NULL` or an empty device list and create a `Sound` table with an empty `devices` array.
   - The current code does not later refresh `Sound.devices`.

   **Recommendation:**
   - Ensure that system `.init` is invoked synchronously as part of registration, ahead of `sound_system_lua_init`, or
   - Make `sound_system_lua_init` robust to the "not ready yet" case, and provide a way to rebuild `Sound.devices` after the sound system is initialized (e.g., a `Sound.refresh_devices()` helper).

### 4. Memory and Resource Management

8. **`ma_device_info` ownership and cleanup**
   - `ma_context_get_devices` populates `device_infos` and `device_info_count` in `SoundSystemData`.
   - The current code does not explicitly free `device_infos` before `ma_context_uninit`.

   **Recommendation:**
   - Confirm miniaudio's ownership rules for `device_infos`.
   - If the array must be freed by the app, do so in `sound_sys_shutdown` before `ma_context_uninit`, and reset both the pointer and count.

9. **Memory tags for sound system allocations**
   - Sound system allocations (e.g., `SoundSystemData`, sound and listener arrays) currently reuse `MMTAG_S_SPRITE`.

   **Recommendation:**
   - Introduce a dedicated tag for sound system allocations (e.g., `MMTAG_S_SOUND`) for clearer profiling and tracking.

10. **Component lifetime vs. system arrays**
    - The sound system stores raw pointers to components in its `sounds` and `listeners` arrays.
    - Correctness relies on components being removed from the system (via `on_component_removed`) before their memory is freed.

    **Recommendation:**
    - Ensure the ECS destroys components only after `on_component_removed` has run for all systems that track them.
    - Optionally assert in the destroy paths that components are no longer present in the sound system during development builds.

## Suggested Enhancements

These are non-blocking improvements that would extend the capabilities of the current SFX system.

### 1. Per-Sound Controls

1. **Per-sound volume**
   - Add a `volume` property on `EseEntityComponentSound` (and Lua), separate from listener/master volume.
   - Combine gains in the mixer as `total_gain = listener_gain * sound_volume` (and later any global SFX volume).

2. **Fade in/out helpers**
   - Provide Lua helpers like `comp:fade_in(duration)` / `comp:fade_out(duration)`.
   - Implement via a small per-sound state (current gain, target gain, rate) updated either in `sound_sys_update` or in a simple per-frame step inside the mixer.

3. **Pitch / playback rate control**
   - Allow basic pitch shifting via playback rate changes (e.g., `comp.pitch`), with a simple resampler.
   - Even coarse pitch control can significantly increase perceived SFX variety.

### 2. Spatial and Listener Features

4. **Multiple listeners**
   - Support more than one active listener for split-screen or special camera setups.
   - Options include mixing contributions from all listeners or designating a primary listener from Lua.

5. **Improved attenuation model**
   - Replace linear distance attenuation with a more natural curve (e.g., inverse distance or inverse-square with clamping).
   - Optionally expose a simple rolloff parameter to Lua.

6. **Listener orientation**
   - If/when entities gain orientation, use it to compute panning based on relative angle, not just raw `dx`.

### 3. System-Level Controls

7. **Global and category volumes**
   - Introduce a global SFX volume (e.g., `Sound.volume`) and possibly per-category volumes.
   - Categories can be driven by tags or explicit fields on sound components.

8. **Voice limiting and prioritization**
   - Add a configurable limit on simultaneous sounds.
   - When the limit is reached, stop or avoid starting lower-priority sounds (e.g., distant or explicitly tagged as low priority).

9. **One-shot helper API**
   - Provide a convenience function like `Sound.play_one_shot(name, x, y, options)` that creates a transient entity + sound component, plays once, and destroys itself when playback finishes.

### 4. Lua and Tooling

10. **Device refresh in Lua**
    - Add `Sound.refresh_devices()` to re-enumerate devices, repopulate `Sound.devices`, and keep `Sound.selected_device` consistent.

11. **Introspection helpers**
    - Expose helper functions to convert between frames and seconds based on the PCM's sample rate (e.g., `sound:seconds_to_frames(t)` / `sound:frames_to_seconds(f)`).

12. **Debug visualization**
    - Provide a debug overlay (or logging mode) that visualizes active sounds, their positions, gains, and listener relationships.

## Summary

The current audio system provides a solid and well-structured foundation for SFX: it integrates cleanly with the ECS and Lua layers, uses miniaudio effectively, and already supports basic spatialization. The main short-term work is to tighten up thread-safety (locking consistency and callback responsibilities), clarify default behaviors (especially around listeners and spatialization), and verify miniaudio resource ownership. Once those are addressed, the system is well positioned for incremental features like per-sound volume, fades, pitch control, and better device management from Lua.
