# Global Lua Environment

When running inside the engine, Lua scripts have access to a **global environment** that exposes engine functionality, state objects, and utility functions.  

This environment is automatically available — you do not need to `require` or import anything.

---

## Global Functions

### `print(...)`
Prints one or more values to the engine log.  
- Accepts any number of arguments.  
- Values are converted to strings and concatenated with tabs.  

```lua
print("Hello", "world", 123)
-- Output: Hello   world   123
```

---

### `asset_load_script(path)`
Loads a Lua script into the engine.  
- `path` → string (path to script file)  

**Returns:** `true` if loaded successfully, `false` otherwise.  

```lua
if not asset_load_script("scripts/player.lua") then
    print("Failed to load player script")
end
```

---

### `asset_load_atlas(group, atlasFile)`
Loads a sprite atlas into the engine’s asset manager.  
- `group` → string (group name)  
- `atlasFile` → string (path to atlas file)  

**Returns:** `true` if loaded successfully, `false` otherwise.  

```lua
asset_load_atlas("sprites", "assets/sprites.atlas")
```

---

### `asset_load_shader(group, shaderFile)`
Compiles and loads a shader into the renderer.  
- `group` → string (group name)  
- `shaderFile` → string (path to shader file)  

**Returns:** `true` if compiled successfully, `false` otherwise.  

```lua
asset_load_shader("shaders", "shaders/basic.glsl")
```

---

### `set_pipeline(vertexShader, fragmentShader)`
Creates and sets a new rendering pipeline.  
- `vertexShader` → string (path to vertex shader)  
- `fragmentShader` → string (path to fragment shader)  

**Returns:** `true` if pipeline created successfully, `false` otherwise.  

```lua
set_pipeline("shaders/vertex.glsl", "shaders/fragment.glsl")
```

---

### `detect_collision(rect, maxResults)`
Detects collisions between a given rectangle and entities in the world.  
- `rect` → a `Rect` object  
- `maxResults` → integer (maximum number of entities to return)  

**Returns:** a table of entities colliding with the rectangle.  

```lua
local r = Rect.new(100, 100, 50, 50)
local hits = detect_collision(r, 10)

for i, e in ipairs(hits) do
    print("Collided with entity:", e.id)
end
```

---

## Global State Objects

The engine also exposes **global state objects** that reflect the current runtime state.  
These are read-only proxies (except where noted).

- **`InputState`** → the current input state (see [Input API](#))  
- **`Display`** → the current display state (see [Display API](#))  
- **`Camera`** → the current camera state (see [Camera API](#))  

### Example

```lua
print("Resolution:", Display.width, "x", Display.height)
print("Mouse:", InputState.mouse_x, InputState.mouse_y)
print("Camera position:", Camera.position.x, Camera.position.y)
```

---

## Example Script

```lua
-- Load assets
asset_load_atlas("sprites", "assets/sprites.atlas")
asset_load_shader("shaders", "shaders/basic.glsl")

-- Create a pipeline
set_pipeline("shaders/vertex.glsl", "shaders/fragment.glsl")

-- Detect collisions
local r = Rect.new(50, 50, 32, 32)
local results = detect_collision(r, 5)
for _, e in ipairs(results) do
    print("Hit entity:", e.id)
end

-- Print debug info
print("Display:", Display.width, "x", Display.height)
print("Mouse:", InputState.mouse_x, InputState.mouse_y)
```
