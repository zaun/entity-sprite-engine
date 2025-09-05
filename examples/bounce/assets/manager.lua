function ENTITY:entity_init()
    self.data.ups = EntityComponentText.new("Updates Per Second: ?")
    local ups_display = Entity.new()
    ups_display.components.add(self.data.ups)
    ups_display.position.x = 10
    ups_display.position.y = 10
    
    -- Create initial balls
    for i = 1, 400 do
        ENTITY:add_ball()
    end
    
    -- Initialize counter for adding more balls
    self.data.ball_counter = 0
    self.draw_order = 10000

end

function ENTITY:entity_update(delta_time)
    local space = InputState.keys_pressed[InputState.KEY.SPACE]

    if space then
        for i = 1, 100 do
            ENTITY:add_ball()
        end
    end

    self.data.ups.text = "Updates Per Second: " .. string.format("%07.2f", 1 / delta_time) .. " | Entities: " .. tostring(Entity.count())
end

function ENTITY:entity_collision_enter(entity) 
end

function ENTITY:entity_collision_stay(entity) 
end

function ENTITY:entity_collision_exit(entity) 
end

function ENTITY:add_ball()
    local colors = {
        "blue glass", "chrome glass",
        "green glossy", "orange glossy", "red glossy", "yellow glossy",
        "blue shiny", "green shiny", "orange shiny", "red shiny", "yellow shiny"
    }
    local color = colors[math.random(#colors)]

    print("Adding ball");
    local ball = Entity.new()
    ball.components.add(EntityComponentLua.new("ball.lua"))
    ball.components.add(EntityComponentSprite.new("bounce:ball " .. color .. " small"))
    ball.components.add(EntityComponentCollider.new(Rect.new(0, 0, 16, 16)))
    ball.data.size = 16
    ball.data.world_start = 0
    ball.data.world_size = Display.viewport.width
    ball.position.x = math.random(ball.data.world_start, ball.data.world_size - ball.data.size)
    ball.position.y = math.random(ball.data.world_start, ball.data.world_size - ball.data.size)
end