# Arc Lua API

The `Arc` API provides Lua bindings for working with geometric arcs.  
An **arc** is defined by a center point `(x, y)`, a radius, and a start/end angle in radians.

---

## Overview

In Lua, arcs are represented as **proxy tables** with the metatable `ArcProxyMeta`.  
They behave like objects with properties and methods accessible via dot notation.

```lua
-- Create a new arc
local a = Arc.new(0, 0, 10, 0, math.pi / 2)

-- Access properties
print(a.x, a.y, a.radius)

-- Modify properties
a.radius = 20
a.start_angle = 0
a.end_angle = math.pi

-- Call methods
print("Length:", a:get_length())
print("Contains point:", a:contains_point(10, 0))
```

---

## Global `Arc` Table

### `Arc.new([x, y, radius, start_angle, end_angle])`
Creates a new arc.  
- With no arguments: creates a unit circle arc at `(0,0)` spanning `0 → 2π`.  
- With 5 arguments: creates an arc with the given parameters.

**Returns:** `Arc` object.

```lua
local a1 = Arc.new()                  -- default arc
local a2 = Arc.new(5, 5, 10, 0, math.pi) -- half-circle arc
```

---

### `Arc.zero()`
Creates a default arc at `(0,0)` with radius `1` spanning `0 → 2π`.

**Returns:** `Arc` object.

```lua
local a = Arc.zero()
```

---

## Arc Object Properties

Each `Arc` object has the following **read/write** properties:

- `x` → center x-coordinate (number)  
- `y` → center y-coordinate (number)  
- `radius` → arc radius (number)  
- `start_angle` → start angle in radians (number)  
- `end_angle` → end angle in radians (number)  

Example:

```lua
local a = Arc.new()
a.x = 10
a.y = 20
a.radius = 5
a.start_angle = math.pi / 4
a.end_angle = math.pi / 2
```

---

## Arc Object Methods

### `arc:contains_point(x, y [, tolerance])`
Checks if a point lies on the arc within a tolerance.  
- `x, y` → point coordinates  
- `tolerance` (optional, default `0.1`) → distance tolerance  

**Returns:** `true` if point lies on arc, `false` otherwise.

```lua
if a:contains_point(10, 0) then
    print("Point is on arc")
end
```

---

### `arc:get_length()`
Gets the length of the arc.

**Returns:** arc length (number).

```lua
print("Arc length:", a:get_length())
```

---

### `arc:get_point_at_angle(angle)`
Gets the coordinates of a point on the arc at a given angle.  
- `angle` → angle in radians  

**Returns:**  
- `success` (boolean) → `true` if angle is within arc range  
- `x, y` (numbers) → coordinates of the point  

```lua
local ok, px, py = a:get_point_at_angle(math.pi / 4)
if ok then
    print("Point:", px, py)
end
```

---

### `arc:intersects_rect(rect)`
Checks if the arc intersects with a rectangle.  
- `rect` → an `Rect` object  

**Returns:** `true` if arc intersects rectangle, `false` otherwise.

```lua
if a:intersects_rect(r) then
    print("Arc intersects rectangle")
end
```

---

## Metamethods

- `tostring(arc)` → returns a string representation:  
  `"Arc: (x=..., y=..., radius=..., start=..., end=...)"`  

- Garbage collection (`__gc`) → if Lua owns the arc, memory is freed automatically.  

---

## Example Usage

```lua
-- Create an arc
local a = Arc.new(0, 0, 5, 0, math.pi)

-- Print arc info
print(a)

-- Check arc length
print("Length:", a:get_length())

-- Check if point lies on arc
print("Contains (5,0):", a:contains_point(5, 0))

-- Get a point at 45 degrees
local ok, px, py = a:get_point_at_angle(math.pi / 4)
if ok then
    print("Point at 45°:", px, py)
end
```
