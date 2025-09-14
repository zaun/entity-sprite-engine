function STARTUP:make_square(x, y, size)
    -- Add a shape for testing
    local pl = PolyLine.new()
    pl.stroke_color = Color.new(1.0, 0.5, 0.0, 1.0)  -- Orange
    pl.fill_color = Color.new(1.0, 1.0, 0.0, 1.0)    -- Yellow
    pl.stroke_width = 2
    pl.type = 2 -- Filled
    
    -- Add points to create a square
    pl:add_point(Point.new(-size, -size))  -- Top-left
    pl:add_point(Point.new(size, -size))   -- Top-right
    pl:add_point(Point.new(size, size))    -- Bottom-right
    pl:add_point(Point.new(-size, size))   -- Bottom-left
    
    -- Add the shape to the scene
    local shape = Entity.new()
    local shape_comp = EntityComponentShape.new()
    shape_comp.polyline = pl
    shape.components.add(shape_comp)
    shape.position.x = x
    shape.position.y = y
    shape.draw_order = 10000

    -- Add rotate script
    local lua_rotate = EntityComponentLua.new()
    lua_rotate.script = "rotate.lua"
    shape.components.add(lua_rotate)
end

function STARTUP:make_triangle(x, y, size)
    -- Add a triangle shape for testing
    local pl = PolyLine.new()
    pl.stroke_color = Color.new(0.0, 1.0, 0.5, 1.0)  -- Cyan
    pl.fill_color = Color.new(0.0, 0.5, 1.0, 1.0)    -- Blue
    pl.stroke_width = 2
    pl.type = 2 -- Filled
    
    -- Add points to create a triangle (pointing up)
    pl:add_point(Point.new(0, -size))      -- Top
    pl:add_point(Point.new(size, size))    -- Bottom-right
    pl:add_point(Point.new(-size, size))   -- Bottom-left
    
    -- Add the shape to the scene
    local shape = Entity.new()
    local shape_comp = EntityComponentShape.new()
    shape_comp.polyline = pl
    shape.components.add(shape_comp)
    shape.position.x = x
    shape.position.y = y
    shape.draw_order = 10000

    -- Add rotate script
    local lua_rotate = EntityComponentLua.new()
    lua_rotate.script = "rotate.lua"
    shape.components.add(lua_rotate)
end

function STARTUP:make_octagon(x, y, size)
    -- Add an octagon shape for testing
    local pl = PolyLine.new()
    pl.stroke_color = Color.new(1.0, 0.0, 1.0, 1.0)  -- Magenta
    pl.fill_color = Color.new(0.5, 0.0, 0.5, 1.0)    -- Purple
    pl.stroke_width = 2
    pl.type = 1 -- Closed
    
    -- Calculate octagon points (8-sided regular polygon)
    local angle_step = math.pi / 4  -- 45 degrees in radians
    for i = 0, 7 do
        local angle = i * angle_step
        local x = size * math.cos(angle)
        local y = size * math.sin(angle)
        pl:add_point(Point.new(x, y))
    end
    
    -- Add the shape to the scene
    local shape = Entity.new()
    local shape_comp = EntityComponentShape.new()
    shape_comp.polyline = pl
    shape.components.add(shape_comp)
    shape.position.x = x
    shape.position.y = y
    shape.draw_order = 10000

    -- Add rotate script
    local lua_rotate = EntityComponentLua.new()
    lua_rotate.script = "rotate.lua"
    shape.components.add(lua_rotate)
end

function STARTUP:make_heart(x, y, size)
    -- Add a heart shape for testing
    local pl = PolyLine.new()
    pl.stroke_color = Color.new(1.0, 0.0, 0.0, 1.0)  -- Red
    pl.fill_color = Color.new(1.0, 0.2, 0.2, 1.0)    -- Light red
    pl.stroke_width = 2
    pl.type = 2 -- Filled
    
    -- Create heart shape using parametric equations
    local num_points = 32
    for i = 0, num_points - 1 do
        local t = (i / (num_points - 1)) * 2 * math.pi
        -- Heart parametric equations
        local heart_x = 16 * math.pow(math.sin(t), 3)
        local heart_y = -(13 * math.cos(t) - 5 * math.cos(2*t) - 2 * math.cos(3*t) - math.cos(4*t))
        
        -- Scale and add point
        pl:add_point(Point.new(heart_x * size / 16, heart_y * size / 16))
    end
    
    -- Add the shape to the scene
    local shape = Entity.new()
    local shape_comp = EntityComponentShape.new()
    shape_comp.polyline = pl
    shape.components.add(shape_comp)
    shape.position.x = x
    shape.position.y = y
    shape.draw_order = 10000

    -- Add rotate script
    local lua_rotate = EntityComponentLua.new()
    lua_rotate.script = "rotate.lua"
    shape.components.add(lua_rotate)
end

function STARTUP:make_snowman(x, y, size)
    -- Add a snowman shape using one continuous line with 3 circular sections
    local pl = PolyLine.new()
    pl.stroke_color = Color.new(1.0, 1.0, 1.0, 1.0)  -- White
    pl.fill_color = Color.new(0.9, 0.9, 0.9, 1.0)    -- Light gray
    pl.stroke_width = 3
    pl.type = 2 -- Filled
    
    -- Create snowman using a "3" shape with 3 lobes instead of 2
    -- One continuous line that creates 3 circular sections
    local num_points = 90
    local head_radius = size * 0.2
    local middle_radius = size * 0.3
    local base_radius = size * 0.4
    
    for i = 0, num_points - 1 do
        local t = (i / (num_points - 1)) * 6 * math.pi  -- 6π for 3 full circles
        
        local x, y
        
        if t <= 2 * math.pi then
            -- First circle (head) - full circle
            local angle = t
            x = head_radius * math.cos(angle)
            y = -head_radius * math.sin(angle) - size * 0.4
        elseif t <= 4 * math.pi then
            -- Second circle (middle) - full circle
            local angle = t - 2 * math.pi
            x = middle_radius * math.cos(angle)
            y = middle_radius * math.sin(angle)
        else
            -- Third circle (base) - full circle
            local angle = t - 4 * math.pi
            x = base_radius * math.cos(angle)
            y = base_radius * math.sin(angle) + size * 0.4
        end
        
        pl:add_point(Point.new(x, y))
    end
    
    -- Add the shape to the scene
    local shape = Entity.new()
    local shape_comp = EntityComponentShape.new()
    shape_comp.polyline = pl
    shape.components.add(shape_comp)
    shape.position.x = x
    shape.position.y = y
    shape.draw_order = 10000

    -- -- Add rotate script
    -- local lua_rotate = EntityComponentLua.new()
    -- lua_rotate.script = "rotate.lua"
    -- shape.components.add(lua_rotate)
end

function STARTUP:startup()
    print("simple startup script started")

    if asset_load_script("rotate.lua") == false then
        print("Failed to load rotate.lua")
        return
    end

    Camera.position.x = Display.viewport.width / 2
    Camera.position.y = Display.viewport.height / 2

    STARTUP:make_square(Camera.position.x, Camera.position.y, 25)
    STARTUP:make_triangle(75, 75, 30)
    STARTUP:make_octagon(Display.viewport.width - 75, 75, 35)
    STARTUP:make_heart(75, Display.viewport.height - 75, 50)
    STARTUP:make_snowman(Display.viewport.width - 75, Display.viewport.height - 75, 60)

    print("simple startup script done")
end
