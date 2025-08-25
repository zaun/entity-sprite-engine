function ENTITY:entity_init()
    -- Set random initial velocity for the ball
    local angle = math.random() * 2 * math.pi
    local speed = 200 + math.random() * 200  -- Speed between 200-400 pixels/second
    self.data.velocity = Vector.new(
        math.cos(angle) * speed,
        math.sin(angle) * speed
    )
end

function ENTITY:entity_update(delta_time)
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
    
    -- Bounce off top and bottom walls
    if new_y <= 0 then
        self.data.velocity.y = -self.data.velocity.y
        new_y = 0
    elseif new_y >= viewport_height - ball_size then
        self.data.velocity.y = -self.data.velocity.y
        new_y = viewport_height - ball_size
    end

    -- Update ball position
    self.position = Point.new(new_x, new_y)
end

function ENTITY:entity_collision_enter(entity) 
end

function ENTITY:entity_collision_stay(entity) 
end

function ENTITY:entity_collision_exit(entity) 
end
