function STARTUP:makeskull()
    local skull = Entity.new()

    if asset_load_script("skull.lua") then
        local lua_comp = EntityComponentLua.new()
        lua_comp.script = "skull.lua"
        skull.components.add(lua_comp)
    end

    if asset_load_atlas("game", "skull.json", true) then
        local sprite_comp = EntityComponentSprite.new()
        sprite_comp.sprite = "game:skull floating east"
        skull.components.add(sprite_comp)
    end

    local skull_collider = EntityComponentCollider.new()
    skull_collider.rects.add(Rect.new(0, 0, 48, 64))
    skull.components.add(skull_collider)

    skull.position.x = Display.viewport.width / 2
    skull.position.y = Display.viewport.height / 2

    return skull
end

function STARTUP:startup()
    print("simple startup script started")

    if asset_load_shader("game", "shaders.glsl") == false then
        print("Shaders failed")
        return
    end

    if set_pipeline("game:vertexShader", "game:fragmentShader") == false then
        print("Pipline failed")
        return
    end

    if asset_load_atlas("grassland", "grassland.json") == false then
        print("Tileset atlas failed")
        return
    end

    if asset_load_map("game", "map.json") == false then
        print("Map failed")
        return
    end

    -- Setup the map display, cetner of screen
    local map_comp = EntityComponentMap.new()
    map_comp.map = asset_get_map("game:map.json")
    map_comp.size = 64
    map_comp.position.x = 6
    map_comp.position.y = 4
    local map_manager = Entity.new()
    map_manager.draw_order = -1000
    map_manager.components.add(map_comp)
    map_manager.position.x = Display.viewport.width / 2
    map_manager.position.y = Display.viewport.height / 2

    local horse = Entity.new()

    if asset_load_script("horse.lua") then
        local lua_comp = EntityComponentLua.new()
        lua_comp.script = "horse.lua"
        horse.components.add(lua_comp)
    end

    if asset_load_atlas("game", "horse_saddle.json", true) then
        local sprite_comp = EntityComponentSprite.new()
        sprite_comp.sprite = "game:horse running east"
        horse.components.add(sprite_comp)
    end

    local horse_collider = EntityComponentCollider.new()
    horse_collider.draw_debug = true
    print(horse_collider.rects)
    horse_collider.rects.add(Rect.new(0, 0, 144, 144))
    horse.components.add(horse_collider)

    local horse_text = EntityComponentText.new()
    horse_text.text = "Da Horse"
    horse.components.add(horse_text)

    STARTUP:makeskull()
    STARTUP:makeskull()
    STARTUP:makeskull()
    STARTUP:makeskull()
    STARTUP:makeskull()
    STARTUP:makeskull()
    STARTUP:makeskull()

    Camera.position.x = Display.viewport.width / 2
    Camera.position.y = Display.viewport.height / 2

    -- if asset_load_script("rotate.lua") == false then
    --     print("Failed to load rotate.lua")
    --     return
    -- end

    -- -- Add a shape for testing
    -- local pl = PolyLine.new()
    -- pl.stroke_color = Color.new(1.0, 0.5, 0.0, 1.0)  -- Orange
    -- pl.fill_color = Color.new(1.0, 1.0, 0.0, 1.0)    -- Yellow
    -- pl.stroke_width = 2
    -- pl.type = 2 -- Filled
    
    -- -- Add points to create a square
    -- pl:add_point(Point.new(-50, -50))  -- Top-left
    -- pl:add_point(Point.new(50, -50))   -- Top-right
    -- pl:add_point(Point.new(50, 50))    -- Bottom-right
    -- pl:add_point(Point.new(-50, 50))   -- Bottom-left
    
    -- -- Add the shape to the scene
    -- local shape = Entity.new()
    -- local shape_comp = EntityComponentShape.new()
    -- shape_comp.polyline = pl
    -- shape.components.add(shape_comp)
    -- shape.position.x = Display.viewport.width / 2
    -- shape.position.y = Display.viewport.height / 2
    -- shape.draw_order = 10000

    -- -- Add rotate script
    -- local lua_rotate = EntityComponentLua.new()
    -- lua_rotate.script = "rotate.lua"
    -- shape.components.add(lua_rotate)

    print("simple startup script done")
end
