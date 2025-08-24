function STARTUP:startup()

    -- load resources and fail if any fail

    if asset_load_atlas("bounce", "balls.json", true) == false then
        print("Loading balls atlas failed")
        return false
    end

    if asset_load_script("manager.lua") == false then
        print("Loading manager script failed")
        return false
    end

    if asset_load_script("ball.lua") == false then
        print("Bounce ball script failed")
        return false
    end

    -- Create the game manager
    local manager = Entity.new();
    manager.components.add(EntityComponentLua.new("manager.lua"))

    -- Set the camera to the center of the viewport
    -- This makes 0,0 the top left of the screen
    Camera.position.x = Display.viewport.width / 2
    Camera.position.y = Display.viewport.height / 2

    print("Startup done")
    return true
end
