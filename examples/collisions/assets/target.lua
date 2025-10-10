function ENTITY:entity_init()
end

function ENTITY:entity_update(delta_time)
end

function ENTITY:entity_collision_enter(entity) 
    -- print("Collision object " .. self.data.name .. " entered with: " .. entity.data.name)
end

function ENTITY:entity_collision_stay(entity) 
    -- print("Collision object " .. self.data.name .. " stayed with: " .. entity.data.name)
end

function ENTITY:entity_collision_exit(entity) 
    -- print("Collision object " .. self.data.name .. " exited with: " .. entity.data.name)
end