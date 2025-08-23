# Entity Components Lua API

Entities are composed of **components** that define their behavior and properties.  
Components are Lua-accessible objects with their own proxy tables and metatables.

The following component types are currently supported:

- **Sprite Component** (`EntityComponentSprite`) → handles rendering animated sprites  
- **Collider Component** (`EntityComponentCollider`) → handles collision detection  
- **Lua Component** (`EntityComponentLua`) → attaches Lua scripts to entities  
- **Map Component** (`EntityComponentMap`) → handles tile-based map rendering

---

## Overview

Components are managed through the `entity.components` proxy table.  
You can add, remove, and query components dynamically.

**Important Notes:**
- **Component ownership** - components created via `ComponentType.new()` are owned by Lua
- **Entity attachment** - when added to an entity, ownership transfers to the entity
- **Automatic lifecycle** - the engine automatically manages component updates and rendering
- **Type safety** - each component type has its own metatable and validation
- **Memory management** - Lua-owned components are garbage collected, entity-owned components are managed by the engine

```lua
local e = Entity.new()

-- Add a sprite component
local sprite = EntityComponentSprite.new("player.png")
e.components:add(sprite)

-- Add a collider component
local collider = EntityComponentCollider.new()
collider.rects:add(Rect.new(0, 0, 32, 32))
e.components:add(collider)

-- Add a Lua script component
local script = EntityComponentLua.new("scripts/player.lua")
e.components:add(script)
```

---

## Components Proxy API

The `components` property of an entity is a **proxy table** with array-like access and methods.

### Array Access
```lua
local first = e.components[1]
print(first.id)
```

### Properties
- `count` → number of components (read-only)  

### Methods
- `add(component)` → adds a component to the entity  
- `remove(component)` → removes a component from the entity  
- `insert(component, index)` → inserts a component at a specific index  
- `pop()` → removes and returns the last component  
- `shift()` → removes and returns the first component  
- `find(typeName)` → returns a table of indexes of components of the given type  
- `get(id)` → returns a component by its UUID string  

**Notes:**
- **1-based indexing** - Lua-style indexing, not 0-based
- **Component ownership** - components added to entities are managed by the entity
- **Type-based search** - `find()` searches by component type name
- **ID-based retrieval** - `get()` finds components by their UUID string
- **Array-like operations** - supports common array operations like insert, pop, shift

---

## Base Component Properties

All component types inherit from the base `EseEntityComponent` structure and share these common properties:

- `id` → unique UUID string (read-only)  
- `active` → boolean (read/write, controls whether component is processed)  
- `type` → component type classification (read-only)  

**Notes:**
- **ID is immutable** - cannot be changed once assigned
- **Active state** - inactive components are not processed by the engine
- **Type classification** - determines how the component is processed (rendering, collision, scripting, etc.)

---

## Sprite Component (`EntityComponentSprite`)

### `EntityComponentSprite.new([spriteName])`
Creates a new sprite component for rendering animated sprites.  

**Arguments:**
- `spriteName` → optional string, the name of the sprite asset to load

**Returns:** `EntityComponentSprite` object

**Notes:**
- **Optional sprite name** - if not provided, component starts without a sprite
- **Asset loading** - sprite assets must be loaded via the asset manager before use
- **Lua-owned** - the returned component is owned by Lua until added to an entity
- **Automatic rendering** - when active, the component automatically renders the sprite

**Properties:**
- `id` → UUID string (read-only)  
- `active` → boolean (read/write, controls rendering)  
- `sprite` → string (sprite asset name, read/write)  

**Example:**
```lua
local sprite = EntityComponentSprite.new("enemy.png")
sprite.active = true
print("Sprite component ID:", sprite.id)

-- Change sprite at runtime
sprite.sprite = "enemy_hurt.png"

-- Create without initial sprite
local empty_sprite = EntityComponentSprite.new()
empty_sprite.sprite = "player.png"  -- Set later
```

---

## Collider Component (`EntityComponentCollider`)

### `EntityComponentCollider.new()`
Creates a new collider component for collision detection.  

**Returns:** `EntityComponentCollider` object

