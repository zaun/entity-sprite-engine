-- Plays sounds at the location of the
-- current entity

function ENTITY:entity_init()
    -- attach the sounds
    self.data.laser0 = EntityComponentSound.new("scifi:laser0")
    self.components.add(self.data.laser0)
    self.data.laser1 = EntityComponentSound.new("scifi:laser1")
    self.components.add(self.data.laser1)
    self.data.laser2 = EntityComponentSound.new("scifi:laser2")
    self.components.add(self.data.laser2)
    self.data.laser3 = EntityComponentSound.new("scifi:laser3")
    self.components.add(self.data.laser3)

    -- subscribe
    self:subscribe("PLAY_SOUND", "play")
end

function ENTITY:entity_update(delta_time)

end

function ENTITY:play(event, name)
    print("Play sound " .. name)
    self.data[name].play();
end
