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
