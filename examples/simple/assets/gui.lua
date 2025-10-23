function ENTITY:entity_init()
    print("GUI entity initialized")
end

function ENTITY:button_clicked()
    print("Button clicked")
end

-- The GUI is an immediate mode GUI, so we need to track state ourselved.
-- Each GUI begins with a GUI.start() call.
--   GUI.start(draw_order, x, y, width, height)
--   x, y, width, height are the position and size of the GUI on screen coordinates.

-- GUI Containers:
--   Flex containers are used to layout widgets in a row or column.
--     GUI.open_flex(direction, justify, align_items[, width, height, style])
--     direction, justify, align_items are required arguments.
--     width, height are optional arguments and if provided,
--     will set the width and height of the flex container.
--     style is an optional argument that if provided, will be used to style the flex container.
--   Stack containers are used to layout widgets in a stack.
--     GUI.open_stack([width, height, style])
--     width, height are optional arguments and if provided,
--     will set the width and height of the stack container.
--     style is an optional argument that if provided, will be used to style the stack container.

-- GuiStyle is a themeable style object, it has a set of properties that can be styledfor each
-- variant. The variants are:
--   PRIMARY, SECONDARY, SUCCESS, INFO, WARNING, DANGER, LIGHT, DARK, WHITE, TRANSPARENT
--
-- GUI.push_variant(variant) is used to push a variant onto the variant stack.
-- GUI.pop_variant() is used to pop a variant off the variant stack.
-- The top of the variant stack is what is used to style the following widgets.

function ENTITY:entity_update(delta_time)
    local style = GuiStyle.new()
    style.background[GuiStyle.VARIANT.PRIMARY] = Color.new(0, 1, 0, 1)

    GUI.start(9, 0, 0, Display.viewport.width, 56)
        GUI.open_flex(
            GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.START, GUI.STYLE.ALIGN.START,
            GUI.STYLE.AUTO_SIZE, GUI.STYLE.AUTO_SIZE, style
        )
            GUI.open_stack(150, GUI.STYLE.AUTO_SIZE)
                GUI.push_button("Default", ENTITY.button_clicked)
            GUI.close_stack()

            GUI.push_variant(GuiStyle.VARIANT.SECONDARY)
            GUI.open_stack(150, GUI.STYLE.AUTO_SIZE)
                GUI.push_button("Secondary", ENTITY.button_clicked)
            GUI.close_stack()
            GUI.pop_variant()

            GUI.push_variant(GuiStyle.VARIANT.WARNING)
            GUI.open_stack(150, GUI.STYLE.AUTO_SIZE)
                GUI.push_button("Warning", ENTITY.button_clicked)
            GUI.close_stack()
            GUI.pop_variant()
        GUI.close_flex()
    GUI.finish()

    GUI.start(9, 50, 50, 100, 100)
        GUI.open_flex(
            GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.START, GUI.STYLE.ALIGN.START,
            GUI.STYLE.AUTO_SIZE, GUI.STYLE.AUTO_SIZE, style
        )
            GUI.push_image("game:horse stopped north")
        GUI.close_flex()
    GUI.finish()
end
