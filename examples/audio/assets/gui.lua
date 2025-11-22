function ENTITY:play_laserRetro0(sounds)
    sounds.data.soundA.play()
end

function ENTITY:play_laserRetro1(sounds)
    sounds.data.soundB.play()
end

function ENTITY:entity_update(delta_time)
    local v1 = Vector.new(10, 10)
    local v2 = Vector:new(10, 10)

    GUI.start(9, 0, 0, Display.viewport.width, 56)
        GUI.open_flex(
            GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.START, GUI.STYLE.ALIGN.START,
            GUI.STYLE.AUTO_SIZE, GUI.STYLE.AUTO_SIZE
        )
            GUI.open_stack(200, GUI.STYLE.AUTO_SIZE)
                GUI.push_button("Laser Retro 0", ENTITY.play_laserRetro0, self.data.sounds)
            GUI.close_stack()
            GUI.open_stack(200, GUI.STYLE.AUTO_SIZE)
                GUI.push_button("Laser Retro 1", ENTITY.play_laserRetro1, self.data.sounds)
            GUI.close_stack()
            GUI.open_stack(200, GUI.STYLE.AUTO_SIZE)
                GUI.push_button("Pos: " .. self.data.sounds.data.soundA.current_frame, ENTITY.play_laserRetro0)
            GUI.close_stack()
        GUI.close_flex()
    GUI.finish()
end
