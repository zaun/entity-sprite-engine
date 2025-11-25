# Vector Lua API

The `Vector` API provides Lua bindings for working with **2D vectors**.  
A vector is defined by its **x** and **y** floating-point components and supports basic vector operations.

---

## Overview

In Lua, vectors are represented as **proxy tables** that behave like regular Lua objects.  
They behave like objects with properties and methods accessible via dot notation.

⚠️ **Important Notes:**
- Vectors are **mutable** - you can modify x and y components directly
- All mathematical operations are performed in-place (modify the vector itself)
- Vectors can represent both **position** and **direction** depending on context
- No validation is performed on component values

```lua
-- Create a new vector
local v = Vector.new(3, 4)

-- Access properties
print("Components:", v.x, v.y)  --> 3, 4

-- Modify properties
v.x = 10
v.y = 20
print("Modified:", v.x, v.y)  --> 10, 20

-- Vector operations
print("Magnitude:", v:magnitude())  --> ~22.36
v:normalize()                       -- Normalize in-place
print("Normalized:", v.x, v.y)     --> ~0.447, ~0.894
```

---

## Global `Vector` Table

### `Vector.new(x, y)`
Creates a new vector.  

**Arguments:**
- `x` → x-component (number)  
- `y` → y-component (number)  

**Returns:** `Vector` object

**Notes:**
- Both arguments are **required**
- Components can be any numeric value (including negative, fractional, etc.)
- The vector is created with Lua ownership (will be garbage collected)

**Example:**
```lua
local v1 = Vector.new(0, 0)       -- Explicit zero vector
local v2 = Vector.new(5, 7)       -- Vector (5,7)
local v3 = Vector.new(-3.5, 2.1)  -- Vector with negative/fractional components
```

---

### `Vector.zero()`
Creates a vector `(0,0)` (zero vector).  

**Returns:** `Vector` object

**Notes:**
- This is equivalent to `Vector.new()` with no arguments
- Useful for initializing vectors that will be set later
- The vector is created with Lua ownership

**Example:**
```lua
local zero = Vector.zero()
print(zero.x, zero.y)  --> 0, 0

-- Common pattern: create zero vector, then set components
local v = Vector.zero()
v.x = 10
v.y = 20
```

---

### `Vector.fromJSON(json_string)`
Creates a `Vector` from a JSON string previously produced by `vector:toJSON()`.  

**Arguments:**
- `json_string` → string returned from `vector:toJSON()`  

**Returns:** `Vector` object

**Notes:**
- Validates that the JSON represents a vector; invalid JSON raises a Lua error

**Example:**
```lua
local json = some_vector:toJSON()
local copy = Vector.fromJSON(json)
```

---

## Vector Object Properties

Each `Vector` object has the following **read/write** properties:

- `x` → x-component (number)  
- `y` → y-component (number)  

**Notes:**
- Both properties are **mutable** - you can assign new values directly
- No validation is performed on assigned values
- Changes take effect immediately
- Properties accept any numeric value (including negative, fractional, etc.)

**Example:**
```lua
local v = Vector.new(1, 2)
print("Initial:", v.x, v.y)  --> 1, 2

-- Modify components
v.x = v.x + 3
v.y = v.y + 4
print("Modified:", v.x, v.y)  --> 4, 6

-- Direct assignment
v.x = 100.5
v.y = -50.25
print("New values:", v.x, v.y)  --> 100.5, -50.25

-- Mathematical operations
v.x = math.cos(math.pi / 3)  -- cos(60°)
v.y = math.sin(math.pi / 3)  -- sin(60°)
print("Trig values:", v.x, v.y)  --> 0.5, ~0.866
```

---

## Vector Object Methods

### `vector:set_direction(direction, magnitude)`
Sets the vector to point in a **cardinal or ordinal direction** with the given magnitude.  

**Arguments:**
- `direction` → string specifying the direction:
  - **Cardinal directions:** `"n"` (north), `"s"` (south), `"e"` (east), `"w"` (west)
  - **Ordinal directions:** `"ne"` (northeast), `"nw"` (northwest), `"se"` (southeast), `"sw"` (southwest)
- `magnitude` → number (length of the resulting vector)  

**Notes:**
- **Modifies the vector in-place** (changes x and y components)
- North is +Y, East is +X (standard mathematical coordinate system)
- Diagonal directions use 45° angles (π/4 radians)

