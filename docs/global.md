# Global Lua Environment

When running inside the engine, Lua scripts have access to a **global environment** that exposes engine functionality, state objects, and utility functions.  

This environment is automatically available — you do not need to `require` or import anything.

---

## Overview

The global Lua environment is carefully controlled and sandboxed for security. It provides:

**Important Notes:**
- **Sandboxed execution** - scripts run in isolated environments with limited access
- **Security restrictions** - dangerous functions like `dofile`, `loadfile`, and `require` are removed
- **Memory limits** - enforced 10MB memory limit and execution timeouts
- **Global locking** - the global environment is locked against modification after initialization
- **Standard libraries** - limited subset of Lua standard libraries for safety
- **Engine integration** - direct access to engine state objects and functions

---

## Standard Lua Libraries

The engine provides a limited subset of Lua standard libraries for safety:

- **`math`** - mathematical functions and constants
- **`string`** - string manipulation functions  
- **`table`** - table manipulation functions
- **`_VERSION`** - Lua version information

**Notes:**
- **Limited access** - only safe, non-dangerous functions are available
- **No file I/O** - file operations are restricted for security
- **No OS access** - operating system functions are not available
- **Memory safe** - all functions respect memory limits and timeouts

---

## Global Functions

### `print(...)`
Prints one or more values to the engine log.  

**Arguments:**
- `...` → variable arguments of any type

**Returns:** nothing

**Notes:**
- **Engine logging** - output goes to the engine's logging system, not console
- **Type conversion** - all values are automatically converted to strings
- **Tab separation** - multiple arguments are separated by tabs
- **Safe operation** - cannot cause crashes or security issues
- **Performance** - optimized for minimal overhead

**Example:**
```lua
print("Hello", "world", 123)
-- Output: Hello   world   123

print("Entity position:", entity.position.x, entity.position.y)
-- Output: Entity position:   100.0    200.0

print("Debug info:", {health = 100, active = true})
-- Output: Debug info:   table: 0x...
```

---

### `asset_load_script(path)`
Loads a Lua script into the engine's script collection.  

**Arguments:**
- `path` → string (path to script file relative to engine working directory)

**Returns:** `true` if loaded successfully, `false` otherwise

**Notes:**
- **Script collection** - loaded scripts are stored in the engine's script manager
- **Path resolution** - paths are relative to the engine's working directory
- **Error handling** - returns false if file not found or contains syntax errors
- **Memory management** - scripts are managed by the engine, not Lua
- **Security** - only script files can be loaded, no arbitrary file access

**Example:**
```lua
if not asset_load_script("scripts/player.lua") then
    print("Failed to load player script")
    return
end

if not asset_load_script("scripts/enemy.lua") then
    print("Failed to load enemy script")
    return
end

print("All scripts loaded successfully")
```

---

### `asset_load_atlas(group, atlasFile)`
Loads a sprite atlas into the engine's asset manager.  

**Arguments:**
- `group` → string (group name for organizing assets)
- `atlasFile` → string (path to atlas file relative to engine working directory)

**Returns:** `true` if loaded successfully, `false` otherwise

**Notes:**
- **Asset grouping** - assets are organized by group for efficient management
- **Atlas format** - supports standard sprite atlas formats
- **Memory management** - atlases are managed by the engine's asset manager
- **Rendering integration** - loaded atlases are automatically available to renderer
- **Error handling** - returns false if file not found or format invalid

**Example:**
```lua
-- Load sprite atlases
if not asset_load_atlas("sprites", "assets/sprites.atlas") then
    print("Failed to load sprites atlas")
    return
end

if not asset_load_atlas("ui", "assets/ui.atlas") then
    print("Failed to load UI atlas")
    return
end

print("All atlases loaded successfully")
```

---

### `asset_load_shader(group, shaderFile)`
Compiles and loads a shader into the renderer.  

**Arguments:**
- `group` → string (group name for organizing shaders)
- `shaderFile` → string (path to shader file relative to engine working directory)

**Returns:** `true` if compiled successfully, `false` otherwise

**Notes:**
- **Shader compilation** - GLSL shaders are compiled to GPU bytecode
- **Error reporting** - compilation errors are logged to the engine log
- **Render pipeline** - loaded shaders can be used in rendering pipelines
- **Memory management** - shaders are managed by the renderer
- **Performance** - compiled shaders are cached for efficient reuse

**Example:**
```lua
-- Load basic shaders
if not asset_load_shader("shaders", "shaders/basic.glsl") then
    print("Failed to load basic shader")
    return
end

-- Load specialized shaders
if not asset_load_shader("shaders", "shaders/particle.glsl") then
    print("Failed to load particle shader")
    return
end

print("All shaders loaded successfully")
```

---

### `asset_load_map(group, mapFile)`
Loads a map file into the engine's asset manager.  

**Arguments:**
- `group` → string (group name for organizing maps)
- `mapFile` → string (path to map file relative to engine working directory)

