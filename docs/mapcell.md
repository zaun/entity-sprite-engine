# MapCell Lua API

The `MapCell` API provides Lua bindings for working with **map grid cells**.  
A map cell can contain multiple **tile layers**, **flags**, and a **dynamic state**.

---

## Overview

In Lua, map cells are represented as **proxy tables** with the metatable `MapCellProxyMeta`.  
They behave like objects with properties and methods accessible via dot notation.

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

## Global `MapCell` Table

### `MapCell.new()`
Creates a new empty map cell.  

**Returns:** `MapCell` object  

```lua
local cell = MapCell.new()
```

---

## MapCell Object Properties

Each `MapCell` object has the following **read/write** properties:

- `isDynamic` → boolean  
  - If `false` (default), the map renderer draws all layers.  
  - If `true`, the cell is ignored by the renderer (for dynamic/interactive cells).  

- `flags` → number (bitfield)  
  - Arbitrary flags for game logic.  

- `layer_count` → number (read-only)  
  - The number of tile layers currently in the cell.  

### Example

```lua
local cell = MapCell.new()
cell.isDynamic = true
cell.flags = 0x04
print(cell.layer_count)  --> 0
```

---

## MapCell Object Methods

### `cell:add_layer(tile_id)`
Adds a new tile layer to the cell.  
- `tile_id` → number (tile identifier)  

**Returns:** `true` if successful, `false` if allocation failed.  

```lua
cell:add_layer(5)
cell:add_layer(7)
print(cell.layer_count)  --> 2
```

---

### `cell:remove_layer(index)`
Removes a tile layer by index (0-based).  
- `index` → number  

**Returns:** `true` if successful, `false` if index is out of bounds.  

```lua
cell:remove_layer(0)
```

---

### `cell:get_layer(index)`
Gets the tile ID at a given layer index.  
- `index` → number  

**Returns:** tile ID (number), or `0` if index is out of bounds.  

```lua
local id = cell:get_layer(0)
print("Tile ID:", id)
```

---

### `cell:set_layer(index, tile_id)`
Sets the tile ID at a given layer index.  
- `index` → number  
- `tile_id` → number  

**Returns:** `true` if successful, `false` if index is out of bounds.  

```lua
cell:set_layer(0, 42)
```

---

### `cell:clear_layers()`
Removes all tile layers from the cell.  

```lua
cell:clear_layers()
print(cell.layer_count)  --> 0
```

---

### `cell:has_flag(flag)`
Checks if a specific flag is set.  
- `flag` → number (bitmask)  

**Returns:** `true` if flag is set, `false` otherwise.  

```lua
if cell:has_flag(0x01) then
    print("Flag 0x01 is set")
end
```

---

### `cell:set_flag(flag)`
Sets a specific flag.  
- `flag` → number (bitmask)  

```lua
cell:set_flag(0x02)
```

---

### `cell:clear_flag(flag)`
Clears a specific flag.  
- `flag` → number (bitmask)  

```lua
cell:clear_flag(0x02)
```

---

## Metamethods

- `tostring(cell)` → returns a string representation:  
  `"MapCell: (layers=..., flags=..., dynamic=...)"`  

- Garbage collection (`__gc`) → if Lua owns the cell, memory is freed automatically.  

---

## Example Usage

```lua
-- Create a new map cell
local cell = MapCell.new()
print(cell)  --> MapCell: (layers=0, flags=0, dynamic=0)

-- Add layers
cell:add_layer(10)
cell:add_layer(20)
print("Layers:", cell.layer_count)  --> 2
print("First layer:", cell:get_layer(0))

-- Modify a layer
cell:set_layer(0, 99)
print("Updated first layer:", cell:get_layer(0))

-- Flags
cell:set_flag(0x01)
print("Has flag 0x01:", cell:has_flag(0x01))
cell:clear_flag(0x01)

-- Dynamic property
cell.isDynamic = true
print(cell)  --> MapCell: (layers=2, flags=0, dynamic=1)

-- Clear layers
cell:clear_layers()
print("Layers after clear:", cell.layer_count)  --> 0
```
