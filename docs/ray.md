# Ray Lua API

The `Ray` API provides Lua bindings for working with **2D rays**.  
A ray is defined by an **origin point** `(x, y)` and a **direction vector** `(dx, dy)`.

---

## Overview

In Lua, rays are represented as **proxy tables** with the metatable `RayProxyMeta`.  
They behave like objects with properties and methods accessible via dot notation.

⚠️ **Important Notes:**
- **Origin** `(x, y)` represents the starting point of the ray
- **Direction** `(dx, dy)` represents the ray's direction vector (not necessarily normalized)
- Rays extend infinitely in the direction specified
- All properties are **mutable** and can be modified directly

```lua
-- Create a new ray
local r = Ray.new(0, 0, 1, 1)

-- Access properties
print("Origin:", r.x, r.y)           --> 0, 0
print("Direction:", r.dx, r.dy)      --> 1, 1

-- Modify properties
r.x = 5                              -- Move origin
r.y = 10
r.dx = 0                             -- Change direction to vertical
r.dy = 1

-- Normalize direction
r:normalize()
print("Normalized direction:", r.dx, r.dy)  --> 0, 1
```

---

## Global `Ray` Table

### `Ray.new([x, y, dx, dy])`
Creates a new ray.  

**Arguments:**
- **With no arguments:** creates a ray at `(0,0)` pointing right `(1,0)` (positive X direction)
- **With 4 arguments:** creates a ray with the specified parameters:
  - `x` → origin x-coordinate (number)  
  - `y` → origin y-coordinate (number)  
  - `dx` → direction x-component (number)  
  - `dy` → direction y-component (number)  

**Returns:** `Ray` object

**Notes:**
- Both arguments are **required** when creating a non-default ray
- Direction components can be any numeric value (including negative, fractional, etc.)
- The ray is created with Lua ownership (will be garbage collected)

**Example:**
```lua
local r1 = Ray.new()                    -- Default: origin (0,0), direction (1,0)
local r2 = Ray.new(0, 0, 1, 1)          -- Diagonal ray from origin
local r3 = Ray.new(10, 20, 0, -1)       -- Vertical ray pointing down
local r4 = Ray.new(-5, 5, -1, 0)        -- Horizontal ray pointing left
```

---

### `Ray.zero()`
Creates a default ray at `(0,0)` pointing right `(1,0)` (positive X direction).  

**Returns:** `Ray` object

**Notes:**
- This is equivalent to `Ray.new()` with no arguments
- Useful for initializing rays that will be set later
- The ray is created with Lua ownership

**Example:**
```lua
local default = Ray.zero()
print("Origin:", default.x, default.y)           --> 0, 0
print("Direction:", default.dx, default.dy)      --> 1, 0

-- Common pattern: create default ray, then set parameters
local r = Ray.zero()
r.x = 100
r.y = 200
r.dx = 0
r.dy = 1
```

---

## Ray Object Properties

Each `Ray` object has the following **read/write** properties:

- `x` → origin x-coordinate (number)  
- `y` → origin y-coordinate (number)  
- `dx` → direction x-component (number)  
- `dy` → direction y-component (number)  

**Notes:**
- All properties are **mutable** - you can assign new values directly
- **Origin** `(x, y)` represents the starting point of the ray
- **Direction** `(dx, dy)` represents the ray's direction vector
- No validation is performed on assigned values
- Changes take effect immediately

**Example:**
```lua
local r = Ray.new(5, 5, 2, 3)
print("Origin:", r.x, r.y)           --> 5, 5
print("Direction:", r.dx, r.dy)      --> 2, 3

-- Move the ray origin
r.x = 10
r.y = 15
print("New origin:", r.x, r.y)       --> 10, 15

-- Change direction to vertical (pointing up)
r.dx = 0
r.dy = 1
print("New direction:", r.dx, r.dy)  --> 0, 1

-- Set to horizontal direction (pointing right)
r.dx = 1
r.dy = 0
print("Horizontal direction:", r.dx, r.dy)  --> 1, 0

-- Use mathematical operations
r.dx = math.cos(math.pi / 4)  -- 45 degrees
r.dy = math.sin(math.pi / 4)
print("45° direction:", r.dx, r.dy)  --> ~0.707, ~0.707
```

---

## Ray Object Methods

### `ray:intersects_rect(rect)`
Checks if the ray intersects with a rectangle.  

**Arguments:**
- `rect` → a `Rect` object  

**Returns:** `true` if ray intersects with the rectangle, `false` otherwise

**Notes:**
- Checks intersection between the infinite ray and the rectangle edges
- Takes into account the rectangle's rotation if applicable
- Returns `true` if the ray passes through any part of the rectangle

