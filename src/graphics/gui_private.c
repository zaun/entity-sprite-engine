#include <stdlib.h>
#include <string.h>
#include "core/memory_manager.h"
#include "graphics/gui_private.h"
#include "graphics/gui.h"
#include "graphics/draw_list.h"
#include "types/color.h"
#include "types/input_state.h"
#include "utility/log.h"

// Destroy a frame layout's layout tree and reset pointers
void _ese_gui_layout_destroy(EseGuiLayout *layout) {
    if (layout->root != NULL) {
        typedef struct { EseGuiLayoutNode *node; size_t idx; } StackEntry;
        size_t cap = 32, top = 0;
        StackEntry *stack = (StackEntry *)memory_manager.malloc(sizeof(StackEntry) * cap, MMTAG_GUI);
        stack[top++] = (StackEntry){ layout->root, 0 };
        while (top > 0) {
            StackEntry *e = &stack[top - 1];
            if (e->idx < e->node->children_count) {
                EseGuiLayoutNode *child = e->node->children[e->idx++];
                if (top >= cap) {
                    cap *= 2;
                    stack = (StackEntry *)memory_manager.realloc(stack, sizeof(StackEntry) * cap, MMTAG_GUI);
                }
                stack[top++] = (StackEntry){ child, 0 };
            } else {
                if (e->node->widget_type == ESE_GUI_WIDGET_BUTTON && e->node->widget_data.button.text) {
                    memory_manager.free(e->node->widget_data.button.text);
                }
                if (e->node->widget_type == ESE_GUI_WIDGET_IMAGE && e->node->widget_data.image.sprite_id) {
                    memory_manager.free(e->node->widget_data.image.sprite_id);
                }
                if (e->node->children) memory_manager.free(e->node->children);
                memory_manager.free(e->node);
                top--;
            }
        }
        memory_manager.free(stack);
        layout->root = NULL;
    }
    layout->current_container = NULL;
}

// Returns true if the mouse position lies within the node's rectangle
static bool _ese_gui_is_mouse_over(int mouse_x, int mouse_y, EseGuiLayoutNode *node) {
    int x0 = node->x;
    int x1 = node->x + node->width;
    if (x0 > x1) {
        int t = x0; x0 = x1; x1 = t;
    }

    int y0 = node->y;
    int y1 = node->y + node->height;
    if (y0 > y1) {
        int t = y0; y0 = y1; y1 = t;
    }

    return mouse_x >= x0 && mouse_x < x1 &&
           mouse_y >= y0 && mouse_y < y1;
}

// Update hover/press state and trigger callbacks as needed
static void _ese_gui_process_input(EseGui *gui, EseGuiLayoutNode *node) {
    if (gui->input_state == NULL) {
        node->is_hovered = false;
        node->is_down = false;
        return;
    }

    int mouse_x = ese_input_state_get_mouse_x(gui->input_state);
    int mouse_y = ese_input_state_get_mouse_y(gui->input_state);

    node->is_hovered = _ese_gui_is_mouse_over(mouse_x, mouse_y, node);
    node->is_down = ese_input_state_get_mouse_down(gui->input_state, 0);
    bool is_clicked = ese_input_state_get_mouse_clicked(gui->input_state, 0);

    if (node->widget_type == ESE_GUI_WIDGET_BUTTON) {
        if (is_clicked && node->is_hovered && node->widget_data.button.callback) {
            node->widget_data.button.callback();
        }
    }
}

