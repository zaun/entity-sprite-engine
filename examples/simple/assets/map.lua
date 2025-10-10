function ENTITY:map_init()
end

function ENTITY:map_update(delta_time, map)

end

function ENTITY:map_collision_enter(cell)
    print("map map_collision_enter", tostring(cell))
end

function ENTITY:map_collision_stay(cell)
    print("map map_collision_stay", tostring(cell))
end

function ENTITY:map_collision_exit(cell)
    print("map map_collision_exit", tostring(cell))
end