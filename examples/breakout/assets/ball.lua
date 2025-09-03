function ENTITY:entity_init()
    -- Initialize data table
    self.data = {}
    
    -- Ball starts stationary until game begins
    self.data.velocity = Vector.new(0, 0)
    self.data.speed = 300
    self.data.launched = false
    self.data.size = 16
end

function ENTITY:entity_update(delta_time)
    if not self.data.launched then
        return
    end
    
    -- Update ball position based on velocity
    local current_pos = self.position
    local new_x = current_pos.x + self.data.velocity.x * delta_time
    local new_y = current_pos.y + self.data.velocity.y * delta_time
    
    -- Check screen boundaries and bounce
    local viewport_width = Display.viewport.width
    local viewport_height = Display.viewport.height
    local ball_size = self.data.size
    
    -- Bounce off left and right walls
    if new_x <= 0 then
        self.data.velocity.x = -self.data.velocity.x
        new_x = 0
    elseif new_x >= viewport_width - ball_size then
        self.data.velocity.x = -self.data.velocity.x
        new_x = viewport_width - ball_size
    end
    
    -- Bounce off top wall
    if new_y <= 0 then
        self.data.velocity.y = -self.data.velocity.y
        new_y = 0
    end
    
    -- Check if ball fell below screen (lose life)
    if new_y >= viewport_height then
        -- Notify game state that ball was lost
        local game_states = Entity.find_by_tag("gamestate")
        if game_states and #game_states > 0 then
            local game_state = game_states[1]  -- Get first game state
            game_state:dispatch("ball_lost")
        end
        return
    end

    -- Update ball position
    self.position = Point.new(new_x, new_y)
end

function ENTITY:launch_ball()
    if self.data.launched then
        return
    end
    
    -- Launch ball at random angle upward
    local angle = (math.random() - 0.5) * math.pi * 0.5  -- -45 to +45 degrees
    self.data.velocity = Vector.new(
        math.sin(angle) * self.data.speed,
        -math.cos(angle) * self.data.speed  -- Negative Y for upward movement
    )
    self.data.launched = true
end

function ENTITY:reset_ball()
    self.data.launched = false
    self.data.velocity = Vector.new(0, 0)
    -- Position will be set by game state
end

function ENTITY:entity_collision_enter(entity) 
    if entity.data and entity.data.type == "paddle" then
        -- Bounce off paddle with angle based on hit position
        local paddle_center = entity.position.x + entity.data.width / 2
        local ball_center = self.position.x + self.data.size / 2
        local hit_offset = (ball_center - paddle_center) / (entity.data.width / 2)
        hit_offset = math.max(-1, math.min(1, hit_offset))  -- Clamp to [-1, 1]
        
        local angle = hit_offset * math.pi / 3  -- Max 60 degrees
        self.data.velocity = Vector.new(
            math.sin(angle) * self.data.speed,
            -math.abs(math.cos(angle)) * self.data.speed  -- Always bounce upward
        )
    elseif entity.data and entity.data.type == "brick" then
        -- Destroy brick and bounce ball
        local ball_center_x = self.position.x + self.data.size / 2
        local ball_center_y = self.position.y + self.data.size / 2
        local brick_center_x = entity.position.x + entity.data.width / 2
        local brick_center_y = entity.position.y + entity.data.height / 2
        
        -- Determine bounce direction based on which side was hit
        local dx = ball_center_x - brick_center_x
        local dy = ball_center_y - brick_center_y
        
        if math.abs(dx) > math.abs(dy) then
            -- Hit from left or right
            self.data.velocity.x = -self.data.velocity.x
        else
            -- Hit from top or bottom
            self.data.velocity.y = -self.data.velocity.y
        end
        
        -- Destroy the brick
        entity:destroy()
        
        -- Notify game state for scoring
        local game_states = Entity.find_by_tag("gamestate")
        if game_states and #game_states > 0 then
            local game_state = game_states[1]  -- Get first game state
            game_state:dispatch("brick_destroyed")
        end
    end
end

function ENTITY:entity_collision_stay(entity) 
end

function ENTITY:entity_collision_exit(entity) 
end
