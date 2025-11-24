function ENTITY:play(name)
    Entity.publish("PLAY_SOUND", name)
end

function ENTITY:toggle_mode()
    Entity.publish("TOGGLE_MODE")
end

function ENTITY:toggle_music(music)
    if music.is_playing then
        Entity.publish("PAUSE_MUSIC")
    else
        Entity.publish("PLAY_MUSIC")
    end
end

function ENTITY:update_attenuation(listener)
    local newAttenuation = listener.attenuation + 0.25;
    if (newAttenuation > 1.0) then
        newAttenuation = 0
    end
    listener.attenuation = newAttenuation
end

function ENTITY:entity_update(delta_time)
    local v1 = Vector.new(10, 10)
    local v2 = Vector:new(10, 10)

    GUI.start(9, 0, 0, Display.viewport.width, 56)
        GUI.open_flex(
            GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.CENTER, GUI.STYLE.ALIGN.START,
            GUI.STYLE.AUTO_SIZE, GUI.STYLE.AUTO_SIZE
        )
            GUI.open_stack(150, GUI.STYLE.AUTO_SIZE)
                GUI.push_button("Laser 0", ENTITY.play, "laser0")
            GUI.close_stack()
            GUI.open_stack(150, GUI.STYLE.AUTO_SIZE)
                GUI.push_button("Laser 1", ENTITY.play, "laser1")
            GUI.close_stack()
            GUI.open_stack(150, GUI.STYLE.AUTO_SIZE)
                GUI.push_button("Laser 2", ENTITY.play, "laser2")
            GUI.close_stack()
            GUI.open_stack(150, GUI.STYLE.AUTO_SIZE)
                GUI.push_button("Laser 3", ENTITY.play, "laser3")
            GUI.close_stack()
            GUI.open_stack(150, GUI.STYLE.AUTO_SIZE)
                local l = "Play"
                if self.data.sounds.music.is_playing then
                    l = "Stop"
                end
                GUI.push_button(l .. " Music", ENTITY.toggle_music, self.data.sounds.music)
            GUI.close_stack()
        GUI.close_flex()
    GUI.finish()

    GUI.start(9, 0, Display.viewport.height - 59, Display.viewport.width, 56)
        GUI.open_flex(
            GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.START, GUI.STYLE.ALIGN.START,
            GUI.STYLE.AUTO_SIZE, GUI.STYLE.AUTO_SIZE
        )
            GUI.open_stack(200, GUI.STYLE.AUTO_SIZE)
                local l = "Unknown"
                if self.data.listener.data.mode == 0 then
                    l = "Orbit"
                else
                    l = "Drag"
                end
                GUI.push_button("Mode: " .. l, ENTITY.toggle_mode)
            GUI.close_stack()
            GUI.open_stack(200, GUI.STYLE.AUTO_SIZE)
                GUI.push_button("Attenuation: " .. self.data.listener.data.listener.attenuation, ENTITY.update_attenuation, self.data.listener.data.listener)
            GUI.close_stack()
        GUI.close_flex()
    GUI.finish()
end
