/*
* test_gui.c - Unity-based tests for GUI layout functionality
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <execinfo.h>
#include <signal.h>
#include <math.h>
#include <sys/wait.h>

#include "testing.h"
#include "../src/core/engine.h"
#include "../src/core/engine_private.h"

#include "../src/graphics/gui/gui.h"
#include "../src/graphics/gui/gui_private.h"
#include "../src/graphics/draw_list.h"
#include "../src/types/gui_style.h"
#include "../src/types/color.h"
#include "../src/types/input_state.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

/**
* Test Functions Declarations
*/
static void test_ese_gui_create_destroy(void);
static void test_ese_gui_create_null_engine(void);
static void test_ese_gui_frame_management(void);
static void test_ese_gui_flex_row_stacks_justify_start_align_start(void);
static void test_ese_gui_flex_row_flexes_justify_start_align_start(void);
static void test_ese_gui_flex_row_both_justify_start_align_start(void);
static void test_ese_gui_flex_row_stacks_justify_center_align_start(void);
static void test_ese_gui_flex_row_flexes_justify_center_align_start(void);
static void test_ese_gui_flex_row_both_justify_center_align_start(void);
static void test_ese_gui_flex_row_stacks_justify_end_align_start(void);
static void test_ese_gui_flex_row_flexes_justify_end_align_start(void);
static void test_ese_gui_flex_row_both_justify_end_align_start(void);
// static void test_ese_gui_flex_row_stacks_justify_start_align_center(void);
// static void test_ese_gui_flex_row_flexes_justify_start_align_center(void);
// static void test_ese_gui_flex_row_both_justify_start_align_center(void);
// static void test_ese_gui_flex_row_stacks_justify_start_align_end(void);
// static void test_ese_gui_flex_row_flexes_justify_start_align_end(void);
// static void test_ese_gui_flex_row_both_justify_start_align_end(void);

// static void test_ese_gui_flex_row_stacks_justify_start_align_start_spacing(void);
// static void test_ese_gui_flex_row_flexes_justify_start_align_start_spacing(void);
// static void test_ese_gui_flex_row_stacks_justify_start_align_start_padding(void);
// static void test_ese_gui_flex_row_flexes_justify_start_align_start_padding(void);

// static void test_ese_gui_flex_row_stacks_justify_center_align_center_auto_width(void);
// static void test_ese_gui_flex_row_stacks_justify_center_align_center_auto_height(void);
// static void test_ese_gui_flex_row_stacks_justify_center_align_center_auto_both(void);


/**
* Mock callback for testing
*/
static bool button_callback_called = false;
static void test_button_callback(void) {
    button_callback_called = true;
}

static void mock_reset(void) {
    button_callback_called = false;
}

/**
* Test suite setup and teardown
*/
static EseEngine *g_app_engine = NULL;
static EseLuaEngine *g_engine = NULL;
static EseDrawList *g_draw_list = NULL;

void setUp(void) {
    g_app_engine = engine_create(NULL);
    g_engine = g_app_engine->lua_engine;
    g_draw_list = draw_list_create();
}

void tearDown(void) {
    engine_destroy(g_app_engine);
    g_app_engine = NULL;
    g_engine = NULL;
    draw_list_destroy(g_draw_list);
}

/**
* Helper function to create a GUI instance for testing
*/
static EseGui *create_test_gui(void) {
    return engine_get_gui(g_app_engine);
}

/**
* Main test runner
*/
int main(void) {
    log_init();

    printf("\nEseGui Tests\n");
    printf("------------\n");

    UNITY_BEGIN();

    // Basic functionality tests
    RUN_TEST(test_ese_gui_create_destroy);
    RUN_TEST(test_ese_gui_create_null_engine);
    RUN_TEST(test_ese_gui_frame_management);

    // Layout system tests - flex containers
    RUN_TEST(test_ese_gui_flex_row_stacks_justify_start_align_start);
    RUN_TEST(test_ese_gui_flex_row_flexes_justify_start_align_start);
    RUN_TEST(test_ese_gui_flex_row_both_justify_start_align_start);
    RUN_TEST(test_ese_gui_flex_row_stacks_justify_center_align_start);
    RUN_TEST(test_ese_gui_flex_row_flexes_justify_center_align_start);
    RUN_TEST(test_ese_gui_flex_row_both_justify_center_align_start);
    RUN_TEST(test_ese_gui_flex_row_stacks_justify_end_align_start);
    RUN_TEST(test_ese_gui_flex_row_flexes_justify_end_align_start);
    RUN_TEST(test_ese_gui_flex_row_both_justify_end_align_start);
    // RUN_TEST(test_ese_gui_flex_row_stacks_justify_start_align_center);
    // RUN_TEST(test_ese_gui_flex_row_flexes_justify_start_align_center);
    // RUN_TEST(test_ese_gui_flex_row_both_justify_start_align_center);
    // RUN_TEST(test_ese_gui_flex_row_stacks_justify_start_align_end);
    // RUN_TEST(test_ese_gui_flex_row_flexes_justify_start_align_end);
    // RUN_TEST(test_ese_gui_flex_row_both_justify_start_align_end);

    // // Layout system tests - spacing and padding
    // RUN_TEST(test_ese_gui_flex_row_stacks_justify_start_align_start_spacing);
    // RUN_TEST(test_ese_gui_flex_row_flexes_justify_start_align_start_spacing);
    // RUN_TEST(test_ese_gui_flex_row_stacks_justify_start_align_start_padding);
    // RUN_TEST(test_ese_gui_flex_row_flexes_justify_start_align_start_padding);

    // // Layout system tests - auto-sizing Stack widgets
    // RUN_TEST(test_ese_gui_flex_row_stacks_justify_center_align_center_auto_width);
    // RUN_TEST(test_ese_gui_flex_row_stacks_justify_center_align_center_auto_height);
    // RUN_TEST(test_ese_gui_flex_row_stacks_justify_center_align_center_auto_both);

    // memory_manager.destroy(true);

    return UNITY_END();
}

/**
* Test ese_gui_create and ese_gui_destroy
*/
static void test_ese_gui_create_destroy(void) {
    EseGui *gui = create_test_gui();
    TEST_ASSERT_NOT_NULL_MESSAGE(gui, "ese_gui_create should return a valid GUI instance");

    // Test that GUI is properly initialized
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, gui->layouts_count, "Frame stack count should be 0 after creation");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, gui->draw_iterator, "Draw iterator should be 0 after creation");
    TEST_ASSERT_FALSE_MESSAGE(gui->iterator_started, "Iterator should not be started after creation");
}

/**
* Test ese_gui_create (GUI doesn't need engine)
*/
static void test_ese_gui_create_null_engine(void) {
    EseGui *gui = ese_gui_create(g_engine);
    TEST_ASSERT_NOT_NULL_MESSAGE(gui, "ese_gui_create should work with engine");
}

/**
* Test basic frame management
*/
static void test_ese_gui_frame_management(void) {
    EseGui *gui = create_test_gui();
    lua_State *L = g_engine->runtime;

    const char *lua_code = "GUI.start(10, 0, 0, 100, 100); GUI.finish();";
    // Test ese_gui_begin
    TEST_ASSERT_LUA(L, lua_code, "Lua begin and finish should execute without error");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, gui->layouts_count, "Frame stack count should be 1 after begin");

    // Test ese_gui_begin
    TEST_ASSERT_LUA(L, lua_code, "Lua begin and finish should execute without error");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, gui->layouts_count, "Frame stack count should be 2 after begin");
}

