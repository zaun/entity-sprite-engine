function ENTITY:entity_init()
    self.data.last_direction = "south";

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

    local BASE_SPEED = 75.0
    local DIAGONAL_SPEED = BASE_SPEED * 0.707  -- √2/2 ≈ 0.707 for diagonal movement

    -- Check for movement input and set sprite direction
    local moving = false
    local new_sprite = nil

    -- Check input combinations for 8-directional movement
    local right = InputState.keys_down[InputState.KEY.D]
    local left = InputState.keys_down[InputState.KEY.A]
    local up = InputState.keys_down[InputState.KEY.W]
    local down = InputState.keys_down[InputState.KEY.S]
    local running = InputState.keys_down[InputState.KEY.LSHIFT]

    -- Reset collider rotation
    local r = self.data.collider_comp.rects[1]
    r.rotation = 0;

    -- Setup default movement
    local move_x = 0
    local move_y = 0
    local direction = self.data.last_direction
    local moving = false

    if right and up then
        -- Northeast
        move_x = delta_time * DIAGONAL_SPEED
        move_y = - delta_time * DIAGONAL_SPEED
        direction = "north east"
        r.rotation = 45;
        moving = true
    elseif right and down then
        -- Southeast
        move_x = delta_time * DIAGONAL_SPEED
        move_y = delta_time * DIAGONAL_SPEED
        direction = "south east"
        r.rotation = 45;
        moving = true
    elseif left and up then
        -- Northwest
        move_x = - delta_time * DIAGONAL_SPEED
        move_y = - delta_time * DIAGONAL_SPEED
        direction = "north west"
        r.rotation = 45;
        moving = true
    elseif left and down then
        -- Southwest
        move_x = - delta_time * DIAGONAL_SPEED
        move_y = delta_time * DIAGONAL_SPEED
        direction = "south west"
        r.rotation = 45;
        moving = true
    elseif right then
        -- East
        move_x = delta_time * BASE_SPEED
        direction = "east"
        moving = true
    elseif left then
        -- West
        move_x = - delta_time * BASE_SPEED
        direction = "west"
        moving = true
    elseif up then
        -- North
        move_y = - delta_time * BASE_SPEED
        direction = "north"
        moving = true
    elseif down then
        -- South
        move_y = delta_time * BASE_SPEED
        direction = "south"
        moving = true
    end
    self.data.last_direction = direction
    
    -- Set the sprite and move multiplier
    local movement_type = "stopped"
    local multiplyer = 0;
    if moving and running then
        multiplyer = 2;
        movement_type = "running"
    elseif moving then
        multiplyer = 1;
        movement_type = "running"
    end
    
    -- Move the entity
    self.position.x = self.position.x + (move_x * multiplyer)
    self.position.y = self.position.y + (move_y * multiplyer)

    if self.position.x < 0 then
        self.position.x = 0
    end

    if self.position.y < 0 then
        self.position.y = 0
    end

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