**Example:**
```lua
local v = Vector.zero()

v:set_direction("n", 10)      -- North: (0, 10)
print(v.x, v.y)               --> 0, 10

v:set_direction("e", 5)       -- East: (5, 0)  
print(v.x, v.y)               --> 5, 0

v:set_direction("ne", 10)     -- Northeast: (~7.07, ~7.07)
print(v.x, v.y)               --> ~7.07, ~7.07

v:set_direction("sw", 8)      -- Southwest: (~-5.66, ~-5.66)
print(v.x, v.y)               --> ~-5.66, ~-5.66
```

---

### `vector:magnitude()`
Calculates and returns the magnitude (length) of the vector using the formula: `√(x² + y²)`.

**Returns:** magnitude (number)

**Notes:**
- Uses the Pythagorean theorem
- Always returns a non-negative value
- Returns `0` for zero vectors

**Example:**
```lua
local v = Vector.new(3, 4)
print(v:magnitude())  --> 5 (3-4-5 triangle)

local v2 = Vector.new(1, 1)
print(v2:magnitude())  --> ~1.414 (√2)

local zero = Vector.zero()
print(zero:magnitude())  --> 0
```

---

### `vector:normalize()`
Normalizes the vector to unit length (magnitude = 1) by dividing both components by the current magnitude.

**Returns:** nothing (modifies the vector in-place)

**Notes:**
- **Modifies the vector in-place** (changes x and y components)
- If the vector is already a zero vector, the result is undefined
- The resulting vector points in the same direction but has unit length

**Example:**
```lua
local v = Vector.new(3, 4)
print("Before:", v.x, v.y, "Magnitude:", v:magnitude())  --> 3, 4, 5

v:normalize()
print("After:", v.x, v.y, "Magnitude:", v:magnitude())   --> 0.6, 0.8, 1.0

-- Normalizing a unit vector doesn't change it
local unit = Vector.new(1, 0)
unit:normalize()
print(unit.x, unit.y)  --> 1, 0
```

---

## Vector Object Methods

### `vector:toJSON()`
Serializes the vector into a compact JSON string.  

**Returns:** JSON string that can be passed to `Vector.fromJSON()`

**Example:**
```lua
local v = Vector.new(10, 20)
local json = v:toJSON()
local copy = Vector.fromJSON(json)
```

---

## Metamethods

### `tostring(vector)`
Returns a string representation of the vector with its components.

**Format:** `"Vector: 0x... (x=..., y=...)"`

**Example:**
```lua
local v = Vector.new(10, 20)
print(v)
```

---

## Complete Example

```lua
-- Create vectors for different purposes
local position = Vector.new(100, 200)
local velocity = Vector.new(5, -2)
local acceleration = Vector.new(0, -9.8)  -- Gravity

-- Print initial state
print("Position:", position)
print("Velocity:", velocity)
print("Acceleration:", acceleration)

-- Calculate speed
local speed = velocity:magnitude()
print("Speed:", speed)

-- Normalize velocity to get direction
local direction = Vector.new(velocity.x, velocity.y)
direction:normalize()
print("Direction:", direction)

-- Set cardinal directions
local north = Vector.zero()
north:set_direction("n", 10)
print("North vector:", north)

local northeast = Vector.zero()
northeast:set_direction("ne", 15)
print("Northeast vector:", northeast)

-- Vector arithmetic
local new_velocity = Vector.new(velocity.x, velocity.y)
new_velocity.x = new_velocity.x + acceleration.x
new_velocity.y = new_velocity.y + acceleration.y
print("New velocity after gravity:", new_velocity)

-- Update position
position.x = position.x + velocity.x
position.y = position.y + velocity.y
print("New position:", position)

-- Create a unit vector at 45 degrees
local diagonal = Vector.new(1, 1)
diagonal:normalize()
print("45° unit vector:", diagonal)

-- Use vectors for movement
local player_pos = Vector.new(0, 0)
local move_speed = 5

-- Move in cardinal directions
local move_north = Vector.zero()
move_north:set_direction("n", move_speed)
player_pos.x = player_pos.x + move_north.x
player_pos.y = player_pos.y + move_north.y
print("Player moved north to:", player_pos)

-- Move diagonally
local move_ne = Vector.zero()
move_ne:set_direction("ne", move_speed)
player_pos.x = player_pos.x + move_ne.x
player_pos.y = player_pos.y + move_ne.y
print("Player moved northeast to:", player_pos)

-- Calculate distance from origin
local distance = player_pos:magnitude()
print("Distance from origin:", distance)
```
