-- Pure movement controller. Direction stored as a numeric "direction" value.
-- For directions == 8 or 4: direction is sector index in range [1, directions].
-- angle and last_angle hold the raw angle in degrees if present.

function ENTITY:entity_init()
    -- Configurable defaults
    self.data.directions = 8 -- 4 or 8
    self.data.base_speed = 75.0
    self.data.run_multiplier = 2.0
    self.data.acceleration = 600.0
    self.data.deceleration = 800.0
    self.data.max_speed = nil
    self.data.diagonal_normalize = true

    -- current state
    self.data.direction = 1 -- numeric direction (1 is north)
    self.data.last_direction = 1
    self.data.angle = nil -- raw angle degrees
    self.data.last_angle = nil
    self.data.moving = false
    self.data.running = false
    self.data.velocity = { x = 0, y = 0 }

    -- key bindings support multiple keys per action
    self.data.bind = {
        left = { InputState.KEY.A, InputState.KEY.LEFT },
        right = { InputState.KEY.D, InputState.KEY.RIGHT },
        up = { InputState.KEY.W, InputState.KEY.UP },
        down = { InputState.KEY.S, InputState.KEY.DOWN },
        run = { InputState.KEY.LSHIFT, InputState.KEY.RSHIFT }
    }
end

local function any_key_down(keys)
    if not keys then return false end
    for _, k in ipairs(keys) do
        if InputState.keys_down[k] then
            return true
        end
    end
    return false
end

local function vector_to_angle_and_direction(cfg, vx, vy)
    if vx == 0 and vy == 0 then
        return cfg.last_angle, cfg.last_direction
    end

    local angle = math.deg(math.atan2(-vy, vx)) -- 0 = east, 90 = north
    if angle < 0 then angle = angle + 360 end

    -- Quantize to 4 or 8 directional sectors (default to 4 if not 8)
    local sectors = (cfg.directions == 8) and 8 or 4
    local sector_width = 360 / sectors
    local bearing = (90 - angle) % 360 -- 0 = North, increases clockwise
    local idx = math.floor((bearing + sector_width / 2) / sector_width) % sectors + 1
    return angle, idx
end

function ENTITY:entity_update(delta_time)
    if delta_time <= 0 then
        return
    end

    local cfg = self.data

    local right = any_key_down(cfg.bind.right)
    local left = any_key_down(cfg.bind.left)
    local up = any_key_down(cfg.bind.up)
    local down = any_key_down(cfg.bind.down)
    local running = any_key_down(cfg.bind.run)

    local ix = 0
    local iy = 0
    if right then ix = ix + 1 end
    if left then ix = ix - 1 end
    if down then iy = iy + 1 end
    if up then iy = iy - 1 end

    -- Enforce allowed movement directions (4 or 8 only)
    if cfg.directions == 4 then
        if ix ~= 0 and iy ~= 0 then
            -- Keep movement on the last cardinal axis (1/3 = N/S, 2/4 = E/W)
            if cfg.last_direction == 1 or cfg.last_direction == 3 then
                ix = 0 -- keep vertical
            else
                iy = 0 -- keep horizontal
            end
        end
    end

    local input_len = math.sqrt(ix * ix + iy * iy)
    if input_len > 0 and cfg.diagonal_normalize then
        ix = ix / input_len
        iy = iy / input_len
    end

    local target_speed = cfg.base_speed
    if cfg.max_speed then
        target_speed = cfg.max_speed
    end
    if running and input_len > 0 then
        target_speed = target_speed * cfg.run_multiplier
    end

    local tvx = ix * target_speed
    local tvy = iy * target_speed

    local vx = cfg.velocity.x
    local vy = cfg.velocity.y

    local ax = 0
    local ay = 0
    if input_len > 0 then
        ax = cfg.acceleration
        ay = cfg.acceleration
    else
        ax = cfg.deceleration
        ay = cfg.deceleration
    end

    local function approach(current, target, accel, dt)
        local diff = target - current
        if diff == 0 then return target end
        local change = accel * dt
        if math.abs(diff) <= change then
            return target
        end
        return current + (diff > 0 and change or -change)
    end

    vx = approach(vx, tvx, ax, delta_time)
    vy = approach(vy, tvy, ay, delta_time)

    cfg.velocity.x = vx
    cfg.velocity.y = vy

    local move_x = vx * delta_time
    local move_y = vy * delta_time

    self.position.x = self.position.x + move_x
    self.position.y = self.position.y + move_y

    if self.position.x < 0 then self.position.x = 0 end
    if self.position.y < 0 then self.position.y = 0 end

    local angle, dir = vector_to_angle_and_direction(cfg, vx, vy)
    cfg.last_angle = cfg.angle
    cfg.angle = angle
    cfg.last_direction = cfg.direction
    cfg.direction = dir
    cfg.moving = (vx * vx + vy * vy) > 1e-6
    cfg.running = running
end

function ENTITY:entity_collision_enter(entity)
end

function ENTITY:entity_collision_stay(entity)
end

function ENTITY:entity_collision_exit(entity)
end
