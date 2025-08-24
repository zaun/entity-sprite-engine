# Tileset Lua API

The **Tileset** system provides a mapping from tile IDs to weighted sprite lists, enabling procedural tile generation with visual variety.

---

## Overview

A tileset maps numeric tile IDs (0-255) to lists of sprites with weights. When a tile is requested, the system randomly selects a sprite based on the weights, allowing for natural-looking variation in tile-based maps.

**Key Features:**
- **Tile ID mapping** - maps 0-255 tile IDs to sprite lists
- **Weighted selection** - sprites have weights for random selection
- **Dynamic management** - add/remove sprites and adjust weights at runtime
- **Lua integration** - fully accessible from Lua with automatic memory management

---

## Creation

### `Tiles.new()`
Creates a new empty tileset.

**Returns:** `Tileset` object

**Notes:**
- **Empty initialization** - starts with no tile mappings
- **Lua-owned** - the returned tileset is owned by Lua until transferred
- **Automatic cleanup** - Lua-owned tilesets are garbage collected when no longer referenced

**Example:**
```lua
local tileset = Tiles.new()
print("Created tileset:", tileset)
```

---

## Tile Management

### `tileset:add_sprite(tile_id, sprite_id, weight)`
Adds a sprite with weight to a tile mapping.

**Arguments:**
- `tile_id` → number (0-255), the tile ID to add the sprite to
- `sprite_id` → string, the sprite asset name to add
- `weight` → number (must be > 0), the weight for random selection

**Returns:** `true` if successful, `false` if memory allocation fails

**Notes:**
- **Sprite copying** - the sprite string is copied internally
- **Weight validation** - weight must be greater than 0
- **Multiple sprites** - the same tile ID can have multiple sprites
- **Asset loading** - sprite assets must be loaded via the asset manager before use

**Example:**
```lua
local tileset = Tiles.new()

-- Add grass variants to tile ID 1
tileset:add_sprite(1, "grass_1.png", 10)
tileset:add_sprite(1, "grass_2.png", 8)
tileset:add_sprite(1, "grass_3.png", 6)

-- Add stone variants to tile ID 2
tileset:add_sprite(2, "stone_1.png", 15)
tileset:add_sprite(2, "stone_2.png", 12)
```

### `tileset:remove_sprite(tile_id, sprite_id)`
Removes a sprite from a tile mapping.

**Arguments:**
- `tile_id` → number (0-255), the tile ID to remove the sprite from
- `sprite_id` → string, the sprite asset name to remove

**Returns:** `true` if successful, `false` if sprite not found

**Notes:**
- **Exact match** - sprite string must match exactly
- **Weight recalculation** - total weight is automatically recalculated
- **Memory cleanup** - removed sprite strings are freed

**Example:**
```lua
-- Remove a specific grass variant
tileset:remove_sprite(1, "grass_2.png")
```

### `tileset:clear_mapping(tile_id)`
Removes all sprites from a tile mapping.

**Arguments:**
- `tile_id` → number (0-255), the tile ID to clear

**Notes:**
- **Complete removal** - all sprites and weights are removed
- **Memory cleanup** - all sprite strings are freed
- **Fast operation** - efficient bulk removal

**Example:**
```lua
-- Clear all sprites from tile ID 1
tileset:clear_mapping(1)
```

---

## Sprite Retrieval

### `tileset:get_sprite(tile_id)`
Gets a random sprite based on weights for a tile.

**Arguments:**
- `tile_id` → number (0-255), the tile ID to get a sprite for

**Returns:** sprite asset name string, or `nil` if no mapping exists

**Notes:**
- **Weighted selection** - uses internal random number generator
- **Consistent results** - same seed produces same sequence
- **Null handling** - returns `nil` for unmapped tile IDs
- **Performance** - optimized for fast random selection

**Example:**
```lua
-- Get a random grass sprite
local grass_sprite = tileset:get_sprite(1)
if grass_sprite then
    print("Selected grass sprite:", grass_sprite)
else
    print("No sprites mapped to tile ID 1")
end
```

---

## Information and Queries

### `tileset:get_sprite_count(tile_id)`
Gets the number of sprites for a tile mapping.

**Arguments:**
- `tile_id` → number (0-255), the tile ID to check

**Returns:** number of sprites in the mapping

**Notes:**
- **Zero for unmapped** - returns 0 for tile IDs with no sprites
- **Fast lookup** - constant time operation

**Example:**
```lua
local count = tileset:get_sprite_count(1)
print("Tile ID 1 has", count, "sprite variants")
```

### `tileset:update_sprite_weight(tile_id, sprite_id, new_weight)`
Updates the weight of an existing sprite in a tile mapping.

