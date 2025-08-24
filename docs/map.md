# Map Lua API

The **Map** system provides a 2D grid-based structure for organizing and managing tile-based game worlds with support for different coordinate systems.

---

## Overview

A map represents a 2D grid of cells, where each cell can contain tile data, entities, or other game objects. Maps support multiple coordinate systems and can be associated with tilesets for visual rendering.

**Key Features:**
- **2D grid structure** - rectangular array of map cells
- **Multiple coordinate systems** - grid, hexagonal, and isometric support
- **Tileset integration** - can be associated with tilesets for rendering
- **Dynamic resizing** - can be resized at runtime
- **Cell management** - individual cells can be accessed and modified
- **Lua integration** - fully accessible from Lua with automatic memory management

---

## Map Types

The engine supports several coordinate system types:

- **`"grid"`** → Standard square/rectangular tiles (default)
- **`"hex_point_up"`** → Hexagonal tiles with point facing up
- **`"hex_flat_up"`** → Hexagonal tiles with flat side facing up  
- **`"iso"`** → Isometric tiles

---

## Creation

### `Map.new(width, height, [type])`
Creates a new map with specified dimensions and coordinate type.

**Arguments:**
- `width` → number, map width in cells (must be > 0)
- `height` → number, map height in cells (must be > 0)
- `type` → string (optional), coordinate system type (defaults to "grid")

**Returns:** `Map` object

**Notes:**
- **Dimension validation** - width and height must be greater than 0
- **Default type** - if type is not specified, defaults to "grid"
- **Cell initialization** - all cells are created and initialized automatically
- **Lua-owned** - the returned map is owned by Lua until transferred

**Example:**
```lua
-- Create a standard grid map
local grid_map = Map.new(32, 24)

-- Create a hexagonal map
local hex_map = Map.new(20, 16, "hex_point_up")

-- Create an isometric map
local iso_map = Map.new(40, 30, "iso")
```

---

## Properties

### `map.title`
The map's title or name.

**Type:** string (read/write)

**Notes:**
- **Default value** - starts as "Untitled Map"
- **String copying** - assigned strings are copied internally
- **Memory management** - old strings are automatically freed

**Example:**
```lua
map.title = "Grassland Level 1"
print("Map title:", map.title)
```

### `map.author`
The map's author or creator.

**Type:** string (read/write)

**Notes:**
- **Default value** - starts as "Unknown"
- **String copying** - assigned strings are copied internally
- **Memory management** - old strings are automatically freed

**Example:**
```lua
map.author = "Level Designer"
print("Map author:", map.author)
```

### `map.version`
The map's version number.

**Type:** number (read/write)

**Notes:**
- **Default value** - starts as 0
- **Integer storage** - stored as a 32-bit integer

**Example:**
```lua
map.version = 2
print("Map version:", map.version)
```

### `map.type`
The map's coordinate system type.

**Type:** string (read/write)

**Notes:**
- **Read-only** - cannot be changed after creation
- **String representation** - returns the type as a string

**Example:**
```lua
print("Map type:", map.type)  -- "grid", "hex_point_up", etc.
```

### `map.width`
The map's width in cells.

**Type:** number (read-only)

**Notes:**
- **Immutable** - cannot be changed after creation
- **Cell count** - represents the number of cells in the X direction

**Example:**
```lua
print("Map width:", map.width, "cells")
```

### `map.height`
The map's height in cells.

**Type:** number (read-only)

**Notes:**
- **Immutable** - cannot be changed after creation
- **Cell count** - represents the number of cells in the Y direction

**Example:**
```lua
print("Map height:", map.height, "cells")
```

### `map.tileset`
The tileset associated with this map.

**Type:** `Tileset` object or `nil` (read-only)

**Notes:**
- **Association** - can be set via `set_tileset()` method
- **Nil if unset** - returns `nil` if no tileset is associated
- **Reference** - returns the actual tileset object, not a copy

**Example:**
```lua
if map.tileset then
    print("Map has tileset with", map.tileset:get_sprite_count(1), "grass variants")
else
    print("Map has no tileset")
end
```

---

## Cell Management

### `map:get_cell(x, y)`
Gets a map cell at the specified coordinates.

**Arguments:**
- `x` → number, X coordinate (0-based)
- `y` → number, Y coordinate (0-based)

**Returns:** `MapCell` object, or `nil` if coordinates are out of bounds

**Notes:**
- **Bounds checking** - returns `nil` for invalid coordinates
- **0-based indexing** - coordinates start at (0, 0)
- **Cell reference** - returns the actual cell object, not a copy

**Example:**
```lua
local cell = map:get_cell(5, 3)
if cell then
    print("Cell at (5,3):", cell)
    print("Cell tile ID:", cell.tile_id)
else
    print("Invalid coordinates (5,3)")
end
```

### `map:set_cell(x, y, cell)`
Sets a map cell at the specified coordinates.

**Arguments:**
- `x` → number, X coordinate (0-based)
- `y` → number, Y coordinate (0-based)
- `cell` → `MapCell` object, the cell to set

**Returns:** `true` if successful, `false` if coordinates are out of bounds

**Notes:**
- **Bounds checking** - fails if coordinates are invalid
- **Cell copying** - the cell is copied internally
- **Ownership transfer** - the map takes ownership of the new cell
- **Old cell cleanup** - the previous cell at that position is destroyed