static void test_ese_gui_flex_row_stacks_justify_start_align_start(void) {
    EseGui *gui = create_test_gui();
    lua_State *L = g_engine->runtime;

    // Frame size is 100x100 at location 0,0
    //   Flex container is 100x100 - justify start, align items start
    //     Stack 1 is 20x20 at location 0,0 - Stack has fixed size
    //     Stack 2 is 30x20 at location 20,0

    const char *lua_code = 
        "local style = GuiStyle.new(); "
        "style.padding_top = 0;"
        "style.padding_right = 0;"
        "style.padding_bottom = 0;"
        "style.padding_left = 0;"
        "GUI.start(0, 0, 0, 100, 100); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.START, GUI.STYLE.ALIGN.START, "
        "  GUI.STYLE.AUTO_SIZE, GUI.STYLE.AUTO_SIZE, style "
        "); "
        "GUI.open_stack(20, 20); "
        "GUI.close_stack(); "
        "GUI.open_stack(30, 20); "
        "GUI.close_stack(); "
        "GUI.close_flex(); "
        "GUI.finish();";

    // Test the Lua code execution
    TEST_ASSERT_LUA(L, lua_code, "Lua flex should execute without error");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, gui->layouts_count, "Frame stack count should be 1 after begin");

    // Process to calculate layouts
    ese_gui_process(gui, g_draw_list);

    // Verify layout results
    EseGuiLayout *layout = &gui->layouts[0];
    EseGuiWidget *root = layout->root;
    TEST_ASSERT_NOT_NULL_MESSAGE(root, "Root should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", root->type.id, "Root should be a Flex container");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->x, "Flex container should start at x=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->y, "Flex container should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->width, "Flex container should be 100px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->height, "Flex container should be 100px tall");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, root->children_count, "Flex should have 2 children");

    EseGuiWidget *child1 = root->children[0];
    TEST_ASSERT_NOT_NULL_MESSAGE(child1, "First child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("STACK", child1->type.id, "First child should be a Stack");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child1->x, "First child should start at x=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child1->y, "First child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child1->width, "First child should be 20px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child1->height, "First child should be 20px tall");

    EseGuiWidget *child2 = root->children[1];
    TEST_ASSERT_NOT_NULL_MESSAGE(child2, "Second child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("STACK", child2->type.id, "Second child should be a Stack");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child2->x, "Second child should start at x=20");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child2->y, "Second child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(30, child2->width, "Second child should be 30px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child2->height, "Second child should be 20px tall");
}

static void test_ese_gui_flex_row_flexes_justify_start_align_start(void) {
    EseGui *gui = create_test_gui();
    lua_State *L = g_engine->runtime;

    // Frame size is 100x100 at location 0,0
    //   Flex container is 100x100 - justify start, align items start
    //     Flex 1 is 50x100 at location 0,0 - Flex has fixed size
    //     Flex 2 is 50x100 at location 50,0

    const char *lua_code = 
        "local style = GuiStyle.new(); "
        "style.padding_top = 0;"
        "style.padding_right = 0;"
        "style.padding_bottom = 0;"
        "style.padding_left = 0;"
        "GUI.start(0, 0, 0, 100, 100); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.START, GUI.STYLE.ALIGN.START, "
        "  GUI.STYLE.AUTO_SIZE, GUI.STYLE.AUTO_SIZE, style "
        "); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.START, GUI.STYLE.ALIGN.START, "
        "  50, 100, style "
        "); "
        "GUI.close_flex(); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.START, GUI.STYLE.ALIGN.START, "
        "  50, 100, style "
        "); "
        "GUI.close_flex(); "
        "GUI.close_flex(); "
        "GUI.finish();";

    // Test the Lua code execution
    TEST_ASSERT_LUA(L, lua_code, "Lua flex should execute without error");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, gui->layouts_count, "Frame stack count should be 1 after begin");

    // Process to calculate layouts
    ese_gui_process(gui, g_draw_list);

    // Verify layout results
    EseGuiLayout *layout = &gui->layouts[0];
    EseGuiWidget *root = layout->root;
    TEST_ASSERT_NOT_NULL_MESSAGE(root, "Root should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", root->type.id, "Root should be a Flex container");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->x, "Flex container should start at x=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->y, "Flex container should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->width, "Flex container should be 100px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->height, "Flex container should be 100px tall");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, root->children_count, "Flex should have 2 children");

    EseGuiWidget *child1 = root->children[0];
    TEST_ASSERT_NOT_NULL_MESSAGE(child1, "First child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", child1->type.id, "First child should be a Flex");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child1->x, "First child should start at x=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child1->y, "First child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(50, child1->width, "First child should be 50px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, child1->height, "First child should be 100px tall");

    EseGuiWidget *child2 = root->children[1];
    TEST_ASSERT_NOT_NULL_MESSAGE(child2, "Second child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", child2->type.id, "Second child should be a Flex");
    TEST_ASSERT_EQUAL_INT_MESSAGE(50, child2->x, "Second child should start at x=50");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child2->y, "Second child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(50, child2->width, "Second child should be 50px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, child2->height, "Second child should be 100px tall");
}

static void test_ese_gui_flex_row_both_justify_start_align_start(void) {
    EseGui *gui = create_test_gui();
    lua_State *L = g_engine->runtime;

    // Frame size is 100x100 at location 0,0
    //   Flex container is 100x100 - justify start, align items start
    //     Stack 1 is 20x20 at location 0,0 - Stack has fixed size
    //     Flex 1 is 80x100 at location 20,0

    const char *lua_code = 
        "local style = GuiStyle.new(); "
        "style.padding_top = 0;"
        "style.padding_right = 0;"
        "style.padding_bottom = 0;"
        "style.padding_left = 0;"
        "GUI.start(0, 0, 0, 100, 100); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.START, GUI.STYLE.ALIGN.START, "
        "  GUI.STYLE.AUTO_SIZE, GUI.STYLE.AUTO_SIZE, style "
        "); "
        "GUI.open_stack(20, 20); "
        "GUI.close_stack(); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.START, GUI.STYLE.ALIGN.START, "
        "  80, 100, style "
        "); "
        "GUI.close_flex(); "
        "GUI.close_flex(); "
        "GUI.finish();";

    // Test the Lua code execution
    TEST_ASSERT_LUA(L, lua_code, "Lua flex should execute without error");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, gui->layouts_count, "Frame stack count should be 1 after begin");

    // Process to calculate layouts
    ese_gui_process(gui, g_draw_list);

    // Verify layout results
    EseGuiLayout *layout = &gui->layouts[0];
    EseGuiWidget *root = layout->root;
    TEST_ASSERT_NOT_NULL_MESSAGE(root, "Root should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", root->type.id, "Root should be a Flex container");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->x, "Flex container should start at x=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->y, "Flex container should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->width, "Flex container should be 100px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->height, "Flex container should be 100px tall");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, root->children_count, "Flex should have 2 children");

    EseGuiWidget *child1 = root->children[0];
    TEST_ASSERT_NOT_NULL_MESSAGE(child1, "First child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("STACK", child1->type.id, "First child should be a Stack");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child1->x, "First child should start at x=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child1->y, "First child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child1->width, "First child should be 20px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child1->height, "First child should be 20px tall");

    EseGuiWidget *child2 = root->children[1];
    TEST_ASSERT_NOT_NULL_MESSAGE(child2, "Second child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", child2->type.id, "Second child should be a Flex");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child2->x, "Second child should start at x=20");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child2->y, "Second child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(80, child2->width, "Second child should be 80px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, child2->height, "Second child should be 100px tall");
}

