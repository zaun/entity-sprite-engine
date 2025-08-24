# MapCell Lua API

The `MapCell` API provides Lua bindings for working with **map grid cells**.  
A map cell can contain multiple **tile layers**, **flags**, and a **dynamic state**.

---

## Overview

In Lua, map cells are represented as **proxy tables** that behave like regular Lua objects.  
They behave like objects with properties and methods accessible via dot notation.

**Important Notes:**
- **Tile layer management** - each cell can contain multiple stacked tile layers
- **Dynamic rendering control** - `isDynamic` flag controls whether the map renderer processes the cell
- **Flag system** - bitfield flags for game logic and cell properties
- **Layer indexing** - tile layers use 0-based indexing for consistency with C arrays
- **Automatic resizing** - the tile layer array automatically grows as needed

```lua
-- Create a new map cell
local cell = MapCell.new()

-- Access properties
print(cell.isDynamic, cell.flags, cell.layer_count)

-- Modify properties
cell.isDynamic = true
cell.flags = 0x01
```

---

## Global MapCell Table

The `MapCell` table is a global table created by the engine during initialization. It provides a constructor for creating new map cell objects.

---

## Global MapCell Table Methods

### `MapCell.new()`
Creates a new empty map cell with default values.  

**Returns:** `MapCell` object  

**Notes:**
- **Empty initial state** - starts with no tile layers, isDynamic=false, flags=0
- **Initial capacity** - tile layer array starts with capacity for 4 layers
- **Lua-owned** - the returned cell is owned by Lua and will be garbage collected
- **Memory allocation** - automatically allocates memory for tile layer storage
- **Default values** - isDynamic=false, flags=0, layer_count=0

**Example:**
```lua
local cell = MapCell.new()
print("New cell created")  --> New cell created
print("Default isDynamic:", cell.isDynamic)  --> false
print("Default flags:", cell.flags)  --> 0
print("Initial layer count:", cell.layer_count)  --> 0
```

---

## MapCell Object Properties

Each `MapCell` object has the following properties:

- `isDynamic` → boolean (read/write)  
  - If `false` (default), the map renderer draws all layers.  
  - If `true`, the cell is ignored by the renderer (for dynamic/interactive cells).  

- `flags` → number (bitfield, read/write)  
  - Arbitrary flags for game logic.  

- `layer_count` → number (read-only)  
  - The number of tile layers currently in the cell.  

**Notes:**
- **isDynamic** - controls whether the map component renders this cell
- **flags** - unsigned integer for storing arbitrary bit flags
- **layer_count** - automatically updated when layers are added/removed
- **Type validation** - flags must be a number, isDynamic must be a boolean
- **Rendering behavior** - dynamic cells are typically used for interactive elements

**Example:**
```lua
local cell = MapCell.new()
cell.isDynamic = true
cell.flags = 0x04
print(cell.layer_count)  --> 0

-- Set multiple flags
cell.flags = 0x01 | 0x02 | 0x04  -- Set flags 1, 2, and 4
print("Flags:", cell.flags)  --> 7

-- Check if dynamic
if cell.isDynamic then
    print("Cell is dynamic - will not be rendered by map component")
end
```

---

## MapCell Object Methods

### `cell:add_layer(tile_id)`
Adds a new tile layer to the cell.  

**Arguments:**
- `tile_id` → number (tile identifier, 0-255)  

**Returns:** `true` if successful, `false` if allocation failed.  

**Notes:**
- **Appends to end** - new layers are added after existing layers
- **Automatic resizing** - the tile layer array grows automatically if needed
- **Tile ID range** - tile IDs are stored as 8-bit values (0-255)
- **Memory management** - automatically allocates memory for new layers
- **Return value** - check return value to ensure layer was added successfully

**Example:**
```lua
local cell = MapCell.new()

-- Add tile layers
local success = cell:add_layer(5)
if success then
    print("Added layer with tile ID 5")
else
    print("Failed to add layer")
end

cell:add_layer(7)
cell:add_layer(12)
print("Layer count after adding:", cell.layer_count)  --> 3

-- Verify layers were added
print("Layer 0:", cell:get_layer(0))  --> 5
print("Layer 1:", cell:get_layer(1))  --> 7
print("Layer 2:", cell:get_layer(2))  --> 12
```

