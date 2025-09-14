-- Simple polyline test
local pl = PolyLine.new()
pl.stroke_color = Color.new(1.0, 0.0, 0.0, 1.0)  -- Red
pl.fill_color = Color.new(0.0, 1.0, 0.0, 1.0)    -- Green
pl.stroke_width = 3
pl.type = 2 -- Filled

-- Add points to create a triangle
pl:add_point(Point.new(-25, -25))  -- Top
pl:add_point(Point.new(25, -25))   -- Right
pl:add_point(Point.new(0, 25))     -- Bottom

-- Add the shape to the scene
local shape = Entity.new()
local shape_comp = EntityComponentShape.new()
shape_comp.polyline = pl
shape.components.add(shape_comp)
shape.position.x = 100
shape.position.y = 100
shape.draw_order = 10000

print("Polyline test created")