**Returns:** `true` if loaded successfully, `false` otherwise

**Notes:**
- **Map format** - supports standard map formats (TMX, etc.)
- **Tile integration** - maps are integrated with the tile rendering system
- **Memory management** - maps are managed by the engine's asset manager
- **Entity integration** - maps can be attached to entities via MapComponent
- **Error handling** - returns false if file not found or format invalid

**Example:**
```lua
-- Load level maps
if not asset_load_map("maps", "assets/level1.tmx") then
    print("Failed to load level 1")
    return
end

if not asset_load_map("maps", "assets/level2.tmx") then
    print("Failed to load level 2")
    return
end

print("All maps loaded successfully")
```

---

### `asset_get_map(mapName)`
Retrieves a loaded map from the asset manager.  

**Arguments:**
- `mapName` → string (name of the map to retrieve)

**Returns:** Map object if found, `nil` otherwise

**Notes:**
- **Asset lookup** - searches for maps by name in the asset manager
- **Object reference** - returns a reference to the loaded map object
- **Entity attachment** - returned maps can be attached to entities
- **Memory safety** - maps are managed by the engine, not Lua
- **Error handling** - returns nil if map not found

**Example:**
```lua
-- Load a map first
if not asset_load_map("maps", "assets/level1.tmx") then
    print("Failed to load level 1")
    return
end

-- Retrieve the loaded map
local level1 = asset_get_map("level1.tmx")
if level1 then
    print("Level 1 loaded:", level1.width, "x", level1.height)
    
    -- Attach to an entity
    local map_entity = Entity.new()
    local map_component = EntityComponentMap.new()
    map_component.map = level1
    map_entity.components:add(map_component)
else
    print("Level 1 not found")
end
```

---

### `set_pipeline(vertexShader, fragmentShader)`
Creates and sets a new rendering pipeline.  

**Arguments:**
- `vertexShader` → string (path to vertex shader file)
- `fragmentShader` → string (path to fragment shader file)

**Returns:** `true` if pipeline created successfully, `false` otherwise

**Notes:**
- **Shader compilation** - shaders are compiled if not already loaded
- **Pipeline creation** - creates a new GPU rendering pipeline
- **Render state** - sets the current renderer to use this pipeline
- **Performance** - pipeline switching has some overhead
- **Error handling** - returns false if shaders not found or compilation fails

**Example:**
```lua
-- Set basic rendering pipeline
if not set_pipeline("shaders/vertex.glsl", "shaders/fragment.glsl") then
    print("Failed to set basic pipeline")
    return
end

-- Set specialized pipeline for effects
if not set_pipeline("shaders/particle_vertex.glsl", "shaders/particle_fragment.glsl") then
    print("Failed to set particle pipeline")
    return
end

print("Pipeline set successfully")
```

---

### `detect_collision(rect, maxResults)`
Detects collisions between a given rectangle and entities in the world.  

**Arguments:**
- `rect` → a `Rect` object (collision detection area)
- `maxResults` → integer (maximum number of entities to return)

**Returns:** a table of entities colliding with the rectangle

**Notes:**
- **Collision detection** - uses the engine's collision detection system
- **Entity filtering** - only returns entities with collider components
- **Performance** - optimized for efficient collision detection
- **Result limit** - respects maxResults parameter to prevent excessive results
- **Memory safety** - returned entities are references, not copies

**Example:**
```lua
-- Create collision detection area
local r = Rect.new(100, 100, 50, 50)

-- Detect collisions (max 10 results)
local hits = detect_collision(r, 10)

if #hits > 0 then
    print("Found", #hits, "colliding entities:")
    for i, e in ipairs(hits) do
        print("  Entity", i, ":", e.id, "at", e.position.x, e.position.y)
    end
else
    print("No collisions detected")
end

-- Use for player interaction
local player_area = Rect.new(player.position.x - 16, player.position.y - 16, 32, 32)
local nearby_entities = detect_collision(player_area, 5)

for _, entity in ipairs(nearby_entities) do
    if entity.data and entity.data.type == "item" then
        print("Found item:", entity.data.name)
    end
end
```

---

## Global State Objects

The engine exposes **global state objects** that reflect the current runtime state.  
These are read-only proxies (except where noted).

- **`InputState`** → the current input state (see [InputState API](inputstate.md))  
- **`Display`** → the current display state (see [Display API](display.md))  
- **`Camera`** → the current camera state (see [Camera API](camera.md))  

**Notes:**
- **Read-only access** - these objects cannot be modified from Lua
- **Real-time state** - reflect current engine state, not cached values
- **Global scope** - available in all Lua scripts without import
- **Memory safety** - objects are managed by the engine, not Lua
- **Performance** - direct access to engine state, very fast