// Calculate absolute positions for the node tree, then process input per node
void _ese_gui_calculate_node_position(
    EseGui *gui,
    EseGuiLayout *session,
    EseGuiLayoutNode *node,
    size_t depth
) {
    log_assert("GUI", gui, "ese_gui_calculate_node_position called with NULL gui");
    log_assert("GUI", session, "ese_gui_calculate_node_position called with NULL session");
    log_assert("GUI", node, "ese_gui_calculate_node_position called with NULL node");
    log_assert("GUI", depth < 63, "ese_gui_calculate_node_position called with depth >= 63");

    char indent[64] = {0};
    for (size_t i = 0; i < depth; i++) {
        indent[i] = ' ';
    }

    if (node->parent == NULL) {
        log_verbose("GUI", "%sRoot node", indent);
        node->x = session->x;
        node->y = session->y;
        if (node->widget_type != ESE_GUI_WIDGET_BOX) {
            node->width = session->width;
            node->height = session->height;
        }
    }

    size_t child_count = node->children_count;
    size_t child_auto_count = 0;
    int fixed_width = 0;
    int fixed_height = 0;
    for (size_t i = 0; i < child_count; i++) {
        EseGuiLayoutNode *child = node->children[i];
        if (child->widget_type != ESE_GUI_WIDGET_BOX) {
            child_auto_count++;
        } else {
            fixed_width += child->width;
            fixed_height += child->height;
        }
    }

    int start_x = node->x;
    int start_y = node->y;

    if (node->widget_type == ESE_GUI_WIDGET_BOX) {
        log_verbose("GUI", "%sBox node", indent);
        if (node->children_count == 1) {
            node->children[0]->x = start_x + node->widget_data.container.padding_left;
            node->children[0]->y = start_y + node->widget_data.container.padding_top;
            node->children[0]->width = node->width - node->widget_data.container.padding_left - node->widget_data.container.padding_right;
            node->children[0]->height = node->height - node->widget_data.container.padding_top - node->widget_data.container.padding_bottom;
            log_verbose("GUI", "%sChild 1 position: %d, %d, %d, %d", indent, node->children[0]->x, node->children[0]->y, node->children[0]->width, node->children[0]->height);
            _ese_gui_process_input(gui, node->children[0]);
            _ese_gui_calculate_node_position(gui, session, node->children[0], depth + 1);
        }
    } else if (node->widget_type == ESE_GUI_WIDGET_FLEX) {
        log_verbose("GUI", "%sFlex node", indent);
        int total_spacing = 0;
        if (child_count > 0) {
            total_spacing = node->widget_data.container.spacing * (int)(child_count - 1);
        }
        if (node->widget_data.container.direction == FLEX_DIRECTION_ROW) {
            int total_free_width = node->width - node->widget_data.container.padding_left - node->widget_data.container.padding_right - total_spacing;
            if (total_free_width < 0) {
                total_free_width = 0;
            }
            int auto_width = 0;
            if (child_auto_count > 0) {
                auto_width = total_free_width / child_auto_count;
            }

            log_verbose("GUI", "%sAuto child count: %zu Free width: %d, Auto width: %d", indent, child_auto_count, total_free_width, auto_width);

            if (node->widget_data.container.justify == FLEX_JUSTIFY_START) {
                start_x += node->widget_data.container.padding_left;
                log_verbose("GUI", "%sStart justify x: %d", indent, start_x);
            } else if (node->widget_data.container.justify == FLEX_JUSTIFY_END) {
                start_x += node->width - node->widget_data.container.padding_right - total_free_width + fixed_width;
                log_verbose("GUI", "%sEnd justify x: %d", indent, start_x);
            } else if (node->widget_data.container.justify == FLEX_JUSTIFY_CENTER) {
                start_x += (node->width - node->widget_data.container.padding_left - node->widget_data.container.padding_right - total_free_width + fixed_width) / 2;
                log_verbose("GUI", "%sCenter justify x: %d", indent, start_x);
            }

            start_y += node->widget_data.container.padding_top;

            for (size_t i = 0; i < child_count; i++) {
                EseGuiLayoutNode *child = node->children[i];
                child->x = start_x;
                if (child->widget_type != ESE_GUI_WIDGET_BOX) {
                    child->width = auto_width;
                    child->height = node->height - node->widget_data.container.padding_top - node->widget_data.container.padding_bottom;
                    if (node->widget_data.container.align_items == FLEX_ALIGN_ITEMS_START) {
                        child->y = start_y;
                    } else if (node->widget_data.container.align_items == FLEX_ALIGN_ITEMS_END) {
                        child->y = start_y + node->height - node->widget_data.container.padding_bottom - child->height;
                    } else if (node->widget_data.container.align_items == FLEX_ALIGN_ITEMS_CENTER) {
                        child->y = start_y + ((child->height + node->widget_data.container.padding_bottom) / 2);
                    }
                } else {
                    if (node->widget_data.container.align_items == FLEX_ALIGN_ITEMS_START) {
                        child->y = start_y;
                    } else if (node->widget_data.container.align_items == FLEX_ALIGN_ITEMS_END) {
                        child->y = start_y + node->height - node->widget_data.container.padding_bottom - child->height;
                    } else if (node->widget_data.container.align_items == FLEX_ALIGN_ITEMS_CENTER) {
                        child->y = start_y + ((node->height - child->height - node->widget_data.container.padding_bottom) / 2);
                    }
                }

                start_x += child->width + node->widget_data.container.spacing;

                log_verbose("GUI", "%sChild %zu position: %d, %d, %d, %d", indent, i + 1, child->x, child->y, child->width, child->height);
                _ese_gui_process_input(gui, child);
                _ese_gui_calculate_node_position(gui, session, child, depth + 1);
            }
        } else if (node->widget_data.container.direction == FLEX_DIRECTION_COLUMN) {
            int total_free_height = node->height - node->widget_data.container.padding_top - node->widget_data.container.padding_bottom - total_spacing;
            if (total_free_height < 0) {
                total_free_height = 0;
            }
            int auto_height = 0;
            if (child_auto_count > 0) {
                auto_height = total_free_height / child_auto_count;
            }
            log_verbose("GUI", "%sAuto child count: %zu Free height: %d, Auto height: %d", indent, child_auto_count, total_free_height, auto_height);

            if (node->widget_data.container.align_items == FLEX_ALIGN_ITEMS_START) {
                start_y += node->widget_data.container.padding_top;
                log_verbose("GUI", "%sStart align y: %d", indent, start_y);
            } else if (node->widget_data.container.align_items == FLEX_ALIGN_ITEMS_END) {
                start_y += node->height - node->widget_data.container.padding_bottom - total_free_height + fixed_height;
                log_verbose("GUI", "%sEnd align y: %d", indent, start_y);
            } else if (node->widget_data.container.align_items == FLEX_ALIGN_ITEMS_CENTER) {
                start_y += (node->height - node->widget_data.container.padding_top - node->widget_data.container.padding_bottom - total_free_height + fixed_height) / 2;
                log_verbose("GUI", "%sCenter align y: %d", indent, start_y);
            }

            start_x += node->widget_data.container.padding_left;

            for (size_t i = 0; i < child_count; i++) {
                EseGuiLayoutNode *child = node->children[i];
                child->y = start_y;
                if (child->widget_type != ESE_GUI_WIDGET_BOX) {
                    child->width = node->width - node->widget_data.container.padding_left - node->widget_data.container.padding_right;
                    child->height = auto_height;
                    if (node->widget_data.container.justify == FLEX_JUSTIFY_START) {
                        child->x = start_x;
                    } else if (node->widget_data.container.justify == FLEX_JUSTIFY_END) {
                        child->x = start_x;
                    } else if (node->widget_data.container.justify == FLEX_JUSTIFY_CENTER) {
                        child->x = start_x + ((child->width + node->widget_data.container.padding_right) / 2);
                    }
                } else {
                    if (node->widget_data.container.justify == FLEX_JUSTIFY_START) {
                        child->x = start_x;
                    } else if (node->widget_data.container.justify == FLEX_JUSTIFY_END) {
                        child->x = start_x + node->width - node->widget_data.container.padding_right - child->width;
                    } else if (node->widget_data.container.justify == FLEX_JUSTIFY_CENTER) {
                        child->x = start_x + ((node->width - child->width - node->widget_data.container.padding_right) / 2);
                    }
                }

                start_y += child->height + node->widget_data.container.spacing;

                log_verbose("GUI", "%sChild %zu position: %d, %d, %d, %d", indent, i + 1, child->x, child->y, child->width, child->height);
                _ese_gui_process_input(gui, child);
                _ese_gui_calculate_node_position(gui, session, child, depth + 1);
            }
        }
    } else {
        log_verbose("GUI", "%sUnknown node type", indent);
    }
}

