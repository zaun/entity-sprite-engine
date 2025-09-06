function ENTITY:set_state_title()
    print("Setting state to title")

    self.data.state = "title"
    self.data.lives = 3
    self.data.level = 1
    self.data.score = 0

    -- Reset everything
    print("Clearing scene")
    scene_clear()

    -- Create the title
    local title_text = EntityComponentText.new("Breakout")
    local entity_title = Entity.new()
    entity_title.components.add(title_text)
    entity_title.position.x = Display.viewport.width / 2
    entity_title.position.y = Display.viewport.height / 2
    entity_title.draw_order = 10000
    entity_title:add_tag("title")
 
    -- Create the title
    local title_text = EntityComponentText.new("Press SPACE to start")
    local entity_title = Entity.new()
    entity_title.components.add(title_text)
    entity_title.position.x = Display.viewport.width / 2
    entity_title.position.y = Display.viewport.height / 4 * 3
    entity_title.draw_order = 10000
    entity_title:add_tag("title")
end

function ENTITY:set_state_countdown()
    print("Setting state to countdown")

    self.data.state = "countdown"
    self.data.countdown = 5
    
    -- Reset everything
    scene_clear()

    -- Create the counter
    self.data.countdown_text = EntityComponentText.new(tostring(self.data.countdown))
    local countdown = Entity.new()
    countdown.components.add(self.data.countdown_text)
    countdown.position.x = Display.viewport.width / 2
    countdown.position.y = Display.viewport.height / 2
    countdown.draw_order = 10000
end

function ENTITY:set_state_play()
    print("Setting state to play")

    self.data.state = "play"
    self.data.bricks_remaining = 0
    
    -- Reset everything
    scene_clear()

    -- Create the UI
    self.data.score_text = EntityComponentText.new("Score: 0")
    self.data.lives_text = EntityComponentText.new("Lives: 3")
    self.data.level_text = EntityComponentText.new("Level: 1")
    local instructions_text = EntityComponentText.new("Press SPACE to launch ball")

    -- Center
    local score_display = Entity.new()
    score_display.components.add(self.data.score_text)
    score_display.position.x = Display.viewport.width / 2 - 20
    score_display.position.y = 10

    -- Top Left
    local lives_display = Entity.new()
    lives_display.components.add(self.data.lives_text)
    lives_display.position.x = 10
    lives_display.position.y = 10

    -- Top Left
    local level_display = Entity.new()
    level_display.components.add(self.data.level_text)
    level_display.position.x = Display.viewport.width - 100
    level_display.position.y = 10

    self.data.instructions_display = Entity.new()
    self.data.instructions_display.components.add(instructions_text)
    self.data.instructions_display.position.x = Display.viewport.width / 2 - 100
    self.data.instructions_display.position.y = Display.viewport.height / 2 + 100

    -- Create the ball
    local ball = Entity.new()
    ball.active = false;
    ball.components.add(EntityComponentLua.new("ball.lua"))
    ball.components.add(EntityComponentSprite.new("breakout:ball blue glass small"))
    ball.components.add(EntityComponentCollider.new(Rect.new(0, 0, 16, 16)))
    ball:add_tag("game")
    ball:add_tag("ball")

    -- Create the paddle
    local paddle = Entity.new()
    paddle.active = false;
    paddle.components.add(EntityComponentLua.new("paddle.lua"))
    paddle.components.add(EntityComponentSprite.new("breakout:paddle bar blue medium"))
    paddle.components.add(EntityComponentCollider.new(Rect.new(0, 0, 128, 28)))
    paddle:add_tag("game")
    paddle:add_tag("paddle")

    -- Create the level
    ENTITY:setup_board()
end

function ENTITY:set_state_level_complete()
    print("Setting state to level complete")

    self.data.state = "level_complete"

    self.data.level = self.data.level + 1
    self.data.score = self.data.score + self.data.lives * 100

    self.data.score = self.data.score + self.data.lives * 100
    self.data.score_text.text = "Score: " .. tostring(self.data.score)
    self.data.lives_text.text = "Lives: " .. tostring(self.data.lives)
    self.data.instructions_text.text = "Level Complete! Press SPACE to continue"
end

function ENTITY:set_state_game_over()
    print("Setting state to game over")

    self.data.state = "game_over"
end