---

### `cell:remove_layer(index)`
Removes a tile layer by index (0-based).  

**Arguments:**
- `index` → number (0-based layer index)  

**Returns:** `true` if successful, `false` if index is out of bounds.  

**Notes:**
- **0-based indexing** - first layer is at index 0, second at index 1, etc.
- **Bounds checking** - returns false if index is negative or >= layer_count
- **Array shifting** - remaining layers are shifted to fill the gap

- **Index validation** - always check return value to ensure removal succeeded

**Example:**
```lua
local cell = MapCell.new()
cell:add_layer(10)
cell:add_layer(20)
cell:add_layer(30)

print("Before removal:", cell.layer_count)  --> 3

-- Remove middle layer
local success = cell:remove_layer(1)
if success then
    print("Removed layer at index 1")
else
    print("Failed to remove layer")
end

print("After removal:", cell.layer_count)  --> 2

-- Verify remaining layers
print("Layer 0:", cell:get_layer(0))  --> 10
print("Layer 1:", cell:get_layer(1))  --> 30
```

---

### `cell:get_layer(index)`
Gets the tile ID at a given layer index.  

**Arguments:**
- `index` → number (0-based layer index)  

**Returns:** tile ID (number), or `0` if index is out of bounds.  

**Notes:**
- **0-based indexing** - first layer is at index 0, second at index 1, etc.
- **Bounds checking** - returns 0 if index is negative or >= layer_count
- **Tile ID range** - returns values in range 0-255
- **Safe access** - always returns a valid number, never nil
- **Performance** - direct array access, very fast operation

**Example:**
```lua
local cell = MapCell.new()
cell:add_layer(42)
cell:add_layer(73)
cell:add_layer(15)

-- Access layers by index
local id0 = cell:get_layer(0)
local id1 = cell:get_layer(1)
local id2 = cell:get_layer(2)

print("Layer 0 tile ID:", id0)  --> 42
print("Layer 1 tile ID:", id1)  --> 73
print("Layer 2 tile ID:", id2)  --> 15

-- Out of bounds access
local invalid = cell:get_layer(5)
print("Invalid index result:", invalid)  --> 0
```

---

### `cell:set_layer(index, tile_id)`
Sets the tile ID at a given layer index.  

**Arguments:**
- `index` → number (0-based layer index)  
- `tile_id` → number (new tile identifier, 0-255)  

**Returns:** `true` if successful, `false` if index is out of bounds.  

**Notes:**
- **0-based indexing** - first layer is at index 0, second at index 1, etc.
- **Bounds checking** - returns false if index is negative or >= layer_count
- **Tile ID range** - tile IDs are stored as 8-bit values (0-255)
- **In-place modification** - changes existing layer without adding/removing
- **No memory allocation** - modifies existing layer, no new memory needed

**Example:**
```lua
local cell = MapCell.new()
cell:add_layer(10)
cell:add_layer(20)

print("Before modification:")
print("Layer 0:", cell:get_layer(0))  --> 10
print("Layer 1:", cell:get_layer(1))  --> 20

-- Modify existing layer
local success = cell:set_layer(0, 99)
if success then
    print("Modified layer 0 to tile ID 99")
else
    print("Failed to modify layer")
end

print("After modification:")
print("Layer 0:", cell:get_layer(0))  --> 99
print("Layer 1:", cell:get_layer(1))  --> 20

-- Try to modify non-existent layer
local invalid_success = cell:set_layer(5, 100)
if not invalid_success then
    print("Cannot modify non-existent layer")
end
```

---

### `cell:clear_layers()`
Removes all tile layers from the cell.  

**Returns:** nothing

**Notes:**
- **Complete removal** - removes all layers, sets layer_count to 0
- **Memory cleanup** - frees all allocated tile layer memory
- **Reset state** - cell returns to initial empty state
- **No return value** - always succeeds (no failure condition)
- **Efficient operation** - single operation instead of multiple remove calls

