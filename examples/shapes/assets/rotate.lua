function ENTITY:entity_init()
    local found_indexes = self.components:find("EntityComponentShape");
    if #found_indexes ~= 1 then
        return;
    end    
    self.data.shape_comp = self.components[found_indexes[1]];
end

function ENTITY:entity_update(delta_time)
    local speed = 100
    self.data.shape_comp.rotation = self.data.shape_comp.rotation + speed * delta_time
end

function ENTITY:entity_collision_enter(entity) 
end

function ENTITY:entity_collision_stay(entity) 
end

function ENTITY:entity_collision_exit(entity) 
end
