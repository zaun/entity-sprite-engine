function STARTUP:startup()
    print("Breakout startup script started")

    -- 
    -- Load Assets
    --
    if asset_load_atlas("breakout", "bricks.json") == false then
        print("Breakout bricks failed")
        return false
    end
    
    if asset_load_atlas("breakout", "paddle_ball.json") == false then
        print("Breakout paddle_ball failed")
        return false
    end

    if asset_load_script("gamemanager.lua") == false then
        print("Loading gamemanager script failed")
        return false
    end

    if asset_load_script("ball.lua") == false then
        print("Loading ball script failed")
        return false
    end

    if asset_load_script("paddle.lua") == false then
        print("Loading paddle script failed")
        return false
    end

    if asset_load_script("brick.lua") == false then
        print("Loading brick script failed")
        return false
    end

    -- Create the game  manager
    local gamemanager = Entity.new()
    gamemanager.persistent = true; -- When we clear entities, we don't want to clear this one
    gamemanager.components.add(EntityComponentLua.new("gamemanager.lua"))
    gamemanager:add_tag("gamestate")
    gamemanager.data.STATE = {
        "title",
        "countdown",
        "play",
        "level_complete",
        "game_over"
    }

    -- Set the camera to the center of the viewport
    Camera.position.x = Display.viewport.width / 2
    Camera.position.y = Display.viewport.height / 2

    print("Breakout startup script done")
    return true
end
