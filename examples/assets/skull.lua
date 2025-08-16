local M = {}

function M:move()
    self.position.x = math.random(50, Display.viewport.width - 50)
    self.position.y = math.random(50, Display.viewport.height - 50)
end

return M
