# Point Lua API

The `Point` API provides Lua bindings for working with **2D points**.  
A point is defined by its **x** and **y** floating-point coordinates.

---

## Overview

In Lua, points are represented as **proxy tables** that behave like regular Lua objects.  
They behave like objects with properties accessible via dot notation.

⚠️ **Important Notes:**
- Coordinates are stored as **floating-point numbers**
- Points are **mutable** - you can modify x and y values directly
- No validation is performed on coordinate values (can be negative, NaN, or infinite)

```lua
-- Create a new point
local p = Point.new(10, 20)

-- Access properties
print("Position:", p.x, p.y)  --> 10, 20

-- Modify properties
p.x = 15
p.y = 25
print("New position:", p.x, p.y)  --> 15, 25

-- Negative coordinates are valid
p.x = -5.5
p.y = 100.75
```

---

## Global `Point` Table

### `Point.new(x, y)`
Creates a new point with the given coordinates.  

**Arguments:**
- `x` → x-coordinate (number)  
- `y` → y-coordinate (number)  

**Returns:** `Point` object

**Notes:**
- Both arguments are **required**
- Coordinates can be any numeric value (including negative, fractional, etc.)
- The point is created with Lua ownership (will be garbage collected)

**Example:**
```lua
local p1 = Point.new(5, 7)
print(p1.x, p1.y)  --> 5, 7

local p2 = Point.new(-10.5, 3.14159)
print(p2.x, p2.y)  --> -10.5, 3.14159

local p3 = Point.new(0, 0)  -- Same as Point.zero()
```

---

### `Point.zero()`
Creates a new point at the origin `(0, 0)`.  

**Returns:** `Point` object

**Notes:**
- This is equivalent to `Point.new(0, 0)` but more semantically clear
- Useful for initializing points that will be set later
- The point is created with Lua ownership

**Example:**
```lua
local origin = Point.zero()
print(origin.x, origin.y)  --> 0, 0

-- Common pattern: create zero point, then set coordinates
local p = Point.zero()
p.x = 100
p.y = 200
```

---

### `Point.distance(p1, p2)`
Computes the Euclidean distance between two `Point` instances.  

**Arguments:**
- `p1` → first `Point`  
- `p2` → second `Point`  

**Returns:** distance between `p1` and `p2` (number)

**Example:**
```lua
local a = Point.new(0, 0)
local b = Point.new(3, 4)
print(Point.distance(a, b))  --> 5
```

---

### `Point.fromJSON(json_string)`
Creates a `Point` from a JSON string previously produced by `point:toJSON()`.  

**Arguments:**
- `json_string` → string returned from `point:toJSON()`  

**Returns:** `Point` object

**Notes:**
- Validates that the JSON represents a point; invalid JSON raises a Lua error

**Example:**
```lua
local json = some_point:toJSON()
local copy = Point.fromJSON(json)
```

---

## Point Object Properties

Each `Point` object has the following **read/write** properties:

- `x` → x-coordinate (number)  
- `y` → y-coordinate (number)  

**Notes:**
- Both properties are **mutable** - you can assign new values directly
- No validation is performed on assigned values
- Changes take effect immediately
- Properties accept any numeric value (including negative, fractional, etc.)

**Example:**
```lua
local p = Point.new(1, 2)
print("Initial:", p.x, p.y)  --> 1, 2

-- Modify coordinates
p.x = p.x + 5
p.y = p.y + 10
print("Modified:", p.x, p.y)  --> 6, 12

-- Direct assignment
p.x = 100.5
p.y = -50.25
print("New values:", p.x, p.y)  --> 100.5, -50.25

-- Mathematical operations
p.x = math.cos(math.pi / 4)  -- cos(45°)
p.y = math.sin(math.pi / 4)  -- sin(45°)
print("Trig values:", p.x, p.y)  --> ~0.707, ~0.707
```

---

## Point Object Methods

### `point:toJSON()`
Serializes the point into a compact JSON string.  

**Returns:** JSON string that can be passed to `Point.fromJSON()`

**Example:**
```lua
local p = Point.new(10, 20)
local json = p:toJSON()
local copy = Point.fromJSON(json)
```

---

## Metamethods

### `tostring(point)`
Returns a string representation of the point with its coordinates.

**Format:** `"(x=..., y=...)"`

**Example:**
```lua
local p = Point.new(10, 20)
print(p)  --> (x=10.000, y=20.000)
```

---

## Complete Example

```lua
-- Create points for different purposes
local player_pos = Point.new(100, 200)
local target_pos = Point.new(300, 400)
local velocity = Point.new(5, -2)

-- Print initial state
print("Player at:", player_pos)
print("Target at:", target_pos)
print("Velocity:", velocity)

-- Calculate distance to target
local dx = target_pos.x - player_pos.x
local dy = target_pos.y - player_pos.y
local distance = math.sqrt(dx * dx + dy * dy)
print("Distance to target:", distance)

-- Move player towards target
player_pos.x = player_pos.x + velocity.x
player_pos.y = player_pos.y + velocity.y
print("Player moved to:", player_pos)

-- Create a grid of points
local grid = {}
for i = 0, 2 do
    for j = 0, 2 do
        local p = Point.new(i * 50, j * 50)
        table.insert(grid, p)
        print("Grid point", i, j, ":", p)
    end
end

-- Use points in mathematical calculations
local center = Point.new(0, 0)
local radius = 100
local angle = math.pi / 6  -- 30 degrees

local point_on_circle = Point.new(
    center.x + radius * math.cos(angle),
    center.y + radius * math.sin(angle)
)
print("Point on circle:", point_on_circle)

-- Reset a point to origin
player_pos.x = 0
player_pos.y = 0
print("Player reset to:", player_pos)
```
