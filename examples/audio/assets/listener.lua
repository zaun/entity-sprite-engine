function ENTITY:entity_init()
    -- Mode -1 = Nothing
    -- Mode  0 = Orbit in Circle
    -- Mode  1 = User Dragable
    self.data.mode = -1

    -- Drag state (used in mode 1)
    self.data.dragging = false
    self.data.drag_offset_x = 0
    self.data.drag_offset_y = 0
    -- Visual circle radius is ~10 units (see startup.lua shape path)
    self.data.drag_radius = 10

    -- init
    ENTITY:set_mode()

    -- subscribe
    self:subscribe("TOGGLE_MODE", "set_mode")

    -- Verify listener component
    if self.data.listener == nil then
        print("Missing listener")
    end

    -- Configure listener component
    self.data.listener.spatial = true
    self.data.listener.volume = 100
    self.data.listener.max_distance = 1000
    self.data.listener.attenuation = 1.0
    self.data.listener.rolloff = 1.0
end

function ENTITY:entity_update(delta_time)
    if self.data.mode == 0 then
        -- Advance orbit phase
        self.data.angle = self.data.angle + self.data.orbit_speed * delta_time

        -- Determine the center of the orbit: prefer the provided center entity,
        -- otherwise fall back to the center of the viewport.
        local cx
        local cy
        if self.data.orbit_center ~= nil and self.data.orbit_center.position ~= nil then
            cx = self.data.orbit_center.position.x
            cy = self.data.orbit_center.position.y
        else
            cx = Display.viewport.width / 2
            cy = Display.viewport.height / 2
        end

        local r = self.data.orbit_radius

        self.position.x = cx + math.cos(self.data.angle) * r
        self.position.y = cy + math.sin(self.data.angle) * r
    elseif self.data.mode == 1 then
        -- User dragable: left mouse button drags the listener around the screen.
        -- Drag only starts when the click begins over the visual circle.
        local mx = InputState.mouse_x
        local my = InputState.mouse_y
        local left_clicked = InputState.mouse_clicked[0]
        local left_down = InputState.mouse_down[0]

        if left_clicked then
            -- Check if the click started inside the circle
            local dx = mx - self.position.x
            local dy = my - self.position.y
            local r = self.data.drag_radius or 10
            if (dx * dx + dy * dy) <= (r * r) then
                -- Begin drag, remember offset from cursor to entity position
                self.data.dragging = true
                self.data.drag_offset_x = self.position.x - mx
                self.data.drag_offset_y = self.position.y - my
            else
                -- Click started outside the circle; do not drag
                self.data.dragging = false
            end
        end

        if self.data.dragging and left_down then
            -- While dragging, follow the cursor with the stored offset
            self.position.x = mx + self.data.drag_offset_x
            self.position.y = my + self.data.drag_offset_y
        end

        if not left_down then
            -- Mouse released, stop dragging
            self.data.dragging = false
        end
    end
end

function ENTITY:set_mode()
    -- Udpate the mode
    self.data.mode = self.data.mode + 1
    if self.data.mode > 1 then
        self.data.mode = 0
    end

    -- Setup the mode
    if self.data.mode == 0 then
        self.data.mode = 0
        self.data.angle = 0
        self.data.orbit_radius = 100
        self.data.orbit_speed = math.pi * 0.5
    elseif self.data.mode == 1 then
        -- Reset drag state when entering drag mode
        self.data.dragging = false
        self.data.drag_offset_x = 0
        self.data.drag_offset_y = 0
    end
end