**Example:**
```lua
-- Access current engine state
print("Resolution:", Display.width, "x", Display.height)
print("Fullscreen:", Display.fullscreen)
print("Aspect ratio:", Display.aspect_ratio)

print("Mouse position:", InputState.mouse_x, InputState.mouse_y)
print("Mouse buttons:", InputState.mouse_left, InputState.mouse_right, InputState.mouse_middle)
print("Keyboard state:", InputState.keys_down)

print("Camera position:", Camera.position.x, Camera.position.y)
print("Camera rotation:", Camera.rotation)
print("Camera scale:", Camera.scale)

-- Use state for game logic
if InputState.keys_down["w"] then
    print("W key is pressed")
end

if InputState.mouse_left then
    print("Left mouse button is pressed")
end

-- Check display properties
if Display.fullscreen then
    print("Running in fullscreen mode")
end
```

---

## Global Type Constructors

The engine provides global constructors for creating engine objects:

- **`Entity.new()`** → creates new entities (see [Entity API](entity.md))
- **`Point.new(x, y)`** → creates new points (see [Point API](point.md))
- **`Rect.new(x, y, w, h)`** → creates new rectangles (see [Rect API](rect.md))
- **`Vector.new(x, y)`** → creates new vectors (see [Vector API](vector.md))
- **`Arc.new(x, y, radius, start_angle, end_angle)`** → creates new arcs (see [Arc API](arc.md))
- **`Ray.new(x, y, dx, dy)`** → creates new rays (see [Ray API](ray.md))
- **`UUID.new()`** → creates new UUIDs (see [UUID API](uuid.md))
- **`EntityComponentSprite.new([spriteName])`** → creates sprite components (see [EntityComponent API](entitycomponent.md))
- **`EntityComponentCollider.new()`** → creates collider components (see [EntityComponent API](entitycomponent.md))
- **`EntityComponentLua.new([scriptPath])`** → creates Lua script components (see [EntityComponent API](entitycomponent.md))
- **`EntityComponentMap.new([mapAsset])`** → creates map components (see [EntityComponent API](entitycomponent.md))

**Notes:**
- **Global scope** - available in all Lua scripts without import
- **Object creation** - each constructor creates new engine objects
- **Memory ownership** - objects created via constructors are owned by Lua
- **Type safety** - constructors validate arguments and provide proper defaults
- **Integration** - created objects integrate with the engine's systems

---

## Environment Restrictions

The global environment has several security restrictions:

- **No file I/O** - `io` library is not available
- **No OS access** - `os` library is not available
- **No package loading** - `require` and `package` are removed
- **No dynamic loading** - `dofile` and `loadfile` are removed
- **Global locking** - global variables cannot be modified after initialization
- **Memory limits** - enforced memory allocation limits
- **Execution timeouts** - scripts have maximum execution time limits

**Notes:**
- **Security focus** - restrictions prevent malicious script behavior
- **Performance** - limited libraries reduce memory usage and startup time
- **Stability** - prevents scripts from crashing the engine
- **Sandboxing** - each script runs in isolated environment

---

## Complete Example

```lua
-- Load assets
print("Loading game assets...")

if not asset_load_atlas("sprites", "assets/sprites.atlas") then
    print("Failed to load sprites atlas")
    return
end

if not asset_load_shader("shaders", "shaders/basic.glsl") then
    print("Failed to load basic shader")
    return
end

if not asset_load_map("maps", "assets/level1.tmx") then
    print("Failed to load level 1")
    return
end

print("All assets loaded successfully")

-- Set rendering pipeline
if not set_pipeline("shaders/vertex.glsl", "shaders/fragment.glsl") then
    print("Failed to set rendering pipeline")
    return
end

print("Rendering pipeline set")

-- Create game objects
local player = Entity.new()
player.position.x = 400
player.position.y = 300

local sprite = EntityComponentSprite.new("player_sprite.png")
player.components:add(sprite)

local collider = EntityComponentCollider.new()
collider.rects:add(Rect.new(-16, -16, 32, 32))
player.components:add(collider)

local script = EntityComponentLua.new("scripts/player.lua")
player.components:add(script)

print("Player entity created with ID:", player.id)

-- Detect initial collisions
local player_area = Rect.new(player.position.x - 32, player.position.y - 32, 64, 64)
local nearby_entities = detect_collision(player_area, 10)

if #nearby_entities > 0 then
    print("Found", #nearby_entities, "entities near player")
    for i, entity in ipairs(nearby_entities) do
        print("  Entity", i, ":", entity.id)
    end
end

-- Print engine state
print("=== Engine State ===")
print("Display:", Display.width, "x", Display.height, "fullscreen:", Display.fullscreen)
print("Mouse:", InputState.mouse_x, InputState.mouse_y)
print("Camera:", Camera.position.x, Camera.position.y, "rotation:", Camera.rotation, "scale:", Camera.scale)

-- Game loop setup
print("Game initialization complete")
print("Player ready at position:", player.position.x, player.position.y)
print("Player has", player.components.count, "components")
```
