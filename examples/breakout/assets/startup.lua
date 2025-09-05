function STARTUP:startup()
    print("Breakout startup script started")

    -- Load resources and fail if any fail
    if asset_load_atlas("breakout", "bricks.json") == false then
        print("Breakout bricks failed")
        return false
    end
    
    if asset_load_atlas("breakout", "paddle_ball.json") == false then
        print("Breakout paddle_ball failed")
        return false
    end

    -- Load scripts
    if asset_load_script("gamestate.lua") == false then
        print("Loading gamestate script failed")
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

    -- Create the game state manager
    local game_state = Entity.new()
    game_state.components.add(EntityComponentLua.new("gamestate.lua"))
    game_state:add_tag("gamestate")

    -- Create the paddle
    local paddle = Entity.new()
    paddle.components.add(EntityComponentLua.new("paddle.lua"))
    paddle.components.add(EntityComponentSprite.new("breakout:paddle bar blue medium"))
    paddle.components.add(EntityComponentCollider.new(Rect.new(0, 0, 128, 28)))
    paddle:add_tag("paddle")
    paddle.position.x = Display.viewport.width / 2 - 40  -- Center paddle
    paddle.position.y = Display.viewport.height - 50    -- Near bottom of screen

    -- Create the ball
    local ball = Entity.new()
    ball.components.add(EntityComponentLua.new("ball.lua"))
    ball.components.add(EntityComponentSprite.new("breakout:ball blue glass small"))
    ball.components.add(EntityComponentCollider.new(Rect.new(0, 0, 16, 16)))
    ball:add_tag("ball")
    ball.position.x = Display.viewport.width / 2 - 8    -- Center ball
    ball.position.y = Display.viewport.height - 80      -- Above paddle

    -- Set the camera to the center of the viewport
    Camera.position.x = Display.viewport.width / 2
    Camera.position.y = Display.viewport.height / 2

    print("Breakout startup script done")
    return true
end
