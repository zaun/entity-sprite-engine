function ENTITY:entity_init()
    -- Initialize data table
    -- self.data = {}
    
    -- State 0: Startup
    -- State 1: Playing
    -- State 2: Round Over
    -- State 3: Game Over
    self.data.state = 0
    self.data.level = 1
    self.data.score = 0
    self.data.lives = 3
    self.data.bricks_remaining = 0
    
    -- Create UI text components
    self.data.score_text = EntityComponentText.new("Score: 0")
    self.data.lives_text = EntityComponentText.new("Lives: 3")
    self.data.level_text = EntityComponentText.new("Level: 1")
    self.data.instructions_text = EntityComponentText.new("Press SPACE to launch ball")
    
    local score_display = Entity.new()
    score_display.components.add(self.data.score_text)
    score_display.position.x = 10
    score_display.position.y = 10
    
    local lives_display = Entity.new()
    lives_display.components.add(self.data.lives_text)
    lives_display.position.x = 10
    lives_display.position.y = 30
    
    local level_display = Entity.new()
    level_display.components.add(self.data.level_text)
    level_display.position.x = 10
    level_display.position.y = 50
    
    local instructions_display = Entity.new()
    instructions_display.components.add(self.data.instructions_text)
    instructions_display.position.x = Display.viewport.width / 2 - 100
    instructions_display.position.y = Display.viewport.height / 2 + 100
    
    self.draw_order = 10000
    
    -- Call setup_board directly in entity_init
    ENTITY:setup_board()
end

function ENTITY:setup_board()
    -- Clear existing bricks by finding them by tag
    local bricks = Entity.find_by_tag("brick")
    for i = 1, #bricks do
        bricks[i]:destroy()
    end
    
    -- Brick colors: Blue, Green, Red, Yellow, Orange, Purple
    local colors = {"blue", "green", "red", "yellow", "orange", "purple"}
    local color_sprites = {
        ["blue"] = "breakout:blue large",
        ["green"] = "breakout:green large", 
        ["red"] = "breakout:red large",
        ["yellow"] = "breakout:yellow large",
        ["orange"] = "breakout:orange large",
        ["purple"] = "breakout:purple large"
    }
    
    local brick_width = 64
    local brick_height = 24
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
            if paddles and #paddles > 0 then
                local paddle = paddles[1]  -- Get first paddle
                ball.position.x = paddle.position.x + paddle.data.width / 2 - ball.data.size / 2
                ball.position.y = paddle.position.y - ball.data.size - 5
            end
        end
    
    self.data.state = 1
    self.data.instructions_text.text = "Press SPACE to launch ball"
end

function ENTITY:entity_update(delta_time)
    if self.data.state == 1 then
        -- Check if all bricks are destroyed
        if self.data.bricks_remaining <= 0 then
            ENTITY:level_complete()
        end
    elseif self.data.state == 2 then
        -- Level complete state - wait for space to continue
        if InputState.keys_pressed[InputState.KEY.SPACE] then
            ENTITY:setup_board()
        end
    elseif self.data.state == 3 then
        -- Game over state - wait for R to restart
        if InputState.keys_pressed[InputState.KEY.R] then
            ENTITY:restart_game()
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
            if paddles and #paddles > 0 then
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
