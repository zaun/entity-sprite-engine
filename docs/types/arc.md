# Arc Lua API

The `Arc` API provides Lua bindings for working with **geometric arcs**.  
An **arc** is defined by a center point `(x, y)`, a radius, and start/end angles in **radians**.

---

## Overview

In Lua, arcs are represented as **proxy tables** that behave like regular Lua objects.  
They behave like objects with properties and methods accessible via dot notation.

⚠️ **Important:** All angles are in **radians**, not degrees.

```lua
-- Create a new arc (semicircle from 0 to π radians)
local a = Arc.new(0, 0, 10, 0, math.pi)

-- Access properties
print("Center:", a.x, a.y)
print("Radius:", a.radius)
print("Angles:", a.start_angle, a.end_angle)

-- Modify properties
a.radius = 20
a.start_angle = 0
a.end_angle = math.pi / 2  -- Quarter circle

-- Call methods
print("Arc length:", a:get_length())
print("Contains point (10,0):", a:contains_point(10, 0))
print("Point at π/4:", a:get_point_at_angle(math.pi / 4))
```

---

## Global `Arc` Table

### `Arc.new([x, y, radius, start_angle, end_angle])`
Creates a new arc.  

**Arguments:**
- **With no arguments:** creates a unit circle arc at `(0,0)` with radius `1` spanning `0 → 2π` radians (full circle)
- **With 5 arguments:** creates an arc with the specified parameters:
  - `x` → center x-coordinate (number)  
  - `y` → center y-coordinate (number)  
  - `radius` → arc radius (number, must be positive)  
  - `start_angle` → start angle in radians (number)  
  - `end_angle` → end angle in radians (number)  

**Returns:** `Arc` object

**Example:**
```lua
local a1 = Arc.new()                        -- Default: unit circle at origin
local a2 = Arc.new(5, 5, 10, 0, math.pi)     -- Semicircle at (5,5), radius 10
local a3 = Arc.new(0, 0, 5, math.pi/4, 3*math.pi/4)  -- Quarter arc
```

---

### `Arc.zero()`
Creates a default arc at `(0,0)` with radius `1` spanning `0 → 2π` radians (full circle).  
This is equivalent to `Arc.new()` with no arguments.

**Returns:** `Arc` object

**Example:**
```lua
local circle = Arc.zero()
print(circle.radius)        --> 1
print(circle.end_angle)     --> 6.283... (2π)
```

---

### `Arc.fromJSON(json_string)`
Creates a new `Arc` from a JSON string previously produced by `arc:toJSON()`.  

**Arguments:**
- `json_string` → string returned from `arc:toJSON()`  

**Returns:** `Arc` object

**Notes:**
- Validates that the JSON represents an `Arc`; invalid JSON raises a Lua error
- Uses the engine's allocator and JSON parser internally

**Example:**
```lua
local json = some_arc:toJSON()
local copy = Arc.fromJSON(json)
```

---

## Arc Object Properties

Each `Arc` object has the following **read/write** properties:

- `x` → center x-coordinate (number)  
- `y` → center y-coordinate (number)  
- `radius` → arc radius (number, should be positive)  
- `start_angle` → start angle in radians (number)  
- `end_angle` → end angle in radians (number)  

**Notes:**
- All angle values are in **radians**
- Negative radius values may cause unexpected behavior in calculations
- Angles can be any real number (including negative values and values > 2π)

**Example:**
```lua
local a = Arc.new()
a.x = 10               -- Move center to (10, 20)
a.y = 20
a.radius = 5           -- Set radius to 5 units
a.start_angle = math.pi / 4    -- Start at 45 degrees (π/4 radians)
a.end_angle = math.pi / 2      -- End at 90 degrees (π/2 radians)

print("Arc spans", math.deg(a.end_angle - a.start_angle), "degrees")
```

---

## Arc Object Methods

### `arc:contains_point(x, y [, tolerance])`
Checks if a point lies on the arc curve within a specified tolerance.  

**Arguments:**
- `x` → point x-coordinate (number)  
- `y` → point y-coordinate (number)  
- `tolerance` → distance tolerance (number, optional, default `0.1`)  

**Returns:** `true` if point lies on the arc curve, `false` otherwise

**Notes:**
- Only checks if the point is on the arc **curve**, not inside the arc area
- The point must be within the angular range `[start_angle, end_angle]`
- Uses distance tolerance to account for floating-point precision

**Example:**
```lua
local a = Arc.new(0, 0, 5, 0, math.pi)  -- Semicircle, radius 5
print(a:contains_point(5, 0))           --> true (point on arc)
print(a:contains_point(5, 0, 0.01))     --> true (tight tolerance)
print(a:contains_point(0, 5))           --> true (top of semicircle)
print(a:contains_point(0, -5))          --> false (bottom, outside angle range)
```

---