**Example:**
```lua
local cell = MapCell.new()
cell:add_layer(10)
cell:add_layer(20)
cell:add_layer(30)

print("Before clear:", cell.layer_count)  --> 3

-- Clear all layers
cell:clear_layers()

print("After clear:", cell.layer_count)  --> 0

-- Verify all layers are gone
for i = 0, 2 do
    local id = cell:get_layer(i)
    print("Layer", i, ":", id)  --> All return 0
end
```

---

### `cell:has_flag(flag)`
Checks if a specific flag is set.  

**Arguments:**
- `flag` → number (bitmask to check)  

**Returns:** `true` if flag is set, `false` otherwise.  

**Notes:**
- **Bitwise operation** - uses bitwise AND to check if flag is set
- **Flag values** - typically use powers of 2 (1, 2, 4, 8, 16, 32, etc.)
- **Multiple flags** - can check for combinations using bitwise OR
- **Efficient checking** - very fast bitwise operation
- **Common usage** - checking cell properties like "walkable", "damaging", etc.

**Example:**
```lua
local cell = MapCell.new()

-- Set some flags
cell.flags = 0x01 | 0x04 | 0x08  -- Set flags 1, 4, and 8

-- Check individual flags
print("Has flag 1:", cell:has_flag(0x01))   --> true
print("Has flag 2:", cell:has_flag(0x02))   --> false
print("Has flag 4:", cell:has_flag(0x04))   --> true
print("Has flag 8:", cell:has_flag(0x08))   --> true

-- Check combination of flags
local walkable_and_damaging = 0x01 | 0x04
print("Is walkable and damaging:", cell:has_flag(walkable_and_damaging))  --> true

-- Common flag constants
local WALKABLE = 0x01
local DAMAGING = 0x02
local INTERACTIVE = 0x04

if cell:has_flag(WALKABLE) then
    print("Player can walk on this cell")
end

if cell:has_flag(DAMAGING) then
    print("This cell damages the player")
end
```

---

### `cell:set_flag(flag)`
Sets a specific flag.  

**Arguments:**
- `flag` → number (bitmask to set)  

**Returns:** nothing

**Notes:**
- **Bitwise operation** - uses bitwise OR to set the flag
- **Preserves existing flags** - doesn't clear other flags
- **Flag values** - typically use powers of 2 (1, 2, 4, 8, 16, 32, etc.)
- **Multiple flags** - can set multiple flags at once using bitwise OR
- **No return value** - always succeeds

**Example:**
```lua
local cell = MapCell.new()

-- Set individual flags
cell:set_flag(0x01)  -- Set flag 1
cell:set_flag(0x04)  -- Set flag 4

print("Flags after setting:", cell.flags)  --> 5 (binary: 101)

-- Set multiple flags at once
cell:set_flag(0x02 | 0x08)  -- Set flags 2 and 8

print("Flags after multiple set:", cell.flags)  --> 15 (binary: 1111)

-- Common flag constants
local WALKABLE = 0x01
local DAMAGING = 0x02
local INTERACTIVE = 0x04
local SECRET = 0x08

-- Set cell properties
cell:set_flag(WALKABLE)      -- Player can walk here
cell:set_flag(INTERACTIVE)   -- Player can interact with this cell
```

---

### `cell:clear_flag(flag)`
Clears a specific flag.  

**Arguments:**
- `flag` → number (bitmask to clear)  

**Returns:** nothing

**Notes:**
- **Bitwise operation** - uses bitwise AND with NOT to clear the flag
- **Preserves other flags** - only clears the specified flag
- **Flag values** - typically use powers of 2 (1, 2, 4, 8, 16, 32, etc.)
- **Multiple flags** - can clear multiple flags at once using bitwise OR
- **No return value** - always succeeds

