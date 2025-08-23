# Entity Lua API

The `Entity` API provides Lua bindings for working with **game entities**.  
An entity is a container for **components** (rendering, scripts, colliders, etc.), has a **position**, and can store **custom data**.

---

## Overview

In Lua, entities are represented as **proxy tables** with the metatable `EntityProxyMeta`.  
They behave like objects with properties and methods accessible via dot notation.

**Important Notes:**
- **C-owned by default** - entities created via `Entity.new()` are owned by the engine, not Lua
- **Automatic engine registration** - new entities are automatically added to the engine's entity list
- **Component management** - the `components` property provides a proxy table with array-like access and methods
- **Custom data storage** - the `data` property is a Lua table for storing arbitrary script data
- **Position is a Point object** - you cannot assign a new Point, but you can modify the existing one's x and y values
- **Memory safety** - the engine automatically manages entity lifecycle and prevents access to freed entities

```lua
-- Create a new entity
local e = Entity.new()

-- Access properties
print(e.id, e.active, e.draw_order)

-- Modify properties
e.active = true
e.draw_order = 5
e.position.x = 100
e.position.y = 200

-- Work with components
print("Component count:", e.components.count)
```

---

## Global Entity Table

The `Entity` table is a global table created by the engine during initialization. It provides a constructor for creating new entity objects.

---

## Global Entity Table Methods

### `Entity.new()`
Creates a new entity and automatically registers it with the engine.  

**Returns:** `Entity` object  

**Notes:**
- **Automatically registered** - the new entity is immediately added to the engine's entity list
- **C-owned** - the entity is owned by the engine, not by Lua
- **Default values** - entity starts at position (0,0), active=true, draw_order=0
- **Unique ID** - each entity gets a unique UUID generated automatically
- **Position object** - a new `Point` object is created and assigned to the entity

**Example:**
```lua
local e = Entity.new()
print("Entity created with ID:", e.id)  --> "550e8400-e29b-41d4-a716-446655440000"
print("Default position:", e.position.x, e.position.y)  --> 0, 0
print("Default active:", e.active)  --> true
print("Default draw order:", e.draw_order)  --> 0
```

---

## Entity Object Properties

Each `Entity` object has the following properties:

- `id` → unique UUID string (read-only)  
- `active` → boolean (read/write, controls whether entity is processed)  
- `draw_order` → integer (read/write, controls rendering order - higher values render later)  
- `position` → a `Point` object (read-only reference, but its fields are mutable)  
- `components` → a **components proxy table** (read-only reference)  
- `data` → a Lua table for storing arbitrary script data (read/write)  

**Notes:**
- **ID is immutable** - cannot be changed once assigned
- **Active state** - inactive entities are not processed by the engine
- **Draw order** - controls rendering order (0 = background, higher = foreground)
- **Position reference** - you cannot assign a new Point, but you can modify the existing one's x and y values
- **Components proxy** - provides array-like access and management methods
- **Data table** - persistent Lua table for storing custom properties and state
- **Type validation** - active must be boolean, draw_order must be integer

**Example:**
```lua
local e = Entity.new()
print("Entity ID:", e.id)          -- UUID string like "550e8400-e29b-41d4-a716-446655440000"

-- Modify entity state
e.active = true                    -- Enable entity processing
e.draw_order = 10                  -- Set render order (higher = later)

-- Modify position (cannot assign new Point)
e.position.x = 50                  -- Set X coordinate
e.position.y = 75                  -- Set Y coordinate

-- Store custom data
e.data.health = 100                -- Custom health property
e.data.name = "Player"             -- Custom name property
e.data.inventory = {}              -- Custom table property
e.data.inventory.gold = 50         -- Nested data
e.data.inventory.items = {"sword", "shield"}

-- Access custom data
print("Health:", e.data.health)    --> 100
print("Name:", e.data.name)        --> "Player"
print("Gold:", e.data.inventory.gold)  --> 50
```

---

## Components Proxy

The `components` property is a **proxy table** with its own metatable `ComponentsProxyMeta`. It provides array-like access and management methods for entity components.

### Array Access
You can access components by index (1-based):