**Arguments:**
- `tile_id` → number (0-255), the tile ID containing the sprite
- `sprite_id` → string, the sprite asset name to update
- `new_weight` → number (must be > 0), the new weight value

**Returns:** `true` if successful, `false` if sprite not found

**Notes:**
- **Weight validation** - new weight must be greater than 0
- **Total recalculation** - internal total weight is updated
- **Performance** - affects random selection distribution

**Example:**
```lua
-- Increase the weight of a common grass variant
tileset:update_sprite_weight(1, "grass_1.png", 15)
```

---

## Random Seed Control

### `tileset.seed`
Sets the random seed for sprite selection.

**Type:** number (read/write)

**Notes:**
- **Deterministic results** - same seed produces same sprite sequence
- **Runtime modification** - can be changed at any time
- **Default value** - starts with seed 0

**Example:**
```lua
-- Set a specific seed for reproducible results
tileset.seed = 42

-- Or use current time for variety
tileset.seed = os.time()
```

---

## Complete Example

```lua
-- Create a tileset for a grassland biome
local grassland_tileset = Tiles.new()

-- Set a seed for consistent generation
grassland_tileset.seed = 12345

-- Add grass variants (tile ID 1)
grassland_tileset:add_sprite(1, "grass_light.png", 12)
grassland_tileset:add_sprite(1, "grass_medium.png", 15)
grassland_tileset:add_sprite(1, "grass_dark.png", 8)
grassland_tileset:add_sprite(1, "grass_flowered.png", 6)

-- Add dirt variants (tile ID 2)
grassland_tileset:add_sprite(2, "dirt_light.png", 10)
grassland_tileset:add_sprite(2, "dirt_dark.png", 8)

-- Add water variants (tile ID 3)
grassland_tileset:add_sprite(3, "water_shallow.png", 20)
grassland_tileset:add_sprite(3, "water_deep.png", 15)

-- Add stone variants (tile ID 4)
grassland_tileset:add_sprite(4, "stone_small.png", 8)
grassland_tileset:add_sprite(4, "stone_large.png", 5)

-- Generate some random tiles
print("=== Random Tile Generation ===")
for i = 1, 10 do
    local grass = grassland_tileset:get_sprite(1)
    local dirt = grassland_tileset:get_sprite(2)
    local water = grassland_tileset:get_sprite(3)
    local stone = grassland_tileset:get_sprite(4)
    
    print(string.format("Row %d: %s, %s, %s, %s", 
          i, grass or "none", dirt or "none", water or "none", stone or "none"))
end

-- Check sprite counts
print("\n=== Sprite Counts ===")
print("Grass variants:", grassland_tileset:get_sprite_count(1))
print("Dirt variants:", grassland_tileset:get_sprite_count(2))
print("Water variants:", grassland_tileset:get_sprite_count(3))
print("Stone variants:", grassland_tileset:get_sprite_count(4))

-- Modify weights dynamically
print("\n=== Weight Adjustment ===")
grassland_tileset:update_sprite_weight(1, "grass_flowered.png", 12)  -- Increase flowered grass
print("Updated flowered grass weight")

-- Remove a variant
grassland_tileset:remove_sprite(4, "stone_small.png")
print("Removed small stone variant")

-- Check final counts
print("Final grass variants:", grassland_tileset:get_sprite_count(1))
print("Final stone variants:", grassland_tileset:get_sprite_count(4))

-- Clear a mapping entirely
grassland_tileset:clear_mapping(3)
print("Cleared water mapping, variants:", grassland_tileset:get_sprite_count(3))
```

---

## Integration with Maps

Tilesets are designed to work with the Map system. A tileset can be assigned to a map to provide sprite selection for tile rendering:

```lua
-- Create a map and tileset
local map = Map.new(32, 24, "grid")
local tileset = Tiles.new()

-- Set up tileset with tile mappings
tileset:add_sprite(1, "grass.png", 10)
tileset:add_sprite(2, "stone.png", 8)

-- Assign tileset to map
map:set_tileset(tileset)

-- Now the map can use the tileset for rendering
```

---

## Memory Management

- **Lua ownership** - tilesets created via `Tiles.new()` are owned by Lua
- **Automatic cleanup** - Lua-owned tilesets are garbage collected when no longer referenced
- **String copying** - sprite IDs are copied internally, so you can free the original strings
- **Efficient storage** - uses dynamic arrays that grow as needed

**Notes:**
- **Ownership transfer** - can be transferred to other systems (like maps) if needed
- **Memory safety** - prevents memory leaks through automatic cleanup
- **Performance** - optimized for fast sprite lookup and random selection
