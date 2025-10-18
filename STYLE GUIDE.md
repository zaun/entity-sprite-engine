## Environment Varaibles
- LOG_CATEGORIES=ALL : Display all debugging loggs
- LOG_VERBOSE=1 : Turn on verbose logs, requires LOG_CATEGORIES to be set as well.

## C Module Style Guide

This guide defines the comment style, file layout, and naming conventions used by the types
modules. It is written so an AI can replicate the same structure and comments when creating or
updating other files.

## General Code Rules
- Use 4 spaces for indentation.
- Use 100 characters or less per line.
- Empty line at end of file.
- snake_case for function and variable names.
- Uppercase SNAKE_CASE for macros and enums.
- PascalCase for struct names.
- All if statements should use braces.
- Never use goto statements.

## High-level principles
- Group code by functionality with clear section banners. Prefer readability and consistent
  ordering over micro-optimizations.
- Use only the project memory API (`memory_manager`) for all allocations and frees.

### Memory Management
- All allocations must use memory_manager.malloc() with appropriate tags.
- Memory tags should be defined in memory_manager.h and used consistently
- All frees must use memory_manager.free().
- Do not NULL check memory_manager calls.
- Do not NULL check _create() functions.
- Do not NULL check _copy() functions.
- No direct malloc(), calloc(), realloc(), or free() calls unless dealing with 3rd party libraries.


### File headers & licensing:
- A standard comment block at the top of every .h/.c (project name, brief description,
  copyright/licensing).
- should reference the LICENSE.md file.

```c
/*
 * project: Entity Sprite Engine
 *
 * <brief description of file's purpose, 100 characters or less per line, 5 lines or less>
 * 
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
```

### Includes ordering
1. System headers (alphabetical)
2. Project headers (alphabetical)

### Doxygen function example:
```c
/**
 * @brief Short description of the function.
 *
 * @details Detailed logic of the function can be added and can be multi-line. Only needed
 *          if the function logic is complex.
 *
 * @param argumentA Type description of argument
 * @param argumentB Type description of argument
 * @param argumentC Type description of argument
 * @return Type description of data
 */
Type *function_name(const Type *argument, Type argument, Type out_argument);
```

### Section banners and banner order to use:

```c
// ========================================
// Defines and Structs
// ========================================
```

```c
// ========================================
// FORWARD DECLARATIONS
// ========================================
```

```c
// ========================================
// PRIVATE FUNCTIONS
// ========================================
```

```c
// ========================================
// PUBLIC FUNCTIONS
// ========================================
```

### Naming Conventions:
- Public API functions: `ese_<topic>_<action>` (e.g., `ese_map_get_cell`, `ese_map_cell_add_layer`).
- Internal functions: prefix with a single leading underscore (e.g., `_ese_map_make`
  `_ese_map_cell_notify_watchers`). These are `static` and live only in the `.c` file.
- Private functions: prefix with a single leading underscore (e.g., `_ese_map_make`,
  `_ese_map_cell_notify_watchers`). These are not static and definition lies in
  the `_private.h` file
- Lua-bound C functions (private): also `static`, prefixed with underscore, and named by role
  (e.g., `_ese_map_lua_index`, `_ese_map_cell_lua_tostring`). live in `_lua.c` file.
- constants and macros are either typedef enum or #define macros.
- Either `#define NAME value` or
  ```c
	typedef enum {
	    DEFAULT_GROUP = 0,
	    DEFAULT_GROUP_2 = 1,
	} EseGroup;
  ```

### File-naming conventions
- `<module>.h` - public header file
- `<module>_private.h` - private header file (should only be included in the implementation,
                         lua glue file and unit tests)
- `<module>.c` - implementation file
- `<module>_lua.c` - Lua glue file (should only be included in the implementation file)
- `test_<module>.c` - unit test file (should only be included in the tests directory)

## Unit Testing

This guide defines how unit tests are wired, how files are laid out, and the conventions to
follow when adding or updating tests.

### How tests are wired (CMake + Unity)