```lua
local comp = e.components[1]       -- Get first component
local comp2 = e.components[2]      -- Get second component
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
- `get(id)` → returns a component by its ID string  

**Notes:**
- **1-based indexing** - Lua-style indexing, not 0-based
- **Component ownership** - components added to entities are managed by the entity
- **Type-based search** - `find()` searches by component type name
- **ID-based retrieval** - `get()` finds components by their UUID string
- **Array-like operations** - supports common array operations like insert, pop, shift

**Example:**
```lua
-- Add components
local sprite = SpriteComponent.new("player.png")
e.components:add(sprite)

local collider = ColliderComponent.new(50, 50)
e.components:add(collider)

local script = EntityComponentLua.new("player_script.lua")
e.components:add(script)

print("Component count:", e.components.count)  --> 3

-- Access by index
local first_comp = e.components[1]            -- First component (sprite)
local second_comp = e.components[2]           -- Second component (collider)

-- Find components by type
local sprite_indices = e.components:find("Sprite")
print("Sprite components at indices:", table.unpack(sprite_indices))

-- Get component by ID
local comp = e.components:get(sprite.id)
if comp then
    print("Found component by ID:", comp.id)
end

-- Remove component
e.components:remove(collider)
print("After removal, count:", e.components.count)  --> 2

-- Insert at specific position
e.components:insert(collider, 1)   -- Insert at beginning
print("After insert, count:", e.components.count)   --> 3

-- Array operations
local last_comp = e.components:pop()           -- Remove and return last
local first_comp = e.components:shift()        -- Remove and return first
print("After pop/shift, count:", e.components.count)  --> 1
```

---

## Entity Methods

### `entity:dispatch(funcName, ...)`
Calls a function on all **Lua components** attached to the entity.  

**Arguments:**
- `funcName` → string (function name to call)  
- `...` → variable arguments passed to the function  

**Returns:** `true` if dispatched successfully, `false` if no function was found or executed

**Notes:**
- **Lua components only** - only affects components of type `EntityComponentLua`
- **Function search** - looks for the named function in each Lua component's script
- **Argument passing** - all arguments after funcName are passed to the target function
- **Return value** - returns true if any function was executed, false otherwise
- **Error handling** - if a component's function errors, other components are still processed

**Example:**
```lua
-- Dispatch update function to all Lua components
local success = e:dispatch("on_update", 0.016)  -- 16ms delta time
if success then
    print("Update dispatched successfully")
else
    print("No update function found in Lua components")
end

-- Dispatch custom function with multiple arguments
e:dispatch("on_damage", 25, "fire", true)  -- damage amount, damage type, critical hit
```

---

## Automatic Lua Component Callbacks

When an entity has a **Lua component** (`EntityComponentLua`), the engine automatically calls the following functions if they are defined in the script:

- `entity_init()` → called **once** when the component is first created/initialized  
- `entity_update(delta_time)` → called **every frame** with the frame's delta time  

This allows Lua scripts to define per-entity behavior without needing to be manually dispatched.

### Example Lua Component Script

```lua
-- Called once when the entity is created
function entity_init()
    print("Entity initialized with ID:", self.id)
    self.data.health = 100
    self.data.max_health = 100
    self.data.speed = 50
    self.data.name = "Player"
end

-- Called every frame
function entity_update(dt)
    -- Move entity to the right at speed units per second
    self.position.x = self.position.x + self.data.speed * dt
    
    -- Keep entity within screen bounds
    if self.position.x > 800 then
        self.position.x = 800
    elseif self.position.x < 0 then
        self.position.x = 0
    end
    
    -- Health regeneration
    if self.data.health < self.data.max_health then
        self.data.health = math.min(self.data.max_health, self.data.health + 10 * dt)
    end
end
```

---

## Collision Events

Entities automatically receive collision callbacks if they have **collider components**.  
The following functions are dispatched to Lua components if defined:

- `entity_collision_enter(other)` → called when collision starts  
- `entity_collision_stay(other)` → called while collision continues  
- `entity_collision_exit(other)` → called when collision ends  

**Notes:**
- **Automatic detection** - collision events are automatically detected by the engine
- **Other entity reference** - the `other` parameter is the entity being collided with
- **Component requirement** - entity must have a collider component to receive collision events
- **Function dispatch** - these functions are automatically dispatched to all Lua components

### Example

```lua
function entity_collision_enter(other)
    print("Collided with entity:", other.id)
    print("Other entity position:", other.position.x, other.position.y)
    
    -- Check if other entity is an enemy
    if other.data and other.data.type == "enemy" then
        -- Take damage
        self.data.health = self.data.health - 10
        print("Took damage! Health:", self.data.health)
    end
