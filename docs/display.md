# Display Lua API

The `Display` API provides Lua bindings for querying the **current display state**.  
This includes screen dimensions, fullscreen state, aspect ratio, and viewport configuration.  

⚠️ **Important:** The `Display` object is **read-only** in Lua.  
You can inspect its properties, but you cannot modify them directly from Lua.

---

## Overview

In Lua, the display is represented as a **proxy table** with the metatable `DisplayProxyMeta`.  
It behaves like a read-only object with properties accessible via dot notation.

```lua
-- Access display state
print("Resolution:", Display.width, "x", Display.height)
print("Aspect ratio:", Display.aspect_ratio)
print("Fullscreen:", Display.fullscreen)

-- Access viewport
print("Viewport:", Display.viewport.width, "x", Display.viewport.height)
```

---

## Display Object Properties

Each `Display` object has the following **read-only** properties:

- `fullscreen` → `true` if fullscreen, `false` if windowed  
- `width` → display width in pixels (integer)  
- `height` → display height in pixels (integer)  
- `aspect_ratio` → display aspect ratio (`width / height`) (number)  
- `viewport` → a **read-only table** with:  
  - `width` → viewport width in pixels (integer)  
  - `height` → viewport height in pixels (integer)  

### Example

```lua
print("Display:", Display.width .. "x" .. Display.height)
print("Aspect ratio:", Display.aspect_ratio)

if Display.fullscreen then
    print("Running in fullscreen mode")
else
    print("Running in windowed mode")
end

print("Viewport:", Display.viewport.width, "x", Display.viewport.height)
```

---

## Restrictions

- Attempting to assign to any property will raise an error:

```lua
-- ❌ Invalid
Display.width = 1920
-- Error: "Display object is read-only"
```

- The `viewport` table is also read-only:

```lua
-- ❌ Invalid
Display.viewport.width = 800
-- Error: "Display tables are read-only"
```

---

## Metamethods

- `tostring(Display)` → returns a string representation:  
  `"Display: (WxH, fullscreen/windowed, viewport: WxH)"`  

- Garbage collection (`__gc`) → if Lua owns the display object, memory is freed automatically.  

---

## Example Usage

```lua
-- Print display info
print(Display)

-- Check resolution
if Display.width >= 1920 and Display.height >= 1080 then
    print("HD or higher resolution")
end

-- Use aspect ratio
if math.abs(Display.aspect_ratio - (16/9)) < 0.01 then
    print("16:9 aspect ratio")
end

-- Access viewport
local vw, vh = Display.viewport.width, Display.viewport.height
print("Viewport size:", vw, vh)
```
