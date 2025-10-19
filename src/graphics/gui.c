#include <stdlib.h>
#include <string.h>
#include "core/memory_manager.h"
#include "graphics/gui_lua.h"
#include "graphics/gui_private.h"
#include "graphics/gui.h"
#include "types/color.h"
#include "types/gui_style.h"
#include "types/input_state.h"
#include "utility/log.h"
#include "scripting/lua_engine.h"

// Note: memory_manager will never return a NULL pointer, so we don't need to check for NULL

EseGui *ese_gui_create(EseLuaEngine *engine) {
    log_assert("GUI", engine, "ese_gui_create called with NULL engine");

    EseGui *gui = (EseGui *)memory_manager.malloc(sizeof(EseGui), MMTAG_GUI);

    // Initialize frame layout stack
    gui->layouts_capacity = 16; // Support up to 16 nested frames
    gui->layouts = (EseGuiLayout *)memory_manager.malloc(
        sizeof(EseGuiLayout) * gui->layouts_capacity, MMTAG_GUI);
    gui->layouts_count = 0;
    gui->open_layout = NULL;

    // Initialize state
    gui->input_state = NULL;
    gui->draw_iterator = 0;
    gui->iterator_started = false;

    gui->engine = engine;

    // Initialize current style with default values
    gui->current_style = ese_gui_style_create(engine);
    ese_gui_style_ref(gui->current_style);

    return gui;
}

void ese_gui_destroy(EseGui *gui) {
    if (gui == NULL) return;

    // Free frame layout stack and their layout stacks
    if (gui->layouts != NULL) {
        for (size_t i = 0; i < gui->layouts_count; i++) {
            EseGuiLayout *layout = &gui->layouts[i];
            _ese_gui_layout_destroy(layout);
        }
        memory_manager.free(gui->layouts);
    }

    // Free input state if it exists
    if (gui->input_state != NULL) {
        ese_input_state_destroy(gui->input_state);
    }

    // Free current style
    ese_gui_style_unref(gui->current_style);
    ese_gui_style_destroy(gui->current_style);

    memory_manager.free(gui);
}

void ese_gui_input(EseGui *gui, EseInputState *input_state) {
    log_assert("GUI", gui, "ese_gui_input called with NULL gui");
    log_assert("GUI", input_state, "ese_gui_input called with NULL input_state");

    // Store input state for processing during ese_gui_process
    if (gui->input_state != NULL) {
        ese_input_state_destroy(gui->input_state);
    }
    gui->input_state = ese_input_state_copy(input_state);
}

void ese_gui_process(EseGui *gui, EseDrawList *draw_list) {
    log_assert("GUI", gui, "ese_gui_process called with NULL gui");
    log_assert("GUI", draw_list, "ese_gui_process called with NULL draw_list");

    // Step 1: Calculate layout positions and sizes for all widgets (recursive over tree)
    //         and process input for all widgets
    for (size_t frame_idx = 0; frame_idx < gui->layouts_count; frame_idx++) {
        EseGuiLayout *layout = &gui->layouts[frame_idx];
        if (layout->root != NULL) {
            _ese_gui_calculate_node_position(gui, layout, layout->root, 0);
        }
    }

    // Step 2: Generate draw commands for all widgets
    for (size_t frame_idx = 0; frame_idx < gui->layouts_count; frame_idx++) {
        EseGuiLayout *layout = &gui->layouts[frame_idx];
        _ese_gui_generate_draw_commands(gui, draw_list, layout, layout->root, 0);
    }
}

void ese_gui_cleanup(EseGui *gui) {
    // Remove current layouts
    for (size_t i = 0; i < gui->layouts_count; i++) {
        EseGuiLayout *layout = &gui->layouts[i];
        _ese_gui_layout_destroy(layout);
    }
    gui->layouts_count = 0;
    gui->open_layout = NULL;

    // Mark that we've finished processing this frame
    gui->iterator_started = false;
}