end

function entity_collision_stay(other)
    -- Called every frame while colliding
    if other.data and other.data.type == "damage_zone" then
        -- Continuous damage
        self.data.health = self.data.health - 5 * 0.016  -- 5 damage per second
    end
end

function entity_collision_exit(other)
    print("Stopped colliding with:", other.id)
end
```

---

## Restrictions

- **ID is read-only** - cannot be modified:

```lua
-- ❌ Invalid - will cause Lua error
e.id = "new-uuid"
-- Error: "Entity id is a read-only property"
```

- **Position cannot be reassigned** - you cannot assign a new Point object:

```lua
-- ❌ Invalid - will cause Lua error
e.position = Point.new(100, 200)
-- Error: "Entity position must be a EsePoint object"

-- ✅ Correct - modify existing Point's values
e.position.x = 100
e.position.y = 200
```

- **Components cannot be reassigned** - the components property is read-only:

```lua
-- ❌ Invalid - will cause Lua error
e.components = {}
-- Error: "Entity components is not assignable"
```

- **Data must be a table** - cannot assign non-table values to data:

```lua
-- ❌ Invalid - will cause Lua error
e.data = "string value"
-- Error: "Entity data must be a table"

-- ✅ Correct - assign a table
e.data = {health = 100, name = "Player"}
```

---

## Entity Object Methods

**Note:** The Entity API currently only provides the `dispatch()` method beyond property access.

---

## Metamethods

- `tostring(entity)` → returns a string representation:  
  `"Entity: 0x... (id=..., active=..., components=N)"`  

- Garbage collection (`__gc`) → if Lua owns the entity (rare), memory is freed automatically.

**Notes:**
- The `tostring` metamethod provides a human-readable representation for debugging
- The `__gc` metamethod ensures proper cleanup when Lua garbage collection occurs
- Memory ownership is determined by the `__is_lua_owned` flag in the proxy table
- Entities are typically C-owned by the engine, so `__gc` usually doesn't free memory
- The string format includes memory address, ID, active state, and component count

---

## Complete Example

```lua
-- Create a player entity
local player = Entity.new()
print("Created player:", player)  --> Entity: 0x... (id=..., active=true, components=0)

-- Set up player properties
player.active = true
player.draw_order = 10
player.position.x = 400
player.position.y = 300

-- Store player data
player.data.health = 100
player.data.max_health = 100
player.data.speed = 50
player.data.name = "Player"
player.data.inventory = {gold = 0, items = {}}

-- Add components
local sprite = SpriteComponent.new("player_sprite.png")
player.components:add(sprite)

local collider = ColliderComponent.new(32, 32)
player.components:add(collider)

local script = EntityComponentLua.new("player_script.lua")
player.components:add(script)

print("Player component count:", player.components.count)  --> 3

-- Verify components
print("Sprite component:", player.components[1].id)
print("Collider component:", player.components[2].id)
print("Script component:", player.components[3].id)

-- Find components by type
local sprite_indices = player.components:find("Sprite")
print("Sprite components at:", table.unpack(sprite_indices))

-- Dispatch custom function
player:dispatch("on_spawn")

-- Create enemy entity
local enemy = Entity.new()
enemy.position.x = 200
enemy.position.y = 200
enemy.data.type = "enemy"
enemy.data.damage = 20

-- Add enemy components
local enemy_sprite = SpriteComponent.new("enemy_sprite.png")
enemy.components:add(enemy_sprite)

local enemy_collider = ColliderComponent.new(24, 24)
enemy.components:add(enemy_collider)

local enemy_script = EntityComponentLua.new("enemy_script.lua")
enemy.components:add(enemy_script)

-- Entity management
print("=== Entity Summary ===")
print("Player ID:", player.id)
print("Player position:", player.position.x, player.position.y)
print("Player health:", player.data.health)
print("Player components:", player.components.count)

print("Enemy ID:", enemy.id)
print("Enemy position:", enemy.position.x, enemy.position.y)
print("Enemy type:", enemy.data.type)
print("Enemy components:", enemy.components.count)

-- Component operations
local first_comp = player.components:shift()  -- Remove first component
print("Removed first component, new count:", player.components.count)

player.components:insert(first_comp, 1)       -- Insert back at beginning
print("Reinserted component, new count:", player.components.count)

-- Verify final state
print("Final player components:", player.components.count)
print("Final enemy components:", enemy.components.count)
```
