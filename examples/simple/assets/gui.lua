function ENTITY:entity_init()
end

function ENTITY:entity_update(delta_time)
    local style = GuiStyle.new()
    GUI.set_style(style);

    GUI.start(65000, 0, 0, Display.viewport.width, 35)
        style.background = Color.new(0, 1, 0, 1)
        GUI.open_flex()
            -- Set the style for the box container
            style.background = Color.new(1, 0, 0, 1)
            GUI.open_stack(100, GUI.STYLE.AUTO_SIZE)
                GUI.push_button("Click me", function()
                    print("Button clicked")
                end)
            GUI.close_stack()
        GUI.close_flex()
    GUI.finish()
end
