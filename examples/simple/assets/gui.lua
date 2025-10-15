function ENTITY:entity_init()
end

function ENTITY:entity_update(delta_time)
    GUI.start(65000, 0, 0, Display.viewport.width, 35)
        GUI.open_flex({
            direction = GUI.DIRECTION.COLUMN,
            justify = GUI.JUSTIFY.START,
            align_items = GUI.ALIGN.START,
            spacing = 0,
            padding_left = 0,
            padding_top = 0,
            padding_right = 0,
            padding_bottom = 0,
            background_color = Color.new(0, 1, 0, 1),
        })
        GUI.close_flex()
    GUI.finish()
end
