# Camera Lua API

The `Camera` API provides Lua bindings for working with a **2D camera state**.  
A camera has a **position** (`Point`), a **rotation** (in radians), and a **scale** (zoom factor).

---

## Overview

In Lua, cameras are represented as **proxy tables** with the metatable `CameraStateProxyMeta`.  
They behave like objects with properties accessible via dot notation.

```lua
-- Create a camera (from C or engine initialization)
local cam = engine:get_camera()

-- Access properties
print(cam.position.x, cam.position.y)
print(cam.rotation, cam.scale)

-- Modify properties
cam.rotation = math.pi / 4
cam.scale = 2.0

-- Move camera by modifying its position point
cam.position.x = 100
cam.position.y = 200
```

---

## Camera Object Properties

Each `Camera` object has the following properties:

- `position` → an `Point` object (read-only reference, but its fields are mutable)  
- `rotation` → camera rotation in radians (read/write)  
- `scale` → camera zoom factor (read/write)  

### Example

```lua
-- Move camera
cam.position.x = cam.position.x + 10
cam.position.y = cam.position.y + 5

-- Rotate camera
cam.rotation = cam.rotation + 0.1

-- Zoom in
cam.scale = cam.scale * 1.1
```

⚠️ **Note:** You cannot directly assign a new `position` object. Instead, modify the existing `Point`:

```lua
-- ❌ Invalid
cam.position = Point.new(50, 50)

-- ✅ Correct
cam.position.x = 50
cam.position.y = 50
```

---

## Metamethods

- `tostring(camera)` → returns a string representation:  
  `"Camera: (pos=(x,y), rot=..., scale=...)"`  

- Garbage collection (`__gc`) → if Lua owns the camera, memory is freed automatically.  

---

## Example Usage

```lua
-- Assume engine provides a camera
local cam = engine:get_camera()

-- Print camera info
print(cam)

-- Move camera
cam.position.x = 10
cam.position.y = 20

-- Rotate and zoom
cam.rotation = math.pi / 6
cam.scale = 1.5

print("Camera:", cam)
```