static void test_ese_gui_flex_row_stacks_justify_center_align_start(void) {
    EseGui *gui = create_test_gui();
    lua_State *L = g_engine->runtime;

    // Frame size is 100x100 at location 0,0
    //   Flex container is 100x100 - justify center, align items start
    //     Stack 1 is 20x20 at location 25,0
    //     Stack 2 is 30x20 at location 45,0

    const char *lua_code = 
        "local style = GuiStyle.new(); "
        "style.align = GUI.STYLE.ALIGN.START; "
        "style.padding_top = 0;"
        "style.padding_right = 0;"
        "style.padding_bottom = 0;"
        "style.padding_left = 0;"
        "GUI.start(0, 0, 0, 100, 100); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.CENTER, GUI.STYLE.ALIGN.START, "
        "  GUI.STYLE.AUTO_SIZE, GUI.STYLE.AUTO_SIZE, style "
        "); "
        "GUI.open_stack(20, 20); "
        "GUI.close_stack(); "
        "GUI.open_stack(30, 20); "
        "GUI.close_stack(); "
        "GUI.close_flex(); "
        "GUI.finish();";

    // Test the Lua code execution
    TEST_ASSERT_LUA(L, lua_code, "Lua flex should execute without error");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, gui->layouts_count, "Frame stack count should be 1 after begin");

    // Process to calculate layouts
    ese_gui_process(gui, g_draw_list);

    // Verify layout results
    EseGuiLayout *layout = &gui->layouts[0];
    EseGuiWidget *root = layout->root;
    TEST_ASSERT_NOT_NULL_MESSAGE(root, "Root should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", root->type.id, "Root should be a Flex container");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->x, "Flex container should start at x=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->y, "Flex container should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->width, "Flex container should be 100px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->height, "Flex container should be 100px tall");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, root->children_count, "Flex should have 2 children");

    EseGuiWidget *child1 = root->children[0];
    TEST_ASSERT_NOT_NULL_MESSAGE(child1, "First child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("STACK", child1->type.id, "First child should be a Stack");
    TEST_ASSERT_EQUAL_INT_MESSAGE(25, child1->x, "First child should start at x=25 (centered)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child1->y, "First child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child1->width, "First child should be 20px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child1->height, "First child should be 20px tall");

    EseGuiWidget *child2 = root->children[1];
    TEST_ASSERT_NOT_NULL_MESSAGE(child2, "Second child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("STACK", child2->type.id, "Second child should be a Stack");
    TEST_ASSERT_EQUAL_INT_MESSAGE(45, child2->x, "Second child should start at x=45 (centered)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child2->y, "Second child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(30, child2->width, "Second child should be 30px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child2->height, "Second child should be 20px tall");
}

static void test_ese_gui_flex_row_flexes_justify_center_align_start(void) {
    EseGui *gui = create_test_gui();
    lua_State *L = g_engine->runtime;

    // Frame size is 100x100 at location 0,0
    //   Flex container is 100x100 - justify center, align items start
    //     Flex 1 is 50x100 at location 0,0
    //     Flex 2 is 50x100 at location 50,0

    const char *lua_code = 
        "local style = GuiStyle.new(); "
        "style.padding_top = 0;"
        "style.padding_right = 0;"
        "style.padding_bottom = 0;"
        "style.padding_left = 0;"
        "GUI.start(0, 0, 0, 100, 100); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.CENTER, GUI.STYLE.ALIGN.START, "
        "  GUI.STYLE.AUTO_SIZE, GUI.STYLE.AUTO_SIZE, style "
        "); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.START, GUI.STYLE.ALIGN.START, "
        "  50, 100, style "
        "); "
        "GUI.close_flex(); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.START, GUI.STYLE.ALIGN.START, "
        "  50, 100, style "
        "); "
        "GUI.close_flex(); "
        "GUI.close_flex(); "
        "GUI.finish();";

    // Test the Lua code execution
    TEST_ASSERT_LUA(L, lua_code, "Lua flex should execute without error");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, gui->layouts_count, "Frame stack count should be 1 after begin");

    // Process to calculate layouts
    ese_gui_process(gui, g_draw_list);

    // Verify layout results
    EseGuiLayout *layout = &gui->layouts[0];
    EseGuiWidget *root = layout->root;
    TEST_ASSERT_NOT_NULL_MESSAGE(root, "Root should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", root->type.id, "Root should be a Flex container");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->x, "Flex container should start at x=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->y, "Flex container should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->width, "Flex container should be 100px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->height, "Flex container should be 100px tall");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, root->children_count, "Flex should have 2 children");

    EseGuiWidget *child1 = root->children[0];
    TEST_ASSERT_NOT_NULL_MESSAGE(child1, "First child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", child1->type.id, "First child should be a Flex");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child1->x, "First child should start at x=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child1->y, "First child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(50, child1->width, "First child should be 50px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, child1->height, "First child should be 100px tall");

    EseGuiWidget *child2 = root->children[1];
    TEST_ASSERT_NOT_NULL_MESSAGE(child2, "Second child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", child2->type.id, "Second child should be a Flex");
    TEST_ASSERT_EQUAL_INT_MESSAGE(50, child2->x, "Second child should start at x=50");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child2->y, "Second child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(50, child2->width, "Second child should be 50px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, child2->height, "Second child should be 100px tall");
}

static void test_ese_gui_flex_row_both_justify_center_align_start(void) {
    EseGui *gui = create_test_gui();
    lua_State *L = g_engine->runtime;

    // Frame size is 100x100 at location 0,0
    //   Flex container is 100x100 - justify center, align items start
    //     Flex 1 is 40x100 at location 20,0
    //     Stack 1 is 20x20 at location 60,0

    const char *lua_code = 
        "local style = GuiStyle.new(); "
        "style.padding_top = 0;"
        "style.padding_right = 0;"
        "style.padding_bottom = 0;"
        "style.padding_left = 0;"
        "GUI.start(0, 0, 0, 100, 100); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.CENTER, GUI.STYLE.ALIGN.START, "
        "  GUI.STYLE.AUTO_SIZE, GUI.STYLE.AUTO_SIZE, style "
        "); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.START, GUI.STYLE.ALIGN.START, "
        "  40, 100, style "
        "); "
        "GUI.close_flex(); "
        "GUI.open_stack(20, 20); "
        "GUI.close_stack(); "
        "GUI.close_flex(); "
        "GUI.finish();";

    // Test the Lua code execution
    TEST_ASSERT_LUA(L, lua_code, "Lua flex should execute without error");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, gui->layouts_count, "Frame stack count should be 1 after begin");

    // Process to calculate layouts
    ese_gui_process(gui, g_draw_list);

    // Verify layout results
    EseGuiLayout *layout = &gui->layouts[0];
    EseGuiWidget *root = layout->root;
    TEST_ASSERT_NOT_NULL_MESSAGE(root, "Root should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", root->type.id, "Root should be a Flex container");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->x, "Flex container should start at x=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->y, "Flex container should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->width, "Flex container should be 100px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->height, "Flex container should be 100px tall");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, root->children_count, "Flex should have 2 children");

    EseGuiWidget *child1 = root->children[0];
    TEST_ASSERT_NOT_NULL_MESSAGE(child1, "First child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", child1->type.id, "First child should be a Flex");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child1->x, "First child should start at x=20 (centered)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child1->y, "First child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(40, child1->width, "First child should be 40px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, child1->height, "First child should be 100px tall");

    EseGuiWidget *child2 = root->children[1];
    TEST_ASSERT_NOT_NULL_MESSAGE(child2, "Second child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("STACK", child2->type.id, "Second child should be a Stack");
    TEST_ASSERT_EQUAL_INT_MESSAGE(60, child2->x, "Second child should start at x=60 (centered)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child2->y, "Second child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child2->width, "Second child should be 20px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child2->height, "Second child should be 20px tall");
}

static void test_ese_gui_flex_row_stacks_justify_end_align_start(void) {
    EseGui *gui = create_test_gui();
    lua_State *L = g_engine->runtime;

    // Frame size is 100x100 at location 0,0
    //   Flex container is 100x100 - justify end, align items start
    //     Stack 1 is 20x20 at location 50,0
    //     Stack 2 is 30x20 at location 70,0

    const char *lua_code = 
        "local style = GuiStyle.new(); "
        "style.padding_top = 0;"
        "style.padding_right = 0;"
        "style.padding_bottom = 0;"
        "style.padding_left = 0;"
        "GUI.start(0, 0, 0, 100, 100); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.END, GUI.STYLE.ALIGN.START, "
        "  GUI.STYLE.AUTO_SIZE, GUI.STYLE.AUTO_SIZE, style "
        "); "
        "GUI.open_stack(20, 20); "
        "GUI.close_stack(); "
        "GUI.open_stack(30, 20); "
        "GUI.close_stack(); "
        "GUI.close_flex(); "
        "GUI.finish();";

    // Test the Lua code execution
    TEST_ASSERT_LUA(L, lua_code, "Lua flex should execute without error");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, gui->layouts_count, "Frame stack count should be 1 after begin");

    // Process to calculate layouts
    ese_gui_process(gui, g_draw_list);

    // Verify layout results
    EseGuiLayout *layout = &gui->layouts[0];
    EseGuiWidget *root = layout->root;
    TEST_ASSERT_NOT_NULL_MESSAGE(root, "Root should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", root->type.id, "Root should be a Flex container");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->x, "Flex container should start at x=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->y, "Flex container should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->width, "Flex container should be 100px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->height, "Flex container should be 100px tall");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, root->children_count, "Flex should have 2 children");

    EseGuiWidget *child1 = root->children[0];
    TEST_ASSERT_NOT_NULL_MESSAGE(child1, "First child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("STACK", child1->type.id, "First child should be a Stack");
    TEST_ASSERT_EQUAL_INT_MESSAGE(50, child1->x, "First child should start at x=50 (justify end)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child1->y, "First child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child1->width, "First child should be 20px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child1->height, "First child should be 20px tall");

    EseGuiWidget *child2 = root->children[1];
    TEST_ASSERT_NOT_NULL_MESSAGE(child2, "Second child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("STACK", child2->type.id, "Second child should be a Stack");
    TEST_ASSERT_EQUAL_INT_MESSAGE(70, child2->x, "Second child should start at x=70 (justify end)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child2->y, "Second child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(30, child2->width, "Second child should be 30px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child2->height, "Second child should be 20px tall");
}

static void test_ese_gui_flex_row_flexes_justify_end_align_start(void) {
    EseGui *gui = create_test_gui();
    lua_State *L = g_engine->runtime;

    // Frame size is 100x100 at location 0,0
    //   Flex container is 100x100 - justify end, align items start
    //     Flex 1 is 50x100 at location 0,0
    //     Flex 2 is 50x100 at location 50,0

    const char *lua_code = 
        "local style = GuiStyle.new(); "
        "style.padding_top = 0;"
        "style.padding_right = 0;"
        "style.padding_bottom = 0;"
        "style.padding_left = 0;"
        "GUI.start(0, 0, 0, 100, 100); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.END, GUI.STYLE.ALIGN.START, "
        "  GUI.STYLE.AUTO_SIZE, GUI.STYLE.AUTO_SIZE, style "
        "); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.START, GUI.STYLE.ALIGN.START, "
        "  50, 100, style "
        "); "
        "GUI.close_flex(); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.START, GUI.STYLE.ALIGN.START, "
        "  50, 100, style "
        "); "
        "GUI.close_flex(); "
        "GUI.close_flex(); "
        "GUI.finish();";

    // Test the Lua code execution
    TEST_ASSERT_LUA(L, lua_code, "Lua flex should execute without error");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, gui->layouts_count, "Frame stack count should be 1 after begin");

    // Process to calculate layouts
    ese_gui_process(gui, g_draw_list);

    // Verify layout results
    EseGuiLayout *layout = &gui->layouts[0];
    EseGuiWidget *root = layout->root;
    TEST_ASSERT_NOT_NULL_MESSAGE(root, "Root should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", root->type.id, "Root should be a Flex container");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->x, "Flex container should start at x=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->y, "Flex container should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->width, "Flex container should be 100px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->height, "Flex container should be 100px tall");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, root->children_count, "Flex should have 2 children");

    EseGuiWidget *child1 = root->children[0];
    TEST_ASSERT_NOT_NULL_MESSAGE(child1, "First child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", child1->type.id, "First child should be a Flex");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child1->x, "First child should start at x=0 (justify end)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child1->y, "First child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(50, child1->width, "First child should be 50px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, child1->height, "First child should be 100px tall");

    EseGuiWidget *child2 = root->children[1];
    TEST_ASSERT_NOT_NULL_MESSAGE(child2, "Second child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", child2->type.id, "Second child should be a Flex");
    TEST_ASSERT_EQUAL_INT_MESSAGE(50, child2->x, "Second child should start at x=50 (justify end)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child2->y, "Second child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(50, child2->width, "Second child should be 50px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, child2->height, "Second child should be 100px tall");
}

static void test_ese_gui_flex_row_both_justify_end_align_start(void) {
    EseGui *gui = create_test_gui();
    lua_State *L = g_engine->runtime;

    // Frame size is 100x100 at location 0,0
    //   Flex container is 100x100 - justify end, align items start
    //     Stack 1 is 20x20 at location 60,0
    //     Flex 1 is 40x100 at location 80,0

    const char *lua_code = 
        "local style = GuiStyle.new(); "
        "style.padding_top = 0;"
        "style.padding_right = 0;"
        "style.padding_bottom = 0;"
        "style.padding_left = 0;"
        "GUI.start(0, 0, 0, 100, 100); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.END, GUI.STYLE.ALIGN.START, "
        "  GUI.STYLE.AUTO_SIZE, GUI.STYLE.AUTO_SIZE, style "
        "); "
        "GUI.open_stack(20, 20); "
        "GUI.close_stack(); "
        "GUI.open_flex("
        "  GUI.STYLE.DIRECTION.ROW, GUI.STYLE.JUSTIFY.START, GUI.STYLE.ALIGN.START, "
        "  40, 100, style "
        "); "
        "GUI.close_flex(); "
        "GUI.close_flex(); "
        "GUI.finish();";

    // Test the Lua code execution
    TEST_ASSERT_LUA(L, lua_code, "Lua flex should execute without error");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, gui->layouts_count, "Frame stack count should be 1 after begin");

    // Process to calculate layouts
    ese_gui_process(gui, g_draw_list);

    // Verify layout results
    EseGuiLayout *layout = &gui->layouts[0];
    EseGuiWidget *root = layout->root;
    TEST_ASSERT_NOT_NULL_MESSAGE(root, "Root should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", root->type.id, "Root should be a Flex container");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->x, "Flex container should start at x=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, root->y, "Flex container should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->width, "Flex container should be 100px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, root->height, "Flex container should be 100px tall");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, root->children_count, "Flex should have 2 children");

    EseGuiWidget *child1 = root->children[0];
    TEST_ASSERT_NOT_NULL_MESSAGE(child1, "First child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("STACK", child1->type.id, "First child should be a Stack");
    TEST_ASSERT_EQUAL_INT_MESSAGE(40, child1->x, "First child should start at x=40 (justify end)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child1->y, "First child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child1->width, "First child should be 20px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, child1->height, "First child should be 20px tall");

    EseGuiWidget *child2 = root->children[1];
    TEST_ASSERT_NOT_NULL_MESSAGE(child2, "Second child should exist");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("FLEX", child2->type.id, "Second child should be a Flex");
    TEST_ASSERT_EQUAL_INT_MESSAGE(60, child2->x, "Second child should start at x=60 (justify end)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child2->y, "Second child should start at y=0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(40, child2->width, "Second child should be 40px wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, child2->height, "Second child should be 100px tall");
}

// static void test_ese_gui_flex_row_stacks_justify_start_align_center(void) {
//     EseGui *gui = create_test_gui();

//     // Frame size is 100x100 at location 0,0
//     //   Flex container is 100x100 - justify start, align items center
//     //     Stack 1 is 20x20 at location 0,40 - Stack has fixed size
//     //     Stack 2 is 30x30 at location 20,35

//     ese_gui_begin(gui, 0, 0, 0, 100, 100);

//     // Create a flex container with row direction
//     EseColor *bg_color = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_START, FLEX_ALIGN_ITEMS_CENTER, 0, 0, 0, 0, 0, bg_color);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_color_destroy(bg_color);

//     // Add some child containers
//     EseColor *stack_color = ese_color_create(g_engine);
//     ese_gui_open_stack(gui, 20, 20);
//     ese_gui_close_stack(gui);
//     ese_color_destroy(stack_color);

//     EseColor *stack_color2 = ese_color_create(g_engine);
//     ese_gui_open_stack(gui, 30, 30);
//     ese_gui_close_stack(gui);
//     ese_color_destroy(stack_color2);

//     ese_gui_close_flex(gui);
//     ese_gui_end(gui);

//     // Process to calculate layouts
//     ese_gui_process(gui, g_draw_list);

//     // Debug: root tree info
//     EseGuiLayout *layout = &gui->layouts[0];
//     TEST_ASSERT_NOT_NULL_MESSAGE(layout->root, "Root should exist");

//     // Verify layout results
//     // Check flex container dimensions (should be 100x100)
//     EseGuiLayoutNode *flex_node = layout->root;
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->x, "Flex container should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->y, "Flex container should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->width, "Flex container should be 100px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->height, "Flex container should be 100px tall");

//     // Check first Stack position (should be at left edge)
//     TEST_ASSERT_TRUE_MESSAGE(flex_node->children_count >= 2, "Flex should have 2 children");
//     EseGuiLayoutNode *stack1_node = flex_node->children[0];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, stack1_node->x, "First Stack 1 should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(40, stack1_node->y, "First Stack 1 should start at y=40");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, stack1_node->width, "First Stack 1 should be 20px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, stack1_node->height, "First Stack 1should be 20px tall");

//     // Check second Stack position (should be next to first stack )
//     EseGuiLayoutNode *stack2_node = flex_node->children[1];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, stack2_node->x, "Second Stack 2 should start at x=20");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(35, stack2_node->y, "Second Stack 2 should start at y=35");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(30, stack2_node->width, "Second Stack 2 should be 30px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(30, stack2_node->height, "Second Stack 2 should be 30px tall");

//     ese_gui_cleanup(gui);

//     cleanup_test_gui(gui);
// }

// static void test_ese_gui_flex_row_flexes_justify_start_align_center(void) {
//     EseGui *gui = create_test_gui();

//     // Frame size is 100x100 at location 0,0
//     //   Flex container is 100x100 - justify start, align items center
//     //     Flex 1 is 50x100 at location 0,0
//     //     Flex 2 is 50x100 at location 50,0

//     ese_gui_begin(gui, 0, 0, 0, 100, 100);

//     // Create a flex container with row direction
//     EseColor *bg_color = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_START, FLEX_ALIGN_ITEMS_CENTER, 0, 0, 0, 0, 0, bg_color);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_color_destroy(bg_color);

//     // Add some child containers
//     EseColor *stack_color = ese_color_create(g_engine);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_gui_close_flex(gui);
//     ese_color_destroy(stack_color);

//     EseColor *stack_color2 = ese_color_create(g_engine);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_gui_close_flex(gui);
//     ese_color_destroy(stack_color2);

//     ese_gui_close_flex(gui);
//     ese_gui_end(gui);

//     // Process to calculate layouts
//     ese_gui_process(gui, g_draw_list);

//     // Debug: root tree info
//     EseGuiLayout *layout = &gui->layouts[0];
//     TEST_ASSERT_NOT_NULL_MESSAGE(layout->root, "Root should exist");

//     // Verify layout results
//     // Check flex container dimensions (should be 100x100)
//     EseGuiLayoutNode *flex_node = layout->root;
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->x, "Flex container should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->y, "Flex container should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->width, "Flex container should be 100px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->height, "Flex container should be 100px tall");

//     // Check first flex position (should be at left edge, vertically centered)
//     TEST_ASSERT_TRUE_MESSAGE(flex_node->children_count >= 2, "Flex should have 2 children");
//     EseGuiLayoutNode *stack1_node = flex_node->children[0];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, stack1_node->x, "First flex 1 should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(50, stack1_node->y, "First flex 1 should start at y=50");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(50, stack1_node->width, "First flex 1 should be 50px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, stack1_node->height, "First flex 1 should be 100px tall");

//     // Check second flex position (should be next to first flex)
//     EseGuiLayoutNode *stack2_node = flex_node->children[1];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(50, stack2_node->x, "Second flex 2 should start at x=50");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(50, stack2_node->y, "Second flex 2 should start at y=50");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(50, stack2_node->width, "Second flex 2 should be 50px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, stack2_node->height, "Second flex 2 should be 100px tall");

//     ese_gui_cleanup(gui);

//     cleanup_test_gui(gui);
// }

// static void test_ese_gui_flex_row_both_justify_start_align_center(void) {
//     EseGui *gui = create_test_gui();

//     // Frame size is 100x100 at location 0,0
//     //   Flex container is 100x100 - justify start, align items center
//     //     Stack 1 is 20x20 at location 0,40 - Stack has fixed size
//     //     Flex 1 is 80x100 at location 20,0

//     ese_gui_begin(gui, 0, 0, 0, 100, 100);

//     // Create a flex container with row direction
//     EseColor *bg_color = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_START, FLEX_ALIGN_ITEMS_CENTER, 0, 0, 0, 0, 0, bg_color);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_color_destroy(bg_color);

//     // Add Stack first
//     EseColor *stack_color = ese_color_create(g_engine);
//     ese_gui_open_stack(gui, 20, 20);
//     ese_gui_close_stack(gui);
//     ese_color_destroy(stack_color);

//     // Add flex second
//     EseColor *flex_color = ese_color_create(g_engine);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_gui_close_flex(gui);
//     ese_color_destroy(flex_color);

//     ese_gui_close_flex(gui);
//     ese_gui_end(gui);

//     // Process to calculate layouts
//     ese_gui_process(gui, g_draw_list);

//     // Debug: root tree info
//     EseGuiLayout *layout = &gui->layouts[0];
//     TEST_ASSERT_NOT_NULL_MESSAGE(layout->root, "Root should exist");

//     // Verify layout results
//     // Check flex container dimensions (should be 100x100)
//     EseGuiLayoutNode *flex_node = layout->root;
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->x, "Flex container should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->y, "Flex container should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->width, "Flex container should be 100px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->height, "Flex container should be 100px tall");

//     // Check first Stack position (should be at left edge, vertically centered)
//     TEST_ASSERT_TRUE_MESSAGE(flex_node->children_count >= 2, "Flex should have 2 children");
//     EseGuiLayoutNode *stack1_node = flex_node->children[0];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, stack1_node->x, "First Stack should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(40, stack1_node->y, "First Stack should start at y=40");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, stack1_node->width, "First Stack should be 20px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, stack1_node->height, "First Stack should be 20px tall");

//     // Check second flex position (should be next to first stack )
//     EseGuiLayoutNode *flex1_node = flex_node->children[1];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, flex1_node->x, "Second flex should start at x=20");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(50, flex1_node->y, "Second flex should start at y=50");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex1_node->width, "Second flex should be 100px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex1_node->height, "Second flex should be 100px tall");

//     ese_gui_cleanup(gui);

//     cleanup_test_gui(gui);
// }

// static void test_ese_gui_flex_row_stacks_justify_start_align_end(void) {
//     EseGui *gui = create_test_gui();

//     // Frame size is 100x100 at location 0,0
//     //   Flex container is 100x100 - justify start, align items end
//     //     Stack 1 is 20x20 at location 0,80 - Stack has fixed size
//     //     Stack 2 is 30x30 at location 20,70

//     ese_gui_begin(gui, 0, 0, 0, 100, 100);

//     // Create a flex container with row direction
//     EseColor *bg_color = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_START, FLEX_ALIGN_ITEMS_END, 0, 0, 0, 0, 0, bg_color);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_color_destroy(bg_color);

//     // Add some child containers
//     EseColor *stack_color = ese_color_create(g_engine);
//     ese_gui_open_stack(gui, 20, 20);
//     ese_gui_close_stack(gui);
//     ese_color_destroy(stack_color);

//     EseColor *stack_color2 = ese_color_create(g_engine);
//     ese_gui_open_stack(gui, 30, 30);
//     ese_gui_close_stack(gui);
//     ese_color_destroy(stack_color2);

//     ese_gui_close_flex(gui);
//     ese_gui_end(gui);

//     // Process to calculate layouts
//     ese_gui_process(gui, g_draw_list);

//     // Debug: root tree info
//     EseGuiLayout *layout = &gui->layouts[0];
//     TEST_ASSERT_NOT_NULL_MESSAGE(layout->root, "Root should exist");

//     // Verify layout results
//     // Check flex container dimensions (should be 100x100)
//     EseGuiLayoutNode *flex_node = layout->root;
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->x, "Flex container should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->y, "Flex container should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->width, "Flex container should be 100px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->height, "Flex container should be 100px tall");

//     // Check first Stack position (should be at left edge)
//     TEST_ASSERT_TRUE_MESSAGE(flex_node->children_count >= 2, "Flex should have 2 children");
//     EseGuiLayoutNode *stack1_node = flex_node->children[0];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, stack1_node->x, "First Stack 1 should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(80, stack1_node->y, "First Stack 1 should start at y=80");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, stack1_node->width, "First Stack 1 should be 20px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, stack1_node->height, "First Stack 1should be 20px tall");

//     // Check second Stack position (should be next to first stack )
//     EseGuiLayoutNode *stack2_node = flex_node->children[1];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, stack2_node->x, "Second Stack 2 should start at x=20");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(70, stack2_node->y, "Second Stack 2 should start at y=70");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(30, stack2_node->width, "Second Stack 2 should be 30px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(30, stack2_node->height, "Second Stack 2 should be 30px tall");

//     ese_gui_cleanup(gui);

//     cleanup_test_gui(gui);
// }

// static void test_ese_gui_flex_row_flexes_justify_start_align_end(void) {
//     EseGui *gui = create_test_gui();

//     // Frame size is 100x100 at location 0,0
//     //   Flex container is 100x100 - justify start, align items end
//     //     Flex 1 is 50x100 at location 0,0
//     //     Flex 2 is 50x100 at location 50,0

//     ese_gui_begin(gui, 0, 0, 0, 100, 100);

//     // Create a flex container with row direction
//     EseColor *bg_color = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_START, FLEX_ALIGN_ITEMS_END, 0, 0, 0, 0, 0, bg_color);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_color_destroy(bg_color);

//     // Add some child containers
//     EseColor *stack_color = ese_color_create(g_engine);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_gui_close_flex(gui);
//     ese_color_destroy(stack_color);

//     EseColor *stack_color2 = ese_color_create(g_engine);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_gui_close_flex(gui);
//     ese_color_destroy(stack_color2);

//     ese_gui_close_flex(gui);
//     ese_gui_end(gui);

//     // Process to calculate layouts
//     ese_gui_process(gui, g_draw_list);

//     // Debug: root tree info
//     EseGuiLayout *layout = &gui->layouts[0];
//     TEST_ASSERT_NOT_NULL_MESSAGE(layout->root, "Root should exist");

//     // Verify layout results
//     // Check flex container dimensions (should be 100x100)
//     EseGuiLayoutNode *flex_node = layout->root;
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->x, "Flex container should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->y, "Flex container should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->width, "Flex container should be 100px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->height, "Flex container should be 100px tall");

//     // Check first flex position (should be at left edge, aligned to end)
//     TEST_ASSERT_TRUE_MESSAGE(flex_node->children_count >= 2, "Flex should have 2 children");
//     EseGuiLayoutNode *stack1_node = flex_node->children[0];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, stack1_node->x, "First flex 1 should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, stack1_node->y, "First flex 1 should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(50, stack1_node->width, "First flex 1 should be 50px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, stack1_node->height, "First flex 1 should be 100px tall");

//     // Check second flex position (should be next to first flex)
//     EseGuiLayoutNode *stack2_node = flex_node->children[1];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(50, stack2_node->x, "Second flex 2 should start at x=50");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, stack2_node->y, "Second flex 2 should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(50, stack2_node->width, "Second flex 2 should be 50px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, stack2_node->height, "Second flex 2 should be 100px tall");

//     ese_gui_cleanup(gui);

//     cleanup_test_gui(gui);
// }

// static void test_ese_gui_flex_row_both_justify_start_align_end(void) {
//     EseGui *gui = create_test_gui();

//     // Frame size is 100x100 at location 0,0
//     //   Flex container is 100x100 - justify start, align items end
//     //     Stack 1 is 20x20 at location 0,80 - Stack has fixed size
//     //     Flex 1 is 80x100 at location 20,0

//     ese_gui_begin(gui, 0, 0, 0, 100, 100);

//     // Create a flex container with row direction
//     EseColor *bg_color = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_START, FLEX_ALIGN_ITEMS_END, 0, 0, 0, 0, 0, bg_color);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_color_destroy(bg_color);

//     // Add Stack first
//     EseColor *stack_color = ese_color_create(g_engine);
//     ese_gui_open_stack(gui, 20, 20);
//     ese_gui_close_stack(gui);
//     ese_color_destroy(stack_color);

//     // Add flex second
//     EseColor *flex_color = ese_color_create(g_engine);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_gui_close_flex(gui);
//     ese_color_destroy(flex_color);

//     ese_gui_close_flex(gui);
//     ese_gui_end(gui);

//     // Process to calculate layouts
//     ese_gui_process(gui, g_draw_list);

//     // Debug: root tree info
//     EseGuiLayout *layout = &gui->layouts[0];
//     TEST_ASSERT_NOT_NULL_MESSAGE(layout->root, "Root should exist");

//     // Verify layout results
//     // Check flex container dimensions (should be 100x100)
//     EseGuiLayoutNode *flex_node = layout->root;
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->x, "Flex container should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->y, "Flex container should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->width, "Flex container should be 100px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->height, "Flex container should be 100px tall");

//     // Check first Stack position (should be at left edge, aligned to end)
//     TEST_ASSERT_TRUE_MESSAGE(flex_node->children_count >= 2, "Flex should have 2 children");
//     EseGuiLayoutNode *stack1_node = flex_node->children[0];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, stack1_node->x, "First Stack should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(80, stack1_node->y, "First Stack should start at y=80");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, stack1_node->width, "First Stack should be 20px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, stack1_node->height, "First Stack should be 20px tall");

//     // Check second flex position (should be next to first stack )
//     EseGuiLayoutNode *flex1_node = flex_node->children[1];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, flex1_node->x, "Second flex should start at x=20");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex1_node->y, "Second flex should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex1_node->width, "Second flex should be 100px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex1_node->height, "Second flex should be 100px tall");

//     ese_gui_cleanup(gui);

//     cleanup_test_gui(gui);
// }

// static void test_ese_gui_flex_row_stacks_justify_start_align_start_spacing(void) {
//     EseGui *gui = create_test_gui();

//     // Frame size is 100x100 at location 0,0
//     //   Flex container is 100x100 - justify start, align items start, spacing 5
//     //     Stack 1 is 20x20 at location 0,0
//     //     Stack 2 is 30x30 at location 25,0 (20 + 5 spacing)

//     ese_gui_begin(gui, 0, 0, 0, 100, 100);

//     // Create a flex container with row direction and spacing
//     EseColor *bg_color = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_START, FLEX_ALIGN_ITEMS_START, 5, 0, 0, 0, 0, bg_color);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_color_destroy(bg_color);

//     // Add first stack 
//     EseColor *stack1_color = ese_color_create(g_engine);
//     ese_gui_open_stack(gui, 20, 20);
//     ese_gui_close_stack(gui);
//     ese_color_destroy(stack1_color);

//     // Add second stack 
//     EseColor *stack2_color = ese_color_create(g_engine);
//     ese_gui_open_stack(gui, 30, 30);
//     ese_gui_close_stack(gui);
//     ese_color_destroy(stack2_color);

//     ese_gui_close_flex(gui);
//     ese_gui_end(gui);

//     // Process to calculate layouts
//     ese_gui_process(gui, g_draw_list);

//     // Debug: root tree info
//     EseGuiLayout *layout = &gui->layouts[0];
//     TEST_ASSERT_NOT_NULL_MESSAGE(layout->root, "Root should exist");

//     // Verify layout results
//     // Check flex container dimensions (should be 100x100)
//     EseGuiLayoutNode *flex_node = layout->root;
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->x, "Flex container should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->y, "Flex container should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->width, "Flex container should be 100px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->height, "Flex container should be 100px tall");

//     // Check first Stack position (should be at left edge)
//     TEST_ASSERT_TRUE_MESSAGE(flex_node->children_count >= 2, "Flex should have 2 children");
//     EseGuiLayoutNode *stack1_node = flex_node->children[0];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, stack1_node->x, "First Stack should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, stack1_node->y, "First Stack should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, stack1_node->width, "First Stack should be 20px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, stack1_node->height, "First Stack should be 20px tall");

//     // Check second Stack position (should be after first Stack + spacing)
//     EseGuiLayoutNode *stack2_node = flex_node->children[1];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(25, stack2_node->x, "Second Stack should start at x=25 (20 + 5 spacing)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, stack2_node->y, "Second Stack should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(30, stack2_node->width, "Second Stack should be 30px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(30, stack2_node->height, "Second Stack should be 30px tall");

//     ese_gui_cleanup(gui);

//     cleanup_test_gui(gui);
// }

// static void test_ese_gui_flex_row_stacks_justify_start_align_start_padding(void) {
//     EseGui *gui = create_test_gui();

//     // Frame size is 100x100 at location 0,0
//     //   Flex container is 100x100 - justify start, align items start, padding 5,10,10,5
//     //     Stack 1 is 20x20 at location 5,10 (padding left, padding top)
//     //     Stack 2 is 30x30 at location 25,10 (5 + 20)

//     ese_gui_begin(gui, 0, 0, 0, 100, 100);

//     // Create a flex container with row direction and padding
//     EseColor *bg_color = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_START, FLEX_ALIGN_ITEMS_START, 0, 5, 10, 10, 5, bg_color);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_color_destroy(bg_color);

//     // Add first stack 
//     EseColor *stack1_color = ese_color_create(g_engine);
//     ese_gui_open_stack(gui, 20, 20);
//     ese_gui_close_stack(gui);
//     ese_color_destroy(stack1_color);

//     // Add second stack 
//     EseColor *stack2_color = ese_color_create(g_engine);
//     ese_gui_open_stack(gui, 30, 30);
//     ese_gui_close_stack(gui);
//     ese_color_destroy(stack2_color);

//     ese_gui_close_flex(gui);
//     ese_gui_end(gui);

//     // Process to calculate layouts
//     ese_gui_process(gui, g_draw_list);

//     // Debug: root tree info
//     EseGuiLayout *layout = &gui->layouts[0];
//     TEST_ASSERT_NOT_NULL_MESSAGE(layout->root, "Root should exist");

//     // Verify layout results
//     // Check flex container dimensions (should be 100x100)
//     EseGuiLayoutNode *flex_node = layout->root;
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->x, "Flex container should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->y, "Flex container should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->width, "Flex container should be 100px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->height, "Flex container should be 100px tall");

//     // Check first Stack position (should be at padding offset)
//     TEST_ASSERT_TRUE_MESSAGE(flex_node->children_count >= 2, "Flex should have 2 children");
//     EseGuiLayoutNode *stack1_node = flex_node->children[0];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(5, stack1_node->x, "First Stack should start at x=5 (padding left)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(10, stack1_node->y, "First Stack should start at y=10 (padding top)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, stack1_node->width, "First Stack should be 20px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, stack1_node->height, "First Stack should be 20px tall");

//     // Check second Stack position (should be after first stack )
//     EseGuiLayoutNode *stack2_node = flex_node->children[1];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(25, stack2_node->x, "Second Stack should start at x=25 (5 + 20)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(10, stack2_node->y, "Second Stack should start at y=10 (padding top)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(30, stack2_node->width, "Second Stack should be 30px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(30, stack2_node->height, "Second Stack should be 30px tall");

//     ese_gui_cleanup(gui);

//     cleanup_test_gui(gui);
// }

// static void test_ese_gui_flex_row_flexes_justify_start_align_start_spacing(void) {
//     EseGui *gui = create_test_gui();

//     // Frame size is 100x100 at location 0,0
//     //   Flex container is 100x100 - justify start, align items start, spacing 5
//     //     Flex 1 is 47.5x100 at location 0,0
//     //     Flex 2 is 47.5x100 at location 52.5,0 (47.5 + 5 spacing)

//     ese_gui_begin(gui, 0, 0, 0, 100, 100);

//     // Create a flex container with row direction and spacing
//     EseColor *bg_color = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_START, FLEX_ALIGN_ITEMS_START, 5, 0, 0, 0, 0, bg_color);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_color_destroy(bg_color);

//     // Add first flex
//     EseColor *flex1_color = ese_color_create(g_engine);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_gui_close_flex(gui);
//     ese_color_destroy(flex1_color);

//     // Add second flex
//     EseColor *flex2_color = ese_color_create(g_engine);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_gui_close_flex(gui);
//     ese_color_destroy(flex2_color);

//     ese_gui_close_flex(gui);
//     ese_gui_end(gui);

//     // Process to calculate layouts
//     ese_gui_process(gui, g_draw_list);

//     // Debug: root tree info
//     EseGuiLayout *layout = &gui->layouts[0];
//     TEST_ASSERT_NOT_NULL_MESSAGE(layout->root, "Root should exist");

//     // Verify layout results
//     // Check flex container dimensions (should be 100x100)
//     EseGuiLayoutNode *flex_node = layout->root;
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->x, "Flex container should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->y, "Flex container should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->width, "Flex container should be 100px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->height, "Flex container should be 100px tall");

//     // Check first flex position (should be at left edge)
//     TEST_ASSERT_TRUE_MESSAGE(flex_node->children_count >= 2, "Flex should have 2 children");
//     EseGuiLayoutNode *flex1_node = flex_node->children[0];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex1_node->x, "First flex should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex1_node->y, "First flex should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(47, flex1_node->width, "First flex should be 47px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex1_node->height, "First flex should be 100px tall");

//     // Check second flex position (should be after first flex + spacing)
//     EseGuiLayoutNode *flex2_node = flex_node->children[1];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(52, flex2_node->x, "Second flex should start at x=52 (47 + 5 spacing)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex2_node->y, "Second flex should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(47, flex2_node->width, "Second flex should be 47px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex2_node->height, "Second flex should be 100px tall");

//     ese_gui_cleanup(gui);

//     cleanup_test_gui(gui);
// }

// static void test_ese_gui_flex_row_flexes_justify_start_align_start_padding(void) {
//     EseGui *gui = create_test_gui();

//     // Frame size is 100x100 at location 0,0
//     //   Flex container is 100x100 - justify start, align items start, padding 5,10,10,5
//     //     Flex 1 is 40x80 at location 5,10 (padding left, padding top)
//     //     Flex 2 is 40x80 at location 50,10 (5 + 40 + 5 spacing)

//     ese_gui_begin(gui, 0, 0, 0, 100, 100);

//     // Create a flex container with row direction and padding
//     EseColor *bg_color = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_START, FLEX_ALIGN_ITEMS_START, 5, 5, 10, 10, 5, bg_color);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_color_destroy(bg_color);

//     // Add first flex
//     EseColor *flex1_color = ese_color_create(g_engine);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_gui_close_flex(gui);
//     ese_color_destroy(flex1_color);

//     // Add second flex
//     EseColor *flex2_color = ese_color_create(g_engine);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_gui_close_flex(gui);
//     ese_color_destroy(flex2_color);

//     ese_gui_close_flex(gui);
//     ese_gui_end(gui);

//     // Process to calculate layouts
//     ese_gui_process(gui, g_draw_list);

//     // Debug: root tree info
//     EseGuiLayout *layout = &gui->layouts[0];
//     TEST_ASSERT_NOT_NULL_MESSAGE(layout->root, "Root should exist");

//     // Verify layout results
//     // Check flex container dimensions (should be 100x100)
//     EseGuiLayoutNode *flex_node = layout->root;
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->x, "Flex container should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->y, "Flex container should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->width, "Flex container should be 100px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->height, "Flex container should be 100px tall");

//     // Check first flex position (should be at padding offset)
//     TEST_ASSERT_TRUE_MESSAGE(flex_node->children_count >= 2, "Flex should have 2 children");
//     EseGuiLayoutNode *flex1_node = flex_node->children[0];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(5, flex1_node->x, "First flex should start at x=5 (padding left)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(10, flex1_node->y, "First flex should start at y=10 (padding top)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(40, flex1_node->width, "First flex should be 40px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(85, flex1_node->height, "First flex should be 85px tall");

//     // Check second flex position (should be after first flex)
//     EseGuiLayoutNode *flex2_node = flex_node->children[1];
//     TEST_ASSERT_EQUAL_INT_MESSAGE(50, flex2_node->x, "Second flex should start at x=50 (5 + 40 + 5 spacing)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(10, flex2_node->y, "Second flex should start at y=10 (padding top)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(40, flex2_node->width, "Second flex should be 40px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(85, flex2_node->height, "Second flex should be 85px tall");

//     ese_gui_cleanup(gui);

//     cleanup_test_gui(gui);
// }

// /**
//  * Test auto-sizing Stack widgets - auto width
//  */
//  static void test_ese_gui_flex_row_stacks_justify_center_align_center_auto_width(void) {
//     EseGui *gui = create_test_gui();

//     // Frame size is 100x100 at location 0,0
//     //   Flex container is 100x100 - justify center, align items center, row direction
//     //     Stack 1 is 50x20 at location 0,40 (Stack has auto width, fixed height)
//     //     Stack 2 is 50x30 at location 50,35 (Stack has auto width, fixed height)

//     ese_gui_begin(gui, 0, 0, 0, 100, 100);

//     // Create a flex container with row direction
//     EseColor *bg_color = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_CENTER, FLEX_ALIGN_ITEMS_CENTER, 0, 0, 0, 0, 0, bg_color);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_color_destroy(bg_color);

//     // Add some child containers with auto width
//     EseColor *stack_color = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_CENTER, FLEX_ALIGN_ITEMS_CENTER, 0, 0, 0, 0, 0, stack_color);
//     ese_gui_open_stack(gui, GUI_AUTO_SIZE, 20);
//     ese_gui_close_stack(gui);

//     EseColor *stack_color2 = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_CENTER, FLEX_ALIGN_ITEMS_CENTER, 0, 0, 0, 0, 0, stack_color2);
//     ese_gui_open_stack(gui, GUI_AUTO_SIZE, 30);
//     ese_gui_close_stack(gui);

//     ese_gui_close_flex(gui);
//     ese_gui_end(gui);

//     // Process to calculate layouts
//     ese_gui_process(gui, g_draw_list);
//     EseGuiLayout *layout = &gui->layouts[0];
//     TEST_ASSERT_NOT_NULL_MESSAGE(layout->root, "Root should exist");

//     // Check flex container dimensions (should be 100x100)
//     EseGuiLayoutNode *flex_node = layout->root;
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->x, "Flex container should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->y, "Flex container should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->width, "Flex container should be 100px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->height, "Flex container should be 100px tall");

//     // Check that stack es have auto-calculated widths
//     TEST_ASSERT_TRUE_MESSAGE(flex_node->children_count >= 2, "Flex should have 2 children");
//     EseGuiLayoutNode *stack1_node = flex_node->children[0];
//     EseGuiLayoutNode *stack2_node = flex_node->children[1];
    
//     // Stack 1
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, stack1_node->x, "First Stack should be positioned at x=0 (justify center)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(40, stack1_node->y, "First Stack should be positioned at y=40 (align center)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(50, stack1_node->width, "First Stack should have auto-calculated width of 50px");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, stack1_node->height, "First Stack should have fixed height 20");

//     // Stack 2
//     TEST_ASSERT_EQUAL_INT_MESSAGE(50, stack2_node->x, "Second Stack should be positioned at x=50 (justify center)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(35, stack2_node->y, "Second Stack should be positioned at y=35 (align center)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(50, stack2_node->width, "Second Stack should have auto-calculated width of 50px");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(30, stack2_node->height, "Second Stack should have fixed height 30");    

//     ese_gui_cleanup(gui);

//     cleanup_test_gui(gui);
// }

// /**
//  * Test auto-sizing Stack widgets - auto height
//  */
// static void test_ese_gui_flex_row_stacks_justify_center_align_center_auto_height(void) {
//     EseGui *gui = create_test_gui();

//     // Frame size is 100x100 at location 0,0
//     //   Flex container is 100x100 - justify center, align items center, row direction
//     //     Stack 1 is 20x100 at location 40,100 (Stack has fixed width, auto height)
//     //     Stack 2 is 30x100 at location 50,100 (Stack has fixed width, auto height)

//     ese_gui_begin(gui, 0, 0, 0, 100, 100);

//     // Create a flex container with row direction
//     EseColor *bg_color = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_CENTER, FLEX_ALIGN_ITEMS_CENTER, 0, 0, 0, 0, 0, bg_color);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);

//     // Add some child containers with auto width
//     EseColor *stack_color = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_CENTER, FLEX_ALIGN_ITEMS_CENTER, 0, 0, 0, 0, 0, stack_color);
//     ese_gui_open_stack(gui, 20, GUI_AUTO_SIZE);
//     ese_gui_close_stack(gui);

//     EseColor *stack_color2 = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_CENTER, FLEX_ALIGN_ITEMS_CENTER, 0, 0, 0, 0, 0, stack_color2);
//     ese_gui_open_stack(gui, 30, GUI_AUTO_SIZE);
//     ese_gui_close_stack(gui);

//     ese_gui_close_flex(gui);
//     ese_gui_end(gui);

//     // Process to calculate layouts
//     ese_gui_process(gui, g_draw_list);
//     EseGuiLayout *layout = &gui->layouts[0];
//     TEST_ASSERT_NOT_NULL_MESSAGE(layout->root, "Root should exist");

//     // Check flex container dimensions (should be 100x100)
//     EseGuiLayoutNode *flex_node = layout->root;
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->x, "Flex container should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->y, "Flex container should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->width, "Flex container should be 100px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->height, "Flex container should be 100px tall");

//     // Check that stack es have auto-calculated widths
//     TEST_ASSERT_TRUE_MESSAGE(flex_node->children_count >= 2, "Flex should have 2 children");
//     EseGuiLayoutNode *stack1_node = flex_node->children[0];
//     EseGuiLayoutNode *stack2_node = flex_node->children[1];
    
//     // Stack 1
//     TEST_ASSERT_EQUAL_INT_MESSAGE(25, stack1_node->x, "First Stack should be positioned at x=25 (justify center)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, stack1_node->y, "First Stack should be positioned at y=40 (align center)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(20, stack1_node->width, "First Stack should have auto-calculated width of 50px");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, stack1_node->height, "First Stack should have fixed height 20");

//     // Stack 2
//     TEST_ASSERT_EQUAL_INT_MESSAGE(45, stack2_node->x, "Second Stack should be positioned at x=45 (justify center)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, stack2_node->y, "Second Stack should be positioned at y=35 (align center)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(30, stack2_node->width, "Second Stack should have auto-calculated width of 50px");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, stack2_node->height, "Second Stack should have fixed height 30");    

//     ese_gui_cleanup(gui);

//     cleanup_test_gui(gui);
// }

// /**
//  * Test auto-sizing Stack widgets - auto both width and height
//  */
// static void test_ese_gui_flex_row_stacks_justify_center_align_center_auto_both(void) {
//     EseGui *gui = create_test_gui();

//     // Frame size is 100x100 at location 0,0
//     //   Flex container is 100x100 - justify center, align items center, row direction
//     //     Stack 1 is 50x100 at location 50,100 (Stack has auto width, auto height)
//     //     Stack 2 is 50x100 at location 50,100 (Stack has auto width, auto height)

//     ese_gui_begin(gui, 0, 0, 0, 100, 100);

//     // Create a flex container with row direction
//     EseColor *bg_color = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_CENTER, FLEX_ALIGN_ITEMS_CENTER, 0, 0, 0, 0, 0, bg_color);
//     ese_gui_open_flex(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);

//     // Add some child containers with auto width
//     EseColor *stack_color = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_CENTER, FLEX_ALIGN_ITEMS_CENTER, 0, 0, 0, 0, 0, stack_color);
//     ese_gui_open_stack(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_gui_close_stack(gui);

//     EseColor *stack_color2 = ese_color_create(g_engine);
//     set_gui_style(gui, FLEX_DIRECTION_ROW, FLEX_JUSTIFY_CENTER, FLEX_ALIGN_ITEMS_CENTER, 0, 0, 0, 0, 0, stack_color2);
//     ese_gui_open_stack(gui, GUI_AUTO_SIZE, GUI_AUTO_SIZE);
//     ese_gui_close_stack(gui);

//     ese_gui_close_flex(gui);
//     ese_gui_end(gui);

//     // Process to calculate layouts
//     ese_gui_process(gui, g_draw_list);
//     EseGuiLayout *layout = &gui->layouts[0];
//     TEST_ASSERT_NOT_NULL_MESSAGE(layout->root, "Root should exist");

//     // Check flex container dimensions (should be 100x100)
//     EseGuiLayoutNode *flex_node = layout->root;
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->x, "Flex container should start at x=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, flex_node->y, "Flex container should start at y=0");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->width, "Flex container should be 100px wide");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, flex_node->height, "Flex container should be 100px tall");

//     // Check that stack es have auto-calculated widths
//     TEST_ASSERT_TRUE_MESSAGE(flex_node->children_count >= 2, "Flex should have 2 children");
//     EseGuiLayoutNode *stack1_node = flex_node->children[0];
//     EseGuiLayoutNode *stack2_node = flex_node->children[1];
    
//     // Stack 1
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, stack1_node->x, "First Stack should be positioned at x=0 (justify center)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, stack1_node->y, "First Stack should be positioned at y=40 (align center)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(50, stack1_node->width, "First Stack should have auto-calculated width of 50px");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, stack1_node->height, "First Stack should have fixed height 20");

//     // Stack 2
//     TEST_ASSERT_EQUAL_INT_MESSAGE(50, stack2_node->x, "Second Stack should be positioned at x=50 (justify center)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(0, stack2_node->y, "Second Stack should be positioned at y=35 (align center)");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(50, stack2_node->width, "Second Stack should have auto-calculated width of 50px");
//     TEST_ASSERT_EQUAL_INT_MESSAGE(100, stack2_node->height, "Second Stack should have fixed height 30");    

//     ese_gui_cleanup(gui);

//     cleanup_test_gui(gui);
// }
