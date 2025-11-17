# InputState Lua API

The `InputState` API provides Lua bindings for querying the **current input state**.  
This includes **keyboard keys**, **mouse buttons**, **mouse position**, and **scroll wheel deltas**.

⚠️ **Important:** The `InputState` object is **read-only** in Lua.  
You can inspect its properties, but you cannot modify them directly from Lua.

---

## Overview

In Lua, the input state is represented as a **proxy table** that behaves like a regular Lua object.  
It behaves like a read-only object with properties accessible via dot notation.

**Important Notes:**
- **Completely read-only** - no properties can be modified from Lua
- **Global access** - accessed via the global `InputState` table (not created by scripts)
- **Real-time state** - reflects the current input state as managed by the engine
- **Frame-based input** - `keys_pressed` and `keys_released` are reset each frame
- **Mouse coordinates** are in screen pixels (0,0 = top-left corner)
- **Scroll deltas** represent change since last frame (positive = up/right, negative = down/left)

```lua
-- Access mouse position (global InputState table)
print("Mouse:", InputState.mouse_x, InputState.mouse_y)

-- Check if a key is down
if InputState.keys_down[InputState.KEY.SPACE] then
    print("Spacebar is held down")
end

-- Check if a key was pressed this frame
if InputState.keys_pressed[InputState.KEY.ENTER] then
    print("Enter was just pressed")
end

-- Check if a mouse button is down
if InputState.mouse_buttons[InputState.KEY.MOUSE_LEFT] then
    print("Left mouse button is down")
end
```

---

## Global InputState Table

**Note:** The `InputState` table is a global table created by the engine during initialization. It provides read-only access to the current input state and cannot be created or destroyed by Lua scripts.

---

## InputState Object Properties

Each `InputState` object has the following **read-only** properties:

### Keyboard State
- `keys_down` → table of booleans, indexed by key code. `true` if key is currently held down.  
- `keys_pressed` → table of booleans, indexed by key code. `true` if key was pressed this frame.  
- `keys_released` → table of booleans, indexed by key code. `true` if key was released this frame.  

### Mouse State
- `mouse_x` → current mouse X coordinate in pixels (integer)  
- `mouse_y` → current mouse Y coordinate in pixels (integer)  
- `mouse_scroll_dx` → horizontal scroll delta this frame (integer)  
- `mouse_scroll_dy` → vertical scroll delta this frame (integer)  
- `mouse_down` → read-only table of booleans, indexed by button index (0=left, 1=right, 2=middle, 3=X1, 4=X2)  
- `mouse_clicked` → read-only table of booleans, indexed by button index; true if clicked this frame  
- `mouse_released` → read-only table of booleans, indexed by button index; true if released this frame  

### Key Constants
- `KEY` → table of constants mapping human-readable names to keyboard key codes.  
  Example: `InputState.KEY.A`, `InputState.KEY.SPACE`, `InputState.KEY.F1`

**Notes:**
- **All properties are read-only** - attempting to modify any property will raise a Lua error
- **Frame-based input tracking** - `keys_pressed` and `keys_released` are reset each frame by the engine
- **Mouse coordinates** are in screen pixels with (0,0) at the top-left corner
- **Scroll deltas** represent the change since the last frame (positive = up/right, negative = down/left)
- **Key and mouse tables are read-only** - the `keys_down`, `keys_pressed`, `keys_released`, `mouse_down`, `mouse_clicked`, and `mouse_released` tables cannot be modified
- **Mouse button indices** are 0-based (0=left, 1=right, 2=middle, 3=X1, 4=X2)
- **Key constants** provide human-readable names for supported keyboard keys (letters, numbers, function keys, control keys, symbols, keypad)

**Example:**
```lua
-- Keyboard state
print("W key down:", InputState.keys_down[InputState.KEY.W])
print("Space pressed this frame:", InputState.keys_pressed[InputState.KEY.SPACE])
print("Escape released this frame:", InputState.keys_released[InputState.KEY.ESCAPE])

-- Mouse state
print("Mouse position:", InputState.mouse_x, InputState.mouse_y)
print("Left mouse down:", InputState.mouse_down[0])
print("Right mouse down:", InputState.mouse_down[1])

-- Scroll wheel
if InputState.mouse_scroll_dy > 0 then
    print("Scrolled up")
elseif InputState.mouse_scroll_dy < 0 then
    print("Scrolled down")
end

if InputState.mouse_scroll_dx > 0 then
    print("Scrolled right")
elseif InputState.mouse_scroll_dx < 0 then
    print("Scrolled left")
end

-- Key constants examples (keyboard only)
print("A key code:", InputState.KEY.A)
print("Space key code:", InputState.KEY.SPACE)
print("F1 key code:", InputState.KEY.F1)
```

