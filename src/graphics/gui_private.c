#include <stdlib.h>
#include <string.h>
#include "core/memory_manager.h"
#include "core/asset_manager.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "scripting/lua_engine.h"
#include "graphics/gui_private.h"
#include "graphics/gui.h"
#include "graphics/draw_list.h"
#include "graphics/font.h"
#include "types/color.h"
#include "types/gui_style.h"
#include "types/input_state.h"
#include "utility/log.h"

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Font drawing callback for GUI text rendering
static void _ese_gui_font_draw_callback(float screen_x, float screen_y, float screen_w, float screen_h, uint64_t z_index,
                                       const char *texture_id, float texture_x1, float texture_y1, float texture_x2, float texture_y2,
                                       int width, int height, void *user_data);

// Structure to pass scissor information to the font callback
typedef struct {
    EseDrawList *draw_list;
    bool scissor_active;
    float scissor_x, scissor_y, scissor_w, scissor_h;
} EseGuiFontCallbackData;

// ========================================
// PRIVATE FUNCTIONS
// ========================================

/**
 * @brief Font drawing callback for GUI text rendering
 * 
 * This callback is used by the font system to draw individual characters
 * as sprites in the draw list. It handles scissor clipping to ensure text
 * is properly clipped to the button area.
 */
static void _ese_gui_font_draw_callback(float screen_x, float screen_y, float screen_w, float screen_h, uint64_t z_index,
                                       const char *texture_id, float texture_x1, float texture_y1, float texture_x2, float texture_y2,
                                       int width, int height, void *user_data) {
    EseGuiFontCallbackData *data = (EseGuiFontCallbackData *)user_data;
    EseDrawList *draw_list = data->draw_list;
    
    log_verbose("GUI", "Font callback: char at (%.1f,%.1f) size %.1fx%.1f texture=%s", 
                screen_x, screen_y, screen_w, screen_h, texture_id);
    
    // Check if the character is within the scissor bounds
    if (data->scissor_active) {
        float char_right = screen_x + screen_w;
        float char_bottom = screen_y + screen_h;
        float scissor_right = data->scissor_x + data->scissor_w;
        float scissor_bottom = data->scissor_y + data->scissor_h;
        
        // Skip characters that are completely outside the scissor area
        if (char_right < data->scissor_x || screen_x > scissor_right ||
            char_bottom < data->scissor_y || screen_y > scissor_bottom) {
            log_verbose("GUI", "Character clipped by scissor");
            return;
        }
    }
    
    EseDrawListObject *text_obj = draw_list_request_object(draw_list);
    if (text_obj == NULL) {
        log_error("GUI", "Failed to get draw list object for text");
        return;
    }
    
    draw_list_object_set_texture(text_obj, texture_id, texture_x1, texture_y1, texture_x2, texture_y2);
    draw_list_object_set_bounds(text_obj, (int)screen_x, (int)screen_y, (int)screen_w, (int)screen_h);
    draw_list_object_set_z_index(text_obj, z_index);
    
    // Apply scissor if active
    if (data->scissor_active) {
        draw_list_object_set_scissor(text_obj, data->scissor_x, data->scissor_y, data->scissor_w, data->scissor_h);
    }
    
    log_verbose("GUI", "Text object created successfully");
}

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
                _ese_gui_free_node_colors(e->node);
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

// Returns true if the mouse position lies within the node's rectangle and scissor bounds
static bool _ese_gui_is_mouse_over(int mouse_x, int mouse_y, EseGuiLayoutNode *node, EseGuiLayout *session) {
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

    // Check if mouse is within node bounds
    bool within_node = mouse_x >= x0 && mouse_x < x1 &&
                       mouse_y >= y0 && mouse_y < y1;
    
    if (!within_node) {
        return false;
    }
    
    // If scissor is active, also check if mouse is within scissor bounds
    if (session->draw_scissors_active) {
        int scissor_x0 = (int)session->draw_scissors_x;
        int scissor_y0 = (int)session->draw_scissors_y;
        int scissor_x1 = scissor_x0 + (int)session->draw_scissors_w;
        int scissor_y1 = scissor_y0 + (int)session->draw_scissors_h;
        
        return mouse_x >= scissor_x0 && mouse_x < scissor_x1 &&
               mouse_y >= scissor_y0 && mouse_y < scissor_y1;
    }
    
    return true;
}

