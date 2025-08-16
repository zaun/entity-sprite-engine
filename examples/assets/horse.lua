local M = {}

function M:entity_init()
    print("entity_init");
    local found_indexes = self.components:find("EntityComponentSprite");
    print(found_indexes);
    self.data.time = 0;

    -- Add these lines to check the return value
    print("Type of found_indexes:", type(found_indexes));
    if type(found_indexes) == "table" then
        print("Count of found_indexes:", #found_indexes);
    end

    if #found_indexes ~= 1 then
        return;
    end    
    self.data.sprite_comp = self.components[found_indexes[1]];

    print(Display);
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

    if right and up then
        -- Northeast
        self.position.x = self.position.x + (delta_time * DIAGONAL_SPEED * multiplyer)
        self.position.y = self.position.y - (delta_time * DIAGONAL_SPEED * multiplyer)
        new_sprite = "game:horse " .. movement_type .. " north east"
        moving = true
    elseif right and down then
        -- Southeast
        self.position.x = self.position.x + (delta_time * DIAGONAL_SPEED * multiplyer)
        self.position.y = self.position.y + (delta_time * DIAGONAL_SPEED * multiplyer)
        new_sprite = "game:horse " .. movement_type .. " south east"
        moving = true
    elseif left and up then
        -- Northwest
        self.position.x = self.position.x - (delta_time * DIAGONAL_SPEED * multiplyer)
        self.position.y = self.position.y - (delta_time * DIAGONAL_SPEED * multiplyer)
        new_sprite = "game:horse " .. movement_type .. " north west"
        moving = true
    elseif left and down then
        -- Southwest
        self.position.x = self.position.x - (delta_time * DIAGONAL_SPEED * multiplyer)
        self.position.y = self.position.y + (delta_time * DIAGONAL_SPEED * multiplyer)
        new_sprite = "game:horse " .. movement_type .. " south west"
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
