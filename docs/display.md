# Display Lua API

The `Display` API provides Lua bindings for querying the **current display state**.  
This includes screen dimensions, fullscreen state, aspect ratio, and viewport configuration.  

⚠️ **Important:** The `Display` object is **read-only** in Lua.  
You can inspect its properties, but you cannot modify them directly from Lua.

---

## Overview

In Lua, the display is represented as a **proxy table** that behaves like a regular Lua object.  
It behaves like a read-only object with properties accessible via dot notation.

**Important Notes:**
- **Completely read-only** - no properties can be modified from Lua
- **Global access** - accessed via the global `Display` table (not created by scripts)
- **Real-time state** - reflects the current display configuration as managed by the engine
- **Viewport is a nested read-only table** - provides viewport dimensions with its own metatable

```lua
-- Access display state (global Display table)
print("Resolution:", Display.width, "x", Display.height)
print("Aspect ratio:", Display.aspect_ratio)
print("Fullscreen:", Display.fullscreen)

-- Access viewport (nested read-only table)
print("Viewport:", Display.viewport.width, "x", Display.viewport.height)
```

---

## Global Display Table

**Note:** The `Display` table is a global table created by the engine during initialization. It provides read-only access to the current display state and cannot be created or destroyed by Lua scripts.

---

## Display Object Properties

Each `Display` object has the following **read-only** properties:

- `fullscreen` → `true` if fullscreen, `false` if windowed (boolean)  
- `width` → display width in pixels (integer)  
- `height` → display height in pixels (integer)  
- `aspect_ratio` → display aspect ratio (`width / height`) (float)  
- `viewport` → a **read-only table** with:  
  - `width` → viewport width in pixels (integer)  
  - `height` → viewport height in pixels (integer)  

**Notes:**
- **All properties are read-only** - attempting to modify any property will raise a Lua error
- **Dimensions are in pixels** - width and height are always positive integers
- **Aspect ratio is calculated** - automatically computed as `width / height` (e.g., 16/9 = 1.777...)
- **Viewport is nested** - the viewport table has its own behavior for read-only access
- **Real-time updates** - properties reflect the current display state as managed by the engine
- **No validation needed** - all values are guaranteed to be valid by the C implementation

**Example:**
```lua
-- Display information
print("Display:", Display.width .. "x" .. Display.height)
print("Aspect ratio:", Display.aspect_ratio)

-- Fullscreen state
if Display.fullscreen then
    print("Running in fullscreen mode")
else
    print("Running in windowed mode")
end

-- Viewport access
print("Viewport:", Display.viewport.width, "x", Display.viewport.height)

-- Common aspect ratio checks
local aspect = Display.aspect_ratio
if math.abs(aspect - (16/9)) < 0.01 then
    print("16:9 aspect ratio (widescreen)")
elseif math.abs(aspect - (4/3)) < 0.01 then
    print("4:3 aspect ratio (standard)")
elseif math.abs(aspect - (21/9)) < 0.01 then
    print("21:9 aspect ratio (ultrawide)")
end
```

---

## Restrictions

- **All properties are read-only** - attempting to assign to any property will raise an error:

```lua
-- ❌ Invalid - will cause Lua error
Display.width = 1920
-- Error: "Display object is read-only"

Display.fullscreen = true
-- Error: "Display object is read-only"

Display.aspect_ratio = 1.0
-- Error: "Display object is read-only"
```

- **The `viewport` table is also read-only**:

```lua
-- ❌ Invalid - will cause Lua error
Display.viewport.width = 800
-- Error: "Display tables are read-only"

Display.viewport.height = 600
-- Error: "Display tables are read-only"
```

- **No new properties can be added**:

```lua
-- ❌ Invalid - will cause Lua error
Display.custom_property = "value"
-- Error: "Display object is read-only"
```

---

## Display Object Methods

**Note:** The Display API currently only provides property access. No additional methods are available beyond property getters.

---

## Metamethods

- `tostring(Display)` → returns a string representation:  
  `"Display: 0x... (WxH, fullscreen/windowed, viewport: WxH)"`  

**Notes:**
- The `tostring` metamethod provides a human-readable representation for debugging
- The string format includes dimensions, fullscreen state, and viewport size

---

## Complete Example

```lua
-- Print complete display information
print(Display)  --> Display: 0x... (1920x1080, windowed, viewport: 1920x1080)

-- Store display dimensions for calculations
local screen_width = Display.width
local screen_height = Display.height
local aspect = Display.aspect_ratio

print("Screen dimensions:", screen_width, "x", screen_height)
print("Aspect ratio:", aspect)

-- Check display capabilities
if screen_width >= 1920 and screen_height >= 1080 then
    print("HD or higher resolution supported")
end

if screen_width >= 2560 and screen_height >= 1440 then
    print("2K resolution supported")
end

if screen_width >= 3840 and screen_height >= 2160 then
    print("4K resolution supported")
end

-- Use aspect ratio for UI scaling
local ui_scale = 1.0
if aspect > 2.0 then
    ui_scale = 1.2  -- Ultrawide displays need larger UI
    print("Ultrawide display detected, increasing UI scale")
elseif aspect < 1.3 then
    ui_scale = 0.9  -- Tall displays can use smaller UI
    print("Tall display detected, decreasing UI scale")
end

-- Viewport information
local vw, vh = Display.viewport.width, Display.viewport.height
print("Viewport size:", vw, "x", vh)

-- Check if viewport matches display (common case)
if vw == screen_width and vh == screen_height then
    print("Viewport matches display size (no letterboxing)")
else
    print("Viewport differs from display (letterboxing or scaling)")
end

-- Fullscreen state handling
if Display.fullscreen then
    print("Running in fullscreen mode")
    -- Fullscreen-specific logic here
else
    print("Running in windowed mode")
    -- Windowed-specific logic here
end

-- Display state summary
print("=== Display Summary ===")
print("Resolution:", screen_width, "x", screen_height)
print("Aspect ratio:", string.format("%.3f", aspect))
print("Fullscreen:", Display.fullscreen)
print("Viewport:", vw, "x", vh)
print("UI Scale:", ui_scale)
```
