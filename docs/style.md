## C Module Style Guide (based on `map.[ch]` and `map_cell.[ch]`)

This guide defines the comment style, file layout, and naming conventions used by the types modules. It is written so an AI can replicate the same structure and comments when creating or updating other files.

### High-level principles
- Keep headers (`.h`) as the source of truth for public API documentation using rich Doxygen comments.
- Keep implementations (`.c`) lightweight: use section banners and short one-line comments for private helpers and Lua glue. Avoid full Doxygen in `.c` for private/internal functions.
- Group code by functionality with clear section banners. Prefer readability and consistent ordering over micro-optimizations.
- Use only the project memory API (`memory_manager`) for all allocations and frees.

### Naming
- Public API functions: `ese_<topic>_<action>` (e.g., `ese_map_get_cell`, `ese_map_cell_add_layer`).
- Private/internal functions: prefix with a single leading underscore (e.g., `_ese_map_make`, `_ese_map_cell_notify_watchers`). These are `static` and live only in the `.c` file.
- Lua-bound C functions (private): also `static`, prefixed with underscore, and named by role (e.g., `_ese_map_lua_index`, `_ese_map_cell_lua_tostring`).
- Metatable names are string macros: `MAP_PROXY_META`, `MAP_CELL_PROXY_META` in headers.

### Includes ordering
1. System headers (alphabetical)
2. Project headers (alphabetical)

Example in `.c`:
```c
#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/lua/src/lauxlib.h"
#include "types/map_private.h"
#include "types/map_cell.h"
```

### Section banners in `.c`
Use banner comments to structure files into predictable sections. Exact format:
```c
/* --- Section Name ----------------------------------------------------------------------------- */
```
Common sections, in order:
1. Defines
2. Structs
3. Forward declarations
4. Internal Helpers
5. Lua Methods
6. Lua Init
7. C API
8. Feature-specific API blocks (e.g., Tile/Flag API)
9. Watcher API

Notes:
- Keep headers for these sections concise and consistent with the examples in `map.c` and `map_cell.c`.
- Sub-grouping comments (single line `// ...`) may be used inside a section to cluster related functions.

### Comment style
- Private/internal functions (leading underscore): use brief, single-line comments with `///` above the declaration or definition. Do not use multi-line Doxygen blocks for private functions.
  - Example: `/// Notify all registered MapCell watchers of a change.`
- Lua metamethods and methods: prefix the `///` one-liner with `Lua:` to make intent clear (e.g., `/// Lua: __index metamethod for Map.`).
- Public API documentation lives in `.h` and uses Doxygen blocks with `@brief`, `@param`, `@return`, and optional `@details`/`@warning` sections.
- In `.c`, public functions generally do not repeat the full Doxygen documentation; rely on the header.

### Header (`.h`) layout
Order and structure:
1. Include guard
2. System includes
3. Project includes or forward `typedef struct` declarations
4. Public macros (e.g., metatable names)
5. Public typedefs and enums (with Doxygen comments)
6. Themed sections with banners, mirroring the implementation where it helps discovery. Examples used in `map.h`:
   - `/* --- Lua API ---------------------------------------------------------------------------------- */`
   - `/* --- C API ------------------------------------------------------------------------------------ */`
   - `/* --- Map Operations --------------------------------------------------------------------------- */`
   - `/* --- Map Type Conversion ---------------------------------------------------------------------- */`
7. Public function declarations with Doxygen blocks

Header Doxygen example:
```c
/**
 * @brief Gets a map cell at the specified coordinates.
 * @param map Pointer to the EseMap object
 * @param x X coordinate
 * @param y Y coordinate
 * @return Pointer to the EseMapCell, or NULL if out of bounds
 */
EseMapCell *ese_map_get_cell(const EseMap *map, uint32_t x, uint32_t y);
```

