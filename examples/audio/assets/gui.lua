function ENTITY:play_laserRetro0(sounds)
    print("Sound Device Count: " .. #Sound.devices)
    for name, device in pairs(Sound.devices) do
        print("Device " .. tostring(name) .. ": " .. tostring(device))
    end
    print(sounds.data.soundA.sound)
end

function ENTITY:entity_update(delta_time)
    GUI.start(9, 0, 0, Display.viewport.width, 56)
        GUI.open_flex(
            GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.START, GUI.STYLE.ALIGN.START,
            GUI.STYLE.AUTO_SIZE, GUI.STYLE.AUTO_SIZE
        )
            GUI.open_stack(200, GUI.STYLE.AUTO_SIZE)
                GUI.push_button("Laser Retro 0", ENTITY.play_laserRetro0, self.data.sounds)
            GUI.close_stack()
        GUI.close_flex()
    GUI.finish()
end
