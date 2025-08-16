local M = {}

function M:entity_init()
    self.data.time = 0;

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

function M:entity_update(delta_time)
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

    local multiplyer = 1;
    local movement_type = "walking"
    if running then
        multiplyer = 2;
        movement_type = "running"
    end

    -- Reset collider rotation
    local r = self.data.collider_comp.rects[1]
    r.rotation = 0;

    if right and up then
        -- Northeast
        self.position.x = self.position.x + (delta_time * DIAGONAL_SPEED * multiplyer)
        self.position.y = self.position.y - (delta_time * DIAGONAL_SPEED * multiplyer)
        new_sprite = "game:horse " .. movement_type .. " north east"
        r.rotation = 45;
        moving = true
    elseif right and down then
        -- Southeast
        self.position.x = self.position.x + (delta_time * DIAGONAL_SPEED * multiplyer)
        self.position.y = self.position.y + (delta_time * DIAGONAL_SPEED * multiplyer)
        new_sprite = "game:horse " .. movement_type .. " south east"
        r.rotation = 45;
        moving = true
    elseif left and up then
        -- Northwest
        self.position.x = self.position.x - (delta_time * DIAGONAL_SPEED * multiplyer)
        self.position.y = self.position.y - (delta_time * DIAGONAL_SPEED * multiplyer)
        new_sprite = "game:horse " .. movement_type .. " north west"
        r.rotation = 45;
        moving = true
    elseif left and down then
        -- Southwest
        self.position.x = self.position.x - (delta_time * DIAGONAL_SPEED * multiplyer)
        self.position.y = self.position.y + (delta_time * DIAGONAL_SPEED * multiplyer)
        new_sprite = "game:horse " .. movement_type .. " south west"
        r.rotation = 45;
        moving = true
    elseif right then
        -- East
        self.position.x = self.position.x + (delta_time * BASE_SPEED * multiplyer)
        new_sprite = "game:horse " .. movement_type .. " east"
        moving = true
    elseif left then
        -- West
        self.position.x = self.position.x - (delta_time * BASE_SPEED * multiplyer)
        new_sprite = "game:horse " .. movement_type .. " west"
        moving = true
    elseif up then
        -- North
        self.position.y = self.position.y - (delta_time * BASE_SPEED * multiplyer)
        new_sprite = "game:horse " .. movement_type .. " north"
        moving = true
    elseif down then
        -- South
        self.position.y = self.position.y + (delta_time * BASE_SPEED * multiplyer)
        new_sprite = "game:horse " .. movement_type .. " south"
        moving = true
    end

    if self.position.x < 0 then
        self.position.x = 0
    end

    if self.position.y < 0 then
        self.position.y = 0
    end

    -- Update sprite if moving
    if moving and new_sprite then
        self.data.sprite_comp.sprite = new_sprite
    end
end

function M:entity_collision_enter(entity) 
    entity.dispatch("move");
end

function M:entity_collision_stay(entity) 
end

function M:entity_collision_exit(entity) 
end

return M