function ENTITY:setup_board()
    -- Brick colors: Blue, Green, Red, Yellow, Orange, Purple
    local colors = {"blue", "green", "red", "yellow", "orange", "purple"}
    local color_sprites = {
        ["blue"] = "breakout:blue small",
        ["green"] = "breakout:green small", 
        ["red"] = "breakout:red small",
        ["yellow"] = "breakout:yellow small",
        ["orange"] = "breakout:orange small",
        ["purple"] = "breakout:purple small"
    }
    
    local brick_width = 64
    local brick_height = 32
    local start_x = 50
    local start_y = 100
    local spacing_x = 5
    local spacing_y = 5
    
    local rows = 0
    local cols = 0
    
    -- Level design
    if self.data.level == 1 then
        rows = 2
        cols = 10
    elseif self.data.level == 2 then
        rows = 4
        cols = 10
    elseif self.data.level <= 4 then
        rows = 6
        cols = 10
    else
        rows = 8
        cols = 10
    end
    
    self.data.bricks_remaining = rows * cols
    
    -- Create bricks
    for row = 0, rows - 1 do
        for col = 0, cols - 1 do
            local brick = Entity.new()
            brick.components.add(EntityComponentLua.new("brick.lua"))
            
            -- Determine color based on level and row
            local color_index = 1
            if self.data.level >= 3 then
                color_index = math.min(6, math.floor(row / 2) + 1)
            end
            
            local color = colors[color_index]
            local sprite_name = color_sprites[color]
            
            brick.components.add(EntityComponentSprite.new(sprite_name))
            brick.components.add(EntityComponentCollider.new(Rect.new(0, 0, brick_width, brick_height)))
            brick:add_tag("brick")
            
            brick.position.x = start_x + col * (brick_width + spacing_x)
            brick.position.y = start_y + row * (brick_height + spacing_y)
        end
    end
    
    -- Reset ball position
    local balls = Entity.find_by_tag("ball")
    if balls and #balls > 0 then
        local ball = balls[1]  -- Get first ball
        ball:dispatch("reset_ball")
        local paddles = Entity.find_by_tag("paddle")
        if paddles and #paddles > 0 and ball.data and ball.data.size then
            local paddle = paddles[1]  -- Get first paddle
            ball.position.x = paddle.position.x + paddle.data.width / 2 - ball.data.size / 2
            ball.position.y = paddle.position.y - ball.data.size - 5
        end
    end
    
    self.data.state = 1
    self.data.instructions_text.text = "Press SPACE to launch ball"
end

--
-- Entity functions
--

function ENTITY:entity_init()
    print("Entity init")
    print(self)
    print(self.data)
    ENTITY:set_state_title()
end

function ENTITY:entity_update(delta_time)
    if self.data.state == "title" then
        if InputState.keys_pressed[InputState.KEY.SPACE] then
            ENTITY:set_state_countdown()
        end
    end
    if self.data.state == "countdown" then
        self.data.countdown = self.data.countdown - delta_time
        self.data.countdown_text.text = tostring(math.ceil(self.data.countdown))
        if self.data.countdown <= 0 then
            ENTITY:set_state_play()
        end
    end
    if self.data.state == "play" then
        -- Remove the instructions on ball launch
        if InputState.keys_pressed[InputState.KEY.SPACE] then
            self.data.instructions_display.active = false
        end
        -- Check if all bricks are destroyed
        if self.data.bricks_remaining <= 0 then
            ENTITY:level_complete()
        end
    end
    if self.data.state == "level_complete" then
        if InputState.keys_pressed[InputState.KEY.SPACE] then
            if self.data.level > 5 then
                ENTITY:set_state_game_over()
            else
                ENTITY:set_state_play()
            end
        end
    end
    if self.data.state == "game_over" then
        if InputState.keys_pressed[InputState.KEY.R] then
            ENTITY:set_state_title()
        end
    end
end


function ENTITY:ball_lost()
    self.data.lives = self.data.lives - 1
    self.data.lives_text.text = "Lives: " .. tostring(self.data.lives)
    
    if self.data.lives <= 0 then
        ENTITY:game_over()
    else
        -- Reset ball position
        local balls = Entity.find_by_tag("ball")
        if balls and #balls > 0 then
            local ball = balls[1]  -- Get first ball
            ball:dispatch("reset_ball")
            local paddles = Entity.find_by_tag("paddle")
            if paddles and #paddles > 0 and ball.data and ball.data.size then
                local paddle = paddles[1]  -- Get first paddle
                ball.position.x = paddle.position.x + paddle.data.width / 2 - ball.data.size / 2
                ball.position.y = paddle.position.y - ball.data.size - 5
            end
        end
        self.data.instructions_text.text = "Press SPACE to launch ball"
    end
end

function ENTITY:brick_destroyed()
    self.data.bricks_remaining = self.data.bricks_remaining - 1
    self.data.score = self.data.score + 10
    self.data.score_text.text = "Score: " .. tostring(self.data.score)
end

function ENTITY:level_complete()
    self.data.level = self.data.level + 1
    self.data.level_text.text = "Level: " .. tostring(self.data.level)
    self.data.instructions_text.text = "Level Complete! Press SPACE to continue"
    
    -- Add bonus points for remaining lives
    self.data.score = self.data.score + self.data.lives * 100
    self.data.score_text.text = "Score: " .. tostring(self.data.score)
    
    -- Wait for space to continue
    self.data.state = 2
end

function ENTITY:game_over()
    self.data.state = 3
    self.data.instructions_text.text = "Game Over! Press R to restart"
end

function ENTITY:restart_game()
    self.data.level = 1
    self.data.score = 0
    self.data.lives = 3
    self.data.bricks_remaining = 0
    
    -- Update UI
    self.data.score_text.text = "Score: 0"
    self.data.lives_text.text = "Lives: 3"
    self.data.level_text.text = "Level: 1"
    
    -- Reset ball and paddle positions
    local balls = Entity.find_by_tag("ball")
    local paddles = Entity.find_by_tag("paddle")
    
    if paddles and #paddles > 0 then
        local paddle = paddles[1]  -- Get first paddle
        paddle.position.x = Display.viewport.width / 2 - 40
        paddle.position.y = Display.viewport.height - 50
    end
    
    if balls and #balls > 0 then
        local ball = balls[1]  -- Get first ball
        ball:dispatch("reset_ball")
        ball.position.x = Display.viewport.width / 2 - 8
        ball.position.y = Display.viewport.height - 80
    end
    
    ENTITY:setup_board()
end

function ENTITY:entity_collision_enter(entity) 
end

function ENTITY:entity_collision_stay(entity) 
end

function ENTITY:entity_collision_exit(entity) 
end