void ese_gui_begin(EseGui *gui, uint64_t z_index, int x, int y, int width, int height) {
    log_assert("GUI", gui, "ese_gui_begin called with NULL gui");
    log_assert("GUI", gui->open_layout == NULL, "ese_gui_begin called while another frame is active");

    // Check if we need to grow the frame stack
    if (gui->layouts_count >= gui->layouts_capacity) {
        // For now, just log an error - in a real implementation we'd grow the array
        log_error("GUI", "ese_gui_begin called with no capacity to grow frame stack");
        return;
    }

    // Create new frame layout
    EseGuiLayout *layout = &gui->layouts[gui->layouts_count++];
    layout->z_index = z_index;
    layout->x = x;
    layout->y = y;
    layout->width = width;
    layout->height = height;

    // Initialize draw scissor state
    layout->draw_scissors_active = false;
    layout->draw_scissors_x = 0.0f;
    layout->draw_scissors_y = 0.0f;
    layout->draw_scissors_w = 0.0f;
    layout->draw_scissors_h = 0.0f;

    // Initialize layout stack for this frame layout
    layout->root = NULL;

    // Initialize container stack for building hierarchy
    layout->current_container = NULL;

    // currently open frame layout
    gui->open_layout = layout;
}

void ese_gui_end(EseGui *gui) {
    log_assert("GUI", gui, "ese_gui_end called with NULL gui");
    log_assert("GUI", gui->open_layout != NULL, "ese_gui_end called with no open frame layout");

    gui->open_layout = NULL;
}

void ese_gui_open_flex(EseGui *gui, int width, int height) {
    log_assert("GUI", gui, "ese_gui_open_flex called with NULL gui");
    log_assert("GUI", gui->open_layout != NULL, "ese_gui_open_flex called with no open frame layout");
    log_assert("GUI", gui->current_style != NULL, "ese_gui_open_flex called with NULL current_style");

    EseGuiLayout *layout = gui->open_layout;
    EseGuiStyle *style = gui->current_style;

    EseGuiLayoutNode *node = (EseGuiLayoutNode *)memory_manager.calloc(1, sizeof(EseGuiLayoutNode), MMTAG_GUI);

    node->x = 0;
    node->y = 0;
    node->width = width;
    node->height = height;
    node->is_hovered = false;
    node->is_down = false;
    _ese_gui_copy_colors_from_style(node, style);
    node->widget_type = ESE_GUI_WIDGET_FLEX;

    node->widget_data.container.direction = ese_gui_style_get_direction(style);
    node->widget_data.container.justify = ese_gui_style_get_justify(style);
    node->widget_data.container.align_items = ese_gui_style_get_align_items(style);
    node->widget_data.container.spacing = ese_gui_style_get_spacing(style);
    node->widget_data.container.padding_left = ese_gui_style_get_padding_left(style);
    node->widget_data.container.padding_top = ese_gui_style_get_padding_top(style);
    node->widget_data.container.padding_right = ese_gui_style_get_padding_right(style);
    node->widget_data.container.padding_bottom = ese_gui_style_get_padding_bottom(style);
    node->children = NULL;
    node->children_count = 0;
    node->children_capacity = 0;

    // Attach to current container if any, otherwise become root
    node->parent = NULL;
    if (layout->current_container != NULL) {
        EseGuiLayoutNode *parent = layout->current_container;
        node->parent = parent;
        if (parent->children_count >= parent->children_capacity) {
            size_t new_cap = parent->children_capacity == 0 ? 4 : parent->children_capacity * 2;
            parent->children = (EseGuiLayoutNode **)memory_manager.realloc(parent->children, sizeof(EseGuiLayoutNode*) * new_cap, MMTAG_GUI);
            parent->children_capacity = new_cap;
        }
        parent->children[parent->children_count++] = node;
    } else {
        layout->root = node;
    }

    // Push this container onto the container stack
    layout->current_container = node;
}