**Example:**
```lua
local cell = MapCell.new()

-- Set some flags
cell.flags = 0x01 | 0x02 | 0x04 | 0x08
print("Initial flags:", cell.flags)  --> 15 (binary: 1111)

-- Clear individual flags
cell:clear_flag(0x02)  -- Clear flag 2
print("After clearing flag 2:", cell.flags)  --> 13 (binary: 1101)

cell:clear_flag(0x08)  -- Clear flag 8
print("After clearing flag 8:", cell.flags)  --> 5 (binary: 0101)

-- Clear multiple flags at once
cell:clear_flag(0x01 | 0x04)  -- Clear flags 1 and 4
print("After clearing multiple:", cell.flags)  --> 0 (binary: 0000)

-- Common flag constants
local WALKABLE = 0x01
local DAMAGING = 0x02

-- Toggle walkable state
if cell:has_flag(WALKABLE) then
    cell:clear_flag(WALKABLE)
    print("Cell is no longer walkable")
else
    cell:set_flag(WALKABLE)
    print("Cell is now walkable")
end
```

---

## Restrictions

- **Limited properties** - only `isDynamic` and `flags` can be modified:

```lua
-- ❌ Invalid - will cause Lua error
cell.layer_count = 5
-- Error: "unknown or unassignable property 'layer_count'"

cell.custom_property = "value"
-- Error: "unknown or unassignable property 'custom_property'"
```

- **Type validation** - properties have type restrictions:

```lua
-- ❌ Invalid - will cause Lua error
cell.flags = "string"
-- Error: "flags must be a number"

cell.isDynamic = "true"
-- Error: implicit boolean conversion (may not work as expected)
```

---

## MapCell Object Methods

**Note:** The MapCell API provides comprehensive tile layer and flag management methods beyond property access.

---

## Metamethods

- `tostring(cell)` → returns a string representation:  
  `"MapCell: 0x... (layers=..., flags=..., dynamic=...)"`  

**Notes:**
- The `tostring` metamethod provides a human-readable representation for debugging
- The string format includes layer count, flags, and dynamic state

---

## Complete Example

```lua
-- Create a new map cell
local cell = MapCell.new()
print(cell)  --> MapCell: 0x... (layers=0, flags=0, dynamic=0)

-- Set cell properties
cell.isDynamic = true
cell.flags = 0x01 | 0x04  -- Set flags 1 and 4

-- Add tile layers
cell:add_layer(10)
cell:add_layer(20)
cell:add_layer(30)
print("Layers after adding:", cell.layer_count)  --> 3

-- Access and modify layers
print("First layer:", cell:get_layer(0))  --> 10
cell:set_layer(0, 99)
print("Updated first layer:", cell:get_layer(0))  --> 99

-- Flag management
print("Has flag 1:", cell:has_flag(0x01))  --> true
print("Has flag 2:", cell:has_flag(0x02))  --> false

cell:set_flag(0x02)
print("After setting flag 2:", cell:has_flag(0x02))  --> true

cell:clear_flag(0x01)
print("After clearing flag 1:", cell:has_flag(0x01))  --> false

-- Layer operations
print("Layer 1:", cell:get_layer(1))  --> 20
cell:remove_layer(1)
print("After removing layer 1, count:", cell.layer_count)  --> 2

-- Verify remaining layers
print("Layer 0:", cell:get_layer(0))  --> 99
print("Layer 1:", cell:get_layer(1))  --> 30

-- Clear all layers
cell:clear_layers()
print("Layers after clear:", cell.layer_count)  --> 0

-- Final state
print("Final cell state:")
print("isDynamic:", cell.isDynamic)  --> true
print("flags:", cell.flags)  --> 6 (flags 2 and 4)
print("layer_count:", cell.layer_count)  --> 0

-- Create multiple cells for a map
local cells = {}
for i = 1, 5 do
    cells[i] = MapCell.new()
    cells[i].flags = i  -- Set different flags for each cell
    cells[i]:add_layer(i * 10)  -- Add different tile layers
end

-- Process cells
for i, cell in ipairs(cells) do
    print("Cell", i, ":", cell)
    print("  Flags:", cell.flags)
    print("  Layers:", cell.layer_count)
    if cell.layer_count > 0 then
        print("  First layer tile:", cell:get_layer(0))
    end
end
```
