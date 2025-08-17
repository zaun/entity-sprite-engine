# Entity Components Lua API

Entities are composed of **components** that define their behavior and properties.  
Components are Lua-accessible objects with their own proxy tables and metatables.

The following component types are currently supported:

- **Sprite Component** (`EntityComponentSprite`) → handles rendering animated sprites  
- **Collider Component** (`EntityComponentCollider`) → handles collision detection  
- **Lua Component** (`EntityComponentLua`) → attaches Lua scripts to entities  

---

## Overview

Components are managed through the `entity.components` proxy table.  
You can add, remove, and query components dynamically.

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
- `count` → number of components  

### Methods
- `add(component)` → adds a component to the entity  
- `remove(component)` → removes a component from the entity  
- `insert(component, index)` → inserts a component at a specific index  
- `pop()` → removes and returns the last component  
- `shift()` → removes and returns the first component  
- `find(typeName)` → returns a table of indexes of components of the given type  
- `get(id)` → returns a component by its UUID string  

---

## Sprite Component (`EntityComponentSprite`)

### `EntityComponentSprite.new([spriteName])`
Creates a new sprite component.  
- `spriteName` → optional string, the name of the sprite asset  

**Properties:**
- `id` → UUID string (read-only)  
- `active` → boolean (read/write)  
- `sprite` → string (sprite asset name, read/write)  

**Example:**
```lua
local sprite = EntityComponentSprite.new("enemy.png")
sprite.active = true
print(sprite.id)
```

---

## Collider Component (`EntityComponentCollider`)

### `EntityComponentCollider.new()`
Creates a new collider component.  

**Properties:**
- `id` → UUID string (read-only)  
- `active` → boolean (read/write)  
- `rects` → a **proxy collection** of `Rect` objects  

### Rects Proxy API
The `rects` property is a proxy table with array-like access and methods:

- `count` → number of rects  
- `add(rect)` → adds a rect to the collider  
- `remove(rect)` → removes a rect from the collider  
- `insert(rect, index)` → inserts a rect at a specific index  
- `pop()` → removes and returns the last rect  
- `shift()` → removes and returns the first rect  

**Example:**
```lua
local collider = EntityComponentCollider.new()
collider.rects:add(Rect.new(0, 0, 32, 32))
collider.rects:add(Rect.new(10, 10, 16, 16))

print("Collider rect count:", collider.rects.count)
```

---

## Lua Component (`EntityComponentLua`)

### `EntityComponentLua.new([scriptPath])`
Creates a new Lua script component.  
- `scriptPath` → optional string, path to the Lua script file  

**Properties:**
- `id` → UUID string (read-only)  
- `active` → boolean (read/write)  
- `script` → string (path to script file, read/write)  

**Behavior:**
- On first update, the script is loaded and `entity_init()` is called if defined.  
- On every update, `entity_update(delta_time)` is called if defined.  
- You can also dispatch custom functions via `entity:dispatch("funcName", ...)`.  

**Example:**
```lua
local script = EntityComponentLua.new("scripts/player.lua")
script.active = true
```

---

## Component Lifecycle

- Components are **owned by entities** once added.  
- Removing a component (`remove`, `pop`, `shift`) transfers ownership back to Lua.  
- Components are garbage-collected if Lua owns them.  

---

## Example Usage

```lua
local e = Entity.new()

-- Add components
local sprite = EntityComponentSprite.new("player.png")
local collider = EntityComponentCollider.new()
collider.rects:add(Rect.new(0, 0, 32, 32))
local script = EntityComponentLua.new("scripts/player.lua")

e.components:add(sprite)
e.components:add(collider)
e.components:add(script)

-- Access components
print("Component count:", e.components.count)
print("First component ID:", e.components[1].id)

-- Remove last component
local last = e.components:pop()
print("Removed component:", last.id)

-- Find all sprite components
local indexes = e.components:find("EntityComponentSprite")
for _, idx in ipairs(indexes) do
    print("Sprite component at index:", idx)
end
```
