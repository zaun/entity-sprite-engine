# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

## Repository overview

ESE (Entity Sprite Engine) is a C/C++ 2D game engine with a LuaJIT scripting layer and an Entity–Component–System (ECS) architecture. The core engine is built as a static library (`entityspriteengine`) with platform-specific backends (Metal on macOS, OpenGL/GLFW on Linux). Tests, examples, and Lua-facing documentation live alongside the engine sources.

Key directories:
- `src/` – engine implementation
  - `core/` – engine lifecycle, asset management, memory manager, job queue, HTTP, console, etc.
  - `entity/` – entities, components, ECS systems, and their Lua bindings
  - `graphics/` – renderer abstraction, draw lists, sprite/tile/GUI rendering
  - `platform/` – platform layer (macOS Metal / Linux GLFW+OpenGL, audio, filesystem, time)
  - `scripting/` – Lua engine, Lua value wrappers, global environment setup
  - `types/` – primitives like `point`, `rect`, `vector`, maps, UUIDs, etc.
  - `utility/` – generic containers, logging, grouped hashmaps, etc.
  - `vendor/` – third-party code (LuaJIT, cJSON, SPIR-V tooling, mbedtls)
- `examples/` – demo applications (e.g. `examples/simple`) that link against the engine
- `tests/` – C test suite using Unity, one executable per `test_*.c`
- `docs/` – API docs for Lua and engine concepts (`global.md`, `entity.md`, `map.md`, etc.)

Refer to `README.md` for a feature list, getting-started Lua examples, and a more detailed project structure diagram.

## Build and run

All commands below assume the project root as the working directory.

### Prerequisites

- CMake ≥ 3.16
- C/C++ toolchain (C99 + C++11)
- macOS: Xcode / clang with Metal and Cocoa frameworks
- Linux: development packages for OpenGL, GLFW3, GLEW, OpenAL
- No external LuaJIT install is needed; LuaJIT is vendored and built as part of the build.

### Configure and build

```bash
mkdir -p build
cd build

# Default configuration (auto-detects platform)
cmake ..
cmake --build .
```

Platform overrides (if auto-detection isn’t what you want):

```bash
# macOS (Metal renderer)
cmake -DFORCE_PLATFORM_MAC=ON ..
cmake --build .

# Linux (GLFW/OpenGL renderer)
cmake -DFORCE_PLATFORM_LINUX=ON ..
cmake --build .
```

Sanitizers (AddressSanitizer + UBSan):

```bash
cmake -DSANITIZE=ON ..
cmake --build .
```

The main build products in `build/` are:
- `libentityspriteengine.a` – static engine library
- `examples/...` – example binaries/app bundles
- `tests/...` – unit test executables

### Running the example demo

From `build/` after a successful build:

```bash
# macOS app bundle
open examples/simple/simple_demo.app

# Linux executable
cd examples/simple
./simple_demo
```

## Testing and coverage

Tests live in `tests/`, use Unity, and are auto-discovered by `tests/CMakeLists.txt` as one executable per `test_*.c`.

### Build tests

Tests are built as part of the normal build once CMake has configured the `tests` subdirectory:

```bash
cd build
cmake --build .
```

You can also build specific test targets if needed (targets are named after the source file without `.c`):

```bash
cmake --build . --target test_point
cmake --build . --target test_rect
```

### Run all tests

From `build/`:

```bash
ctest --output-on-failure
```

Alternatively, use the `check` custom target defined in `tests/CMakeLists.txt`:

```bash
cmake --build . --target check
```

### Run a single test

From `build/`:

```bash
# Run by ctest name (recommended)
ctest -R test_point --output-on-failure

# Or run the executable directly
./tests/test_point
```

Environment:
- CTest sets `ESETEST=1` for all registered tests.

### Coverage build

Coverage flags are opt-in via `BUILD_COVERAGE` in `tests/CMakeLists.txt`:

```bash
mkdir -p build
cd build
cmake -DBUILD_COVERAGE=ON ..
cmake --build .
cmake --build . --target coverage
```

If `lcov` and `genhtml` are installed, the `coverage` target will generate an HTML report under `build/coverage/html`.

## Formatting and style

### clang-format

The repository includes a `.clang-format` configured with:
- LLVM base style
- 4-space indentation, no tabs
- 100-character column limit

Use `clang-format` to format C/C++/ObjC files before committing changes, for example:

```bash
clang-format -i src/core/engine.c
```

### C style and project conventions

See `STYLE GUIDE.md` for full details. Important points for generated C code:

- Indentation and layout
  - 4 spaces per indent; no tabs; aim for ≤100 characters per line.
  - Always use braces on `if` statements, even for single statements.
- Naming
  - Functions and variables: `snake_case`.
  - Macros and enums: `UPPER_SNAKE_CASE`.
  - Struct types: `PascalCase`.
  - Public API functions generally follow `ese_<topic>_<action>` (e.g. `ese_map_get_cell`).
  - Internal/"private" functions use a single leading underscore and live in the `.c` or `_private.h` file as appropriate.
- File naming
  - `<module>.h` – public header (with include guards, not `#pragma once`).
  - `<module>_private.h` – private header used only by implementation, Lua glue, and tests.
  - `<module>.c` – implementation.
  - `<module>_lua.c` – Lua glue for that module.
  - `test_<module>.c` – unit tests under `tests/`.
- Logging
  - Use `log_error`, `log_debug`, `log_verbose`, and `log_assert` from `utility/log.[ch]` with a category string (e.g. `"CAMERA"`, `"MEMORY_MANAGER"`).
  - Reserve `log_error` for unrecoverable failures, `log_debug` for normal internal diagnostics, `log_verbose` for high-volume traces, and `log_assert` for invariants/contracts.
