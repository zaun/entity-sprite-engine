# Point Lua API

The `Point` API provides Lua bindings for working with **2D points**.  
A point is defined by its **x** and **y** floating-point coordinates.

---

## Overview

In Lua, points are represented as **proxy tables** with the metatable `PointProxyMeta`.  
They behave like objects with properties accessible via dot notation.

```lua
-- Create a new point
local p = Point.new(10, 20)

-- Access properties
print(p.x, p.y)  --> 10, 20

-- Modify properties
p.x = 15
p.y = 25
```

---

## Global `Point` Table

### `Point.new(x, y)`
Creates a new point with the given coordinates.  

**Arguments:**
- `x` → number (x-coordinate)  
- `y` → number (y-coordinate)  

**Returns:** `Point` object  

```lua
local p = Point.new(5, 7)
print(p.x, p.y)  --> 5, 7
```

---

### `Point.zero()`
Creates a new point at the origin `(0, 0)`.  

**Returns:** `Point` object  

```lua
local p = Point.zero()
print(p.x, p.y)  --> 0, 0
```

---

## Point Object Properties

Each `Point` object has the following **read/write** properties:

- `x` → x-coordinate (number)  
- `y` → y-coordinate (number)  

### Example

```lua
local p = Point.new(1, 2)
p.x = p.x + 5
p.y = p.y + 10
print(p.x, p.y)  --> 6, 12
```

---

## Metamethods

- `tostring(point)` → returns a string representation:  
  `"Point: (x=..., y=...)"`  

- Garbage collection (`__gc`) → if Lua owns the point, memory is freed automatically.  

---

## Example Usage

```lua
-- Create a point
local p1 = Point.new(3, 4)
print(p1)  --> Point: (x=3.00, y=4.00)

-- Create a zero point
local p2 = Point.zero()
print(p2)  --> Point: (x=0.00, y=0.00)

-- Modify coordinates
p1.x = 10
p1.y = 20
print(p1)  --> Point: (x=10.00, y=20.00)
```