**Notes:**
- **Multiple collision shapes** - supports multiple rectangles for complex collision detection
- **Dynamic resizing** - collision rectangles can be added/removed at runtime
- **Automatic collision detection** - the engine automatically detects collisions between entities
- **Performance optimized** - uses efficient collision detection algorithms

**Properties:**
- `id` → UUID string (read-only)  
- `active` → boolean (read/write, controls collision detection)  
- `rects` → a **proxy collection** of `Rect` objects (read-only reference)  

### Rects Proxy API
The `rects` property is a proxy table with array-like access and methods:

- `count` → number of rects (read-only)  
- `add(rect)` → adds a rect to the collider  
- `remove(rect)` → removes a rect from the collider  
- `insert(rect, index)` → inserts a rect at a specific index  
- `pop()` → removes and returns the last rect  
- `shift()` → removes and returns the first rect  

**Notes:**
- **1-based indexing** - Lua-style indexing for rectangle access
- **Rect ownership** - rectangles added to colliders are managed by the collider
- **Dynamic collection** - the collection automatically resizes as rectangles are added/removed
- **Collision detection** - all rectangles in the collection participate in collision detection

**Example:**
```lua
local collider = EntityComponentCollider.new()
collider.active = true

-- Add collision rectangles
collider.rects:add(Rect.new(0, 0, 32, 32))      -- Main body
collider.rects:add(Rect.new(10, 10, 16, 16))    -- Hitbox

print("Collider rect count:", collider.rects.count)  --> 2

-- Access rectangles by index
local main_rect = collider.rects[1]
local hitbox_rect = collider.rects[2]

-- Remove a rectangle
local removed_rect = collider.rects:remove(hitbox_rect)
print("Removed rect:", removed_rect.x, removed_rect.y)

-- Insert at specific position
collider.rects:insert(hitbox_rect, 1)  -- Insert at beginning
print("After insert, count:", collider.rects.count)  --> 2
```

---

## Lua Component (`EntityComponentLua`)

### `EntityComponentLua.new([scriptPath])`
Creates a new Lua script component for attaching Lua scripts to entities.  

**Arguments:**
- `scriptPath` → optional string, path to the Lua script file to execute

**Returns:** `EntityComponentLua` object

**Notes:**
- **Optional script path** - if not provided, component starts without a script
- **Script loading** - scripts are loaded from the asset manager's script collection
- **Automatic execution** - the engine automatically calls script functions when appropriate
- **Instance management** - each component maintains its own script instance
- **Error handling** - script errors are logged but don't crash the engine

**Properties:**
- `id` → UUID string (read-only)  
- `active` → boolean (read/write, controls script execution)  
- `script` → string (path to script file, read/write)  

**Behavior:**
- **On first update** - the script is loaded and `entity_init()` is called if defined
- **On every update** - `entity_update(delta_time)` is called if defined
- **Custom dispatch** - you can also dispatch custom functions via `entity:dispatch("funcName", ...)`
- **Script reloading** - changing the script property reloads the script and reinitializes

**Example:**
```lua
local script = EntityComponentLua.new("scripts/player.lua")
script.active = true

-- Change script at runtime
script.script = "scripts/player_advanced.lua"  -- Reloads and reinitializes

-- Create without initial script
local empty_script = EntityComponentLua.new()
empty_script.script = "scripts/enemy.lua"  -- Set later
```

---

## Map Component (`EntityComponentMap`)

### `EntityComponentMap.new([mapAsset])`
Creates a new map component for rendering tile-based maps.  

**Arguments:**
- `mapAsset` → optional string, the name of the map asset to render

**Returns:** `EntityComponentMap` object

**Notes:**
- **Optional map asset** - if not provided, component starts without a map
- **Tile-based rendering** - renders maps using the engine's tile rendering system
- **Position centering** - the component centers the map on a specific map cell position
- **Dynamic map switching** - can change maps at runtime by modifying the map property

**Properties:**
- `id` → UUID string (read-only)  
- `active` → boolean (read/write, controls rendering)  
- `map` → string (map asset name, read/write)  
- `map_pos` → `Point` object (map cell position to center on, read/write)  
- `size` → integer (tile size in pixels, read/write)  
- `seed` → integer (random seed for procedural generation, read/write)  