void ese_gui_close_flex(EseGui *gui) {
    log_assert("GUI", gui, "ese_gui_close_flex called with NULL gui");
    log_assert("GUI", gui->open_layout != NULL, "ese_gui_close_flex called with no open frame layout");

    EseGuiLayout *layout = gui->open_layout;

    // Pop the last opened container
    log_assert("GUI", layout->current_container != NULL, "ese_gui_close_flex called with no open containers");
    log_assert("GUI", layout->current_container->widget_type == ESE_GUI_WIDGET_FLEX, "ese_gui_close_flex called with no open FLEX containers");
    layout->current_container = layout->current_container->parent;
}

void ese_gui_open_stack(EseGui *gui, int width, int height) {
    log_assert("GUI", gui, "ese_gui_open_stack called with NULL gui");
    log_assert("GUI", gui->open_layout != NULL, "ese_gui_open_stack called with no open frame layout");
    log_assert("GUI", gui->current_style != NULL, "ese_gui_open_stack called with NULL current_style");

    EseGuiLayout *layout = gui->open_layout;
    EseGuiStyle *style = gui->current_style;

    EseGuiLayoutNode *node = (EseGuiLayoutNode *)memory_manager.calloc(1, sizeof(EseGuiLayoutNode), MMTAG_GUI);

    node->x = 0;
    node->y = 0;
    node->width = width;
    node->height = height;
    node->is_hovered = false;
    node->is_down = false;
    _ese_gui_copy_colors_from_style(node, style);
    node->widget_type = ESE_GUI_WIDGET_STACK;

    node->widget_data.container.direction = ese_gui_style_get_direction(style);
    node->widget_data.container.justify = ese_gui_style_get_justify(style);
    node->widget_data.container.align_items = ese_gui_style_get_align_items(style);
    node->widget_data.container.spacing = ese_gui_style_get_spacing(style);
    node->widget_data.container.padding_left = ese_gui_style_get_padding_left(style);
    node->widget_data.container.padding_top = ese_gui_style_get_padding_top(style);
    node->widget_data.container.padding_right = ese_gui_style_get_padding_right(style);
    node->widget_data.container.padding_bottom = ese_gui_style_get_padding_bottom(style);
    node->children = NULL;
    node->children_count = 0;
    node->children_capacity = 0;

    // Attach to current container if any, otherwise become root
    node->parent = NULL;
    if (layout->current_container != NULL) {
        EseGuiLayoutNode *parent = layout->current_container;
        node->parent = parent;
        if (parent->children_count >= parent->children_capacity) {
            size_t new_cap = parent->children_capacity == 0 ? 4 : parent->children_capacity * 2;
            parent->children = (EseGuiLayoutNode **)memory_manager.realloc(parent->children, sizeof(EseGuiLayoutNode*) * new_cap, MMTAG_GUI);
            parent->children_capacity = new_cap;
        }
        parent->children[parent->children_count++] = node;
    } else {
        // We are the root
        layout->root = node;
    }

    // Push this container onto the container stack
    layout->current_container = node;
}

void ese_gui_close_stack(EseGui *gui) {
    log_assert("GUI", gui, "ese_gui_close_stack called with NULL gui");
    log_assert("GUI", gui->open_layout != NULL, "ese_gui_close_stack called with no open frame layout");

    EseGuiLayout *layout = gui->open_layout;

    // Only close a STACK container
    log_assert("GUI", layout->current_container != NULL, "ese_gui_close_stack called with no open containers");
    log_assert("GUI", layout->current_container->widget_type == ESE_GUI_WIDGET_STACK, "ese_gui_close_stack called with no open STACK containers");
    layout->current_container = layout->current_container->parent;
}