### Implementation (`.c`) layout template
Use this as a starting skeleton for new modules.
```c
#include <string.h>
#include "core/memory_manager.h"
#include "utility/log.h"
#include "types/<topic>_private.h"
#include "types/<topic>.h"

/* --- Defines ---------------------------------------------------------------------------------- */

/* --- Structs ---------------------------------------------------------------------------------- */
// Private struct definitions (if any)

/* --- Forward declarations --------------------------------------------------------------------- */
/// Brief one-liner description.
static void _<topic>_helper(...);

/* --- Internal Helpers ------------------------------------------------------------------------- */
static void _<topic>_helper(...) {
    // implementation
}

/* --- Lua Methods ------------------------------------------------------------------------------ */
/// Lua: __index metamethod for <Topic>.
static int _<topic>_lua_index(lua_State *L) { /* ... */ }

/* --- Lua Init --------------------------------------------------------------------------------- */
void <topic>_lua_init(EseLuaEngine *engine) { /* register metatable */ }

/* --- C API ------------------------------------------------------------------------------------ */
<Public API implementations relying on header docs>

/* --- Watcher API ------------------------------------------------------------------------------ */
<watcher add/remove/notify helpers>
```

### Lua integration conventions
- Metatables are created once per engine and locked by setting `__metatable = "locked"`.
- `*_lua_push` creates a userdata and sets the correct metatable when there is no registry reference; otherwise, it retrieves the referenced userdata with `lua_rawgeti`.
- Reference management pairs: `*_ref` increments/stores a registry reference, `*_unref` decrements and clears when reaching zero. Maintain a `lua_ref` and `lua_ref_count` in the object when needed.
- Property access from Lua is exposed via `__index`/`__newindex` metamethods that dispatch by string key.

### Watcher pattern
- Expose `add_watcher`/`remove_watcher` in public API.
- Internally, allocate arrays for callbacks and `userdata` lazily; grow capacity by doubling when full.
- Central notify helper iterates and calls registered callbacks.
- When mutations occur, call the notify helper and mark any affected cached values as dirty (e.g., `_ese_map_set_layer_count_dirty`).

### Memory and lifecycle
- Allocate and free with `memory_manager` exclusively. Do not call `malloc/free/realloc/strdup` directly.
- For composite objects (e.g., `EseMap` holds `EseMapCell***`): create helper allocators/finalizers (`_allocate_*`, `_free_*`) and invoke them from public constructors/destructors.
- If Lua holds references, destruction functions should defer freeing internals until references are released, mirroring `ese_map_destroy` and `ese_map_cell_destroy`.

### Error handling and diagnostics
- Validate inputs with `log_assert` for public API entry points where appropriate.
- For Lua functions, prefer returning informative `luaL_error` messages for invalid arguments.
- Optional profiling hooks can wrap hot Lua metamethods/methods (see `profile_start/stop` usage).

### Quick checklists
- New header:
  - Include guard, includes, forward decls
  - Public macros, enums/typedefs with Doxygen
  - Public functions grouped under bannered sections with Doxygen blocks
- New implementation:
  - Banners present, includes ordered
  - Private helpers `static` with `_` prefix and `///` one-liners
  - Lua glue functions private, `Lua:` one-liner comments
  - Watcher notify helper if there is a watcher API
  - All allocations/frees via `memory_manager`

### Minimal examples
Header snippet:
```c
#ifndef ESE_FOO_H
#define ESE_FOO_H

/* --- C API ------------------------------------------------------------------------------------ */
/** @brief Creates a new Foo. */
Foo *ese_foo_create(...);
/** @brief Destroys a Foo. */
void ese_foo_destroy(Foo *foo);

#endif // ESE_FOO_H
```

Implementation snippet:
```c
/* --- Forward declarations --------------------------------------------------------------------- */
/// Internal creation helper.
static Foo *_ese_foo_make(...);

/* --- Internal Helpers ------------------------------------------------------------------------- */
static Foo *_ese_foo_make(...) { /* ... */ }

/* --- C API ------------------------------------------------------------------------------------ */
Foo *ese_foo_create(...) { /* ... */ }
void ese_foo_destroy(Foo *foo) { /* ... */ }
```


