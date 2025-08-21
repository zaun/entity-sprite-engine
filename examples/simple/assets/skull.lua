function ENTITY:move()

    local new_x = math.random(50, Display.viewport.width - 50)
    local new_y = math.random(50, Display.viewport.height - 50)

    local r = Rect.new(new_x, new_y, 200, 200);
    local entities = detect_collision(r, 1);
    if (#entities == 0) then
        self.position.x = new_x
        self.position.y = new_y
    else
        ENTITY:move();
    end
end
