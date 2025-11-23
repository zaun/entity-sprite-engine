function ENTITY:entity_init()
    -- Angle in radians for the orbit around the center entity
    self.data.angle = 0

    -- Fallback defaults in case startup.lua did not provide these
    if self.data.orbit_radius == nil then
        self.data.orbit_radius = 100
    end
    if self.data.orbit_speed == nil then
        self.data.orbit_speed = math.pi * 0.5 -- radians per second
    end

    -- Configure the attached listener component if present
    if self.data.listener ~= nil then
        -- Enable spatialized audio so position affects how sounds are heard
        self.data.listener.spatial = true

        -- Give the listener a reasonable default volume and distance
        if self.data.listener.volume == 0 then
            self.data.listener.volume = 100
        end
        if self.data.listener.max_distance <= 0 then
            self.data.listener.max_distance = 400
        end
    end
end

function ENTITY:entity_update(delta_time)
    -- Advance orbit phase
    self.data.angle = self.data.angle + self.data.orbit_speed * delta_time

    -- Determine the center of the orbit: prefer the provided center entity,
    -- otherwise fall back to the center of the viewport.
    local cx
    local cy
    if self.data.orbit_center ~= nil and self.data.orbit_center.position ~= nil then
        cx = self.data.orbit_center.position.x
        cy = self.data.orbit_center.position.y
    else
        cx = Display.viewport.width / 2
        cy = Display.viewport.height / 2
    end

    local r = self.data.orbit_radius

    self.position.x = cx + math.cos(self.data.angle) * r
    self.position.y = cy + math.sin(self.data.angle) * r
end
