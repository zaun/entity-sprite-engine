# GuiStyle Lua API

The `GuiStyle` API provides Lua bindings for styling GUI elements (layout, spacing, colors, text).

---

## Global `GuiStyle` Table

- `GuiStyle.new()` → creates a new style with defaults
- `GuiStyle.fromJSON(json_string)` → creates a style from JSON produced by `style:toJSON()`

---

## GuiStyle Object Properties

- `direction` → flex direction enum (number)
- `justify` → flex justify enum (number)
- `align_items` → flex align items enum (number)
- `border_width` → integer
- `padding_left`, `padding_top`, `padding_right`, `padding_bottom` → integers
- `spacing` → integer
- `font_size` → integer
- `background`, `background_hovered`, `background_pressed` → `Color`
- `border`, `border_hovered`, `border_pressed` → `Color`
- `text`, `text_hovered`, `text_pressed` → `Color`

---

## GuiStyle Object Methods

- `style:toJSON()` → returns a JSON string representing the style

---

## Examples

```lua
local s = GuiStyle.new()
s.direction = 0
s.justify = 0
s.align_items = 0
s.border_width = 1
s.padding_left, s.padding_top, s.padding_right, s.padding_bottom = 8, 4, 8, 4
s.spacing = 6
s.font_size = 14

s.background = Color.new(0.1, 0.1, 0.1, 1)
s.border = Color.new(0.3, 0.3, 0.3, 1)
s.text = Color.white()

local json = s:toJSON()
local s2 = GuiStyle.fromJSON(json)
```


