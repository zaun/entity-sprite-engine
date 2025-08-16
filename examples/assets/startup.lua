local M = {}

function M:makeskull()
    local skull = Entity.new()

    if asset_load_script("skull.lua") then
        local lua_comp = EntityComponentLua.new();
        lua_comp.script = "skull.lua";
        skull.components.add(lua_comp);
    end

    if asset_load_atlas("game", "skull.json") then
        local sprite_comp = EntityComponentSprite.new();
        sprite_comp.sprite = "game:skull floating east";
        skull.components.add(sprite_comp);
    end

    local skull_collider = EntityComponentCollider.new();
    skull_collider.rects.add(Rect.new(0, 0, 48, 64));
    skull.components.add(skull_collider);

    skull.position.x = Display.viewport.width / 2;
    skull.position.y = Display.viewport.height / 2;

    return skull
end

function M:startup()

    if asset_load_shader("game", "shaders.glsl") == false then
        print("Shaders not loaded")
        return;
    end

    if set_pipeline("game:vertexShader", "game:fragmentShader") == false then
        print("Pipline filed")
        return;
    end

    local horse = Entity.new()

    if asset_load_script("horse.lua") then
        local lua_comp = EntityComponentLua.new();
        lua_comp.script = "horse.lua";
        horse.components.add(lua_comp);
    end

    if asset_load_atlas("game", "horse_saddle.json") then
        local sprite_comp = EntityComponentSprite.new();
        sprite_comp.sprite = "game:horse running east";
        horse.components.add(sprite_comp);
    end

    local horse_collider = EntityComponentCollider.new();
    print(horse_collider.rects);
    horse_collider.rects.add(Rect.new(0, 0, 144, 144));
    horse.components.add(horse_collider);

    M:makeskull()
    M:makeskull()
    M:makeskull()
    M:makeskull()
    M:makeskull()
    M:makeskull()
    M:makeskull()

    Camera.position.x = Display.viewport.width / 2;
    Camera.position.y = Display.viewport.height / 2;
end

return M
