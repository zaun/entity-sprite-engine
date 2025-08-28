function STARTUP:startup()

    -- load resources and fail if any fail

    if asset_load_atlas("breakout", "bricks.json") == false then
        print("Breakout bricks failed")
        return false
    end
    
    if asset_load_atlas("breakout", "paddle_ball.json") == false then
        print("Breakout paddle_ball failed")
        return false
    end

    return true
end
