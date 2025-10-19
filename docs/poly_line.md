# PolyLine Lua API

The `PolyLine` API provides Lua bindings for creating and editing polylines (open/closed/filled) with stroke and fill.

---

## Global `PolyLine` Table

- `PolyLine.new()` → creates an empty polyline
- `PolyLine.fromJSON(json_string)` → creates a polyline from serialized JSON produced by `polyline:toJSON()`

---

## PolyLine Object Properties

- `type` → number enum: `0=OPEN`, `1=CLOSED`, `2=FILLED`
- `stroke_width` → number (line width)
- `stroke_color` → `Color` or `nil`
- `fill_color` → `Color` or `nil`

---

## PolyLine Object Methods

- `polyline:add_point(point)` → appends a `Point` to the list
- `polyline:remove_point(index)` → removes point at 0-based index
- `polyline:get_point(index)` → returns a new `Point` with coordinates at index
- `polyline:get_point_count()` → returns number of points
- `polyline:clear_points()` → removes all points
- `polyline:toJSON()` → returns a compact JSON string representing geometry and styling

---

## Examples

```lua
local p = PolyLine.new()
p.type = 1 -- CLOSED
p.stroke_width = 2

local red = Color.red()
local blue = Color.blue()

p.stroke_color = red
p.fill_color = blue

p:add_point(Point.new(0, 0))
p:add_point(Point.new(100, 0))
p:add_point(Point.new(100, 50))
p:add_point(Point.new(0, 50))

print("points:", p:get_point_count())
local json = p:toJSON()
local p2 = PolyLine.fromJSON(json)
```


