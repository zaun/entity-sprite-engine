# InputState Lua API

The `Input` API provides Lua bindings for querying the **current input state**.  
This includes **keyboard keys**, **mouse buttons**, **mouse position**, and **scroll wheel deltas**.

⚠️ **Important:** The `Input` object is **read-only** in Lua.  
You can inspect its properties, but you cannot modify them directly from Lua.

---

## Overview

In Lua, the input state is represented as a **proxy table** with the metatable `InputProxyMeta`.  
It behaves like a read-only object with properties accessible via dot notation.

```lua
-- Access mouse position
print("Mouse:", Input.mouse_x, Input.mouse_y)

-- Check if a key is down
if Input.keys_down[Input.KEY.SPACE] then
    print("Spacebar is held down")
end

-- Check if a key was pressed this frame
if Input.keys_pressed[Input.KEY.ENTER] then
    print("Enter was just pressed")
end

-- Check if a mouse button is down
if Input.mouse_buttons[0] then
    print("Left mouse button is down")
end
```

---

## Input Object Properties

Each `Input` object has the following **read-only** properties:

### Keyboard State
- `keys_down` → table of booleans, indexed by key code. `true` if key is currently held down.  
- `keys_pressed` → table of booleans, indexed by key code. `true` if key was pressed this frame.  
- `keys_released` → table of booleans, indexed by key code. `true` if key was released this frame.  

### Mouse State
- `mouse_x` → current mouse X coordinate (integer)  
- `mouse_y` → current mouse Y coordinate (integer)  
- `mouse_scroll_dx` → horizontal scroll delta this frame (integer)  
- `mouse_scroll_dy` → vertical scroll delta this frame (integer)  
- `mouse_buttons` → table of booleans, indexed by button index (0 = left, 1 = right, etc.)  

### Key Constants
- `KEY` → table of constants mapping human-readable names to key codes.  
  Example: `Input.KEY.A`, `Input.KEY.SPACE`, `Input.KEY.F1`, `Input.KEY.MOUSE_LEFT`

---

## Example Usage

```lua
-- Print mouse position
print("Mouse:", Input.mouse_x, Input.mouse_y)

-- Check if W key is held
if Input.keys_down[Input.KEY.W] then
    print("Moving forward")
end

-- Check if Escape was pressed this frame
if Input.keys_pressed[Input.KEY.ESCAPE] then
    print("Pause menu opened")
end

-- Check if left mouse button is down
if Input.mouse_buttons[Input.KEY.MOUSE_LEFT] then
    print("Mouse left button held")
end

-- Check scroll wheel
if Input.mouse_scroll_dy > 0 then
    print("Scrolled up")
elseif Input.mouse_scroll_dy < 0 then
    print("Scrolled down")
end
```

---

## Restrictions

- Attempting to assign to any property will raise an error:

```lua
-- ❌ Invalid
Input.mouse_x = 100
-- Error: "Input object is read-only"
```

- The `keys_down`, `keys_pressed`, `keys_released`, and `mouse_buttons` tables are also read-only:

```lua
-- ❌ Invalid
Input.keys_down[Input.KEY.A] = true
-- Error: "Input tables are read-only"
```

---

## Metamethods

- `tostring(Input)` → returns a string representation of the input state, including mouse position and currently held keys.  
- Garbage collection (`__gc`) → if Lua owns the input object, memory is freed automatically.  
