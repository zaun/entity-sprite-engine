#include <stdlib.h>
#include <string.h>
#include "core/memory_manager.h"
#include "graphics/gui/gui_lua.h"
#include "graphics/gui/gui_private.h"
#include "graphics/gui/gui_widget.h"
#include "graphics/gui/gui_widget_flex.h"
#include "graphics/gui/gui_widget_stack.h"
#include "graphics/gui/gui_widget_button.h"
#include "graphics/gui/gui_widget_image.h"
#include "graphics/gui/gui.h"
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
    gui->default_style = ese_gui_style_create(engine);
    ese_gui_style_ref(gui->default_style);

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
    ese_gui_style_unref(gui->default_style);
    ese_gui_style_destroy(gui->default_style);

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

    // Step 1: Layout pass
    for (size_t frame_idx = 0; frame_idx < gui->layouts_count; frame_idx++) {
        EseGuiLayout *layout = &gui->layouts[frame_idx];
        if (layout->root == NULL) continue;
        // root takes full layout region unless explicitly sized
        if (layout->root->width == 0 || layout->root->width == GUI_AUTO_SIZE) layout->root->width = layout->width;
        if (layout->root->height == 0 || layout->root->height == GUI_AUTO_SIZE) layout->root->height = layout->height;
        _ese_widget_layout(layout->root, layout->x, layout->y, layout->root->width, layout->root->height);
    }

    // Step 2: Process input: hover and clicks
    for (size_t frame_idx = 0; frame_idx < gui->layouts_count; frame_idx++) {
        EseGuiLayout *layout = &gui->layouts[frame_idx];
        if (layout->root == NULL) continue;
        if (gui->input_state == NULL) continue;
        const int mx = ese_input_state_get_mouse_x(gui->input_state);
        const int my = ese_input_state_get_mouse_y(gui->input_state);
        _ese_widget_process_mouse_hover(layout->root, mx, my);
        // Process left-button clicks only for now (index 0)
        if (ese_input_state_get_mouse_clicked(gui->input_state, 0)) {
            _ese_widget_process_mouse_clicked(layout->root, mx, my, InputMouse_LEFT);
        }
    }

    // Step 3: Draw pass
    for (size_t frame_idx = 0; frame_idx < gui->layouts_count; frame_idx++) {
        EseGuiLayout *layout = &gui->layouts[frame_idx];
        if (layout->root == NULL) continue;
        _ese_widget_draw(gui, layout->root, draw_list, 0);
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

EseGuiStyle *ese_gui_get_default_style(EseGui *gui) {
    log_assert("GUI", gui != NULL, "ese_gui_get_default_style called with NULL gui");
    return gui->default_style;
}

void ese_gui_set_default_style(EseGui *gui, EseGuiStyle *style) {
    log_assert("GUI", gui != NULL, "ese_gui_set_default_style called with NULL gui");
    log_assert("GUI", style != NULL, "ese_gui_set_default_style called with NULL style");

    // Unref the old style
    ese_gui_style_unref(gui->default_style);
    ese_gui_style_destroy(gui->default_style);

    // Set and ref the new style
    gui->default_style = style;
    ese_gui_style_ref(gui->default_style);
}

void ese_gui_reset_default_style(EseGui *gui) {
    log_assert("GUI", gui != NULL, "ese_gui_reset_default_style called with NULL gui");

    // Unref the old style
    ese_gui_style_unref(gui->default_style);
    ese_gui_style_destroy(gui->default_style);

    // Set and ref the new style
    gui->default_style = ese_gui_style_create(gui->engine);
    ese_gui_style_ref(gui->default_style);
}

EseGuiStyleVariant ese_gui_get_top_variant(EseGui *gui) {
    log_assert("GUI", gui != NULL, "ese_gui_get_top_variant called with NULL gui");
    log_assert("GUI", gui->open_layout != NULL, "ese_gui_get_top_variant called with no open layout");
    log_assert("GUI", gui->open_layout->variant_stack_count > 0, "ese_gui_get_top_variant called with empty variant stack");

    return gui->open_layout->variant_stack[gui->open_layout->variant_stack_count - 1];
}

void ese_gui_push_variant(EseGui *gui, EseGuiStyleVariant variant) {
    log_assert("GUI", gui != NULL, "ese_gui_push_variant called with NULL gui");
    log_assert("GUI", gui->open_layout != NULL, "ese_gui_push_variant called with no open layout");
    log_assert("GUI", gui->open_layout->variant_stack_count < MAX_VARIANT_STACK, "ese_gui_push_variant called with full variant stack");

    gui->open_layout->variant_stack[gui->open_layout->variant_stack_count++] = variant;
}

void ese_gui_pop_variant(EseGui *gui) {
    log_assert("GUI", gui != NULL, "ese_gui_pop_variant called with NULL gui");
    log_assert("GUI", gui->open_layout != NULL, "ese_gui_pop_variant called with no open layout");
    log_assert("GUI", gui->open_layout->variant_stack_count > 1, "ese_gui_pop_variant called with only default variant on stack");

    gui->open_layout->variant_stack_count--;
}
