function ENTITY:map_init()
end

function ENTITY:map_update(delta_time, map)
    print(delta_time .. '  ' .. tostring(map))
end
