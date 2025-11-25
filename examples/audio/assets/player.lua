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

    self.data.music = EntityComponentMusic.new()
    self.data.music.music:add("music:track0")
    self.data.music.music:add("music:track1")
    self.data.music.music:add("music:track2")
    self.data.music.music:add("music:track3")
    self.data.music.music:add("music:track4")
    self.components.add(self.data.music)
    print(self.data.music:toJSON())
    local n = #self.data.music.music
    print("Music has " .. n .. " ttracks")
    print(self:toJSON())

    -- subscribe
    self:subscribe("PLAY_SOUND", "play")
    self:subscribe("PLAY_MUSIC", "play")
    self:subscribe("PAUSE_MUSIC", "play")
end

function ENTITY:play(event, name)
    if event == "PLAY_SOUND" then
        print("Play sound " .. name)
        self.data[name].play();
    elseif event == "PLAY_MUSIC" then
        print("Play music")
        self.data.music.play()
    elseif event == "PAUSE_MUSIC" then
        print("Pause music")
        self.data.music.stop()
    end
end