**Example:**
```lua
local r = Ray.new(0, 0, 1, 0)      -- Horizontal ray from origin
local rect = Rect.new(5, -2, 10, 4)  -- Rectangle at (5,-2), size 10×4

if r:intersects_rect(rect) then
    print("Ray intersects rectangle")  --> true (ray goes through rect)
end

-- Diagonal ray
local r2 = Ray.new(0, 0, 1, 1)
print("Diagonal ray intersects:", r2:intersects_rect(rect))  --> true
```

---

### `ray:get_point_at_distance(distance)`
Gets a point along the ray at a given distance from the origin.  

**Arguments:**
- `distance` → distance along the ray from origin (number)  

**Returns:** `x, y` coordinates (numbers) representing the point on the ray

**Formula:** Returns `(origin_x + direction_x × distance, origin_y + direction_y × distance)`

**Notes:**
- The ray extends infinitely, so any positive or negative distance is valid
- Negative distance goes in the opposite direction of the ray
- No normalization is performed - uses the current direction vector as-is

**Example:**
```lua
local r = Ray.new(0, 0, 3, 4)  -- Ray from origin with direction (3,4)

-- Get point 5 units along the ray
local px, py = r:get_point_at_distance(5)
print("Point at distance 5:", px, py)  --> 15, 20

-- Get point 2 units in opposite direction
local px2, py2 = r:get_point_at_distance(-2)
print("Point at distance -2:", px2, py2)  --> -6, -8

-- Get point at unit distance
local px3, py3 = r:get_point_at_distance(1)
print("Point at distance 1:", px3, py3)  --> 3, 4
```

---

### `ray:normalize()`
Normalizes the ray’s direction vector to unit length.  

**Returns:** nothing (modifies the ray in-place)

**Notes:**
- **Modifies the ray in-place** (changes dx and dy components)
- If the direction vector is already zero, the result is undefined
- The resulting direction points in the same direction but has unit length
- Useful for consistent movement speeds regardless of original direction magnitude  

```lua
local r = Ray.new(0, 0, 6, 8)  -- Direction magnitude = 10
print("Before normalize:", r.dx, r.dy)  --> 6, 8

r:normalize()
print("After normalize:", r.dx, r.dy)   --> 0.6, 0.8

-- Verify magnitude is now 1
local magnitude = math.sqrt(r.dx * r.dx + r.dy * r.dy)
print("Magnitude:", magnitude)  --> 1.0

-- Normalizing a unit vector doesn't change it
local r2 = Ray.new(0, 0, 1, 0)
r2:normalize()
print("Unit vector unchanged:", r2.dx, r2.dy)  --> 1, 0
```

---

## Metamethods

- `tostring(ray)` → returns a string representation:  
  `"Ray: (x=..., y=..., dx=..., dy=...)"`  

- Garbage collection (`__gc`) → if Lua owns the ray, memory is freed automatically.

**Notes:**
- The `tostring` metamethod provides a human-readable representation for debugging
- The `__gc` metamethod ensures proper cleanup when Lua garbage collection occurs
- Memory ownership is determined by the `__is_lua_owned` flag in the proxy table  

---

## Complete Example

```lua
-- Create a ray from origin pointing diagonally
local r = Ray.new(0, 0, 3, 4)
print(r)  --> Ray: (x=0.00, y=0.00, dx=3.00, dy=4.00)

-- Check initial direction magnitude
local magnitude = math.sqrt(r.dx * r.dx + r.dy * r.dy)
print("Initial magnitude:", magnitude)  --> 5.0

-- Normalize direction to unit length
r:normalize()
print("Normalized direction:", r.dx, r.dy)  --> 0.6, 0.8

-- Verify normalization worked
local new_magnitude = math.sqrt(r.dx * r.dx + r.dy * r.dy)
print("New magnitude:", new_magnitude)  --> 1.0

-- Get points at various distances
local px1, py1 = r:get_point_at_distance(1)
print("Point at distance 1:", px1, py1)  --> 0.6, 0.8

local px5, py5 = r:get_point_at_distance(5)
print("Point at distance 5:", px5, py5)  --> 3.0, 4.0

-- Move ray origin and check intersection
r.x = 10
r.y = 20
print("Moved ray:", r)  --> Ray: (x=10.00, y=20.00, dx=0.60, dy=0.80)

local rect = Rect.new(8, 18, 6, 6)
if r:intersects_rect(rect) then
    print("Ray intersects rectangle at (8,18) to (14,24)")
end

-- Create a horizontal ray for line-of-sight
local horizontal_ray = Ray.new(0, 0, 1, 0)
print("Horizontal ray:", horizontal_ray)  --> Ray: (x=0.00, y=0.00, dx=1.00, dy=0.00)

-- Check if it intersects a vertical wall
local wall = Rect.new(5, -10, 2, 20)
if horizontal_ray:intersects_rect(wall) then
    print("Horizontal ray hits wall at x=5")
end
```
