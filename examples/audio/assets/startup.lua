function STARTUP:startup()
    if asset_load_script("gui.lua") == false then
        print("Failed to load gui.lua")
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

    local listener = Entity.new()
    listener.data.listener = EntityComponentListener.new()
    listener.add_tag("listener")

    local sounds = Entity.new();
    sounds.data.soundA = EntityComponentSound.new("scifi:laser0")
    sounds.components.add(sounds.data.soundA)
    sounds.data.soundB = EntityComponentSound.new("scifi:laser1")
    sounds.components.add(sounds.data.soundB)
    sounds.add_tag("sounds")

    local gui = Entity.new()
    gui.components.add(EntityComponentLua.new("gui.lua"))
    gui.data.sounds = sounds


end
