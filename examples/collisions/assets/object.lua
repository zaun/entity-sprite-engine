function ENTITY:entity_init()
    self.data.done = false
end

function ENTITY:entity_update(delta_time)
    if self.data.done then
        return
    end

    if self.data.velocity.x == 0 and self.data.velocity.y == 0 then
        return
    end

    self.position.x = self.position.x + self.data.velocity.x * delta_time
    self.position.y = self.position.y + self.data.velocity.y * delta_time
    print("Object " .. self.data.name .. " position: " .. tostring(self.position))
end

function ENTITY:entity_collision_enter(entity) 
    if not entity:has_tag("target") then
        return
    end
    print(self.data.name .. " entered " .. entity.data.name .. ". ")
    print("    " .. self.data.name .. "'s world bounds " ..tostring(self.world_bounds))
    print("    " .. entity.data.name .. "'s world bounds " ..tostring(entity.world_bounds))
end

function ENTITY:entity_collision_stay(entity) 
    if not entity:has_tag("target") then
        return
    end

    if self.data.done then
        print("Collision is broken. Should not be here. Already exited.")
    else
        print(self.data.name .. " stayed " .. entity.data.name .. ". ")
        print("    " .. self.data.name .. "'s world bounds " ..tostring(self.world_bounds))
        print("    " .. entity.data.name .. "'s world bounds " ..tostring(entity.world_bounds))
    end
end

function ENTITY:entity_collision_exit(entity) 
    if not entity:has_tag("target") then
        return
    end

    print(self.data.name .. " exited " .. entity.data.name .. ". ")
    print("    " .. self.data.name .. "'s world bounds " ..tostring(self.world_bounds))
    print("    " .. entity.data.name .. "'s world bounds " ..tostring(entity.world_bounds))
    self.data.done = true
end