# Rect Lua API

The `Rect` API provides Lua bindings for working with **2D rectangles**.  
A rectangle is defined by its **position** `(x, y)`, **dimensions** `(width, height)`, and an optional **rotation**.

---

## Overview

In Lua, rectangles are represented as **proxy tables** that behave like regular Lua objects.  
They behave like objects with properties and methods accessible via dot notation.

⚠️ **Important Notes:**
- Position `(x, y)` represents the **top-left corner** of the rectangle
- **Rotation** is specified in **degrees** (not radians) and rotates around the rectangle's **center**
- Internally stored as radians but exposed to Lua as degrees for convenience

```lua
-- Create a new rectangle
local r = Rect.new(10, 20, 100, 50)

-- Access properties  
print("Position:", r.x, r.y)           --> 10, 20
print("Size:", r.width, r.height)      --> 100, 50
print("Rotation:", r.rotation)         --> 0 (degrees)

-- Modify properties
r.x = 15
r.y = 25  
r.width = 120
r.height = 60
r.rotation = 45  -- Rotate 45 degrees around center
```

---

## Global `Rect` Table

### `Rect.new(x, y, width, height)`
Creates a new rectangle with the given position and size.  

**Arguments:**
- `x` → x-coordinate of top-left corner (number)  
- `y` → y-coordinate of top-left corner (number)  
- `width` → rectangle width (number)  
- `height` → rectangle height (number)  

**Returns:** `Rect` object

**Notes:**
- All four arguments are **required**
- For a zero-size rect at the origin, prefer `Rect.zero()`

**Example:**
```lua
local r1 = Rect.new(0, 0, 0, 0)          -- Explicit zero-size rect at origin
local r2 = Rect.new(10, 20, 100, 50)     -- 100×50 rect at (10,20)
local r3 = Rect.new(-5, -10, 25, 30)     -- Rect with negative coordinates
```

---

### `Rect.zero()`
Creates a rectangle at `(0,0)` with size `(0,0)` and no rotation.  
This is equivalent to `Rect.new(0, 0, 0, 0)`.

**Returns:** `Rect` object

**Example:**
```lua
local empty = Rect.zero()
print(empty.x, empty.y)           --> 0, 0
print(empty.width, empty.height)  --> 0, 0
print(empty.rotation)             --> 0
```

---

### `Rect.fromJSON(json_string)`
Creates a `Rect` from a JSON string previously produced by `rect:toJSON()`.  

**Arguments:**
- `json_string` → string returned from `rect:toJSON()`  

**Returns:** `Rect` object

**Notes:**
- Validates that the JSON represents a rectangle; invalid JSON raises a Lua error

**Example:**
```lua
local json = some_rect:toJSON()
local copy = Rect.fromJSON(json)
```

---

## Rect Object Properties

Each `Rect` object has the following **read/write** properties:

- `x` → x-coordinate of top-left corner (number)  
- `y` → y-coordinate of top-left corner (number)  
- `width` → rectangle width (number, should be non-negative)  
- `height` → rectangle height (number, should be non-negative)  
- `rotation` → rotation in **degrees** around the center (number)  

**Notes:**
- Rotation is in **degrees** (converted from internal radians representation)
- Rotation occurs around the rectangle's center point: `(x + width/2, y + height/2)`
- Negative width/height values may cause unexpected behavior in collision detection
- All properties accept floating-point values

**Example:**
```lua
local r = Rect.new(10, 20, 100, 50)
r.rotation = 30              -- Rotate 30 degrees clockwise
print("Rotation:", r.rotation)    --> 30.0

-- Calculate center point
local center_x = r.x + r.width / 2   --> 60
local center_y = r.y + r.height / 2  --> 45
print("Center:", center_x, center_y)

-- Modify size
r.width = 200
r.height = 75
print("New area:", r:area())  --> 15000
```

---

## Rect Object Methods

### `rect:contains_point(x, y)` / `rect:contains_point(point)`
Checks if a point lies inside the rectangle.  

**Arguments (two forms):**
- `rect:contains_point(x, y)` → `x` and `y` are numeric coordinates  
- `rect:contains_point(point)` → `point` is a `Point` object  

**Returns:** `true` if point is inside, `false` otherwise.  

```lua
if r:contains_point(15, 25) then
    print("Point is inside rectangle")
end

local p = Point.new(15, 25)
if r:contains_point(p) then
    print("Point object is inside rectangle")
end
```

---

### `rect:intersects(other)`
Checks if two rectangles intersect.  
- `other` → another `Rect` object  

**Returns:** `true` if rectangles intersect, `false` otherwise.  

```lua
local r1 = Rect.new(0, 0, 50, 50)
local r2 = Rect.new(25, 25, 50, 50)
print(r1:intersects(r2))  --> true
```

---

### `rect:area()`
Gets the area of the rectangle.  

**Returns:** area (number)  

```lua
print("Area:", r:area())
```

---

### `rect:toJSON()`
Serializes the rectangle into a compact JSON string including position, size, and rotation.  

**Returns:** JSON string that can be passed to `Rect.fromJSON()`

**Example:**
```lua
local r = Rect.new(10, 20, 100, 50)
local json = r:toJSON()
local copy = Rect.fromJSON(json)
```

---

## Metamethods

- `tostring(rect)` → returns a string representation:  
  `"(x=..., y=..., w=..., h=..., rot=...deg)"`  
  
---
---

## Example Usage

```lua
-- Create a rectangle
local r = Rect.new(10, 20, 100, 50)
print(r)  --> Rect: (x=10.00, y=20.00, w=100.00, h=50.00, rot=0.00deg)

-- Rotate it
r.rotation = 45
print("Rotation:", r.rotation)

-- Check if point is inside
print("Contains (20,30):", r:contains_point(20, 30))

-- Check intersection
local r2 = Rect.new(50, 50, 100, 100)
print("Intersects:", r:intersects(r2))

-- Area
print("Area:", r:area())
```
