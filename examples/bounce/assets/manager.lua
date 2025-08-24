function ENTITY:entity_init()
    ENTITY:add_ball()
end

function ENTITY:entity_update(delta_time)
end

function ENTITY:entity_collision_enter(entity) 
end

function ENTITY:entity_collision_stay(entity) 
end

function ENTITY:entity_collision_exit(entity) 
end

function ENTITY:add_ball()
    print("Adding ball");
    local ball = Entity.new()
    ball.components.add(EntityComponentLua.new("ball.lua"))
    ball.components.add(EntityComponentSprite.new("bounce:ball blue glass small"))
    ball.components.add(EntityComponentCollider.new(Rect.new(0, 0, 16, 16)))
    ball.position.x = Display.viewport.width / 2
    ball.position.y = Display.viewport.height / 2
    ball.data.size = 16
end