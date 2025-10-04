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
    shape_comp.polylines:add(pl)
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
    shape_comp.polylines:add(pl)
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
    shape_comp.polylines:add(pl)
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
    shape_comp.polylines:add(pl)
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
    shape_comp.polylines:add(pl)
    shape.components.add(shape_comp)
    shape.position.x = x
    shape.position.y = y
    shape.draw_order = 10000

    -- -- Add rotate script
    -- local lua_rotate = EntityComponentLua.new()
    -- lua_rotate.script = "rotate.lua"
    -- shape.components.add(lua_rotate)
end

function STARTUP:make_path(x, y, scale, svg_path)
    -- Add the shape to the scene
    local shape = Entity.new()
    local shape_comp = EntityComponentShape.new()

    print("Setting path: " .. svg_path)
    shape_comp:set_path(svg_path, scale)
    
    -- Style the generated polyline(s) for visibility
    local count = shape_comp.polylines.count
    for i = 1, count do
        local pl = shape_comp.polylines[i]
        if pl ~= nil then
            pl.stroke_color = Color.new(1.0, 0.5, 0.0, 1.0)
            pl.fill_color = Color.new(1.0, 1.0, 0.0, 0.4)
            pl.stroke_width = 2
            pl.type = 2 -- Filled
        end
    end

    shape.components.add(shape_comp)
    shape.position.x = x
    shape.position.y = y
    shape.draw_order = 10000

    -- Add rotate script
    local lua_rotate = EntityComponentLua.new()
    lua_rotate.script = "rotate.lua"
    shape.components.add(lua_rotate)
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

    STARTUP:make_path(
        (Camera.position.x + 75) / 2,
        (Camera.position.y + 75) / 2,
        5.0,
        "M0 -2.04443V-0.0444M0 2.9556H0.01M-3.22165 -2.98205L-7.72594 -2.50664C-8.29144 -2.44696 -8.57419 -2.41712 -8.70007 -2.28853C-8.80941 -2.17684 -8.86022 -2.02044 -8.83742 -1.8658C-8.81116 -1.6878 -8.59995 -1.4975 -8.17754 -1.1168L-4.81292 1.9154C-4.68805 2.0279 -4.62561 2.0842 -4.58606 2.1527C-4.55106 2.2134 -4.52919 2.2807 -4.52186 2.3503C-4.51358 2.429 -4.53102 2.5112 -4.5659 2.6757L-5.50566 7.1064C-5.62365 7.6627 -5.68264 7.9408 -5.59925 8.1002C-5.52681 8.2388 -5.39377 8.3354 -5.23966 8.3615C-5.06225 8.3915 -4.81596 8.2495 -4.32338 7.9654L-0.3999 5.7024C-0.2543 5.6184 -0.1815 5.5765 -0.1041 5.56C-0.0356 5.5455 0.0352 5.5455 0.1037 5.56C0.1811 5.5765 0.2539 5.6184 0.3995 5.7024L4.323 7.9654C4.8156 8.2495 5.0619 8.3915 5.2393 8.3615C5.3934 8.3354 5.5264 8.2388 5.5989 8.1002C5.6823 7.9408 5.6233 7.6627 5.5053 7.1064L4.5655 2.6757C4.5306 2.5112 4.5132 2.429 4.5215 2.3503C4.5288 2.2807 4.5507 2.2134 4.5857 2.1527C4.6252 2.0842 4.6877 2.0279 4.8125 1.9154L8.1772 -1.1168C8.5996 -1.4975 8.8108 -1.6878 8.837 -1.8658C8.8598 -2.02044 8.809 -2.17684 8.6997 -2.28853C8.5738 -2.41712 8.2911 -2.44696 7.7256 -2.50664L3.2213 -2.98205C3.0541 -2.99969 2.9705 -3.00851 2.8982 -3.04071C2.8343 -3.06919 2.777 -3.1108 2.7302 -3.16282C2.6772 -3.22161 2.643 -3.29838 2.5745 -3.45192L0.7305 -7.58885C0.499 -8.10823 0.3832 -8.36792 0.222 -8.44789C0.082 -8.51737 -0.0824 -8.51737 -0.2224 -8.44789C-0.3836 -8.36792 -0.4994 -8.10823 -0.7309 -7.58885L-2.57492 -3.45192C-2.64337 -3.29838 -2.67759 -3.22161 -2.73054 -3.16282C-2.7774 -3.1108 -2.83466 -3.06919 -2.89861 -3.04071C-2.97089 -3.00851 -3.05447 -2.99969 -3.22165 -2.98205Z"
    )

    print("simple startup script done")
end