void ese_gui_push_button(EseGui *gui, const char* text, void (*callback)(void *userdata), void *userdata) {
    log_assert("GUI", gui, "ese_gui_push_button called with NULL gui");
    log_assert("GUI", gui->open_layout != NULL, "ese_gui_push_button called with no open frame layout");
    log_assert("GUI", text != NULL, "ese_gui_push_button called with NULL text");
    log_assert("GUI", callback != NULL, "ese_gui_push_button called with NULL callback");

    EseGuiLayout *layout = gui->open_layout;

    EseGuiLayoutNode *button_node = (EseGuiLayoutNode *)memory_manager.calloc(1, sizeof(EseGuiLayoutNode), MMTAG_GUI);
    button_node->x = 0;
    button_node->y = 0;
    button_node->width = GUI_AUTO_SIZE;
    button_node->height = GUI_AUTO_SIZE;
    button_node->is_hovered = false;
    button_node->is_down = false;
    _ese_gui_copy_colors_from_style(button_node, gui->current_style);
    button_node->widget_type = ESE_GUI_WIDGET_BUTTON;

    button_node->widget_data.button.text = memory_manager.strdup(text, MMTAG_GUI);
    button_node->widget_data.button.callback = callback;
    button_node->widget_data.button.userdata = userdata;

    // Attach to current container
    button_node->children = NULL;
    button_node->children_count = 0;
    button_node->children_capacity = 0;
    button_node->parent = NULL;
    log_assert("GUI", layout->current_container != NULL, "ese_gui_push_button called with no open container");
    {
        EseGuiLayoutNode *parent = layout->current_container;
        button_node->parent = parent;
        if (parent->children_count >= parent->children_capacity) {
            size_t new_cap = parent->children_capacity == 0 ? 4 : parent->children_capacity * 2;
            parent->children = (EseGuiLayoutNode **)memory_manager.realloc(parent->children, sizeof(EseGuiLayoutNode*) * new_cap, MMTAG_GUI);
            parent->children_capacity = new_cap;
        }
        parent->children[parent->children_count++] = button_node;
    }
}

void ese_gui_push_image(EseGui *gui, EseGuiImageFit fit, const char *sprite_id) {
    log_assert("GUI", gui, "ese_gui_push_image called with NULL gui");
    log_assert("GUI", gui->open_layout != NULL, "ese_gui_push_image called with no open frame layout");
    log_assert("GUI", sprite_id != NULL, "ese_gui_push_image called with NULL sprite_id");

    EseGuiLayout *layout = gui->open_layout;

    EseGuiLayoutNode *image_node = (EseGuiLayoutNode *)memory_manager.calloc(1, sizeof(EseGuiLayoutNode), MMTAG_GUI);

    image_node->x = 0;
    image_node->y = 0;
    image_node->width = GUI_AUTO_SIZE;
    image_node->height = GUI_AUTO_SIZE;
    image_node->is_hovered = false;
    image_node->is_down = false;
    _ese_gui_copy_colors_from_style(image_node, gui->current_style);
    image_node->widget_type = ESE_GUI_WIDGET_IMAGE;

    image_node->widget_data.image.sprite_id = memory_manager.strdup(sprite_id, MMTAG_GUI);
    image_node->widget_data.image.fit = fit;

    // Attach to current container
    image_node->children = NULL;
    image_node->children_count = 0;
    image_node->children_capacity = 0;
    image_node->parent = NULL;
    log_assert("GUI", layout->current_container != NULL, "ese_gui_push_image called with no open container");
    {
        EseGuiLayoutNode *parent = layout->current_container;
        image_node->parent = parent;
        if (parent->children_count >= parent->children_capacity) {
            size_t new_cap = parent->children_capacity == 0 ? 4 : parent->children_capacity * 2;
            parent->children = (EseGuiLayoutNode **)memory_manager.realloc(parent->children, sizeof(EseGuiLayoutNode*) * new_cap, MMTAG_GUI);
            parent->children_capacity = new_cap;
        }
        parent->children[parent->children_count++] = image_node;
    }
}

EseGuiStyle *ese_gui_get_style(EseGui *gui) {
    log_assert("GUI", gui != NULL, "ese_gui_get_style called with NULL gui");
    return gui->current_style;
}

void ese_gui_set_style(EseGui *gui, EseGuiStyle *style) {
    log_assert("GUI", gui != NULL, "ese_gui_set_style called with NULL gui");
    log_assert("GUI", style != NULL, "ese_gui_set_style called with NULL style");

    // Unref the old style
    ese_gui_style_unref(gui->current_style);
    ese_gui_style_destroy(gui->current_style);

    // Set and ref the new style
    gui->current_style = style;
    ese_gui_style_ref(gui->current_style);
}
