# Vector Lua API

The `Vector` API provides Lua bindings for working with **2D vectors**.  
A vector is defined by its **x** and **y** floating-point components and supports basic vector operations.

---

## Overview

In Lua, vectors are represented as **proxy tables** with the metatable `VectorProxyMeta`.  
They behave like objects with properties and methods accessible via dot notation.

```lua
-- Create a new vector
local v = Vector.new(3, 4)

-- Access properties
print(v.x, v.y)  --> 3, 4

-- Modify properties
v.x = 10
v.y = 20

-- Normalize
v:normalize()
print(v.x, v.y)
```

---

## Global `Vector` Table

### `Vector.new([x, y])`
Creates a new vector.  
- With no arguments: creates a vector `(0,0)`.  
- With 2 arguments: creates a vector with the given components.  

**Returns:** `Vector` object  

```lua
local v1 = Vector.new()       -- (0,0)
local v2 = Vector.new(5, 7)   -- (5,7)
```

---

### `Vector.zero()`
Creates a vector `(0,0)`.  

**Returns:** `Vector` object  

```lua
local v = Vector.zero()
```

---

## Vector Object Properties

Each `Vector` object has the following **read/write** properties:

- `x` → x-component (number)  
- `y` → y-component (number)  

### Example

```lua
local v = Vector.new(1, 2)
v.x = v.x + 3
v.y = v.y + 4
print(v.x, v.y)  --> 4, 6
```

---

## Vector Object Methods

### `vector:set_direction(direction, magnitude)`
Sets the vector to point in a **cardinal or ordinal direction** with the given magnitude.  
- `direction` → string (`"n"`, `"s"`, `"e"`, `"w"`, or combinations like `"ne"`, `"sw"`)  
- `magnitude` → number (length of the resulting vector)  

```lua
local v = Vector.zero()
v:set_direction("ne", 10)
print(v.x, v.y)  --> ~7.07, ~7.07
```

---

### `vector:magnitude()`
Gets the magnitude (length) of the vector.  

**Returns:** number  

```lua
local v = Vector.new(3, 4)
print(v:magnitude())  --> 5
```

---

### `vector:normalize()`
Normalizes the vector to unit length.  

**Returns:** nothing  

```lua
local v = Vector.new(3, 4)
v:normalize()
print(v.x, v.y)  --> ~0.6, ~0.8
```

---

## Metamethods

- `tostring(vector)` → returns a string representation:  
  `"Vector: (x=..., y=...)"`  

- Garbage collection (`__gc`) → if Lua owns the vector, memory is freed automatically.  

---

## Example Usage

```lua
-- Create a vector
local v = Vector.new(3, 4)
print(v)  --> Vector: (x=3.00, y=4.00)

-- Magnitude
print("Length:", v:magnitude())  --> 5

-- Normalize
v:normalize()
print("Normalized:", v.x, v.y)

-- Set direction
v:set_direction("nw", 5)
print("Northwest vector:", v.x, v.y)
```
