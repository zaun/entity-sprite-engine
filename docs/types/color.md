# Color Lua API

The `Color` API provides Lua bindings for working with RGBA colors (float components 0..1).

---

## Global `Color` Table

- `Color.new(r, g, b[, a])` → creates a new color. Alpha defaults to 1.0. Requires 3 or 4 numeric args.
- `Color.white()` → returns a color (1,1,1,1)
- `Color.black()` → returns a color (0,0,0,1)
- `Color.red()` → returns a color (1,0,0,1)
- `Color.green()` → returns a color (0,1,0,1)
- `Color.blue()` → returns a color (0,0,1,1)
- `Color.fromJSON(json_string)` → creates a color from serialized JSON produced by `toJSON()`

---

## Color Object Properties

Read/write properties:
- `r` → red component (number 0..1)
- `g` → green component (number 0..1)
- `b` → blue component (number 0..1)
- `a` → alpha component (number 0..1)

---

## Color Object Methods

- `color:set_hex("#RRGGBB" | "#RRGGBBAA" | "#RGB" | "#RGBA")` → sets components from hex string
- `color:set_byte(r, g, b[, a])` → sets components from byte values (0..255)
- `color:toJSON()` → returns a compact JSON string representing the color

---

## Metamethods

- `tostring(color)` → returns a string like `"Color: r=1.00 g=0.50 b=0.00 a=1.00"`

---

## Examples

```lua
local c = Color.new(1, 0.5, 0)
print(c) -- Color: r=1.00 g=0.50 b=0.00 a=1.00

c:set_hex("#3366CC")
c:set_byte(255, 0, 0, 128)

local json = c:toJSON()
local d = Color.fromJSON(json)
```