- Memory management
  - All allocations must go through the project `memory_manager` API (`memory_manager.malloc`, `memory_manager.calloc`, `memory_manager.realloc`, `memory_manager.free`, etc.).
  - Do not call `malloc`, `calloc`, `realloc`, or `free` directly inside engine code except where interfacing with third-party libraries.
  - Memory tags are defined in `memory_manager.h` and should be used consistently.

Environment variables useful during development (from `STYLE GUIDE.md`):
- `LOG_CATEGORIES=ALL` – enable all logging categories (or a comma-separated subset like `MEMORY_MANAGER,RENDERER`).
- `LOG_VERBOSE=1` – enable verbose logs (requires `LOG_CATEGORIES` to be set).

## Engine and ECS architecture

### Engine core (`src/core/`)

The engine is orchestrated via `core/engine.[ch]` and associated modules:
- `engine_create` bootstraps the renderer, Lua engine, asset manager, and job queue.
- `engine_update` runs the per-frame loop: input handling, ECS systems, Lua scripts, and preparing draw lists.
- The engine exposes helpers for collision queries, entity lookup by tag/UUID, console logging, and access to the GUI context and job queue.

The engine uses a custom `memory_manager` and job queue; code that needs threading should integrate with the job queue rather than spawning threads directly.

### ECS: entities, components, and systems

Entities and components live under `src/entity/`:
- `entity.[ch]` – defines `EseEntity`, entity lifecycle (`entity_create`, `entity_destroy`, `entity_update`), component management, tags, collision helpers, and drawing callbacks.
- `components/` – data and behavior for components such as sprites, colliders, maps, shapes, text, and Lua-scripted behavior (`entity_component_sprite`, `entity_component_collider`, `entity_component_map`, etc.).
- `entity_component.[ch]` – shared component operations (copy/destroy, update, collision dispatch, drawing, Lua function invocation).

Systems are defined via the generic system manager in `core/system_manager.[ch]` and concrete system implementations under `entity/systems/`:
- `core/system_manager.[ch]` – declares `EseSystemManager`, `EseSystemManagerVTable`, and `EseSystemPhase` (`EARLY`, `LUA`, `LATE`, `CLEANUP`), plus helpers `engine_add_system`, `engine_run_phase`, `engine_notify_comp_add/rem`.
- `entity/systems/*.h` – system-specific headers (sprite animation, render systems for sprites/maps/text/shapes, Lua system, cleanup system).

The pattern for a new system is:
1. Define an `EseSystemManagerVTable` with the callbacks you need (`init`, `update`, `accepts`, `on_component_added`, `on_component_removed`, `shutdown`).
2. Use `system_manager_create(vtable, phase, user_data)` to create the system and `engine_add_system` to register it.
3. Use `accepts` + `on_component_added/removed` to maintain a dense internal collection of components to operate on each frame.

`src/EseSystem.md` contains a detailed design document and worked example (animation system) showing how systems replace per-component update logic and how they integrate with the job queue.

### Rendering and platform layers

Rendering is organized around draw lists and platform-specific backends:
- `graphics/` – draw lists, sprite/shape/text rendering, camera, GUI rendering, shader and tilemap support.
- `platform/mac` vs `platform/glfw` – window creation, renderer integration (Metal vs OpenGL), filesystem, time, and audio.

The build selects one platform at configure time (`PLATFORM` set to `"MAC"` or `"LINUX"`), and only the relevant platform sources are compiled.

### Scripting and Lua environment

Lua integration is under `src/scripting/` and `docs/`:
- `scripting/lua_engine.[ch]` and related files – embedding LuaJIT, sandboxed global environment, script loading, and bridging Lua values to C.
- `entity/*_lua.c` and component `_lua` files – Lua bindings for entities, components, and engine APIs.
- `docs/global.md` – documents the sandboxed global Lua environment, available standard libraries (`math`, `string`, `table`), global functions (`print`, `asset_load_script`, `asset_load_atlas`, `asset_load_shader`, `asset_load_map`, etc.), and security restrictions.

Lua scripts in games talk to the engine almost exclusively through these bindings; when adding new engine features intended for Lua, follow the existing `_lua.c` patterns and update the relevant docs under `docs/`.

### Types, utilities, and docs

- `types/` – core math and geometry types (`point`, `rect`, `vector`, `ray`, `tileset`, `uuid`, etc.) with corresponding docs under `docs/` (`point.md`, `rect.md`, `vector.md`, `uuid.md`, etc.).
- `utility/` – reusable infrastructure such as arrays, hashmaps, grouped hashmaps, job queue, logging, and testing helpers.
- `docs/` – source-of-truth documentation for Lua and engine-facing APIs; consult these files when adding or modifying public behaviors.

## Working with tests

The test suite is structured so each `test_<module>.c` exercises a single module (often both its C API and Lua glue):
- `tests/testing.h` provides shared helpers, including `create_test_engine` and `TEST_ASSERT_DEATH`.
- Tests typically:
  - Initialize logging and the memory manager.
  - Create a test engine (for Lua-related tests).
  - Use Unity’s `RUN_TEST` macros and then destroy the memory manager once at the end of the run.

When adding new engine or Lua-visible functionality, follow the existing patterns in `tests/test_*.c` and wire the new test file into CMake implicitly (matching `test_*.c` is enough).

## When generating or editing code

When Warp modifies or adds C code in this repo, it should:
- Respect the ECS and System architecture: prefer adding/updating systems under `entity/systems/` and using `core/system_manager` instead of embedding per-component update logic directly in components where possible.
- Use the project memory manager, logging API, naming conventions, and file layout rules summarized above and detailed in `STYLE GUIDE.md`.
- Keep Lua-facing changes consistent with existing `_lua.c` bindings and update `docs/*.md` where public APIs change or expand.