**Example:**
```lua
local map = EntityComponentMap.new("maps/level1.tmx")
map.active = true
map.size = 32  -- 32x32 pixel tiles
map.map_pos.x = 10  -- Center on map cell (10, 5)
map.map_pos.y = 5

-- Change map at runtime
map.map = "maps/level2.tmx"

-- Adjust rendering position
map.map_pos.x = 15
map.map_pos.y = 8
```

---

## Component Lifecycle

- **Creation** - components created via `ComponentType.new()` are owned by Lua
- **Entity attachment** - when added to an entity via `entity.components:add()`, ownership transfers to the entity
- **Processing** - active components are processed by the engine each frame
- **Removal** - removing a component (`remove`, `pop`, `shift`) transfers ownership back to Lua
- **Cleanup** - Lua-owned components are garbage collected, entity-owned components are managed by the engine

**Notes:**
- **Ownership transfer** - adding to entity transfers ownership, removing returns ownership to Lua
- **Automatic cleanup** - the engine automatically manages entity-owned component lifecycle
- **Garbage collection** - Lua-owned components are freed when Lua garbage collection occurs
- **Memory safety** - prevents double-freeing and memory leaks

---

## Component Type Constants

The following component type constants are available for type checking and searching:

- `ENTITY_COMPONENT_COLLIDER` → Collider component type
- `ENTITY_COMPONENT_LUA` → Lua script component type  
- `ENTITY_COMPONENT_MAP` → Map rendering component type
- `ENTITY_COMPONENT_SPRITE` → Sprite rendering component type

**Example:**
```lua
-- Find all sprite components
local sprite_indices = e.components:find("EntityComponentSprite")
for _, idx in ipairs(sprite_indices) do
    local sprite = e.components[idx]
    print("Sprite component at index:", idx, "with sprite:", sprite.sprite)
end

-- Check component type
local comp = e.components[1]
if comp.type == "EntityComponentSprite" then
    print("This is a sprite component")
end
```

---

## Complete Example

```lua
-- Create a player entity
local player = Entity.new()
player.position.x = 400
player.position.y = 300

-- Add sprite component for visual representation
local sprite = EntityComponentSprite.new("player_sprite.png")
sprite.active = true
player.components:add(sprite)

-- Add collider component for collision detection
local collider = EntityComponentCollider.new()
collider.active = true
collider.rects:add(Rect.new(-16, -16, 32, 32))  -- 32x32 collision box centered on entity
player.components:add(collider)

-- Add Lua script component for behavior
local script = EntityComponentLua.new("scripts/player.lua")
script.active = true
player.components:add(script)

-- Create an enemy entity
local enemy = Entity.new()
enemy.position.x = 200
enemy.position.y = 200

-- Add enemy components
local enemy_sprite = EntityComponentSprite.new("enemy_sprite.png")
enemy.components:add(enemy_sprite)

local enemy_collider = EntityComponentCollider.new()
enemy_collider.rects:add(Rect.new(-12, -12, 24, 24))  -- Smaller collision box
enemy.components:add(enemy_collider)

local enemy_script = EntityComponentLua.new("scripts/enemy.lua")
enemy.components:add(enemy_script)

-- Component management
print("=== Component Summary ===")
print("Player components:", player.components.count)
print("Enemy components:", enemy.components.count)

-- Find specific component types
local player_sprites = player.components:find("EntityComponentSprite")
local player_colliders = player.components:find("EntityComponentCollider")
local player_scripts = player.components:find("EntityComponentLua")

print("Player sprite components:", #player_sprites)
print("Player collider components:", #player_colliders)
print("Player script components:", #player_scripts)

-- Component operations
local first_comp = player.components:shift()  -- Remove first component
print("Removed first component, new count:", player.components.count)

player.components:insert(first_comp, 1)       -- Insert back at beginning
print("Reinserted component, new count:", player.components.count)

-- Verify final state
print("Final player components:", player.components.count)
print("Final enemy components:", enemy.components.count)

-- Component property access
for i = 1, player.components.count do
    local comp = player.components[i]
    print("Component", i, "ID:", comp.id, "Active:", comp.active, "Type:", comp.type)
end
```
