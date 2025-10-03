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
    skull_collider.draw_debug = true
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

    if asset_load_atlas("sceneA", "grassland.jsonc") == false then
        print("Tileset atlas failed")
        return
    end

    if asset_load_map("sceneA", "map.jsonc") == false then
        print("Map failed")
        return
    end

    -- Setup the map display, cetner of screen
    local map_comp = EntityComponentMap.new()
    map_comp.map = asset_get_map("sceneA:map.jsonc")
    map_comp.size = 64
    map_comp.position.x = 6
    map_comp.position.y = 4

    if asset_load_script("map.lua") then
        map_comp.script = "map.lua"
        print("Map script loaded: " .. map_comp.script)
    end

    local map_manager = Entity.new()
    map_manager.draw_order = 0
    map_manager.components.add(map_comp)
    map_manager.position.x = Display.viewport.width / 2
    map_manager.position.y = Display.viewport.height / 2

    local horse = Entity.new()
    horse.draw_order = 3

    if asset_load_script("topDownController.lua") then
        local lua_comp = EntityComponentLua.new()
        lua_comp.script = "topDownController.lua"
        horse.components.add(lua_comp)
    end

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

    print(Camera.position.x, Camera.position.y)

    print("simple startup script done")
end