### `arc:get_length()`
Calculates and returns the arc length using the formula: `radius × Δangle`, where `Δangle` is the normalized difference between `end_angle` and `start_angle`.

**Returns:** arc length (number)

**Example:**
```lua
local a = Arc.new(0, 0, 10, 0, math.pi)  -- Semicircle, radius 10
print("Arc length:", a:get_length())      --> 31.416... (10π)

local quarter = Arc.new(0, 0, 4, 0, math.pi/2)  -- Quarter circle
print("Quarter arc:", quarter:get_length())     --> 6.283... (2π)
```

---

### `arc:get_point_at_angle(angle)`
Gets the coordinates of a point on the arc at a specific angle.  

**Arguments:**
- `angle` → angle in radians (number)  

**Returns:**  
- `success` (boolean) → `true` if angle is within the arc's angular range `[start_angle, end_angle]`
- `x, y` (numbers) → coordinates of the point on the arc at the given angle

**Formula:** For an angle within range, returns `(center_x + radius×cos(angle), center_y + radius×sin(angle))`

**Example:**
```lua
local a = Arc.new(0, 0, 10, 0, math.pi)  -- Semicircle

local ok, px, py = a:get_point_at_angle(math.pi / 4)  -- 45 degrees
if ok then
    print("Point at 45°:", px, py)  --> ~7.07, ~7.07
end

-- Angle outside range
local ok2, px2, py2 = a:get_point_at_angle(3 * math.pi / 2)  -- 270 degrees
print("Valid:", ok2)  --> false (270° not in 0-180° range)
```

---

### `arc:intersects_rect(rect)`
Checks if the arc intersects with a rectangle.  

**Arguments:**
- `rect` → a `Rect` object  

**Returns:** `true` if the arc intersects the rectangle, `false` otherwise

**Notes:**
- Tests intersection between the arc curve and the rectangle bounds
- Uses the current rectangle geometry (including any rotation configured on the `Rect`)

**Example:**
```lua
local a = Arc.new(0, 0, 10, 0, math.pi)      -- Semicircle
local r = Rect.new(-5, -5, 10, 10)           -- Square centered at origin

if a:intersects_rect(r) then
    print("Arc intersects the square")       --> true
end

local r2 = Rect.new(20, 20, 5, 5)           -- Square far away
print("Intersects far square:", a:intersects_rect(r2))  --> false
```

---

### `arc:toJSON()`
Serializes the arc into a compact JSON string.  

**Returns:** JSON string that can be passed to `Arc.fromJSON()`

**Notes:**
- Encodes center, radius, and angular range
- Raises a Lua error if serialization fails

**Example:**
```lua
local arc = Arc.new(0, 0, 5, 0, math.pi)
local json = arc:toJSON()
local copy = Arc.fromJSON(json)
```

---

## Metamethods

### `tostring(arc)`
Returns a string representation of the arc with its properties.

**Format:** `"Arc: 0x... (x=..., y=..., radius=..., start=..., end=...)"`

**Example:**
```lua
local a = Arc.new(10, 20, 5, 0, math.pi/2)
print(a)  --> Arc: 0x... (x=10.00, y=20.00, radius=5.00, start=0.00, end=1.57)
```



---

## Complete Example

```lua
-- Create different types of arcs
local full_circle = Arc.new(0, 0, 10, 0, 2 * math.pi)  -- Full circle
local semicircle = Arc.new(5, 5, 8, 0, math.pi)        -- Semicircle  
local quarter = Arc.new(-2, 3, 6, math.pi/2, math.pi)  -- Quarter arc (90°-180°)

-- Print arc information
print("Full circle:", full_circle)
print("Length:", full_circle:get_length())  --> ~62.83 (20π)

-- Test point containment
print("Semicircle contains (13,5):", semicircle:contains_point(13, 5))  --> true
print("Semicircle contains (5,13):", semicircle:contains_point(5, 13))  --> false

-- Get points along the arc
local ok, x, y = quarter:get_point_at_angle(3 * math.pi / 4)  -- 135 degrees
if ok then
    print("Point at 135°:", x, y)  --> approximately (-6.24, 7.24)
end

-- Test intersection with rectangles
local rect = Rect.new(0, 0, 20, 20)
print("Full circle intersects rect:", full_circle:intersects_rect(rect))  --> true

-- Modify arc properties
semicircle.radius = 15
semicircle.end_angle = 3 * math.pi / 2  -- Extend to 270 degrees
print("Modified semicircle length:", semicircle:get_length())  --> ~23.56 (15×π/2)

-- Create arc dynamically
local dynamic = Arc.zero()
dynamic.x = 100
dynamic.y = 50
dynamic.radius = 25
dynamic.start_angle = math.pi / 6    -- 30 degrees
dynamic.end_angle = 5 * math.pi / 6  -- 150 degrees
print("Dynamic arc:", dynamic)
```
