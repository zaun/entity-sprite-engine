# Ray Lua API

The `Ray` API provides Lua bindings for working with **2D rays**.  
A ray is defined by an **origin point** `(x, y)` and a **direction vector** `(dx, dy)`.

---

## Overview

In Lua, rays are represented as **proxy tables** with the metatable `RayProxyMeta`.  
They behave like objects with properties and methods accessible via dot notation.

```lua
-- Create a new ray
local r = Ray.new(0, 0, 1, 1)

-- Access properties
print(r.x, r.y, r.dx, r.dy)

-- Modify properties
r.x = 5
r.y = 10
r.dx = 0
r.dy = 1

-- Normalize direction
r:normalize()
```

---

## Global `Ray` Table

### `Ray.new([x, y, dx, dy])`
Creates a new ray.  
- With no arguments: creates a ray at `(0,0)` pointing right `(1,0)`.  
- With 4 arguments: creates a ray with the given origin and direction.  

**Returns:** `Ray` object  

```lua
local r1 = Ray.new()              -- default ray
local r2 = Ray.new(0, 0, 1, 1)    -- diagonal ray
```

---

### `Ray.zero()`
Creates a default ray at `(0,0)` pointing right `(1,0)`.  

**Returns:** `Ray` object  

```lua
local r = Ray.zero()
```

---

## Ray Object Properties

Each `Ray` object has the following **read/write** properties:

- `x` → origin x-coordinate (number)  
- `y` → origin y-coordinate (number)  
- `dx` → direction x-component (number)  
- `dy` → direction y-component (number)  

### Example

```lua
local r = Ray.new(5, 5, 2, 3)
print(r.x, r.y, r.dx, r.dy)

r.dx = 0
r.dy = 1
```

---

## Ray Object Methods

### `ray:intersects_rect(rect)`
Checks if the ray intersects with a rectangle.  
- `rect` → an `Rect` object  

**Returns:** `true` if ray intersects rectangle, `false` otherwise.  

```lua
if r:intersects_rect(myRect) then
    print("Ray hits rectangle")
end
```

---

### `ray:get_point_at_distance(distance)`
Gets a point along the ray at a given distance from the origin.  
- `distance` → number (distance along the ray)  

**Returns:** `x, y` coordinates (numbers)  

```lua
local px, py = r:get_point_at_distance(10)
print("Point at distance 10:", px, py)
```

---

### `ray:normalize()`
Normalizes the ray’s direction vector to unit length.  

**Returns:** nothing  

```lua
r:normalize()
print("Normalized direction:", r.dx, r.dy)
```

---

## Metamethods

- `tostring(ray)` → returns a string representation:  
  `"Ray: (x=..., y=..., dx=..., dy=...)"`  

- Garbage collection (`__gc`) → if Lua owns the ray, memory is freed automatically.  

---

## Example Usage

```lua
-- Create a ray
local r = Ray.new(0, 0, 3, 4)
print(r)  --> Ray: (x=0.00, y=0.00, dx=3.00, dy=4.00)

-- Normalize direction
r:normalize()
print("Normalized:", r.dx, r.dy)

-- Get a point 5 units along the ray
local px, py = r:get_point_at_distance(5)
print("Point at distance 5:", px, py)

-- Check intersection with a rectangle
if r:intersects_rect(myRect) then
    print("Ray intersects rectangle")
end
```