---

## Restrictions

- **All properties are read-only** - attempting to assign to any property will raise an error:

```lua
-- ❌ Invalid - will cause Lua error
InputState.mouse_x = 100
-- Error: "Input object is read-only"

InputState.mouse_y = 200
-- Error: "Input object is read-only"
```

- **The key and mouse button tables are also read-only**:

```lua
-- ❌ Invalid - will cause Lua error
InputState.keys_down[InputState.KEY.A] = true
-- Error: "Input tables are read-only"

InputState.keys_pressed[InputState.KEY.SPACE] = true
-- Error: "Input tables are read-only"

InputState.mouse_buttons[0] = true
-- Error: "Input tables are read-only"
```

- **No new properties can be added**:

```lua
-- ❌ Invalid - will cause Lua error
InputState.custom_property = "value"
-- Error: "Input object is read-only"
```

---

## InputState Object Methods

**Note:** The InputState API currently only provides property access. No additional methods are available beyond property getters.

---

## Metamethods

- `tostring(InputState)` → returns a string representation of the input state, including mouse position and currently held keys.  
**Notes:**
- The `tostring` metamethod provides a human-readable representation for debugging
- The string format includes mouse position, scroll deltas, and visual representation of held keys

---

## Complete Example

```lua
-- Print complete input state information
print(InputState)  --> Input: 0x... (mouse_x=400, mouse_y=300 mouse_scroll_dx=0, mouse_scroll_dy=0)...

-- Store input state for calculations
local mouse_x = InputState.mouse_x
local mouse_y = InputState.mouse_y

-- Movement controls (WASD)
local moving_forward = InputState.keys_down[InputState.KEY.W]
local moving_backward = InputState.keys_down[InputState.KEY.S]
local moving_left = InputState.keys_down[InputState.KEY.A]
local moving_right = InputState.keys_down[InputState.KEY.D]

if moving_forward then
    print("Moving forward")
end
if moving_backward then
    print("Moving backward")
end
if moving_left then
    print("Moving left")
end
if moving_right then
    print("Moving right")
end

-- Action controls
if InputState.keys_pressed[InputState.KEY.SPACE] then
    print("Jump!")
end

if InputState.keys_pressed[InputState.KEY.ESCAPE] then
    print("Pause menu opened")
end

if InputState.keys_pressed[InputState.KEY.ENTER] then
    print("Action performed")
end

-- Mouse interaction
if InputState.mouse_down[0] then
    print("Left mouse button held at:", mouse_x, mouse_y)
end

if InputState.mouse_down[1] then
    print("Right mouse button held at:", mouse_x, mouse_y)
end

-- Scroll wheel handling
local scroll_y = InputState.mouse_scroll_dy
if scroll_y > 0 then
    print("Scrolled up, zoom in")
elseif scroll_y < 0 then
    print("Scrolled down, zoom out")
end

local scroll_x = InputState.mouse_scroll_dx
if scroll_x > 0 then
    print("Scrolled right")
elseif scroll_x < 0 then
    print("Scrolled left")
end

-- Key state checking function
local function is_key_held(key_constant)
    return InputState.keys_down[key_constant]
end

local function was_key_pressed(key_constant)
    return InputState.keys_pressed[key_constant]
end

local function was_key_released(key_constant)
    return InputState.keys_released[key_constant]
end

-- Usage examples
if is_key_held(InputState.KEY.LSHIFT) then
    print("Left shift is held (sprint mode)")
end

if was_key_pressed(InputState.KEY.F1) then
    print("F1 pressed (help menu)")
end

if was_key_released(InputState.KEY.CAPSLOCK) then
    print("Caps lock released")
end

-- Mouse button checking function
local function is_mouse_button_held(button_index)
    return InputState.mouse_down[button_index]
end

-- Check all mouse buttons
for i = 0, 4 do  -- 0=left, 1=right, 2=middle, 3=X1, 4=X2
    if is_mouse_button_held(i) then
        print("Mouse button", i, "is held at:", mouse_x, mouse_y)
    end
end

-- Input state summary
print("=== Input State Summary ===")
print("Mouse position:", mouse_x, mouse_y)
print("Scroll deltas:", InputState.mouse_scroll_dx, InputState.mouse_scroll_dy)
print("WASD movement:", moving_forward, moving_backward, moving_left, moving_right)
print("Left mouse:", InputState.mouse_down[0])
print("Right mouse:", InputState.mouse_down[1])
```
