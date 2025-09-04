function ENTITY:entity_init()
    self.data.type = "brick"
    self.data.width = 64
    self.data.height = 24
    self.data.points = 10
    self.data.destroyed = false
end

function ENTITY:entity_update(delta_time)
    -- Bricks don't need update logic
end

function ENTITY:entity_collision_enter(entity) 
    -- Collision handling is done in the ball's collision function
end

function ENTITY:entity_collision_stay(entity) 
end

function ENTITY:entity_collision_exit(entity) 
end