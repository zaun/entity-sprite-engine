function STARTUP:startup()
    -- Load resources
    if asset_load_script("gui.lua") == false then
        print("Failed to load gui.lua")
        return
    end

    if asset_load_script("player.lua") == false then
        print("Failed to load player.lua")
        return
    end

    if asset_load_script("listener.lua") == false then
        print("Failed to load listener.lua")
        return
    end

    if asset_load_sound("scifi", "laser0", "laserRetro_000.ogg") == false then
        print("Failed to load laserRetro_000.ogg")
        return
    end

    if asset_load_sound("scifi", "laser1", "laserRetro_001.ogg") == false then
        print("Failed to load laserRetro_001.ogg")
        return
    end

    if asset_load_sound("scifi", "laser2", "laserRetro_002.ogg") == false then
        print("Failed to load laserRetro_002.ogg")
        return
    end

    if asset_load_sound("scifi", "laser3", "laserRetro_003.ogg") == false then
        print("Failed to load laserRetro_003.ogg")
        return
    end

    -- Center the camera on the viewport so our shapes are in the middle of the screen
    Camera.position.x = Display.viewport.width / 2
    Camera.position.y = Display.viewport.height / 2

    -- Create a green circle at the center of the screen
    local player_entity = Entity.new()
    player_entity.components.add(EntityComponentLua.new("player.lua"))
    player_entity.draw_order = 3
    local center_shape = EntityComponentShape.new()
    center_shape:set_path('M 10 0 A 10 10 0 0 1 -10 0 A 10 10 0 0 1 10 0 Z', {
        stroke_width = 1.5,
        stroke_color = Color.new(0.0, 1.0, 0.0, 1.0),
        fill_color = Color.new(0.0, 1.0, 0.0, 0.5),
    })
    player_entity.components.add(center_shape)
    player_entity.position.x = Display.viewport.width / 2
    player_entity.position.y = Display.viewport.height / 2

    -- Create a blue circle that orbits around the green center circle.
    -- The audio listener component is attached to this entity.
    local listener_entity = Entity.new()
    listener_entity.draw_order = 3

    local orbit_shape = EntityComponentShape.new()
    orbit_shape:set_path('M 10 0 A 10 10 0 0 1 -10 0 A 10 10 0 0 1 10 0 Z', {
        stroke_width = 1.5,
        stroke_color = Color.new(0.0, 0.5, 1.0, 1.0),
        fill_color = Color.new(0.0, 0.5, 1.0, 0.7),
    })
    listener_entity.components.add(orbit_shape)

    listener_entity.data.listener = EntityComponentListener.new()
    listener_entity.components.add(listener_entity.data.listener)
    listener_entity.data.listener.spatial = true
    listener_entity.data.listener.volume = 100
    listener_entity.data.listener.max_distance = 1000
    -- Use default attenuation/rolloff here; they can be tweaked from Lua as:
    -- listener_entity.data.listener.attenuation = 1.0
    -- listener_entity.data.listener.rolloff = 1.0

    listener_entity.data.orbit_center = player_entity
    listener_entity.data.orbit_radius = 100
    listener_entity.data.orbit_speed = math.pi * 0.5 -- radians per second

    -- Start the listener to the right of the center circle on its orbit path
    listener_entity.position.x = player_entity.position.x + listener_entity.data.orbit_radius
    listener_entity.position.y = player_entity.position.y
    listener_entity.components.add(EntityComponentLua.new("listener.lua"))
    listener_entity.add_tag("listener")

    local gui = Entity.new()
    gui.components.add(EntityComponentLua.new("gui.lua"))
    gui.data.sounds = player_entity.data
    gui.data.listener = listener_entity.data.listener
end
