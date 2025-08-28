function ENTITY:entity_init()
    -- State 0: Startup
    -- State 1: Playing
    -- State 2: Round Over
    self.data.state = 0
    self.data.level = 1

    self:setup_board()
end

function ENTITY:setup_board()
    -- Brick colors:Blue, Green, Red, Yellow, Orange, Purple

    -- Level 1: large bricks 2 lines, all blue
    -- Level 2: large bricks 4 lines, all blue
    -- Level 3: large bricks 8 lines, blue and green
    -- Level 4: large bricks 8 lines, blue, green and red
    -- Level 5: large bricks 8 lines, blue, green, red, yellow and orange
    -- Level 6: large bricks 8 lines, blue, green, red, yellow and orange
    -- Level 7: large bricks 10 lines, blue, green, red, yellow, orange and purple
    -- Level 8: large bricks 10 lines, blue, green, red, yellow, orange and purple
    
    
    self.data.state = 1

    return true
end
