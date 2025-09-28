function ENTITY:entity_init()
    local found_indexes = self.components:find("EntityComponentSprite");
    if #found_indexes ~= 1 then
        return;
    end    
    self.data.sprite_comp = self.components[found_indexes[1]];

    found_indexes = self.components:find("EntityComponentCollider");
    if #found_indexes ~= 1 then
        return;
    end    
    self.data.collider_comp = self.components[found_indexes[1]];
end

function ENTITY:entity_update(delta_time)
    if self.data.sprite_comp == nil then
        return
    end

    if self.data.direction == self.data.last_direction and
       self.data.moving == self.data.last_moving and
       self.data.running == self.data.last_running then
        return
    end

    -- print(self.data.direction);
    local direction = "stopped"
    local movement_type = "stopped"
    
    if (self.data.running) then
        movement_type = "running"
    elseif (self.data.moving) then
        movement_type = "walking"
    else
        movement_type = "stopped"
    end

    -- reset collider rotation
    local r = self.data.collider_comp.rects[1]
    r.rotation = 0

    if self.data.directions == 8 and self.data.direction == 2 then
        -- Northeast
        direction = "north east"
        r.width = 110
        r.height = 70
        r.rotation = 135;
    elseif self.data.directions == 8 and self.data.direction == 4 then
        -- Southeast
        direction = "south east"
        r.width = 110
        r.height = 70
        r.rotation = 45;
    elseif self.data.directions == 8 and self.data.direction == 8 then
        -- Northwest
        direction = "north west"
        r.width = 110
        r.height = 70
        r.rotation = 45;
    elseif self.data.directions == 8 and self.data.direction == 6 then
        -- Southwest
        direction = "south west"
        r.width = 110
        r.height = 70
        r.rotation = 135;
    elseif (self.data.directions == 8 and self.data.direction == 3)
        or (self.data.directions == 4 and self.data.direction == 2) then
        -- East
        direction = "east"
        r.width = 110
        r.height = 70
    elseif (self.data.directions == 8 and self.data.direction == 7)
        or (self.data.directions == 4 and self.data.direction == 4) then
        -- West
        direction = "west"
        r.width = 110
        r.height = 70
    elseif (self.data.directions == 8 and self.data.direction == 1)
        or (self.data.directions == 4 and self.data.direction == 1) then
        -- North
        direction = "north"
        r.width = 32
        r.height = 144
    elseif (self.data.directions == 8 and self.data.direction == 5)
        or (self.data.directions == 4 and self.data.direction == 3) then
        -- South
        direction = "south"
        r.width = 32
        r.height = 144
    end

    self.data.collider_comp.offset.x = 144 / 2 - r.width / 2
    self.data.collider_comp.offset.y = 144 / 2 - r.height / 2

    new_sprite = "game:horse " .. movement_type .. " " .. direction

    if self.data.sprite_comp.sprite ~= new_sprite then
        self.data.sprite_comp.sprite = new_sprite
    end
end

function ENTITY:entity_collision_enter(entity) 
    entity.dispatch("move");
end

function ENTITY:entity_collision_stay(entity) 
end

function ENTITY:entity_collision_exit(entity) 
end
