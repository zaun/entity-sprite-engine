# Camera Lua API

The `Camera` API provides Lua bindings for working with a **2D camera state**.  
A camera has a **position** (`Point`), a **rotation** (in radians), and a **scale** (zoom factor).

---

## Overview

In Lua, cameras are represented as **proxy tables** with the metatable `CameraStateProxyMeta`.  
They behave like objects with properties accessible via dot notation.

**Important Notes:**
- **Position is a Point object reference** - you cannot assign a new Point, but you can modify the existing one
- **Rotation is in radians** (0 to 2π, where π/2 = 90°, π = 180°, 3π/2 = 270°)
- **Scale affects zoom** (1.0 = normal, >1.0 = zoom in, <1.0 = zoom out)
- **No global constructor** - cameras are created by the engine and accessed via `engine:get_camera()`
- **Memory ownership** is managed by the engine (C-owned, not Lua-owned)

```lua
-- Get camera from engine (from C or engine initialization)
local cam = engine:get_camera()

-- Access properties
print(cam.position.x, cam.position.y)
print(cam.rotation, cam.scale)

-- Modify properties
cam.rotation = math.pi / 4  -- 45 degrees
cam.scale = 2.0             -- zoom in 2x

-- Move camera by modifying its position point
cam.position.x = 100
cam.position.y = 200
```

---

## Global Camera Table

**Note:** There is no global `Camera` table with constructors. Cameras are created by the engine and accessed through engine functions.

---

## Camera Object Properties

Each `Camera` object has the following properties:

- `position` → a `Point` object (read-only reference, but its fields are mutable)  
- `rotation` → camera rotation in radians (read/write)  
- `scale` → camera zoom factor (read/write)  

**Notes:**
- **Position** is a reference to a `Point` object - you cannot assign a new Point, but you can modify the existing one's x and y values
- **Rotation** is in radians (0 = right, π/2 = down, π = left, 3π/2 = up)
- **Scale** affects zoom level (1.0 = normal size, 2.0 = 2x zoom in, 0.5 = 2x zoom out)
- All properties are validated for type safety (rotation and scale must be numbers)
- Changes take effect immediately and affect rendering

**Example:**
```lua
-- Move camera
cam.position.x = cam.position.x + 10
cam.position.y = cam.position.y + 5

-- Rotate camera (in radians)
cam.rotation = cam.rotation + 0.1  -- small rotation increment
cam.rotation = math.pi / 6         -- 30 degrees
cam.rotation = math.pi / 2         -- 90 degrees (pointing down)

-- Zoom in/out
cam.scale = cam.scale * 1.1        -- 10% zoom in
cam.scale = 2.0                    -- 2x zoom in
cam.scale = 0.5                    -- 2x zoom out
cam.scale = 1.0                    -- normal zoom
```

⚠️ **Important:** You cannot directly assign a new `position` object. Instead, modify the existing `Point`:

```lua
-- ❌ Invalid - will cause Lua error
cam.position = Point.new(50, 50)

-- ✅ Correct - modify existing Point's values
cam.position.x = 50
cam.position.y = 50

-- ✅ Also correct - copy values from another Point
local new_pos = Point.new(100, 200)
cam.position.x = new_pos.x
cam.position.y = new_pos.y
```

---

## Camera Object Methods

**Note:** The Camera API currently only provides property access. No additional methods are available beyond property getters and setters.

---

## Metamethods

- `tostring(camera)` → returns a string representation:  
  `"Camera: 0x... (pos=(x,y), rot=..., scale=...)"`  

- Garbage collection (`__gc`) → if Lua owns the camera, memory is freed automatically.

**Notes:**
- The `tostring` metamethod provides a human-readable representation for debugging
- The `__gc` metamethod ensures proper cleanup when Lua garbage collection occurs
- Memory ownership is determined by the `__is_lua_owned` flag in the proxy table
- Cameras are typically C-owned by the engine, so `__gc` usually doesn't free memory

---

## Complete Example

```lua
-- Get camera from engine
local cam = engine:get_camera()

-- Print initial camera state
print(cam)  --> Camera: 0x... (pos=(0.00, 0.00), rot=0.00, scale=1.00)

-- Move camera to center of screen
cam.position.x = 400
cam.position.y = 300
print("Moved camera to:", cam.position.x, cam.position.y)  --> 400, 300

-- Rotate camera 45 degrees clockwise
cam.rotation = math.pi / 4
print("Rotated camera to:", math.deg(cam.rotation), "degrees")  --> 45.0

-- Zoom in for close-up view
cam.scale = 2.0
print("Zoomed in to:", cam.scale, "x")  --> 2.0

-- Create smooth camera movement
local function move_camera_smoothly(target_x, target_y, speed)
    local dx = target_x - cam.position.x
    local dy = target_y - cam.position.y
    local distance = math.sqrt(dx * dx + dy * dy)
    
    if distance > speed then
        local ratio = speed / distance
        cam.position.x = cam.position.x + dx * ratio
        cam.position.y = cam.position.y + dy * ratio
    else
        cam.position.x = target_x
        cam.position.y = target_y
    end
end

-- Smooth camera follow example
move_camera_smoothly(500, 400, 5.0)

-- Check final camera state
print("Final camera:", cam)
print("Position:", cam.position.x, cam.position.y)
print("Rotation:", math.deg(cam.rotation), "degrees")
print("Scale:", cam.scale)
```
