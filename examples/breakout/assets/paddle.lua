function ENTITY:entity_init()
    -- Initialize data table
    self.data = {}
    
    self.data.type = "paddle"
    self.data.width = 80
    self.data.height = 16
    self.data.speed = 400
    self.data.size = 16  -- For collision detection
end

function ENTITY:entity_update(delta_time)
    local move_speed = self.data.speed * delta_time
    
    -- Handle keyboard input
    if InputState.keys_pressed[InputState.KEY.LEFT] or InputState.keys_pressed[InputState.KEY.A] then
        self.position.x = self.position.x - move_speed
    end
    
    if InputState.keys_pressed[InputState.KEY.RIGHT] or InputState.keys_pressed[InputState.KEY.D] then
        self.position.x = self.position.x + move_speed
    end
    
    -- Keep paddle within screen bounds
    local viewport_width = Display.viewport.width
    if self.position.x < 0 then
        self.position.x = 0
    elseif self.position.x > viewport_width - self.data.width then
        self.position.x = viewport_width - self.data.width
    end
    
    -- Launch ball when space is pressed
    if InputState.keys_pressed[InputState.KEY.SPACE] then
        local balls = Entity.find_by_tag("ball")
        if balls and #balls > 0 then
            local ball = balls[1]  -- Get first ball
            if ball and not ball.data.launched then
                ball:dispatch("launch_ball")
            end
        end
    end
end

function ENTITY:entity_collision_enter(entity) 
end

function ENTITY:entity_collision_stay(entity) 
end

function ENTITY:entity_collision_exit(entity) 
end