**Example:**
```lua
-- Create a new cell
local new_cell = MapCell.new()
new_cell.tile_id = 5
new_cell.elevation = 2

-- Set it in the map
if map:set_cell(10, 15, new_cell) then
    print("Cell set successfully")
else
    print("Failed to set cell - invalid coordinates")
end
```

---

## Map Operations

### `map:resize(new_width, new_height)`
Resizes the map to new dimensions.

**Arguments:**
- `new_width` → number, new width in cells (must be > 0)
- `new_height` → number, new height in cells (must be > 0)

**Returns:** `true` if successful, `false` if memory allocation fails

**Notes:**
- **Dimension validation** - new dimensions must be greater than 0
- **Cell preservation** - existing cells within new bounds are preserved
- **Cell destruction** - cells outside new bounds are destroyed
- **New cell creation** - empty positions are filled with new cells

**Example:**
```lua
-- Expand the map
if map:resize(64, 48) then
    print("Map resized to", map.width, "x", map.height)
else
    print("Failed to resize map")
end
```

### `map:set_tileset(tileset)`
Associates a tileset with this map.

**Arguments:**
- `tileset` → `Tileset` object, the tileset to associate

**Notes:**
- **Association** - the map now references the tileset
- **No copying** - the tileset object is referenced, not copied
- **Rendering** - the tileset will be used for tile rendering
- **Null allowed** - passing `nil` removes the tileset association

**Example:**
```lua
-- Create and set up a tileset
local tileset = Tiles.new()
tileset:add_sprite(1, "grass.png", 10)
tileset:add_sprite(2, "stone.png", 8)

-- Associate it with the map
map:set_tileset(tileset)
print("Map now has tileset:", map.tileset)
```

---

## Complete Example

```lua
-- Create a grassland map
local grassland_map = Map.new(40, 30, "grid")
grassland_map.title = "Grassland Plains"
grassland_map.author = "World Generator"
grassland_map.version = 1

print("Created map:", grassland_map.title)
print("Dimensions:", grassland_map.width, "x", grassland_map.height)
print("Type:", grassland_map.type)

-- Create a tileset for the grassland
local grassland_tileset = Tiles.new()
grassland_tileset:add_sprite(1, "grass_light.png", 15)
grassland_tileset:add_sprite(1, "grass_medium.png", 12)
grassland_tileset:add_sprite(1, "grass_dark.png", 8)
grassland_tileset:add_sprite(2, "dirt.png", 10)
grassland_tileset:add_sprite(3, "water.png", 5)

-- Associate tileset with map
grassland_map:set_tileset(grassland_tileset)
print("Map tileset:", grassland_map.tileset)

-- Set some cells with different tile types
for x = 0, 39 do
    for y = 0, 29 do
        local cell = MapCell.new()
        
        -- Create a simple pattern
        if x < 10 or x > 29 or y < 5 or y > 24 then
            cell.tile_id = 2  -- Dirt border
        elseif x >= 15 and x <= 24 and y >= 10 and y <= 19 then
            cell.tile_id = 3  -- Water in center
        else
            cell.tile_id = 1  -- Grass everywhere else
        end
        
        -- Set elevation based on position
        cell.elevation = math.floor((x + y) / 10)
        
        -- Set the cell in the map
        grassland_map:set_cell(x, y, cell)
    end
end

print("Map populated with", grassland_map.width * grassland_map.height, "cells")

-- Access some specific cells
local center_cell = grassland_map:get_cell(20, 15)
if center_cell then
    print("Center cell tile ID:", center_cell.tile_id)
    print("Center cell elevation:", center_cell.elevation)
end

-- Resize the map
if grassland_map:resize(50, 40) then
    print("Map resized to", grassland_map.width, "x", grassland_map.height)
    
    -- Check that existing cells are preserved
    local preserved_cell = grassland_map:get_cell(20, 15)
    if preserved_cell then
        print("Preserved cell tile ID:", preserved_cell.tile_id)
    end
end

-- Print final map information
print("\n=== Final Map Summary ===")
print("Title:", grassland_map.title)
print("Author:", grassland_map.author)
print("Version:", grassland_map.version)
print("Dimensions:", grassland_map.width, "x", grassland_map.height)
print("Type:", grassland_map.type)
print("Has tileset:", grassland_map.tileset ~= nil)
```

---

## Integration with Entity Components

Maps can be used with the `EntityComponentMap` to render tile-based worlds:

```lua
-- Create a map component
local map_component = EntityComponentMap.new()
map_component.map = grassland_map
map_component.position.x = 20  -- Center on map cell (20, 15)
map_component.position.y = 15
map_component.size = 32       -- 32x32 pixel tiles

-- Add to an entity
local world_entity = Entity.new()
world_entity.components:add(map_component)
```

---

## Memory Management

- **Lua ownership** - maps created via `Map.new()` are owned by Lua
- **Automatic cleanup** - Lua-owned maps are garbage collected when no longer referenced
- **Cell management** - all cells are automatically created and destroyed with the map
- **String copying** - title and author strings are copied internally

**Notes:**
- **Ownership transfer** - can be transferred to other systems (like entity components) if needed
- **Memory safety** - prevents memory leaks through automatic cleanup
- **Performance** - optimized for fast cell access and modification
- **Cell lifecycle** - cells are managed automatically and cannot be manually freed