// Update hover/press state and trigger callbacks as needed
static void _ese_gui_process_input(EseGui *gui, EseGuiLayout *session, EseGuiLayoutNode *node) {
    if (gui->input_state == NULL) {
        node->is_hovered = false;
        node->is_down = false;
        return;
    }

    int mouse_x = ese_input_state_get_mouse_x(gui->input_state);
    int mouse_y = ese_input_state_get_mouse_y(gui->input_state);

    node->is_hovered = _ese_gui_is_mouse_over(mouse_x, mouse_y, node, session);
    node->is_down = node->is_hovered && ese_input_state_get_mouse_down(gui->input_state, 0);
    bool is_clicked = ese_input_state_get_mouse_clicked(gui->input_state, 0);

    if (node->widget_type == ESE_GUI_WIDGET_BUTTON) {
        if (is_clicked && node->is_hovered && node->widget_data.button.callback) {
            node->widget_data.button.callback(node->widget_data.button.userdata);
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
        if (node->widget_type != ESE_GUI_WIDGET_STACK) {
            node->width = session->width;
            node->height = session->height;
        }
    }

    size_t child_count = node->children_count;
    size_t child_auto_count_width = 0;
    size_t child_auto_count_height = 0;
    int fixed_width = 0;
    int fixed_height = 0;
    for (size_t i = 0; i < child_count; i++) {
        EseGuiLayoutNode *child = node->children[i];
        if (child->width == GUI_AUTO_SIZE) {
            child_auto_count_width++;
        } else {
            fixed_width += child->width;
        }
        if (child->height == GUI_AUTO_SIZE) {
            child_auto_count_height++;
        } else {
            fixed_height += child->height;
        }
    }

    int start_x = node->x;
    int start_y = node->y;

    if (node->widget_type == ESE_GUI_WIDGET_STACK) {
        log_verbose("GUI", "%sStack node", indent);
        // Set up scissor state for input processing BEFORE calculating child position
        bool previous_scissor_active = session->draw_scissors_active;
        float previous_scissor_x = session->draw_scissors_x;
        float previous_scissor_y = session->draw_scissors_y;
        float previous_scissor_w = session->draw_scissors_w;
        float previous_scissor_h = session->draw_scissors_h;
        
        // Calculate the clipping rectangle for children (accounting for padding)
        int clip_x = node->x + node->widget_data.container.padding_left;
        int clip_y = node->y + node->widget_data.container.padding_top;
        int clip_w = node->width - node->widget_data.container.padding_left - node->widget_data.container.padding_right;
        int clip_h = node->height - node->widget_data.container.padding_top - node->widget_data.container.padding_bottom;
        
        // Only apply clipping if the container has positive dimensions
        if (clip_w > 0 && clip_h > 0) {
            session->draw_scissors_active = true;
            session->draw_scissors_x = (float)clip_x;
            session->draw_scissors_y = (float)clip_y;
            session->draw_scissors_w = (float)clip_w;
            session->draw_scissors_h = (float)clip_h;
        } else {
            session->draw_scissors_active = false;
        }
        
        for (size_t i = 0; i < child_count; i++) {
            node->children[i]->x = start_x + node->widget_data.container.padding_left;
            node->children[i]->y = start_y + node->widget_data.container.padding_top;
            node->children[i]->width = node->width - node->widget_data.container.padding_left - node->widget_data.container.padding_right;
            node->children[i]->height = node->height - node->widget_data.container.padding_top - node->widget_data.container.padding_bottom;
            log_verbose("GUI", "%sChild 1 position: %d, %d, %d, %d", indent, node->children[i]->x, node->children[i]->y, node->children[i]->width, node->children[i]->height);
            
            _ese_gui_process_input(gui, session, node->children[0]);
            _ese_gui_calculate_node_position(gui, session, node->children[0], depth + 1);
        }
            
        // Restore previous scissor state
        session->draw_scissors_active = previous_scissor_active;
        session->draw_scissors_x = previous_scissor_x;
        session->draw_scissors_y = previous_scissor_y;
        session->draw_scissors_w = previous_scissor_w;
        session->draw_scissors_h = previous_scissor_h;
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
            if (child_auto_count_width > 0) {
                auto_width = total_free_width / child_auto_count_width;
            }

            log_verbose("GUI", "%sAuto child count: %zu Free width: %d, Auto width: %d", indent, child_auto_count_width, total_free_width, auto_width);

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

            // Set up scissor state for input processing BEFORE processing children
            bool previous_scissor_active = session->draw_scissors_active;
            float previous_scissor_x = session->draw_scissors_x;
            float previous_scissor_y = session->draw_scissors_y;
            float previous_scissor_w = session->draw_scissors_w;
            float previous_scissor_h = session->draw_scissors_h;
            
            // Calculate the clipping rectangle for children (accounting for padding)
            int clip_x = node->x + node->widget_data.container.padding_left;
            int clip_y = node->y + node->widget_data.container.padding_top;
            int clip_w = node->width - node->widget_data.container.padding_left - node->widget_data.container.padding_right;
            int clip_h = node->height - node->widget_data.container.padding_top - node->widget_data.container.padding_bottom;
            
            // Only apply clipping if the container has positive dimensions
            if (clip_w > 0 && clip_h > 0) {
                session->draw_scissors_active = true;
                session->draw_scissors_x = (float)clip_x;
                session->draw_scissors_y = (float)clip_y;
                session->draw_scissors_w = (float)clip_w;
                session->draw_scissors_h = (float)clip_h;
            } else {
                session->draw_scissors_active = false;
            }

            for (size_t i = 0; i < child_count; i++) {
                EseGuiLayoutNode *child = node->children[i];
                child->x = start_x;
                if (child->widget_type != ESE_GUI_WIDGET_STACK) {
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
                    // Handle Stack widgets with auto-sizing
                    if (child->width == GUI_AUTO_SIZE) {
                        child->width = auto_width;
                    }
                    if (child->height == GUI_AUTO_SIZE) {
                        child->height = node->height - node->widget_data.container.padding_top - node->widget_data.container.padding_bottom;
                    }
                    
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
                _ese_gui_process_input(gui, session, child);
                _ese_gui_calculate_node_position(gui, session, child, depth + 1);
            }
            
            // Restore previous scissor state
            session->draw_scissors_active = previous_scissor_active;
            session->draw_scissors_x = previous_scissor_x;
            session->draw_scissors_y = previous_scissor_y;
            session->draw_scissors_w = previous_scissor_w;
            session->draw_scissors_h = previous_scissor_h;
        } else if (node->widget_data.container.direction == FLEX_DIRECTION_COLUMN) {
            int total_free_height = node->height - node->widget_data.container.padding_top - node->widget_data.container.padding_bottom - total_spacing;
            if (total_free_height < 0) {
                total_free_height = 0;
            }
            int auto_height = 0;
            if (child_auto_count_height > 0) {
                auto_height = total_free_height / child_auto_count_height;
            }
            log_verbose("GUI", "%sAuto child count: %zu Free height: %d, Auto height: %d", indent, child_auto_count_height, total_free_height, auto_height);

            if (node->widget_data.container.justify == FLEX_JUSTIFY_START) {
                start_y += node->widget_data.container.padding_top;
                log_verbose("GUI", "%sStart justify y: %d", indent, start_y);
            } else if (node->widget_data.container.justify == FLEX_JUSTIFY_END) {
                start_y += node->height - node->widget_data.container.padding_bottom - total_free_height;
                log_verbose("GUI", "%sEnd justify y: %d", indent, start_y);
            } else if (node->widget_data.container.justify == FLEX_JUSTIFY_CENTER) {
                start_y += node->widget_data.container.padding_top + (node->height - node->widget_data.container.padding_top - node->widget_data.container.padding_bottom - total_free_height) / 2;
                log_verbose("GUI", "%sCenter justify y: %d", indent, start_y);
            }

            start_x += node->widget_data.container.padding_left;

            // Set up scissor state for input processing BEFORE processing children
            bool previous_scissor_active = session->draw_scissors_active;
            float previous_scissor_x = session->draw_scissors_x;
            float previous_scissor_y = session->draw_scissors_y;
            float previous_scissor_w = session->draw_scissors_w;
            float previous_scissor_h = session->draw_scissors_h;
            
            // Calculate the clipping rectangle for children (accounting for padding)
            int clip_x = node->x + node->widget_data.container.padding_left;
            int clip_y = node->y + node->widget_data.container.padding_top;
            int clip_w = node->width - node->widget_data.container.padding_left - node->widget_data.container.padding_right;
            int clip_h = node->height - node->widget_data.container.padding_top - node->widget_data.container.padding_bottom;
            
            // Only apply clipping if the container has positive dimensions
            if (clip_w > 0 && clip_h > 0) {
                session->draw_scissors_active = true;
                session->draw_scissors_x = (float)clip_x;
                session->draw_scissors_y = (float)clip_y;
                session->draw_scissors_w = (float)clip_w;
                session->draw_scissors_h = (float)clip_h;
            } else {
                session->draw_scissors_active = false;
            }

            for (size_t i = 0; i < child_count; i++) {
                EseGuiLayoutNode *child = node->children[i];
                child->y = start_y;
                if (child->widget_type != ESE_GUI_WIDGET_STACK) {
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
                _ese_gui_process_input(gui, session, child);
                _ese_gui_calculate_node_position(gui, session, child, depth + 1);
            }
            
            // Restore previous scissor state
            session->draw_scissors_active = previous_scissor_active;
            session->draw_scissors_x = previous_scissor_x;
            session->draw_scissors_y = previous_scissor_y;
            session->draw_scissors_w = previous_scissor_w;
            session->draw_scissors_h = previous_scissor_h;
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
        case ESE_GUI_WIDGET_STACK:
            if (node->colors[background] != NULL) {
                EseDrawListObject *bg_obj = draw_list_request_object(draw_list);
                draw_list_object_set_rect_color(bg_obj, 
                    (unsigned char)(ese_color_get_r(node->colors[background]) * 255), 
                    (unsigned char)(ese_color_get_g(node->colors[background]) * 255), 
                    (unsigned char)(ese_color_get_b(node->colors[background]) * 255), 
                    (unsigned char)(ese_color_get_a(node->colors[background]) * 255), true);
                draw_list_object_set_bounds(bg_obj, node->x, node->y, node->width, node->height);
                draw_list_object_set_z_index(bg_obj, draw_order);
                if (session->draw_scissors_active) {
                    draw_list_object_set_scissor(bg_obj, session->draw_scissors_x, session->draw_scissors_y, session->draw_scissors_w, session->draw_scissors_h);
                }
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
                if (session->draw_scissors_active) {
                    draw_list_object_set_scissor(border_obj, session->draw_scissors_x, session->draw_scissors_y, session->draw_scissors_w, session->draw_scissors_h);
                }
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
                if (session->draw_scissors_active) {
                    draw_list_object_set_scissor(bg_obj, session->draw_scissors_x, session->draw_scissors_y, session->draw_scissors_w, session->draw_scissors_h);
                }
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
                if (session->draw_scissors_active) {
                    draw_list_object_set_scissor(border_obj, session->draw_scissors_x, session->draw_scissors_y, session->draw_scissors_w, session->draw_scissors_h);
                }
            }

            if (node->widget_data.button.text != NULL) {
                log_verbose("GUI", "Drawing button text: '%s'", node->widget_data.button.text);
                
                // Get font size from the current style
                int font_size = 20; // Default font size
                if (gui->current_style != NULL) {
                    font_size = ese_gui_style_get_font_size(gui->current_style);
                }
                
                // Calculate text position (centered in button)
                float text_width = strlen(node->widget_data.button.text) * (font_size * 0.6f); // Approximate character width
                float text_x = node->x + (node->width - text_width) / 2.0f;
                float text_y = node->y + (node->height - font_size) / 2.0f;
                
                log_verbose("GUI", "Text position: x=%.1f, y=%.1f, size=%d", text_x, text_y, font_size);
                
                // Get asset manager from the engine
                EseEngine *game_engine = (EseEngine *)lua_engine_get_registry_key(gui->engine->runtime, ENGINE_KEY);
                EseAssetManager *am = game_engine ? game_engine->asset_manager : NULL;
                if (am != NULL) {
                    log_verbose("GUI", "Asset manager found, drawing text");
                    
                    // Prepare callback data with scissor information
                    EseGuiFontCallbackData callback_data = {
                        .draw_list = draw_list,
                        .scissor_active = session->draw_scissors_active,
                        .scissor_x = session->draw_scissors_x,
                        .scissor_y = session->draw_scissors_y,
                        .scissor_w = session->draw_scissors_w,
                        .scissor_h = session->draw_scissors_h
                    };
                    
                    // Draw the text with clipping - try both font names
                    font_draw_text_scaled(am, "console_font_10x20", node->widget_data.button.text,
                                        text_x, text_y, draw_order + 2, (float)font_size,
                                        _ese_gui_font_draw_callback, &callback_data);
                } else {
                    log_error("GUI", "Asset manager not found, cannot draw text");
                }
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
                if (session->draw_scissors_active) {
                    draw_list_object_set_scissor(bg_obj, session->draw_scissors_x, session->draw_scissors_y, session->draw_scissors_w, session->draw_scissors_h);
                }
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
                if (session->draw_scissors_active) {
                    draw_list_object_set_scissor(border_obj, session->draw_scissors_x, session->draw_scissors_y, session->draw_scissors_w, session->draw_scissors_h);
                }
            }

            if (node->widget_data.image.sprite_id != NULL) {
                EseDrawListObject *img_obj = draw_list_request_object(draw_list);
                draw_list_object_set_texture(img_obj, node->widget_data.image.sprite_id, 0.0f, 0.0f, 1.0f, 1.0f);
                draw_list_object_set_bounds(img_obj, node->x, node->y, node->width, node->height);
                draw_list_object_set_z_index(img_obj, draw_order + 2);
                if (session->draw_scissors_active) {
                    draw_list_object_set_scissor(img_obj, session->draw_scissors_x, session->draw_scissors_y, session->draw_scissors_w, session->draw_scissors_h);
                }
            }
            break;
    }

    // Process children with clipping if this is a container
    if (node->widget_type == ESE_GUI_WIDGET_FLEX || node->widget_type == ESE_GUI_WIDGET_STACK) {
        // Calculate the clipping rectangle for children (accounting for padding)
        int clip_x = node->x + node->widget_data.container.padding_left;
        int clip_y = node->y + node->widget_data.container.padding_top;
        int clip_w = node->width - node->widget_data.container.padding_left - node->widget_data.container.padding_right;
        int clip_h = node->height - node->widget_data.container.padding_top - node->widget_data.container.padding_bottom;
        
        // Only apply clipping if the container has positive dimensions
        if (clip_w > 0 && clip_h > 0) {
            // Update the layout's scissor state for children
            session->draw_scissors_active = true;
            session->draw_scissors_x = (float)clip_x;
            session->draw_scissors_y = (float)clip_y;
            session->draw_scissors_w = (float)clip_w;
            session->draw_scissors_h = (float)clip_h;
        } else {
            // Disable scissor if container has invalid dimensions
            session->draw_scissors_active = false;
        }
    }

    // Process children normally (they will inherit the layout's scissor state)
    for (size_t i = 0; i < node->children_count; i++) {
        _ese_gui_generate_draw_commands(gui, draw_list, session, node->children[i], depth + 1);
    }
}

// Copy colors from a style to a node, creating new color objects
void _ese_gui_copy_colors_from_style(EseGuiLayoutNode *node, EseGuiStyle *style) {
    log_assert("GUI", node, "_ese_gui_copy_colors_from_style called with NULL node");
    log_assert("GUI", style, "_ese_gui_copy_colors_from_style called with NULL style");
    
    // Initialize all colors to NULL first
    for (size_t i = 0; i < ESE_GUI_COLOR_MAX; i++) {
        node->colors[i] = NULL;
    }
    
    // Copy background colors
    EseColor *bg = ese_gui_style_get_background(style);
    node->colors[ESE_GUI_COLOR_BACKGROUND] = ese_color_copy(bg);
    
    EseColor *bg_hovered = ese_gui_style_get_background_hovered(style);
    node->colors[ESE_GUI_COLOR_BACKGROUND_HOVERED] = ese_color_copy(bg_hovered);
    
    EseColor *bg_pressed = ese_gui_style_get_background_pressed(style);
    node->colors[ESE_GUI_COLOR_BACKGROUND_PRESSED] = ese_color_copy(bg_pressed);
    
    // Copy border colors
    EseColor *border = ese_gui_style_get_border(style);
    node->colors[ESE_GUI_COLOR_BORDER] = ese_color_copy(border);
    
    EseColor *border_hovered = ese_gui_style_get_border_hovered(style);
    node->colors[ESE_GUI_COLOR_BORDER_HOVERED] = ese_color_copy(border_hovered);
    
    EseColor *border_pressed = ese_gui_style_get_border_pressed(style);
    node->colors[ESE_GUI_COLOR_BORDER_PRESSED] = ese_color_copy(border_pressed);
    
    // Copy text colors
    EseColor *text = ese_gui_style_get_text(style);
    node->colors[ESE_GUI_COLOR_BUTTON_TEXT] = ese_color_copy(text);
    
    EseColor *text_hovered = ese_gui_style_get_text_hovered(style);
    node->colors[ESE_GUI_COLOR_BUTTON_TEXT_HOVERED] = ese_color_copy(text_hovered);
    
    EseColor *text_pressed = ese_gui_style_get_text_pressed(style);
    node->colors[ESE_GUI_COLOR_BUTTON_TEXT_PRESSED] = ese_color_copy(text_pressed);
}

// Free all colors owned by a node
void _ese_gui_free_node_colors(EseGuiLayoutNode *node) {
    if (node == NULL) return;
    
    for (size_t i = 0; i < ESE_GUI_COLOR_MAX; i++) {
        if (node->colors[i] != NULL) {
            ese_color_destroy(node->colors[i]);
            node->colors[i] = NULL;
        }
    }
}


