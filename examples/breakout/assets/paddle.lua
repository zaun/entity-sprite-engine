
function ENTITY:new_ball()
    self.data.ball_locked = true
    self.position.x = Display.viewport.width / 2 - self.data.width / 2
    self.position.y = Display.viewport.height - self.data.height - 10
end

function ENTITY:set_state_play()
    self.data.is_playing = true
end

function ENTITY:entity_init()
    self.data.speed = 400
    self.data.ball_locked = true
    self.data.is_playing = false
    self.position.x = Display.viewport.width / 2 - self.data.width / 2
    self.position.y = Display.viewport.height - self.data.height - 10
end

function ENTITY:entity_update(delta_time)

    -- If ball is locked, move with paddle
    if self.data.ball_locked then
        local ball = Entity.find_first_by_tag("ball")
        ball.position.x = self.position.x + self.data.width / 2 - ball.data.size / 2
        ball.position.y = self.position.y - ball.data.size - 5
    end

    local move_speed = self.data.speed * delta_time
    
    -- Handle keyboard input
    if InputState.keys_down[InputState.KEY.LEFT] or InputState.keys_down[InputState.KEY.A] then
        self.position.x = self.position.x - move_speed
    end
    
    if InputState.keys_down[InputState.KEY.RIGHT] or InputState.keys_down[InputState.KEY.D] then
        self.position.x = self.position.x + move_speed
    end
    
    -- Keep paddle within screen bounds
    local viewport_width = Display.viewport.width
    if self.position.x < 0 then
        self.position.x = 0
    elseif self.position.x > viewport_width - self.data.width then
        self.position.x = viewport_width - self.data.width
    end

    if not self.data.is_playing then
        return
    end

    -- Launch ball when space is pressed
    if InputState.keys_pressed[InputState.KEY.SPACE] then
        local ball = Entity.find_first_by_tag("ball")
        if ball then
            if ball and not ball.data.launched then
                self.data.ball_locked = false
                ball:dispatch("launch_ball")
            end
        end
    end
end
