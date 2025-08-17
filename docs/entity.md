# Entity Lua API

The `Entity` API provides Lua bindings for working with **game entities**.  
An entity is a container for **components** (rendering, scripts, colliders, etc.), has a **position**, and can store **custom data**.

---

## Overview

In Lua, entities are represented as **proxy tables** with the metatable `EntityProxyMeta`.  
They behave like objects with properties and methods accessible via dot notation.

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

## Global `Entity` Table

### `Entity.new()`
Creates a new entity.  

**Returns:** `Entity` object  

```lua
local e = Entity.new()
```

---

## Entity Object Properties

Each `Entity` object has the following properties:

- `id` → unique UUID string (read-only)  
- `active` → boolean (read/write)  
- `draw_order` → integer (read/write, controls rendering order)  
- `position` → a `Point` object (read/write via `x` and `y`)  
- `components` → a **components proxy table** (see below)  
- `data` → a Lua table for storing arbitrary script data (read/write)  

### Example

```lua
local e = Entity.new()
print(e.id)          -- UUID string
e.active = true
e.draw_order = 10
e.position.x = 50
e.position.y = 75

-- Store custom data
e.data.health = 100
e.data.name = "Player"
```

---

## Components Proxy

The `components` property is a **proxy table** with its own methods and array-like access.  

### Array Access
You can access components by index:

```lua
local comp = e.components[1]
```

### Properties
- `count` → number of components  

### Methods
- `add(component)` → adds a component to the entity  
- `remove(component)` → removes a component from the entity  
- `insert(component, index)` → inserts a component at a specific index  
- `pop()` → removes and returns the last component  
- `shift()` → removes and returns the first component  
- `find(typeName)` → returns a table of indexes of components of the given type  
- `get(id)` → returns a component by its ID string  

### Example

```lua
-- Add a component
local sprite = SpriteComponent.new("player.png")
e.components:add(sprite)

-- Remove a component
e.components:remove(sprite)

-- Insert at index
e.components:insert(sprite, 1)

-- Pop last component
local last = e.components:pop()

-- Shift first component
local first = e.components:shift()

-- Find all colliders
local colliders = e.components:find("Collider")

-- Get by ID
local comp = e.components:get("some-uuid")
```

---

## Entity Methods

### `entity:dispatch(funcName, ...)`
Calls a function on all **Lua components** attached to the entity.  
- `funcName` → string (function name)  
- `...` → arguments passed to the function  

**Returns:** `true` if dispatched, `false` otherwise  

```lua
e:dispatch("on_update", dt)
```

---

## Automatic Lua Component Callbacks

When an entity has a **Lua component** (`EntityComponentLua`), the engine automatically calls the following functions if they are defined in the script:

- `entity_init()` → called **once** when the component is first created/initialized  
- `entity_update(delta_time)` → called **every frame** with the frame’s delta time  

This allows Lua scripts to define per-entity behavior without needing to be manually dispatched.

### Example Lua Component Script

```lua
-- Called once when the entity is created
function entity_init()
    print("Entity initialized with ID:", self.id)
    self.data.health = 100
end

-- Called every frame
function entity_update(dt)
    -- Move entity to the right at 50 units per second
    self.position.x = self.position.x + 50 * dt
end
```

---

## Collision Events

Entities automatically receive collision callbacks if they have **collider components**.  
The following functions are dispatched to Lua components if defined:

- `entity_collision_enter(other)` → called when collision starts  
- `entity_collision_stay(other)` → called while collision continues  
- `entity_collision_exit(other)` → called when collision ends  

### Example

```lua
function entity_collision_enter(other)
    print("Collided with:", other.id)
end
```

---

## Metamethods

- `tostring(entity)` → returns a string representation:  
  `"Entity: (id=..., active=..., components=N)"`  

- Garbage collection (`__gc`) → if Lua owns the entity (rare), memory is freed automatically.  

---

## Example Usage

```lua
-- Create an entity
local e = Entity.new()
print(e)

-- Set position
e.position.x = 100
e.position.y = 200

-- Add a component
local collider = ColliderComponent.new(50, 50)
e.components:add(collider)

-- Dispatch a function to Lua components
e:dispatch("on_update", 0.016)

-- Store custom data
e.data.health = 100
e.data.name = "Enemy"
```