- All test sources live in the `tests/` directory and are named `test_<module>.c`.
- The `tests/CMakeLists.txt` auto-discovers `test_*.c` files and builds one runnable
  executable per file, linking against the `unity` test framework and the
  `entityspriteengine` static library.
- Each built test binary is also registered as a CTest. CTest sets the environment variable
  `ESETEST=1` for all tests.
- To build and run tests:
  1) From the project root, create or enter the `build/` directory.
  2) Run `cmake ..` then `make`.
  3) Run `ctest --output-on-failure` to execute all tests, or run individual
     test executables under `build/tests/` directly.

### Test file layout

Each `tests/test_<module>.c` should follow this structure (sections optional if unused):

1. Includes
   - System headers as needed
   - `"testing.h"` (provides `ASSERT_DEATH` and `create_test_engine`)
   - The module headers under test and any required project headers
2. Forward declarations for test functions
   - Group into "C API" and "Lua API" declarations for readability
3. Local test helpers/mocks (static)
4. Suite fixtures
   - `setUp` and `tearDown` functions used by Unity
5. Main test runner
   - Initialize logging, start Unity, `RUN_TEST(...)` each test, destroy memory manager,
     and return Unity’s exit code
6. Test functions (implementation)
   - First C API tests, then Lua API tests

Example runner skeleton:

```c
int main(void) {
    log_init();
    UNITY_BEGIN();
    RUN_TEST(test_module_behavior);
    memory_manager.destroy();
    return UNITY_END();
}
```

### Engine and Lua wiring

- Include `"testing.h"` to use `create_test_engine()` which returns an initialized
  `EseLuaEngine*` and registers it in Lua's registry under `LUA_ENGINE_KEY`.
- Preferred pattern is to keep a module-global `static EseLuaEngine *g_engine` and set it in
  `setUp()`; destroy it in `tearDown()`:

```c
static EseLuaEngine *g_engine = NULL;

void setUp(void) {
    g_engine = create_test_engine();
}

void tearDown(void) {
    lua_engine_destroy(g_engine);
}
```

- When testing Lua glue:
  - Call `<type>_lua_init(g_engine)` before executing any Lua code for that type
  - Use `luaL_dostring(L, code)` and assert `LUA_OK` (or not) as appropriate
  - Pop any values you read from the Lua stack with `lua_pop`
  - Follow the project rule for instance vs. static method calls (use `:` for instance,
    `.` for static)

### Naming and organization

- File name: `tests/test_<module>.c`
- Test names: `test_ese_<module>_<behavior>`
- Group tests logically:
  - C API tests first (creation, getters/setters, algorithms, serialization, watchers, etc.)
  - Lua API tests next (init, push/get, properties, methods, JSON, GC, etc.)
- Keep tests small and focused. Prefer one behavior per test. For Lua properties, write
  separate tests per property and separate getter/setter checks.

### Assertions and death tests

- Use Unity assertions (`TEST_ASSERT_*`) consistently; prefer `TEST_ASSERT_FLOAT_WITHIN` for
  float comparisons and `TEST_ASSERT_EQUAL_PTR` for pointers.
- For API contracts that abort on invalid input, use `ASSERT_DEATH(expr, msg)` from
  `testing.h`.
  - If `expr` contains commas, wrap it in parentheses: `ASSERT_DEATH((fn(a, b)), "msg")`.
  - This macro is POSIX-only (uses `fork`/`wait`/signals) and is suitable for CI on macOS
    and Linux.

### Memory and resource rules in tests

- Do not call `malloc`/`free` directly; use the module’s `*_create`, `*_destroy`, `*_copy`,
  etc., per project rules.
- Always destroy objects you create in a test.
- Call `memory_manager.destroy()` once in the test runner after all tests have executed to
  validate clean teardown.

### Coverage expectations

- Aim to cover the full public C API of the module under test.
- Where Lua bindings exist, include Lua API coverage as well (init, constructors, instance
  methods, metamethods like `__tostring`, JSON round-trips, and GC behavior).