// Generate draw commands for a node tree depth-first
void _ese_gui_generate_draw_commands(EseGui *gui, EseDrawList *draw_list, EseGuiLayout *session, EseGuiLayoutNode *node, size_t depth) {
    if (node == NULL) return;
    if (node->width <= 0 || node->height <= 0) {
        return;
    }

    uint64_t draw_order = ((uint64_t)session->z_index << 32);
    draw_order += depth * 10;

    EseGuiColorType background = ESE_GUI_COLOR_BACKGROUND;
    if (node->is_hovered) background = ESE_GUI_COLOR_BACKGROUND_HOVERED;
    if (node->is_down) background = ESE_GUI_COLOR_BACKGROUND_PRESSED;

    EseGuiColorType border = ESE_GUI_COLOR_BORDER;
    if (node->is_hovered) border = ESE_GUI_COLOR_BORDER_HOVERED;
    if (node->is_down) border = ESE_GUI_COLOR_BORDER_PRESSED;

    EseGuiColorType text = ESE_GUI_COLOR_BUTTON_TEXT;
    if (node->is_hovered) text = ESE_GUI_COLOR_BUTTON_TEXT_HOVERED;
    if (node->is_down) text = ESE_GUI_COLOR_BUTTON_TEXT_PRESSED;

    switch (node->widget_type) {
        case ESE_GUI_WIDGET_NONE:
            break;
        case ESE_GUI_WIDGET_FLEX:
        case ESE_GUI_WIDGET_BOX:
            if (node->colors[background] != NULL) {
                EseDrawListObject *bg_obj = draw_list_request_object(draw_list);
                draw_list_object_set_rect_color(bg_obj, 
                    (unsigned char)(ese_color_get_r(node->colors[background]) * 255), 
                    (unsigned char)(ese_color_get_g(node->colors[background]) * 255), 
                    (unsigned char)(ese_color_get_b(node->colors[background]) * 255), 
                    (unsigned char)(ese_color_get_a(node->colors[background]) * 255), true);
                draw_list_object_set_bounds(bg_obj, node->x, node->y, node->width, node->height);
                draw_list_object_set_z_index(bg_obj, draw_order);
            } else {
                EseColor *default_bg = gui->default_colors[background];
                EseDrawListObject *bg_obj = draw_list_request_object(draw_list);
                draw_list_object_set_rect_color(bg_obj, 
                    (unsigned char)(ese_color_get_r(default_bg) * 255), 
                    (unsigned char)(ese_color_get_g(default_bg) * 255), 
                    (unsigned char)(ese_color_get_b(default_bg) * 255), 
                    (unsigned char)(ese_color_get_a(default_bg) * 255), true);
                draw_list_object_set_bounds(bg_obj, node->x, node->y, node->width, node->height);
                draw_list_object_set_z_index(bg_obj, draw_order);
            }

            if (node->colors[border] != NULL) {
                EseDrawListObject *border_obj = draw_list_request_object(draw_list);
                draw_list_object_set_rect_color(border_obj, 
                    (unsigned char)(ese_color_get_r(node->colors[border]) * 255), 
                    (unsigned char)(ese_color_get_g(node->colors[border]) * 255), 
                    (unsigned char)(ese_color_get_b(node->colors[border]) * 255), 
                    (unsigned char)(ese_color_get_a(node->colors[border]) * 255), false);
                draw_list_object_set_bounds(border_obj, node->x, node->y, node->width, node->height);
                draw_list_object_set_z_index(border_obj, draw_order + 1);
            } else {
                EseColor *default_border = gui->default_colors[border];
                EseDrawListObject *border_obj = draw_list_request_object(draw_list);
                draw_list_object_set_rect_color(border_obj, 
                    (unsigned char)(ese_color_get_r(default_border) * 255), 
                    (unsigned char)(ese_color_get_g(default_border) * 255), 
                    (unsigned char)(ese_color_get_b(default_border) * 255), 
                    (unsigned char)(ese_color_get_a(default_border) * 255), false);
                draw_list_object_set_bounds(border_obj, node->x, node->y, node->width, node->height);
                draw_list_object_set_z_index(border_obj, draw_order + 1);
            }
            break;
        case ESE_GUI_WIDGET_BUTTON:
            if (node->colors[background] != NULL) {
                EseDrawListObject *bg_obj = draw_list_request_object(draw_list);
                draw_list_object_set_rect_color(bg_obj, 
                    (unsigned char)(ese_color_get_r(node->colors[background]) * 255), 
                    (unsigned char)(ese_color_get_g(node->colors[background]) * 255), 
                    (unsigned char)(ese_color_get_b(node->colors[background]) * 255), 
                    (unsigned char)(ese_color_get_a(node->colors[background]) * 255), true);
                draw_list_object_set_bounds(bg_obj, node->x, node->y, node->width, node->height);
                draw_list_object_set_z_index(bg_obj, draw_order);
            } else {
                EseColor *default_bg = gui->default_colors[background];
                EseDrawListObject *bg_obj = draw_list_request_object(draw_list);
                draw_list_object_set_rect_color(bg_obj, 
                    (unsigned char)(ese_color_get_r(default_bg) * 255), 
                    (unsigned char)(ese_color_get_g(default_bg) * 255), 
                    (unsigned char)(ese_color_get_b(default_bg) * 255), 
                    (unsigned char)(ese_color_get_a(default_bg) * 255), true);
                draw_list_object_set_bounds(bg_obj, node->x, node->y, node->width, node->height);
                draw_list_object_set_z_index(bg_obj, draw_order);
            }

            if (node->colors[border] != NULL) {
                EseDrawListObject *border_obj = draw_list_request_object(draw_list);
                draw_list_object_set_rect_color(border_obj, 
                    (unsigned char)(ese_color_get_r(node->colors[border]) * 255), 
                    (unsigned char)(ese_color_get_g(node->colors[border]) * 255), 
                    (unsigned char)(ese_color_get_b(node->colors[border]) * 255), 
                    (unsigned char)(ese_color_get_a(node->colors[border]) * 255), false);
                draw_list_object_set_bounds(border_obj, node->x, node->y, node->width, node->height);
                draw_list_object_set_z_index(border_obj, draw_order + 1);
            } else {
                EseColor *default_border = gui->default_colors[border];
                EseDrawListObject *border_obj = draw_list_request_object(draw_list);
                draw_list_object_set_rect_color(border_obj, 
                    (unsigned char)(ese_color_get_r(default_border) * 255), 
                    (unsigned char)(ese_color_get_g(default_border) * 255), 
                    (unsigned char)(ese_color_get_b(default_border) * 255), 
                    (unsigned char)(ese_color_get_a(default_border) * 255), false);
                draw_list_object_set_bounds(border_obj, node->x, node->y, node->width, node->height);
                draw_list_object_set_z_index(border_obj, draw_order + 1);
            }

            if (node->widget_data.button.text != NULL) {
                // Note: Text rendering is not yet implemented in the draw_list API
                // For now, we'll skip text rendering until a text API is added
                // TODO: Implement text rendering when draw_list supports it
            }
            break;
        case ESE_GUI_WIDGET_IMAGE:
            if (node->colors[background] != NULL) {
                EseDrawListObject *bg_obj = draw_list_request_object(draw_list);
                draw_list_object_set_rect_color(bg_obj, 
                    (unsigned char)(ese_color_get_r(node->colors[background]) * 255), 
                    (unsigned char)(ese_color_get_g(node->colors[background]) * 255), 
                    (unsigned char)(ese_color_get_b(node->colors[background]) * 255), 
                    (unsigned char)(ese_color_get_a(node->colors[background]) * 255), true);
                draw_list_object_set_bounds(bg_obj, node->x, node->y, node->width, node->height);
                draw_list_object_set_z_index(bg_obj, draw_order);
            } else {
                EseColor *default_bg = gui->default_colors[background];
                EseDrawListObject *bg_obj = draw_list_request_object(draw_list);
                draw_list_object_set_rect_color(bg_obj, 
                    (unsigned char)(ese_color_get_r(default_bg) * 255), 
                    (unsigned char)(ese_color_get_g(default_bg) * 255), 
                    (unsigned char)(ese_color_get_b(default_bg) * 255), 
                    (unsigned char)(ese_color_get_a(default_bg) * 255), true);
                draw_list_object_set_bounds(bg_obj, node->x, node->y, node->width, node->height);
                draw_list_object_set_z_index(bg_obj, draw_order);
            }

            if (node->colors[border] != NULL) {
                EseDrawListObject *border_obj = draw_list_request_object(draw_list);
                draw_list_object_set_rect_color(border_obj, 
                    (unsigned char)(ese_color_get_r(node->colors[border]) * 255), 
                    (unsigned char)(ese_color_get_g(node->colors[border]) * 255), 
                    (unsigned char)(ese_color_get_b(node->colors[border]) * 255), 
                    (unsigned char)(ese_color_get_a(node->colors[border]) * 255), false);
                draw_list_object_set_bounds(border_obj, node->x, node->y, node->width, node->height);
                draw_list_object_set_z_index(border_obj, draw_order + 1);
            } else {
                EseColor *default_border = gui->default_colors[border];
                EseDrawListObject *border_obj = draw_list_request_object(draw_list);
                draw_list_object_set_rect_color(border_obj, 
                    (unsigned char)(ese_color_get_r(default_border) * 255), 
                    (unsigned char)(ese_color_get_g(default_border) * 255), 
                    (unsigned char)(ese_color_get_b(default_border) * 255), 
                    (unsigned char)(ese_color_get_a(default_border) * 255), false);
                draw_list_object_set_bounds(border_obj, node->x, node->y, node->width, node->height);
                draw_list_object_set_z_index(border_obj, draw_order + 1);
            }

            if (node->widget_data.image.sprite_id != NULL) {
                EseDrawListObject *img_obj = draw_list_request_object(draw_list);
                draw_list_object_set_texture(img_obj, node->widget_data.image.sprite_id, 0.0f, 0.0f, 1.0f, 1.0f);
                draw_list_object_set_bounds(img_obj, node->x, node->y, node->width, node->height);
                draw_list_object_set_z_index(img_obj, draw_order + 2);
            }
            break;
    }

    for (size_t i = 0; i < node->children_count; i++) {
        _ese_gui_generate_draw_commands(gui, draw_list, session, node->children[i], depth + 1);
    }
}


