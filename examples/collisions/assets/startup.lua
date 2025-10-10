function STARTUP:makeTarget(name, x, y, off_x, off_y, w, h, rot)
    local target = Entity.new()
    target.draw_order = 3
    target.data.name = name
    target:add_tag("target")

    if asset_load_script("target.lua") then
        local lua_comp = EntityComponentLua.new()
        lua_comp.script = "target.lua"
        target.components.add(lua_comp)
    end

    local target_shape = EntityComponentShape.new()
    target_shape:set_path('M 10 0 A 10 10 0 0 1 -10 0 A 10 10 0 0 1 10 0 Z', {
		stroke_width = 1.0,
		stroke_color = Color.new(0, 1, 0),
		fill_color = Color.new(0, 0.5, 0.7, 0.5)
	})
    target.components.add(target_shape)

    local target_collider = EntityComponentCollider.new()
    target_collider.draw_debug = true
    local r = Rect.new(off_x, off_y, w, h)
    r.rotation = rot
    target_collider.rects.add(r)
    target.components.add(target_collider)

    target.position.x = x
    target.position.y = y

    print("Created target " .. name .. " at " .. x .. "," .. y .. " with rect x: " .. off_x .. " y: " .. off_y .. " w: " .. w .. " h: " .. h .. " r: " .. rot .. "deg")

    return target
end

function STARTUP:makeObject(name, x, y, off_x, off_y, w, h, rot, velocity)
    local object = Entity.new()
    object.draw_order = 3
    object.data.name = name
    object.data.velocity = velocity

    if asset_load_script("object.lua") then
        local lua_comp = EntityComponentLua.new()
        lua_comp.script = "object.lua"
        object.components.add(lua_comp)
    end

    local object_shape = EntityComponentShape.new()
    object_shape:set_path('M 10 0 A 10 10 0 0 1 -10 0 A 10 10 0 0 1 10 0 Z', {
		stroke_width = 1.0,
		stroke_color = Color.new(0, 1, 0),
		fill_color = Color.new(0, 1, 0, 0.5)
	})
    object.components.add(object_shape)

    local object_collider = EntityComponentCollider.new()
    object_collider.draw_debug = true
    local r = Rect.new(off_x, off_y, w, h)
    r.rotation = rot
    object_collider.rects.add(r)
    object.components.add(object_collider)

    object.position.x = x
    object.position.y = y

    print("Created object " .. name .. " at " .. x .. "," .. y .. " with rect x: " .. off_x .. " y: " .. off_y .. " w: " .. w .. " h: " .. h .. " r: " .. rot .. "deg")

    return object
end

function STARTUP:startup()
    STARTUP:makeTarget('Target A', 150, 150, -10, -10, 20, 20, 0)

    STARTUP:makeObject('Object A', 150, 100, -10, -10, 20, 20, 0, Vector.new(0, 125))
    STARTUP:makeObject('Object C', 150, 200, -10, -10, 20, 20, 0, Vector.new(0, -125))
    STARTUP:makeObject('Object B', 100, 150, -10, -10, 20, 20, 0, Vector.new(125, 0))
    STARTUP:makeObject('Object D', 200, 150, -10, -10, 20, 20, 0, Vector.new(-125, 0))
    STARTUP:makeObject('Object E', 100, 100, -10, -10, 20, 20, 45, Vector.new(125, 125))
    STARTUP:makeObject('Object F', 200, 200, -10, -10, 20, 20, 135, Vector.new(-125, -125))
    STARTUP:makeObject('Object G', 100, 200, -10, -10, 20, 20, 225, Vector.new(125, -125))
    STARTUP:makeObject('Object H', 200, 100, -10, -10, 20, 20, 315, Vector.new(-125, 125))

    Camera.position.x = Display.viewport.width / 2
    Camera.position.y = Display.viewport.height / 2
end
