# Rect Lua API

The `Rect` API provides Lua bindings for working with **2D rectangles**.  
A rectangle is defined by its **position** `(x, y)`, **dimensions** `(width, height)`, and an optional **rotation** (in degrees).

---

## Overview

In Lua, rectangles are represented as **proxy tables** with the metatable `RectProxyMeta`.  
They behave like objects with properties and methods accessible via dot notation.

```lua
-- Create a new rectangle
local r = Rect.new(10, 20, 100, 50)

-- Access properties
print(r.x, r.y, r.width, r.height, r.rotation)

-- Modify properties
r.x = 15
r.y = 25
r.width = 120
r.height = 60
r.rotation = 45  -- degrees
```

---

## Global `Rect` Table

### `Rect.new([x, y, width, height])`
Creates a new rectangle.  
- With no arguments: creates a rectangle at `(0,0)` with size `(0,0)`.  
- With 4 arguments: creates a rectangle with the given position and size.  

**Returns:** `Rect` object  

```lua
local r1 = Rect.new()                  -- default rect
local r2 = Rect.new(10, 20, 100, 50)   -- positioned rect
```

---

### `Rect.zero()`
Creates a rectangle at `(0,0)` with size `(0,0)` and no rotation.  

**Returns:** `Rect` object  

```lua
local r = Rect.zero()
```

---

## Rect Object Properties

Each `Rect` object has the following **read/write** properties:

- `x` → x-coordinate of top-left corner (number)  
- `y` → y-coordinate of top-left corner (number)  
- `width` → rectangle width (number)  
- `height` → rectangle height (number)  
- `rotation` → rotation in **degrees** (number)  

### Example

```lua
local r = Rect.new(0, 0, 50, 50)
r.rotation = 30
print("Rotation:", r.rotation)  --> 30
```

---

## Rect Object Methods

### `rect:contains_point(x, y)`
Checks if a point lies inside the rectangle.  
- `x, y` → point coordinates  

**Returns:** `true` if point is inside, `false` otherwise.  

```lua
if r:contains_point(15, 25) then
    print("Point is inside rectangle")
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

## Metamethods

- `tostring(rect)` → returns a string representation:  
  `"Rect: (x=..., y=..., w=..., h=..., rot=...deg)"`  

- Garbage collection (`__gc`) → if Lua owns the rect, memory is freed automatically.  

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
