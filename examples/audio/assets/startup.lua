function STARTUP:startup()
    if asset_load_script("gui.lua") == false then
        print("Failed to load gui.lua")
        return
    end

    if asset_load_sound("scifi", "laser0", "laserRetro_000.ogg") == false then
        print("Failed to load laserRetro_000.ogg")
        return
    end

    local gui = Entity.new()
    gui.components.add(EntityComponentLua.new("gui.lua"))

    local sounds = Entity.new();
    sounds.data.soundA = EntityComponentSound.new("scifi:laser0")
    sounds.components.add(sounds.data.soundA)
    sounds.add_tag("sounds")

    gui.data.sounds = sounds
end
